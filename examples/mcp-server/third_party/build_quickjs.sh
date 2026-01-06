#!/bin/bash
set -e

# QuickJS build script for RISC-V
# This script downloads and cross-compiles QuickJS to RISC-V

QUICKJS_VERSION="2024-01-13"
QUICKJS_URL="https://bellard.org/quickjs/quickjs-${QUICKJS_VERSION}.tar.xz"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/quickjs-build"
INSTALL_DIR="${SCRIPT_DIR}/quickjs-riscv"

echo "======================================"
echo "Building QuickJS for RISC-V"
echo "======================================"

# Find RISC-V compiler
RISCV_CC=""
for version in 14 13 12 11 10; do
    if command -v riscv64-linux-gnu-gcc-$version &> /dev/null; then
        RISCV_CC="riscv64-linux-gnu-gcc-$version"
        RISCV_CXX="riscv64-linux-gnu-g++-$version"
        break
    fi
done

if [ -z "$RISCV_CC" ]; then
    RISCV_CC="riscv64-linux-gnu-gcc"
    RISCV_CXX="riscv64-linux-gnu-g++"
fi

if ! command -v $RISCV_CC &> /dev/null; then
    echo "ERROR: RISC-V compiler not found!"
    echo "Please install: sudo apt-get install gcc-riscv64-linux-gnu g++-riscv64-linux-gnu"
    exit 1
fi

echo "Using RISC-V compiler: $RISCV_CC"
$RISCV_CC --version | head -n1

# Clean previous build
rm -rf "$BUILD_DIR" "$INSTALL_DIR"
mkdir -p "$BUILD_DIR" "$INSTALL_DIR"

cd "$BUILD_DIR"

# Download QuickJS
echo "Downloading QuickJS..."
if [ ! -f "quickjs-${QUICKJS_VERSION}.tar.xz" ]; then
    wget -q "$QUICKJS_URL" || curl -L -o "quickjs-${QUICKJS_VERSION}.tar.xz" "$QUICKJS_URL"
fi

echo "Extracting QuickJS..."
tar xf "quickjs-${QUICKJS_VERSION}.tar.xz"
cd "quickjs-${QUICKJS_VERSION}"

# Create minimal Makefile for cross-compilation
cat > Makefile.riscv << 'EOF'
# Minimal QuickJS Makefile for RISC-V cross-compilation

CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
AR = $(CROSS_COMPILE)ar
STRIP = $(CROSS_COMPILE)strip

CFLAGS = -O2 -Wall -MMD -fPIC -fno-strict-aliasing
CFLAGS += -D_GNU_SOURCE -DCONFIG_VERSION=\"$(QUICKJS_VERSION)\"
CFLAGS += -DCONFIG_BIGNUM

SOURCES = quickjs.c libregexp.c libunicode.c cutils.c quickjs-libc.c libbf.c
OBJECTS = $(SOURCES:.c=.o)

all: libquickjs.a qjs

libquickjs.a: $(OBJECTS)
	$(AR) rcs $@ $^

qjs: qjs.o libquickjs.a
	$(CC) $(CFLAGS) -o $@ $^ -lm -ldl -lpthread

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) qjs.o libquickjs.a qjs *.d

-include *.d
EOF

# Build for RISC-V
echo "Building QuickJS for RISC-V..."
make -f Makefile.riscv \
    CROSS_COMPILE=riscv64-linux-gnu- \
    QUICKJS_VERSION="${QUICKJS_VERSION}" \
    -j$(nproc)

# Install
echo "Installing to $INSTALL_DIR..."
mkdir -p "$INSTALL_DIR/lib" "$INSTALL_DIR/include" "$INSTALL_DIR/bin"

cp libquickjs.a "$INSTALL_DIR/lib/"
cp qjs "$INSTALL_DIR/bin/"
cp quickjs.h quickjs-libc.h "$INSTALL_DIR/include/"

echo ""
echo "======================================"
echo "QuickJS built successfully!"
echo "======================================"
echo "Library: $INSTALL_DIR/lib/libquickjs.a"
echo "Binary: $INSTALL_DIR/bin/qjs"
echo "Headers: $INSTALL_DIR/include/"
echo ""

# Verify binary
file "$INSTALL_DIR/bin/qjs"
