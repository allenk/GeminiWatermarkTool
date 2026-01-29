# Homebrew formula for Gemini Watermark Tool
# To install: brew install allenk/tap/unmark
# Or: brew tap allenk/tap && brew install unmark

class Unmark < Formula
  desc "Remove Gemini AI watermarks from images using reverse alpha blending"
  homepage "https://github.com/allenk/GeminiWatermarkTool"
  license "MIT"
  version "0.1.2"

  on_macos do
    url "https://github.com/allenk/GeminiWatermarkTool/releases/download/v#{version}/GeminiWatermarkTool-macOS-Universal.zip"
    sha256 "PLACEHOLDER_SHA256_MACOS"

    def install
      bin.install "GeminiWatermarkTool" => "unmark"
      bin.install_symlink "unmark" => "GeminiWatermarkTool"
    end
  end

  on_linux do
    on_intel do
      url "https://github.com/allenk/GeminiWatermarkTool/releases/download/v#{version}/GeminiWatermarkTool-Linux-x64.zip"
      sha256 "PLACEHOLDER_SHA256_LINUX"

      def install
        bin.install "GeminiWatermarkTool" => "unmark"
        bin.install_symlink "unmark" => "GeminiWatermarkTool"
      end
    end
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/unmark --version")
  end
end
