/**
 * @file    d3d11_backend.hpp
 * @brief   Direct3D 11 Render Backend
 * @author  AllenK (Kwyshell)
 * @license MIT
 *
 * @details
 * D3D11 backend for Windows platforms. Provides better compatibility
 * with virtualized environments (Hyper-V, Remote Desktop) compared to OpenGL.
 * Uses WIL (Windows Implementation Libraries) for modern COM pointer management.
 */

#pragma once

#include "gui/backend/render_backend.hpp"

#ifdef _WIN32

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wil/com.h>

#include <unordered_map>
#include <cstdint>

// Forward declaration
struct SDL_Window;

namespace gwt::gui {

class D3D11Backend final : public IRenderBackend {
public:
    D3D11Backend() = default;
    ~D3D11Backend() override;

    // ==========================================================================
    // Static availability check
    // ==========================================================================
    
    /**
     * Check if D3D11 is available on this system
     * @return true if D3D11 can be used
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
    [[nodiscard]] BackendType type() const noexcept override { return BackendType::D3D11; }
    [[nodiscard]] bool supports_compute() const noexcept override { return true; }

private:
    // Helper to create render target view
    bool create_render_target();
    void cleanup_render_target();

    // Helper to convert TextureFormat to DXGI format
    static DXGI_FORMAT get_dxgi_format(TextureFormat format);
    static uint32_t get_bytes_per_pixel(TextureFormat format);

    struct TextureData {
        wil::com_ptr<ID3D11Texture2D> texture;
        wil::com_ptr<ID3D11ShaderResourceView> srv;
        TextureDesc desc;
    };

    // DXGI objects - factory kept alive for consistent swap chain operations
    wil::com_ptr<IDXGIFactory2> m_factory;
    wil::com_ptr<IDXGIAdapter1> m_adapter;

    // D3D11 objects (using wil::com_ptr for automatic release)
    wil::com_ptr<ID3D11Device> m_device;
    wil::com_ptr<ID3D11DeviceContext> m_context;
    wil::com_ptr<IDXGISwapChain1> m_swap_chain;
    wil::com_ptr<ID3D11RenderTargetView> m_rtv;

    SDL_Window* m_window{nullptr};
    HWND m_hwnd{nullptr};
    
    std::unordered_map<uint64_t, TextureData> m_textures;
    uint64_t m_next_handle_id{1};
    
    int m_window_width{0};
    int m_window_height{0};
    bool m_initialized{false};
    bool m_using_warp{false};
    bool m_in_resize{false};  // Set during resize → present without vsync
    
    // Feature level info
    D3D_FEATURE_LEVEL m_feature_level{D3D_FEATURE_LEVEL_11_0};
};

}  // namespace gwt::gui

#endif  // _WIN32
