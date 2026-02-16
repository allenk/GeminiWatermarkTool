/**
 * @file    watermark_engine.cpp
 * @brief   Gemini Watermark Tool - Watermark Engine
 * @author  AllenK (Kwyshell)
 * @date    2025.12.13
 * @license MIT
 *
 * @details
 * Watermark Engine Implementation
 *
 * @see https://github.com/allenk/GeminiWatermarkTool
 */

#include "core/watermark_engine.hpp"
#include "core/blend_modes.hpp"
#include "utils/path_formatter.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <stdexcept>
#include <atomic>
#include <chrono>

namespace gwt {

WatermarkPosition get_watermark_config(int image_width, int image_height) {
    // Gemini's rules:
    // - Large (96x96, 64px margin): BOTH width AND height > 1024
    // - Small (48x48, 32px margin): Otherwise (including 1024x1024)

    if (image_width > 1024 && image_height > 1024) {
        return WatermarkPosition{
            .margin_right = 64,
            .margin_bottom = 64,
            .logo_size = 96
        };
    } else {
        return WatermarkPosition{
            .margin_right = 32,
            .margin_bottom = 32,
            .logo_size = 48
        };
    }
}

WatermarkSize get_watermark_size(int image_width, int image_height) {
    // Large (96x96) only when BOTH dimensions >= 1024
    // 1024x1024 is Small
    if (image_width > 1024 && image_height > 1024) {
        return WatermarkSize::Large;
    }
    return WatermarkSize::Small;
}

// Helper function to initialize alpha maps
void WatermarkEngine::init_alpha_maps(const cv::Mat& bg_small, const cv::Mat& bg_large) {
    cv::Mat small_resized = bg_small;
    cv::Mat large_resized = bg_large;

    // Resize if needed
    if (small_resized.cols != 48 || small_resized.rows != 48) {
        spdlog::warn("Small capture is {}x{}, expected 48x48. Resizing.",
                     small_resized.cols, small_resized.rows);
        cv::resize(small_resized, small_resized, cv::Size(48, 48), 0, 0, cv::INTER_AREA);
    }

    if (large_resized.cols != 96 || large_resized.rows != 96) {
        spdlog::warn("Large capture is {}x{}, expected 96x96. Resizing.",
                     large_resized.cols, large_resized.rows);
        cv::resize(large_resized, large_resized, cv::Size(96, 96), 0, 0, cv::INTER_AREA);
    }

    // Calculate alpha maps from background
    // alpha = bg_value / 255
    alpha_map_small_ = calculate_alpha_map(small_resized);
    alpha_map_large_ = calculate_alpha_map(large_resized);

    spdlog::debug("Alpha map small: {}x{}, large: {}x{}",
                  alpha_map_small_.cols, alpha_map_small_.rows,
                  alpha_map_large_.cols, alpha_map_large_.rows);

    // Log alpha statistics for debugging
    double min_val, max_val;
    cv::minMaxLoc(alpha_map_large_, &min_val, &max_val);
    spdlog::debug("Large alpha map range: {:.4f} - {:.4f}", min_val, max_val);
}

WatermarkEngine::WatermarkEngine(
    const std::filesystem::path& bg_small,
    const std::filesystem::path& bg_large,
    float logo_value)
    : logo_value_(logo_value) {

    // Load background captures from files
    cv::Mat bg_small_bk = cv::imread(bg_small.string(), cv::IMREAD_COLOR);
    if (bg_small_bk.empty()) {
        throw std::runtime_error("Failed to load small background capture: " + bg_small.string());
    }

    cv::Mat bg_large_bk = cv::imread(bg_large.string(), cv::IMREAD_COLOR);
    if (bg_large_bk.empty()) {
        throw std::runtime_error("Failed to load large background capture: " + bg_large.string());
    }

    init_alpha_maps(bg_small_bk, bg_large_bk);
    spdlog::info("Loaded background captures from files");
}

WatermarkEngine::WatermarkEngine(
    const unsigned char* png_data_small, size_t png_size_small,
    const unsigned char* png_data_large, size_t png_size_large,
    float logo_value)
    : logo_value_(logo_value) {

    // Decode PNG from memory
    std::vector<unsigned char> buf_small(png_data_small, png_data_small + png_size_small);
    std::vector<unsigned char> buf_large(png_data_large, png_data_large + png_size_large);

    cv::Mat bg_small = cv::imdecode(buf_small, cv::IMREAD_COLOR);
    if (bg_small.empty()) {
        throw std::runtime_error("Failed to decode embedded small background capture");
    }

    cv::Mat bg_large = cv::imdecode(buf_large, cv::IMREAD_COLOR);
    if (bg_large.empty()) {
        throw std::runtime_error("Failed to decode embedded large background capture");
    }

    init_alpha_maps(bg_small, bg_large);
    spdlog::info("Loaded embedded background captures (standalone mode)");
}

void WatermarkEngine::remove_watermark(
    cv::Mat& image,
    std::optional<WatermarkSize> force_size) {
    if (image.empty()) {
        throw std::runtime_error("Empty image provided");
    }

    // Ensure BGR format
    if (image.channels() == 4) {
        cv::cvtColor(image, image, cv::COLOR_BGRA2BGR);
    } else if (image.channels() == 1) {
        cv::cvtColor(image, image, cv::COLOR_GRAY2BGR);
    }

    // Determine watermark size
    WatermarkSize size = force_size.value_or(
        get_watermark_size(image.cols, image.rows)
    );

    // Get position config based on actual size used
    WatermarkPosition config;
    if (size == WatermarkSize::Small) {
        config = WatermarkPosition{32, 32, 48};
    } else {
        config = WatermarkPosition{64, 64, 96};
    }

    cv::Point pos = config.get_position(image.cols, image.rows);
    const cv::Mat& alpha_map = get_alpha_map(size);

    spdlog::debug("Removing watermark at ({}, {}) with {}x{} alpha map (size: {})",
                  pos.x, pos.y, alpha_map.cols, alpha_map.rows,
                  size == WatermarkSize::Small ? "Small" : "Large");

    // Apply reverse alpha blending
    remove_watermark_alpha_blend(image, alpha_map, pos, logo_value_);
}


void WatermarkEngine::add_watermark(
    cv::Mat& image,
    std::optional<WatermarkSize> force_size) {
    if (image.empty()) {
        throw std::runtime_error("Empty image provided");
    }

    // Ensure BGR format
    if (image.channels() == 4) {
        cv::cvtColor(image, image, cv::COLOR_BGRA2BGR);
    } else if (image.channels() == 1) {
        cv::cvtColor(image, image, cv::COLOR_GRAY2BGR);
    }

    // Determine watermark size
    WatermarkSize size = force_size.value_or(
        get_watermark_size(image.cols, image.rows)
    );

    // Get position config based on actual size used
    WatermarkPosition config;
    if (size == WatermarkSize::Small) {
        config = WatermarkPosition{32, 32, 48};
    } else {
        config = WatermarkPosition{64, 64, 96};
    }

    cv::Point pos = config.get_position(image.cols, image.rows);
    const cv::Mat& alpha_map = get_alpha_map(size);

    spdlog::debug("Adding watermark at ({}, {}) with {}x{} alpha map (size: {})",
                  pos.x, pos.y, alpha_map.cols, alpha_map.rows,
                  size == WatermarkSize::Small ? "Small" : "Large");

    // Apply alpha blending
    add_watermark_alpha_blend(image, alpha_map, pos, logo_value_);
}

cv::Mat& WatermarkEngine::get_alpha_map_mutable(WatermarkSize size) {
    return (size == WatermarkSize::Small) ? alpha_map_small_ : alpha_map_large_;
}

const cv::Mat& WatermarkEngine::get_alpha_map(WatermarkSize size) const {
    return (size == WatermarkSize::Small) ? alpha_map_small_ : alpha_map_large_;
}

// =============================================================================
// Watermark Detection (Three-Stage Algorithm)
// =============================================================================

DetectionResult WatermarkEngine::detect_watermark(
    const cv::Mat& image,
    std::optional<WatermarkSize> force_size) const
{
    DetectionResult result{};
    result.detected = false;
    result.confidence = 0.0f;
    result.spatial_score = 0.0f;
    result.gradient_score = 0.0f;
    result.variance_score = 0.0f;

    if (image.empty()) {
        return result;
    }

    // Determine watermark size and position
    const WatermarkSize size = force_size.value_or(get_watermark_size(image.cols, image.rows));
    const WatermarkPosition config = get_watermark_config(image.cols, image.rows);
    const cv::Point pos = config.get_position(image.cols, image.rows);
    const cv::Mat& alpha_map = get_alpha_map(size);

    result.size = size;
    result.region = cv::Rect(pos.x, pos.y, alpha_map.cols, alpha_map.rows);

    // Calculate ROI (clamp to image bounds)
    const int x1 = std::max(0, pos.x);
    const int y1 = std::max(0, pos.y);
    const int x2 = std::min(image.cols, pos.x + alpha_map.cols);
    const int y2 = std::min(image.rows, pos.y + alpha_map.rows);

    if (x1 >= x2 || y1 >= y2) {
        spdlog::debug("Detection: ROI out of bounds");
        return result;
    }

    // Extract region and convert to grayscale
    const cv::Rect image_roi(x1, y1, x2 - x1, y2 - y1);
    const cv::Mat region = image(image_roi);
    cv::Mat gray_region;

    if (region.channels() >= 3) {
        cv::cvtColor(region, gray_region, cv::COLOR_BGR2GRAY);
    } else {
        gray_region = region.clone();
    }

    // Convert to float [0, 1]
    cv::Mat gray_f;
    gray_region.convertTo(gray_f, CV_32F, 1.0 / 255.0);

    // Get corresponding alpha region
    const cv::Rect alpha_roi(x1 - pos.x, y1 - pos.y, x2 - x1, y2 - y1);
    cv::Mat alpha_region = alpha_map(alpha_roi);

    // =========================================================================
    // Stage 1: Spatial Structural Correlation (NCC)
    // The watermark's diamond/star pattern should correlate with the alpha map
    // =========================================================================
    cv::Mat spatial_match;
    cv::matchTemplate(gray_f, alpha_region, spatial_match, cv::TM_CCOEFF_NORMED);

    double min_spatial, spatial_score;
    cv::minMaxLoc(spatial_match, &min_spatial, &spatial_score);
    result.spatial_score = static_cast<float>(spatial_score);

    // Circuit Breaker: If spatial correlation is too low, definitely no watermark
    constexpr double kSpatialThreshold = 0.25;
    if (spatial_score < kSpatialThreshold) {
        spdlog::debug("Detection: spatial={:.3f} < {:.2f}, rejected",
                      spatial_score, kSpatialThreshold);
        result.confidence = static_cast<float>(spatial_score * 0.5);  // Return low confidence
        return result;
    }

    // =========================================================================
    // Stage 2: Gradient-Domain Correlation (Edge Signature)
    // Watermark edges should match alpha map edges
    // =========================================================================
    cv::Mat img_gx, img_gy, img_gmag;
    cv::Sobel(gray_f, img_gx, CV_32F, 1, 0, 3);
    cv::Sobel(gray_f, img_gy, CV_32F, 0, 1, 3);
    cv::magnitude(img_gx, img_gy, img_gmag);

    cv::Mat alpha_gx, alpha_gy, alpha_gmag;
    cv::Sobel(alpha_region, alpha_gx, CV_32F, 1, 0, 3);
    cv::Sobel(alpha_region, alpha_gy, CV_32F, 0, 1, 3);
    cv::magnitude(alpha_gx, alpha_gy, alpha_gmag);

    cv::Mat grad_match;
    cv::matchTemplate(img_gmag, alpha_gmag, grad_match, cv::TM_CCOEFF_NORMED);

    double min_grad, grad_score;
    cv::minMaxLoc(grad_match, &min_grad, &grad_score);
    result.gradient_score = static_cast<float>(grad_score);

    // =========================================================================
    // Stage 3: Statistical Variance Analysis (Texture Dampening)
    // Watermarks reduce texture variance in the affected region
    // =========================================================================
    double var_score = 0.0;
    const int ref_h = std::min(y1, config.logo_size);

    if (ref_h > 8) {
        // Use region above watermark as reference
        const cv::Rect ref_roi(x1, y1 - ref_h, x2 - x1, ref_h);
        const cv::Mat ref_region = image(ref_roi);
        cv::Mat gray_ref;

        if (ref_region.channels() >= 3) {
            cv::cvtColor(ref_region, gray_ref, cv::COLOR_BGR2GRAY);
        } else {
            gray_ref = ref_region;
        }

        cv::Scalar m_wm, s_wm, m_ref, s_ref;
        cv::meanStdDev(gray_region, m_wm, s_wm);
        cv::meanStdDev(gray_ref, m_ref, s_ref);

        if (s_ref[0] > 5.0) {
            // Watermarks dampen high-frequency background variance
            var_score = std::clamp(1.0 - (s_wm[0] / s_ref[0]), 0.0, 1.0);
        }
    }
    result.variance_score = static_cast<float>(var_score);

    // =========================================================================
    // Heuristic Fusion: Weighted Ensemble
    // =========================================================================
    const double confidence =
        (spatial_score * 0.50) +   // Spatial correlation is most important
        (grad_score * 0.30) +      // Edge signature
        (var_score * 0.20);        // Variance dampening

    result.confidence = static_cast<float>(std::clamp(confidence, 0.0, 1.0));

    // Determine if watermark is detected based on confidence threshold
    constexpr float kDetectionThreshold = 0.35f;
    result.detected = (result.confidence >= kDetectionThreshold);

    spdlog::debug("Detection: spatial={:.3f}, grad={:.3f}, var={:.3f} -> conf={:.3f} ({})",
                  spatial_score, grad_score, var_score, result.confidence,
                  result.detected ? "DETECTED" : "not detected");

    return result;
}

cv::Mat WatermarkEngine::create_interpolated_alpha(int target_width, int target_height) {
    // Use 96x96 large alpha map as source (higher resolution = better quality)
    const cv::Mat& source = alpha_map_large_;

    if (target_width == source.cols && target_height == source.rows) {
        return source.clone();
    }

    cv::Mat interpolated;

    // Use INTER_LINEAR (bilinear) for upscaling, INTER_AREA for downscaling
    int interp_method = (target_width > source.cols || target_height > source.rows)
                        ? cv::INTER_LINEAR
                        : cv::INTER_AREA;

    cv::resize(source, interpolated, cv::Size(target_width, target_height), 0, 0, interp_method);

    spdlog::debug("Created interpolated alpha map: {}x{} -> {}x{} (method: {})",
                  source.cols, source.rows, target_width, target_height,
                  interp_method == cv::INTER_LINEAR ? "bilinear" : "area");

    return interpolated;
}

void WatermarkEngine::remove_watermark_custom(
    cv::Mat& image,
    const cv::Rect& region)
{
    if (image.empty()) {
        throw std::runtime_error("Empty image provided");
    }

    // Ensure BGR format
    if (image.channels() == 4) {
        cv::cvtColor(image, image, cv::COLOR_BGRA2BGR);
    } else if (image.channels() == 1) {
        cv::cvtColor(image, image, cv::COLOR_GRAY2BGR);
    }

    // Check for exact match with standard sizes
    if (region.width == 48 && region.height == 48) {
        spdlog::info("Custom region matches 48x48, using small alpha map");
        cv::Point pos(region.x, region.y);
        remove_watermark_alpha_blend(image, alpha_map_small_, pos, logo_value_);
        return;
    }

    if (region.width == 96 && region.height == 96) {
        spdlog::info("Custom region matches 96x96, using large alpha map");
        cv::Point pos(region.x, region.y);
        remove_watermark_alpha_blend(image, alpha_map_large_, pos, logo_value_);
        return;
    }

    // Create interpolated alpha map for custom size
    cv::Mat custom_alpha = create_interpolated_alpha(region.width, region.height);
    cv::Point pos(region.x, region.y);

    spdlog::info("Removing watermark at ({},{}) with custom {}x{} alpha map",
                 pos.x, pos.y, region.width, region.height);

    remove_watermark_alpha_blend(image, custom_alpha, pos, logo_value_);
}

void WatermarkEngine::add_watermark_custom(
    cv::Mat& image,
    const cv::Rect& region)
{
    if (image.empty()) {
        throw std::runtime_error("Empty image provided");
    }

    // Ensure BGR format
    if (image.channels() == 4) {
        cv::cvtColor(image, image, cv::COLOR_BGRA2BGR);
    } else if (image.channels() == 1) {
        cv::cvtColor(image, image, cv::COLOR_GRAY2BGR);
    }

    // Check for exact match with standard sizes
    if (region.width == 48 && region.height == 48) {
        cv::Point pos(region.x, region.y);
        add_watermark_alpha_blend(image, alpha_map_small_, pos, logo_value_);
        return;
    }

    if (region.width == 96 && region.height == 96) {
        cv::Point pos(region.x, region.y);
        add_watermark_alpha_blend(image, alpha_map_large_, pos, logo_value_);
        return;
    }

    // Create interpolated alpha map for custom size
    cv::Mat custom_alpha = create_interpolated_alpha(region.width, region.height);
    cv::Point pos(region.x, region.y);

    spdlog::info("Adding watermark at ({},{}) with custom {}x{} alpha map",
                 pos.x, pos.y, region.width, region.height);

    add_watermark_alpha_blend(image, custom_alpha, pos, logo_value_);
}

// =============================================================================
// Guided Multi-Scale Detection (Snap Engine)
// =============================================================================

GuidedDetectionResult WatermarkEngine::guided_detect(
    const cv::Mat& image,
    const cv::Rect& search_rect,
    std::atomic<bool>* cancel_flag,
    int min_size,
    int max_size) const
{
    auto start_time = std::chrono::high_resolution_clock::now();

    GuidedDetectionResult result{};

    if (image.empty() || search_rect.width < 8 || search_rect.height < 8) {
        return result;
    }

    // Clamp search rect to image bounds
    cv::Rect search = search_rect & cv::Rect(0, 0, image.cols, image.rows);
    if (search.width < 8 || search.height < 8) {
        return result;
    }

    // Clamp min/max to sensible range
    min_size = std::max(min_size, 16);
    max_size = std::min(max_size, std::min(search.width, search.height));
    if (min_size > max_size) {
        spdlog::debug("guided_detect: min_size {} > max_size {}, no search possible",
                      min_size, max_size);
        return result;
    }

    // =========================================================================
    // Size preference factor
    //
    // NCC has an inherent bias toward smaller templates: a 24x24 patch can
    // trivially find a high-correlation match within any watermark region.
    // To counter this, we weight scores by a size factor that prefers
    // templates closer to the known standard size (96px).
    //
    //   adjusted_score = raw_ncc * sqrt(scale / kReferenceSize)
    //
    // This ensures a 96x96 match at NCC=0.30 beats a 24x24 match at NCC=0.58.
    // =========================================================================
    constexpr double kReferenceSize = 96.0;

    auto size_adjusted_score = [&](double raw_ncc, int scale) -> double {
        double weight = std::sqrt(static_cast<double>(scale) / kReferenceSize);
        weight = std::min(weight, 1.0);  // Cap at 1.0 for scales >= 96
        return raw_ncc * weight;
    };

    // Extract grayscale search region
    cv::Mat search_region = image(search);
    cv::Mat gray_region;
    if (search_region.channels() >= 3) {
        cv::cvtColor(search_region, gray_region, cv::COLOR_BGR2GRAY);
    } else {
        gray_region = search_region.clone();
    }
    cv::Mat gray_f;
    gray_region.convertTo(gray_f, CV_32F, 1.0 / 255.0);

    // Use alpha_map_large_ (96x96) as template source for best quality
    const cv::Mat& source_alpha = alpha_map_large_;

    // =========================================================================
    // Phase 1: Coarse search — large steps for quick scan
    // =========================================================================
    struct Candidate {
        cv::Point position;       // Position within search region
        int scale;                // Template size
        double raw_score;         // Raw NCC score
        double adjusted_score;    // Size-adjusted score
    };

    constexpr int kCoarseScaleStep = 8;
    constexpr int kTopK = 5;

    std::vector<Candidate> coarse_candidates;
    coarse_candidates.reserve(kTopK);

    // Generate scale list
    std::vector<int> coarse_scales;
    for (int s = min_size; s <= max_size; s += kCoarseScaleStep) {
        coarse_scales.push_back(s);
    }
    // Always include the exact standard sizes if in range
    for (int std_size : {48, 96}) {
        if (std_size >= min_size && std_size <= max_size) {
            bool already_present = false;
            for (int s : coarse_scales) {
                if (std::abs(s - std_size) <= 2) { already_present = true; break; }
            }
            if (!already_present) coarse_scales.push_back(std_size);
        }
    }
    std::sort(coarse_scales.begin(), coarse_scales.end());

    result.total_scales = static_cast<int>(coarse_scales.size());

    spdlog::debug("guided_detect: searching {} scales [{}-{}] in {}x{} region",
                  coarse_scales.size(), min_size, max_size,
                  search.width, search.height);

    for (int scale : coarse_scales) {
        // Check cancellation
        if (cancel_flag && cancel_flag->load(std::memory_order_relaxed)) {
            result.was_cancelled = true;
            spdlog::debug("guided_detect: cancelled at scale {}", scale);
            break;
        }

        // Template must fit within search region
        if (scale > gray_f.cols || scale > gray_f.rows) {
            result.scales_searched++;
            continue;
        }

        // Resize alpha template to current scale
        cv::Mat tmpl;
        cv::resize(source_alpha, tmpl, cv::Size(scale, scale), 0, 0,
                   scale > source_alpha.cols ? cv::INTER_LINEAR : cv::INTER_AREA);

        // NCC template matching
        cv::Mat match_result;
        cv::matchTemplate(gray_f, tmpl, match_result, cv::TM_CCOEFF_NORMED);

        // Find best match at this scale
        double min_val, max_val;
        cv::Point min_loc, max_loc;
        cv::minMaxLoc(match_result, &min_val, &max_val, &min_loc, &max_loc);

        result.scales_searched++;

        double adj = size_adjusted_score(max_val, scale);

        spdlog::debug("  scale {:3d}: raw_ncc={:.3f} adjusted={:.3f}", scale, max_val, adj);

        if (adj > 0.08) {  // Minimal threshold for coarse phase (on adjusted score)
            Candidate c{max_loc, scale, max_val, adj};

            // Insert into sorted top-K by adjusted score
            if (static_cast<int>(coarse_candidates.size()) < kTopK) {
                coarse_candidates.push_back(c);
                std::sort(coarse_candidates.begin(), coarse_candidates.end(),
                          [](const Candidate& a, const Candidate& b) {
                              return a.adjusted_score > b.adjusted_score;
                          });
            } else if (adj > coarse_candidates.back().adjusted_score) {
                coarse_candidates.back() = c;
                std::sort(coarse_candidates.begin(), coarse_candidates.end(),
                          [](const Candidate& a, const Candidate& b) {
                              return a.adjusted_score > b.adjusted_score;
                          });
            }
        }
    }

    if (coarse_candidates.empty()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start_time).count();
        spdlog::info("guided_detect: no candidates found in {} us ({} scales)",
                     elapsed, result.scales_searched);
        return result;
    }

