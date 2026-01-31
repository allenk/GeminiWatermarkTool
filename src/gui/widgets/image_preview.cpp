/**
 * @file    image_preview.cpp
 * @brief   Image Preview Widget Implementation
 * @author  AllenK (Kwyshell)
 * @license MIT
 */

#include "gui/widgets/image_preview.hpp"

#include <imgui.h>
#include <algorithm>

namespace gwt::gui {

ImagePreview::ImagePreview(AppController& controller)
    : m_controller(controller)
{
}

void ImagePreview::render() {
    const auto& state = m_controller.state();

    if (!state.image.has_image()) {
        render_placeholder();
        return;
    }

    render_image();
}

void ImagePreview::render_placeholder() {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 content_start = ImGui::GetCursorScreenPos();

    // Draw placeholder text centered
    const char* text = "Drop an image here or click Open";
    ImVec2 text_size = ImGui::CalcTextSize(text);

    ImVec2 text_pos(
        content_start.x + (avail.x - text_size.x) * 0.5f,
        content_start.y + (avail.y - text_size.y) * 0.5f
    );

    ImGui::SetCursorScreenPos(text_pos);
    ImGui::TextDisabled("%s", text);

    // Draw border around the entire preview area
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    float margin = 10.0f;
    ImVec2 p_min(content_start.x + margin, content_start.y + margin);
    ImVec2 p_max(content_start.x + avail.x - margin, content_start.y + avail.y - margin);

    draw_list->AddRect(p_min, p_max, IM_COL32(128, 128, 128, 128), 0, 0, 1.0f);
}

void ImagePreview::render_image() {
    auto& state = m_controller.state();
    auto& opts = state.preview_options;

    void* tex_id = m_controller.get_preview_texture_id();
    if (!tex_id) return;

    // Get viewport size before creating scrolling region
    ImVec2 viewport_size = ImGui::GetContentRegionAvail();
    ImVec2 viewport_start = ImGui::GetCursorScreenPos();

    // Calculate image display size
    float img_w = static_cast<float>(state.image.width);
    float img_h = static_cast<float>(state.image.height);

    // Base scale: fit to viewport
    float scale_x = viewport_size.x / img_w;
    float scale_y = viewport_size.y / img_h;
    float base_scale = std::min(scale_x, scale_y);

    // Apply zoom
    float final_scale = base_scale * opts.zoom;
    float display_w = img_w * final_scale;
    float display_h = img_h * final_scale;

    // Calculate content size for scrolling
    float padding = 20.0f;
    float content_w = std::max(display_w + padding * 2, viewport_size.x);
    float content_h = std::max(display_h + padding * 2, viewport_size.y);

    // Create scrollable region
    ImGuiWindowFlags scroll_flags = ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::BeginChild("ImageScrollRegion", viewport_size, false, scroll_flags);

    // Set content size using SetCursorPos instead of Dummy
    ImGui::SetCursorPos(ImVec2(content_w, content_h));

    // Calculate image position (centered in content area)
    float image_x = (content_w - display_w) * 0.5f;
    float image_y = (content_h - display_h) * 0.5f;

    // Get scroll offset
    float scroll_x = ImGui::GetScrollX();
    float scroll_y = ImGui::GetScrollY();

    // Calculate screen position
    ImVec2 child_pos = ImGui::GetWindowPos();
    ImVec2 image_screen_pos(
        child_pos.x + image_x - scroll_x,
        child_pos.y + image_y - scroll_y
    );

    // Draw image using DrawList (allows drawing outside normal flow)
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 uv_min(0, 0);
    ImVec2 uv_max(1, 1);
    ImU32 tint = IM_COL32_WHITE;

    draw_list->AddImage(
        reinterpret_cast<ImTextureID>(tex_id),
        image_screen_pos,
        ImVec2(image_screen_pos.x + display_w, image_screen_pos.y + display_h),
        uv_min, uv_max, tint
    );

    // Draw watermark overlay
    if (opts.highlight_watermark && state.watermark_info) {
        const auto& info = *state.watermark_info;

        float wm_x = image_screen_pos.x + info.position.x * final_scale;
        float wm_y = image_screen_pos.y + info.position.y * final_scale;
        float wm_w = info.width() * final_scale;
        float wm_h = info.height() * final_scale;

        ImU32 color = opts.show_processed
            ? IM_COL32(0, 255, 0, 180)
            : IM_COL32(255, 100, 100, 180);

        draw_list->AddRect(
            ImVec2(wm_x, wm_y),
            ImVec2(wm_x + wm_w, wm_y + wm_h),
            color, 0, 0, 2.0f
        );

        const char* label = opts.show_processed ? "Removed" : "Watermark";
        draw_list->AddText(
            ImVec2(wm_x, wm_y - ImGui::GetTextLineHeight() - 2),
            color, label
        );
    }

    // Handle input at the end (after content is set up)
    handle_input(viewport_size, content_w, content_h);

    ImGui::EndChild();

    // Draw info overlay on top
    ImGui::SetCursorScreenPos(ImVec2(viewport_start.x + 5, viewport_start.y + 5));
    ImGui::Text("%.0f%% | %s",
                opts.zoom * 100.0f,
                opts.show_processed ? "Processed" : "Original");
}

void ImagePreview::handle_input(const ImVec2& viewport_size, float content_w, float content_h) {
    auto& state = m_controller.state();
    auto& opts = state.preview_options;

    ImGuiIO& io = ImGui::GetIO();

    // Check if this child window is hovered (use RootAndChildWindows for better detection)
    bool is_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

    // Mouse wheel zoom (works when hovered)
    if (is_hovered && io.MouseWheel != 0 && !io.KeyShift) {
        float zoom_delta = io.MouseWheel * 0.1f;
        opts.zoom = std::clamp(opts.zoom + zoom_delta * opts.zoom, 0.1f, 10.0f);
    }

    // Pan with Space + left mouse drag, middle mouse, or Alt + left mouse
    bool space_held = ImGui::IsKeyDown(ImGuiKey_Space);
    bool left_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool middle_down = ImGui::IsMouseDown(ImGuiMouseButton_Middle);

    bool pan_active = is_hovered && (
        middle_down ||
        (space_held && left_down) ||
        (io.KeyAlt && left_down)
    );

    if (pan_active) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);

