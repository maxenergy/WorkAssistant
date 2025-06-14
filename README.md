# Work Study Assistant

A cross-platform C++ application for intelligent work and study activity monitoring, content extraction, and AI-powered classification.

## Features

- **Cross-platform window monitoring** (Windows, Linux)
- **Intelligent screen capture** with change detection
- **Content extraction** via OCR and UI automation
- **AI-powered classification** using local LLM
- **Secure encrypted storage** 
- **Web-based user interface**

## Architecture

The application follows a modular architecture with platform abstraction layers:

```
Core Service (C++)
├── Window Monitoring (Platform-specific)
├── Screen Capture (screen_capture_lite)
├── Content Extraction (OCR + UI Automation)
├── AI Classification (llama.cpp)
├── Encrypted Storage (libsodium)
└── Web Interface (Drogon + React)
```

## Build Requirements

### System Dependencies

**Linux:**
```bash
sudo apt-get install build-essential cmake pkg-config
sudo apt-get install libx11-dev libatspi2.0-dev
sudo apt-get install libssl-dev libsqlite3-dev
```

**Windows:**
- Visual Studio 2019+ with C++ build tools
- CMake 3.14+
- vcpkg for package management

### Third-party Libraries
- screen_capture_lite (screen capture)
- Tesseract (OCR)
- llama.cpp (AI inference)
- libsodium (encryption)
- Drogon (web server)
- nlohmann/json (JSON processing)

## Building

```bash
# Clone repository
git clone <repository-url>
cd WorkAssistant

# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
cmake --build . --config Release

# Run
./work_study_assistant
```

## Development Status

This is an early development version implementing the core window monitoring functionality.

### Completed
- ✅ Project structure and CMake build system
- ✅ Cross-platform window monitoring abstraction
- ✅ Linux X11 window monitor implementation
- ✅ Thread pool for background processing
- ✅ Basic event handling system

### In Progress
- 🔄 Windows platform implementation
- 🔄 Screen capture integration
- 🔄 OCR content extraction

### Planned
- ⏳ AI classification system
- ⏳ Encrypted storage
- ⏳ Web interface
- ⏳ Service/daemon mode

## Usage

Currently the application runs in console mode and demonstrates window monitoring:

```bash
./work_study_assistant
```

The application will:
1. Initialize the platform-specific window monitor
2. Start monitoring window events (focus changes, creation, destruction)
3. Display real-time information about active windows
4. Process window information in background threads

## License

[License TBD]

## Contributing

This project is in active development. Contributions welcome following the development plan in `DEVELOPMENT_PLAN.md`.