/**
 * Code Executor
 *
 * Handles compilation, sanitization, and sandboxed execution of code
 * using libriscv as the execution environment.
 */

#include "mcp_server.hpp"
#include "code_executor.hpp"
#include "sanitizer.hpp"
#include <libriscv/machine.hpp>
#include <libriscv/threads.hpp>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef USE_QUICKJS
#include <sys/stat.h>
#endif

namespace fs = std::filesystem;

// Default resource limits
static const uint64_t DEFAULT_MAX_INSTRUCTIONS = 36'000'000UL;
static const uint64_t DEFAULT_MAX_MEMORY = 32UL * 1024 * 1024;
static const uint64_t DEFAULT_TIMEOUT_SECONDS = 5;

// Maximum binary size (32 MB)
static const uint64_t MAX_BINARY_SIZE = 32'000'000UL;

// Environment variables for executed programs
static const std::vector<std::string> EXEC_ENV = {
    "LC_CTYPE=C",
    "LC_ALL=C",
    "USER=sandbox",
    "HOME=/tmp",
    "PATH=/usr/bin:/bin"
};

inline uint64_t micros_now() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000ul + ts.tv_nsec / 1000ul;
}

CodeExecutor::CodeExecutor() {
    work_dir_ = "/tmp/mcp-code-exec";
    fs::create_directories(work_dir_);
}

CodeExecutor::~CodeExecutor() {
    // Cleanup temporary files
    cleanup_temp_files();
}

json CodeExecutor::execute(const json& arguments) {
    std::string code = arguments.value("code", "");
    std::string language = arguments.value("language", "cpp");
    uint64_t timeout_seconds = arguments.value("timeout_seconds", DEFAULT_TIMEOUT_SECONDS);
    uint64_t max_memory_mb = arguments.value("max_memory_mb", 32);

    if (code.empty()) {
        throw std::runtime_error("Code cannot be empty");
    }

    // Sanitize the code
    std::string sanitized_code = sanitize_code(code, language);

    // Create unique work directory for this execution
    auto exec_id = std::to_string(next_exec_id_++);
    auto exec_dir = work_dir_ + "/exec_" + exec_id;
    fs::create_directories(exec_dir);

    ExecutionResult result;

    try {
        // Write code to file
        std::string source_file = exec_dir + "/code." + get_extension(language);
        write_file(source_file, sanitized_code);

        // Compile the code
        auto compile_start = micros_now();
        std::string binary_path = exec_dir + "/program";
        CompilationResult comp_result = compile_code(source_file, binary_path, language);
        auto compile_end = micros_now();

        result.compile_time_us = compile_end - compile_start;
        result.compilation_output = comp_result.output;

        if (!comp_result.success) {
            result.success = false;
            result.error = "Compilation failed";
            result.output = comp_result.output;
            return result.to_json();
        }

        // Load the compiled binary
        std::vector<uint8_t> binary = load_binary(binary_path);
        result.binary_size = binary.size();

        if (binary.empty()) {
            result.success = false;
            result.error = "Failed to load compiled binary";
            return result.to_json();
        }

        // Execute in libriscv sandbox
        auto exec_start = micros_now();
        result = execute_sandboxed(
            binary,
            max_memory_mb * 1024 * 1024,
            DEFAULT_MAX_INSTRUCTIONS
        );
        auto exec_end = micros_now();

        result.execution_time_us = exec_end - exec_start;
        result.success = true;

    } catch (const std::exception& e) {
        result.success = false;
        result.error = std::string("Execution error: ") + e.what();
    }

    // Cleanup
    fs::remove_all(exec_dir);

    return result.to_json();
}

std::string CodeExecutor::sanitize_code(const std::string& code, const std::string& language) {
    CodeSanitizer sanitizer;
    return sanitizer.sanitize(code, language);
}

std::string CodeExecutor::get_extension(const std::string& language) {
    if (language == "c") return "c";
    if (language == "cpp") return "cpp";
    if (language == "python") return "py";
    if (language == "rust") return "rs";
    if (language == "javascript" || language == "js") return "js";
    if (language == "typescript" || language == "ts") return "ts";
    return "txt";
}

#ifdef USE_QUICKJS
// Check if QuickJS library is available
static bool is_quickjs_available() {
    // Check for QuickJS library in third_party directory
    std::string qjs_lib = "third_party/quickjs-riscv/lib/libquickjs.a";
    struct stat buffer;
    return (stat(qjs_lib.c_str(), &buffer) == 0);
}