        // Use mouse delta for smooth panning
        ImVec2 delta = io.MouseDelta;
        if (delta.x != 0 || delta.y != 0) {
            float max_scroll_x = std::max(0.0f, content_w - viewport_size.x);
            float max_scroll_y = std::max(0.0f, content_h - viewport_size.y);

            float new_scroll_x = std::clamp(ImGui::GetScrollX() - delta.x, 0.0f, max_scroll_x);
            float new_scroll_y = std::clamp(ImGui::GetScrollY() - delta.y, 0.0f, max_scroll_y);
            ImGui::SetScrollX(new_scroll_x);
            ImGui::SetScrollY(new_scroll_y);
        }
    }

    // Show grab cursor when space is held
    if (is_hovered && space_held && !left_down) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    // Double-click to reset view
    if (is_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !io.KeyAlt && !space_held) {
        opts.reset_view();
        ImGui::SetScrollX(0);
        ImGui::SetScrollY(0);
    }

    // Arrow keys for panning
    float pan_speed = 20.0f;
    if (ImGui::IsKeyDown(ImGuiKey_LeftArrow)) {
        ImGui::SetScrollX(ImGui::GetScrollX() - pan_speed);
    }
    if (ImGui::IsKeyDown(ImGuiKey_RightArrow)) {
        ImGui::SetScrollX(ImGui::GetScrollX() + pan_speed);
    }
    if (ImGui::IsKeyDown(ImGuiKey_UpArrow)) {
        ImGui::SetScrollY(ImGui::GetScrollY() - pan_speed);
    }
    if (ImGui::IsKeyDown(ImGuiKey_DownArrow)) {
        ImGui::SetScrollY(ImGui::GetScrollY() + pan_speed);
    }
}

}  // namespace gwt::gui
