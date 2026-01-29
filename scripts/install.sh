#!/bin/bash
#
# Gemini Watermark Tool - Mac/Linux Installer
# https://github.com/allenk/GeminiWatermarkTool
#
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/allenk/GeminiWatermarkTool/main/scripts/install.sh | bash
#
# Or with custom install directory:
#   curl -fsSL https://raw.githubusercontent.com/allenk/GeminiWatermarkTool/main/scripts/install.sh | bash -s -- --prefix /custom/path
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default configuration
REPO="allenk/GeminiWatermarkTool"
BINARY_NAME="unmark"
INSTALL_DIR="/usr/local/bin"

# Detect OS and architecture
detect_platform() {
    local os arch

    os="$(uname -s)"
    arch="$(uname -m)"

    case "$os" in
        Darwin)
            OS="macOS"
            ASSET_NAME="GeminiWatermarkTool-macOS-Universal"
            ;;
        Linux)
            OS="Linux"
            case "$arch" in
                x86_64)
                    ASSET_NAME="GeminiWatermarkTool-Linux-x64"
                    ;;
                *)
                    echo -e "${RED}Error: Unsupported Linux architecture: $arch${NC}"
                    echo "Only x86_64 is supported on Linux."
                    exit 1
                    ;;
            esac
            ;;
        *)
            echo -e "${RED}Error: Unsupported operating system: $os${NC}"
            echo "This installer supports macOS and Linux only."
            exit 1
            ;;
    esac

    echo -e "${BLUE}Detected platform: $OS ($arch)${NC}"
}

