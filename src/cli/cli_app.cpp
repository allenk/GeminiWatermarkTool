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
 *
 * v0.2.5: Added --region, --snap, --denoise, --fallback-region for
 * advanced processing of resized/reprocessed watermarks.
 */

#include "cli/cli_app.hpp"
#include "core/watermark_engine.hpp"
#include "utils/ascii_logo.hpp"
#include "utils/path_formatter.hpp"
#include "embedded_assets.hpp"

#ifdef GWT_HAS_AI_DENOISE
#include "core/ai_denoise.hpp"
#endif

#include <opencv2/imgcodecs.hpp>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fmt/core.h>
#include <fmt/color.h>

#include <filesystem>
#include <algorithm>
#include <sstream>
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
// Region Parsing
// =============================================================================

/**
 * Parse a region string into a cv::Rect, resolving relative coordinates.
 *
 * Supported formats:
 *   "x,y,w,h"           Absolute coordinates
 *   "br:mx,my,w,h"      Relative to bottom-right corner (margin_x, margin_y, w, h)
 *   "bl:mx,my,w,h"      Relative to bottom-left corner
 *   "tr:mx,my,w,h"      Relative to top-right corner
 *   "tl:mx,my,w,h"      Relative to top-left (same as absolute)
 *   "br:auto"            Use Gemini default position for this image size
 *
 * @param region_str  Region specification string
 * @param img_w       Image width (needed for relative coords)
 * @param img_h       Image height (needed for relative coords)
 * @return            Resolved cv::Rect, or empty rect on parse failure
 */
cv::Rect parse_region(const std::string& region_str, int img_w, int img_h) {
    if (region_str.empty()) return {};

    // Check for corner prefix
    std::string body = region_str;
    std::string corner;

    auto colon_pos = region_str.find(':');
    if (colon_pos != std::string::npos && colon_pos <= 2) {
        corner = region_str.substr(0, colon_pos);
        body = region_str.substr(colon_pos + 1);

        // Handle "br:auto" — use Gemini default watermark position
        if (body == "auto") {
            auto config = get_watermark_config(img_w, img_h);
            auto pos = config.get_position(img_w, img_h);
            return cv::Rect(pos.x, pos.y, config.logo_size, config.logo_size);
        }
    }

    // Parse "a,b,c,d" using istringstream
    int a, b, c, d;
    char sep1, sep2, sep3;
    std::istringstream iss(body);

    if (!(iss >> a >> sep1 >> b >> sep2 >> c >> sep3 >> d) ||
        sep1 != ',' || sep2 != ',' || sep3 != ',') {
        spdlog::error("Invalid region format: '{}' (expected x,y,w,h)", region_str);
        return {};
    }

    if (c <= 0 || d <= 0) {
        spdlog::error("Invalid region size: {}x{}", c, d);
        return {};
    }

    // Resolve relative coordinates
    int x, y, w = c, h = d;

    if (corner.empty() || corner == "tl") {
        // Absolute or top-left relative (same thing)
        x = a;
        y = b;
    } else if (corner == "br") {
        // Bottom-right: a=margin_x, b=margin_y from bottom-right corner
        x = img_w - a - w;
        y = img_h - b - h;
    } else if (corner == "bl") {
        // Bottom-left: a=margin_x from left, b=margin_y from bottom
        x = a;
        y = img_h - b - h;
    } else if (corner == "tr") {
        // Top-right: a=margin_x from right, b=margin_y from top
        x = img_w - a - w;
        y = b;
    } else {
        spdlog::error("Unknown corner prefix: '{}' (expected tl/tr/bl/br)", corner);
        return {};
    }

    // Clamp to image bounds
    cv::Rect rect(x, y, w, h);
    rect &= cv::Rect(0, 0, img_w, img_h);

    if (rect.width < 4 || rect.height < 4) {
        spdlog::error("Region too small after clamping: {}x{}", rect.width, rect.height);
        return {};
    }

    return rect;
}

// =============================================================================
// Denoise Method Parsing
// =============================================================================

/**
 * Denoise configuration for CLI
 */
struct DenoiseConfig {
    bool enabled{false};
    InpaintMethod method{InpaintMethod::NS};
    float strength{0.85f};
    float sigma{50.0f};           // AI only
    int radius{10};               // NS/TELEA/GAUSSIAN only

