/**
 * MCP Code Execution Server
 *
 * A Model Context Protocol (MCP) server that provides secure code execution
 * capabilities using libriscv as a sandboxed execution environment.
 *
 * This server implements the MCP protocol over stdio and provides tools for:
 * - Executing code in multiple languages (C, C++, Python, Rust, etc.)
 * - Listing available execution tools
 * - Reading tool specifications
 * - Secure sandboxed execution with resource limits
 */

#include "mcp_server.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

MCPServer::MCPServer() {
    initialize_tools();
}

void MCPServer::initialize_tools() {
    // Register available code execution tools
    tools_ = {
        {
            {"name", "execute_code"},
            {"description", "Execute code in a sandboxed RISC-V environment. Supports multiple languages including C, C++, Python, and Rust. Code is compiled to RISC-V, sanitized, and executed with strict resource limits."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"code", {
                        {"type", "string"},
                        {"description", "The source code to execute"}
                    }},
                    {"language", {
                        {"type", "string"},
                        {"enum", json::array({"c", "cpp", "python", "rust"})},
                        {"description", "Programming language of the code"},
                        {"default", "cpp"}
                    }},
                    {"timeout_seconds", {
                        {"type", "number"},
                        {"description", "Maximum execution time in seconds"},
                        {"default", 5},
                        {"minimum", 1},
                        {"maximum", 30}
                    }},
                    {"max_memory_mb", {
                        {"type", "number"},
                        {"description", "Maximum memory usage in megabytes"},
                        {"default", 32},
                        {"minimum", 1},
                        {"maximum", 128}
                    }}
                }},
                {"required", json::array({"code"})}
            }}
        },
        {
            {"name", "list_languages"},
            {"description", "List all supported programming languages and their compilation details"},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {}}
            }}
        }
    };
}

void MCPServer::run() {
    std::string line;

    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        try {
            auto request = json::parse(line);
            auto response = handle_request(request);

            std::cout << response.dump() << std::endl;
            std::cout.flush();
        } catch (const json::parse_error& e) {
            auto error_response = create_error_response(-1, -32700, "Parse error", e.what());
            std::cout << error_response.dump() << std::endl;
            std::cout.flush();
        } catch (const std::exception& e) {
            auto error_response = create_error_response(-1, -32603, "Internal error", e.what());
            std::cout << error_response.dump() << std::endl;
            std::cout.flush();
        }
    }
}

json MCPServer::handle_request(const json& request) {
    if (!request.contains("jsonrpc") || request["jsonrpc"] != "2.0") {
        return create_error_response(
            request.value("id", -1),
            -32600,
            "Invalid Request",
            "Missing or invalid jsonrpc version"
        );
    }

    std::string method = request.value("method", "");
    int id = request.value("id", -1);

    if (method == "initialize") {
        return handle_initialize(request);
    } else if (method == "tools/list") {
        return handle_list_tools(request);
    } else if (method == "tools/call") {
        return handle_call_tool(request);
    } else if (method == "resources/list") {
        return handle_list_resources(request);
    } else if (method == "resources/read") {
        return handle_read_resource(request);
    } else {
        return create_error_response(
            id,
            -32601,
            "Method not found",
            "The method '" + method + "' does not exist"
        );
    }
}

json MCPServer::handle_initialize(const json& request) {
    json response = {
        {"jsonrpc", "2.0"},
        {"id", request["id"]},
        {"result", {
            {"protocolVersion", "2024-11-05"},
            {"serverInfo", {
                {"name", "libriscv-mcp-server"},
                {"version", "1.0.0"}
            }},
            {"capabilities", {
                {"tools", {{"listChanged", false}}},
                {"resources", {{"listChanged", false}}}
            }}
        }}
    };

    return response;
}

json MCPServer::handle_list_tools(const json& request) {
    json response = {
        {"jsonrpc", "2.0"},
        {"id", request["id"]},
        {"result", {
            {"tools", tools_}
        }}
    };

    return response;
}

