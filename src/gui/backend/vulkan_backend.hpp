/**
 * @file    vulkan_backend.hpp
 * @brief   Vulkan Render Backend (Optional)
 * @author  AllenK (Kwyshell)
 * @license MIT
 */

#pragma once

#if defined(GWT_HAS_VULKAN)

#include "gui/backend/render_backend.hpp"

#include <unordered_map>
#include <cstdint>

// Forward declarations
struct SDL_Window;

namespace gwt::gui {

class VulkanBackend final : public IRenderBackend {
public:
    VulkanBackend() = default;
    ~VulkanBackend() override;

    /**
     * Check if Vulkan runtime is available on this system
     */
    [[nodiscard]] static bool is_available() noexcept;

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
    [[nodiscard]] BackendType type() const noexcept override { return BackendType::Vulkan; }
    [[nodiscard]] bool supports_compute() const noexcept override { return true; }

private:
    // TODO: Vulkan handles and resources
    // VkInstance m_instance{VK_NULL_HANDLE};
    // VkDevice m_device{VK_NULL_HANDLE};
    // VkPhysicalDevice m_physical_device{VK_NULL_HANDLE};
    // ...

    SDL_Window* m_window{nullptr};
    bool m_initialized{false};
    
    std::unordered_map<uint64_t, void*> m_textures;  // TODO: proper texture struct
    uint64_t m_next_handle_id{1};
};

}  // namespace gwt::gui

#endif  // GWT_HAS_VULKAN
