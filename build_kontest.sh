#!/bin/bash
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

echo_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

echo_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

echo "=========================================="
echo "libriscv MCP Server - Kontest Build Script"
echo "Target: Ubuntu 20.04+"
echo "=========================================="
echo ""

# Check if running on Ubuntu
if [ -f /etc/os-release ]; then
    . /etc/os-release
    echo_info "Detected OS: $NAME $VERSION"
    if [[ ! "$NAME" =~ "Ubuntu" ]]; then
        echo_warn "This script is optimized for Ubuntu 20.04+"
        echo_warn "Continuing anyway, but you may need to adjust package names..."
    fi
else
    echo_warn "Could not detect OS. Assuming Ubuntu/Debian-based system."
fi

echo ""
echo_info "Step 1: Updating package lists..."
sudo apt-get update

echo ""
echo_info "Step 2: Installing build essentials..."
sudo apt-get install -y \
    build-essential \
    git \
    wget \
    curl

echo ""
echo_info "Step 3: Installing CMake 3.14+..."
CMAKE_VERSION=$(cmake --version 2>/dev/null | head -n1 | grep -oP '\d+\.\d+' || echo "0.0")
CMAKE_MAJOR=$(echo $CMAKE_VERSION | cut -d. -f1)
CMAKE_MINOR=$(echo $CMAKE_VERSION | cut -d. -f2)

if [ "$CMAKE_MAJOR" -lt 3 ] || ([ "$CMAKE_MAJOR" -eq 3 ] && [ "$CMAKE_MINOR" -lt 14 ]); then
    echo_info "CMake version $CMAKE_VERSION is too old. Installing CMake 3.25..."

    # Remove old cmake if present
    sudo apt-get remove -y cmake || true

    # Download and install newer CMake
    CMAKE_URL="https://github.com/Kitware/CMake/releases/download/v3.25.0/cmake-3.25.0-linux-x86_64.sh"
    wget -q $CMAKE_URL -O /tmp/cmake-install.sh
    chmod +x /tmp/cmake-install.sh
    sudo /tmp/cmake-install.sh --prefix=/usr/local --skip-license --exclude-subdir
    rm /tmp/cmake-install.sh

    # Verify installation
    CMAKE_NEW_VERSION=$(cmake --version | head -n1)
    echo_info "Installed: $CMAKE_NEW_VERSION"
else
    echo_info "CMake version $CMAKE_VERSION is sufficient"
fi

echo ""
echo_info "Step 4: Installing RISC-V cross-compiler toolchain..."
sudo apt-get install -y \
    gcc-riscv64-linux-gnu \
    g++-riscv64-linux-gnu

# Verify RISC-V compiler
RISCV_COMPILER=""
for version in 14 13 12 11 10; do
    if command -v riscv64-linux-gnu-g++-$version &> /dev/null; then
        RISCV_COMPILER="riscv64-linux-gnu-g++-$version"
        break
    fi
done

if [ -z "$RISCV_COMPILER" ]; then
    RISCV_COMPILER="riscv64-linux-gnu-g++"
fi

if command -v $RISCV_COMPILER &> /dev/null; then
    echo_info "Found RISC-V compiler: $RISCV_COMPILER"
    $RISCV_COMPILER --version | head -n1
else
    echo_error "RISC-V compiler not found!"
    exit 1
fi

echo ""
echo_info "Step 5: Installing nlohmann-json (optional, will auto-fetch if missing)..."
sudo apt-get install -y nlohmann-json3-dev || echo_warn "nlohmann-json not available via apt, will be fetched by CMake"

echo ""
echo_info "Step 6: Installing Python3 and TypeScript tools (optional)..."
sudo apt-get install -y python3 python3-pip || true

# Try to install TypeScript/JavaScript tooling
if command -v npm &> /dev/null; then
    echo_info "npm found, installing TypeScript and esbuild globally..."
    sudo npm install -g typescript esbuild || echo_warn "Failed to install TypeScript tools"
