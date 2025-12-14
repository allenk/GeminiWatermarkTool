# Gemini Watermark Tool

A lightweight command-line tool to remove Gemini AI watermarks from images.

<!-- CLI Preview -->
![Preview](artworks/preview.png)

## Features

- **One-Click Removal** - Simply drag & drop an image onto the executable
- **In-Place Editing** - Process files directly without specifying output
- **Batch Processing** - Process entire directories at once
- **Zero Dependencies** - Single standalone `.exe`, no installation required
- **Auto Size Detection** - Automatically detects 48×48 or 96×96 watermark size
- **Mathematically Accurate** - Precise restoration

<!-- Before/After Preview -->
![Comparison](artworks/comparison.png)

## Download

Download the latest release from the [Releases](https://github.com/allenk/GeminiWatermarkTool/releases) page.

| File | Description |
|------|-------------|
| `GeminiWatermarkTool.exe` | Windows x64 executable (standalone) |
| `GeminiWatermarkTool` | Linux x64 executable |
| `GeminiWatermarkTool` | macOS executable (Intel x64 / Apple Silicon ARM64) |

## ⚠️ Disclaimer

> **USE AT YOUR OWN RISK**
>
> This tool modifies image files. While it is designed to work reliably, unexpected results may occur due to:
> - Variations in Gemini's watermark implementation
> - Corrupted or unusual image formats
> - Edge cases not covered by testing
>
> **Always back up your original images before processing.**
>
> The author assumes no responsibility for any data loss, image corruption, or unintended modifications. By using this tool, you acknowledge that you understand these risks.

## Building from Source

This project uses CMake for building. You can use either system package managers or vcpkg to install dependencies.

### Prerequisites

- CMake 3.20 or higher
- C++20 compatible compiler (GCC 10+, Clang 12+, or MSVC 2019+)

### Option 1: Using System Package Managers (Recommended for Linux/macOS)

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get update
sudo apt-get install -y cmake build-essential libopencv-dev libfmt-dev libspdlog-dev
```

**Linux (Fedora/RHEL):**
```bash
sudo dnf install cmake gcc-c++ opencv-devel fmt-devel spdlog-devel
```

**macOS (using Homebrew):**
```bash
brew install cmake opencv fmt spdlog
```

**Note:** Homebrew automatically installs the correct architecture version (Intel x64 or Apple Silicon ARM64) based on your Mac.

**Note:**
- CLI11 is header-only and will be automatically downloaded by CMake if not found in the system
- If `fmt` or `spdlog` are not found with CONFIG mode, CMake will try MODULE mode (works with most system packages)

Then build without vcpkg:
```bash
# Linux/macOS
cmake -B build -S .
cmake --build build --config Release
```

### Option 2: Using vcpkg (Cross-platform, Recommended for Windows)

**Note:** This step is only needed if you don't have vcpkg installed yet. If you already have vcpkg set up, you can skip to the "Build Instructions" section.

```bash
# Clone vcpkg (if not already installed)
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg

# Bootstrap vcpkg (only needed once during initial setup)
# Windows
.\bootstrap-vcpkg.bat

# Linux/macOS
./bootstrap-vcpkg.sh
```

After bootstrapping, remember the path to vcpkg (e.g., `/path/to/vcpkg`) as you'll need it for the CMake toolchain file.

### Build Instructions with vcpkg

Replace `[path to vcpkg]` with your actual vcpkg installation path (e.g., `~/vcpkg` or `C:\vcpkg`).

**Windows:**
```bash
# Configure
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[path to vcpkg]/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build --config Release

# The executable will be at: build/Release/GeminiWatermarkTool.exe
```

**Linux/macOS (with vcpkg):**
```bash
# Configure
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[path to vcpkg]/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build --config Release

# The executable will be at: build/GeminiWatermarkTool
```

**macOS Architecture Notes:**
- CMake automatically detects your Mac's architecture (Intel x64 or Apple Silicon ARM64)
- On Apple Silicon Macs, it will build for ARM64 by default
- On Intel Macs, it will build for x86_64 by default
- To build a universal binary (both architectures), add: `-DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"`

### Quick Build Summary

**For Linux/macOS users:** Use system package managers (Option 1) - simpler and faster, no vcpkg needed.

**For Windows users:** Use vcpkg (Option 2) - recommended for easier dependency management.

**Note:** After the first build, you only need to run the `cmake --build` command for subsequent builds. The `cmake -B build -S .` configuration step is only needed when:
- Building for the first time
- Changing CMake configuration
- Adding/removing source files

### Dependencies

The following dependencies are automatically managed by vcpkg:
- OpenCV 4 (with JPEG, PNG, WebP support)
- fmt
- CLI11
- spdlog

## Quick Start

<img src="artworks/app_ico.png" alt="App Icon" width="256" height="256">

### Simplest Usage

**Windows (Drag & Drop):**
1. Download `GeminiWatermarkTool[version].zip`
2. Drag an image file onto the executable
3. Done! The watermark is removed in-place

**Linux/macOS (Command Line):**
```bash
./GeminiWatermarkTool image.jpg
```

### Command Line

**Windows:**
```bash
# Simple mode - edit file in-place
GeminiWatermarkTool.exe watermarked.jpg

# Specify output file
GeminiWatermarkTool.exe -i watermarked.jpg -o clean.jpg

# Batch processing
GeminiWatermarkTool.exe -i ./input_folder/ -o ./output_folder/
```

**Linux/macOS:**
```bash
# Simple mode - edit file in-place
./GeminiWatermarkTool watermarked.jpg

# Specify output file
./GeminiWatermarkTool -i watermarked.jpg -o clean.jpg

# Batch processing
./GeminiWatermarkTool -i ./input_folder/ -o ./output_folder/
```

## Usage

### Simple Mode (Recommended)

The easiest way to use this tool - just provide a single image path:

```bash
# Windows
GeminiWatermarkTool.exe image.jpg

# Linux/macOS
./GeminiWatermarkTool image.jpg
```

This will **remove the watermark in-place**, overwriting the original file.

> ⚠️ **Warning**: Simple mode overwrites the original file permanently. **Always back up important images before processing.**

### Standard Mode

For more control, use the `-i` (input) and `-o` (output) options:

```bash
# Windows
GeminiWatermarkTool.exe -i input.jpg -o output.jpg

# Linux/macOS
./GeminiWatermarkTool -i input.jpg -o output.jpg

# With explicit --remove flag (optional)
./GeminiWatermarkTool -i input.jpg -o output.jpg --remove
```

### Batch Processing

Process all images in a directory:

```bash
# Windows
GeminiWatermarkTool.exe -i ./watermarked_images/ -o ./clean_images/

# Linux/macOS
./GeminiWatermarkTool -i ./watermarked_images/ -o ./clean_images/
```

Supported formats: `.jpg`, `.jpeg`, `.png`, `.webp`, `.bmp`

## Command Line Options

| Option | Short | Description |
|--------|-------|-------------|
| `--input <path>` | `-i` | Input image file or directory |
| `--output <path>` | `-o` | Output image file or directory |
| `--remove` | `-r` | Remove watermark (default behavior) |
| `--force-small` | | Force 48×48 watermark size |
| `--force-large` | | Force 96×96 watermark size |
| `--verbose` | `-v` | Enable verbose output |
| `--quiet` | `-q` | Suppress all output except errors |
| `--banner` | `-b` | Show full ASCII banner |
| `--version` | `-V` | Show version information |
| `--help` | `-h` | Show help message |

## Watermark Size Detection

The tool automatically detects the appropriate watermark size based on image dimensions:

| Image Size | Watermark | Position |
|------------|-----------|----------|
| W ≤ 1024 **or** H ≤ 1024 | 48×48 | Bottom-right, 32px margin |
| W > 1024 **and** H > 1024 | 96×96 | Bottom-right, 64px margin |

### Examples

| Image Dimensions | Detected Size |
|------------------|---------------|
| 800 × 600 | Small (48×48) |
| 800 × 1200 | Small (48×48) |
| 1024 × 768 | Small (48×48) |
| 1024 × 1024 | Small (48×48) |
| 1920 × 1080 | Large (96×96) |

Use `--force-small` or `--force-large` to override automatic detection.

## Examples

### Example 1: Quick Edit

```bash
# Windows
GeminiWatermarkTool.exe photo_from_gemini.jpg

# Linux/macOS
./GeminiWatermarkTool photo_from_gemini.jpg
```

### Example 2: Preserve Original

```bash
# Windows
GeminiWatermarkTool.exe -i original.jpg -o cleaned.jpg

# Linux/macOS
./GeminiWatermarkTool -i original.jpg -o cleaned.jpg
```

### Example 3: Process Multiple Files

```bash
# Windows
GeminiWatermarkTool.exe -i ./gemini_outputs/ -o ./processed/

# Linux/macOS
./GeminiWatermarkTool -i ./gemini_outputs/ -o ./processed/
```

### Example 4: Verbose Mode

```bash
# Windows
GeminiWatermarkTool.exe -i image.jpg -o output.jpg -v

# Linux/macOS
./GeminiWatermarkTool -i image.jpg -o output.jpg -v
```

## System Requirements

- **OS**: 
  - Windows 10 / 11 (x64)
  - Linux (x64)
  - macOS 10.15+ (Intel x64 / Apple Silicon ARM64)
- **Runtime**: None required (statically linked on Windows)
- **Disk**: ~15 MB

## Troubleshooting

### "The image doesn't look different after processing"

The watermark is semi-transparent. If the original background was similar to the watermark color, the difference may be subtle. Try viewing at 100% zoom in the watermark area (bottom-right corner).

### "Wrong watermark size detected"

Use `--force-small` or `--force-large` to manually specify:

```bash
# Windows
GeminiWatermarkTool.exe -i image.jpg -o output.jpg --force-small

# Linux/macOS
./GeminiWatermarkTool -i image.jpg -o output.jpg --force-small
```

### "File access denied"

Make sure the output path is writable and the file isn't open in another program.

## Limitations

- Only removes **Gemini visible watermarks** (the semi-transparent logo in bottom-right)
- Does not remove invisible/steganographic watermarks
- Designed for Gemini's current watermark pattern (as of 2024)

## Legal Disclaimer

This tool is provided for **personal and educational use only**. 

The removal of watermarks may have legal implications depending on your jurisdiction and the intended use of the images. Users are solely responsible for ensuring their use of this tool complies with applicable laws, terms of service, and intellectual property rights.

The author does not condone or encourage the misuse of this tool for copyright infringement, misrepresentation, or any other unlawful purposes.

**THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES, OR OTHER LIABILITY ARISING FROM THE USE OF THIS SOFTWARE.**

## License

MIT License

## Author

**Allen Kuo** ([@allenk](https://github.com/allenk))

## Related

- [Medium](https://allenkuo.medium.com/)

---

<p align="center">
  <i>If this tool helped you, consider giving it a ⭐</i>
</p>
