/**
 * @file    app_controller.cpp
 * @brief   Application Controller Implementation
 * @author  AllenK (Kwyshell)
 * @license MIT
 */

#include "gui/app/app_controller.hpp"
#include "embedded_assets.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <spdlog/spdlog.h>
#include <fmt/core.h>

#include <algorithm>

namespace gwt::gui {

// =============================================================================
// Construction / Destruction
// =============================================================================

AppController::AppController(IRenderBackend& backend)
    : m_backend(backend)
{
    // Initialize watermark engine with embedded assets
    m_engine = std::make_unique<WatermarkEngine>(
        embedded::bg_48_png, embedded::bg_48_png_size,
        embedded::bg_96_png, embedded::bg_96_png_size
    );

    spdlog::debug("AppController initialized");
}

AppController::~AppController() {
    // Destroy texture if exists
    if (m_state.preview_texture.valid()) {
        m_backend.destroy_texture(m_state.preview_texture);
    }
}

// =============================================================================
// Image Operations
// =============================================================================

bool AppController::load_image(const std::filesystem::path& path) {
    spdlog::info("Loading image: {}", path.string());

    // Read image first to validate
    cv::Mat image = cv::imread(path.string(), cv::IMREAD_COLOR);
    if (image.empty()) {
        m_state.state = ProcessState::Error;
        m_state.error_message = "Failed to load image: " + path.string();
        m_state.status_message = "Load failed";
        spdlog::error("{}", m_state.error_message);
        return false;
    }

    // Clean up old state completely (including texture)
    if (m_state.preview_texture.valid()) {
        m_backend.destroy_texture(m_state.preview_texture);
        m_state.preview_texture = TextureHandle{};
    }
    m_state.reset();

    // Update state with new image
    m_state.image.file_path = path;
    m_state.image.original = image;
    m_state.image.width = image.cols;
    m_state.image.height = image.rows;
    m_state.image.channels = image.channels();

    // Detect watermark info
    update_watermark_info();

    // Update display
    update_display_image();

    // Update state
    m_state.state = ProcessState::Loaded;
    m_state.status_message = fmt::format("Loaded: {}x{}", image.cols, image.rows);
    m_state.error_message.clear();

    spdlog::info("Image loaded: {}x{} ({} channels)",
                 image.cols, image.rows, image.channels());

    return true;
}

bool AppController::save_image(const std::filesystem::path& path) {
    if (!m_state.can_save()) {
        spdlog::warn("No processed image to save");
        return false;
    }

    spdlog::info("Saving image: {}", path.string());

    // Determine output format and quality
    std::vector<int> params;
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".jpg" || ext == ".jpeg") {
        params = {cv::IMWRITE_JPEG_QUALITY, 100};
    } else if (ext == ".png") {
        params = {cv::IMWRITE_PNG_COMPRESSION, 6};
    } else if (ext == ".webp") {
        params = {cv::IMWRITE_WEBP_QUALITY, 101};  // Lossless
    }

    // Create output directory if needed
    auto output_dir = path.parent_path();
    if (!output_dir.empty() && !std::filesystem::exists(output_dir)) {
        std::filesystem::create_directories(output_dir);
    }

    // Write image
    bool success = cv::imwrite(path.string(), m_state.image.processed, params);

    if (success) {
        m_state.status_message = "Saved: " + path.filename().string();
        spdlog::info("Image saved: {}", path.string());
    } else {
        m_state.error_message = "Failed to save: " + path.string();
        m_state.status_message = "Save failed";
        spdlog::error("{}", m_state.error_message);
    }

    return success;
}

void AppController::close_image() {
    // Destroy texture
    if (m_state.preview_texture.valid()) {
        m_backend.destroy_texture(m_state.preview_texture);
        m_state.preview_texture = TextureHandle{};
    }

    m_state.reset();
    spdlog::debug("Image closed");
}

// =============================================================================
// Processing Operations
// =============================================================================

