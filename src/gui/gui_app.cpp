/**
 * @file    gui_app.cpp
 * @brief   GUI Application Entry Point Implementation
 * @author  AllenK (Kwyshell)
 * @license MIT
 */

#include "gui/gui_app.hpp"
#include "gui/backend/render_backend.hpp"
#include "gui/app/app_controller.hpp"
#include "gui/widgets/main_window.hpp"
#include "gui/resources/style.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <implot.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace gwt::gui {

namespace {

// Window settings
constexpr int kDefaultWidth = 1600;
constexpr int kDefaultHeight = 1250;
constexpr int kMinWidth = 1030;
constexpr int kMinHeight = 888;
constexpr const char* kWindowTitle = "Gemini Watermark Tool";

// Parse backend type from command line
BackendType parse_backend_arg(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--backend=opengl") return BackendType::OpenGL;
#if defined(GWT_HAS_VULKAN)
        if (arg == "--backend=vulkan") return BackendType::Vulkan;
#endif
    }
    return BackendType::Auto;
}

}  // anonymous namespace

int run(int argc, char** argv) {
    // Setup logging
    auto logger = spdlog::stdout_color_mt("gwt-gui");
    spdlog::set_default_logger(logger);

#if defined(DEBUG) || defined(_DEBUG)
    spdlog::set_level(spdlog::level::debug);
#else
    spdlog::set_level(spdlog::level::info);
#endif

    spdlog::info("Starting Gemini Watermark Tool GUI v{}", APP_VERSION);

    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        spdlog::error("Failed to initialize SDL: {}", SDL_GetError());
        return 1;
    }

    // Parse backend type
    BackendType backend_type = parse_backend_arg(argc, argv);
    spdlog::info("Requested backend: {}", to_string(backend_type));

    // Create render backend
    auto backend = create_backend(backend_type);
    if (!backend) {
        spdlog::error("Failed to create render backend");
        SDL_Quit();
        return 1;
    }

    // Create SDL window with appropriate flags
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    if (backend->type() == BackendType::OpenGL) {
        window_flags |= SDL_WINDOW_OPENGL;
    }
#if defined(GWT_HAS_VULKAN)
    else if (backend->type() == BackendType::Vulkan) {
        window_flags |= SDL_WINDOW_VULKAN;
    }
