/**
 * @file    watermark_detector.hpp
 * @brief   Watermark Region Detection (Legacy Interface)
 * @author  AllenK (Kwyshell)
 * @license MIT
 *
 * @details
 * This module provides a standalone detection interface that wraps
 * WatermarkEngine::detect_watermark() for backward compatibility.
 *
 * The detection algorithm uses three-stage analysis:
 * 1. Spatial NCC - Normalized cross-correlation with alpha map
 * 2. Gradient NCC - Edge signature correlation
 * 3. Variance Analysis - Texture dampening detection
 *
 * For new code, prefer using WatermarkEngine::detect_watermark() directly.
 */

#pragma once

#include "core/watermark_engine.hpp"
#include <opencv2/core.hpp>
#include <optional>

namespace gwt {

// Re-export DetectionResult from watermark_engine.hpp
// (already defined there with full detection info)

/**
 * Detect potential watermark regions in an image
 *
 * This is a convenience wrapper that creates a temporary WatermarkEngine
 * and calls detect_watermark(). For better performance when processing
 * multiple images, use WatermarkEngine directly.
 *
 * @param image      Input image (BGR, 8-bit)
 * @param hint_rect  Ignored (kept for API compatibility)
 * @return           Detection result, or std::nullopt if nothing found
 */
std::optional<DetectionResult> detect_watermark_region(
    const cv::Mat& image,
    const std::optional<cv::Rect>& hint_rect = std::nullopt
);

/**
 * Get fallback watermark region based on image dimensions
 * Used when detection fails
 *
 * @param image_width   Image width
 * @param image_height  Image height
 * @return              Default watermark region
 */
cv::Rect get_fallback_watermark_region(int image_width, int image_height);

}  // namespace gwt
