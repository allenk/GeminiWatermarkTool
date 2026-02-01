/**
 * @file    image_preview.cpp
 * @brief   Image Preview Widget Implementation
 * @author  AllenK (Kwyshell)
 * @license MIT
 */

#include "gui/widgets/image_preview.hpp"

#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace gwt::gui {

// Anchor handle size in screen pixels
static constexpr float ANCHOR_SIZE = 6.0f;
static constexpr float ANCHOR_HIT_RADIUS = 10.0f;

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

// =============================================================================
// Coordinate conversion
// =============================================================================

ImVec2 ImagePreview::image_to_screen(float ix, float iy) const {
    return ImVec2(
        m_image_screen_pos.x + ix * m_final_scale,
        m_image_screen_pos.y + iy * m_final_scale
    );
}

ImVec2 ImagePreview::screen_to_image(float sx, float sy) const {
    if (m_final_scale < 1e-6f) return ImVec2(0, 0);
    return ImVec2(
        (sx - m_image_screen_pos.x) / m_final_scale,
        (sy - m_image_screen_pos.y) / m_final_scale
    );
}

// =============================================================================
// Main render
// =============================================================================

void ImagePreview::render_image() {
    auto& state = m_controller.state();
    auto& opts = state.preview_options;

    void* tex_id = m_controller.get_preview_texture_id();
    if (!tex_id) return;

    // Get available space for the scrolling region
    ImVec2 avail_for_child = ImGui::GetContentRegionAvail();
    ImVec2 viewport_start = ImGui::GetCursorScreenPos();

    // Image dimensions
    float img_w = static_cast<float>(state.image.width);
    float img_h = static_cast<float>(state.image.height);

    // Create scrollable region first, then get actual content region inside
    ImGuiWindowFlags scroll_flags = ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::BeginChild("ImageScrollRegion", avail_for_child, false, scroll_flags);

    // Get actual viewport size INSIDE the child window (accounts for scrollbar)
    ImVec2 viewport_size = ImGui::GetContentRegionAvail();

    // Base scale: fit to actual viewport (no extra space)
    float scale_x = viewport_size.x / img_w;
    float scale_y = viewport_size.y / img_h;
    float base_scale = std::min(scale_x, scale_y);

    // Apply zoom
    m_final_scale = base_scale * opts.zoom;
    float display_w = img_w * m_final_scale;
    float display_h = img_h * m_final_scale;

    // Content size equals display size when zoomed, viewport size when fit
    // This ensures no scrollbar at zoom=1.0
    float content_w = std::max(display_w, viewport_size.x);
    float content_h = std::max(display_h, viewport_size.y);

    // Set content size for scrolling
    ImGui::SetCursorPos(ImVec2(content_w, content_h));

    // Calculate image position (centered in content area)
    float image_x = (content_w - display_w) * 0.5f;
    float image_y = (content_h - display_h) * 0.5f;

    // Get scroll offset
    float scroll_x = ImGui::GetScrollX();
    float scroll_y = ImGui::GetScrollY();

    // Calculate screen position and cache for coordinate conversion
    ImVec2 child_pos = ImGui::GetWindowPos();
    m_image_screen_pos = ImVec2(
        child_pos.x + image_x - scroll_x,
        child_pos.y + image_y - scroll_y
    );

    // Draw image using DrawList
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    draw_list->AddImage(
        reinterpret_cast<ImTextureID>(tex_id),
        m_image_screen_pos,
        ImVec2(m_image_screen_pos.x + display_w, m_image_screen_pos.y + display_h),
        ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE
    );

    // Draw watermark overlay
    bool is_custom_mode = (state.process_options.size_mode == WatermarkSizeMode::Custom);

    if (is_custom_mode && state.custom_watermark.has_region) {
        // Custom mode: draw resizable rect with anchors
        draw_custom_rect_with_anchors(draw_list);
    } else if (opts.highlight_watermark && state.watermark_info) {
        // Standard mode: draw simple highlight rect
        const auto& info = *state.watermark_info;

        ImVec2 wm_tl = image_to_screen(
            static_cast<float>(info.position.x),
            static_cast<float>(info.position.y));
        ImVec2 wm_br = image_to_screen(
            static_cast<float>(info.position.x + info.width()),
            static_cast<float>(info.position.y + info.height()));

        ImU32 color = opts.show_processed
            ? IM_COL32(0, 255, 0, 180)
            : IM_COL32(255, 100, 100, 180);

        draw_list->AddRect(wm_tl, wm_br, color, 0, 0, 2.0f);

        const char* label = opts.show_processed ? "Removed" : "Watermark";
        draw_list->AddText(
            ImVec2(wm_tl.x, wm_tl.y - ImGui::GetTextLineHeight() - 2),
            color, label
        );
    }

    // Handle input
    handle_input(viewport_size, content_w, content_h);

    // Handle custom rect interaction (must be after handle_input for pan priority)
    if (is_custom_mode) {
        handle_custom_rect_interaction();
    }

    ImGui::EndChild();

    // Draw info overlay on top
    ImGui::SetCursorScreenPos(ImVec2(viewport_start.x + 5, viewport_start.y + 5));
    ImGui::Text("%.0f%% | %s",
                opts.zoom * 100.0f,
                opts.show_processed ? "Processed" : "Original");
}