    spdlog::debug("guided_detect: top {} coarse candidates:", coarse_candidates.size());
    for (const auto& c : coarse_candidates) {
        spdlog::debug("  scale={} pos=({},{}) raw={:.3f} adj={:.3f}",
                      c.scale, c.position.x, c.position.y, c.raw_score, c.adjusted_score);
    }

    // =========================================================================
    // Phase 2: Fine refinement around top candidates
    // =========================================================================
    constexpr int kFineScaleStep = 2;
    constexpr int kFineScaleRange = 10;  // +/- 10 pixels around candidate scale

    Candidate best{cv::Point(0, 0), 0, -1.0, -1.0};

    for (const auto& candidate : coarse_candidates) {
        if (cancel_flag && cancel_flag->load(std::memory_order_relaxed)) {
            result.was_cancelled = true;
            break;
        }

        // Refine scale around candidate
        int scale_lo = std::max(min_size, candidate.scale - kFineScaleRange);
        int scale_hi = std::min(max_size, candidate.scale + kFineScaleRange);

        for (int s = scale_lo; s <= scale_hi; s += kFineScaleStep) {
            if (s > gray_f.cols || s > gray_f.rows) continue;

            cv::Mat tmpl;
            cv::resize(source_alpha, tmpl, cv::Size(s, s), 0, 0,
                       s > source_alpha.cols ? cv::INTER_LINEAR : cv::INTER_AREA);

            cv::Mat match_result;
            cv::matchTemplate(gray_f, tmpl, match_result, cv::TM_CCOEFF_NORMED);

            double min_val, max_val;
            cv::Point min_loc, max_loc;
            cv::minMaxLoc(match_result, &min_val, &max_val, &min_loc, &max_loc);

            double adj = size_adjusted_score(max_val, s);
            if (adj > best.adjusted_score) {
                best = Candidate{max_loc, s, max_val, adj};
            }
        }
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - start_time).count();