void AppController::process_current() {
    if (!m_state.image.has_image()) {
        spdlog::warn("No image to process");
        return;
    }

    m_state.state = ProcessState::Processing;
    m_state.status_message = "Processing...";

    try {
        // Clone original for processing
        m_state.image.processed = m_state.image.original.clone();

        // Apply watermark operation
        if (m_state.process_options.remove_mode) {
            m_engine->remove_watermark(
                m_state.image.processed,
                m_state.process_options.force_size
            );
            spdlog::info("Watermark removed");
        } else {
            m_engine->add_watermark(
                m_state.image.processed,
                m_state.process_options.force_size
            );
            spdlog::info("Watermark added");
        }

        // Show processed result
        m_state.preview_options.show_processed = true;
        update_display_image();

        m_state.state = ProcessState::Completed;
        m_state.status_message = m_state.process_options.remove_mode
            ? "Watermark removed"
            : "Watermark added";
        m_state.error_message.clear();

    } catch (const std::exception& e) {
        m_state.state = ProcessState::Error;
        m_state.error_message = e.what();
        m_state.status_message = "Processing failed";
        spdlog::error("Processing failed: {}", e.what());
    }
}

void AppController::revert_to_original() {
    if (!m_state.image.has_image()) return;

    m_state.preview_options.show_processed = false;
    update_display_image();

    m_state.status_message = "Reverted to original";
}

// =============================================================================
// Options
// =============================================================================

void AppController::set_remove_mode(bool remove) {
    m_state.process_options.remove_mode = remove;
    spdlog::debug("Mode set to: {}", remove ? "Remove" : "Add");
}

void AppController::set_force_size(std::optional<WatermarkSize> size) {
    m_state.process_options.force_size = size;

    if (size) {
        spdlog::debug("Force size: {}",
                      *size == WatermarkSize::Small ? "48x48" : "96x96");
    } else {
        spdlog::debug("Force size: Auto");
    }

    // Re-detect watermark info if image loaded
    if (m_state.image.has_image()) {
        update_watermark_info();
    }
}

void AppController::toggle_preview() {
    if (!m_state.image.has_image()) return;

    // Can only toggle if we have processed image
    if (m_state.image.has_processed()) {
        m_state.preview_options.show_processed = !m_state.preview_options.show_processed;
        update_display_image();
    }
}

// =============================================================================
// Batch Operations
// =============================================================================

void AppController::add_batch_files(std::span<const std::filesystem::path> files) {
    for (const auto& file : files) {
        if (is_supported_extension(file) && std::filesystem::is_regular_file(file)) {
            m_state.batch.files.push_back(file);
        }
    }
    spdlog::info("Batch queue: {} files", m_state.batch.files.size());
}

void AppController::set_batch_output_dir(const std::filesystem::path& dir) {
    m_state.batch.output_dir = dir;
}

void AppController::start_batch_processing() {
    if (m_state.batch.files.empty()) {
        spdlog::warn("No files in batch queue");
        return;
    }

    m_state.batch.current_index = 0;
    m_state.batch.success_count = 0;
    m_state.batch.fail_count = 0;
    m_state.batch.in_progress = true;
    m_state.batch.cancel_requested = false;

    spdlog::info("Starting batch processing: {} files", m_state.batch.files.size());
}

bool AppController::process_batch_next() {
    if (!m_state.batch.in_progress) return false;
    if (m_state.batch.cancel_requested) {
        m_state.batch.in_progress = false;
        m_state.status_message = "Batch cancelled";
        return false;
    }
    if (m_state.batch.current_index >= m_state.batch.files.size()) {
        m_state.batch.in_progress = false;
        m_state.status_message = fmt::format("Batch complete: {} ok, {} failed",
                                              m_state.batch.success_count,
                                              m_state.batch.fail_count);
        return false;
    }

    const auto& input = m_state.batch.files[m_state.batch.current_index];

    // Determine output path
    std::filesystem::path output;
    if (m_state.batch.output_dir) {
        output = *m_state.batch.output_dir / input.filename();
    } else {
        output = input;  // In-place
    }

    // Process
    bool success = process_image(
        input, output,
        m_state.process_options.remove_mode,
        *m_engine,
        m_state.process_options.force_size
    );

    if (success) {
        m_state.batch.success_count++;
    } else {
        m_state.batch.fail_count++;
    }

    m_state.batch.current_index++;
    m_state.status_message = fmt::format("Batch: {}/{}",
                                          m_state.batch.current_index,
                                          m_state.batch.files.size());

    return m_state.batch.current_index < m_state.batch.files.size();
}