// =============================================================================
// Custom watermark rect drawing
// =============================================================================

void ImagePreview::draw_custom_rect_with_anchors(ImDrawList* draw_list) {
    auto& state = m_controller.state();
    const auto& cr = state.custom_watermark.region;

    float x = static_cast<float>(cr.x);
    float y = static_cast<float>(cr.y);
    float w = static_cast<float>(cr.width);
    float h = static_cast<float>(cr.height);

    ImVec2 tl = image_to_screen(x, y);
    ImVec2 br = image_to_screen(x + w, y + h);
    ImVec2 tr = ImVec2(br.x, tl.y);
    ImVec2 bl = ImVec2(tl.x, br.y);
    ImVec2 tc = ImVec2((tl.x + br.x) * 0.5f, tl.y);
    ImVec2 bc = ImVec2((tl.x + br.x) * 0.5f, br.y);
    ImVec2 ml = ImVec2(tl.x, (tl.y + br.y) * 0.5f);
    ImVec2 mr = ImVec2(br.x, (tl.y + br.y) * 0.5f);

    // Color based on state
    ImU32 rect_color = state.custom_watermark.is_drawing
        ? IM_COL32(255, 255, 0, 200)      // Yellow while drawing
        : IM_COL32(0, 200, 255, 200);     // Cyan for custom region
    ImU32 fill_color = IM_COL32(0, 200, 255, 30);
    ImU32 anchor_color = IM_COL32(255, 255, 255, 255);
    ImU32 anchor_border = IM_COL32(0, 100, 200, 255);

    // Semi-transparent fill
    draw_list->AddRectFilled(tl, br, fill_color);

    // Border
    draw_list->AddRect(tl, br, rect_color, 0, 0, 2.0f);

    // Label
    const char* label = "Custom Watermark";
    draw_list->AddText(
        ImVec2(tl.x, tl.y - ImGui::GetTextLineHeight() - 2),
        rect_color, label
    );

    // Size label inside
    char size_text[64];
    snprintf(size_text, sizeof(size_text), "%dx%d", cr.width, cr.height);
    ImVec2 size_text_sz = ImGui::CalcTextSize(size_text);
    draw_list->AddText(
        ImVec2((tl.x + br.x - size_text_sz.x) * 0.5f,
               (tl.y + br.y - size_text_sz.y) * 0.5f),
        IM_COL32(255, 255, 255, 200), size_text
    );

    // Don't draw anchors while actively drawing
    if (state.custom_watermark.is_drawing) return;

    // Draw anchor points (8 points: corners + edge midpoints)
    auto draw_anchor = [&](ImVec2 pos) {
        draw_list->AddRectFilled(
            ImVec2(pos.x - ANCHOR_SIZE, pos.y - ANCHOR_SIZE),
            ImVec2(pos.x + ANCHOR_SIZE, pos.y + ANCHOR_SIZE),
            anchor_color
        );
        draw_list->AddRect(
            ImVec2(pos.x - ANCHOR_SIZE, pos.y - ANCHOR_SIZE),
            ImVec2(pos.x + ANCHOR_SIZE, pos.y + ANCHOR_SIZE),
            anchor_border, 0, 0, 1.0f
        );
    };

    draw_anchor(tl);
    draw_anchor(tc);
    draw_anchor(tr);
    draw_anchor(ml);
    draw_anchor(mr);
    draw_anchor(bl);
    draw_anchor(bc);
    draw_anchor(br);
}

// =============================================================================
// Anchor hit testing
// =============================================================================

