# Gemini Watermark Tool

[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Platform](https://img.shields.io/badge/Platform-Windows%20|%20Linux%20|%20macOS%20|%20Android-lightgrey.svg)](#system-requirements)
[![AI Denoise](https://img.shields.io/badge/AI-FDnCNN%20%2B%20Vulkan%20GPU-green.svg)](#ai-denoise--fdncnn-neural-network-new-in-v025)
[![MCP](https://img.shields.io/badge/MCP-Server%20Available-blue.svg)](https://github.com/allenk/gwt-integrations)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![GitHub Stars](https://img.shields.io/github/stars/allenk/GeminiWatermarkTool?style=social)](https://github.com/allenk/GeminiWatermarkTool)

> ## 📌 Original Author Notice (Reverse Alpha Blending)
> I am the original author of **GeminiWatermarkTool** and the reverse **alpha-blending** restoration method used to remove the visible "Gemini" watermark while preserving image detail (Allen Kuo / allenk).
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

Gemini Watermark Tool removes Gemini visible watermarks from images using a **mathematically accurate reverse alpha blending algorithm**.  
Unlike many tools that rely entirely on generative inpainting, this project focuses on **deterministic reconstruction** combined with lightweight AI-assisted cleanup.

The design philosophy is simple:  
**small, standalone, fast, and reliable.**

The entire toolkit is distributed as a **portable executable with zero runtime dependencies**, making it easy to install, automate, and integrate into scripts or pipelines without complex setup.

Key capabilities include:

- **Fast & offline**: single executable, **no external services or dependencies**
- **Standalone & portable**: small footprint, easy to deploy and use anywhere
- **GUI + CLI**: drag-and-drop desktop workflow or command-line automation
- **Deterministic watermark reconstruction**: reverse alpha blending algorithm designed specifically for Gemini watermark overlays
- **AI-assisted cleanup**: optional GPU-accelerated FDnCNN denoise (NCNN + Vulkan) for residual artifacts
- **Smart detection**: three-stage NCC detection with confidence scoring to automatically skip non-watermarked images
- **Batch processing**: process entire directories with preview and progress tracking
- **Cross-platform**: Windows / Linux / macOS / Android (CLI)
- **AI Agent ready**: [Claude Code Skill + MCP Server](https://github.com/allenk/gwt-integrations) for automation and agent workflows

## 🖥️ GUI Application — Major Update

> **GeminiWatermarkTool now comes with a full graphical desktop application.**
> No command line needed — just open, drag & drop, and process. Supports single-image editing with real-time preview, and batch processing with smart watermark detection.

The desktop GUI provides an interactive workflow for both single-image and batch operations.

![GUI Demo](artworks/gui_demo.png)

## AI Agent - Integration (New in v0.2.5)
GeminiWatermarkTool can now be integrated into AI-agent workflows via MCP server and Claude Code skills.
This enables automated watermark processing inside agent-based pipelines.

> **[gwt-integrations](https://github.com/allenk/gwt-integrations)** — Claude Code Skill + MCP Server for GeminiWatermarkTool

Enable AI coding agents (Claude Code, Cursor, Windsurf, etc.) to use GeminiWatermarkTool directly:

| Component | Description |
|-----------|-------------|
| **Claude Code Skill** | Teaches AI agents GWT's full CLI syntax, region/snap/denoise options, and best practices — agents can remove watermarks from images without manual guidance |
| **MCP Server** | Exposes GWT as 4 tools via [Model Context Protocol](https://modelcontextprotocol.io/) — `remove_watermark`, `detect_watermark`, `batch_process`, `get_tool_info` — any MCP-compatible client can call them |
| **install.py** | Cross-platform installer (stdlib only) — auto-detects GWT binary, configures Claude Code skill and MCP server in one command |


![AI MCP Skill](artworks/gwt-mcp.gif)

### Single Image Editing

![GUI Single Image](artworks/gui_single.gif)

- Drag & drop or open any supported image
- Auto-detect watermark size (48×48 / 96×96) or select manually
- **Custom watermark mode**: draw a search region interactively, resize with 8-point anchors, fine-tune position with WASD keys
- **Multi-scale guided detection (Snap Engine)**: coarse-to-fine NCC template matching auto-locks to the exact watermark position within your drawn region — supports variable sizes from 32–320px with adjustable search range slider
- Real-time before/after comparison (press **V**)
- One-key processing (**X**) and revert (**Z**)
- Zoom, pan (Space/Alt + drag, mouse wheel), and fit-to-window

### Software Inpainting Cleanup (New in v0.2.3)

![Software Inpainting](artworks/gui_inpaint_sw.gif)

Reverse alpha blending is mathematically exact — but only when the image hasn't been resized, recompressed, or processed after watermarking. In practice, many images go through post-processing that breaks the pixel-perfect math, leaving faint residual artifacts after removal.

**Software Inpainting** addresses this by applying a lightweight cleanup pass after the reverse blending step. It uses gradient-weighted masks derived from the watermark's own alpha channel to target only the residual pixels, leaving the rest of the image untouched.

Three built-in methods are available:

| Method | Description | Best for |
|--------|-------------|----------|
| **NS** | Navier-Stokes based inpainting — propagates surrounding pixel flow into the damaged region | General-purpose cleanup with smooth results |
| **TELEA** | Fast marching method — fills inward from boundary pixels based on distance weighting | Quick processing, good for small residuals |
| **Soft Inpaint** | Gradient-weighted Gaussian blend — uses the watermark alpha as a soft mask for weighted blending | Preserving fine texture in photographic content |

The cleanup controls appear automatically in **Custom** mode under the Detected Info panel. You can adjust the **method**, **strength** (0–100%), and **inpaint radius** (1–25 px) to fine-tune the result.

### AI Denoise — FDnCNN Neural Network (New in v0.2.5)

![AI Denoise](artworks/gui_inpaint_ai_fdncnn.gif)

**AI Denoise** uses a GPU-accelerated neural network ([FDnCNN](https://github.com/cszn/KAIR)) to clean up watermark residuals that conventional inpainting methods struggle with — particularly the faint sparkle edges and corner artifacts left after reverse alpha blending on resized images.

Unlike NS/TELEA which require a binary mask to know *which* pixels are damaged, AI Denoise examines a 41×41 pixel neighborhood around each point and learns from training data what "normal" image content looks like. Combined with gradient-masked blending from the alpha map, it repairs only the artifact pixels while preserving clean background detail.

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| **Strength** | 0–300% | 120% | Controls mask coverage — values above 100% expand repair to weaker gradient edges |
| **Sigma** | 1–150 | 50 | Noise level estimation — higher values denoise more aggressively |

**Technical details:**
- **Model**: FDnCNN Color, 20-layer Conv+ReLU, FP16 (~1.3 MB embedded in executable)
- **Inference**: [NCNN](https://github.com/Tencent/ncnn) with Vulkan GPU acceleration, automatic CPU fallback
- **Pipeline**: gradient mask from alpha map → NCNN inference on padded ROI → per-pixel masked blend
- **Performance**: < 5 ms per region on modern GPUs, ~20 ms on CPU

AI Denoise is the **recommended default** when available. The GPU device name is displayed below the controls. If GPU initialization fails, the tool automatically falls back to NS inpainting.

### Batch Processing

<!-- TODO: Replace with actual GIF -->
![GUI Batch Processing](artworks/gui_batch.gif)

- **Drag & drop** multiple files or an **entire folder** to enter batch mode
- Thumbnail atlas preview with filename labels and status overlays (OK / SKIP / FAIL)
- **Detection threshold slider** (0–100%, 5% steps, 25% recommended) — automatically skip images without watermarks
- Confirmation dialog before overwriting originals
- Non-blocking processing with progress bar and scrollable result log
- Thumbnails refresh after completion to show processed results

### Keyboard Shortcuts

| Key | Action |
|-----|--------|
| X | Process image |
| V | Compare with original |
| Z | Revert to original |
| C (hold) | Hide overlay |
| W A S D | Move custom watermark region |
| Space / Alt | Pan (hold + drag) |
| Scroll | Zoom to cursor |
| Ctrl +/- | Zoom in / out |
| Ctrl 0 | Zoom fit |
| Ctrl+W | Close / exit batch mode |

### Render Backends (Windows)

The GUI supports multiple render backends for maximum compatibility:

| Backend | Description | Use Case |
|---------|-------------|----------|
| **D3D11** (default) | Direct3D 11 with WARP fallback | Best for Windows — works in Hyper-V, Docker, RDP |
| **OpenGL** | OpenGL 3.3 Core | Cross-platform, requires GPU drivers |

**Why D3D11?**
- Native Windows API — no additional drivers needed
- **WARP fallback** — software rendering when no GPU available
- Works in **Hyper-V**, **Docker**, **Remote Desktop**, and other virtualized environments
- Better stability in Windows sandbox configurations

```bash
# Auto-select (D3D11 on Windows, OpenGL elsewhere)
GeminiWatermarkTool

# Force specific backend
GeminiWatermarkTool --backend=d3d11
GeminiWatermarkTool --backend=opengl
```

## CLI — What's New

In addition to the GUI, the command line has been significantly enhanced.

### Simple Mode — Now Supports Multiple Files

```bash
# Process multiple files at once (new!)
GeminiWatermarkTool img1.jpg img2.png img3.webp
```

Watermark detection is **enabled by default** in simple mode — images without a detectable watermark are automatically skipped to prevent accidental damage.

```bash
# Force processing without detection
GeminiWatermarkTool --force image.jpg

# Custom detection threshold (default: 25%)
GeminiWatermarkTool --threshold 0.40 image.jpg
```

### Standard Mode

```bash
# Single file with explicit output
GeminiWatermarkTool -i input.jpg -o output.jpg

# Batch directory processing
GeminiWatermarkTool -i ./watermarked_images/ -o ./clean_images/
```

## Watermark Detection

> Inspired by [@dannycreations](https://github.com/dannycreations)'s [contribution](https://github.com/allenk/GeminiWatermarkTool/pull/13) on watermark presence detection. We took the concept further with a production-grade three-stage algorithm deeply integrated into both CLI and GUI workflows.

Batch processing watermark-free images can cause unnecessary pixel damage. The tool now uses a **three-stage NCC (Normalized Cross-Correlation)** algorithm to detect watermarks before processing, ensuring only watermarked images are modified:

1. **Spatial NCC** — correlates the image region with the known alpha map (50% weight, with circuit breaker at 0.25 to short-circuit obvious non-matches)
2. **Gradient NCC** — Sobel edge matching to detect the star-shaped structural pattern (30% weight)
3. **Statistical Variance** — texture dampening analysis to distinguish real watermarks from white/flat regions (20% weight)

A combined confidence score determines whether a watermark is present. The default threshold is **25%** — images below this score are skipped. This eliminates false positives from white backgrounds or similar-looking content that plagued simpler correlation-based approaches.

| Flag | Effect |
|------|--------|
| `--force` | Skip detection, process all images unconditionally |
| `--threshold 0.40` | Set custom confidence threshold (0.0–1.0) |
| `--no-banner` | Hide ASCII banner (for scripts and AI agents) |
| `--banner` | Force show ASCII banner |

Detection is **enabled by default** in simple/drag-and-drop mode and **disabled by default** in standard (`-i` / `-o`) mode. In the GUI, the threshold is adjustable via a slider (0–100%, 5% steps) with a recommended 25% default.

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

### First Run — OS Security Prompts

Downloaded binaries are not code-signed, so your OS may show a security warning on first launch. This is normal for open-source software distributed outside app stores.

<details>
<summary><b>macOS</b> — "Apple cannot check it for malicious software"</summary>

**Option A (recommended):** Right-click the binary → **Open** → click **Open** in the dialog. You only need to do this once.

**Option B (terminal):**
```bash
xattr -dr com.apple.quarantine GeminiWatermarkTool
chmod +x GeminiWatermarkTool
```
</details>

<details>
<summary><b>Windows</b> — SmartScreen "Windows protected your PC"</summary>

**Option A:** Click **More info** → **Run anyway**.

**Option B (PowerShell):**
```powershell
Unblock-File .\GeminiWatermarkTool.exe
```
</details>

<details>
<summary><b>Linux</b> — No security prompt</summary>

Linux does not quarantine downloaded binaries. Just ensure the file is executable:
```bash
chmod +x GeminiWatermarkTool
./GeminiWatermarkTool
```
</details>

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

## CLI — Quick Start

<img src="artworks/app_ico.png" alt="App Icon" width="256" height="256">

Don't need the GUI? The CLI is designed for **maximum simplicity** — one drag, one drop, done.

### Drag & Drop (Windows) — The Easiest Way

1. Download `GeminiWatermarkTool-Windows-x64.exe`
2. Drag an image file onto the executable
3. ✅ Done! The watermark is removed in-place — no terminal, no arguments

<!-- CLI Preview -->
![Preview](artworks/preview.png)

### Command Line

```bash
# Simple mode - just provide a filename
GeminiWatermarkTool watermarked.jpg

# Specify output file (preserves original)
GeminiWatermarkTool -i watermarked.jpg -o clean.jpg

# Batch processing - entire directory
GeminiWatermarkTool -i ./input_folder/ -o ./output_folder/
```

Supported formats: `.jpg`, `.jpeg`, `.png`, `.webp`, `.bmp`

> ⚠️ **Warning**: Simple mode overwrites the original file permanently. **Always back up important images before processing.**

## Command Line Options

| Option | Short | Description |
|--------|-------|-------------|
| `--input <path>` | `-i` | Input image file or directory |
| `--output <path>` | `-o` | Output image file or directory |
| `--remove` | `-r` | Remove watermark (default behavior) |
| `--force` | `-f` | Force processing (skip watermark detection) |
| `--threshold <val>` | `-t` | Detection confidence threshold, 0.0–1.0 (default: 0.25) |
| `--force-small` | | Force 48×48 watermark size |
| `--force-large` | | Force 96×96 watermark size |
| `--verbose` | `-v` | Enable verbose output |
| `--quiet` | `-q` | Suppress all output except errors |
| `--banner` | `-b` | Show full ASCII banner |
| `--version` | `-V` | Show version information |
| `--help` | `-h` | Show help message |

### Advanced Options (v0.2.5)

| Option | Description |
|--------|-------------|
| `--region <spec>` | Explicit watermark region (see Region Syntax below) |
| `--fallback-region <spec>` | Search region when standard detection fails |
| `--snap` | Enable multi-scale snap search within region |
| `--snap-max-size <N>` | Max snap search size, 32–320 (default: 160) |
| `--snap-threshold <N>` | Min snap confidence to accept, 0.0–1.0 (default: 0.60) |
| `--denoise <method>` | Cleanup after removal: `ai`, `ns`, `telea`, `soft`, `off` |
| `--sigma <N>` | AI denoise noise level, 1–150 (default: 50) |
| `--strength <N>` | Denoise strength %, 0–300 (default: 120 for AI, 85 for others) |
| `--radius <N>` | Inpaint radius for NS/TELEA/Soft, 1–25 (default: 10) |

**Region syntax:**

| Format | Description |
|--------|-------------|
| `x,y,w,h` | Absolute coordinates |
| `br:mx,my,w,h` | Bottom-right corner (margin_x, margin_y, width, height) |
| `bl:mx,my,w,h` | Bottom-left corner |
| `tr:mx,my,w,h` | Top-right corner |
| `tl:mx,my,w,h` | Top-left (same as absolute) |
| `br:auto` | Use Gemini default position based on image size |

**Examples:**

```bash
# Standard removal with AI denoise cleanup
GeminiWatermarkTool -i input.jpg -o clean.jpg --denoise ai

# Batch with AI denoise (all files)
GeminiWatermarkTool -i ./photos/ -o ./clean/ --denoise ai

# Fallback for images that fail detection: search bottom-right area with snap
GeminiWatermarkTool -i ./photos/ -o ./clean/ \
    --fallback-region br:auto --snap --denoise ai

# Resized watermarks: expand snap search to 320px
GeminiWatermarkTool -i ./photos/ -o ./clean/ \
    --fallback-region br:80,80,200,200 --snap --snap-max-size 320 --denoise ai

# Force process at explicit region (all images have watermark at same spot)
GeminiWatermarkTool -i ./photos/ -o ./clean/ \
    --force --region 500,800,160,160 --snap --denoise ai --sigma 75
```

> **Note:** When no `--denoise` is specified, the CLI behaves identically to previous versions (no cleanup pass). All existing scripts and agent integrations continue to work unchanged.

## Watermark Size Detection

The tool automatically detects the appropriate watermark size based on image dimensions:

| Image Size | Watermark | Position |
|------------|-----------|----------|
| W ≤ 1024 **or** H ≤ 1024 | 48×48 | Bottom-right, 32px margin |
| W > 1024 **and** H > 1024 | 96×96 | Bottom-right, 64px margin |

Use `--force-small` or `--force-large` to override automatic detection.

## System Requirements

| Platform | Requirements |
|----------|--------------|
| Windows | Windows 10/11 x64 |
| Linux | x64, glibc 2.35+ (Ubuntu 22.04+, Debian 12+) |
| macOS | macOS 11.0+ (Intel or Apple Silicon) |
| Android | ARM64, Android 10+ (API 29+) |

All binaries are statically linked with no external runtime dependencies.

> **AI Denoise** uses Vulkan for GPU acceleration. Most modern GPUs (NVIDIA, AMD, Intel) with up-to-date drivers support Vulkan. If no Vulkan GPU is detected, inference automatically falls back to CPU (OpenMP multi-threaded).

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
| C++ Compiler | C++20 | MSVC 2022, GCC 12+, Clang 14+ |
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
# Clone with submodules (NCNN source)
git clone --recursive https://github.com/allenk/GeminiWatermarkTool.git

# Or if already cloned:
git submodule update --init --recursive

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

| Preset | Platform | Backend | Notes |
|--------|----------|---------|-------|
| `windows-x64-Release` | Windows | D3D11 + OpenGL | Default, includes AI Denoise |
| `windows-x64-OpenGL-Release` | Windows | OpenGL only | Includes AI Denoise |
| `linux-x64-Release` | Linux | OpenGL | Includes AI Denoise |
| `mac-universal-Release` | macOS | OpenGL | Intel + Apple Silicon, includes AI Denoise |
| `android-arm64-Release` | Android | — | CLI only, includes AI Denoise |

All presets enable `ENABLE_AI_DENOISE=ON` by default. To build without AI, set `-DENABLE_AI_DENOISE=OFF` manually.

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
│   ├── core/                   # Core engine (CLI + GUI shared)
│   │   ├── watermark_engine.hpp/cpp
│   │   ├── watermark_detector.hpp/cpp
│   │   ├── blend_modes.hpp/cpp
│   │   ├── ai_denoise.hpp/cpp         # NCNN FDnCNN denoiser
│   │   ├── ai_denoise_model.cpp       # Embedded model weights (isolated TU)
│   │   ├── ncnn_shim.hpp              # Vulkan loader shim for NCNN
│   │   └── types.hpp
│   ├── cli/                    # CLI application
│   │   └── cli_app.hpp/cpp
│   ├── utils/                  # Utilities
│   │   ├── ascii_logo.hpp
│   │   └── path_formatter.hpp
│   ├── main.cpp                # Entry point (CLI/GUI dispatcher)
│   └── gui/                    # Desktop GUI (ImGui + SDL3)
│       ├── gui_app.hpp/cpp           # GUI entry point
│       ├── app/
│       │   ├── app_state.hpp         # Application state
│       │   └── app_controller.hpp/cpp # Logic controller
│       ├── widgets/
│       │   ├── main_window.hpp/cpp   # Main window + menus
│       │   └── image_preview.hpp/cpp # Image viewer + batch view
│       ├── backend/
│       │   ├── render_backend.hpp/cpp  # Backend interface + factory
│       │   ├── opengl_backend.hpp/cpp  # OpenGL 3.3 implementation
│       │   └── d3d11_backend.hpp/cpp   # Direct3D 11 implementation (Windows)
│       └── resources/
│           └── style.hpp             # Theme and layout constants
├── external/
│   └── ncnn/                         # NCNN source (git submodule)
│       └── model-convert/output/     # Converted FDnCNN model headers
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
| SDL3 | Window management and input (GUI) |
| Dear ImGui | Immediate mode GUI framework (GUI) |
| ImPlot | Plotting widgets (GUI) |
| glad | OpenGL loader (GUI) |
| nativefiledialog-extended | Native file dialogs (GUI) |
| WIL | Windows Implementation Libraries (D3D11 backend, Windows only) |
| NCNN | Neural network inference runtime (AI Denoise, git submodule) |
| volk | Vulkan meta-loader for dynamic dispatch (AI Denoise, vcpkg) |

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

### Residual Cleanup (Software Inpainting + AI Denoise)

When images have been resized or recompressed after watermarking, the exact math no longer holds perfectly. Two approaches are available for cleaning up residual artifacts:

**Software Inpainting** (NS / TELEA / Gaussian):
```
1. Compute gradient magnitude from watermark alpha channel
2. Build soft weight mask: stronger where alpha gradient is high
3. Apply selected inpainting method (NS / TELEA / Gaussian blend)
4. Blend result using weight mask — only affected pixels are modified
```

**AI Denoise** (FDnCNN, recommended):
```
1. Compute gradient mask from alpha map (locate sparkle edges)
2. Run FDnCNN inference on padded ROI (Vulkan GPU or CPU)
   Input:  [R, G, B, sigma/255] → Output: denoised clean image
3. Per-pixel masked blend: mask × denoised + (1-mask) × original
   → Only edge artifacts repaired, clean background untouched
```

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
- GitHub: https://github.com/allenk  
- LinkedIn: https://www.linkedin.com/in/allen-kuo-7b513a45/
- Medium: https://allenkuo.medium.com

**Quick start:**

```bash
# Install the integration (after GeminiWatermarkTool is on PATH)
git clone https://github.com/allenk/gwt-integrations.git
cd gwt-integrations
python install.py
```

After installation, AI agents can process watermarks conversationally:

```
"Remove the Gemini watermark from screenshot.png using AI denoise"
"Batch process all images in ./photos/ with fallback snap detection"
```

See [gwt-integrations README](https://github.com/allenk/gwt-integrations) for full setup instructions and MCP configuration.

## Related

- [Removing Gemini AI Watermarks: A Deep Dive into Reverse Alpha Blending](https://allenkuo.medium.com/removing-gemini-ai-watermarks-a-deep-dive-into-reverse-alpha-blending-bbbd83af2a3f)
- [SynthID Image Watermark Research Report](https://allenkuo.medium.com/synthid-image-watermark-research-report-9b864b19f9cf)
- [SynthID Research Report](report/synthid_research.md) — Why invisible watermarks cannot be removed
- [gwt-integrations](https://github.com/allenk/gwt-integrations) — Claude Code Skill + MCP Server for AI agent automation

## Third-Party Licenses

This project incorporates the following open-source components:

| Component | License | Usage |
|-----------|---------|-------|
| [NCNN](https://github.com/Tencent/ncnn) | BSD-3-Clause | Neural network inference runtime (Vulkan GPU + CPU) |
| [FDnCNN model weights](https://github.com/cszn/KAIR) (KAIR) | MIT | Pre-trained denoising model (embedded FP16 weights) |
| [volk](https://github.com/zeux/volk) | MIT | Vulkan meta-loader for dynamic dispatch |

Full license texts for these components are available in their respective repositories.

---

<p align="center">
  <i>If this tool helped you, consider giving it a ⭐</i>
</p>
