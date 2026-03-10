#!/bin/bash

# Exit on any error
set -e

# Add homebrew to PATH for cmake
export PATH="/opt/homebrew/bin:$PATH"

# Define directories
BUILD_DIR="build"

echo "==================================="
echo " Building logiq-agent"
echo "==================================="

# Parse arguments
CLEAN=0
BUILD_TYPE="Release"

for arg in "$@"; do
    case "$arg" in
        --clean)
            CLEAN=1
            ;;
        --debug)
            BUILD_TYPE="Debug"
            ;;
        --help)
            echo "Usage: ./build.sh [--clean] [--debug] [--help]"
            echo "  --clean : Remove existing build directory before building"
            echo "  --debug : Build with Debug configuration (default is Release)"
            exit 0
            ;;
    esac
done

if [ "$CLEAN" -eq 1 ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Configuring project ($BUILD_TYPE mode)..."
cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" ..

echo "Building project..."
# Use number of available cores for faster compilation
CORES=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
cmake --build . --config "$BUILD_TYPE" -j "$CORES"

echo "==================================="
echo " Build successful!"
echo " Executable is located at: $BUILD_DIR/logiq-agent"
echo "==================================="
