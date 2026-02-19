/**
 * @file    main_window.cpp
 * @brief   Main Window UI Implementation
 * @author  AllenK (Kwyshell)
 * @license MIT
 */

#include "gui/widgets/main_window.hpp"
#include "utils/path_formatter.hpp"

#include <imgui.h>
#include <implot.h>
#include <SDL3/SDL.h>
#include <nfd.h>

#include <spdlog/spdlog.h>
#include <fmt/core.h>
#include <algorithm>
#include <cstdlib>
#include <cstdio>

namespace gwt::gui {

// =============================================================================
// File Dialog Helpers (Cross-platform via nativefiledialog-extended)
// With Linux fallback to zenity/kdialog when portal is unavailable
// =============================================================================

namespace {

#ifdef __linux__
// =============================================================================
// Linux Fallback: zenity / kdialog
// =============================================================================
enum class LinuxDialogTool {
    None,
    Zenity,
    Kdialog
};

// Escape single quotes for safe shell argument interpolation
std::string shell_escape(const std::string& input) {
    std::string escaped = input;
    size_t pos = 0;
    while ((pos = escaped.find('\'', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "'\\''");
        pos += 4;
    }
    return escaped;
}

// Detect available dialog tool (cached on first call)
LinuxDialogTool detect_dialog_tool() {
    static LinuxDialogTool cached = []() {
        // Check zenity first (more common on GNOME/GTK desktops)
        if (std::system("which zenity > /dev/null 2>&1") == 0)
            return LinuxDialogTool::Zenity;
        if (std::system("which kdialog > /dev/null 2>&1") == 0)
            return LinuxDialogTool::Kdialog;
        return LinuxDialogTool::None;
    }();
    return cached;
}

// Run a shell command and capture its stdout as a file path
std::optional<std::filesystem::path> run_command_dialog(const std::string& cmd) {
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return std::nullopt;

    char buffer[4096];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }

    int status = pclose(pipe);
    if (status != 0 || result.empty()) return std::nullopt;

    // Remove trailing newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }

    if (result.empty()) return std::nullopt;
    return std::filesystem::path(result);
}

std::optional<std::filesystem::path> fallback_open_file_dialog() {
    auto tool = detect_dialog_tool();

    if (tool == LinuxDialogTool::Zenity) {
        return run_command_dialog(
            "zenity --file-selection "
            "--title='Open Image' "
            "--file-filter='Image Files|*.jpg *.jpeg *.png *.webp *.bmp' "
            "--file-filter='All Files|*' "
            "2>/dev/null"
        );
    }
    if (tool == LinuxDialogTool::Kdialog) {
        return run_command_dialog(
            "kdialog --getopenfilename . "
            "'Image Files (*.jpg *.jpeg *.png *.webp *.bmp)' "
            "2>/dev/null"
        );
    }

    spdlog::error("No file dialog available. Install zenity or kdialog.");
    return std::nullopt;
}

std::optional<std::filesystem::path> fallback_save_file_dialog(const std::string& default_name) {
    auto tool = detect_dialog_tool();

    if (tool == LinuxDialogTool::Zenity) {
        std::string cmd = "zenity --file-selection --save --confirm-overwrite "
                          "--title='Save Image' "
                          "--file-filter='PNG Image|*.png' "
                          "--file-filter='JPEG Image|*.jpg *.jpeg' "
                          "--file-filter='WebP Image|*.webp' "
                          "--file-filter='All Files|*' ";
        if (!default_name.empty()) {
            cmd += "--filename='" + shell_escape(default_name) + "' ";
        }
        cmd += "2>/dev/null";
        return run_command_dialog(cmd);
    }
    if (tool == LinuxDialogTool::Kdialog) {
        std::string cmd = "kdialog --getsavefilename ";
        if (!default_name.empty()) {
            cmd += "'" + shell_escape(default_name) + "' ";
        } else {
            cmd += ". ";
        }
        cmd += "'Image Files (*.png *.jpg *.jpeg *.webp)' 2>/dev/null";
        return run_command_dialog(cmd);
    }

    spdlog::error("No file dialog available. Install zenity or kdialog.");
    return std::nullopt;
}

std::optional<std::filesystem::path> fallback_pick_folder_dialog() {
    auto tool = detect_dialog_tool();

    if (tool == LinuxDialogTool::Zenity) {
        return run_command_dialog(
            "zenity --file-selection --directory "
            "--title='Select Folder' "
            "2>/dev/null"
        );
    }
    if (tool == LinuxDialogTool::Kdialog) {
        return run_command_dialog(
            "kdialog --getexistingdirectory . 2>/dev/null"
        );
    }

    spdlog::error("No file dialog available. Install zenity or kdialog.");
    return std::nullopt;
}
#endif  // __linux__

// =============================================================================
// Cross-platform file dialogs (NFD with Linux fallback)
// =============================================================================
std::optional<std::filesystem::path> open_file_dialog() {
    NFD_Init();

    nfdchar_t* out_path = nullptr;
    nfdfilteritem_t filters[] = {
        {"Image Files", "jpg,jpeg,png,webp,bmp"}
    };

    nfdresult_t result = NFD_OpenDialog(&out_path, filters, 1, nullptr);

    std::optional<std::filesystem::path> path;
    if (result == NFD_OKAY && out_path) {
        path = std::filesystem::path(out_path);
        NFD_FreePath(out_path);
    } else if (result == NFD_ERROR) {
        spdlog::debug("NFD dialog failed: {}", NFD_GetError());
#ifdef __linux__
        spdlog::info("Falling back to zenity/kdialog...");
        NFD_Quit();
        return fallback_open_file_dialog();
#endif
    }
    // NFD_CANCEL means user cancelled, no error

    NFD_Quit();
    return path;
}

std::optional<std::filesystem::path> save_file_dialog(const std::string& default_name = "") {
    NFD_Init();

    nfdchar_t* out_path = nullptr;
    nfdfilteritem_t filters[] = {
        {"PNG Image", "png"},
        {"JPEG Image", "jpg,jpeg"},
        {"WebP Image", "webp"},
        {"All Files", "*"}
    };

    const nfdchar_t* default_path = nullptr;
    const nfdchar_t* default_filename = default_name.empty() ? nullptr : default_name.c_str();

    nfdresult_t result = NFD_SaveDialog(&out_path, filters, 4, default_path, default_filename);

    std::optional<std::filesystem::path> path;
    if (result == NFD_OKAY && out_path) {
        path = std::filesystem::path(out_path);
        NFD_FreePath(out_path);
    } else if (result == NFD_ERROR) {
        spdlog::debug("NFD dialog failed: {}", NFD_GetError());
#ifdef __linux__
        spdlog::info("Falling back to zenity/kdialog...");
        NFD_Quit();
        return fallback_save_file_dialog(default_name);
#endif
    }

    NFD_Quit();
    return path;
}

[[maybe_unused]] std::optional<std::filesystem::path> pick_folder_dialog() {
    NFD_Init();

    nfdchar_t* out_path = nullptr;
    nfdresult_t result = NFD_PickFolder(&out_path, nullptr);

    std::optional<std::filesystem::path> path;
    if (result == NFD_OKAY && out_path) {
        path = std::filesystem::path(out_path);
        NFD_FreePath(out_path);
    } else if (result == NFD_ERROR) {
        spdlog::debug("NFD dialog failed: {}", NFD_GetError());
#ifdef __linux__
        spdlog::info("Falling back to zenity/kdialog...");
        NFD_Quit();
        return fallback_pick_folder_dialog();
#endif
    }

    NFD_Quit();
    return path;
}

}  // anonymous namespace

// =============================================================================
// Construction
// =============================================================================

MainWindow::MainWindow(AppController& controller)
    : m_controller(controller)
    , m_image_preview(std::make_unique<ImagePreview>(controller))
{
    spdlog::debug("MainWindow created");
}

MainWindow::~MainWindow() = default;

// =============================================================================
// Main Render
// =============================================================================

void MainWindow::render() {
    // Update texture if needed
    m_controller.update_texture_if_needed();

    // Get DPI scale
    const float scale = m_controller.state().dpi_scale;

    // Setup main window flags
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_MenuBar;

    // Get viewport
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    // Begin main window
    ImGui::Begin("MainWindow", nullptr, window_flags);

    render_menu_bar();
    render_toolbar();

    // Calculate status bar height (same formula as render_status_bar)
    float status_bar_height = ImGui::GetFrameHeight() + 8.0f * scale;

    // Calculate content height (available space minus status bar)
    float content_height = ImGui::GetContentRegionAvail().y - status_bar_height;
    content_height = std::max(1.0f, content_height);

    // Main content area with control panel and image
    float control_panel_width = 230.0f * scale;

    ImGui::BeginChild("ControlPanel", ImVec2(control_panel_width, content_height), true);
    render_control_panel();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("ImageArea", ImVec2(0, content_height), true);
    render_image_area();
    ImGui::EndChild();

    ImGui::End();

    // Status bar (separate window at bottom)
    render_status_bar();

    // Dialogs
    if (m_controller.state().show_about_dialog) {
        render_about_dialog();
    }

    // Batch confirmation dialog
    if (m_controller.state().batch.show_confirm_dialog) {
        render_batch_confirm_dialog();
    }

    // Batch processing tick (process one file per frame)
    if (m_controller.state().batch.in_progress) {
        m_controller.process_batch_next();
    }
}

// =============================================================================
// Event Handling
// =============================================================================

bool MainWindow::handle_event(const SDL_Event& event) {
    // Handle keyboard shortcuts
    if (event.type == SDL_EVENT_KEY_DOWN) {
        bool ctrl = (event.key.mod & SDL_KMOD_CTRL) != 0;
        bool shift = (event.key.mod & SDL_KMOD_SHIFT) != 0;

        if (ctrl && !shift) {
            switch (event.key.key) {
                case SDLK_O: action_open_file(); return true;
                case SDLK_S: action_save_file(); return true;
                case SDLK_W: action_close_file(); return true;
                case SDLK_Z: action_revert(); return true;
                case SDLK_EQUALS: action_zoom_in(); return true;
                case SDLK_MINUS: action_zoom_out(); return true;
                case SDLK_0: action_zoom_fit(); return true;
                case SDLK_1: action_zoom_100(); return true;
            }
        } else if (ctrl && shift) {
            switch (event.key.key) {
                case SDLK_S: action_save_file_as(); return true;
            }
        } else if (!ctrl && !shift) {
            // Single key shortcuts (no modifiers)
            switch (event.key.key) {
                case SDLK_X: action_process(); return true;
                case SDLK_V: action_toggle_preview(); return true;
                case SDLK_Z: action_revert(); return true;
            }
        }
    }

    // Handle drag & drop
    if (event.type == SDL_EVENT_DROP_FILE) {
        // SDL always returns UTF-8 paths on all platforms
        // On Windows without UTF-8 beta, we must convert properly
        std::filesystem::path path = gwt::path_from_utf8(event.drop.data);

        if (std::filesystem::is_directory(path)) {
            // Directory dropped: collect all supported image files (non-recursive)
            for (const auto& entry : std::filesystem::directory_iterator(path)) {
                if (entry.is_regular_file() && AppController::is_supported_extension(entry.path())) {
                    m_pending_drops.push_back(entry.path());
                }
            }
            return true;
        } else if (AppController::is_supported_extension(path)) {
            // Single file
            m_pending_drops.push_back(path);
            return true;
        }
    }

    // After all drop events, process the collected files
    // SDL_EVENT_DROP_COMPLETE signals end of a drop operation
    if (event.type == SDL_EVENT_DROP_COMPLETE) {
        if (!m_pending_drops.empty()) {
            if (m_pending_drops.size() == 1) {
                // Single file: normal load
                m_controller.exit_batch_mode();
                m_controller.load_image(m_pending_drops[0]);
            } else {
                // Multiple files: enter batch mode
                m_controller.exit_batch_mode();
                m_controller.enter_batch_mode(m_pending_drops);
            }
            m_pending_drops.clear();
            return true;
        }
    }

    return false;
}

// =============================================================================
// UI Components
// =============================================================================

void MainWindow::render_menu_bar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                action_open_file();
            }
            if (ImGui::MenuItem("Save", "Ctrl+S", false, m_controller.state().can_save())) {
                action_save_file();
            }
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, m_controller.state().can_save())) {
                action_save_file_as();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Close", "Ctrl+W", false,
                                m_controller.state().image.has_image() || m_controller.state().batch.is_batch_mode())) {
                action_close_file();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                // Request quit
                SDL_Event quit_event = {};
                quit_event.type = SDL_EVENT_QUIT;
                SDL_PushEvent(&quit_event);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Process", "X", false, m_controller.state().can_process())) {
                action_process();
            }
            if (ImGui::MenuItem("Revert", "Z", false, m_controller.state().image.has_processed())) {
                action_revert();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Compare Original", "V", false, m_controller.state().image.has_processed())) {
                action_toggle_preview();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Zoom In", "Ctrl++")) {
                action_zoom_in();
            }
            if (ImGui::MenuItem("Zoom Out", "Ctrl+-")) {
                action_zoom_out();
            }
            if (ImGui::MenuItem("Fit to Window", "Ctrl+0")) {
                action_zoom_fit();
            }
            if (ImGui::MenuItem("100%", "Ctrl+1")) {
                action_zoom_100();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Online Documentation")) {
                // Open Medium article in default browser
                constexpr const char* url =
                    "https://allenkuo.medium.com/removing-gemini-ai-watermarks-"
                    "a-deep-dive-into-reverse-alpha-blending-bbbd83af2a3f";
                std::string cmd;
                #ifdef _WIN32
                    cmd = std::string("start ") + url;
                #elif __APPLE__
                    cmd = std::string("open ") + url;
                #else
                    cmd = std::string("xdg-open ") + url + " &";
                #endif
                [[maybe_unused]] int ret = system(cmd.c_str());
            }
            ImGui::Separator();
            if (ImGui::MenuItem("About")) {
                m_controller.state().show_about_dialog = true;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void MainWindow::render_toolbar() {
    const float scale = m_controller.state().dpi_scale;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f * scale, 6.0f * scale));

    ImGui::Separator();

    if (ImGui::Button("Open")) {
        action_open_file();
    }
    ImGui::SameLine();

    ImGui::BeginDisabled(!m_controller.state().can_save());
    if (ImGui::Button("Save")) {
        action_save_file();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();

    ImGui::BeginDisabled(!m_controller.state().can_process());
    if (ImGui::Button("Process")) {
        action_process();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();

    ImGui::BeginDisabled(!m_controller.state().image.has_processed());
    if (ImGui::Button("Compare")) {
        action_toggle_preview();
    }
    ImGui::EndDisabled();

    ImGui::PopStyleVar();
    ImGui::Separator();
}

void MainWindow::render_control_panel() {
    auto& state = m_controller.state();

    ImGui::Text("Operation");
    ImGui::Separator();

    // Mode selection
    bool remove_mode = state.process_options.remove_mode;
    if (ImGui::RadioButton("Remove Watermark", remove_mode)) {
        m_controller.set_remove_mode(true);
    }
    if (ImGui::RadioButton("Add Watermark", !remove_mode)) {
        m_controller.set_remove_mode(false);
    }

    ImGui::Spacing();
    ImGui::Text("Watermark Size");
    ImGui::Separator();

    // Size selection using WatermarkSizeMode
    auto& opts = state.process_options;
    int size_option = static_cast<int>(opts.size_mode);

    if (ImGui::RadioButton("Auto Detect", size_option == 0)) {
        m_controller.set_size_mode(WatermarkSizeMode::Auto);
    }
    if (ImGui::RadioButton("48x48 (Small)", size_option == 1)) {
        m_controller.set_size_mode(WatermarkSizeMode::Small);
    }
    if (ImGui::RadioButton("96x96 (Large)", size_option == 2)) {
        m_controller.set_size_mode(WatermarkSizeMode::Large);
    }
    // Custom mode is not available in batch mode
    if (!state.batch.is_batch_mode()) {
        if (ImGui::RadioButton("Custom", size_option == 3)) {
            m_controller.set_size_mode(WatermarkSizeMode::Custom);
        }
    }

    // Custom mode controls (not in batch mode)
    if (opts.size_mode == WatermarkSizeMode::Custom && state.image.has_image()
        && !state.batch.is_batch_mode()) {
        ImGui::Indent();

        if (state.custom_watermark.has_region) {
            // Re-detect button (full-image detection)
            if (ImGui::SmallButton("Re-detect")) {
                state.custom_watermark.detection_attempted = false;
                m_controller.detect_custom_watermark();
            }
            ImGui::SameLine();
            // Snap button (multi-scale search within region)
            if (ImGui::SmallButton("Snap")) {
                m_controller.snap_to_watermark();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset")) {
                state.custom_watermark.clear();
                m_controller.detect_custom_watermark();
            }

            // Show snap result info
            if (state.custom_watermark.snap_result.has_value()) {
                const auto& snap = *state.custom_watermark.snap_result;
                if (snap.snap_found) {
                    // Color based on confidence level
                    ImVec4 color;
                    if (snap.snap_confidence >= 0.3f) {
                        color = ImVec4(0.1f, 0.9f, 0.25f, 1.0f);  // Green
                    } else if (snap.snap_confidence >= 0.1f) {
                        color = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);   // Yellow
                    } else {
                        color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);   // Red
                    }
                    ImGui::TextColored(color,
                        "Snap: %dx%d at (%d,%d) %.0f%%",
                        snap.detected_size, snap.detected_size,
                        snap.snapped_rect.x, snap.snapped_rect.y,
                        snap.snap_confidence * 100.0f);
                } else {
                    ImGui::TextColored(
                        ImVec4(0.8f, 0.4f, 0.2f, 1.0f),
                        "No watermark found in region");
                }
            } else if (state.custom_watermark.detection_confidence > 0.0f) {
                // Legacy detection confidence display (no snap yet)
                ImGui::TextColored(
                    ImVec4(0.3f, 0.8f, 0.3f, 1.0f),
                    "Confidence: %.0f%%",
                    state.custom_watermark.detection_confidence * 100.0f);
            } else if (!state.custom_watermark.snap_result.has_value()) {
                ImGui::TextColored(
                    ImVec4(0.8f, 0.6f, 0.2f, 1.0f),
                    "Fallback position");
            }
        } else {
            ImGui::TextWrapped("Draw a rectangle on the preview to mark the watermark region.");
        }

        ImGui::Unindent();
    }

    // Show detected info
    if (state.watermark_info && state.image.has_image() && !state.batch.is_batch_mode()) {
        ImGui::Spacing();
        ImGui::Text("Detected Info");
        ImGui::Separator();

        const auto& info = *state.watermark_info;
        ImGui::Text("Size: %dx%d", info.width(), info.height());
        ImGui::Text("Position:");
        ImGui::Text("  (%d, %d)", info.position.x, info.position.y);

        if (info.is_custom) {
            // Show effective rect (snap or user region)
            cv::Rect eff = state.custom_watermark.effective_rect();
            bool using_snap = state.custom_watermark.snap_result.has_value()
                           && state.custom_watermark.snap_result->snap_found;

            ImGui::Text("Region%s:", using_snap ? " (snapped)" : "");
            ImGui::Text("  (%d,%d)-(%d,%d) %dx%d",
                       eff.x, eff.y,
                       eff.x + eff.width,
                       eff.y + eff.height,
                       eff.width, eff.height);
        }

        // Inpaint cleanup controls — only in Custom remove mode
        if (opts.remove_mode && info.is_custom) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImVec4(0.35f, 0.85f, 0.75f, 1.0f));  // Teal/cyan
            ImGui::Text("Cleanup");
            ImGui::PopStyleColor();
            ImGui::Separator();

            auto& inpaint = state.process_options.inpaint;

            bool enabled = inpaint.enabled;
            if (ImGui::Checkbox("Inpaint residual", &enabled)) {
                inpaint.enabled = enabled;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "Clean up residual artifacts after watermark removal.\n"
                    "Most effective on resized images where interpolation\n"
                    "breaks the exact alpha-pixel relationship.\n\n"
                    "NS / TELEA: OpenCV inpaint (edge reconstruction)\n"
                    "Soft Inpaint: Gradient-guided Gaussian blur");
            }

            if (inpaint.enabled) {
                // Method selector (first — determines behavior)
                const char* methods[] = { "NS", "TELEA", "Soft Inpaint" };
                int method_idx = 0;
                if (inpaint.method == InpaintMethod::NS) method_idx = 0;
                else if (inpaint.method == InpaintMethod::TELEA) method_idx = 1;
                else method_idx = 2;  // GAUSSIAN

                ImGui::SetNextItemWidth(-1);
                if (ImGui::Combo("##inpaint_method", &method_idx, methods, 3)) {
                    switch (method_idx) {
                        case 0:  inpaint.method = InpaintMethod::NS;       break;
                        case 1:  inpaint.method = InpaintMethod::TELEA;    break;
                        default: inpaint.method = InpaintMethod::GAUSSIAN; break;
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "NS: Navier-Stokes fluid dynamics (recommended)\n"
                        "TELEA: Fast marching method\n"
                        "Soft Inpaint: Gradient-guided Gaussian blur\n"
                        "  (smoother but may blur fine details)");
                }

                // Strength slider
                int strength_pct = static_cast<int>(inpaint.strength * 100.0f + 0.5f);
                strength_pct = ((strength_pct + 2) / 5) * 5;

                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##inpaint_strength", &strength_pct, 0, 100,
                                     "Strength: %d%%")) {
                    strength_pct = ((strength_pct + 2) / 5) * 5;
                    inpaint.strength = static_cast<float>(strength_pct) / 100.0f;
                }

                // Radius slider
                ImGui::SetNextItemWidth(-1);
                ImGui::SliderInt("##inpaint_radius", &inpaint.inpaint_radius, 1, 25,
                                "Radius: %d px");
            }
        }
    }

    // Detection threshold (always visible for batch, optional for single)
    if (state.batch.is_batch_mode()) {
        ImGui::Spacing();
        ImGui::Text("Detection");
        ImGui::Separator();

        bool use_det = state.batch.use_detection;
        if (ImGui::Checkbox("Auto-detect watermark", &use_det)) {
            state.batch.use_detection = use_det;
        }

        if (state.batch.use_detection) {
            // Convert to integer percentage for clean 5% steps
            int threshold_pct = static_cast<int>(state.batch.detection_threshold * 100.0f + 0.5f);
            // Snap to nearest 5
            threshold_pct = ((threshold_pct + 2) / 5) * 5;

            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderInt("##threshold", &threshold_pct, 0, 100, "Threshold: %d%%")) {
                // Snap to 5% steps
                threshold_pct = ((threshold_pct + 2) / 5) * 5;
                state.batch.detection_threshold = static_cast<float>(threshold_pct) / 100.0f;
            }
            if (threshold_pct > 0) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                                  "Skip images below %d%%", threshold_pct);
            } else {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                                  "Process all images");
            }
            ImGui::TextColored(ImVec4(0.4f, 0.6f, 0.4f, 1.0f),
                              "(25%% recommended)");
        }
    }

    // Preview options (only in single-image mode)
    if (!state.batch.is_batch_mode()) {
        ImGui::Spacing();
        ImGui::Text("Preview");
        ImGui::Separator();

        bool highlight = state.preview_options.highlight_watermark;
        if (ImGui::Checkbox("Highlight Watermark", &highlight)) {
            state.preview_options.highlight_watermark = highlight;
        }

        bool show_processed = state.preview_options.show_processed;
        ImGui::BeginDisabled(!state.image.has_processed());
        if (ImGui::Checkbox("Show Processed", &show_processed)) {
            state.preview_options.show_processed = show_processed;
            m_controller.invalidate_texture();
        }
        ImGui::EndDisabled();

        // Zoom
        ImGui::Spacing();
        ImGui::Text("Zoom: %.0f%%", state.preview_options.zoom * 100);
        if (ImGui::Button("Fit")) action_zoom_fit();
        ImGui::SameLine();
        if (ImGui::Button("100%")) action_zoom_100();
        ImGui::SameLine();
        if (ImGui::Button("+")) action_zoom_in();
        ImGui::SameLine();
        if (ImGui::Button("-")) action_zoom_out();
    }

    // Batch info
    if (state.batch.is_batch_mode()) {
        ImGui::Spacing();
        ImGui::Text("Batch");
        ImGui::Separator();

        ImGui::Text("Files: %zu", state.batch.total());

        if (state.batch.is_complete()) {
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f),
                              "OK: %zu  Skipped: %zu  Failed: %zu",
                              state.batch.success_count,
                              state.batch.skip_count,
                              state.batch.fail_count);
        }

        // Exit batch mode button
        if (!state.batch.in_progress) {
            if (ImGui::SmallButton("Exit Batch Mode")) {
                m_controller.exit_batch_mode();
            }
        }
    }

    // Process button
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImVec2 button_size(-1, 40.0f * state.dpi_scale);

    if (state.batch.in_progress) {
        // Cancel button during batch processing
        if (ImGui::Button("Cancel Batch", button_size)) {
            m_controller.cancel_batch();
        }
    } else {
        const char* button_label = state.batch.is_batch_mode()
            ? "Process Batch" : "Process Image";
        ImGui::BeginDisabled(!state.can_process());
        if (ImGui::Button(button_label, button_size)) {
            action_process();
        }
        ImGui::EndDisabled();
    }

    // Tips section
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();

    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Shortcuts");

    if (ImGui::BeginTable("shortcuts", 2)) {
        ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);

        auto row = [](const char* key, const char* desc) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", key);
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", desc);
            };

        row("X", "Process image");
        row("V", "Compare original");
        row("Z", "Revert to original");
        row("C (hold)", "Hide overlay");
        row("W A S D", "Move selected region");
        row("Space", "Pan (hold + drag)");
        row("Alt", "Pan (hold + drag)");
        row("Ctrl +/-", "Zoom in/out");
        row("Ctrl 0", "Zoom fit");
        row("Scroll", "Zoom to cursor");

        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::Text(" ");
}

