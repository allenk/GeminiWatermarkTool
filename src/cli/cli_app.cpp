/**
 * @file    cli_app.cpp
 * @brief   CLI Application Implementation
 * @author  AllenK (Kwyshell)
 * @license MIT
 *
 * @details
 * Command-line interface for Gemini Watermark Tool.
 * Supports single file processing, batch processing, and drag & drop.
 *
 * Features auto-detection of watermarks to prevent processing images
 * that don't have Gemini watermarks (protecting original images).
 */

#include "cli/cli_app.hpp"
#include "core/watermark_engine.hpp"
#include "utils/ascii_logo.hpp"
#include "utils/path_formatter.hpp"
#include "embedded_assets.hpp"

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fmt/core.h>
#include <fmt/color.h>

#include <filesystem>
#include <algorithm>
#include <string>

// TTY detection (cross-platform)
#ifdef _WIN32
    #include <io.h>
    #ifndef STDIN_FILENO
        #define STDIN_FILENO 0
        #define STDOUT_FILENO 1
        #define STDERR_FILENO 2
    #endif
    #define isatty _isatty
#else
    #include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace gwt::cli {

namespace {

// =============================================================================
// TTY Detection
// =============================================================================

/**
 * Check if stdout is connected to a terminal (TTY).
 * Returns false when output is piped or redirected (e.g., AI agent calls).
 */
bool is_terminal() noexcept {
    return isatty(STDOUT_FILENO) != 0;
}

// =============================================================================
// Logo and Banner printing
// =============================================================================

[[maybe_unused]] void print_logo() {
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

// =============================================================================
// Processing helpers
// =============================================================================

struct BatchResult {
    int success = 0;
    int skipped = 0;
    int failed = 0;

    void print() const {
        int total = success + skipped + failed;
        if (total > 1) {
            fmt::print("\n");
            fmt::print(fmt::fg(fmt::color::green), "[Summary] ");
            fmt::print("Processed: {}", success);
            if (skipped > 0) {
                fmt::print(fmt::fg(fmt::color::yellow), ", Skipped: {}", skipped);
            }
            if (failed > 0) {
                fmt::print(fmt::fg(fmt::color::red), ", Failed: {}", failed);
            }
            fmt::print(" (Total: {})\n", total);
        }
    }
};

void process_single(
    const fs::path& input,
    const fs::path& output,
    bool remove,
    WatermarkEngine& engine,
    std::optional<WatermarkSize> force_size,
    bool use_detection,
    float detection_threshold,
    BatchResult& result
) {
    auto proc_result = process_image(input, output, remove, engine,
                                     force_size, use_detection, detection_threshold);

    if (proc_result.skipped) {
        result.skipped++;
        fmt::print(fmt::fg(fmt::color::yellow), "[SKIP] ");
        fmt::print("{}: {}\n", gwt::filename_utf8(input), proc_result.message);
    } else if (proc_result.success) {
        result.success++;
        fmt::print(fmt::fg(fmt::color::green), "[OK] ");
        fmt::print("{}", gwt::filename_utf8(input));
        if (proc_result.confidence > 0) {
            fmt::print(fmt::fg(fmt::color::gray), " ({:.0f}% confidence)",
                       proc_result.confidence * 100.0f);
        }
        fmt::print("\n");
    } else {
        result.failed++;
        fmt::print(fmt::fg(fmt::color::red), "[FAIL] ");
        fmt::print("{}: {}\n", gwt::filename_utf8(input), proc_result.message);
    }
}

/**
 * Parse --banner / --no-banner from argv before CLI11 parsing.
 * Returns: std::nullopt (use auto), true (force show), false (force hide)
 */
std::optional<bool> parse_banner_flag(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--banner") return true;
        if (arg == "--no-banner") return false;
    }
    return std::nullopt;  // Auto-detect
}

/**
 * Determine if banner should be shown.
 * Priority: --banner/--no-banner flag > TTY auto-detection
 */
bool should_show_banner(std::optional<bool> flag_override) {
    if (flag_override.has_value()) {
        return flag_override.value();
    }
    // Auto: show banner only if stdout is a terminal
    return is_terminal();
}

}  // anonymous namespace

