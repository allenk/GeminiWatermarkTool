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
    
    [[nodiscard]] int width() const noexcept {
        return (size == WatermarkSize::Small) ? 48 : 96;
    }
    
    [[nodiscard]] int height() const noexcept {
        return width();  // Square
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
 * Processing options
 */
struct ProcessOptions {
    bool remove_mode{true};     // true = remove, false = add
    std::optional<WatermarkSize> force_size;  // Override auto-detection
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
 * Batch processing state
 */
struct BatchState {
    std::vector<std::filesystem::path> files;
    std::optional<std::filesystem::path> output_dir;
    size_t current_index{0};
    size_t success_count{0};
    size_t fail_count{0};
    bool in_progress{false};
    bool cancel_requested{false};
    
    void clear() {
        files.clear();
        output_dir.reset();
        current_index = success_count = fail_count = 0;
        in_progress = cancel_requested = false;
    }
    
    [[nodiscard]] float progress() const noexcept {
        if (files.empty()) return 0.0f;
        return static_cast<float>(current_index) / static_cast<float>(files.size());
    }
    
    [[nodiscard]] size_t total() const noexcept { return files.size(); }
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
        
        preview_options.reset_view();
        preview_options.show_processed = false;
        
        batch.clear();
        
        texture_needs_update = true;
        // Note: dpi_scale is not reset
    }
    
    /**
     * Check if ready to process
     */
    [[nodiscard]] bool can_process() const noexcept {
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
