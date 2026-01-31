/**
 * @file    style.hpp
 * @brief   ImGui Style Configuration
 * @author  AllenK (Kwyshell)
 * @license MIT
 *
 * @details
 * Defines the visual theme for the application.
 * Modern dark theme with accent colors.
 */

#pragma once

#include <imgui.h>

namespace gwt::gui {

/**
 * Apply the application's custom ImGui style
 */
inline void apply_style() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    // ==========================================================================
    // Sizing & Rounding
    // ==========================================================================
    style.WindowPadding = ImVec2(12, 12);
    style.FramePadding = ImVec2(8, 4);
    style.CellPadding = ImVec2(6, 4);
    style.ItemSpacing = ImVec2(8, 6);
    style.ItemInnerSpacing = ImVec2(6, 4);
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 12.0f;
    
    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;
    
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;
    
    // ==========================================================================
    // Color Palette - Modern Dark Theme
    // ==========================================================================
    
    // Background colors
    constexpr ImVec4 bg_dark    = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    constexpr ImVec4 bg_medium  = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    constexpr ImVec4 bg_light   = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
    
    // Border colors
    constexpr ImVec4 border     = ImVec4(0.28f, 0.28f, 0.30f, 1.00f);
    constexpr ImVec4 border_dim = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    
    // Text colors
    constexpr ImVec4 text       = ImVec4(0.92f, 0.92f, 0.94f, 1.00f);
    constexpr ImVec4 text_dim   = ImVec4(0.60f, 0.60f, 0.62f, 1.00f);
    
    // Accent colors (Teal/Cyan theme)
    constexpr ImVec4 accent     = ImVec4(0.26f, 0.75f, 0.82f, 1.00f);  // #42BFD1
    constexpr ImVec4 accent_dim = ImVec4(0.20f, 0.58f, 0.64f, 1.00f);
    constexpr ImVec4 accent_bg  = ImVec4(0.26f, 0.75f, 0.82f, 0.20f);
    
    // Status colors
    constexpr ImVec4 success    = ImVec4(0.40f, 0.80f, 0.40f, 1.00f);
    constexpr ImVec4 warning    = ImVec4(0.90f, 0.70f, 0.20f, 1.00f);
    constexpr ImVec4 error      = ImVec4(0.90f, 0.35f, 0.35f, 1.00f);
    
    // ==========================================================================
    // Apply Colors
    // ==========================================================================
    
    // Text
    colors[ImGuiCol_Text]                  = text;
    colors[ImGuiCol_TextDisabled]          = text_dim;
    
    // Backgrounds
    colors[ImGuiCol_WindowBg]              = bg_dark;
    colors[ImGuiCol_ChildBg]               = bg_medium;
    colors[ImGuiCol_PopupBg]               = bg_dark;
    
    // Borders
    colors[ImGuiCol_Border]                = border;
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    
    // Frame backgrounds (input fields, etc.)
    colors[ImGuiCol_FrameBg]               = bg_light;
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.22f, 0.22f, 0.24f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.26f, 0.26f, 0.28f, 1.00f);
    
    // Title bar
    colors[ImGuiCol_TitleBg]               = bg_dark;
    colors[ImGuiCol_TitleBgActive]         = bg_medium;
    colors[ImGuiCol_TitleBgCollapsed]      = bg_dark;
    
    // Menu bar
    colors[ImGuiCol_MenuBarBg]             = bg_dark;
    
    // Scrollbar
    colors[ImGuiCol_ScrollbarBg]           = bg_dark;
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.30f, 0.30f, 0.32f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.40f, 0.40f, 0.42f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = accent_dim;
    
    // Checkmark
    colors[ImGuiCol_CheckMark]             = accent;
    
    // Slider
    colors[ImGuiCol_SliderGrab]            = accent_dim;
    colors[ImGuiCol_SliderGrabActive]      = accent;
    
    // Buttons
    colors[ImGuiCol_Button]                = bg_light;
    colors[ImGuiCol_ButtonHovered]         = accent_dim;
    colors[ImGuiCol_ButtonActive]          = accent;
    
    // Headers (collapsing headers, tree nodes, etc.)
    colors[ImGuiCol_Header]                = accent_bg;
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.26f, 0.75f, 0.82f, 0.40f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.26f, 0.75f, 0.82f, 0.60f);
    
    // Separator
    colors[ImGuiCol_Separator]             = border_dim;
    colors[ImGuiCol_SeparatorHovered]      = accent_dim;
    colors[ImGuiCol_SeparatorActive]       = accent;
    
    // Resize grip
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.26f, 0.75f, 0.82f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.26f, 0.75f, 0.82f, 0.60f);
    colors[ImGuiCol_ResizeGripActive]      = accent;
    
    // Tabs
    colors[ImGuiCol_Tab]                   = bg_medium;
    colors[ImGuiCol_TabHovered]            = accent_dim;
    colors[ImGuiCol_TabActive]             = accent_bg;
    colors[ImGuiCol_TabUnfocused]          = bg_dark;
    colors[ImGuiCol_TabUnfocusedActive]    = bg_medium;
    
    // Plot colors
    colors[ImGuiCol_PlotLines]             = accent;
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.60f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]         = accent;
    colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.35f, 1.00f);
    
    // Table colors
    colors[ImGuiCol_TableHeaderBg]         = bg_medium;
    colors[ImGuiCol_TableBorderStrong]     = border;
    colors[ImGuiCol_TableBorderLight]      = border_dim;
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
    
    // Text selection
    colors[ImGuiCol_TextSelectedBg]        = accent_bg;
    
    // Drag/Drop
    colors[ImGuiCol_DragDropTarget]        = accent;
    
    // Nav highlight
    colors[ImGuiCol_NavHighlight]          = accent;
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    
    // Modal dim
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
}

/**
 * Get status color based on state
 */
inline ImVec4 get_status_color(bool success) {
    if (success) {
        return ImVec4(0.40f, 0.80f, 0.40f, 1.00f);  // Green
    }
    return ImVec4(0.90f, 0.35f, 0.35f, 1.00f);      // Red
}

/**
 * Get accent color
 */
inline ImVec4 get_accent_color() {
    return ImVec4(0.26f, 0.75f, 0.82f, 1.00f);
}

}  // namespace gwt::gui
