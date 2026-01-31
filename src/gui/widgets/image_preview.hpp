/**
 * @file    image_preview.hpp
 * @brief   Image Preview Widget
 * @author  AllenK (Kwyshell)
 * @license MIT
 */

#pragma once

#include "gui/app/app_controller.hpp"
#include <imgui.h>

namespace gwt::gui {

class ImagePreview {
public:
    explicit ImagePreview(AppController& controller);
    ~ImagePreview() = default;
    
    // Non-copyable
    ImagePreview(const ImagePreview&) = delete;
    ImagePreview& operator=(const ImagePreview&) = delete;

    /**
     * Render the preview
     * Call within ImGui context
     */
    void render();

private:
    AppController& m_controller;
    
    void render_image();
    void render_placeholder();
    void handle_input(const ImVec2& viewport_size, float display_w, float display_h);
};

}  // namespace gwt::gui