// Create a C wrapper that embeds JavaScript code and uses QuickJS API
static bool create_quickjs_wrapper(const std::string& js_file, const std::string& wrapper_file) {
    // Read the JavaScript code
    std::ifstream js_input(js_file);
    if (!js_input.is_open()) {
        return false;
    }

    std::stringstream js_buffer;
    js_buffer << js_input.rdbuf();
    std::string js_code = js_buffer.str();
    js_input.close();

    // Escape the JavaScript code for C string literal
    std::string escaped_js;
    for (char c : js_code) {
        if (c == '"') escaped_js += "\\\"";
        else if (c == '\\') escaped_js += "\\\\";
        else if (c == '\n') escaped_js += "\\n";
        else if (c == '\r') escaped_js += "\\r";
        else if (c == '\t') escaped_js += "\\t";
        else escaped_js += c;
    }

    // Create C wrapper that uses QuickJS API
    std::ofstream wrapper_output(wrapper_file);
    if (!wrapper_output.is_open()) {
        return false;
    }

    wrapper_output << "#include <quickjs.h>\n";
    wrapper_output << "#include <quickjs-libc.h>\n";
    wrapper_output << "#include <stdio.h>\n";
    wrapper_output << "#include <stdlib.h>\n";
    wrapper_output << "#include <string.h>\n\n";

    wrapper_output << "static const char js_code[] = \"" << escaped_js << "\";\n\n";

    wrapper_output << "int main(int argc, char **argv) {\n";
    wrapper_output << "    JSRuntime *rt = JS_NewRuntime();\n";
    wrapper_output << "    if (!rt) {\n";
    wrapper_output << "        fprintf(stderr, \"Failed to create QuickJS runtime\\n\");\n";
    wrapper_output << "        return 1;\n";
    wrapper_output << "    }\n\n";

    wrapper_output << "    JSContext *ctx = JS_NewContext(rt);\n";
    wrapper_output << "    if (!ctx) {\n";
    wrapper_output << "        fprintf(stderr, \"Failed to create QuickJS context\\n\");\n";
    wrapper_output << "        JS_FreeRuntime(rt);\n";
    wrapper_output << "        return 1;\n";
    wrapper_output << "    }\n\n";

    wrapper_output << "    // Add console.log support\n";
    wrapper_output << "    js_std_add_helpers(ctx, argc, argv);\n";
    wrapper_output << "    js_std_init_handlers(rt);\n\n";

    wrapper_output << "    // Evaluate the JavaScript code\n";
    wrapper_output << "    JSValue result = JS_Eval(ctx, js_code, strlen(js_code), \"<code>\", JS_EVAL_TYPE_GLOBAL);\n\n";

    wrapper_output << "    // Check for errors\n";
    wrapper_output << "    int exit_code = 0;\n";
    wrapper_output << "    if (JS_IsException(result)) {\n";
    wrapper_output << "        JSValue exception = JS_GetException(ctx);\n";
    wrapper_output << "        const char *str = JS_ToCString(ctx, exception);\n";
    wrapper_output << "        if (str) {\n";
    wrapper_output << "            fprintf(stderr, \"JavaScript Error: %s\\n\", str);\n";
    wrapper_output << "            JS_FreeCString(ctx, str);\n";
    wrapper_output << "        }\n";
    wrapper_output << "        JS_FreeValue(ctx, exception);\n";
    wrapper_output << "        exit_code = 1;\n";
    wrapper_output << "    }\n\n";

    wrapper_output << "    JS_FreeValue(ctx, result);\n";
    wrapper_output << "    js_std_loop(ctx);\n";
    wrapper_output << "    js_std_free_handlers(rt);\n";
    wrapper_output << "    JS_FreeContext(ctx);\n";
    wrapper_output << "    JS_FreeRuntime(rt);\n\n";

    wrapper_output << "    return exit_code;\n";
    wrapper_output << "}\n";

    wrapper_output.close();
    return true;
}
#endif

