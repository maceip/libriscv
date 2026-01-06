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
    return "txt";
}

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

        // Capture output
        std::string output;
        machine.set_printer([&output](auto&, const char* text, size_t len) {
            output.append(text, len);
        });

        // Execute with instruction limit
        auto start_time = micros_now();
        try {
            machine.simulate(max_instructions);
        } catch (const std::exception& e) {
            result.exception = e.what();
        }
        auto end_time = micros_now();

        result.output = output;
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