void MainWindow::render_image_area() {
    m_image_preview->render();
}

void MainWindow::render_status_bar() {
    const auto& state = m_controller.state();
    const float scale = state.dpi_scale;

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    float height = ImGui::GetFrameHeight() + 8.0f * scale;  // match render()

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - height));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, height));

    ImGui::Begin("StatusBar", nullptr, flags);

    // Vertical centering
    float text_height = ImGui::GetTextLineHeight();
    float window_padding_y = ImGui::GetStyle().WindowPadding.y;
    float inner_height = height - window_padding_y * 2.0f;
    float offset_y = (inner_height - text_height) * 0.5f;
    if (offset_y > 0) {
        ImGui::SetCursorPosY(window_padding_y + offset_y);
    }

    // Status message
    ImGui::Text("%s", state.status_message.c_str());

    // Image info on the right
    if (state.image.has_image()) {
        std::string info = fmt::format("{}x{} | {}",
                                        state.image.width,
                                        state.image.height,
                                        state.preview_options.show_processed ? "Processed" : "Original");

        float text_width = ImGui::CalcTextSize(info.c_str()).x;
        ImGui::SameLine(ImGui::GetWindowWidth() - text_width - 10.0f * scale);
        ImGui::Text("%s", info.c_str());
    }

    ImGui::End();
}

