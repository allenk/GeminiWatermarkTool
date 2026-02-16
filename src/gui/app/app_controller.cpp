/**
 * @file    app_controller.cpp
 * @brief   Application Controller Implementation
 * @author  AllenK (Kwyshell)
 * @license MIT
 */

#include "gui/app/app_controller.hpp"
#include "core/watermark_detector.hpp"
#include "embedded_assets.hpp"
#include "utils/path_formatter.hpp"

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
    // Destroy textures
    if (m_state.preview_texture.valid()) {
        m_backend.destroy_texture(m_state.preview_texture);
    }
    if (m_state.batch.thumbnail_texture.valid()) {
        m_backend.destroy_texture(m_state.batch.thumbnail_texture);
    }
}

// =============================================================================
// Image Operations
// =============================================================================

bool AppController::load_image(const std::filesystem::path& path) {
    spdlog::info("Loading image: {}", path);

    // Read image first to validate
    cv::Mat image = cv::imread(path.string(), cv::IMREAD_COLOR);
    if (image.empty()) {
        m_state.state = ProcessState::Error;
        m_state.error_message = "Failed to load image: " + to_utf8(path);
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


    // Run auto-detection when entering custom mode
    if (m_state.process_options.size_mode == WatermarkSizeMode::Custom && m_state.image.has_image()) {
        detect_custom_watermark();
    }

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
        spdlog::warn("No image to save");
        return false;
    }

    spdlog::info("Saving image: {}", path);

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

    // Write currently displayed image (WYSIWYG - What You See Is What You Get)
    bool success = cv::imwrite(path.string(), m_state.image.display, params);

    if (success) {
        m_state.status_message = "Saved: " + filename_utf8(path);
        spdlog::info("Image saved: {}", path);
    } else {
        m_state.error_message = "Failed to save: " + to_utf8(path);
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

        bool is_custom = (m_state.process_options.size_mode == WatermarkSizeMode::Custom) &&
                         m_state.custom_watermark.has_region;

        // For custom mode, use the effective rect (snap result if available,
        // otherwise the user's drawn region)
        cv::Rect effective_region = m_state.custom_watermark.effective_rect();

        // Apply watermark operation
        if (m_state.process_options.remove_mode) {
            if (is_custom) {
                m_engine->remove_watermark_custom(
                    m_state.image.processed,
                    effective_region
                );
                spdlog::info("Watermark removed (custom region: ({},{}) {}x{})",
                             effective_region.x, effective_region.y,
                             effective_region.width, effective_region.height);
            } else {
                m_engine->remove_watermark(
                    m_state.image.processed,
                    m_state.process_options.force_size
                );
                spdlog::info("Watermark removed");
            }
        } else {
            if (is_custom) {
                m_engine->add_watermark_custom(
                    m_state.image.processed,
                    effective_region
                );
                spdlog::info("Watermark added (custom region: ({},{}) {}x{})",
                             effective_region.x, effective_region.y,
                             effective_region.width, effective_region.height);
            } else {
                m_engine->add_watermark(
                    m_state.image.processed,
                    m_state.process_options.force_size
                );
                spdlog::info("Watermark added");
            }
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
        m_state.process_options.size_mode = (*size == WatermarkSize::Small)
            ? WatermarkSizeMode::Small
            : WatermarkSizeMode::Large;
        spdlog::debug("Force size: {}",
                      *size == WatermarkSize::Small ? "48x48" : "96x96");
    } else {
        m_state.process_options.size_mode = WatermarkSizeMode::Auto;
        spdlog::debug("Force size: Auto");
    }

    // Clear custom state when switching away from custom
    m_state.custom_watermark.clear();

    // Re-detect watermark info if image loaded
    if (m_state.image.has_image()) {
        update_watermark_info();
    }
}

void AppController::set_size_mode(WatermarkSizeMode mode) {
    m_state.process_options.size_mode = mode;

    switch (mode) {
        case WatermarkSizeMode::Auto:
            m_state.process_options.force_size = std::nullopt;
            m_state.custom_watermark.clear();
            break;
        case WatermarkSizeMode::Small:
            m_state.process_options.force_size = WatermarkSize::Small;
            m_state.custom_watermark.clear();
            break;
        case WatermarkSizeMode::Large:
            m_state.process_options.force_size = WatermarkSize::Large;
            m_state.custom_watermark.clear();
            break;
        case WatermarkSizeMode::Custom:
            m_state.process_options.force_size = std::nullopt;
            // Run auto-detection when entering custom mode
            if (m_state.image.has_image() && !m_state.custom_watermark.detection_attempted) {
                detect_custom_watermark();
            }
            break;
    }

    // Re-detect watermark info
    if (m_state.image.has_image()) {
        update_watermark_info();
    }

    spdlog::debug("Size mode set to: {}", static_cast<int>(mode));
}

void AppController::set_custom_region(const cv::Rect& region) {
    // Clamp to image bounds
    cv::Rect clamped = region & cv::Rect(0, 0, m_state.image.width, m_state.image.height);

    if (clamped.width < 4 || clamped.height < 4) {
        spdlog::warn("Custom region too small: {}x{}", clamped.width, clamped.height);
        return;
    }

    m_state.custom_watermark.region = clamped;
    m_state.custom_watermark.has_region = true;
    m_state.process_options.custom_region = clamped;

    // Clear previous snap result when region changes
    m_state.custom_watermark.clear_snap();

    // Update watermark info to reflect custom region
    update_watermark_info();
}

void AppController::detect_custom_watermark() {
    if (!m_state.image.has_image()) return;

    m_state.custom_watermark.detection_attempted = true;
    m_state.custom_watermark.clear_snap();  // Clear previous snap
    m_state.status_message = "Detecting watermark...";

    // Run detection
    auto result = detect_watermark_region(m_state.image.original);

    if (result && result->detected) {
        m_state.custom_watermark.region = result->region;
        m_state.custom_watermark.has_region = true;
        m_state.custom_watermark.detection_confidence = result->confidence;
        m_state.process_options.custom_region = result->region;

        m_state.status_message = fmt::format("Detected watermark ({:.0f}% confidence)",
                                              result->confidence * 100.0f);

        spdlog::info("Auto-detected watermark: ({},{}) {}x{} confidence={:.2f} "
                     "(spatial={:.2f}, grad={:.2f}, var={:.2f})",
                     result->region.x, result->region.y,
                     result->region.width, result->region.height,
                     result->confidence,
                     result->spatial_score, result->gradient_score, result->variance_score);
    } else {
        // Fallback to default position (detection failed or low confidence)
        cv::Rect fallback = get_fallback_watermark_region(
            m_state.image.width, m_state.image.height);

        m_state.custom_watermark.region = fallback;
        m_state.custom_watermark.has_region = true;
        m_state.custom_watermark.detection_confidence = result ? result->confidence : 0.0f;
        m_state.process_options.custom_region = fallback;

        if (result) {
            m_state.status_message = fmt::format("No watermark detected ({:.0f}%), using default",
                                                  result->confidence * 100.0f);
        } else {
            m_state.status_message = "Detection failed, using default position";
        }
        spdlog::info("Detection: not found, using fallback: ({},{}) {}x{}",
                     fallback.x, fallback.y, fallback.width, fallback.height);
    }

    update_watermark_info();
}

void AppController::snap_to_watermark() {
    if (!m_state.image.has_image()) return;
    if (!m_state.custom_watermark.has_region) return;
    if (!m_engine) return;

    auto& cw = m_state.custom_watermark;
    const cv::Rect& search_rect = cw.region;

    // Warn about large search regions
    int area = search_rect.width * search_rect.height;
    if (area > 600 * 600) {
        m_state.status_message = "Very large region. Consider narrowing selection.";
        spdlog::warn("Snap: search region {}x{} is very large", search_rect.width, search_rect.height);
    } else if (area > 300 * 300) {
        m_state.status_message = "Searching... (large region)";
    } else {
        m_state.status_message = "Searching...";
    }

    cw.snap_in_progress = true;

    // Determine scale range based on image size
    // Standard sizes: 48 for <=1024, 96 for >1024
    // Allow generous range for resized images
    int min_size = 32;
    int max_size = std::min(160, std::min(search_rect.width, search_rect.height));

    // Run guided detection
    GuidedDetectionResult det = m_engine->guided_detect(
        m_state.image.original, search_rect, nullptr, min_size, max_size);

    cw.snap_in_progress = false;

    if (det.found) {
        SnapResult snap;
        snap.snapped_rect = det.match_rect;
        snap.snap_confidence = det.confidence;
        snap.snap_found = true;
        snap.detected_size = det.detected_size;
        cw.snap_result = snap;

        // Also update the process options to use snap result
        m_state.process_options.custom_region = det.match_rect;

        m_state.status_message = fmt::format(
            "Snapped to ({},{}) {}x{} | Confidence: {:.0f}%",
            det.match_rect.x, det.match_rect.y,
            det.detected_size, det.detected_size,
            det.confidence * 100.0f);

        spdlog::info("Snap successful: ({},{}) {}x{} adjusted={:.2f} raw_ncc={:.2f}",
                     det.match_rect.x, det.match_rect.y,
                     det.detected_size, det.detected_size,
                     det.confidence, det.raw_ncc);
    } else {
        cw.snap_result = SnapResult{};  // Empty result, snap_found = false

        if (det.was_cancelled) {
            m_state.status_message = "Search cancelled";
        } else {
            m_state.status_message = fmt::format(
                "No watermark found in region ({} scales searched)",
                det.scales_searched);
        }

        spdlog::info("Snap: no match found (scales_searched={}, cancelled={})",
                     det.scales_searched, det.was_cancelled);
    }

    update_watermark_info();
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

void AppController::enter_batch_mode(std::span<const std::filesystem::path> files) {
    // Clean up previous batch thumbnail
    if (m_state.batch.thumbnail_texture.valid()) {
        m_backend.destroy_texture(m_state.batch.thumbnail_texture);
        m_state.batch.thumbnail_texture = TextureHandle{};
    }
    m_state.batch.clear();

    // Filter supported files
    for (const auto& file : files) {
        if (is_supported_extension(file) && std::filesystem::is_regular_file(file)) {
            BatchFileResult result;
            result.path = file;
            result.status = BatchFileStatus::Pending;
            m_state.batch.files.push_back(std::move(result));
        }
    }

    if (m_state.batch.files.empty()) {
        spdlog::warn("No supported image files found in dropped files");
        return;
    }

    spdlog::info("Entering batch mode: {} files", m_state.batch.files.size());

    // Default to Auto Detect in batch mode
    m_state.process_options.size_mode = WatermarkSizeMode::Auto;
    m_state.process_options.force_size = std::nullopt;

    // Clear single-image state (batch replaces it)
    if (m_state.preview_texture.valid()) {
        m_backend.destroy_texture(m_state.preview_texture);
        m_state.preview_texture = TextureHandle{};
    }
    m_state.image.clear();
    m_state.custom_watermark.clear();
    m_state.watermark_info.reset();
    m_state.state = ProcessState::Idle;

    // Generate thumbnail atlas
    generate_thumbnail_atlas();

    m_state.status_message = fmt::format("Batch: {} files ready", m_state.batch.files.size());
}

void AppController::exit_batch_mode() {
    // Destroy batch thumbnail texture
    if (m_state.batch.thumbnail_texture.valid()) {
        m_backend.destroy_texture(m_state.batch.thumbnail_texture);
        m_state.batch.thumbnail_texture = TextureHandle{};
    }
    m_state.batch.clear();
    m_state.status_message = "Ready";
    spdlog::info("Exited batch mode");
}

void AppController::start_batch_processing() {
    if (m_state.batch.files.empty()) {
        spdlog::warn("No files in batch queue");
        return;
    }

    m_state.batch.current_index = 0;
    m_state.batch.success_count = 0;
    m_state.batch.skip_count = 0;
    m_state.batch.fail_count = 0;
    m_state.batch.in_progress = true;
    m_state.batch.cancel_requested = false;

    // Reset all file statuses
    for (auto& f : m_state.batch.files) {
        f.status = BatchFileStatus::Pending;
        f.confidence = 0.0f;
        f.message.clear();
    }

    spdlog::info("Starting batch processing: {} files (threshold: {:.0f}%)",
                 m_state.batch.files.size(),
                 m_state.batch.detection_threshold * 100.0f);
}

bool AppController::process_batch_next() {
    if (!m_state.batch.in_progress) return false;
    if (m_state.batch.cancel_requested) {
        m_state.batch.in_progress = false;
        m_state.status_message = fmt::format("Batch cancelled ({}/{})",
                                              m_state.batch.current_index,
                                              m_state.batch.files.size());
        return false;
    }
    if (m_state.batch.current_index >= m_state.batch.files.size()) {
        m_state.batch.in_progress = false;
        m_state.status_message = fmt::format("Batch complete: {} ok, {} skipped, {} failed",
                                              m_state.batch.success_count,
                                              m_state.batch.skip_count,
                                              m_state.batch.fail_count);
        spdlog::info("{}", m_state.status_message);

        // Regenerate thumbnail atlas to show processed results
        generate_thumbnail_atlas();

        return false;
    }

    auto& file_result = m_state.batch.files[m_state.batch.current_index];
    file_result.status = BatchFileStatus::Processing;

    const auto& input = file_result.path;

    // Output = overwrite original (same as CLI simple mode)
    std::filesystem::path output = input;

    // Process using core process_image with detection
    auto proc_result = process_image(
        input, output,
        m_state.process_options.remove_mode,
        *m_engine,
        m_state.process_options.force_size,
        m_state.batch.use_detection,
        m_state.batch.detection_threshold
    );

    file_result.confidence = proc_result.confidence;
    file_result.message = proc_result.message;

    if (proc_result.skipped) {
        file_result.status = BatchFileStatus::Skipped;
        m_state.batch.skip_count++;
    } else if (proc_result.success) {
        file_result.status = BatchFileStatus::OK;
        m_state.batch.success_count++;
    } else {
        file_result.status = BatchFileStatus::Failed;
        m_state.batch.fail_count++;
    }

    m_state.batch.current_index++;
    m_state.status_message = fmt::format("Batch: {}/{} (OK:{} Skip:{} Fail:{})",
                                          m_state.batch.current_index,
                                          m_state.batch.files.size(),
                                          m_state.batch.success_count,
                                          m_state.batch.skip_count,
                                          m_state.batch.fail_count);

    return m_state.batch.current_index < m_state.batch.files.size();
}

void AppController::cancel_batch() {
    m_state.batch.cancel_requested = true;
}

// =============================================================================
// Batch Helpers
// =============================================================================

void AppController::generate_thumbnail_atlas() {
    using namespace batch_theme;

    auto& batch = m_state.batch;
    if (batch.files.empty()) return;

    const int cell_size   = kThumbnailCellSize;
    const int label_h     = kLabelHeight;
    const int pad         = kCellPadding;
    const int gap_v       = kCellGapV;
    const int thumb_h     = cell_size - label_h;
    int count = static_cast<int>(std::min(batch.files.size(),
                                          static_cast<size_t>(kThumbnailMaxCount)));

    // Grid layout
    batch.thumbnail_cols = kThumbnailCols;
    batch.thumbnail_rows = (count + batch.thumbnail_cols - 1) / batch.thumbnail_cols;

    // Atlas dimensions (with vertical gaps between rows)
    int atlas_w = batch.thumbnail_cols * cell_size;
    int atlas_h = batch.thumbnail_rows * cell_size
                + (batch.thumbnail_rows > 0 ? (batch.thumbnail_rows - 1) * gap_v : 0);

    // Create atlas (RGBA)
    cv::Mat atlas(atlas_h, atlas_w, CV_8UC4,
                  cv::Scalar(kAtlasBgR, kAtlasBgG, kAtlasBgB, kAtlasBgA));

    for (int i = 0; i < count; ++i) {
        int col = i % batch.thumbnail_cols;
        int row = i / batch.thumbnail_cols;

        int cell_x = col * cell_size;
        int cell_y = row * (cell_size + gap_v);

        // Draw cell background
        cv::rectangle(atlas,
            cv::Rect(cell_x + pad, cell_y + pad,
                     cell_size - pad * 2, cell_size - pad * 2),
            cv::Scalar(kCellBgR, kCellBgG, kCellBgB, kCellBgA), cv::FILLED);

        cv::Mat thumb = cv::imread(batch.files[i].path.string(), cv::IMREAD_COLOR);
        if (thumb.empty()) continue;

        // Fit thumbnail maintaining aspect ratio (above label area)
        int avail_w = cell_size - pad * 2;
        int avail_h = thumb_h - pad * 2;
        float scale = std::min(
            static_cast<float>(avail_w) / thumb.cols,
            static_cast<float>(avail_h) / thumb.rows
        );
        int tw = static_cast<int>(thumb.cols * scale);
        int th = static_cast<int>(thumb.rows * scale);

        cv::Mat resized;
        cv::resize(thumb, resized, cv::Size(tw, th), 0, 0, cv::INTER_AREA);

        cv::Mat rgba;
        cv::cvtColor(resized, rgba, cv::COLOR_BGR2RGBA);

        // Center image within cell
        int ox = cell_x + (cell_size - tw) / 2;
        int oy = cell_y + pad + (avail_h - th) / 2;

        cv::Rect roi(ox, oy, tw, th);
        if (roi.x >= 0 && roi.y >= 0 &&
            roi.x + roi.width <= atlas_w && roi.y + roi.height <= atlas_h) {
            rgba.copyTo(atlas(roi));
        }

        // Thin border
        cv::rectangle(atlas, roi,
            cv::Scalar(kCellBorderR, kCellBorderG, kCellBorderB, kCellBorderA), 1);
    }

    // Upload atlas as texture
    TextureDesc desc;
    desc.width = atlas_w;
    desc.height = atlas_h;
    desc.format = TextureFormat::RGBA8;

    std::span<const uint8_t> data(atlas.data, atlas.total() * atlas.elemSize());

    if (batch.thumbnail_texture.valid()) {
        m_backend.destroy_texture(batch.thumbnail_texture);
    }

    batch.thumbnail_texture = m_backend.create_texture(desc, data);
    batch.thumbnails_ready = batch.thumbnail_texture.valid();

    spdlog::info("Thumbnail atlas generated: {}x{} ({} thumbs, {}px cells, gap {}px)",
                 atlas_w, atlas_h, count, cell_size, gap_v);
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

void* AppController::get_batch_thumbnail_texture_id() const {
    return m_backend.get_imgui_texture_id(m_state.batch.thumbnail_texture);
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

    // Custom mode uses custom_watermark state
    if (m_state.process_options.size_mode == WatermarkSizeMode::Custom &&
        m_state.custom_watermark.has_region) {

        const auto& cr = m_state.custom_watermark.region;
        WatermarkInfo info;
        info.is_custom = true;
        info.position = cv::Point(cr.x, cr.y);
        info.region = cr;
        // Set size hint based on dimensions
        info.size = (cr.width <= 48 && cr.height <= 48)
            ? WatermarkSize::Small
            : WatermarkSize::Large;

        m_state.watermark_info = info;

        spdlog::debug("Custom watermark info: {}x{} at ({}, {})",
                      cr.width, cr.height, cr.x, cr.y);
        return;
    }

    // Standard mode: Use forced size or auto-detect
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
    info.is_custom = false;

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
