 /**
  * @file    main.cpp
  * @brief   Gemini Watermark Tool - CLI Entry Point
  * @author  AllenK (Kwyshell)
  * @date    2025.12.13
  * @license MIT
  *
  * @details
  * A command-line tool to add/remove Gemini-style visible watermarks from images.
  *
  * Watermark Mechanism:
  *   Gemini adds watermarks using blending and overlay techniques;
  *     we designed a reverse operation to eliminate them and mitigate this effect
  *
  *   To remove watermark (reverse blending)
  *
  * Build Modes:
  *   - Normal:     Requires external asset files, supports both add/remove
  *   - Standalone: Assets embedded in binary, remove-only, single .exe distribution
  *
  * Usage:
  *   GeminiWatermarkTool image.jpg                            (standalone: in-place)
  *   GeminiWatermarkTool -i input.jpg -o output.jpg --remove
  *   GeminiWatermarkTool -i input.jpg -o output.jpg --add     (normal mode only)
  *
  * @see https://github.com/allenk/GeminiWatermarkTool
  */

#include "watermark_engine.hpp"
#include "ascii_logo.hpp"
#include "embedded_assets.hpp"

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fmt/core.h>
#include <fmt/color.h>

#include <filesystem>
#include <iostream>
#include <algorithm>
#include <string>
#include <cstdlib>

// =============================================================================
// Platform-specific includes
// =============================================================================
#ifdef _WIN32
    #include <windows.h>
#endif

namespace fs = std::filesystem;

// =============================================================================
// Platform-specific console setup
// =============================================================================

/**
 * Setup console for proper UTF-8 and ANSI color support
 */
void setup_console() {
#ifdef _WIN32
    // Set console output to UTF-8
    SetConsoleOutputCP(CP_UTF8);

    // Enable ANSI escape sequences for colors (Windows 10+)
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif
    // Unix-like systems (Linux, macOS) support ANSI by default
}

// =============================================================================
// Logo and Banner printing
// =============================================================================
void print_logo() {
    fmt::print(fmt::fg(fmt::color::cyan), "{}", gwt::ASCII_COMPACT);
    fmt::print(fmt::fg(fmt::color::yellow), "  [Standalone Edition]");
    fmt::print(fmt::fg(fmt::color::gray), "  v{}\n", APP_VERSION);
    fmt::print("\n");
}

void print_banner() {
    fmt::print(fmt::fg(fmt::color::medium_purple), "{}", gwt::ASCII_BANNER);
    fmt::print(fmt::fg(fmt::color::gray), "  Version: {}\n", APP_VERSION);
    fmt::print(fmt::fg(fmt::color::yellow), "  *** Standalone Edition - Remove Only ***\n");
    fmt::print("\n");
}

// Check if running in simple mode: one or more file arguments without flags
bool is_simple_mode(int argc, char** argv) {
    if (argc < 2) return false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        // If any argument starts with '-', it's not simple mode
        if (!arg.empty() && arg[0] == '-') {
            return false;
        }
    }
    return true;
}

// Simple mode: process one or more files in-place
int run_simple_mode(int argc, char** argv) {
    setup_console();
    print_banner();

    // Configure logging
    auto logger = spdlog::stdout_color_mt("gwt");
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);

    int success_count = 0;
    int fail_count = 0;

    try {
        // Initialize engine with embedded assets
        gwt::WatermarkEngine engine(
            gwt::embedded::bg_48_png, gwt::embedded::bg_48_png_size,
            gwt::embedded::bg_96_png, gwt::embedded::bg_96_png_size
        );

        for (int i = 1; i < argc; ++i) {
            fs::path input(argv[i]);

            // Validate input
            if (!fs::exists(input)) {
                spdlog::error("File not found: {}", argv[i]);
                fail_count++;
                continue;
            }

            if (fs::is_directory(input)) {
                spdlog::error("Skipping directory: {} (For directory processing, use -i <dir> -o <dir>)", argv[i]);
                fail_count++;
                continue;
            }

            spdlog::info("Processing: {}", input.filename().string());

            // Process in-place (input == output), no force_size
            if (gwt::process_image(input, input, true, engine, std::nullopt)) {
                success_count++;
            } else {
                fail_count++;
            }
        }

        fmt::print(fmt::fg(fmt::color::green), "\n[OK] Completed: {} succeeded", success_count);
        if (fail_count > 0) {
            fmt::print(fmt::fg(fmt::color::red), ", {} failed", fail_count);
        }
        fmt::print("\n");

        return (fail_count > 0) ? 1 : 0;

    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }
}

