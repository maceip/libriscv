#!/bin/bash
set -e

echo "Building libriscv MCP Code Execution Server..."

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
echo "Building..."
make -j$(nproc)

echo ""
echo "✓ Build complete!"
echo ""
echo "Executable: $(pwd)/mcp-server"
echo ""
echo "To run: ./build/mcp-server"
echo "To test: ./test_client.py"
