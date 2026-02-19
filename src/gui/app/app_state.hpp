/**
 * @file    app_state.hpp
 * @brief   Application State for GUI
 * @author  AllenK (Kwyshell)
 * @license MIT
 *
 * @details
 * Defines all shared state for GUI components.
 * This is the single source of truth for UI state.
 */

#pragma once

#include "core/watermark_engine.hpp"
#include "gui/backend/render_backend.hpp"
#include "gui/resources/style.hpp"

#include <opencv2/core.hpp>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace gwt::gui {

// =============================================================================
// Enumerations
// =============================================================================

/**
 * Processing state machine
 */
enum class ProcessState {
    Idle,           // No image loaded
    Loaded,         // Image loaded, ready to process
    Processing,     // Currently processing
    Completed,      // Processing completed
    Error           // Error occurred
};

[[nodiscard]] constexpr std::string_view to_string(ProcessState state) noexcept {
    switch (state) {
        case ProcessState::Idle:       return "Idle";
        case ProcessState::Loaded:     return "Loaded";
        case ProcessState::Processing: return "Processing";
        case ProcessState::Completed:  return "Completed";
        case ProcessState::Error:      return "Error";
        default:                        return "Unknown";
    }
}

/**
 * Watermark size selection mode
 */
enum class WatermarkSizeMode {
    Auto,       // Auto-detect based on image dimensions
    Small,      // Force 48x48
    Large,      // Force 96x96
    Custom      // User-defined region
};

// =============================================================================
// Custom Watermark Interaction State
// =============================================================================

/**
 * Anchor point for resizing custom watermark rect
 */
enum class AnchorPoint {
    None,
    TopLeft, Top, TopRight,
    Left, Right,
    BottomLeft, Bottom, BottomRight,
    Body  // Drag entire rect
};

/**
 * Snap result from guided multi-scale detection
 */
struct SnapResult {
    cv::Rect snapped_rect{0, 0, 0, 0};   // Precise watermark position and size
    float snap_confidence{0.0f};           // NCC confidence score
    bool snap_found{false};                // Whether a match was found
    int detected_size{0};                  // Detected watermark size in pixels
};

/**
 * State for custom watermark mode interaction
 */
struct CustomWatermarkState {
    // The custom watermark region (in image pixel coordinates)
    // This is the user's search box / manually drawn rect
    cv::Rect region{0, 0, 0, 0};

    // Whether the region has been set
    bool has_region{false};

    // Whether auto-detection has been attempted
    bool detection_attempted{false};

    // Interaction state
    bool is_drawing{false};         // User is drawing a new rect
    bool is_resizing{false};        // User is resizing via anchor
    AnchorPoint active_anchor{AnchorPoint::None};
    cv::Point drag_start{0, 0};     // Mouse start pos (image coords)
    cv::Rect drag_start_rect{0, 0, 0, 0};  // Rect at drag start

    // Detection confidence (0.0 = fallback, 1.0 = high confidence)
    float detection_confidence{0.0f};

    // Snap result (from guided multi-scale detection)
    std::optional<SnapResult> snap_result;
    bool snap_in_progress{false};           // Snap search is running

    void clear() {
        region = cv::Rect{0, 0, 0, 0};
        has_region = false;
        detection_attempted = false;
        is_drawing = false;
        is_resizing = false;
        active_anchor = AnchorPoint::None;
        detection_confidence = 0.0f;
        snap_result.reset();
        snap_in_progress = false;
    }

    void clear_snap() {
        snap_result.reset();
        snap_in_progress = false;
    }

    /**
     * Get the effective rect for processing.
     * Uses snap result if available, otherwise uses user's region.
     */
    [[nodiscard]] cv::Rect effective_rect() const noexcept {
        if (snap_result && snap_result->snap_found) {
            return snap_result->snapped_rect;
        }
        return region;
    }

    [[nodiscard]] int width() const noexcept { return region.width; }
    [[nodiscard]] int height() const noexcept { return region.height; }
};

// =============================================================================
// Watermark Info
// =============================================================================

/**
 * Detected watermark information
 */
struct WatermarkInfo {
    WatermarkSize size{WatermarkSize::Small};
    cv::Point position{0, 0};       // Top-left corner
    cv::Rect region{0, 0, 0, 0};    // Full watermark region
    bool is_custom{false};           // Whether this is a custom region

    [[nodiscard]] int width() const noexcept {
        if (is_custom) return region.width;
        return (size == WatermarkSize::Small) ? 48 : 96;
    }

    [[nodiscard]] int height() const noexcept {
        if (is_custom) return region.height;
        return width();  // Square for standard sizes
    }
};

// =============================================================================
// Image State
// =============================================================================

/**
 * Current image state
 */
struct ImageState {
    std::optional<std::filesystem::path> file_path;
    cv::Mat original;       // Original loaded image
    cv::Mat processed;      // After watermark processing
    cv::Mat display;        // Currently displayed (original or processed)

    int width{0};
    int height{0};
    int channels{0};

    bool has_image() const noexcept { return !original.empty(); }
    bool has_processed() const noexcept { return !processed.empty(); }

    void clear() {
        file_path.reset();
        original.release();
        processed.release();
        display.release();
        width = height = channels = 0;
    }
};

// =============================================================================
// Processing Options
// =============================================================================

/**
 * Inpaint cleanup options (applied after reverse alpha blend)
 */
