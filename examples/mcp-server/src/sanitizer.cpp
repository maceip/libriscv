/**
 * Code Sanitizer Implementation
 *
 * Implements security checks and code sanitization for multiple languages.
 * Based on the sanitization approach from the webapi example.
 */

#include "sanitizer.hpp"
#include <algorithm>
#include <stdexcept>

CodeSanitizer::CodeSanitizer() {
    initialize_patterns();
}

void CodeSanitizer::initialize_patterns() {
    // C++ forbidden patterns
    forbidden_patterns_cpp_ = {
        // Dangerous system calls and file operations
        {"system(", "Direct system() calls are forbidden", false},
        {"popen(", "popen() is forbidden", false},
        {"exec", "exec family functions are forbidden", false},
        {"fork(", "fork() is forbidden", false},
        {"dlopen(", "Dynamic library loading is forbidden", false},
        {"dlsym(", "Dynamic symbol lookup is forbidden", false},

        // Inline assembly (unless specifically allowed)
        {"__asm", "Inline assembly is restricted", false},
        {"asm(", "Inline assembly is restricted", false},

        // Dangerous preprocessor directives
        {"#include.*\\.\\./", "Path traversal in includes", true},

        // Network operations (can be relaxed if needed)
        {"socket(", "Direct socket operations are restricted", false},
        {"bind(", "Direct bind operations are restricted", false},
        {"connect(", "Direct connect operations are restricted", false},
    };

    // C forbidden patterns (similar to C++)
    forbidden_patterns_c_ = forbidden_patterns_cpp_;
}

std::string CodeSanitizer::sanitize(const std::string& code, const std::string& language) {
    if (language == "cpp") {
        return sanitize_cpp(code);
    } else if (language == "c") {
        return sanitize_c(code);
    } else if (language == "python") {
        return sanitize_python(code);
    } else if (language == "rust") {
        return sanitize_rust(code);
    } else if (language == "javascript" || language == "js") {
        return sanitize_javascript(code);
    } else if (language == "typescript" || language == "ts") {
        return sanitize_typescript(code);
    } else {
        throw std::runtime_error("Unsupported language for sanitization: " + language);
    }
}

bool CodeSanitizer::check_forbidden_patterns(
    const std::string& code,
    const std::vector<ForbiddenPattern>& patterns,
    std::string& violation
) {
    for (const auto& pattern : patterns) {
        if (pattern.is_regex) {
            try {
                std::regex re(pattern.pattern, std::regex::icase);
                if (std::regex_search(code, re)) {
                    violation = pattern.description + " (pattern: " + pattern.pattern + ")";
                    return true;
                }
            } catch (const std::regex_error&) {
                // Skip invalid regex patterns
                continue;
            }
        } else {
            // Simple substring search
            if (code.find(pattern.pattern) != std::string::npos) {
                violation = pattern.description + " (found: " + pattern.pattern + ")";
                return true;
            }
        }
    }
    return false;
}

std::string CodeSanitizer::sanitize_cpp(const std::string& code) {
    std::string violation;

    // Check for forbidden patterns
    if (check_forbidden_patterns(code, forbidden_patterns_cpp_, violation)) {
        throw std::runtime_error("Code sanitization failed: " + violation);
    }

    // Basic validation
    if (code.size() > 1024 * 1024) { // 1MB limit
        throw std::runtime_error("Code size exceeds maximum allowed size");
    }

    // Check for reasonable main function
    if (code.find("int main") == std::string::npos &&
        code.find("int main(") == std::string::npos) {
        // Allow code without main, but warn
    }

    // For now, we return the code as-is after validation
    // In a production system, you might want to add more transformations
    return code;
}

std::string CodeSanitizer::sanitize_c(const std::string& code) {
    std::string violation;

    // Check for forbidden patterns
    if (check_forbidden_patterns(code, forbidden_patterns_c_, violation)) {
        throw std::runtime_error("Code sanitization failed: " + violation);
    }

    // Basic validation
    if (code.size() > 1024 * 1024) { // 1MB limit
        throw std::runtime_error("Code size exceeds maximum allowed size");
    }

    return code;
}

std::string CodeSanitizer::sanitize_python(const std::string& code) {
    // Python-specific sanitization
    std::vector<ForbiddenPattern> python_patterns = {
        {"import os", "os module is restricted", false},
        {"import subprocess", "subprocess module is forbidden", false},
        {"import sys", "sys module is restricted", false},
        {"__import__", "Dynamic imports are forbidden", false},
        {"eval(", "eval() is forbidden", false},
        {"exec(", "exec() is forbidden", false},
        {"compile(", "compile() is forbidden", false},
        {"open(", "File operations are restricted", false},
    };

    std::string violation;
    if (check_forbidden_patterns(code, python_patterns, violation)) {
        throw std::runtime_error("Python code sanitization failed: " + violation);
    }

    return code;
}

