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

#include "watermark_engine.hpp"
#include "blend_modes.hpp"
#include "path_formatter.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>

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

double WatermarkEngine::detect_watermark(
    const cv::Mat& image,
    std::optional<WatermarkSize> force_size) {
    if (image.empty()) return 0.0;

    const WatermarkSize size = force_size.value_or(get_watermark_size(image.cols, image.rows));
    const WatermarkPosition config = get_watermark_config(image.cols, image.rows);
    const cv::Point pos = config.get_position(image.cols, image.rows);
    const cv::Mat& alpha_map = get_alpha_map(size);

    const int x1 = std::max(0, pos.x);
    const int y1 = std::max(0, pos.y);
    const int x2 = std::min(image.cols, pos.x + alpha_map.cols);
    const int y2 = std::min(image.rows, pos.y + alpha_map.rows);

    if (x1 >= x2 || y1 >= y2) return 0.0;

    const cv::Rect image_roi(x1, y1, x2 - x1, y2 - y1);
    const cv::Mat region = image(image_roi);
    cv::Mat gray_region;
    if (region.channels() >= 3) {
        cv::cvtColor(region, gray_region, cv::COLOR_BGR2GRAY);
    } else {
        gray_region = region.clone();
    }

    // Stage 1: Spatial Structural Correlation (NCC)
    cv::Mat gray_f, alpha_region;
    gray_region.convertTo(gray_f, CV_32F, 1.0 / 255.0);
    const cv::Rect alpha_roi(x1 - pos.x, y1 - pos.y, x2 - x1, y2 - y1);
    alpha_region = alpha_map(alpha_roi);

    cv::Mat spatial_match;
    cv::matchTemplate(gray_f, alpha_region, spatial_match, cv::TM_CCOEFF_NORMED);
    double min_spatial, spatial_score;
    cv::minMaxLoc(spatial_match, &min_spatial, &spatial_score);

    // Circuit Breaker: Mandatory structural alignment
    if (spatial_score < 0.25) {
        return 0.0;
    }

    // Stage 2: Gradient-Domain Correlation (Edge Signature)
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

    // Stage 3: Statistical Variance Analysis (Texture Dampening)
    double var_score = 0.0;
    const int ref_h = std::min(y1, config.logo_size);
    if (ref_h > 8) {
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

    // Heuristic Fusion: Weighted Ensemble
    const double confidence = (spatial_score * 0.5) + (grad_score * 0.3) + (var_score * 0.2);
    spdlog::debug("Detection: spatial={:.3f}, grad={:.3f}, var={:.3f} -> conf={:.3f}",
                  spatial_score, grad_score, var_score, confidence);

    return std::clamp(confidence, 0.0, 1.0);
}

void WatermarkEngine::remove_watermark(
    cv::Mat& image,
    std::optional<WatermarkSize> force_size,
    bool unsafe) {
    if (image.empty()) {
        throw std::runtime_error("Empty image provided");
    }

    // Ensure BGR format
    if (image.channels() == 4) {
        cv::cvtColor(image, image, cv::COLOR_BGRA2BGR);
    } else if (image.channels() == 1) {
        cv::cvtColor(image, image, cv::COLOR_GRAY2BGR);
    }

    const WatermarkSize size = force_size.value_or(get_watermark_size(image.cols, image.rows));
    const WatermarkPosition config = get_watermark_config(image.cols, image.rows);
    const cv::Point pos = config.get_position(image.cols, image.rows);
    cv::Mat& alpha_map = get_alpha_map(size);

    if (!unsafe && detect_watermark(image, force_size) < 0.30) {
        spdlog::info("No watermark detected, skipping removal.");
        return;
    }

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
    cv::Mat& alpha_map = get_alpha_map(size);

    spdlog::debug("Adding watermark at ({}, {}) with {}x{} alpha map (size: {})",
                  pos.x, pos.y, alpha_map.cols, alpha_map.rows,
                  size == WatermarkSize::Small ? "Small" : "Large");

    // Apply alpha blending
    add_watermark_alpha_blend(image, alpha_map, pos, logo_value_);
}

cv::Mat& WatermarkEngine::get_alpha_map(WatermarkSize size) {
    return (size == WatermarkSize::Small) ? alpha_map_small_ : alpha_map_large_;
}

bool process_image(
    const std::filesystem::path& input_path,
    const std::filesystem::path& output_path,
    bool remove,
    WatermarkEngine& engine,
    std::optional<WatermarkSize> force_size,
    bool unsafe) {
    try {
        // Read image
        cv::Mat image = cv::imread(input_path.string(), cv::IMREAD_COLOR);
        if (image.empty()) {
            spdlog::error("Failed to load image: {}", input_path);
            return false;
        }

        spdlog::info("Processing: {} ({}x{})",
                     input_path.filename(),
                     image.cols, image.rows);

        // Process with force_size parameter
        if (remove) {
            engine.remove_watermark(image, force_size, unsafe);
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
            // JPEG: 100 = minimal loss (still lossy, but best quality)
            params = {cv::IMWRITE_JPEG_QUALITY, 100};
        } else if (ext == ".png") {
            // PNG: lossless, compression level only affects file size/speed
            params = {cv::IMWRITE_PNG_COMPRESSION, 6};
        } else if (ext == ".webp") {
            // WebP: 101+ = lossless mode
            params = {cv::IMWRITE_WEBP_QUALITY, 101};
        }

        // Write output
        bool success = cv::imwrite(output_path.string(), image, params);
        if (!success) {
            spdlog::error("Failed to write image: {}", output_path);
            return false;
        }

        spdlog::info("Saved: {}", output_path.filename());
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Error processing {}: {}", input_path, e.what());
        return false;
    }
}

} // namespace gwt
