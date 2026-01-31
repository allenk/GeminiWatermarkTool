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

#include <memory>
#include <string>

namespace gwt::gui {

namespace {

// Window settings
constexpr int kDefaultWidth = 1600;
constexpr int kDefaultHeight = 1200;
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
    spdlog::set_level(spdlog::level::debug);

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

    // Load default font with scaled size
    io.Fonts->Clear();

    // Or load a better font if available
    ImFontConfig font_config;
    font_config.SizePixels = scaled_font_size;
    font_config.OversampleH = 2;
    font_config.OversampleV = 1;
    io.Fonts->AddFontDefault(&font_config);

    // Scale ImGui style
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(dpi_scale);

    // Apply custom style (after scaling)
    apply_style();

    // Initialize ImGui backend
    backend->imgui_init();

    // Create controller and main window
    AppController controller(*backend);
    MainWindow main_window(controller);

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
                    backend->on_resize(event.window.data1, event.window.data2);
                    break;

                default:
                    // Let main window handle other events
                    main_window.handle_event(event);
                    break;
            }
        }

        // Begin frame
        backend->begin_frame();
        backend->imgui_new_frame();
        ImGui::NewFrame();

        // Render UI
        main_window.render();

        // End frame
        ImGui::Render();
        backend->imgui_render();
        backend->end_frame();
        backend->present();
    }

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