void AppController::cancel_batch() {
    m_state.batch.cancel_requested = true;
}

void AppController::clear_batch() {
    m_state.batch.clear();
}

// =============================================================================
// Texture Management
// =============================================================================

void AppController::update_texture_if_needed() {
    if (!m_state.texture_needs_update) return;
    if (m_state.image.display.empty()) return;

    create_or_update_texture();
    m_state.texture_needs_update = false;
}

void AppController::invalidate_texture() {
    m_state.texture_needs_update = true;
}

void* AppController::get_preview_texture_id() const {
    return m_backend.get_imgui_texture_id(m_state.preview_texture);
}

// =============================================================================
// Utility
// =============================================================================

std::vector<std::string> AppController::supported_extensions() {
    return {".jpg", ".jpeg", ".png", ".webp", ".bmp"};
}

bool AppController::is_supported_extension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    static const std::vector<std::string> supported = supported_extensions();
    return std::find(supported.begin(), supported.end(), ext) != supported.end();
}

// =============================================================================
// Internal Helpers
// =============================================================================

void AppController::update_watermark_info() {
    if (!m_state.image.has_image()) {
        m_state.watermark_info.reset();
        return;
    }

    int w = m_state.image.width;
    int h = m_state.image.height;

    // Use forced size or auto-detect
    WatermarkSize size = m_state.process_options.force_size.value_or(
        get_watermark_size(w, h)
    );

    WatermarkPosition config = get_watermark_config(w, h);
    if (m_state.process_options.force_size) {
        // Override config based on forced size
        if (size == WatermarkSize::Small) {
            config = WatermarkPosition{32, 32, 48};
        } else {
            config = WatermarkPosition{64, 64, 96};
        }
    }

    cv::Point pos = config.get_position(w, h);

    WatermarkInfo info;
    info.size = size;
    info.position = pos;
    info.region = cv::Rect(pos.x, pos.y, config.logo_size, config.logo_size);

    m_state.watermark_info = info;

    spdlog::debug("Watermark info: {}x{} at ({}, {})",
                  config.logo_size, config.logo_size, pos.x, pos.y);
}

void AppController::update_display_image() {
    if (!m_state.image.has_image()) {
        m_state.image.display.release();
        m_state.texture_needs_update = true;
        return;
    }

    if (m_state.preview_options.show_processed && m_state.image.has_processed()) {
        m_state.image.display = m_state.image.processed;
    } else {
        m_state.image.display = m_state.image.original;
    }

    m_state.texture_needs_update = true;
}

void AppController::create_or_update_texture() {
    if (m_state.image.display.empty()) return;

    // Prepare texture data (BGR -> RGBA)
    cv::Mat rgba = prepare_texture_data(m_state.image.display);

    // Create texture description
    TextureDesc desc;
    desc.width = rgba.cols;
    desc.height = rgba.rows;
    desc.format = TextureFormat::RGBA8;

    // Create span from cv::Mat data
    std::span<const uint8_t> data(rgba.data, rgba.total() * rgba.elemSize());

    if (!m_state.preview_texture.valid()) {
        // Create new texture
        TextureHandle result = m_backend.create_texture(desc, data);
        if (result.valid()) {
            m_state.preview_texture = result;
        } else {
            spdlog::error("Failed to create texture: {}", to_string(m_backend.last_error()));
        }
    } else {
        // Update existing texture
        m_backend.update_texture(m_state.preview_texture, data);
    }
}

cv::Mat AppController::prepare_texture_data(const cv::Mat& image) {
    cv::Mat rgba;

    if (image.channels() == 3) {
        cv::cvtColor(image, rgba, cv::COLOR_BGR2RGBA);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, rgba, cv::COLOR_BGRA2RGBA);
    } else if (image.channels() == 1) {
        cv::cvtColor(image, rgba, cv::COLOR_GRAY2RGBA);
    } else {
        rgba = image.clone();
    }

    return rgba;
}

}  // namespace gwt::gui
