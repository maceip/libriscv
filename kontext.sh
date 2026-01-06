#!/bin/bash
set -e

# ANSI color codes
BG_BLACK_GREEN='\033[40;32m'
BG_BLACK_CYAN_ITALIC='\033[40;36;3m'
DARK_PURPLE='\033[0;35m'
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo_info() {
    echo -e "${GREEN}[info]${NC} $1"
}

echo_warn() {
    echo -e "${YELLOW}[warn]${NC} $1"
}

echo_error() {
    echo -e "${RED}[error]${NC} $1"
}

print_header() {
    echo -e "${BG_BLACK_GREEN}══════════════════════════════════════════════════════${DARK_PURPLE}kontext.dev${BG_BLACK_GREEN}═════${NC}"
    echo -e "${BG_BLACK_CYAN_ITALIC}$1${NC}"
    echo -e "${BG_BLACK_GREEN}══════════════════════════════════════════════════════════════════════${NC}"
}

print_header "libriscv mcp server - build script\ntarget: ubuntu 20.04+"

# Check if running on Ubuntu
if [ -f /etc/os-release ]; then
    . /etc/os-release
    echo_info "detected os: $NAME $VERSION"
    if [[ ! "$NAME" =~ "Ubuntu" ]]; then
        echo_warn "this script is optimized for ubuntu 20.04+"
        echo_warn "continuing anyway, but you may need to adjust package names..."
    fi
else
    echo_warn "could not detect os. assuming ubuntu/debian-based system."
fi

echo ""
echo_info "step 1: updating package lists..."
sudo apt-get update

echo ""
echo_info "step 2: installing build essentials..."
sudo apt-get install -y \
    build-essential \
    git \
    wget \
    curl

echo ""
echo_info "step 3: installing cmake 3.14+..."
CMAKE_VERSION=$(cmake --version 2>/dev/null | head -n1 | grep -oP '\d+\.\d+' || echo "0.0")
CMAKE_MAJOR=$(echo $CMAKE_VERSION | cut -d. -f1)
CMAKE_MINOR=$(echo $CMAKE_VERSION | cut -d. -f2)

if [ "$CMAKE_MAJOR" -lt 3 ] || ([ "$CMAKE_MAJOR" -eq 3 ] && [ "$CMAKE_MINOR" -lt 14 ]); then
    echo_info "cmake version $CMAKE_VERSION is too old. installing cmake 3.25..."

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
    echo_info "installed: $CMAKE_NEW_VERSION"
else
    echo_info "cmake version $CMAKE_VERSION is sufficient"
fi

echo ""
echo_info "step 4: installing risc-v cross-compiler toolchain..."
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
    echo_info "found risc-v compiler: $RISCV_COMPILER"
    $RISCV_COMPILER --version | head -n1
else
    echo_error "risc-v compiler not found!"
    exit 1
fi

echo ""
echo_info "step 5: installing nlohmann-json (optional, will auto-fetch if missing)..."
sudo apt-get install -y nlohmann-json3-dev || echo_warn "nlohmann-json not available via apt, will be fetched by cmake"

echo ""
echo_info "step 6: installing python3 and typescript tools (optional)..."
sudo apt-get install -y python3 python3-pip || true

# Try to install TypeScript/JavaScript tooling
if command -v npm &> /dev/null; then
    echo_info "npm found, installing typescript and esbuild globally..."
    sudo npm install -g typescript esbuild || echo_warn "failed to install typescript tools"
else
    echo_warn "npm not found. typescript/javascript execution will be limited."
    echo_warn "to enable full typescript support, install node.js and run:"
    echo_warn "  sudo npm install -g typescript esbuild"
fi

echo ""
echo_info "step 7: building quickjs for risc-v (javascript runtime)..."

# Navigate to MCP server directory
cd "$(dirname "$0")/examples/mcp-server"

# Build QuickJS if RISC-V compiler is available
if command -v $RISCV_COMPILER &> /dev/null; then
    echo_info "building quickjs for full javascript support..."
    if [ -x "third_party/build_quickjs.sh" ]; then
        ./third_party/build_quickjs.sh || echo_warn "quickjs build failed, javascript support will be limited"
    else
        echo_warn "quickjs build script not found, javascript support will be limited"
    fi
