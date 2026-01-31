/**
 * @file    app_controller.hpp
 * @brief   Application Controller
 * @author  AllenK (Kwyshell)
 * @license MIT
 *
 * @details
 * Coordinates between View layer and Core layer.
 * Handles all user actions and state management.
 */

#pragma once

#include "gui/app/app_state.hpp"
#include "gui/backend/render_backend.hpp"
#include "core/watermark_engine.hpp"

#include <memory>
#include <filesystem>
#include <span>

namespace gwt::gui {

class AppController {
public:
    /**
     * Construct controller with render backend
     * @param backend  Reference to render backend (must outlive controller)
     */
    explicit AppController(IRenderBackend& backend);
    
    ~AppController();
    
    // Non-copyable, non-movable (holds reference to backend)
    AppController(const AppController&) = delete;
    AppController& operator=(const AppController&) = delete;
    AppController(AppController&&) = delete;
    AppController& operator=(AppController&&) = delete;

    // ==========================================================================
    // State Access
    // ==========================================================================
    
    /**
     * Get current application state (read-only)
     */
    [[nodiscard]] const AppState& state() const noexcept { return m_state; }
    
    /**
     * Get mutable state (use sparingly, prefer commands)
     */
    [[nodiscard]] AppState& state() noexcept { return m_state; }

    // ==========================================================================
    // Image Operations
    // ==========================================================================
    
    /**
     * Load an image from file
     * @param path  Path to image file
     * @return      true if successful
     */
    bool load_image(const std::filesystem::path& path);
    
    /**
     * Save processed image to file
     * @param path  Output path
     * @return      true if successful
     */
    bool save_image(const std::filesystem::path& path);
    
    /**
     * Close current image and reset state
     */
    void close_image();

    // ==========================================================================
    // Processing Operations
    // ==========================================================================
    
    /**
     * Process current image (remove or add watermark)
     * Uses current process_options
     */
    void process_current();
    
    /**
     * Revert to original image
     */
    void revert_to_original();

    // ==========================================================================
    // Options
    // ==========================================================================
    
    /**
     * Set remove/add mode
     * @param remove  true = remove watermark, false = add watermark
     */
    void set_remove_mode(bool remove);
    
    /**
     * Set forced watermark size
     * @param size  Size to force, or nullopt for auto-detection
     */
    void set_force_size(std::optional<WatermarkSize> size);
    
    /**
     * Toggle between original and processed preview
     */
    void toggle_preview();

    // ==========================================================================
    // Batch Operations
    // ==========================================================================
    
    /**
     * Add files to batch queue
     * @param files  Paths to add
     */
    void add_batch_files(std::span<const std::filesystem::path> files);
    
    /**
     * Set output directory for batch processing
     * @param dir  Output directory path
     */
    void set_batch_output_dir(const std::filesystem::path& dir);
    
    /**
     * Start batch processing
     */
    void start_batch_processing();
    
    /**
     * Process next file in batch (call from main loop)
     * @return  true if more files to process
     */
    bool process_batch_next();
    
    /**
     * Cancel batch processing
     */
    void cancel_batch();
    
    /**
     * Clear batch queue
     */
    void clear_batch();

    // ==========================================================================
    // Texture Management
    // ==========================================================================
    
    /**
     * Update preview texture if needed
     * Call this once per frame before rendering
     */
    void update_texture_if_needed();
    
    /**
     * Force texture update
     */
    void invalidate_texture();
    
    /**
     * Get ImGui texture ID for preview
     */
    [[nodiscard]] void* get_preview_texture_id() const;

    // ==========================================================================
    // Utility
    // ==========================================================================
    
    /**
     * Get supported image file extensions
     */
    [[nodiscard]] static std::vector<std::string> supported_extensions();
    
    /**
     * Check if file extension is supported
     */
    [[nodiscard]] static bool is_supported_extension(const std::filesystem::path& path);

private:
    AppState m_state;
    IRenderBackend& m_backend;
    std::unique_ptr<WatermarkEngine> m_engine;
    
    // Internal helpers
    void update_watermark_info();
    void update_display_image();
    void create_or_update_texture();
    cv::Mat prepare_texture_data(const cv::Mat& image);
};

}  // namespace gwt::gui
