#!/bin/bash

# Simple build script for Work Assistant

echo "Building Work Assistant (simplified version)..."

# Create minimal build directory
cd /workspaces/WorkAssistant
rm -rf build_simple
mkdir build_simple
cd build_simple

# Configure with minimal dependencies
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DWITH_OPENCV=OFF \
    -DWITH_PADDLE=OFF \
    -DWITH_DROGON=OFF \
    -DWITH_SCREEN_CAPTURE_LITE=OFF

echo "Configuration complete. Building..."

# Build core components only
make core_lib -j2
make platform_lib -j2  
make storage_lib -j2
make web_lib -j2
make ai_lib -j2

# Build main executable
make work_study_assistant -j2

echo "Build complete!"
echo "Executable: ./work_study_assistant"