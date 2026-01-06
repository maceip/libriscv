/**
 * Code Executor Header
 *
 * Defines structures and classes for compiling and executing code
 * in a sandboxed RISC-V environment.
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * Result of code compilation
 */
struct CompilationResult {
    bool success = false;
    std::string output;
};

/**
 * Result of code execution
 */
struct ExecutionResult {
    bool success = false;
    int exit_code = -1;
    std::string output;
    std::string error;
    std::string exception;
    std::string compilation_output;

    uint64_t execution_time_us = 0;
    uint64_t compile_time_us = 0;
    uint64_t instructions_executed = 0;
    uint64_t memory_used = 0;
    uint64_t binary_size = 0;

    json to_json() const;
};

/**
 * Code executor class - compiles and runs code in libriscv sandbox
 */
class CodeExecutor {
public:
    CodeExecutor();
    ~CodeExecutor();

    /**
     * Execute code with the given parameters
     *
     * @param arguments JSON object containing:
     *   - code: source code string
     *   - language: programming language (c, cpp, python, rust)
     *   - timeout_seconds: execution timeout
     *   - max_memory_mb: maximum memory limit
     *
     * @return JSON result with execution details
     */
    json execute(const json& arguments);

private:
    std::string work_dir_;
    uint64_t next_exec_id_ = 0;

    std::string sanitize_code(const std::string& code, const std::string& language);
    std::string get_extension(const std::string& language);

    CompilationResult compile_code(
        const std::string& source_file,
        const std::string& output_file,
        const std::string& language
    );

    std::string find_riscv_compiler(const std::string& compiler);

    std::vector<uint8_t> load_binary(const std::string& path);

    ExecutionResult execute_sandboxed(
        const std::vector<uint8_t>& binary,
        uint64_t max_memory,
        uint64_t max_instructions
    );

    // JavaScript/TypeScript support
    bool create_js_wrapper(const std::string& js_file, const std::string& cpp_file);
    bool transpile_typescript(const std::string& ts_file, const std::string& js_file);

    void write_file(const std::string& path, const std::string& content);
    void cleanup_temp_files();
};
