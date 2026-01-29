# Gemini Watermark Tool

> ## 📌 Original Author Notice (Reverse Alpha Blending)
> I am the original author of **GeminiWatermarkTool** and the reverse **alpha-blending** restoration method used to remove the visible “Gemini” watermark while preserving image detail (Allen Kuo / allenk).
>
> This project achieves high-precision restoration by using my calibrated **48×48** and **96×96** **Reverse-Alpha Masks** to invert the blending equation. Since I published this work and these assets, many derivative tools (desktop apps, websites, browser extensions, etc.) have appeared using the same approach and/or directly reusing the masks produced by this project — because the method is deterministic and highly effective.
>
> ✅ **MIT License reminder**  
> This project is released under the **MIT License**. Commercial use and ports are allowed.  
> However, if you **redistribute** any substantial portion of this project (including code or mask assets), you must:
> - Preserve the original **copyright notice**
> - Include the full **MIT license text**
> - (Recommended) Provide attribution with a link back to this repository
>
> 📖 For the full technical write-up (including how the Reverse-Alpha Masks were derived and calibrated), see:  
> **Removing Gemini AI Watermarks: A Deep Dive into Reverse Alpha Blending**  
> https://allenkuo.medium.com/removing-gemini-ai-watermarks-a-deep-dive-into-reverse-alpha-blending-bbbd83af2a3f


Gemini Watermark Tool is a lightweight, standalone command-line utility that removes Gemini Nano Banana / Pro watermarks from images using **mathematically accurate reverse alpha blending**.

- **Fast & offline**: single executable, **zero dependencies**
- **Flexible workflows**: in-place editing, explicit input/output, and **batch directory** processing
- **Cross-platform**: Windows / Linux / macOS / Android
- **Auto detection**: detects 48×48 vs 96×96 watermark size automatically

<!-- CLI Preview -->
![Preview](artworks/preview.png)

## Features

- **Batch Processing** - Process entire directories at once
- **One-Click Removal** - Simply drag & drop an image onto the executable
- **In-Place Editing** - Process files directly without specifying output
- **Deterministic (Not Inpainting)** - Restores pixels via reverse alpha blending (no guessing)
- **Cross-Platform** - Windows, Linux, macOS, and Android
- **Zero Dependencies** - Single standalone executable, no installation required
- **Auto Size Detection** - Automatically detects 48×48 or 96×96 watermark size
- **Mathematically Accurate** - Precise restoration using reverse alpha blending

## Demo

![Comparison](artworks/demo.gif)

## Side by Side Comparison

![Comparison](artworks/comparison.png)
Best for: **slides, documents, UI screenshots, diagrams, logos**.

**Focus on the bottom example (text-heavy slide).**  
Generative inpainting often breaks text: warped edges, wrong spacing, invented strokes.  
GeminiWatermarkTool reverses the blending equation to recover pixels, keeping text crisp.

---

## ⚠️ About SynthID (Invisible Watermark)

> **Important**: This tool removes **visible watermarks only**. It does NOT remove SynthID.

### What is SynthID?

SynthID is Google DeepMind's **invisible watermarking** technology embedded in AI-generated images. Unlike visible watermarks:

- **Invisible** to human eyes
- **Integrated** during generation (not added afterward)  
- **Extremely robust** against common image manipulations

### Why Can't SynthID Be Removed?

Our extensive research revealed a fundamental truth:

> **SynthID is not a watermark added to an image — it IS the image.**

SynthID operates as a **Statistical Bias** during generation. Every pixel choice is subtly influenced by Google's private key using **Tournament Sampling**. The watermark and visual content are **inseparably bound**.

```
Visible Watermark:  Image + Overlay = Result     ✓ Removable (this tool)
SynthID:            Biased Generation = Image    ✗ Cannot separate
```

### Potential Removal Approaches

| Approach | Trade-off | Feasibility |
|----------|-----------|-------------|
| **Extreme Quantization** (binarization) | Image becomes unusable skeleton | ✓ Works |
| **AI Repaint** (Stable Diffusion, etc.) | Style changes significantly | ✓ Works |
| **White-box Adversarial Attack** | Requires detector model | ✗ Not available |

**Conclusion**: Removing SynthID while preserving image quality is **currently not feasible**.

