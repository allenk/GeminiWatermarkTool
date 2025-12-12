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

### Simplest Usage (Drag & Drop)

1. Download `GeminiWatermarkTool[version].zip`
2. Drag an image file onto the executable
3. Done! The watermark is removed in-place

### Command Line

```bash
# Simple mode - edit file in-place
GeminiWatermarkTool.exe watermarked.jpg

# Specify output file
GeminiWatermarkTool.exe -i watermarked.jpg -o clean.jpg

# Batch processing
GeminiWatermarkTool.exe -i ./input_folder/ -o ./output_folder/
```

## Usage

### Simple Mode (Recommended)

The easiest way to use this tool - just provide a single image path:

```bash
GeminiWatermarkTool.exe image.jpg
```

This will **remove the watermark in-place**, overwriting the original file.

> ⚠️ **Warning**: Simple mode overwrites the original file permanently. **Always back up important images before processing.**

### Standard Mode

For more control, use the `-i` (input) and `-o` (output) options:

```bash
# Single file
GeminiWatermarkTool.exe -i input.jpg -o output.jpg

# With explicit --remove flag (optional)
GeminiWatermarkTool.exe -i input.jpg -o output.jpg --remove
```

### Batch Processing

Process all images in a directory:

```bash
GeminiWatermarkTool.exe -i ./watermarked_images/ -o ./clean_images/
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
| W < 1024 **or** H < 1024 | 48×48 | Bottom-right, 32px margin |
| W ≥ 1024 **and** H ≥ 1024 | 96×96 | Bottom-right, 64px margin |

### Examples

| Image Dimensions | Detected Size |
|------------------|---------------|
| 800 × 600 | Small (48×48) |
| 800 × 1200 | Small (48×48) |
| 1024 × 768 | Small (48×48) |
| 1024 × 1024 | Large (96×96) |
| 1920 × 1080 | Large (96×96) |

Use `--force-small` or `--force-large` to override automatic detection.

## Examples

### Example 1: Quick Edit

```bash
# Just remove the watermark, keep the same filename
GeminiWatermarkTool.exe photo_from_gemini.jpg
```

### Example 2: Preserve Original

```bash
# Save to a new file, keeping the original intact
GeminiWatermarkTool.exe -i original.jpg -o cleaned.jpg
```

### Example 3: Process Multiple Files

```bash
# Process all images in a folder
GeminiWatermarkTool.exe -i ./gemini_outputs/ -o ./processed/
```

### Example 4: Verbose Mode

```bash
# See detailed processing information
GeminiWatermarkTool.exe -i image.jpg -o output.jpg -v
```

## System Requirements

- **OS**: Windows 10 / 11 (x64)
- **Runtime**: None required (statically linked)
- **Disk**: ~15 MB

## Troubleshooting

### "The image doesn't look different after processing"

The watermark is semi-transparent. If the original background was similar to the watermark color, the difference may be subtle. Try viewing at 100% zoom in the watermark area (bottom-right corner).

### "Wrong watermark size detected"

Use `--force-small` or `--force-large` to manually specify:

```bash
GeminiWatermarkTool.exe -i image.jpg -o output.jpg --force-small
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