#endif

    SDL_Window* window = SDL_CreateWindow(
        kWindowTitle,
        kDefaultWidth,
        kDefaultHeight,
        window_flags
    );

    if (!window) {
        spdlog::error("Failed to create window: {}", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Set minimum window size
    SDL_SetWindowMinimumSize(window, kMinWidth, kMinHeight);
    spdlog::info("Window minimum size: {}x{}", kMinWidth, kMinHeight);

    // Clamp window size to usable screen area (avoid window larger than display)
    {
        int display_index = SDL_GetDisplayForWindow(window);
        SDL_Rect display_bounds;
        
        if (display_index >= 0 && SDL_GetDisplayUsableBounds(display_index, &display_bounds)) {
            int current_w, current_h;
            SDL_GetWindowSize(window, &current_w, &current_h);
            
            // Use 95% of screen as max to leave some margin for taskbar etc.
            int max_w = static_cast<int>(display_bounds.w * 0.95f);
            int max_h = static_cast<int>(display_bounds.h * 0.95f);
            
            // Also respect minimum size
            int actual_w = std::clamp(current_w, kMinWidth, max_w);
            int actual_h = std::clamp(current_h, kMinHeight, max_h);
            
            // Only resize if needed
            if (actual_w != current_w || actual_h != current_h) {
                SDL_SetWindowSize(window, actual_w, actual_h);
                spdlog::info("Window clamped to {}x{} (screen usable: {}x{})",
                             actual_w, actual_h, display_bounds.w, display_bounds.h);
            }
            
            // Center window on screen
            SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        }
    }

    // Initialize backend with window
    if (!backend->init(window)) {
        spdlog::error("Failed to initialize backend: {}", to_string(backend->last_error()));
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    spdlog::info("Using render backend: {}", backend->name());

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;  // Disable imgui.ini - we manage window state ourselves
    // Note: Docking requires imgui[docking] feature in vcpkg

    // Handle HiDPI scaling
    float dpi_scale = 1.0f;
    int display_index = SDL_GetDisplayForWindow(window);
    if (display_index >= 0) {
        // SDL3: Get content scale (DPI / 96.0 on Windows)
        float scale_x = 1.0f, scale_y = 1.0f;
        if (SDL_GetDisplayContentScale(display_index) > 0) {
            scale_x = scale_y = SDL_GetDisplayContentScale(display_index);
        }
        dpi_scale = scale_x;
        spdlog::info("Display DPI scale: {:.2f}", dpi_scale);
    }

    // Scale font size based on DPI
    float base_font_size = 16.0f;
    float scaled_font_size = base_font_size * dpi_scale;

    // Clear existing fonts
    io.Fonts->Clear();

    // =========================================================================
    // Font Loading Strategy:
    // 1. Try Noto Sans CJK (best cross-platform CJK support)
    // 2. Try system fonts (Windows: Microsoft YaHei/JhengHei, macOS: PingFang)
    // 3. Fallback to ImGui default font (no CJK support)
    // =========================================================================

    // Define font search paths for each platform
    std::vector<std::string> font_paths;
    std::string windir;

#ifdef _WIN32
    char* buf = nullptr;
    size_t size = 0;
    if (_dupenv_s(&buf, &size, "WINDIR") == 0 && buf != nullptr) {
        windir = buf;
        free(buf);
    }

    // Windows font paths
    std::string fonts_dir = windir.empty() ? "C:\\Windows\\Fonts\\" : windir + "\\Fonts\\";

    font_paths = {
        // Noto Sans CJK (if installed)
        fonts_dir + "NotoSansCJK-Regular.ttc",
        fonts_dir + "NotoSansCJKtc-Regular.otf",
        fonts_dir + "NotoSansCJKsc-Regular.otf",
        // Microsoft JhengHei (Traditional Chinese, Windows Vista+)
        fonts_dir + "msjh.ttc",
        fonts_dir + "msjhl.ttc",
        // Microsoft YaHei (Simplified Chinese, Windows Vista+)
        fonts_dir + "msyh.ttc",
        fonts_dir + "msyhl.ttc",
        // Yu Gothic (Japanese, Windows 8.1+)
        fonts_dir + "YuGothM.ttc",
        // Malgun Gothic (Korean, Windows Vista+)
        fonts_dir + "malgun.ttf",
        // Segoe UI (general fallback)
        fonts_dir + "segoeui.ttf",
    };
#elif __APPLE__
    // macOS font paths
    font_paths = {
        // Noto Sans CJK (if installed via Homebrew)
        "/opt/homebrew/share/fonts/NotoSansCJK-Regular.ttc",
        "/usr/local/share/fonts/NotoSansCJK-Regular.ttc",
        // PingFang (macOS 10.11+)
        "/System/Library/Fonts/PingFang.ttc",
        // Hiragino Sans (Japanese)
        "/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc",
        // SF Pro (general)
        "/System/Library/Fonts/SFNS.ttf",
    };
#else
    // Linux font paths
    font_paths = {
        // Noto Sans CJK (common locations)
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        // WenQuanYi (common Chinese font on Linux)
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
        "/usr/share/fonts/wenquanyi/wqy-microhei/wqy-microhei.ttc",
        // DejaVu Sans (fallback)
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    };
#endif

    // Font configuration
    ImFontConfig font_config;
    font_config.SizePixels = scaled_font_size;
    font_config.OversampleH = 2;
    font_config.OversampleV = 1;
    font_config.PixelSnapH = true;

    // Glyph ranges for CJK support
    // Use full Chinese range to support both Simplified and Traditional
    static const ImWchar glyph_ranges[] = {
        0x0020, 0x00FF,  // Basic Latin + Latin Supplement
        0x2000, 0x206F,  // General Punctuation
        0x3000, 0x30FF,  // CJK Symbols and Punctuation, Hiragana, Katakana
        0x31F0, 0x31FF,  // Katakana Phonetic Extensions
        0xFF00, 0xFFEF,  // Half-width and Full-width Forms
        0x4E00, 0x9FAF,  // CJK Unified Ideographs
        0x3400, 0x4DBF,  // CJK Unified Ideographs Extension A
        0,
    };

    // Try to load fonts
    ImFont* loaded_font = nullptr;
    std::string loaded_font_name;

    for (const auto& font_path : font_paths) {
        if (std::filesystem::exists(font_path)) {
            spdlog::info("Trying font: {}", font_path);

            // if non pixel font, we plus 2 to size to make it visually similar
            scaled_font_size += 2 * dpi_scale;
            font_config.SizePixels = scaled_font_size;

            loaded_font = io.Fonts->AddFontFromFileTTF(
                font_path.c_str(),
                scaled_font_size,
                &font_config,
                glyph_ranges
            );

            if (loaded_font) {
                loaded_font_name = font_path;
                spdlog::info("Loaded font: {}", font_path);
                break;
            } else {
                spdlog::warn("Failed to load font: {}", font_path);
            }
        }
    }

    // Fallback to default font if no CJK font found
    if (!loaded_font) {
        spdlog::warn("No CJK font found, using default font (CJK characters will not display)");
        io.Fonts->AddFontDefault(&font_config);
    }

    // Build font atlas
    io.Fonts->Build();
    spdlog::info("Font atlas built successfully");
    // Scale ImGui style
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(dpi_scale);

    // Apply custom style (after scaling)
    apply_style();

    // Initialize ImGui backend
    backend->imgui_init();

    // Create controller and main window
    AppController controller(*backend);
    controller.state().dpi_scale = dpi_scale;  // Store for widgets to use
    MainWindow main_window(controller);

    // Structure to pass to event watch callback
    struct RenderContext {
        IRenderBackend* backend;
        MainWindow* main_window;
        SDL_Window* window;
    };

    RenderContext render_ctx{backend.get(), &main_window, window};

    // Event watch callback - called during Windows modal resize loop
    auto event_watch = [](void* userdata, SDL_Event* event) -> bool {
        auto* ctx = static_cast<RenderContext*>(userdata);

        if (event->type == SDL_EVENT_WINDOW_RESIZED ||
            event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
            event->type == SDL_EVENT_WINDOW_EXPOSED) {

            // Update backend size
            if (event->type == SDL_EVENT_WINDOW_RESIZED ||
                event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                ctx->backend->on_resize(event->window.data1, event->window.data2);
            }

            // Render frame immediately
            ctx->backend->begin_frame();
            ctx->backend->imgui_new_frame();
            ImGui::NewFrame();
            ctx->main_window->render();
            ImGui::Render();
            ctx->backend->imgui_render();
            ctx->backend->end_frame();
            ctx->backend->present();
        }

        return true;  // Allow event to be added to queue
    };

    // Register event watch for live resize on Windows
    SDL_AddEventWatch(event_watch, &render_ctx);

    // Load file from command line if provided
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (!arg.empty() && arg[0] != '-') {
            std::filesystem::path path(arg);
            if (AppController::is_supported_extension(path) && std::filesystem::exists(path)) {
                controller.load_image(path);
                break;
            }
        }
    }

    // Main loop
    bool running = true;

    while (running) {
        // Process events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            // Let ImGui process the event first
            ImGui_ImplSDL3_ProcessEvent(&event);

            switch (event.type) {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;

                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    if (event.window.windowID == SDL_GetWindowID(window)) {
                        running = false;
                    }
                    break;

                case SDL_EVENT_WINDOW_RESIZED:
                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                    // Resize is handled in event watch callback
                    backend->on_resize(event.window.data1, event.window.data2);
                    break;

                default:
                    // Let main window handle other events
                    main_window.handle_event(event);
                    break;
            }
        }

        // Render frame
        backend->begin_frame();
        backend->imgui_new_frame();
        ImGui::NewFrame();
        main_window.render();
        ImGui::Render();
        backend->imgui_render();
        backend->end_frame();
        backend->present();
    }

    // Remove event watch before cleanup
    SDL_RemoveEventWatch(event_watch, &render_ctx);

    // Cleanup
    spdlog::info("Shutting down...");

    backend->imgui_shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    backend->shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

}  // namespace gwt::gui