    if (best.adjusted_score > 0.08) {
        // Convert position from search-region-relative to image-absolute
        result.found = true;
        result.confidence = static_cast<float>(best.adjusted_score);
        result.raw_ncc = static_cast<float>(best.raw_score);
        result.match_rect = cv::Rect(
            search.x + best.position.x,
            search.y + best.position.y,
            best.scale, best.scale
        );
        result.detected_size = best.scale;

        spdlog::info("guided_detect: found at ({},{}) size {}x{} "
                     "raw_ncc={:.3f} adjusted={:.3f} "
                     "in {} us ({} coarse scales, {} candidates refined)",
                     result.match_rect.x, result.match_rect.y,
                     best.scale, best.scale,
                     best.raw_score, best.adjusted_score,
                     elapsed, result.scales_searched,
                     coarse_candidates.size());
    } else {
        spdlog::info("guided_detect: no match above threshold in {} us", elapsed);
    }

    return result;
}

ProcessResult process_image(
    const std::filesystem::path& input_path,
    const std::filesystem::path& output_path,
    bool remove,
    WatermarkEngine& engine,
    std::optional<WatermarkSize> force_size,
    bool use_detection,
    float detection_threshold) {

    ProcessResult result{};
    result.success = false;
    result.skipped = false;
    result.confidence = 0.0f;

    try {
        // Read image
        cv::Mat image = cv::imread(input_path.string(), cv::IMREAD_COLOR);
        if (image.empty()) {
            result.message = "Failed to load image";
            spdlog::error("Failed to load image: {}", input_path);
            return result;
        }

        spdlog::info("Processing: {} ({}x{})",
                     input_path.filename(),
                     image.cols, image.rows);

        // Watermark detection (only for removal mode)
        if (use_detection && remove) {
            DetectionResult detection = engine.detect_watermark(image, force_size);
            result.confidence = detection.confidence;

            if (!detection.detected && detection.confidence < detection_threshold) {
                result.skipped = true;
                result.success = true;  // Not an error, just skipped
                result.message = fmt::format("No watermark detected ({:.0f}%), skipped",
                                             detection.confidence * 100.0f);
                spdlog::info("{}: {} (spatial={:.2f}, grad={:.2f}, var={:.2f})",
                             input_path.filename(), result.message,
                             detection.spatial_score, detection.gradient_score,
                             detection.variance_score);
                return result;
            }

            spdlog::info("Watermark detected ({:.0f}% confidence), processing...",
                         detection.confidence * 100.0f);
        }

        // Process image
        if (remove) {
            engine.remove_watermark(image, force_size);
        } else {
            engine.add_watermark(image, force_size);
        }

        // Create output directory if needed
        auto output_dir = output_path.parent_path();
        if (!output_dir.empty() && !std::filesystem::exists(output_dir)) {
            std::filesystem::create_directories(output_dir);
        }

        // Determine output format and quality
        std::vector<int> params;
        std::string ext = output_path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".jpg" || ext == ".jpeg") {
            params = {cv::IMWRITE_JPEG_QUALITY, 100};
        } else if (ext == ".png") {
            params = {cv::IMWRITE_PNG_COMPRESSION, 6};
        } else if (ext == ".webp") {
            params = {cv::IMWRITE_WEBP_QUALITY, 101};
        }

        // Write output
        bool write_success = cv::imwrite(output_path.string(), image, params);
        if (!write_success) {
            result.message = "Failed to write image";
            spdlog::error("Failed to write image: {}", output_path);
            return result;
        }

        result.success = true;
        result.message = remove ? "Watermark removed" : "Watermark added";
        spdlog::info("Saved: {}", output_path.filename());
        return result;

    } catch (const std::exception& e) {
        result.message = std::string("Error: ") + e.what();
        spdlog::error("Error processing {}: {}", input_path, e.what());
        return result;
    }
}

} // namespace gwt
