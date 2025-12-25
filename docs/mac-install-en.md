# macOS Build and Installation Guide

This document provides detailed instructions for building and installing Gemini Watermark Tool on macOS.

## System Requirements

-   macOS 11.0 or later
-   Apple Silicon (arm64) or Intel (x86_64) processor
-   Homebrew package manager
-   Xcode Command Line Tools

## Installation Steps

### 1. Install Homebrew (if not already installed)

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

### 2. Install Dependencies

Install all required dependencies using Homebrew:

```bash
brew install ninja opencv fmt cli11 spdlog
```

**Dependencies explained:**

-   `ninja` - Fast build tool
-   `opencv` - Image processing library
-   `fmt` - Formatting library
-   `cli11` - Command-line argument parser
-   `spdlog` - Logging library

### 3. Clone the Repository (if not already done)

```bash
git clone https://github.com/allenk/GeminiWatermarkTool.git
cd GeminiWatermarkTool
```

### 4. Configure the Build

Choose one of the following options based on your needs:

#### Option A: Build for arm64 (Apple Silicon only)

Suitable for running only on your local machine:

```bash
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64
```

#### Option B: Build for x86_64 (Intel only)

**Note:** On Apple Silicon Macs, you need Homebrew and dependencies in a Rosetta environment.

```bash
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=x86_64
```

#### Option C: Build Universal Binary (requires vcpkg)

For supporting both arm64 and x86_64, it's recommended to use vcpkg instead of Homebrew. Refer to the project's `vcpkg.json` configuration.

### 5. Build the Project

```bash
cmake --build build
```

After successful compilation, the executable will be located at `build/GeminiWatermarkTool`.

### 6. Verify the Build

Check binary information:

```bash
# Check file type
file build/GeminiWatermarkTool

# Check architecture
lipo -info build/GeminiWatermarkTool

# Test run
./build/GeminiWatermarkTool --version
```

### 7. Copy to Project Root (Optional)

```bash
cp build/GeminiWatermarkTool .
```

### 8. Configure Shell Alias (Optional)

For easier usage, you can create a shell alias:

#### Method 1: Using a Separate Alias File (Recommended)

1. Create `~/.alias` file:

```bash
cat > ~/.alias << 'EOF'
# Personal aliases

# Gemini Watermark Tool
alias gwt='/Users/your-username/GeminiWatermarkTool/GeminiWatermarkTool'
EOF
```

2. Add loader to `~/.zshrc`:

```bash
echo '
# Load personal aliases
[ -f ~/.alias ] && source ~/.alias' >> ~/.zshrc
```

3. Reload configuration:

```bash
source ~/.zshrc
```

#### Method 2: Add Directly to .zshrc

```bash
echo "alias gwt='$(pwd)/GeminiWatermarkTool'" >> ~/.zshrc
source ~/.zshrc
```

### 9. Usage

After setting up the alias, you can use it from any directory:

```bash
gwt --help
gwt --version
gwt add input.jpg -o output.jpg
gwt remove watermarked.jpg -o restored.jpg
```

## Troubleshooting

### Q: Build fails with missing dependencies

**A:** Ensure all dependencies are installed via Homebrew:

```bash
brew list ninja opencv fmt cli11 spdlog
```

If any package is missing, install it:

```bash
brew install <package-name>
```

### Q: Cannot build x86_64 on Apple Silicon Mac

**A:** Homebrew on Apple Silicon installs arm64 libraries by default. To build x86_64:

1. Install Rosetta 2: `softwareupdate --install-rosetta`
2. Install a second Homebrew in Rosetta environment (at `/usr/local`)
3. Use that Homebrew to install x86_64 dependencies

Alternatively, use vcpkg for cross-architecture compilation.

### Q: Building Universal Binary

**A:** Homebrew cannot directly build universal binaries as its libraries are single-architecture. Use vcpkg instead:

```bash
# Install vcpkg
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg

# Build using CMake Preset
cmake --preset mac-universal-Release
cmake --build --preset mac-universal-Release
```

### Q: Runtime error about missing dynamic libraries

**A:** Ensure Homebrew's library path is in the system path:

```bash
export DYLD_LIBRARY_PATH=/opt/homebrew/lib:$DYLD_LIBRARY_PATH
```

Or rebuild with static linking.

## Clean Build

If you need to rebuild from scratch:

```bash
rm -rf build
```

## Additional Information

-   Project Homepage: https://github.com/allenk/GeminiWatermarkTool
-   Issue Tracker: https://github.com/allenk/GeminiWatermarkTool/issues
