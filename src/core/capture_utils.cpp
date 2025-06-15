#include "screen_capture.h"
#include <cstring>
#include <algorithm>
#include <fstream>
#include <vector>

namespace work_assistant {
namespace capture_utils {

// Calculate perceptual hash (dHash) for change detection
uint64_t CalculateHash(const CaptureFrame& frame) {
    if (!frame.IsValid()) {
        return 0;
    }

    // Resize to 9x8 for dHash algorithm
    const int hashWidth = 9;
    const int hashHeight = 8;
    
    // Simple downsampling
    std::vector<uint8_t> resized(hashWidth * hashHeight);
    
    for (int y = 0; y < hashHeight; ++y) {
        for (int x = 0; x < hashWidth; ++x) {
            // Map to original image coordinates
            int srcX = x * frame.width / hashWidth;
            int srcY = y * frame.height / hashHeight;
            
            // Get pixel value (convert to grayscale if needed)
            int actual_stride = (frame.stride > 0) ? frame.stride : frame.width * frame.bytes_per_pixel;
            int offset = srcY * actual_stride + srcX * frame.bytes_per_pixel;
            if (offset + frame.bytes_per_pixel <= static_cast<int>(frame.data.size())) {
                if (frame.bytes_per_pixel >= 3) {
                    // Convert RGB to grayscale
                    uint8_t r = frame.data[offset];
                    uint8_t g = frame.data[offset + 1];
                    uint8_t b = frame.data[offset + 2];
                    resized[y * hashWidth + x] = static_cast<uint8_t>((r + g + b) / 3);
                } else {
                    resized[y * hashWidth + x] = frame.data[offset];
                }
            }
        }
    }
    
    // Calculate hash by comparing adjacent pixels
    uint64_t hash = 0;
    int bit = 0;
    
    for (int y = 0; y < hashHeight; ++y) {
        for (int x = 0; x < hashWidth - 1; ++x) {
            int idx = y * hashWidth + x;
            if (resized[idx] > resized[idx + 1]) {
                hash |= (1ULL << bit);
            }
            bit++;
        }
    }
    
    return hash;
}

// Calculate difference between two hashes (Hamming distance)
int CompareHashes(uint64_t hash1, uint64_t hash2) {
    uint64_t diff = hash1 ^ hash2;
    int distance = 0;
    
    while (diff) {
        distance += diff & 1;
        diff >>= 1;
    }
    
    return distance;
}

// Convert frame to different format
bool ConvertFrame(const CaptureFrame& source, CaptureFrame& target, int targetBytesPerPixel) {
    if (!source.IsValid()) {
        return false;
    }
    
    target.width = source.width;
    target.height = source.height;
    target.bytes_per_pixel = targetBytesPerPixel;
    target.stride = source.width * targetBytesPerPixel;
    target.timestamp = source.timestamp;
    
    // Determine target format
    if (targetBytesPerPixel == 4) {
        target.format = ImageFormat::RGBA;
    } else if (targetBytesPerPixel == 3) {
        target.format = ImageFormat::RGB;
    } else if (targetBytesPerPixel == 1) {
        target.format = ImageFormat::GRAY;
    } else {
        return false;
    }
    
    target.data.resize(target.height * target.stride);
    
    // Perform conversion
    int src_stride = (source.stride > 0) ? source.stride : source.width * source.bytes_per_pixel;
    for (int y = 0; y < source.height; ++y) {
        for (int x = 0; x < source.width; ++x) {
            int srcOffset = y * src_stride + x * source.bytes_per_pixel;
            int dstOffset = y * target.stride + x * target.bytes_per_pixel;
            
            if (source.bytes_per_pixel == 4 && target.bytes_per_pixel == 3) {
                // RGBA to RGB
                std::memcpy(&target.data[dstOffset], &source.data[srcOffset], 3);
            } else if (source.bytes_per_pixel == 3 && target.bytes_per_pixel == 4) {
                // RGB to RGBA
                std::memcpy(&target.data[dstOffset], &source.data[srcOffset], 3);
                target.data[dstOffset + 3] = 255; // Alpha
            } else if (source.bytes_per_pixel >= 3 && target.bytes_per_pixel == 1) {
                // RGB/RGBA to Grayscale
                uint8_t r = source.data[srcOffset];
                uint8_t g = source.data[srcOffset + 1];
                uint8_t b = source.data[srcOffset + 2];
                target.data[dstOffset] = static_cast<uint8_t>((r + g + b) / 3);
            } else {
                // Direct copy for same format
                std::memcpy(&target.data[dstOffset], &source.data[srcOffset], 
                           std::min(source.bytes_per_pixel, target.bytes_per_pixel));
            }
        }
    }
    
    return true;
}

// Save frame to file (for debugging)
bool SaveFrameToFile(const CaptureFrame& frame, const std::string& filename) {
    if (!frame.IsValid()) {
        return false;
    }
    
    // Save as PPM (simple format)
    if (frame.bytes_per_pixel >= 3) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            return false;
        }
        
        // PPM header
        file << "P6\n" << frame.width << " " << frame.height << "\n255\n";
        
        // Write pixel data
        int actual_stride = (frame.stride > 0) ? frame.stride : frame.width * frame.bytes_per_pixel;
        for (int y = 0; y < frame.height; ++y) {
            for (int x = 0; x < frame.width; ++x) {
                int offset = y * actual_stride + x * frame.bytes_per_pixel;
                file.write(reinterpret_cast<const char*>(&frame.data[offset]), 3);
            }
        }
        
        return true;
    } else if (frame.bytes_per_pixel == 1) {
        // Save as PGM for grayscale
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            return false;
        }
        
        // PGM header
        file << "P5\n" << frame.width << " " << frame.height << "\n255\n";
        
        // Write pixel data
        int actual_stride = (frame.stride > 0) ? frame.stride : frame.width * frame.bytes_per_pixel;
        for (int y = 0; y < frame.height; ++y) {
            file.write(reinterpret_cast<const char*>(&frame.data[y * actual_stride]), frame.width);
        }
        
        return true;
    }
    
    return false;
}

// Crop frame to region
bool CropFrame(const CaptureFrame& source, CaptureFrame& target,
               int x, int y, int width, int height) {
    if (!source.IsValid()) {
        return false;
    }
    
    // Validate region
    if (x < 0 || y < 0 || x + width > source.width || y + height > source.height) {
        return false;
    }
    
    target.width = width;
    target.height = height;
    target.bytes_per_pixel = source.bytes_per_pixel;
    target.stride = width * source.bytes_per_pixel;
    target.format = source.format;
    target.timestamp = source.timestamp;
    
    target.data.resize(height * target.stride);
    
    // Copy cropped region
    int src_stride = (source.stride > 0) ? source.stride : source.width * source.bytes_per_pixel;
    for (int row = 0; row < height; ++row) {
        int srcOffset = (y + row) * src_stride + x * source.bytes_per_pixel;
        int dstOffset = row * target.stride;
        std::memcpy(&target.data[dstOffset], &source.data[srcOffset], target.stride);
    }
    
    return true;
}

} // namespace capture_utils
} // namespace work_assistant