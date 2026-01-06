# Installation Guide

Complete installation instructions for the libriscv MCP Code Execution Server.

## Prerequisites

### System Requirements
- Linux (Ubuntu 20.04+, Fedora 33+, or similar)
- 2GB+ RAM
- 1GB+ disk space

### Required Software

#### 1. RISC-V Cross-Compiler Toolchain

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    g++-riscv64-linux-gnu \
    gcc-riscv64-linux-gnu
```

**Fedora/RHEL:**
```bash
sudo dnf install -y \
    cmake \
    gcc-c++ \
    gcc-riscv64-linux-gnu \
    g++-riscv64-linux-gnu
```

**Arch Linux:**
```bash
sudo pacman -S \
    cmake \
    riscv64-linux-gnu-gcc
```

#### 2. nlohmann/json (Optional)

The build will auto-fetch this if not found, but you can install it manually:

```bash
# Ubuntu/Debian
sudo apt-get install nlohmann-json3-dev

# Fedora
sudo dnf install json-devel

# Arch
sudo pacman -S nlohmann-json
```

#### 3. Python 3 (for testing)

```bash
# Ubuntu/Debian
sudo apt-get install python3

# Fedora
sudo dnf install python3

# Should already be installed on most systems
```

## Building from Source

### 1. Clone the Repository

```bash
git clone https://github.com/fwsGonzo/libriscv.git
cd libriscv/examples/mcp-server
```

### 2. Build the Server

**Option A: Using the build script (recommended)**
```bash
./build.sh
```

**Option B: Manual build**
```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### 3. Verify the Build

```bash
./build/mcp-server --help  # Should show usage or wait for input
```

Test with the Python client:
```bash
./test_client.py
```

## Installation

### System-wide Installation

```bash
cd build
sudo make install
```

This installs:
- `/usr/local/bin/mcp-server` - The executable
- `/usr/local/share/mcp-server/servers/` - Tool definitions

### Local Installation

You can run the server directly from the build directory without installation:

```bash
./build/mcp-server
```

## Configuration

### Claude Desktop

1. Locate your Claude Desktop config file:
   - **macOS**: `~/Library/Application Support/Claude/claude_desktop_config.json`
   - **Windows**: `%APPDATA%\Claude\claude_desktop_config.json`
   - **Linux**: `~/.config/Claude/claude_desktop_config.json`

2. Add the server configuration:

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

3. Replace `/path/to/` with the actual path to your build directory.

4. Restart Claude Desktop.

### Other MCP Clients

For other MCP clients, configure them to run:

```bash
/path/to/mcp-server
```

The server communicates via stdio using JSON-RPC 2.0.

## Verification

### Test the Server Manually

Start the server:
```bash
./build/mcp-server
```

Send an initialize request (paste this and press Enter):
```json
{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}
```

You should receive a JSON response with server capabilities.

### Automated Testing

Run the test suite:
```bash
./test_client.py
```

Expected output:
```
================================================================================
libriscv MCP Code Execution Server - Test Client
================================================================================

✓ Server started
...
ALL TESTS PASSED! ✓
================================================================================
```

## Troubleshooting

### "riscv64-linux-gnu-g++ not found"

**Solution**: Install the RISC-V cross-compiler:
```bash
sudo apt-get install g++-riscv64-linux-gnu gcc-riscv64-linux-gnu
```

### CMake version too old

**Solution**: Install a newer CMake:
```bash
# Ubuntu 20.04
wget https://github.com/Kitware/CMake/releases/download/v3.25.0/cmake-3.25.0-linux-x86_64.sh
chmod +x cmake-3.25.0-linux-x86_64.sh
sudo ./cmake-3.25.0-linux-x86_64.sh --prefix=/usr/local --skip-license
```

### "fatal error: nlohmann/json.hpp: No such file or directory"

**Solution**: The build should auto-fetch this. If it doesn't:
```bash
cd build
rm -rf CMakeCache.txt CMakeFiles/
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Server doesn't respond

**Check**:
1. Server is running: `ps aux | grep mcp-server`
2. Requests are single-line JSON with newline
3. Check stderr for error messages

### Compilation fails in the sandbox

**Check**:
1. RISC-V compiler is in PATH: `which riscv64-linux-gnu-g++`
2. Compiler version is 10 or later: `riscv64-linux-gnu-g++ --version`

### Permission denied when executing

**Solution**:
```bash
chmod +x build/mcp-server
chmod +x build.sh
chmod +x test_client.py
```

## Uninstallation

### Remove System Installation

```bash
cd build
sudo make uninstall
# Or manually:
sudo rm /usr/local/bin/mcp-server
sudo rm -rf /usr/local/share/mcp-server
```

### Clean Build Files

```bash
rm -rf build/
```

## Updating

```bash
cd /path/to/libriscv
git pull
cd examples/mcp-server
./build.sh
```

## Platform-Specific Notes

### macOS

On macOS, you'll need to install the cross-compiler via Homebrew:

```bash
brew tap riscv/riscv
brew install riscv-gnu-toolchain
```

The compiler may be named differently. Check with:
```bash
brew list riscv-gnu-toolchain
```

### Windows (WSL)

Use Windows Subsystem for Linux (WSL2) and follow the Ubuntu instructions.

### Docker

A Dockerfile is provided for containerized deployment:

```bash
docker build -t libriscv-mcp-server .
docker run -i libriscv-mcp-server
```

## Next Steps

- Read the [README.md](README.md) for usage examples
- Check the [tool documentation](servers/code-execution/)
- Try the example code in the test client

## Support

For issues and questions:
- GitHub Issues: https://github.com/fwsGonzo/libriscv/issues
- Documentation: https://github.com/fwsGonzo/libriscv

## Security Considerations

Before deploying in production:

1. Review the [Security section in README.md](README.md#security)
2. Configure appropriate resource limits
3. Consider adding authentication if exposing over network
4. Enable logging for audit trails
5. Review and customize code sanitization rules

The server is designed for use by trusted AI assistants. For untrusted code execution, additional hardening is recommended.