void MainWindow::render_about_dialog() {
    ImGui::OpenPopup("About");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("About", &m_controller.state().show_about_dialog,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Gemini Watermark Tool");
        ImGui::Text("Version %s", APP_VERSION);
        ImGui::Separator();
        ImGui::Text("A tool to add/remove Gemini-style visible watermarks");
        ImGui::Text("using reverse alpha blending.");
        ImGui::Spacing();
        ImGui::Text("Author: Allen Kuo (@allenk)");
        ImGui::Text("License: MIT");
        ImGui::Spacing();

        float ok_width = 120.0f * m_controller.state().dpi_scale;
        if (ImGui::Button("OK", ImVec2(ok_width, 0))) {
            m_controller.state().show_about_dialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void MainWindow::render_batch_confirm_dialog() {
    auto& state = m_controller.state();

    ImGui::OpenPopup("Batch Processing");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Batch Processing", &state.batch.show_confirm_dialog,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Warning: Original files will be overwritten!");
        ImGui::Spacing();

        ImGui::Text("Files to process: %zu", state.batch.total());
        ImGui::Text("Mode: %s", state.process_options.remove_mode ? "Remove Watermark" : "Add Watermark");

        // Size mode
        const char* size_label = "Auto";
        switch (state.process_options.size_mode) {
            case WatermarkSizeMode::Small:  size_label = "48x48"; break;
            case WatermarkSizeMode::Large:  size_label = "96x96"; break;
            case WatermarkSizeMode::Custom: size_label = "Custom (auto-detect per image)"; break;
            default: break;
        }
        ImGui::Text("Size: %s", size_label);

        if (state.batch.use_detection) {
            int threshold_pct = static_cast<int>(state.batch.detection_threshold * 100.0f + 0.5f);
            ImGui::Text("Detection threshold: %d%%", threshold_pct);
            if (threshold_pct > 0) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                                  "Images below %d%% confidence will be skipped.", threshold_pct);
            } else {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                                  "All images will be processed (threshold = 0%%).");
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f),
                              "Detection disabled - all images will be processed!");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float button_w = 120.0f * state.dpi_scale;

        if (ImGui::Button("Process", ImVec2(button_w, 0))) {
            state.batch.show_confirm_dialog = false;
            m_controller.start_batch_processing();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(button_w, 0))) {
            state.batch.show_confirm_dialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

// =============================================================================
// Actions
// =============================================================================

void MainWindow::action_open_file() {
    if (auto path = open_file_dialog()) {
        m_controller.load_image(*path);
        m_last_open_path = path->parent_path().string();
    }
}

void MainWindow::action_save_file() {
    const auto& state = m_controller.state();
    if (!state.can_save()) return;

    // If we have original path, save with "_processed" suffix
    if (state.image.file_path) {
        auto path = *state.image.file_path;
        auto stem = path.stem().string();
        auto ext = path.extension();
        auto output = path.parent_path() / (stem + "_processed" + ext.string());
        m_controller.save_image(output);
    } else {
        action_save_file_as();
    }
}

void MainWindow::action_save_file_as() {
    if (!m_controller.state().can_save()) return;

    std::string default_name;
    if (m_controller.state().image.file_path) {
        auto path = *m_controller.state().image.file_path;
        default_name = path.stem().string() + "_processed" + path.extension().string();
    }

    if (auto path = save_file_dialog(default_name)) {
        m_controller.save_image(*path);
        m_last_save_path = path->parent_path().string();
    }
}

void MainWindow::action_close_file() {
    // Exit batch mode if active
    if (m_controller.state().batch.is_batch_mode()) {
        m_controller.exit_batch_mode();
    }
    m_controller.close_image();
}

void MainWindow::action_process() {
    auto& state = m_controller.state();

    if (state.batch.is_batch_mode()) {
        // Batch mode: show confirmation dialog
        state.batch.show_confirm_dialog = true;
    } else {
        // Single mode: process directly
        m_controller.process_current();
    }
}

void MainWindow::action_revert() {
    m_controller.revert_to_original();
}

void MainWindow::action_toggle_preview() {
    m_controller.toggle_preview();
}

void MainWindow::action_zoom_in() {
    auto& opts = m_controller.state().preview_options;
    opts.zoom = std::min(opts.zoom * 1.25f, 10.0f);
}

void MainWindow::action_zoom_out() {
    auto& opts = m_controller.state().preview_options;
    opts.zoom = std::max(opts.zoom / 1.25f, 0.1f);
}

void MainWindow::action_zoom_fit() {
    m_controller.state().preview_options.zoom = 1.0f;
    m_controller.state().preview_options.pan_x = 0;
    m_controller.state().preview_options.pan_y = 0;
}

void MainWindow::action_zoom_100() {
    // TODO: Calculate actual 100% zoom based on window/image size
    m_controller.state().preview_options.zoom = 1.0f;
}

}  // namespace gwt::gui