AnchorPoint ImagePreview::hit_test_anchor(const ImVec2& mouse_pos, const cv::Rect& rect) const {
    float x = static_cast<float>(rect.x);
    float y = static_cast<float>(rect.y);
    float w = static_cast<float>(rect.width);
    float h = static_cast<float>(rect.height);

    struct AnchorDef {
        float ix, iy;
        AnchorPoint point;
    };

    AnchorDef anchors[] = {
        {x,           y,           AnchorPoint::TopLeft},
        {x + w * 0.5f, y,          AnchorPoint::Top},
        {x + w,       y,           AnchorPoint::TopRight},
        {x,           y + h * 0.5f, AnchorPoint::Left},
        {x + w,       y + h * 0.5f, AnchorPoint::Right},
        {x,           y + h,       AnchorPoint::BottomLeft},
        {x + w * 0.5f, y + h,      AnchorPoint::Bottom},
        {x + w,       y + h,       AnchorPoint::BottomRight},
    };

    float hit_radius = ANCHOR_HIT_RADIUS;

    for (const auto& a : anchors) {
        ImVec2 screen_pos = image_to_screen(a.ix, a.iy);
        float dx = mouse_pos.x - screen_pos.x;
        float dy = mouse_pos.y - screen_pos.y;
        if (dx * dx + dy * dy < hit_radius * hit_radius) {
            return a.point;
        }
    }

    // Test body (inside rect)
    ImVec2 tl_scr = image_to_screen(x, y);
    ImVec2 br_scr = image_to_screen(x + w, y + h);
    if (mouse_pos.x >= tl_scr.x && mouse_pos.x <= br_scr.x &&
        mouse_pos.y >= tl_scr.y && mouse_pos.y <= br_scr.y) {
        return AnchorPoint::Body;
    }

    return AnchorPoint::None;
}

// =============================================================================
// Custom rect interaction
// =============================================================================

void ImagePreview::handle_custom_rect_interaction() {
    auto& state = m_controller.state();
    auto& cw = state.custom_watermark;

    ImGuiIO& io = ImGui::GetIO();
    bool is_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

    // Skip if panning (space/alt/middle mouse)
    bool space_held = ImGui::IsKeyDown(ImGuiKey_Space);
    if (space_held || io.KeyAlt || ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        return;
    }

    ImVec2 mouse_pos = io.MousePos;
    ImVec2 image_pos = screen_to_image(mouse_pos.x, mouse_pos.y);

    if (cw.has_region && !cw.is_drawing) {
        // === Existing rect: handle resize/drag ===

        if (!cw.is_resizing) {
            // Hover: show appropriate cursor
            AnchorPoint hit = hit_test_anchor(mouse_pos, cw.region);

            switch (hit) {
                case AnchorPoint::TopLeft:
                case AnchorPoint::BottomRight:
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE); break;
                case AnchorPoint::TopRight:
                case AnchorPoint::BottomLeft:
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW); break;
                case AnchorPoint::Top:
                case AnchorPoint::Bottom:
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS); break;
                case AnchorPoint::Left:
                case AnchorPoint::Right:
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW); break;
                case AnchorPoint::Body:
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll); break;
                default: break;
            }

            // Start drag/resize on click
            if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hit != AnchorPoint::None) {
                cw.is_resizing = true;
                cw.active_anchor = hit;
                cw.drag_start = cv::Point(static_cast<int>(image_pos.x), static_cast<int>(image_pos.y));
                cw.drag_start_rect = cw.region;
            }
        }

        if (cw.is_resizing) {
            int dx = static_cast<int>(image_pos.x) - cw.drag_start.x;
            int dy = static_cast<int>(image_pos.y) - cw.drag_start.y;

            cv::Rect r = cw.drag_start_rect;

            switch (cw.active_anchor) {
                case AnchorPoint::Body:
                    r.x += dx; r.y += dy; break;
                case AnchorPoint::TopLeft:
                    r.x += dx; r.y += dy; r.width -= dx; r.height -= dy; break;
                case AnchorPoint::Top:
                    r.y += dy; r.height -= dy; break;
                case AnchorPoint::TopRight:
                    r.y += dy; r.width += dx; r.height -= dy; break;
                case AnchorPoint::Left:
                    r.x += dx; r.width -= dx; break;
                case AnchorPoint::Right:
                    r.width += dx; break;
                case AnchorPoint::BottomLeft:
                    r.x += dx; r.width -= dx; r.height += dy; break;
                case AnchorPoint::Bottom:
                    r.height += dy; break;
                case AnchorPoint::BottomRight:
                    r.width += dx; r.height += dy; break;
                default: break;
            }

            // Ensure minimum size
            if (r.width < 8) r.width = 8;
            if (r.height < 8) r.height = 8;

            // Clamp to image bounds
            r &= cv::Rect(0, 0, state.image.width, state.image.height);

            if (r.width >= 8 && r.height >= 8) {
                m_controller.set_custom_region(r);
            }

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                cw.is_resizing = false;
                cw.active_anchor = AnchorPoint::None;
            }
        }
    }

    // === Draw new rect ===
    if (is_hovered && !cw.is_resizing) {
        bool start_draw = false;

        if (!cw.has_region && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            start_draw = true;
        } else if (cw.has_region && io.KeyCtrl && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            AnchorPoint hit = hit_test_anchor(mouse_pos, cw.region);
            if (hit == AnchorPoint::None) {
                start_draw = true;
            }
        }

        if (start_draw) {
            cw.is_drawing = true;
            cw.drag_start = cv::Point(
                static_cast<int>(std::clamp(image_pos.x, 0.0f, static_cast<float>(state.image.width))),
                static_cast<int>(std::clamp(image_pos.y, 0.0f, static_cast<float>(state.image.height)))
            );
        }
    }

    if (cw.is_drawing) {
        int x1 = cw.drag_start.x;
        int y1 = cw.drag_start.y;
        int x2 = static_cast<int>(std::clamp(image_pos.x, 0.0f, static_cast<float>(state.image.width)));
        int y2 = static_cast<int>(std::clamp(image_pos.y, 0.0f, static_cast<float>(state.image.height)));

        int rx = std::min(x1, x2);
        int ry = std::min(y1, y2);
        int rw = std::abs(x2 - x1);
        int rh = std::abs(y2 - y1);

        if (rw >= 4 && rh >= 4) {
            cv::Rect new_rect(rx, ry, rw, rh);
            cw.region = new_rect;
            cw.has_region = true;
            m_controller.set_custom_region(new_rect);
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            cw.is_drawing = false;
            if (rw < 4 || rh < 4) {
                cw.has_region = false;
            }
        }
    }
}