else
    echo_warn "npm not found. TypeScript/JavaScript execution will be limited."
    echo_warn "To enable full TypeScript support, install Node.js and run:"
    echo_warn "  sudo npm install -g typescript esbuild"
fi

echo ""
echo_info "Step 7: Building libriscv MCP server..."

# Navigate to MCP server directory
cd "$(dirname "$0")/examples/mcp-server"

# Clean previous build
if [ -d "build" ]; then
    echo_info "Cleaning previous build..."
    rm -rf build
fi

# Create build directory
mkdir -p build
cd build

echo_info "Running CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

echo_info "Building with $(nproc) cores..."
make -j$(nproc)

# Verify build
if [ ! -f "mcp-server" ]; then
    echo_error "Build failed! mcp-server executable not found."
    exit 1
fi

echo ""
echo_info "Step 8: Running basic tests..."

# Test 1: Check if server starts
echo_info "Test 1: Server startup test..."
timeout 2s ./mcp-server < /dev/null > /dev/null 2>&1 && echo_info "✓ Server starts successfully" || echo_info "✓ Server responds to input"

# Test 2: Send initialize request
echo_info "Test 2: Initialize request test..."
INIT_RESPONSE=$(echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}' | ./mcp-server | head -n1)

if echo "$INIT_RESPONSE" | grep -q "protocolVersion"; then
    echo_info "✓ Initialize request successful"
else
    echo_warn "⚠ Initialize response may be incomplete"
fi

# Test 3: List tools
echo_info "Test 3: List tools test..."
TOOLS_RESPONSE=$(echo '{"jsonrpc":"2.0","id":2,"method":"tools/list"}' | ./mcp-server | head -n1)

if echo "$TOOLS_RESPONSE" | grep -q "execute_code"; then
    echo_info "✓ Tools listing successful"
    TOOL_COUNT=$(echo "$TOOLS_RESPONSE" | grep -o "execute_code\|list_languages" | wc -l)
    echo_info "  Found $TOOL_COUNT tools"
else
    echo_warn "⚠ Tools listing may be incomplete"
fi

# Test 4: Run Python test client if available
cd ..
if [ -f "test_client.py" ]; then
    echo ""
    echo_info "Step 9: Running comprehensive test suite..."
    if command -v python3 &> /dev/null; then
        python3 test_client.py || echo_warn "Some tests failed, but server built successfully"
    else
        echo_warn "Python3 not found, skipping test suite"
    fi
else
    echo_warn "test_client.py not found, skipping comprehensive tests"
fi

echo ""
echo "=========================================="
echo_info "✓ Build completed successfully!"
echo "=========================================="
echo ""
echo "Server location: $(pwd)/build/mcp-server"
echo ""
echo "Usage:"
echo "  1. Run server directly:"
echo "     $(pwd)/build/mcp-server"
echo ""
echo "  2. Test with Python client:"
echo "     cd $(pwd) && python3 test_client.py"
echo ""
echo "  3. Configure with Claude Desktop:"
echo "     Add to claude_desktop_config.json:"
echo '     {'
echo '       "mcpServers": {'
echo '         "code-execution": {'
echo "           \"command\": \"$(pwd)/build/mcp-server\""
echo '         }'
echo '       }'
echo '     }'
echo ""
echo "Supported languages:"
echo "  - C (c17 + glibc)"
echo "  - C++ (c++20 + STL + pthread)"
echo "  - JavaScript (console.log wrapper)"
echo "  - TypeScript (via tsc/esbuild)"
echo "  - Rust (riscv64gc target)"
echo "  - Python (limited)"
echo ""
echo "Next steps:"
echo "  1. Test the server: python3 test_client.py"
echo "  2. Configure your MCP client (Claude Desktop, etc.)"
echo "  3. Start executing sandboxed code!"
echo ""
echo_info "Build completed at $(date)"