CompilationResult CodeExecutor::compile_code(
    const std::string& source_file,
    const std::string& output_file,
    const std::string& language
) {
    CompilationResult result;
    std::string compile_cmd;

    if (language == "c") {
        compile_cmd = find_riscv_compiler("gcc") + " -static -std=c17 -O2 " +
                     source_file + " -o " + output_file + " 2>&1";
    } else if (language == "cpp") {
        compile_cmd = find_riscv_compiler("g++") + " -static -pthread -std=c++20 -O2 " +
                     source_file + " -o " + output_file +
                     " -Wl,--undefined=pthread_join 2>&1";
    } else if (language == "rust") {
        compile_cmd = "rustc --target riscv64gc-unknown-linux-gnu " +
                     source_file + " -o " + output_file + " 2>&1";
    } else if (language == "javascript" || language == "js") {
#ifdef USE_QUICKJS
        // Try to use QuickJS if available
        if (is_quickjs_available()) {
            std::string quickjs_wrapper = source_file + ".qjs.c";
            if (!create_quickjs_wrapper(source_file, quickjs_wrapper)) {
                result.success = false;
                result.output = "Failed to create QuickJS wrapper";
                return result;
            }
            // Compile with QuickJS library
            compile_cmd = find_riscv_compiler("gcc") + " -static -O2 " +
                         "-I third_party/quickjs-riscv/include " +
                         quickjs_wrapper + " " +
                         "third_party/quickjs-riscv/lib/libquickjs.a " +
                         "-o " + output_file + " -lm -ldl -lpthread 2>&1";
        } else
#endif
        {
            // Fallback: Transpile JS to C++ wrapper for execution
            std::string wrapped_file = source_file + ".cpp";
            if (!create_js_wrapper(source_file, wrapped_file)) {
                result.success = false;
                result.output = "Failed to create JavaScript wrapper";
                return result;
            }
            compile_cmd = find_riscv_compiler("g++") + " -static -std=c++17 -O2 " +
                         wrapped_file + " -o " + output_file + " 2>&1";
        }
    } else if (language == "typescript" || language == "ts") {
        // Transpile TypeScript to JavaScript first
        std::string js_file = source_file + ".js";
        if (!transpile_typescript(source_file, js_file)) {
            result.success = false;
            result.output = "Failed to transpile TypeScript. Ensure 'tsc' or 'esbuild' is installed.";
            return result;
        }
#ifdef USE_QUICKJS
        // Try to use QuickJS if available
        if (is_quickjs_available()) {
            std::string quickjs_wrapper = js_file + ".qjs.c";
            if (!create_quickjs_wrapper(js_file, quickjs_wrapper)) {
                result.success = false;
                result.output = "Failed to create QuickJS wrapper";
                return result;
            }
            // Compile with QuickJS library
            compile_cmd = find_riscv_compiler("gcc") + " -static -O2 " +
                         "-I third_party/quickjs-riscv/include " +
                         quickjs_wrapper + " " +
                         "third_party/quickjs-riscv/lib/libquickjs.a " +
                         "-o " + output_file + " -lm -ldl -lpthread 2>&1";
        } else
#endif
        {
            // Fallback: Create C++ wrapper for the JS
            std::string wrapped_file = js_file + ".cpp";
            if (!create_js_wrapper(js_file, wrapped_file)) {
                result.success = false;
                result.output = "Failed to create JavaScript wrapper";
                return result;
            }
            compile_cmd = find_riscv_compiler("g++") + " -static -std=c++17 -O2 " +
                         wrapped_file + " -o " + output_file + " 2>&1";
        }
    } else if (language == "python") {
        // For Python, we could use Cython or similar
        result.success = false;
        result.output = "Python execution not yet implemented";
        return result;
    } else {
        result.success = false;
        result.output = "Unsupported language: " + language;
        return result;
    }

    // Execute compilation command
    FILE* pipe = popen(compile_cmd.c_str(), "r");
    if (!pipe) {
        result.success = false;
        result.output = "Failed to execute compiler";
        return result;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result.output += buffer;
    }

    int status = pclose(pipe);
    result.success = (status == 0);

    return result;
}

std::string CodeExecutor::find_riscv_compiler(const std::string& compiler) {
    // Try to find versioned compilers (gcc-10 through gcc-15)
    for (int version = 15; version >= 10; version--) {
        std::string versioned = "riscv64-linux-gnu-" + compiler + "-" + std::to_string(version);
        std::string check_cmd = "which " + versioned + " > /dev/null 2>&1";
        if (system(check_cmd.c_str()) == 0) {
            return versioned;
        }
    }

    // Fall back to unversioned compiler
    return "riscv64-linux-gnu-" + compiler;
}