// =============================================================================
// Public API
// =============================================================================

bool is_simple_mode(int argc, char** argv) {
    if (argc < 2) return false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (!arg.empty() && arg[0] == '-') {
            // Allow --banner and --no-banner in simple mode
            if (arg == "--banner" || arg == "--no-banner") {
                continue;
            }
            return false;
        }
    }
    return true;
}

int run_simple_mode(int argc, char** argv) {
    // Check banner preference
    auto banner_flag = parse_banner_flag(argc, argv);
    if (should_show_banner(banner_flag)) {
        print_banner();
    }

    auto logger = spdlog::stdout_color_mt("gwt");
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::warn);  // Less verbose in simple mode

    BatchResult result;

    // Default settings for simple mode
    constexpr bool use_detection = true;
    constexpr float detection_threshold = 0.25f;

    fmt::print(fmt::fg(fmt::color::gray),
               "Auto-detection enabled (threshold: {:.0f}%)\n\n",
               detection_threshold * 100.0f);

    try {
        WatermarkEngine engine(
            embedded::bg_48_png, embedded::bg_48_png_size,
            embedded::bg_96_png, embedded::bg_96_png_size
        );

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            
            // Skip banner flags
            if (arg == "--banner" || arg == "--no-banner") {
                continue;
            }

            fs::path input(arg);

            if (!fs::exists(input)) {
                fmt::print(fmt::fg(fmt::color::red), "[ERROR] ");
                fmt::print("File not found: {}\n", gwt::to_utf8(input));
                fmt::print(fmt::fg(fmt::color::gray),
                           "  (Path may contain encoding issues on Windows without UTF-8 beta)\n");
                result.failed++;
                continue;
            }

            if (fs::is_directory(input)) {
                fmt::print(fmt::fg(fmt::color::red), "[ERROR] ");
                fmt::print("Directory not supported in simple mode: {}\n", gwt::to_utf8(input));
                fmt::print("  Use: gwt -i <dir> -o <dir>\n");
                result.failed++;
                continue;
            }

            process_single(input, input, true, engine, std::nullopt,
                          use_detection, detection_threshold, result);
        }

        result.print();
        return (result.failed > 0) ? 1 : 0;
    } catch (const std::exception& e) {
        fmt::print(fmt::fg(fmt::color::red), "[FATAL] {}\n", e.what());
        return 1;
    }
}