// =============================================================================
// Input handling
// =============================================================================

void ImagePreview::handle_input(const ImVec2& viewport_size, float content_w, float content_h) {
    auto& state = m_controller.state();
    auto& opts = state.preview_options;

    ImGuiIO& io = ImGui::GetIO();

    bool is_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

    // Mouse wheel zoom
    if (is_hovered && io.MouseWheel != 0 && !io.KeyShift) {
        float zoom_delta = io.MouseWheel * 0.1f;
        opts.zoom = std::clamp(opts.zoom + zoom_delta * opts.zoom, 0.1f, 10.0f);
    }

    // Pan with Space + left mouse drag, middle mouse, or Alt + left mouse
    bool space_held = ImGui::IsKeyDown(ImGuiKey_Space);
    bool left_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool middle_down = ImGui::IsMouseDown(ImGuiMouseButton_Middle);

    // In custom mode, don't pan with plain left-click
    bool is_custom_interacting = (state.process_options.size_mode == WatermarkSizeMode::Custom) &&
                                  (state.custom_watermark.is_drawing || state.custom_watermark.is_resizing);

    bool pan_active = is_hovered && !is_custom_interacting && (
        middle_down ||
        (space_held && left_down) ||
        (io.KeyAlt && left_down)
    );

    if (pan_active) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);

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

    // Double-click to reset view (not in custom mode interaction)
    if (is_hovered && !is_custom_interacting &&
        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !io.KeyAlt && !space_held) {
        opts.reset_view();
        ImGui::SetScrollX(0);
        ImGui::SetScrollY(0);
    }

    // Arrow keys behavior:
    // - In Custom mode with region: move the custom region (1 pixel per press)
    // - Otherwise: pan the image
    bool custom_mode_with_region =
        (state.process_options.size_mode == WatermarkSizeMode::Custom) &&
        state.custom_watermark.has_region;

    if (custom_mode_with_region && is_hovered) {
        // Move custom region with arrow keys (use IsKeyPressed for single-step movement)
        cv::Rect region = state.custom_watermark.region;
        bool region_changed = false;

        // Hold Shift for faster movement (10 pixels)
        int step = io.KeyShift ? 10 : 1;
        
        if (ImGui::IsKeyPressed(ImGuiKey_A, true)) {
            region.x -= step;
            region_changed = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_D, true)) {
            region.x += step;
            region_changed = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_W, true)) {
            region.y -= step;
            region_changed = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_S, true)) {
            region.y += step;
            region_changed = true;
        }
        
        if (region_changed) {
            m_controller.set_custom_region(region);
        }
    }
}

}  // namespace gwt::gui