int main(int argc, char** argv) {
    // Check for simple mode first (before CLI parsing)
    if (is_simple_mode(argc, argv)) {
        return run_simple_mode(argc, argv);
    }
    setup_console();

    CLI::App app{"Gemini Watermark Tool (Standalone) - Remove visible watermarks"};
    app.footer("\nSimple usage: GeminiWatermarkTool <image>  (in-place edit)");
    print_banner();

    app.set_version_flag("-V,--version", APP_VERSION);

    // Input/Output paths
    std::string input_path;
    std::string output_path;

    app.add_option("-i,--input", input_path, "Input image file or directory")
        ->required()
        ->check(CLI::ExistingPath);

    app.add_option("-o,--output", output_path, "Output image file or directory")
        ->required();

    // Operation mode
    bool remove_mode = false;

    // Standalone mode: remove only, flag is optional (default to remove)
    app.add_flag("--remove,-r", remove_mode,
        "Remove watermark from image (default)");

    // Force specific watermark size
    bool force_small = false;
    bool force_large = false;

    app.add_flag("--force-small", force_small,
        "Force use of 48x48 watermark regardless of image size");
    app.add_flag("--force-large", force_large,
        "Force use of 96x96 watermark regardless of image size");

    // Verbosity
    bool verbose = false;
    bool quiet = false;

    app.add_flag("-v,--verbose", verbose, "Enable verbose output");
    app.add_flag("-q,--quiet", quiet, "Suppress all output except errors");

    // Parse arguments
    CLI11_PARSE(app, argc, argv);

    // Standalone mode: always remove
    remove_mode = true;

    // Configure logging
    auto logger = spdlog::stdout_color_mt("gwt");
    spdlog::set_default_logger(logger);

    if (quiet) {
        spdlog::set_level(spdlog::level::err);
    } else if (verbose) {
        spdlog::set_level(spdlog::level::debug);
    } else {
        spdlog::set_level(spdlog::level::info);
    }

    // Determine force size option
    std::optional<gwt::WatermarkSize> force_size;
    if (force_small && force_large) {
        spdlog::error("Cannot specify both --force-small and --force-large");
        return 1;
    } else if (force_small) {
        force_size = gwt::WatermarkSize::Small;
        spdlog::info("Forcing 48x48 watermark size");
    } else if (force_large) {
        force_size = gwt::WatermarkSize::Large;
        spdlog::info("Forcing 96x96 watermark size");
    }

    try {
        // Initialize engine with embedded assets
        gwt::WatermarkEngine engine(
            gwt::embedded::bg_48_png, gwt::embedded::bg_48_png_size,
            gwt::embedded::bg_96_png, gwt::embedded::bg_96_png_size
        );

        // Check if it's a single file or directory
        fs::path input(input_path);
        fs::path output(output_path);

        int success_count = 0;
        int fail_count = 0;

        if (fs::is_directory(input)) {
            // Batch processing
            if (!fs::exists(output)) {
                fs::create_directories(output);
            }

            spdlog::info("Batch processing directory: {}", input.string());

            for (const auto& entry : fs::directory_iterator(input)) {
                if (!entry.is_regular_file()) continue;

                std::string ext = entry.path().extension().string();
                // Convert to lowercase for comparison
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                if (ext != ".jpg" && ext != ".jpeg" && ext != ".png" &&
                    ext != ".webp" && ext != ".bmp") {
                    continue;
                }

                fs::path out_file = output / entry.path().filename();

                // Pass force_size to process_image
                if (gwt::process_image(entry.path(), out_file, remove_mode, engine, force_size)) {
                    success_count++;
                } else {
                    fail_count++;
                }
            }

            fmt::print(fmt::fg(fmt::color::green), "\n[OK] Completed: {} succeeded", success_count);
            if (fail_count > 0) {
                fmt::print(fmt::fg(fmt::color::red), ", {} failed", fail_count);
            }
            fmt::print("\n");

        } else {
            // Single file processing - pass force_size
            if (gwt::process_image(input, output, remove_mode, engine, force_size)) {
                success_count = 1;
                fmt::print(fmt::fg(fmt::color::green), "[OK] Success: {}\n", output.string());
            } else {
                fail_count = 1;
            }
        }

        return (fail_count > 0) ? 1 : 0;

    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }
}