📄 **[Full SynthID Research Report →](report/synthid_research.md)**
- [SynthID Image Watermark Research Report](https://allenkuo.medium.com/synthid-image-watermark-research-report-9b864b19f9cf)

---

## Download

Download the latest release from the [Releases](https://github.com/allenk/GeminiWatermarkTool/releases) page.

| Platform | File | Architecture |
|----------|------|--------------|
| Windows | `GeminiWatermarkTool-Windows-x64.exe` | x64 |
| Linux | `GeminiWatermarkTool-Linux-x64` | x64 |
| macOS | `GeminiWatermarkTool-macOS-Universal` | Intel + Apple Silicon |
| Android | `GeminiWatermarkTool-Android-arm64` | ARM64 |

## Mac Terminal Installation

Install seamlessly on macOS with a single command:

### Option 1: Quick Install (Recommended)

```bash
curl -fsSL https://raw.githubusercontent.com/allenk/GeminiWatermarkTool/main/scripts/install.sh | bash
```

This will:
- Download the latest universal binary (Intel + Apple Silicon)
- Install to `/usr/local/bin/unmark`
- Handle macOS Gatekeeper automatically

After installation, use it anywhere in terminal:

```bash
unmark image.jpg                    # Remove watermark in-place
unmark -i input.jpg -o output.jpg   # Specify output
unmark -i ./folder/ -o ./clean/     # Batch process
```

### Option 2: Homebrew

```bash
brew tap allenk/tap
brew install unmark
```

### Option 3: Manual Installation

```bash
# Download the latest release
curl -LO https://github.com/allenk/GeminiWatermarkTool/releases/latest/download/GeminiWatermarkTool-macOS-Universal.zip

# Unzip and install
unzip GeminiWatermarkTool-macOS-Universal.zip
chmod +x GeminiWatermarkTool
sudo mv GeminiWatermarkTool /usr/local/bin/unmark

# Remove quarantine (fixes "cannot be opened" error)
sudo xattr -d com.apple.quarantine /usr/local/bin/unmark
```

### Uninstall

```bash
curl -fsSL https://raw.githubusercontent.com/allenk/GeminiWatermarkTool/main/scripts/install.sh | bash -s -- --uninstall
```

Or with Homebrew: `brew uninstall unmark`

## Linux Terminal Installation

```bash
curl -fsSL https://raw.githubusercontent.com/allenk/GeminiWatermarkTool/main/scripts/install.sh | bash
```

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

## Quick Start

<img src="artworks/app_ico.png" alt="App Icon" width="256" height="256">

### Simplest Usage (Drag & Drop) - Windows

1. Download `GeminiWatermarkTool-Windows-x64.exe`
2. Drag an image file onto the executable
3. Done! The watermark is removed in-place

### Command Line

```bash
# Simple mode - edit file in-place
GeminiWatermarkTool watermarked.jpg

# Specify output file
GeminiWatermarkTool -i watermarked.jpg -o clean.jpg

# Batch processing
GeminiWatermarkTool -i ./input_folder/ -o ./output_folder/
```

## Usage

### Simple Mode (Recommended)

The easiest way to use this tool - just provide a single image path:

```bash
GeminiWatermarkTool image.jpg
```

This will **remove the watermark in-place**, overwriting the original file.

> ⚠️ **Warning**: Simple mode overwrites the original file permanently. **Always back up important images before processing.**

### Standard Mode

For more control, use the `-i` (input) and `-o` (output) options:

```bash
# Single file
GeminiWatermarkTool -i input.jpg -o output.jpg

# With explicit --remove flag (optional)
GeminiWatermarkTool -i input.jpg -o output.jpg --remove
```

## Batch Processing (Directory Mode)

Process all images in a directory:

```bash
GeminiWatermarkTool -i ./watermarked_images/ -o ./clean_images/
```

- Input: directory
- Output: directory
- Supported formats: `.jpg`, `.jpeg`, `.png`, `.webp`, `.bmp`

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
# Just remove the watermark, keep the same filename
GeminiWatermarkTool photo_from_gemini.jpg
```

### Example 2: Preserve Original

```bash
# Save to a new file, keeping the original intact
GeminiWatermarkTool -i original.jpg -o cleaned.jpg
```

### Example 3: Process Multiple Files

```bash
# Process all images in a folder
GeminiWatermarkTool -i ./gemini_outputs/ -o ./processed/
```

### Example 4: Verbose Mode

```bash
# See detailed processing information
GeminiWatermarkTool -i image.jpg -o output.jpg -v
```
### Pre-built Binaries

| Platform | Requirements |
|----------|--------------|
| Windows | Windows 10/11 x64 |
| Linux | x64, glibc 2.35+ (Ubuntu 22.04+, Debian 12+) |
| macOS | macOS 11.0+ (Intel or Apple Silicon) |

## System Requirements

| Platform | Requirements |
|----------|--------------|
| Windows | Windows 10/11 x64 |
| Linux | x64, glibc 2.31+ (Ubuntu 20.04+) |
| macOS | macOS 11.0+ (Intel or Apple Silicon) |
| Android | ARM64, Android 10+ (API 29+) |

All binaries are statically linked with no external runtime dependencies.

## Troubleshooting

### "The image doesn't look different after processing"

The watermark is semi-transparent. If the original background was similar to the watermark color, the difference may be subtle. Try viewing at 100% zoom in the watermark area (bottom-right corner).

### "Wrong watermark size detected"

Use `--force-small` or `--force-large` to manually specify:

```bash
GeminiWatermarkTool -i image.jpg -o output.jpg --force-small
```

### "File access denied"

Make sure the output path is writable and the file isn't open in another program.

## Limitations

- Only removes **Gemini visible watermarks** (the semi-transparent logo in bottom-right)
- Does **NOT** remove **SynthID invisible watermarks** — [see why](report/synthid_research.md)
- Designed for Gemini's current watermark pattern (as of 2025)

---

## Building from Source

### Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| CMake | 3.21+ | For CMakePresets support |
| C++ Compiler | C++17 | MSVC 2022, GCC 10+, Clang 12+ |
| vcpkg | Latest | Package manager |
| Ninja | Latest | Recommended build system |

### Setup vcpkg

```bash
# Clone vcpkg
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg

# Bootstrap
./bootstrap-vcpkg.sh    # Linux/macOS
.\bootstrap-vcpkg.bat   # Windows

# Set environment variable
export VCPKG_ROOT="$HOME/vcpkg"       # Linux/macOS (add to .bashrc)
$env:VCPKG_ROOT = "C:\vcpkg"          # Windows PowerShell
```

### Build with CMake Presets

The project uses `CMakePresets.json` for cross-platform configuration.

```bash
# List available presets
cmake --list-presets
```

#### Windows

```powershell
cmake --preset windows-x64-Release
cmake --build --preset windows-x64-Release
```

#### Linux

```bash
cmake --preset linux-x64-Release
cmake --build --preset linux-x64-Release
```

#### macOS (Universal Binary)

macOS requires separate builds for each architecture:

```bash
# Build x64
cmake -B build-x64 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-osx \
  -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build-x64

# Build arm64
cmake -B build-arm64 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=arm64-osx \
  -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-arm64

# Create Universal Binary
lipo -create build-x64/GeminiWatermarkTool build-arm64/GeminiWatermarkTool \
  -output GeminiWatermarkTool
```

#### Android

Requires Android NDK:

```bash
export ANDROID_NDK_HOME="/path/to/android-ndk"

cmake --preset android-arm64-Release
cmake --build --preset android-arm64-Release
```

### Build Presets

| Preset | Platform | Mode |
|--------|----------|------|
| `windows-x64-Release` | Windows | Normal |
| `linux-x64-Release` | Linux | Normal |
| `android-arm64-Release` | Android | Normal |

### Manual Build (without presets)

```bash
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-linux

cmake --build build
```

---

## Project Structure

```
gemini-watermark-tool/
├── CMakeLists.txt              # Main build configuration
├── CMakePresets.json           # Cross-platform build presets
├── vcpkg.json                  # Dependencies manifest
├── src/
│   ├── main.cpp                # CLI entry point
│   ├── watermark_engine.cpp    # Core watermark processing
│   ├── watermark_engine.hpp
│   ├── blend_modes.cpp         # Alpha blending algorithms
│   ├── blend_modes.hpp
│   └── ascii_logo.hpp          # ASCII art banner
├── report/
│   └── synthid_research.md     # SynthID research documentation
└── resources/
    ├── app.ico                 # Windows application icon
    └── app.rc.in               # Windows resource template
```

## Dependencies

All dependencies are managed via vcpkg and statically linked:

| Package | Purpose |
|---------|---------|
| OpenCV | Image I/O and pixel operations |
| fmt | Modern string formatting |
| CLI11 | Command line argument parsing |
| spdlog | Logging framework |

---

## How It Works

### Gemini Watermark Analysis

Gemini applies visible watermarks using **alpha blending**:

```
watermarked = α × logo + (1 - α) × original
```

### Alpha Reconstruction

By statistically analyzing and comparing values related to Alpha, we can reconstruct an Alpha Map that is either correct or very close to it.

### Removal Algorithm (Reverse Alpha Blending)

Solving for the original pixel:

```
original = (watermarked - α × logo) / (1 - α)
         = (watermarked - alpha_map) / (1 - α)
```

This mathematical inversion produces exact restoration of the original pixels.

---

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

- [Removing Gemini AI Watermarks: A Deep Dive into Reverse Alpha Blending](https://allenkuo.medium.com/removing-gemini-ai-watermarks-a-deep-dive-into-reverse-alpha-blending-bbbd83af2a3f)
- [SynthID Image Watermark Research Report](https://allenkuo.medium.com/synthid-image-watermark-research-report-9b864b19f9cf)
- [SynthID Research Report](report/synthid_research.md) — Why invisible watermarks cannot be removed

---

<p align="center">
  <i>If this tool helped you, consider giving it a ⭐</i>
</p>

