/**
 * @file    main.cpp
 * @brief   Gemini Watermark Tool - Entry Point
 * @author  AllenK (Kwyshell)
 * @license MIT
 *
 * @details
 * Entry point that routes to either CLI or GUI mode based on arguments.
 *
 * Launch modes:
 *   - No arguments:           Launch GUI (if available), otherwise show help
 *   - --gui or -g:            Force GUI mode
 *   - Any other arguments:    CLI mode (original behavior)
 *   - Single file path:       CLI simple mode (in-place edit)
 */

#include "cli/cli_app.hpp"

#if defined(GWT_HAS_GUI)
#include "gui/gui_app.hpp"
#endif

#include <string_view>
#include <cstring>

namespace {

/**
 * Check if GUI mode should be launched
 */
[[nodiscard]] bool should_launch_gui(int argc, char** argv) {
#if defined(GWT_HAS_GUI)
    // No arguments: launch GUI
    if (argc == 1) {
        return true;
    }

    // Check for explicit --gui flag
    for (int i = 1; i < argc; ++i) {
        std::string_view arg{argv[i]};
        if (arg == "--gui" || arg == "-g") {
            return true;
        }
    }
#else
    // Suppress unused parameter warnings
    (void)argc;
    (void)argv;
#endif

    return false;
}

/**
 * Remove GUI-specific flags from argv for CLI processing
 */
void filter_gui_flags([[maybe_unused]] int& argc, [[maybe_unused]] char** argv) {
#if defined(GWT_HAS_GUI)
    int write_idx = 1;
    for (int read_idx = 1; read_idx < argc; ++read_idx) {
        std::string_view arg{argv[read_idx]};
        if (arg != "--gui" && arg != "-g") {
            argv[write_idx++] = argv[read_idx];
        }
    }
    argc = write_idx;
#endif
}

}  // anonymous namespace

int main(int argc, char** argv) {
    // Check if GUI mode should be launched
    if (should_launch_gui(argc, argv)) {
#if defined(GWT_HAS_GUI)
        return gwt::gui::run(argc, argv);
#else
        // GUI not compiled in, fall through to CLI
#endif
    }

    // Filter out GUI flags if any (in case --gui was combined with other args)
    filter_gui_flags(argc, argv);

    // Run CLI mode
    return gwt::cli::run(argc, argv);
}
