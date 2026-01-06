# libriscv MCP Code Execution Server

A **Model Context Protocol (MCP)** server that provides secure, sandboxed code execution capabilities using [libriscv](https://github.com/fwsGonzo/libriscv) as the execution environment.

This server enables AI assistants and other MCP clients to execute code safely in multiple programming languages with strict resource controls and security sandboxing.

## Features

### 🔒 Secure Sandboxed Execution
- **RISC-V Virtual Machine**: Code runs in a complete RISC-V sandbox isolated from the host
- **No Host Access**: Zero access to host filesystem, network, or system resources
- **Resource Limits**: Configurable memory, instruction count, and timeout limits
- **Code Sanitization**: Automatic detection and blocking of dangerous code patterns

### 🌐 Multi-Language Support
- **C** - Full C17 support with glibc
- **C++** - Full C++20 with STL and threading (pthread)
- **JavaScript** - **Full QuickJS runtime** for complete JavaScript support (with fallback to console.log wrapper)
- **TypeScript** - Transpiled to JavaScript via tsc or esbuild, executed with QuickJS
- **Python** - Limited support via compilation
- **Rust** - Full Rust standard library support

### 📊 Detailed Execution Metrics
- Execution time (microseconds precision)
- Instruction count
- Memory usage
- Binary size
- Exit codes and exceptions

### 🔌 MCP Protocol Compliant
- JSON-RPC 2.0 over stdio
- Tool discovery via filesystem resources
- Progressive disclosure of capabilities
- Standard MCP server lifecycle

## Architecture

This implementation is based on the [varnish webapi example](../webapi/varnish) and extends it to support the MCP protocol with enhanced security and multi-language capabilities.

```
┌─────────────────┐
│   MCP Client    │ (Claude, other AI assistants)
│  (AI Assistant) │
└────────┬────────┘
         │ JSON-RPC over stdio
         │
┌────────▼────────────────────────┐
│     MCP Server                  │
│  ┌──────────────────────────┐  │
│  │  Protocol Handler        │  │
│  │  (JSON-RPC 2.0)          │  │
│  └──────────┬───────────────┘  │
│             │                   │
│  ┌──────────▼───────────────┐  │
│  │  Code Sanitizer          │  │
│  │  - Pattern matching      │  │
│  │  - Security checks       │  │
│  └──────────┬───────────────┘  │
│             │                   │
│  ┌──────────▼───────────────┐  │
│  │  Compiler                │  │
│  │  - RISC-V cross-compile  │  │
│  │  - Multi-language        │  │
│  └──────────┬───────────────┘  │
│             │                   │
│  ┌──────────▼───────────────┐  │
│  │  libriscv Sandbox        │  │
│  │  - RISC-V emulation      │  │
│  │  - Resource limits       │  │
│  │  - Syscall filtering     │  │
│  └──────────────────────────┘  │
└─────────────────────────────────┘
```

## Building

### Prerequisites

1. **RISC-V Cross-Compiler**:
   ```bash
   # Ubuntu/Debian
   sudo apt-get install g++-riscv64-linux-gnu gcc-riscv64-linux-gnu

   # Fedora
   sudo dnf install gcc-riscv64-linux-gnu g++-riscv64-linux-gnu
   ```

2. **CMake** (3.14+):
   ```bash
   sudo apt-get install cmake
   ```

3. **nlohmann/json** (auto-fetched if not found):
   ```bash
   sudo apt-get install nlohmann-json3-dev  # Optional
   ```

### Build Steps

```bash
# From the mcp-server directory
mkdir build
cd build
cmake ..
make -j$(nproc)

# The executable will be at: build/mcp-server
```

### Quick Build Script

```bash
./build.sh
```

## Usage

### Running the Server

The MCP server communicates over stdio using JSON-RPC 2.0:

```bash
./build/mcp-server
```

The server reads requests from stdin and writes responses to stdout.

### MCP Client Configuration

Add to your MCP client configuration (e.g., Claude Desktop):

```json
{
  "mcpServers": {
    "code-execution": {
      "command": "/path/to/libriscv/examples/mcp-server/build/mcp-server",
      "args": []
    }
  }
}
```

### Example Requests

#### Initialize Connection
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "initialize",
  "params": {
    "protocolVersion": "2024-11-05",
    "capabilities": {},
    "clientInfo": {
      "name": "example-client",
      "version": "1.0.0"
    }
  }
}
```

#### List Available Tools
```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "tools/list"
}
```

#### Execute C++ Code
```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "method": "tools/call",
  "params": {
    "name": "execute_code",
    "arguments": {
      "code": "#include <iostream>\nint main() {\n    std::cout << \"Hello from libriscv!\" << std::endl;\n    return 0;\n}",
      "language": "cpp",
      "timeout_seconds": 5,
      "max_memory_mb": 32
    }
  }
}
```

## Security

### Code Sanitization

The server automatically blocks:
- System calls: `system()`, `popen()`, `exec*()`
- Process operations: `fork()`, `vfork()`
- Dynamic loading: `dlopen()`, `dlsym()`
- Direct network operations: `socket()`, `bind()`, `connect()`
- Inline assembly (configurable)
- Path traversal in includes

### Resource Limits

Default limits (configurable per execution):
- **Max Instructions**: 36 million (prevents infinite loops)
- **Max Memory**: 32 MB (configurable up to 128 MB)
- **Max Execution Time**: 5 seconds (configurable up to 30s)
- **Max Binary Size**: 32 MB

### Sandboxing

libriscv provides complete isolation:
- Virtual memory management
- Syscall interception and filtering
- No direct hardware access
- Deterministic execution
- Resource accounting

## Tool Documentation

Tool definitions are available in the `servers/` directory for progressive disclosure:

- **[execute_code](servers/code-execution/execute_code.md)** - Execute code in multiple languages
- **[list_languages](servers/code-execution/list_languages.md)** - List supported languages and limits

These files can be read by MCP clients via the `resources/read` method.

## Examples

### Example 1: Hello World in C++

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "tools/call",
  "params": {
    "name": "execute_code",
    "arguments": {
      "code": "#include <iostream>\nint main() { std::cout << \"Hello!\" << std::endl; return 0; }",
      "language": "cpp"
    }
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "content": [{
      "type": "text",
      "text": "{\"success\":true,\"exit_code\":0,\"output\":\"Hello!\\n\",\"execution_time_us\":1234,\"instructions_executed\":5678,\"memory_used\":8192}"
    }]
  }
}
```

