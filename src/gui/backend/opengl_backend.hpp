/**
 * @file    opengl_backend.hpp
 * @brief   OpenGL Render Backend
 * @author  AllenK (Kwyshell)
 * @license MIT
 */

#pragma once

#include "gui/backend/render_backend.hpp"

#include <SDL3/SDL.h>

#include <unordered_map>
#include <cstdint>

namespace gwt::gui {

class OpenGLBackend final : public IRenderBackend {
public:
    OpenGLBackend() = default;
    ~OpenGLBackend() override;

    // ==========================================================================
    // Lifecycle
    // ==========================================================================
    
    [[nodiscard]] bool init(SDL_Window* window) override;
    
    void shutdown() override;

    // ==========================================================================
    // ImGui Integration
    // ==========================================================================
    
    void imgui_init() override;
    void imgui_shutdown() override;
    void imgui_new_frame() override;
    void imgui_render() override;

    // ==========================================================================
    // Frame Management
    // ==========================================================================
    
    void begin_frame() override;
    void end_frame() override;
    void present() override;
    void on_resize(int width, int height) override;

    // ==========================================================================
    // Texture Operations
    // ==========================================================================
    
    [[nodiscard]] TextureHandle
    create_texture(const TextureDesc& desc, std::span<const uint8_t> data = {}) override;
    
    void update_texture(TextureHandle handle, std::span<const uint8_t> data) override;
    void destroy_texture(TextureHandle handle) override;
    
    [[nodiscard]] void* get_imgui_texture_id(TextureHandle handle) const override;

    // ==========================================================================
    // Backend Info
    // ==========================================================================
    
    [[nodiscard]] std::string_view name() const noexcept override;
    [[nodiscard]] BackendType type() const noexcept override { return BackendType::OpenGL; }
    [[nodiscard]] bool supports_compute() const noexcept override { return false; }

private:
    struct TextureData {
        uint32_t gl_id{0};
        TextureDesc desc;
    };

    SDL_Window* m_window{nullptr};
    SDL_GLContext m_gl_context{nullptr};
    
    std::unordered_map<uint64_t, TextureData> m_textures;
    uint64_t m_next_handle_id{1};
    
    int m_window_width{0};
    int m_window_height{0};
    bool m_initialized{false};

    // Helper to convert TextureFormat to OpenGL format
    static std::pair<uint32_t, uint32_t> get_gl_format(TextureFormat format);
};

}  // namespace gwt::gui
