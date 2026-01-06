/**
 * MCP Server Header
 *
 * Defines the MCPServer class and CodeExecutor for sandboxed code execution
 */

#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class MCPServer {
public:
    MCPServer();

    /**
     * Main server loop - reads JSON-RPC requests from stdin
     * and writes responses to stdout
     */
    void run();

private:
    json tools_;

    void initialize_tools();

    json handle_request(const json& request);
    json handle_initialize(const json& request);
    json handle_list_tools(const json& request);
    json handle_call_tool(const json& request);
    json handle_list_resources(const json& request);
    json handle_read_resource(const json& request);

    json create_error_response(
        int id,
        int code,
        const std::string& message,
        const std::string& data = ""
    );

    // Tool implementations
    json execute_code(const json& arguments);
    json list_languages(const json& arguments);
};