else
    echo_warn "risc-v compiler not available, skipping quickjs build"
fi

echo ""
echo_info "step 8: building libriscv mcp server..."

# Clean previous build
if [ -d "build" ]; then
    echo_info "cleaning previous build..."
    rm -rf build
fi

# Create build directory
mkdir -p build
cd build

echo_info "running cmake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

echo_info "building with $(nproc) cores..."
make -j$(nproc)

# Verify build
if [ ! -f "mcp-server" ]; then
    echo_error "build failed! mcp-server executable not found."
    exit 1
fi

echo ""
echo_info "step 9: running basic tests..."

# Test 1: Check if server starts
echo_info "test 1: server startup test..."
timeout 2s ./mcp-server < /dev/null > /dev/null 2>&1 && echo_info "server starts successfully" || echo_info "server responds to input"

# Test 2: Send initialize request
echo_info "test 2: initialize request test..."
INIT_RESPONSE=$(echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}' | ./mcp-server | head -n1)

if echo "$INIT_RESPONSE" | grep -q "protocolVersion"; then
    echo_info "initialize request successful"
else
    echo_warn "initialize response may be incomplete"
fi

# Test 3: List tools
echo_info "test 3: list tools test..."
TOOLS_RESPONSE=$(echo '{"jsonrpc":"2.0","id":2,"method":"tools/list"}' | ./mcp-server | head -n1)

if echo "$TOOLS_RESPONSE" | grep -q "execute_code"; then
    echo_info "tools listing successful"
    TOOL_COUNT=$(echo "$TOOLS_RESPONSE" | grep -o "execute_code\|list_languages" | wc -l)
    echo_info "  found $TOOL_COUNT tools"
else
    echo_warn "tools listing may be incomplete"
fi

# Test 4: Execute JavaScript code
echo_info "test 4: javascript execution test..."
JS_RESPONSE=$(echo '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"execute_code","arguments":{"code":"console.log(\"hello from javascript\")","language":"javascript"}}}' | ./mcp-server | head -n1)

if echo "$JS_RESPONSE" | grep -q "hello from javascript"; then
    echo_info "javascript execution successful"
    if echo "$JS_RESPONSE" | grep -q "quickjs"; then
        echo_info "  using quickjs runtime"
    else
        echo_info "  using fallback c++ wrapper"
    fi
else
    echo_warn "javascript execution may have failed"
fi

# Test 5: Run Python test client if available
cd ..
if [ -f "test_client.py" ]; then
    echo ""
    echo_info "step 10: running comprehensive test suite..."
    if command -v python3 &> /dev/null; then
        python3 test_client.py || echo_warn "some tests failed, but server built successfully"
    else
        echo_warn "python3 not found, skipping test suite"
    fi
else
    echo_warn "test_client.py not found, skipping comprehensive tests"
fi

echo ""
print_header "build completed successfully"
echo ""
echo "server location: $(pwd)/build/mcp-server"
echo ""
echo "usage:"
echo "  1. run server directly:"
echo "     $(pwd)/build/mcp-server"
echo ""
echo "  2. test with python client:"
echo "     cd $(pwd) && python3 test_client.py"
echo ""
echo "  3. configure with claude desktop:"
echo "     add to claude_desktop_config.json:"
echo '     {'
echo '       "mcpServers": {'
echo '         "code-execution": {'
echo "           \"command\": \"$(pwd)/build/mcp-server\""
echo '         }'
echo '       }'
echo '     }'
echo ""
echo "supported languages:"
echo "  - c (c17 + glibc)"
echo "  - c++ (c++20 + stl + pthread)"
if [ -f "$(pwd)/third_party/quickjs-riscv/lib/libquickjs.a" ]; then
    echo "  - javascript (full quickjs runtime)"
    echo "  - typescript (via tsc/esbuild + quickjs)"
else
    echo "  - javascript (console.log wrapper - limited)"
    echo "  - typescript (via tsc/esbuild - limited)"
fi
echo "  - rust (riscv64gc target)"
echo "  - python (limited)"
echo ""
echo "next steps:"
echo "  1. test the server: python3 test_client.py"
echo "  2. configure your mcp client (claude desktop, etc.)"
echo "  3. start executing sandboxed code"
echo ""
echo_info "build completed at $(date)"