### Example 2: Fibonacci in C

**Code:**
```c
#include <stdio.h>

int fib(int n) {
    if (n <= 1) return n;
    return fib(n-1) + fib(n-2);
}

int main() {
    printf("fib(10) = %d\n", fib(10));
    return 0;
}
```

### Example 3: Threading in C++

**Code:**
```cpp
#include <iostream>
#include <thread>
#include <vector>

void worker(int id) {
    std::cout << "Thread " << id << std::endl;
}

int main() {
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; i++) {
        threads.emplace_back(worker, i);
    }
    for (auto& t : threads) {
        t.join();
    }
    return 0;
}
```

## Testing

Run the test client:

```bash
# Build the server first
./build.sh

# Run interactive test client
./test_client.sh
```

Or use the Python test client:

```bash
python3 test_client.py
```

## Performance

Typical execution times on modern hardware:

- **Compilation**: 50-500ms (depending on code complexity)
- **Execution**: 1-10ms (for simple programs)
- **Total overhead**: ~50-100ms per request

The server uses aggressive caching where possible and compiles with `-O2` optimizations.

## Comparison with varnish Example

This MCP server builds upon the [varnish webapi example](../webapi/varnish):

| Feature | Varnish (HTTP) | MCP Server (stdio) |
|---------|----------------|-------------------|
| Protocol | HTTP REST | JSON-RPC 2.0 over stdio |
| Interface | Web browser | MCP clients (AI) |
| Caching | Varnish cache | None (stateless) |
| Languages | C/C++ | C/C++/Rust/Python |
| Sanitization | Basic (Python) | Enhanced multi-language |
| Tool Discovery | N/A | MCP resources |
| Use Case | Interactive demo | AI agent integration |

## VA Compliance Notes

This server implements security controls suitable for environments requiring strict code execution governance:

1. **Sandboxing**: Complete isolation via RISC-V virtualization
2. **Code Review**: All code is sanitized before compilation
3. **Resource Limits**: Strict enforcement prevents resource exhaustion
4. **Audit Trail**: All executions can be logged (add logging as needed)
5. **No Persistence**: Stateless design, temporary files cleaned up
6. **Deterministic**: Same input produces same output

For production VA deployment, consider adding:
- Request/response logging
- Authentication and authorization
- Rate limiting
- Binary signature verification
- Extended sanitization rules

## Troubleshooting

### Compiler Not Found

If you see "riscv64-linux-gnu-g++ not found":
```bash
# Install RISC-V toolchain
sudo apt-get install g++-riscv64-linux-gnu
```

### JSON Parse Errors

Ensure requests are single-line JSON followed by newline:
```bash
echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}' | ./mcp-server
```

### Compilation Failures

Check that the code is valid for the target language and doesn't use unsupported features.

## Contributing

Contributions welcome! Areas for improvement:
- Additional language support (Go, JavaScript via QuickJS, etc.)
- Enhanced sanitization rules
- Performance optimizations
- Better error messages
- Caching of compiled binaries

## License

Same as libriscv - see main repository LICENSE.

## References

- [Model Context Protocol](https://modelcontextprotocol.io/)
- [Anthropic MCP Code Execution Blog Post](https://www.anthropic.com/engineering/code-execution-with-mcp)
- [libriscv Documentation](https://github.com/fwsGonzo/libriscv)
- [varnish Example](../webapi/varnish)

## Authors

Based on the libriscv webapi example by fwsGonzo.
MCP server implementation created for secure AI-assisted code execution.
