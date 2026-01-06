/**
 * Code Sanitizer
 *
 * Sanitizes source code before compilation to prevent malicious code execution.
 * This includes checking for dangerous patterns, syscalls, and constructs.
 */

#pragma once

#include <string>
#include <vector>
#include <regex>

class CodeSanitizer {
public:
    CodeSanitizer();

    /**
     * Sanitize code based on the language
     *
     * @param code Source code to sanitize
     * @param language Programming language
     * @return Sanitized code
     * @throws std::runtime_error if code contains forbidden patterns
     */
    std::string sanitize(const std::string& code, const std::string& language);

private:
    struct ForbiddenPattern {
        std::string pattern;
        std::string description;
        bool is_regex;
    };

    std::vector<ForbiddenPattern> forbidden_patterns_cpp_;
    std::vector<ForbiddenPattern> forbidden_patterns_c_;

    void initialize_patterns();

    bool check_forbidden_patterns(
        const std::string& code,
        const std::vector<ForbiddenPattern>& patterns,
        std::string& violation
    );

    std::string sanitize_cpp(const std::string& code);
    std::string sanitize_c(const std::string& code);
    std::string sanitize_python(const std::string& code);
    std::string sanitize_rust(const std::string& code);
    std::string sanitize_javascript(const std::string& code);
    std::string sanitize_typescript(const std::string& code);

    std::string remove_comments(const std::string& code, const std::string& language);
};