json MCPServer::handle_call_tool(const json& request) {
    auto params = request["params"];
    std::string tool_name = params["name"];
    auto arguments = params.value("arguments", json::object());

    try {
        json result;

        if (tool_name == "execute_code") {
            result = execute_code(arguments);
        } else if (tool_name == "list_languages") {
            result = list_languages(arguments);
        } else {
            return create_error_response(
                request["id"],
                -32602,
                "Invalid params",
                "Unknown tool: " + tool_name
            );
        }

        json response = {
            {"jsonrpc", "2.0"},
            {"id", request["id"]},
            {"result", {
                {"content", json::array({
                    {
                        {"type", "text"},
                        {"text", result.dump(2)}
                    }
                })}
            }}
        };

        return response;
    } catch (const std::exception& e) {
        return create_error_response(
            request["id"],
            -32603,
            "Tool execution failed",
            e.what()
        );
    }
}

json MCPServer::handle_list_resources(const json& request) {
    json resources = json::array();

    // List tool definition files in /servers/ directory
    std::string servers_dir = "./servers";

    if (fs::exists(servers_dir) && fs::is_directory(servers_dir)) {
        for (const auto& entry : fs::recursive_directory_iterator(servers_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".md") {
                std::string uri = "file://" + fs::absolute(entry.path()).string();
                std::string name = fs::relative(entry.path(), servers_dir).string();

                resources.push_back({
                    {"uri", uri},
                    {"name", name},
                    {"mimeType", "text/markdown"},
                    {"description", "Tool definition for " + name}
                });
            }
        }
    }

    json response = {
        {"jsonrpc", "2.0"},
        {"id", request["id"]},
        {"result", {
            {"resources", resources}
        }}
    };

    return response;
}

json MCPServer::handle_read_resource(const json& request) {
    auto params = request["params"];
    std::string uri = params["uri"];

    // Parse file:// URI
    if (uri.substr(0, 7) != "file://") {
        return create_error_response(
            request["id"],
            -32602,
            "Invalid params",
            "Only file:// URIs are supported"
        );
    }

    std::string filepath = uri.substr(7);

    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return create_error_response(
                request["id"],
                -32602,
                "Resource not found",
                "Cannot open file: " + filepath
            );
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        json response = {
            {"jsonrpc", "2.0"},
            {"id", request["id"]},
            {"result", {
                {"contents", json::array({
                    {
                        {"uri", uri},
                        {"mimeType", "text/markdown"},
                        {"text", content}
                    }
                })}
            }}
        };

        return response;
    } catch (const std::exception& e) {
        return create_error_response(
            request["id"],
            -32603,
            "Failed to read resource",
            e.what()
        );
    }
}

json MCPServer::create_error_response(int id, int code, const std::string& message, const std::string& data) {
    json error = {
        {"code", code},
        {"message", message}
    };

    if (!data.empty()) {
        error["data"] = data;
    }

    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", error}
    };
}

json MCPServer::list_languages(const json& arguments) {
    json languages = {
        {"supported_languages", json::array({
            {
                {"language", "c"},
                {"compiler", "riscv64-linux-gnu-gcc"},
                {"standard", "c17"},
                {"description", "C language with glibc support"}
            },
            {
                {"language", "cpp"},
                {"compiler", "riscv64-linux-gnu-g++"},
                {"standard", "c++20"},
                {"description", "C++ with full standard library and threading support"}
            },
            {
                {"language", "python"},
                {"interpreter", "python3"},
                {"version", "3.x"},
                {"description", "Python code (compiled to C extension)"}
            },
            {
                {"language", "rust"},
                {"compiler", "rustc"},
                {"target", "riscv64gc-unknown-linux-gnu"},
                {"description", "Rust with full standard library"}
            }
        })},
        {"default_limits", {
            {"max_instructions", 36000000},
            {"max_memory_bytes", 33554432},
            {"timeout_seconds", 5}
        }}
    };

    return languages;
}

int main() {
    // Disable buffering for stdio
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cout.tie(nullptr);

    try {
        MCPServer server;
        server.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