    static DenoiseConfig from_string(const std::string& method_str) {
        DenoiseConfig cfg;
        cfg.enabled = true;

        std::string lower = method_str;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if (lower == "off" || lower == "none" || lower == "disable") {
            cfg.enabled = false;
        } else if (lower == "ns") {
            cfg.method = InpaintMethod::NS;
        } else if (lower == "telea") {
            cfg.method = InpaintMethod::TELEA;
        } else if (lower == "soft" || lower == "gaussian") {
            cfg.method = InpaintMethod::GAUSSIAN;
#ifdef GWT_HAS_AI_DENOISE
        } else if (lower == "ai") {
            cfg.method = InpaintMethod::AI_DENOISE;
            cfg.strength = 1.2f;  // AI default
            cfg.sigma = 50.0f;
#endif
        } else {
            spdlog::warn("Unknown denoise method '{}', using NS", method_str);
            cfg.method = InpaintMethod::NS;
        }

        return cfg;
    }
};

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

/**
 * Process a single file using the basic pipeline (backward-compatible).
 * No region/snap/denoise support — identical to original behavior.
 */
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
 * Process a single file with advanced pipeline (region/snap/denoise).
 *
 * Flow:
 *   1. Standard 3-stage detection
 *   2a. Detected → remove at standard position + denoise
 *   2b. Not detected + --force → remove at force position + denoise
 *   2c. Not detected + --fallback-region + --snap
 *       → snap found + confidence >= snap_threshold → apply at snap position
 *       → snap found + confidence < snap_threshold  → bypass (unless --force)
 *       → snap not found → bypass
 *   2d. Not detected + --fallback-region (no snap) → apply directly
 *   2e. Not detected → bypass
 */