std::string CodeSanitizer::sanitize_rust(const std::string& code) {
    // Rust-specific sanitization
    std::vector<ForbiddenPattern> rust_patterns = {
        {"std::process::Command", "Process execution is forbidden", false},
        {"std::process::exit", "Process exit is restricted", false},
        {"unsafe", "Unsafe code blocks are restricted", false},
        {"#![feature", "Feature gates are restricted", false},
    };

    std::string violation;
    if (check_forbidden_patterns(code, rust_patterns, violation)) {
        throw std::runtime_error("Rust code sanitization failed: " + violation);
    }

    return code;
}

std::string CodeSanitizer::remove_comments(const std::string& code, const std::string& language) {
    // Simple comment removal (C-style comments for C/C++)
    // This is a simplified version - a production system would use a proper parser

    std::string result;
    bool in_block_comment = false;
    bool in_line_comment = false;
    bool in_string = false;

    for (size_t i = 0; i < code.length(); ++i) {
        char c = code[i];
        char next = (i + 1 < code.length()) ? code[i + 1] : '\0';

        if (in_string) {
            result += c;
            if (c == '"' && (i == 0 || code[i-1] != '\\')) {
                in_string = false;
            }
            continue;
        }

        if (in_line_comment) {
            if (c == '\n') {
                in_line_comment = false;
                result += c;
            }
            continue;
        }

        if (in_block_comment) {
            if (c == '*' && next == '/') {
                in_block_comment = false;
                ++i; // Skip the '/'
            }
            continue;
        }

        if (c == '/' && next == '/') {
            in_line_comment = true;
            ++i;
            continue;
        }

        if (c == '/' && next == '*') {
            in_block_comment = true;
            ++i;
            continue;
        }

        if (c == '"') {
            in_string = true;
        }

        result += c;
    }

    return result;
}

std::string CodeSanitizer::sanitize_javascript(const std::string& code) {
    // JavaScript-specific sanitization
    std::vector<ForbiddenPattern> js_patterns = {
        {"require\\s*\\(\\s*['\"]child_process['\"]", "child_process module is forbidden", true},
        {"require\\s*\\(\\s*['\"]fs['\"]", "fs module is restricted", true},
        {"require\\s*\\(\\s*['\"]net['\"]", "net module is restricted", true},
        {"require\\s*\\(\\s*['\"]http['\"]", "http module is restricted", true},
        {"require\\s*\\(\\s*['\"]https['\"]", "https module is restricted", true},
        {"process\\.exit", "process.exit is restricted", false},
        {"eval\\s*\\(", "eval() is forbidden", true},
        {"Function\\s*\\(", "Function() constructor is forbidden", true},
        {"globalThis", "globalThis access is restricted", false},
        {"global\\.", "global object access is restricted", false},
        {"__dirname", "__dirname is restricted", false},
        {"__filename", "__filename is restricted", false},
    };

    std::string violation;
    if (check_forbidden_patterns(code, js_patterns, violation)) {
        throw std::runtime_error("JavaScript code sanitization failed: " + violation);
    }

    // Basic validation
    if (code.size() > 1024 * 1024) { // 1MB limit
        throw std::runtime_error("Code size exceeds maximum allowed size");
    }

    return code;
}

std::string CodeSanitizer::sanitize_typescript(const std::string& code) {
    // TypeScript uses same restrictions as JavaScript plus type-specific ones
    std::string violation;
    
    // First apply JavaScript sanitization
    try {
        sanitize_javascript(code);
    } catch (const std::exception& e) {
        throw std::runtime_error("TypeScript code sanitization failed: " + std::string(e.what()));
    }
    
    // TypeScript-specific patterns
    std::vector<ForbiddenPattern> ts_patterns = {
        {"declare\\s+global", "Global namespace modification is restricted", true},
        {"@ts-ignore", "TypeScript error suppression is discouraged", false},
        {"@ts-nocheck", "TypeScript checking bypass is forbidden", false},
    };

    if (check_forbidden_patterns(code, ts_patterns, violation)) {
        throw std::runtime_error("TypeScript code sanitization failed: " + violation);
    }

    return code;
}
