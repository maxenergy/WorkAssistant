#!/bin/bash

# Script to download and setup PaddlePaddle Inference Library

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
THIRD_PARTY_DIR="$PROJECT_ROOT/third_party"
INSTALL_DIR="$THIRD_PARTY_DIR/install/paddle_inference"

# PaddlePaddle Inference Library version
PADDLE_VERSION="2.5.2"
PLATFORM="linux"
ARCH="x64"
CUDA_VERSION="cpu"  # Use CPU version for simplicity

# Download URL
if [ "$PLATFORM" == "linux" ]; then
    FILENAME="paddle_inference_c_${PADDLE_VERSION}-${PLATFORM}-${ARCH}-gcc82-${CUDA_VERSION}.tgz"
    DOWNLOAD_URL="https://paddle-inference-lib.bj.bcebos.com/${PADDLE_VERSION}/cxx_c/Linux/CPU/gcc8.2/${FILENAME}"
elif [ "$PLATFORM" == "win" ]; then
    FILENAME="paddle_inference_c_${PADDLE_VERSION}-${PLATFORM}-${ARCH}-${CUDA_VERSION}.zip"
    DOWNLOAD_URL="https://paddle-inference-lib.bj.bcebos.com/${PADDLE_VERSION}/cxx_c/Windows/CPU/${FILENAME}"
fi

echo "=== PaddlePaddle Inference Library Download Script ==="
echo "Version: $PADDLE_VERSION"
echo "Platform: $PLATFORM"
echo "Architecture: $ARCH"
echo "CUDA: $CUDA_VERSION"
echo "Install directory: $INSTALL_DIR"
echo

# Create directories
mkdir -p "$THIRD_PARTY_DIR/downloads"
mkdir -p "$INSTALL_DIR"

# Download if not exists
DOWNLOAD_FILE="$THIRD_PARTY_DIR/downloads/$FILENAME"
if [ ! -f "$DOWNLOAD_FILE" ]; then
    echo "Downloading PaddlePaddle Inference Library..."
    echo "URL: $DOWNLOAD_URL"
    wget -O "$DOWNLOAD_FILE" "$DOWNLOAD_URL" || {
        echo "Failed to download. Trying alternative URL..."
        # Alternative URL for CPU version
        ALT_URL="https://paddle-inference-lib.bj.bcebos.com/${PADDLE_VERSION}/cxx_c/Linux/CPU/gcc8.2/paddle_inference_c_${PADDLE_VERSION}-linux-x64-gcc82-shared-std.tgz"
        wget -O "$DOWNLOAD_FILE" "$ALT_URL" || {
            echo "Download failed. Please download manually from:"
            echo "https://www.paddlepaddle.org.cn/inference/master/guides/install/download_lib.html"
            exit 1
        }
    }
else
    echo "Using cached download: $DOWNLOAD_FILE"
fi

# Extract
echo "Extracting PaddlePaddle Inference Library..."
if [[ "$FILENAME" == *.tgz ]] || [[ "$FILENAME" == *.tar.gz ]]; then
    tar -xzf "$DOWNLOAD_FILE" -C "$THIRD_PARTY_DIR/install/" || {
        echo "Extraction failed. Archive might be corrupted."
        rm -f "$DOWNLOAD_FILE"
        echo "Please run this script again."
        exit 1
    }
elif [[ "$FILENAME" == *.zip ]]; then
    unzip -o "$DOWNLOAD_FILE" -d "$THIRD_PARTY_DIR/install/"
fi

# Find the extracted directory
EXTRACTED_DIR=$(find "$THIRD_PARTY_DIR/install" -maxdepth 1 -name "paddle_inference*" -type d | head -1)

if [ -z "$EXTRACTED_DIR" ]; then
    echo "Error: Could not find extracted PaddlePaddle directory"
    exit 1
fi

# Move to final location if needed
if [ "$EXTRACTED_DIR" != "$INSTALL_DIR" ]; then
    echo "Moving to final location..."
    rm -rf "$INSTALL_DIR"
    mv "$EXTRACTED_DIR" "$INSTALL_DIR"
fi

# Verify installation
if [ -f "$INSTALL_DIR/paddle/include/paddle_c_api.h" ]; then
    echo "✓ PaddlePaddle Inference Library installed successfully!"
    echo "  Include dir: $INSTALL_DIR/paddle/include"
    echo "  Library dir: $INSTALL_DIR/paddle/lib"
    
    # Create a CMake config file for easier integration
    cat > "$INSTALL_DIR/PaddleInferenceConfig.cmake" << EOF
# PaddlePaddle Inference Library Configuration
set(PADDLE_INFERENCE_FOUND TRUE)
set(PADDLE_INFERENCE_ROOT "${INSTALL_DIR}")
set(PADDLE_INFERENCE_INCLUDE_DIRS "\${PADDLE_INFERENCE_ROOT}/paddle/include")
set(PADDLE_INFERENCE_LIB_DIR "\${PADDLE_INFERENCE_ROOT}/paddle/lib")

# Find libraries
find_library(PADDLE_INFERENCE_LIB paddle_inference_c PATHS \${PADDLE_INFERENCE_LIB_DIR} NO_DEFAULT_PATH)

if(NOT PADDLE_INFERENCE_LIB)
    set(PADDLE_INFERENCE_FOUND FALSE)
    message(WARNING "PaddlePaddle Inference library not found")
else()
    set(PADDLE_INFERENCE_LIBRARIES \${PADDLE_INFERENCE_LIB})
    message(STATUS "Found PaddlePaddle Inference: \${PADDLE_INFERENCE_LIB}")
endif()
EOF
    
    echo
    echo "To use in CMake, add:"
    echo "  list(APPEND CMAKE_PREFIX_PATH \"$INSTALL_DIR\")"
    echo "  find_package(PaddleInference REQUIRED)"
else
    echo "✗ Installation verification failed"
    echo "  Expected header not found: $INSTALL_DIR/paddle/include/paddle_c_api.h"
    exit 1
fi

echo
echo "=== Installation Complete ==="