std::vector<uint8_t> CodeExecutor::load_binary(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return {};
    }

    size_t size = file.tellg();
    if (size > MAX_BINARY_SIZE) {
        throw std::runtime_error("Binary too large");
    }

    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return {};
    }

    return buffer;
}

ExecutionResult CodeExecutor::execute_sandboxed(
    const std::vector<uint8_t>& binary,
    uint64_t max_memory,
    uint64_t max_instructions
) {
    ExecutionResult result;

    try {
        // Create RISC-V machine with resource limits
        const riscv::MachineOptions<riscv::RISCV64> options {
            .memory_max = max_memory
        };

        riscv::Machine<riscv::RISCV64> machine { binary, options };

        // Setup Linux environment
        machine.setup_linux({"program"}, EXEC_ENV);
        machine.setup_linux_syscalls();
        machine.setup_posix_threads();

        // Capture output using userdata
        struct OutputCapture {
            std::string output;
        };
        OutputCapture capture;
        machine.set_userdata(&capture);

        machine.set_printer([](const riscv::Machine<riscv::RISCV64>& m, const char* text, size_t len) {
            auto* cap = m.template get_userdata<OutputCapture>();
            cap->output.append(text, len);
        });

        // Execute with instruction limit
        auto start_time = micros_now();
        try {
            machine.simulate(max_instructions);
        } catch (const std::exception& e) {
            result.exception = e.what();
        }
        auto end_time = micros_now();

        result.output = capture.output;
        result.execution_time_us = end_time - start_time;
        result.instructions_executed = machine.instruction_counter();
        result.exit_code = machine.cpu.reg(10); // A0 register
        result.memory_used = machine.memory.pages_active() * 4096;
        result.success = true;

    } catch (const std::exception& e) {
        result.success = false;
        result.error = std::string("Sandbox error: ") + e.what();
    }

    return result;
}

void CodeExecutor::write_file(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to write file: " + path);
    }
    file << content;
}

void CodeExecutor::cleanup_temp_files() {
    try {
        if (fs::exists(work_dir_)) {
            fs::remove_all(work_dir_);
        }
    } catch (...) {
        // Ignore cleanup errors
    }
}

json ExecutionResult::to_json() const {
    json result = {
        {"success", success},
        {"exit_code", exit_code},
        {"output", output},
        {"execution_time_us", execution_time_us},
        {"compile_time_us", compile_time_us},
        {"instructions_executed", instructions_executed},
        {"memory_used", memory_used},
        {"binary_size", binary_size}
    };

    if (!error.empty()) {
        result["error"] = error;
    }

    if (!exception.empty()) {
        result["exception"] = exception;
    }

    if (!compilation_output.empty()) {
        result["compilation_output"] = compilation_output;
    }

    return result;
}

// Implementation of execute_code in MCPServer
json MCPServer::execute_code(const json& arguments) {
    CodeExecutor executor;
    return executor.execute(arguments);
}

// JavaScript/TypeScript support functions

