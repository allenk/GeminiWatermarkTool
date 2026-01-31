/**
 * @file    render_backend.hpp
 * @brief   Abstract Render Backend Interface
 * @author  AllenK (Kwyshell)
 * @license MIT
 *
 * @details
 * Defines the interface for render backends (OpenGL, Vulkan).
 * Allows GUI to be decoupled from specific graphics API.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

// Forward declarations
struct SDL_Window;
union SDL_Event;

namespace gwt::gui {

// =============================================================================
// Error Types
// =============================================================================

enum class BackendError {
    None,
    InitFailed,
    WindowCreationFailed,
    ContextCreationFailed,
    TextureCreationFailed,
    TextureUpdateFailed,
    ShaderCompileFailed,
    DeviceLost
};

[[nodiscard]] constexpr std::string_view to_string(BackendError error) noexcept {
    switch (error) {
        case BackendError::None:                  return "No error";
        case BackendError::InitFailed:            return "Initialization failed";
        case BackendError::WindowCreationFailed:  return "Window creation failed";
        case BackendError::ContextCreationFailed: return "Context creation failed";
        case BackendError::TextureCreationFailed: return "Texture creation failed";
        case BackendError::TextureUpdateFailed:   return "Texture update failed";
        case BackendError::ShaderCompileFailed:   return "Shader compilation failed";
        case BackendError::DeviceLost:            return "Device lost";
        default:                                  return "Unknown error";
    }
}

// =============================================================================
// Texture Handle
// =============================================================================

struct TextureHandle {
    uint64_t id{0};

    [[nodiscard]] constexpr bool valid() const noexcept { return id != 0; }
    explicit constexpr operator bool() const noexcept { return valid(); }

    constexpr bool operator==(const TextureHandle&) const noexcept = default;
};

// =============================================================================
// Texture Description
// =============================================================================

enum class TextureFormat {
    RGB8,
    RGBA8,
    BGR8,
    BGRA8
};

struct TextureDesc {
    uint32_t width{0};
    uint32_t height{0};
    TextureFormat format{TextureFormat::RGBA8};
    bool generate_mips{false};
};

// =============================================================================
// Backend Type
// =============================================================================

enum class BackendType {
    OpenGL,
#if defined(GWT_HAS_VULKAN)
    Vulkan,
#endif
    Auto  // Auto-select best available
};

[[nodiscard]] constexpr std::string_view to_string(BackendType type) noexcept {
    switch (type) {
        case BackendType::OpenGL: return "OpenGL";
#if defined(GWT_HAS_VULKAN)
        case BackendType::Vulkan: return "Vulkan";
#endif
        case BackendType::Auto:   return "Auto";
        default:                  return "Unknown";
    }
}

// =============================================================================
// Render Backend Interface
// =============================================================================

class IRenderBackend {
public:
    virtual ~IRenderBackend() = default;

    // Non-copyable
    IRenderBackend(const IRenderBackend&) = delete;
    IRenderBackend& operator=(const IRenderBackend&) = delete;

    // ==========================================================================
    // Lifecycle
    // ==========================================================================

    /**
     * Initialize the backend with an existing SDL window
     * @param window  SDL window (must be valid)
     * @return        true on success, false on failure (check last_error())
     */
    [[nodiscard]] virtual bool init(SDL_Window* window) = 0;

    /**
     * Shutdown and cleanup resources
     */
    virtual void shutdown() = 0;

    // ==========================================================================
    // ImGui Integration
    // ==========================================================================

    /**
     * Initialize ImGui for this backend
     */
    virtual void imgui_init() = 0;

    /**
     * Shutdown ImGui
     */
    virtual void imgui_shutdown() = 0;

    /**
     * Begin ImGui frame
     */
    virtual void imgui_new_frame() = 0;

    /**
     * Render ImGui draw data
     */
    virtual void imgui_render() = 0;

    // ==========================================================================
    // Frame Management
    // ==========================================================================

    /**
     * Begin a new frame
     */
    virtual void begin_frame() = 0;

    /**
     * End current frame
     */
    virtual void end_frame() = 0;

    /**
     * Present the frame to screen
     */
    virtual void present() = 0;

    /**
     * Handle window resize
     */
    virtual void on_resize(int width, int height) = 0;

    // ==========================================================================
    // Texture Operations
    // ==========================================================================

    /**
     * Create a texture from pixel data
     * @param desc  Texture description
     * @param data  Pixel data (can be empty for uninitialized texture)
     * @return      Texture handle (invalid handle on failure, check last_error())
     */
    [[nodiscard]] virtual TextureHandle
    create_texture(const TextureDesc& desc, std::span<const uint8_t> data = {}) = 0;

    /**
     * Update texture data
     * @param handle  Texture handle
     * @param data    New pixel data
     */
    virtual void update_texture(TextureHandle handle, std::span<const uint8_t> data) = 0;

    /**
     * Destroy a texture
     * @param handle  Texture handle
     */
    virtual void destroy_texture(TextureHandle handle) = 0;

    /**
     * Get ImGui-compatible texture ID
     * @param handle  Texture handle
     * @return        Pointer suitable for ImGui::Image()
     */
    [[nodiscard]] virtual void* get_imgui_texture_id(TextureHandle handle) const = 0;

    // ==========================================================================
    // Backend Info
    // ==========================================================================

    /**
     * Get backend name
     */
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    /**
     * Get backend type
     */
    [[nodiscard]] virtual BackendType type() const noexcept = 0;

    /**
     * Check if backend supports compute shaders
     */
    [[nodiscard]] virtual bool supports_compute() const noexcept = 0;

    /**
     * Get last error that occurred
     */
    [[nodiscard]] BackendError last_error() const noexcept { return m_last_error; }

protected:
    IRenderBackend() = default;
    IRenderBackend(IRenderBackend&&) = default;
    IRenderBackend& operator=(IRenderBackend&&) = default;

    void set_error(BackendError error) noexcept { m_last_error = error; }
    void clear_error() noexcept { m_last_error = BackendError::None; }

    BackendError m_last_error{BackendError::None};
};

// =============================================================================
// Factory Function
// =============================================================================

/**
 * Create a render backend
 * @param type  Backend type (Auto will try Vulkan first if available)
 * @return      Unique pointer to backend instance
 */
[[nodiscard]] std::unique_ptr<IRenderBackend>
create_backend(BackendType type = BackendType::Auto);

/**
 * Check if a backend type is available
 * @param type  Backend type to check
 * @return      true if backend can be created
 */
[[nodiscard]] bool is_backend_available(BackendType type) noexcept;

}  // namespace gwt::gui
