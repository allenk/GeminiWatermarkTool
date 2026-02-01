/**
 * @file    main_window.cpp
 * @brief   Main Window UI Implementation
 * @author  AllenK (Kwyshell)
 * @license MIT
 */

#include "gui/widgets/main_window.hpp"

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
        std::filesystem::path path(event.drop.data);

        if (AppController::is_supported_extension(path)) {
            m_controller.load_image(path);
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
            if (ImGui::MenuItem("Close", "Ctrl+W", false, m_controller.state().image.has_image())) {
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

    ImGui::Separator();
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
    if (ImGui::RadioButton("Custom", size_option == 3)) {
        m_controller.set_size_mode(WatermarkSizeMode::Custom);
    }

    // Custom mode controls
    if (opts.size_mode == WatermarkSizeMode::Custom && state.image.has_image()) {
        ImGui::Indent();

        if (state.custom_watermark.has_region) {
            // Re-detect button
            if (ImGui::SmallButton("Re-detect")) {
                state.custom_watermark.detection_attempted = false;
                m_controller.detect_custom_watermark();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset")) {
                state.custom_watermark.clear();
                m_controller.detect_custom_watermark();
            }

            // Show confidence
            if (state.custom_watermark.detection_confidence > 0.0f) {
                ImGui::TextColored(
                    ImVec4(0.3f, 0.8f, 0.3f, 1.0f),
                    "Confidence: %.0f%%",
                    state.custom_watermark.detection_confidence * 100.0f);
            } else {
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
    if (state.watermark_info && state.image.has_image()) {
        ImGui::Spacing();
        ImGui::Text("Detected Info");
        ImGui::Separator();

        const auto& info = *state.watermark_info;
        ImGui::Text("Size: %dx%d", info.width(), info.height());
        ImGui::Text("Position:");
        ImGui::Text("  (%d, %d)", info.position.x, info.position.y);

        if (info.is_custom) {
            ImGui::Text("Region:");
            ImGui::Text("  (%d,%d)-(%d,%d)",
                       info.region.x, info.region.y,
                       info.region.x + info.region.width,
                       info.region.y + info.region.height);
        }
    }

    ImGui::Spacing();
    ImGui::Text("Preview");
    ImGui::Separator();

    // Preview options
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

    // Process button at bottom
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImVec2 button_size(-1, 40.0f * state.dpi_scale);
    ImGui::BeginDisabled(!state.can_process());
    if (ImGui::Button("Process Image", button_size)) {
        action_process();
    }
    ImGui::EndDisabled();

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
        row("W A S D", "Move selected region");
        row("Space", "Pan (hold + drag)");
        row("Alt", "Pan (hold + drag)");

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
    float height = ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.y * scale;

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - height));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, height));

    ImGui::Begin("StatusBar", nullptr, flags);

    // Vertical centering: add padding
    float text_height = ImGui::GetTextLineHeight();
    float padding_y = (height - text_height) * 0.5f - ImGui::GetStyle().WindowPadding.y;
    if (padding_y > 0) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + padding_y);
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
    m_controller.close_image();
}

void MainWindow::action_process() {
    m_controller.process_current();
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