struct InpaintOptions {
    bool enabled{true};                              // Enable inpaint cleanup
    float strength{0.85f};                           // Blend: 0=reverse-only, 1=full effect
    InpaintMethod method{InpaintMethod::NS};          // Cleanup method (NS default)
    int inpaint_radius{10};                           // Blur/inpaint radius

    static constexpr int kPadding = 32;              // Context padding (covers weight spread)
};

/**
 * Processing options
 */
struct ProcessOptions {
    bool remove_mode{true};     // true = remove, false = add
    WatermarkSizeMode size_mode{WatermarkSizeMode::Auto};
    std::optional<WatermarkSize> force_size;  // Override auto-detection (for Auto/Small/Large)
    std::optional<cv::Rect> custom_region;    // Custom watermark region
    InpaintOptions inpaint;                   // Inpaint cleanup options
};

// =============================================================================
// Preview Options
// =============================================================================

/**
 * Preview display options
 */
struct PreviewOptions {
    bool show_processed{false};     // Show processed instead of original
    bool highlight_watermark{true}; // Draw box around watermark region
    bool split_view{false};         // Side-by-side comparison

    float zoom{1.0f};               // Zoom level (1.0 = fit to window)
    float pan_x{0.0f};              // Pan offset X
    float pan_y{0.0f};              // Pan offset Y

    void reset_view() {
        zoom = 1.0f;
        pan_x = pan_y = 0.0f;
    }
};

// =============================================================================
// Batch Processing State
// =============================================================================

/**
 * Status of a single file in batch processing
 */
enum class BatchFileStatus {
    Pending,    // Not yet processed
    Processing, // Currently being processed
    OK,         // Successfully processed
    Skipped,    // Skipped (no watermark detected)
    Failed      // Processing failed
};

/**
 * Result for a single file in batch
 */
struct BatchFileResult {
    std::filesystem::path path;
    BatchFileStatus status{BatchFileStatus::Pending};
    float confidence{0.0f};         // Detection confidence
    std::string message;            // Status message
};

/**
 * Batch processing state
 */
struct BatchState {
    // File list and per-file results
    std::vector<BatchFileResult> files;

    // Processing state
    size_t current_index{0};
    size_t success_count{0};
    size_t skip_count{0};
    size_t fail_count{0};
    bool in_progress{false};
    bool cancel_requested{false};

    // Detection settings
    float detection_threshold{batch_theme::kDefaultThreshold};
    bool use_detection{true};           // Enable watermark detection

    // Thumbnail atlas
    TextureHandle thumbnail_texture;    // Combined atlas texture
    int thumbnail_cols{0};              // Grid columns (set during generation)
    int thumbnail_rows{0};              // Grid rows (set during generation)
    int thumbnail_cell_size{batch_theme::kThumbnailCellSize};
    bool thumbnails_ready{false};       // Atlas has been generated

    // Confirmation dialog
    bool show_confirm_dialog{false};    // Show "overwrite files?" dialog

    void clear() {
        files.clear();
        current_index = success_count = skip_count = fail_count = 0;
        in_progress = cancel_requested = false;
        thumbnails_ready = false;
        show_confirm_dialog = false;
        // Note: detection_threshold and use_detection are NOT reset
        // Note: thumbnail_texture must be destroyed externally before clear
    }

    [[nodiscard]] float progress() const noexcept {
        if (files.empty()) return 0.0f;
        return static_cast<float>(current_index) / static_cast<float>(files.size());
    }

    [[nodiscard]] size_t total() const noexcept { return files.size(); }
    [[nodiscard]] bool is_batch_mode() const noexcept { return !files.empty(); }
    [[nodiscard]] bool is_complete() const noexcept {
        return !files.empty() && !in_progress && current_index >= files.size();
    }
};

// =============================================================================
// Main Application State
// =============================================================================

/**
 * Complete application state
 * Shared by all GUI components
 */
struct AppState {
    // Current state
    ProcessState state{ProcessState::Idle};
    std::string status_message{"Ready"};
    std::string error_message;

    // Image
    ImageState image;
    std::optional<WatermarkInfo> watermark_info;

    // Custom watermark mode state
    CustomWatermarkState custom_watermark;

    // Options
    ProcessOptions process_options;
    PreviewOptions preview_options;

    // Batch
    BatchState batch;

    // Texture handle for preview
    TextureHandle preview_texture;
    bool texture_needs_update{false};

    // UI state
    bool show_about_dialog{false};
    bool show_settings_dialog{false};

    // Display scaling
    float dpi_scale{1.0f};

    /**
     * Scale a pixel value by DPI
     */
    [[nodiscard]] float scaled(float pixels) const noexcept {
        return pixels * dpi_scale;
    }

    /**
     * Reset to initial state
     */
    void reset() {
        state = ProcessState::Idle;
        status_message = "Ready";
        error_message.clear();

        image.clear();
        watermark_info.reset();
        custom_watermark.clear();

        preview_options.reset_view();
        preview_options.show_processed = false;

        batch.clear();

        texture_needs_update = true;
        // Note: dpi_scale and process_options are not reset
    }

    /**
     * Check if ready to process
     */
    [[nodiscard]] bool can_process() const noexcept {
        // Batch mode: can process if files are queued and not already processing
        if (batch.is_batch_mode() && !batch.in_progress) return true;
        // Single mode
        return state == ProcessState::Loaded || state == ProcessState::Completed;
    }

    /**
     * Check if can save
     */
    [[nodiscard]] bool can_save() const noexcept {
        return state == ProcessState::Completed && image.has_processed();
    }
};

}  // namespace gwt::gui