# Get the latest release version and download URL
get_latest_release() {
    echo -e "${BLUE}Fetching latest release information...${NC}"

    # Try to get release info from GitHub API
    RELEASE_INFO=$(curl -fsSL "https://api.github.com/repos/${REPO}/releases/latest" 2>/dev/null) || {
        echo -e "${RED}Error: Failed to fetch release information from GitHub.${NC}"
        echo "Please check your internet connection and try again."
        exit 1
    }

    # Extract version tag
    VERSION=$(echo "$RELEASE_INFO" | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/')

    if [ -z "$VERSION" ]; then
        echo -e "${RED}Error: Could not determine latest version.${NC}"
        exit 1
    fi

    # Find the download URL for our asset
    DOWNLOAD_URL=$(echo "$RELEASE_INFO" | grep "browser_download_url" | grep "$ASSET_NAME" | head -1 | sed -E 's/.*"([^"]+)".*/\1/')

    if [ -z "$DOWNLOAD_URL" ]; then
        echo -e "${RED}Error: Could not find download URL for $ASSET_NAME${NC}"
        echo "Available assets:"
        echo "$RELEASE_INFO" | grep "browser_download_url" | sed -E 's/.*"([^"]+)".*/  - \1/'
        exit 1
    fi

    echo -e "${GREEN}Latest version: $VERSION${NC}"
}

# Download and install the binary
install_binary() {
    local tmp_dir tmp_file

    tmp_dir=$(mktemp -d)
    tmp_file="$tmp_dir/$ASSET_NAME"

    echo -e "${BLUE}Downloading $ASSET_NAME...${NC}"

    # Download with progress
    if command -v curl &> /dev/null; then
        curl -fSL --progress-bar "$DOWNLOAD_URL" -o "$tmp_file" || {
            echo -e "${RED}Error: Download failed.${NC}"
            rm -rf "$tmp_dir"
            exit 1
        }
    elif command -v wget &> /dev/null; then
        wget -q --show-progress "$DOWNLOAD_URL" -O "$tmp_file" || {
            echo -e "${RED}Error: Download failed.${NC}"
            rm -rf "$tmp_dir"
            exit 1
        }
    else
        echo -e "${RED}Error: Neither curl nor wget found. Please install one.${NC}"
        rm -rf "$tmp_dir"
        exit 1
    fi

    # Handle zip files (GitHub releases might be zipped)
    if file "$tmp_file" | grep -q "Zip archive"; then
        echo -e "${BLUE}Extracting archive...${NC}"
        unzip -q "$tmp_file" -d "$tmp_dir"
        tmp_file=$(find "$tmp_dir" -name "GeminiWatermarkTool*" -type f ! -name "*.zip" | head -1)
    fi

    # Make executable
    chmod +x "$tmp_file"

    # Verify it runs
    echo -e "${BLUE}Verifying binary...${NC}"
    if ! "$tmp_file" --version &> /dev/null; then
        echo -e "${RED}Error: Binary verification failed.${NC}"
        rm -rf "$tmp_dir"
        exit 1
    fi

    # Install to target directory
    echo -e "${BLUE}Installing to $INSTALL_DIR/$BINARY_NAME...${NC}"

    # Check if we need sudo
    if [ -w "$INSTALL_DIR" ]; then
        mv "$tmp_file" "$INSTALL_DIR/$BINARY_NAME"
    else
        echo -e "${YELLOW}Administrator privileges required to install to $INSTALL_DIR${NC}"
        sudo mv "$tmp_file" "$INSTALL_DIR/$BINARY_NAME"
        sudo chmod +x "$INSTALL_DIR/$BINARY_NAME"
    fi

    # Cleanup
    rm -rf "$tmp_dir"

    # Also create symlink with original name for compatibility
    if [ "$BINARY_NAME" != "GeminiWatermarkTool" ]; then
        if [ -w "$INSTALL_DIR" ]; then
            ln -sf "$INSTALL_DIR/$BINARY_NAME" "$INSTALL_DIR/GeminiWatermarkTool" 2>/dev/null || true
        else
            sudo ln -sf "$INSTALL_DIR/$BINARY_NAME" "$INSTALL_DIR/GeminiWatermarkTool" 2>/dev/null || true
        fi
    fi
}

# macOS: Handle Gatekeeper quarantine
handle_macos_security() {
    if [ "$OS" = "macOS" ]; then
        echo -e "${BLUE}Removing macOS quarantine attribute...${NC}"
        if [ -w "$INSTALL_DIR/$BINARY_NAME" ]; then
            xattr -d com.apple.quarantine "$INSTALL_DIR/$BINARY_NAME" 2>/dev/null || true
        else
            sudo xattr -d com.apple.quarantine "$INSTALL_DIR/$BINARY_NAME" 2>/dev/null || true
        fi
    fi
}

# Verify installation
verify_installation() {
    echo ""
    echo -e "${GREEN}Installation complete!${NC}"
    echo ""

    # Check if install dir is in PATH
    if [[ ":$PATH:" != *":$INSTALL_DIR:"* ]]; then
        echo -e "${YELLOW}Note: $INSTALL_DIR is not in your PATH.${NC}"
        echo "Add it to your shell configuration:"
        echo ""
        echo "  # For bash (~/.bashrc or ~/.bash_profile):"
        echo "  export PATH=\"$INSTALL_DIR:\$PATH\""
        echo ""
        echo "  # For zsh (~/.zshrc):"
        echo "  export PATH=\"$INSTALL_DIR:\$PATH\""
        echo ""
    fi

    echo -e "${GREEN}Usage:${NC}"
    echo "  $BINARY_NAME image.jpg              # Remove watermark in-place"
    echo "  $BINARY_NAME -i input.jpg -o out.jpg  # Specify output file"
    echo "  $BINARY_NAME -i ./folder/ -o ./out/   # Batch process directory"
    echo ""
    echo -e "${BLUE}Run '$BINARY_NAME --help' for more options.${NC}"
    echo ""

    # Show version
    if command -v "$BINARY_NAME" &> /dev/null; then
        echo -e "${GREEN}Installed version:${NC}"
        "$BINARY_NAME" --version
    elif [ -x "$INSTALL_DIR/$BINARY_NAME" ]; then
        echo -e "${GREEN}Installed version:${NC}"
        "$INSTALL_DIR/$BINARY_NAME" --version
    fi
}

# Uninstall function
uninstall() {
    echo -e "${BLUE}Uninstalling Gemini Watermark Tool...${NC}"

    local files_to_remove=(
        "$INSTALL_DIR/$BINARY_NAME"
        "$INSTALL_DIR/GeminiWatermarkTool"
    )

    for file in "${files_to_remove[@]}"; do
        if [ -f "$file" ] || [ -L "$file" ]; then
            echo "Removing $file"
            if [ -w "$file" ]; then
                rm -f "$file"
            else
                sudo rm -f "$file"
            fi
        fi
    done

    echo -e "${GREEN}Uninstall complete.${NC}"
}

# Print banner
print_banner() {
    echo ""
    echo -e "${BLUE}╔═══════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║${NC}        ${GREEN}Gemini Watermark Tool Installer${NC}               ${BLUE}║${NC}"
    echo -e "${BLUE}║${NC}        Remove Gemini AI watermarks easily           ${BLUE}║${NC}"
    echo -e "${BLUE}╚═══════════════════════════════════════════════════════╝${NC}"
    echo ""
}

# Parse arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --prefix)
                INSTALL_DIR="$2"
                shift 2
                ;;
            --uninstall)
                print_banner
                uninstall
                exit 0
                ;;
            --help|-h)
                print_banner
                echo "Usage: install.sh [OPTIONS]"
                echo ""
                echo "Options:"
                echo "  --prefix DIR    Install to DIR instead of /usr/local/bin"
                echo "  --uninstall     Remove the installed binary"
                echo "  --help, -h      Show this help message"
                echo ""
                echo "Examples:"
                echo "  # Install to default location (/usr/local/bin)"
                echo "  curl -fsSL https://raw.githubusercontent.com/$REPO/main/scripts/install.sh | bash"
                echo ""
                echo "  # Install to custom directory"
                echo "  curl -fsSL ... | bash -s -- --prefix ~/.local/bin"
                echo ""
                echo "  # Uninstall"
                echo "  curl -fsSL ... | bash -s -- --uninstall"
                exit 0
                ;;
            *)
                echo -e "${RED}Unknown option: $1${NC}"
                echo "Use --help for usage information."
                exit 1
                ;;
        esac
    done
}

# Main installation flow
main() {
    parse_args "$@"
    print_banner
    detect_platform
    get_latest_release
    install_binary
    handle_macos_security
    verify_installation
}

main "$@"
