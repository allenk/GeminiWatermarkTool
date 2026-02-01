/**
 * @file    watermark_detector.cpp
 * @brief   Watermark Region Detection Implementation
 * @author  AllenK (Kwyshell)
 * @license MIT
 *
 * @details
 * Three-stage watermark detection using alpha map correlation:
 * 1. Spatial NCC - Normalized cross-correlation with alpha map
 * 2. Gradient NCC - Edge signature correlation
 * 3. Variance Analysis - Texture dampening detection
 *
 * This provides a standalone detection interface that wraps
 * WatermarkEngine::detect_watermark() for backward compatibility.
 */

#include "core/watermark_detector.hpp"
#include "core/watermark_engine.hpp"
#include "embedded_assets.hpp"

#include <opencv2/imgproc.hpp>
#include <spdlog/spdlog.h>
#include <chrono>
#include <memory>

namespace gwt {

namespace {

// Lazy-initialized singleton WatermarkEngine for detection
// This avoids repeatedly creating engines when detect_watermark_region is called
std::unique_ptr<WatermarkEngine> g_detection_engine;

WatermarkEngine& get_detection_engine() {
    if (!g_detection_engine) {
        g_detection_engine = std::make_unique<WatermarkEngine>(
            embedded::bg_48_png, embedded::bg_48_png_size,
            embedded::bg_96_png, embedded::bg_96_png_size
        );
    }
    return *g_detection_engine;
}

}  // anonymous namespace

// =============================================================================
// Public API
// =============================================================================

std::optional<DetectionResult> detect_watermark_region(
    const cv::Mat& image,
    const std::optional<cv::Rect>& /*hint_rect*/)
{
    if (image.empty()) return std::nullopt;

    auto start_time = std::chrono::high_resolution_clock::now();

    spdlog::info("Watermark detection in {}x{} image", image.cols, image.rows);

    // Use WatermarkEngine's three-stage detection algorithm
    WatermarkEngine& engine = get_detection_engine();
    DetectionResult result = engine.detect_watermark(image);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time).count();

    spdlog::info("Detection completed in {} us: spatial={:.2f} grad={:.2f} "
                 "var={:.2f} -> confidence={:.2f} ({})",
                 duration,
                 result.spatial_score,
                 result.gradient_score,
                 result.variance_score,
                 result.confidence,
                 result.detected ? "DETECTED" : "not detected");

    return result;
}

cv::Rect get_fallback_watermark_region(int image_width, int image_height) {
    WatermarkPosition config = get_watermark_config(image_width, image_height);
    cv::Point pos = config.get_position(image_width, image_height);
    return cv::Rect(pos.x, pos.y, config.logo_size, config.logo_size);
}

}  // namespace gwt
