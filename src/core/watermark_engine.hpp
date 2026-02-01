#pragma once

#include <opencv2/core.hpp>
#include <string>
#include <optional>
#include <filesystem>

namespace gwt {

/**
 * Watermark size mode based on image dimensions
 */
enum class WatermarkSize {
    Small,   // 48x48, for images <= 1024x1024
    Large,   // 96x96, for images > 1024x1024
};

/**
 * Watermark detection result
 */
struct DetectionResult {
    bool detected;           // Whether watermark was detected
    float confidence;        // Detection confidence (0.0 - 1.0)
    cv::Rect region;         // Detected watermark region
    WatermarkSize size;      // Detected watermark size
    
    // Debug info
    float spatial_score;     // Stage 1: Spatial NCC score
    float gradient_score;    // Stage 2: Gradient NCC score  
    float variance_score;    // Stage 3: Variance analysis score
};

/**
 * Watermark position configuration
 */
struct WatermarkPosition {
    int margin_right;   // Distance from right edge
    int margin_bottom;  // Distance from bottom edge
    int logo_size;      // 48 or 96

    // Get top-left position for a given image size
    cv::Point get_position(int image_width, int image_height) const {
        return cv::Point(
            image_width - margin_right - logo_size,
            image_height - margin_bottom - logo_size
        );
    }
};

/**
 * Get the appropriate watermark configuration based on image size
 *
 * Rules discovered from Gemini:
 *   - W >= 1024 AND H >= 1024: 96x96 logo at (W-64-96, H-64-96)
 *   - Otherwise:               48x48 logo at (W-32-48, H-32-48)
 */
WatermarkPosition get_watermark_config(int image_width, int image_height);

/**
 * Determine watermark size mode from image dimensions
 */
WatermarkSize get_watermark_size(int image_width, int image_height);

/**
 * Main watermark engine class
 *
 * Uses background captures to dynamically calculate alpha maps.
 * No pre-processed masks needed - just the original captures.
 *
 * Math:
 *   Gemini adds watermark: result = alpha * logo + (1 - alpha) * original
 *   To remove: original = (result - alpha * 255) / (1 - alpha)
 */
class WatermarkEngine {
public:
    /**
     * Initialize the engine with background captures from files
     *
     * These are the raw captures from Gemini on pure background.
     * The engine will dynamically calculate alpha maps from them.
     *
     * @param bg_small  Path to 48x48 background capture
     * @param bg_large  Path to 96x96 background capture
     * @param logo_value      The logo brightness (default: 255.0 = white)
     */
    WatermarkEngine(
        const std::filesystem::path& bg_small,
        const std::filesystem::path& bg_large,
        float logo_value = 255.0f
    );

    /**
     * Initialize the engine with embedded PNG data (standalone mode)
     *
     * @param png_data_small  Pointer to 48x48 PNG data
     * @param png_size_small  Size of 48x48 PNG data
     * @param png_data_large  Pointer to 96x96 PNG data
     * @param png_size_large  Size of 96x96 PNG data
     * @param logo_value      The logo brightness (default: 255.0 = white)
     */
    WatermarkEngine(
        const unsigned char* png_data_small, size_t png_size_small,
        const unsigned char* png_data_large, size_t png_size_large,
        float logo_value = 255.0f
    );

    /**
     * Detect watermark in an image using alpha map correlation
     *
     * Three-stage detection algorithm:
     *   Stage 1: Spatial NCC - Normalized cross-correlation with alpha map
     *   Stage 2: Gradient NCC - Edge signature correlation
     *   Stage 3: Variance Analysis - Texture dampening detection
     *
     * @param image      The image to analyze
     * @param force_size Force a specific watermark size (auto-detect if nullopt)
     * @return           Detection result with confidence and region
     */
    DetectionResult detect_watermark(
        const cv::Mat& image,
        std::optional<WatermarkSize> force_size = std::nullopt
    ) const;

    /**
     * Remove watermark from an image
     *
     * @param image     The image to process (will be modified in-place)
     * @param force_size Force a specific watermark size (auto-detect if nullopt)
     */
    void remove_watermark(
        cv::Mat& image,
        std::optional<WatermarkSize> force_size = std::nullopt
    );

    /**
     * Remove watermark from a custom region with interpolated alpha map
     *
     * @param image     The image to process (will be modified in-place)
     * @param region    Custom watermark region (position + size)
     */
    void remove_watermark_custom(
        cv::Mat& image,
        const cv::Rect& region
    );

    /**
     * Add watermark to an image (Gemini-style)
     *
     * @param image     The image to process (will be modified in-place)
     * @param force_size Force a specific watermark size (auto-detect if nullopt)
     */
    void add_watermark(
        cv::Mat& image,
        std::optional<WatermarkSize> force_size = std::nullopt
    );

    /**
     * Add watermark at a custom region with interpolated alpha map
     *
     * @param image     The image to process (will be modified in-place)
     * @param region    Custom watermark region (position + size)
     */
    void add_watermark_custom(
        cv::Mat& image,
        const cv::Rect& region
    );

    /**
     * Get the alpha map for a specific size (for external use)
     */
    const cv::Mat& get_alpha_map(WatermarkSize size) const;

private:
    cv::Mat alpha_map_small_;   // 48x48 alpha map (CV_32FC1, 0.0-1.0)
    cv::Mat alpha_map_large_;   // 96x96 alpha map (CV_32FC1, 0.0-1.0)
    float logo_value_;          // Logo brightness (255 = white)

    cv::Mat& get_alpha_map_mutable(WatermarkSize size);
    
    /**
     * Create an interpolated alpha map for a custom size
     * Uses bilinear interpolation from the 96x96 alpha map
     *
     * @param target_width   Target width
     * @param target_height  Target height
     * @return               Interpolated alpha map (CV_32FC1)
     */
    cv::Mat create_interpolated_alpha(int target_width, int target_height);

    // Helper to initialize alpha maps from cv::Mat
    void init_alpha_maps(const cv::Mat& bg_small, const cv::Mat& bg_large);
};

/**
 * Result of processing an image
 */
struct ProcessResult {
    bool success;              // Whether processing succeeded
    bool skipped;              // Whether processing was skipped (no watermark detected)
    float confidence;          // Detection confidence (if detection was used)
    std::string message;       // Status message
};

/**
 * Process a single image file
 *
 * @param input_path   Input image path
 * @param output_path  Output image path
 * @param remove       Remove watermark (true) or add watermark (false)
 * @param engine       The watermark engine to use
 * @param force_size   Force a specific watermark size (auto-detect if nullopt)
 * @param use_detection  Enable watermark detection before processing
 * @param detection_threshold  Confidence threshold for detection (default: 0.25)
 * @return             Processing result
 */
ProcessResult process_image(
    const std::filesystem::path& input_path,
    const std::filesystem::path& output_path,
    bool remove,
    WatermarkEngine& engine,
    std::optional<WatermarkSize> force_size = std::nullopt,
    bool use_detection = false,
    float detection_threshold = 0.25f
);

} // namespace gwt
