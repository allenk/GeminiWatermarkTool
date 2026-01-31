/**
 * @file    render_backend.cpp
 * @brief   Render Backend Factory Implementation
 * @author  AllenK (Kwyshell)
 * @license MIT
 */

#include "gui/backend/render_backend.hpp"
#include "gui/backend/opengl_backend.hpp"

#if defined(GWT_HAS_VULKAN)
#include "gui/backend/vulkan_backend.hpp"
#endif

#include <spdlog/spdlog.h>

namespace gwt::gui {

std::unique_ptr<IRenderBackend> create_backend(BackendType type) {
    // Auto mode: try backends in order of preference
    if (type == BackendType::Auto) {
#if defined(GWT_HAS_VULKAN)
        // Try Vulkan first
        if (is_backend_available(BackendType::Vulkan)) {
            spdlog::info("Auto-selecting Vulkan backend");
            auto backend = std::make_unique<VulkanBackend>();
            return backend;
        }
        spdlog::debug("Vulkan not available, trying OpenGL");
#endif
        // Fall back to OpenGL
        type = BackendType::OpenGL;
    }

    // Create specific backend
    switch (type) {
        case BackendType::OpenGL:
            spdlog::info("Creating OpenGL backend");
            return std::make_unique<OpenGLBackend>();

#if defined(GWT_HAS_VULKAN)
        case BackendType::Vulkan:
            spdlog::info("Creating Vulkan backend");
            return std::make_unique<VulkanBackend>();
#endif

        default:
            spdlog::error("Unknown backend type requested");
            return nullptr;
    }
}

bool is_backend_available(BackendType type) noexcept {
    switch (type) {
        case BackendType::OpenGL:
            // OpenGL is always available (compiled in)
            return true;

#if defined(GWT_HAS_VULKAN)
        case BackendType::Vulkan:
            // Check if Vulkan runtime is available
            return VulkanBackend::is_available();
#endif

        case BackendType::Auto:
            // Auto is always "available" (will fall back)
            return true;

        default:
            return false;
    }
}

}  // namespace gwt::gui
