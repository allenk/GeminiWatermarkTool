/**
 * @file    main_window.hpp
 * @brief   Main Window UI Component
 * @author  AllenK (Kwyshell)
 * @license MIT
 */

#pragma once

#include "gui/app/app_controller.hpp"
#include "gui/widgets/image_preview.hpp"

#include <memory>
#include <string>

namespace gwt::gui {

class MainWindow {
public:
    /**
     * Construct main window with controller reference
     * @param controller  Application controller (must outlive window)
     */
    explicit MainWindow(AppController& controller);
    
    ~MainWindow();
    
    // Non-copyable
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    /**
     * Render the main window
     * Call this every frame within ImGui context
     */
    void render();

    /**
     * Handle SDL event
     * @param event  SDL event
     * @return       true if event was consumed
     */
    bool handle_event(const union SDL_Event& event);

private:
    AppController& m_controller;
    std::unique_ptr<ImagePreview> m_image_preview;
    
    // File dialog state
    std::string m_last_open_path;
    std::string m_last_save_path;
    
    // UI rendering
    void render_menu_bar();
    void render_toolbar();
    void render_control_panel();
    void render_image_area();
    void render_status_bar();
    void render_about_dialog();
    
    // Actions
    void action_open_file();
    void action_save_file();
    void action_save_file_as();
    void action_close_file();
    void action_process();
    void action_revert();
    void action_toggle_preview();
    void action_zoom_in();
    void action_zoom_out();
    void action_zoom_fit();
    void action_zoom_100();
};

}  // namespace gwt::gui