int run(int argc, char** argv) {
    // Check for simple mode first
    if (is_simple_mode(argc, argv)) {
        return run_simple_mode(argc, argv);
    }

    // Check banner preference before CLI11 parsing
    auto banner_flag = parse_banner_flag(argc, argv);

    CLI::App app{"Gemini Watermark Tool (Standalone) - Remove visible watermarks"};
    app.footer("\nSimple usage: GeminiWatermarkTool <image>  (in-place edit with auto-detection)");

    app.set_version_flag("-V,--version", APP_VERSION);

    // Banner control flags (parsed manually above, but registered for --help)
    bool banner_show = false;
    bool banner_hide = false;
    app.add_flag("--banner", banner_show, 
                 "Show ASCII banner (default: auto-detect based on TTY)");
    app.add_flag("--no-banner", banner_hide, 
                 "Hide ASCII banner (useful for scripts and AI agents)");

    // Input/Output paths
    std::string input_path;
    std::string output_path;

    app.add_option("-i,--input", input_path, "Input image file or directory")
        ->required();
        // Note: We check existence manually below for better CJK path error messages

    app.add_option("-o,--output", output_path, "Output image file or directory")
        ->required();

    // Operation mode
    bool remove_mode = false;
    app.add_flag("--remove,-r", remove_mode, "Remove watermark from image (default)");

    // Force processing (disable detection)
    bool force_process = false;
    app.add_flag("--force,-f", force_process,
                 "Force processing without watermark detection (may damage images without watermarks)");

    // Detection threshold
    float detection_threshold = 0.25f;
    app.add_option("--threshold,-t", detection_threshold,
                   "Watermark detection confidence threshold (0.0-1.0, default: 0.25)")
        ->check(CLI::Range(0.0f, 1.0f));

    // Force specific watermark size
    bool force_small = false;
    bool force_large = false;
    app.add_flag("--force-small", force_small, "Force use of 48x48 watermark regardless of image size");
    app.add_flag("--force-large", force_large, "Force use of 96x96 watermark regardless of image size");

    // Verbosity
    bool verbose = false;
    bool quiet = false;
    app.add_flag("-v,--verbose", verbose, "Enable verbose output");
    app.add_flag("-q,--quiet", quiet, "Suppress all output except errors");

    // Parse arguments
    CLI11_PARSE(app, argc, argv);

    // Print banner after parsing (so --help doesn't show banner)
    if (should_show_banner(banner_flag)) {
        print_banner();
    }

    // Standalone mode: always remove
    remove_mode = true;

    // Detection is enabled by default, disabled with --force
    bool use_detection = !force_process;

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
    std::optional<WatermarkSize> force_size;
    if (force_small && force_large) {
        spdlog::error("Cannot specify both --force-small and --force-large");
        return 1;
    } else if (force_small) {
        force_size = WatermarkSize::Small;
        spdlog::info("Forcing 48x48 watermark size");
    } else if (force_large) {
        force_size = WatermarkSize::Large;
        spdlog::info("Forcing 96x96 watermark size");
    }

    // Print detection status
    if (use_detection) {
        fmt::print(fmt::fg(fmt::color::gray),
                   "Auto-detection enabled (threshold: {:.0f}%)\n\n",
                   detection_threshold * 100.0f);
    } else {
        fmt::print(fmt::fg(fmt::color::yellow),
                   "WARNING: Force mode - processing ALL images without detection!\n\n");
    }

    try {
        WatermarkEngine engine(
            embedded::bg_48_png, embedded::bg_48_png_size,
            embedded::bg_96_png, embedded::bg_96_png_size
        );

        fs::path input(input_path);
        fs::path output(output_path);

        // Manual existence check with better error messages for CJK paths
        if (!fs::exists(input)) {
            fmt::print(fmt::fg(fmt::color::red), "[ERROR] ");
            fmt::print("Input path not found: {}\n", gwt::to_utf8(input));
            fmt::print(fmt::fg(fmt::color::gray),
                       "  (If the path contains CJK characters, try enabling Windows UTF-8 beta\n");
            fmt::print(fmt::fg(fmt::color::gray),
                       "   or use the GUI version which handles Unicode paths correctly)\n");
            return 1;
        }

        BatchResult result;

        if (fs::is_directory(input)) {
            if (!fs::exists(output)) {
                fs::create_directories(output);
            }

            spdlog::info("Batch processing directory: {}", input);

            for (const auto& entry : fs::directory_iterator(input)) {
                if (!entry.is_regular_file()) continue;

                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                if (ext != ".jpg" && ext != ".jpeg" && ext != ".png" &&
                    ext != ".webp" && ext != ".bmp") {
                    continue;
                }

                fs::path out_file = output / entry.path().filename();
                process_single(entry.path(), out_file, remove_mode, engine,
                              force_size, use_detection, detection_threshold, result);
            }

            result.print();
        } else {
            process_single(input, output, remove_mode, engine,
                          force_size, use_detection, detection_threshold, result);
        }

        return (result.failed > 0) ? 1 : 0;
    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }
}

}  // namespace gwt::cli