void process_single_advanced(
    const fs::path& input,
    const fs::path& output,
    WatermarkEngine& engine,
    std::optional<WatermarkSize> force_size,
    bool use_detection,
    float detection_threshold,
    bool force_process,
    const std::string& region_str,
    const std::string& fallback_region_str,
    bool use_snap,
    int snap_max_size,
    float snap_threshold,
    const DenoiseConfig& denoise_cfg,
#ifdef GWT_HAS_AI_DENOISE
    NcnnDenoiser* denoiser,
#endif
    BatchResult& result
) {
    try {
        // Read image
        cv::Mat image = cv::imread(input.string(), cv::IMREAD_COLOR);
        if (image.empty()) {
            result.failed++;
            fmt::print(fmt::fg(fmt::color::red), "[FAIL] ");
            fmt::print("{}: Failed to load image\n", gwt::filename_utf8(input));
            return;
        }

        const int img_w = image.cols;
        const int img_h = image.rows;
        float confidence = 0.0f;

        spdlog::info("Processing: {} ({}x{})", input.filename(), img_w, img_h);

        // =================================================================
        // Phase 1: Determine watermark region
        // =================================================================

        cv::Rect wm_region;   // The final watermark region to process
        bool detected = false;
        bool using_custom = false;

        // If explicit --region is given, parse it
        cv::Rect explicit_region;
        if (!region_str.empty()) {
            explicit_region = parse_region(region_str, img_w, img_h);
        }

        // Try standard detection first
        // Skip when: --force with explicit region, or --fallback-region + --snap
        // (user explicitly wants custom region search, standard position may be wrong)
        bool skip_standard_detect = (force_process && !explicit_region.empty()) ||
                                    (!fallback_region_str.empty() && use_snap);

        if (use_detection && !skip_standard_detect) {
            DetectionResult det = engine.detect_watermark(image, force_size);
            confidence = det.confidence;

            if (det.detected || det.confidence >= detection_threshold) {
                // Standard detection succeeded
                auto config = get_watermark_config(img_w, img_h);
                auto pos = config.get_position(img_w, img_h);
                wm_region = cv::Rect(pos.x, pos.y, config.logo_size, config.logo_size);
                detected = true;

                spdlog::info("Detected ({:.0f}%), region: ({},{}) {}x{}",
                             confidence * 100.0f,
                             wm_region.x, wm_region.y,
                             wm_region.width, wm_region.height);
            }
        }

        // If not detected, try fallback strategies
        if (!detected) {
            if (force_process && !explicit_region.empty()) {
                // --force + --region: use explicit region directly
                wm_region = explicit_region;
                using_custom = true;
                spdlog::info("Force mode: using explicit region ({},{}) {}x{}",
                             wm_region.x, wm_region.y,
                             wm_region.width, wm_region.height);
            } else if (force_process) {
                // --force without region: use standard position
                auto config = get_watermark_config(img_w, img_h);
                auto pos = config.get_position(img_w, img_h);
                wm_region = cv::Rect(pos.x, pos.y, config.logo_size, config.logo_size);
                spdlog::info("Force mode: using default position");
            } else if (!fallback_region_str.empty()) {
                // Fallback region: parse and optionally snap
                cv::Rect fb_region = parse_region(fallback_region_str, img_w, img_h);
                if (fb_region.empty()) {
                    result.failed++;
                    fmt::print(fmt::fg(fmt::color::red), "[FAIL] ");
                    fmt::print("{}: Invalid fallback region\n", gwt::filename_utf8(input));
                    return;
                }

                if (use_snap) {
                    // Snap search within fallback region
                    int max_sz = std::min(snap_max_size,
                                          std::min(fb_region.width, fb_region.height));
                    GuidedDetectionResult snap = engine.guided_detect(
                        image, fb_region, nullptr, 32, max_sz);

                    if (snap.found && (snap.confidence >= snap_threshold || force_process)) {
                        wm_region = snap.match_rect;
                        using_custom = true;
                        confidence = snap.confidence;
                        spdlog::info("Snap found: ({},{}) {}x{} ({:.0f}%)",
                                     wm_region.x, wm_region.y,
                                     snap.detected_size, snap.detected_size,
                                     snap.confidence * 100.0f);
                    } else {
                        // Snap failed or confidence too low → bypass
                        result.skipped++;
                        fmt::print(fmt::fg(fmt::color::yellow), "[SKIP] ");
                        if (snap.found) {
                            fmt::print("{}: Snap confidence too low ({:.0f}% < {:.0f}%)\n",
                                       gwt::filename_utf8(input),
                                       snap.confidence * 100.0f,
                                       snap_threshold * 100.0f);
                        } else {
                            fmt::print("{}: No watermark found in fallback region ({} scales)\n",
                                       gwt::filename_utf8(input), snap.scales_searched);
                        }
                        return;
                    }
                } else {
                    // Use fallback region directly without snap
                    wm_region = fb_region;
                    using_custom = true;
                    spdlog::info("Using fallback region: ({},{}) {}x{}",
                                 wm_region.x, wm_region.y,
                                 wm_region.width, wm_region.height);
                }
            } else {
                // No fallback, no force → bypass
                result.skipped++;
                fmt::print(fmt::fg(fmt::color::yellow), "[SKIP] ");
                fmt::print("{}: No watermark detected ({:.0f}%)\n",
                           gwt::filename_utf8(input), confidence * 100.0f);
                return;
            }
        }

        // If explicit region given and detected → try snap refinement
        if (detected && !explicit_region.empty() && use_snap) {
            int max_sz = std::min(snap_max_size,
                                  std::min(explicit_region.width, explicit_region.height));
            GuidedDetectionResult snap = engine.guided_detect(
                image, explicit_region, nullptr, 32, max_sz);

            if (snap.found && snap.confidence >= snap_threshold) {
                wm_region = snap.match_rect;
                using_custom = true;
                spdlog::info("Snap refined: ({},{}) {}x{} ({:.0f}%)",
                             wm_region.x, wm_region.y,
                             snap.detected_size, snap.detected_size,
                             snap.confidence * 100.0f);
            } else {
                // Snap unreliable → keep original detected position
                spdlog::info("Snap refinement skipped (confidence {:.0f}%), using detected position",
                             snap.found ? snap.confidence * 100.0f : 0.0f);
            }
        }

        // =================================================================
        // Phase 2: Remove watermark
        // =================================================================

        if (using_custom) {
            engine.remove_watermark_custom(image, wm_region);
        } else {
            engine.remove_watermark(image, force_size);
        }

        // =================================================================
        // Phase 3: Denoise cleanup (if enabled)
        // =================================================================

        if (denoise_cfg.enabled) {
#ifdef GWT_HAS_AI_DENOISE
            if (denoise_cfg.method == InpaintMethod::AI_DENOISE && denoiser && denoiser->is_ready()) {
                const auto& alpha = engine.get_alpha_map(WatermarkSize::Large);
                denoiser->denoise(
                    image, wm_region, alpha,
                    denoise_cfg.sigma, denoise_cfg.strength, 32);
            } else
#endif
            {
                engine.inpaint_residual(
                    image, wm_region,
                    denoise_cfg.strength, denoise_cfg.method,
                    denoise_cfg.radius, 32);
            }
        }

        // =================================================================
        // Phase 4: Save
        // =================================================================

        auto output_dir = output.parent_path();
        if (!output_dir.empty() && !fs::exists(output_dir)) {
            fs::create_directories(output_dir);
        }

        std::vector<int> params;
        std::string ext = output.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".jpg" || ext == ".jpeg") {
            params = {cv::IMWRITE_JPEG_QUALITY, 100};
        } else if (ext == ".png") {
            params = {cv::IMWRITE_PNG_COMPRESSION, 6};
        } else if (ext == ".webp") {
            params = {cv::IMWRITE_WEBP_QUALITY, 101};
        }

        if (!cv::imwrite(output.string(), image, params)) {
            result.failed++;
            fmt::print(fmt::fg(fmt::color::red), "[FAIL] ");
            fmt::print("{}: Failed to write\n", gwt::filename_utf8(input));
            return;
        }

        result.success++;
        fmt::print(fmt::fg(fmt::color::green), "[OK] ");
        fmt::print("{}", gwt::filename_utf8(input));
        if (confidence > 0) {
            fmt::print(fmt::fg(fmt::color::gray), " ({:.0f}%)", confidence * 100.0f);
        }
        if (using_custom) {
            fmt::print(fmt::fg(fmt::color::gray), " [{}x{}]",
                       wm_region.width, wm_region.height);
        }
        if (denoise_cfg.enabled) {
#ifdef GWT_HAS_AI_DENOISE
            if (denoise_cfg.method == InpaintMethod::AI_DENOISE) {
                fmt::print(fmt::fg(fmt::color::gray), " +AI");
            } else
#endif
            {
                fmt::print(fmt::fg(fmt::color::gray), " +inpaint");
            }
        }
        fmt::print("\n");

    } catch (const std::exception& e) {
        result.failed++;
        fmt::print(fmt::fg(fmt::color::red), "[FAIL] ");
        fmt::print("{}: {}\n", gwt::filename_utf8(input), e.what());
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

/**
 * Check if any advanced CLI options are present.
 * When true, use process_single_advanced instead of process_single.
 */
bool has_advanced_options(
    const std::string& region_str,
    const std::string& fallback_region_str,
    const std::string& denoise_method_str,
    bool use_snap)
{
    return !region_str.empty() ||
           !fallback_region_str.empty() ||
           !denoise_method_str.empty() ||
           use_snap;
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

    // =====================================================================
    // Existing options (unchanged)
    // =====================================================================

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

    // =====================================================================
    // New options (v0.2.5: region, snap, denoise)
    // =====================================================================

    std::string region_str;
    app.add_option("--region", region_str,
                   "Explicit watermark region: x,y,w,h or br:mx,my,w,h or br:auto");

    std::string fallback_region_str;
    app.add_option("--fallback-region", fallback_region_str,
                   "Fallback search region when detection fails (same syntax as --region)");

    bool use_snap = false;
    app.add_flag("--snap", use_snap,
                 "Enable multi-scale snap search within region/fallback-region");

    int snap_max_size = 160;
    app.add_option("--snap-max-size", snap_max_size,
                   "Maximum watermark size for snap search (default: 160)")
        ->check(CLI::Range(32, 320));

    float snap_threshold = 0.60f;
    app.add_option("--snap-threshold", snap_threshold,
                   "Minimum snap confidence to accept match (0.0-1.0, default: 0.60)")
        ->check(CLI::Range(0.0f, 1.0f));

    std::string denoise_method_str;
    app.add_option("--denoise", denoise_method_str,
                   "Cleanup method after removal: ai, ns, telea, soft, off (default: off in CLI)");

    float cli_sigma = -1.0f;  // -1 = use default
    app.add_option("--sigma", cli_sigma,
                   "AI denoise sigma (1-150, default: 50)")
        ->check(CLI::Range(1.0f, 150.0f));

    float cli_strength = -1.0f;  // -1 = use default
    app.add_option("--strength", cli_strength,
                   "Denoise strength in percent (0-300, default: 120 for AI, 85 for others)")
        ->check(CLI::Range(0.0f, 300.0f));

    int cli_radius = -1;  // -1 = use default
    app.add_option("--radius", cli_radius,
                   "Inpaint radius for NS/TELEA/Soft (1-25, default: 10)")
        ->check(CLI::Range(1, 25));

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

    // Build denoise config
    DenoiseConfig denoise_cfg;
    if (!denoise_method_str.empty()) {
        denoise_cfg = DenoiseConfig::from_string(denoise_method_str);

        // Override defaults with explicit CLI params
        if (cli_sigma >= 0) denoise_cfg.sigma = cli_sigma;
        if (cli_strength >= 0) denoise_cfg.strength = cli_strength / 100.0f;
        if (cli_radius >= 0) denoise_cfg.radius = cli_radius;
    }

    // Check if we need the advanced pipeline
    bool advanced = has_advanced_options(region_str, fallback_region_str,
                                        denoise_method_str, use_snap);

    // Print detection status
    if (use_detection) {
        fmt::print(fmt::fg(fmt::color::gray),
                   "Auto-detection enabled (threshold: {:.0f}%)\n",
                   detection_threshold * 100.0f);
    } else {
        fmt::print(fmt::fg(fmt::color::yellow),
                   "WARNING: Force mode - processing ALL images without detection!\n");
    }

    if (advanced) {
        if (!region_str.empty())
            fmt::print(fmt::fg(fmt::color::gray), "Region: {}\n", region_str);
        if (!fallback_region_str.empty())
            fmt::print(fmt::fg(fmt::color::gray), "Fallback: {}\n", fallback_region_str);
        if (use_snap)
            fmt::print(fmt::fg(fmt::color::gray), "Snap: enabled (max {}px, threshold {:.0f}%)\n",
                       snap_max_size, snap_threshold * 100.0f);
        if (denoise_cfg.enabled) {
            const char* method_name = "NS";
#ifdef GWT_HAS_AI_DENOISE
            if (denoise_cfg.method == InpaintMethod::AI_DENOISE) method_name = "AI";
            else
#endif
            if (denoise_cfg.method == InpaintMethod::TELEA) method_name = "TELEA";
            else if (denoise_cfg.method == InpaintMethod::GAUSSIAN) method_name = "Soft";
            fmt::print(fmt::fg(fmt::color::gray), "Denoise: {} (strength={:.0f}%)\n",
                       method_name, denoise_cfg.strength * 100.0f);
        }
    }
    fmt::print("\n");

    try {
        WatermarkEngine engine(
            embedded::bg_48_png, embedded::bg_48_png_size,
            embedded::bg_96_png, embedded::bg_96_png_size
        );

#ifdef GWT_HAS_AI_DENOISE
        // Initialize AI denoiser if needed
        std::unique_ptr<NcnnDenoiser> denoiser;
        if (denoise_cfg.enabled && denoise_cfg.method == InpaintMethod::AI_DENOISE) {
            denoiser = std::make_unique<NcnnDenoiser>();
            if (!denoiser->initialize()) {
                spdlog::warn("AI denoiser init failed, falling back to NS");
                denoise_cfg.method = InpaintMethod::NS;
                denoise_cfg.strength = 0.85f;
                denoiser.reset();
            }
        }
#endif

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

                if (advanced) {
                    process_single_advanced(
                        entry.path(), out_file, engine, force_size,
                        use_detection, detection_threshold, force_process,
                        region_str, fallback_region_str,
                        use_snap, snap_max_size, snap_threshold, denoise_cfg,
#ifdef GWT_HAS_AI_DENOISE
                        denoiser.get(),
#endif
                        result);
                } else {
                    process_single(entry.path(), out_file, remove_mode, engine,
                                  force_size, use_detection, detection_threshold, result);
                }
            }

            result.print();
        } else {
            if (advanced) {
                process_single_advanced(
                    input, output, engine, force_size,
                    use_detection, detection_threshold, force_process,
                    region_str, fallback_region_str,
                    use_snap, snap_max_size, snap_threshold, denoise_cfg,
#ifdef GWT_HAS_AI_DENOISE
                    denoiser.get(),
#endif
                    result);
            } else {
                process_single(input, output, remove_mode, engine,
                              force_size, use_detection, detection_threshold, result);
            }
        }

        return (result.failed > 0) ? 1 : 0;
    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }
}

}  // namespace gwt::cli