bool CodeExecutor::create_js_wrapper(const std::string& js_file, const std::string& cpp_file) {
    // Read the JavaScript code
    std::ifstream js_input(js_file);
    if (!js_input.is_open()) {
        return false;
    }
    
    std::stringstream js_buffer;
    js_buffer << js_input.rdbuf();
    std::string js_code = js_buffer.str();
    js_input.close();
    
    // Escape the JavaScript code for C++ string literal
    std::string escaped_js;
    for (char c : js_code) {
        if (c == '"') escaped_js += "\\\"";
        else if (c == '\\') escaped_js += "\\\\";
        else if (c == '\n') escaped_js += "\\n";
        else if (c == '\r') escaped_js += "\\r";
        else if (c == '\t') escaped_js += "\\t";
        else escaped_js += c;
    }
    
    // Create C++ wrapper that simulates JavaScript execution
    std::ofstream cpp_output(cpp_file);
    if (!cpp_output.is_open()) {
        return false;
    }
    
    // Write the C++ wrapper code
    cpp_output << "#include <iostream>\n";
    cpp_output << "#include <string>\n";
    cpp_output << "#include <sstream>\n";
    cpp_output << "#include <cmath>\n";
    cpp_output << "#include <map>\n";
    cpp_output << "#include <vector>\n";
    cpp_output << "#include <functional>\n";
    cpp_output << "\n";
    cpp_output << "// Simple JavaScript runtime simulation\n";
    cpp_output << "class JSRuntime {\n";
    cpp_output << "public:\n";
    cpp_output << "    std::map<std::string, std::string> variables;\n";
    cpp_output << "    std::stringstream output;\n";
    cpp_output << "    \n";
    cpp_output << "    void console_log(const std::string& msg) {\n";
    cpp_output << "        output << msg << std::endl;\n";
    cpp_output << "    }\n";
    cpp_output << "    \n";
    cpp_output << "    std::string get_output() {\n";
    cpp_output << "        return output.str();\n";
    cpp_output << "    }\n";
    cpp_output << "};\n";
    cpp_output << "\n";
    cpp_output << "// JavaScript console.log implementation\n";
    cpp_output << "JSRuntime runtime;\n";
    cpp_output << "\n";
    cpp_output << "void console_log(const std::string& msg) {\n";
    cpp_output << "    runtime.console_log(msg);\n";
    cpp_output << "}\n";
    cpp_output << "\n";
    cpp_output << "// Main execution\n";
    cpp_output << "int main() {\n";
    cpp_output << "    std::cout << \"JavaScript Execution Environment\" << std::endl;\n";
    cpp_output << "    std::cout << \"=================================\" << std::endl;\n";
    cpp_output << "    std::cout << std::endl;\n";
    cpp_output << "    \n";
    cpp_output << "    // Embedded JavaScript code\n";
    cpp_output << "    std::string js_code = \"" << escaped_js << "\";\n";
    cpp_output << "    \n";
    cpp_output << "    std::cout << \"Original JavaScript code:\" << std::endl;\n";
    cpp_output << "    std::cout << js_code << std::endl;\n";
    cpp_output << "    std::cout << std::endl;\n";
    cpp_output << "    \n";
    cpp_output << "    // Simple console.log pattern matching\n";
    cpp_output << "    size_t pos = 0;\n";
    cpp_output << "    while ((pos = js_code.find(\"console.log(\", pos)) != std::string::npos) {\n";
    cpp_output << "        size_t start = pos + 12;\n";
    cpp_output << "        size_t end = js_code.find(\")\", start);\n";
    cpp_output << "        if (end != std::string::npos) {\n";
    cpp_output << "            std::string arg = js_code.substr(start, end - start);\n";
    cpp_output << "            // Remove quotes if present\n";
    cpp_output << "            if (arg.size() >= 2 && arg.front() == '\\\"' && arg.back() == '\\\"') {\n";
    cpp_output << "                arg = arg.substr(1, arg.size() - 2);\n";
    cpp_output << "            } else if (arg.size() >= 2 && arg.front() == '\\'' && arg.back() == '\\'') {\n";
    cpp_output << "                arg = arg.substr(1, arg.size() - 2);\n";
    cpp_output << "            }\n";
    cpp_output << "            std::cout << arg << std::endl;\n";
    cpp_output << "            runtime.console_log(arg);\n";
    cpp_output << "        }\n";
    cpp_output << "        pos = end;\n";
    cpp_output << "    }\n";
    cpp_output << "    \n";
    cpp_output << "    std::cout << std::endl;\n";
    cpp_output << "    std::cout << \"Note: This is a simplified JavaScript runtime.\" << std::endl;\n";
    cpp_output << "    std::cout << \"Full JavaScript execution via QuickJS integration planned.\" << std::endl;\n";
    cpp_output << "    \n";
    cpp_output << "    return 0;\n";
    cpp_output << "}\n";

    cpp_output.close();
    return true;
}

bool CodeExecutor::transpile_typescript(const std::string& ts_file, const std::string& js_file) {
    // Try esbuild first (faster)
    std::string esbuild_cmd = "which esbuild > /dev/null 2>&1";
    if (system(esbuild_cmd.c_str()) == 0) {
        std::string cmd = "esbuild " + ts_file + " --outfile=" + js_file + 
                         " --format=esm --target=es2020 2>&1";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return false;
        
        char buffer[256];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        int status = pclose(pipe);
        return (status == 0);
    }
    
    // Fall back to tsc
    std::string tsc_cmd = "which tsc > /dev/null 2>&1";
    if (system(tsc_cmd.c_str()) == 0) {
        std::string cmd = "tsc " + ts_file + " --outFile " + js_file + 
                         " --target ES2020 --module commonjs 2>&1";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return false;
        
        char buffer[256];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        int status = pclose(pipe);
        return (status == 0);
    }
    
    // No TypeScript compiler found
    return false;
}
