/**
 * @file    opengl_backend.cpp
 * @brief   OpenGL Render Backend Implementation
 * @author  AllenK (Kwyshell)
 * @license MIT
 */

#include "gui/backend/opengl_backend.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

// Use ImGui's built-in OpenGL loader
#if defined(IMGUI_IMPL_OPENGL_ES2)
    #include <GLES2/gl2.h>
#elif defined(IMGUI_IMPL_OPENGL_ES3)
    #include <GLES3/gl3.h>
#elif defined(__APPLE__)
    #define GL_SILENCE_DEPRECATION
    #include <OpenGL/gl3.h>
#else
    // For desktop OpenGL, we need glad or similar
    // ImGui's OpenGL3 backend includes its own loader
    #include <glad/glad.h>
#endif

#include <spdlog/spdlog.h>

namespace gwt::gui {

// =============================================================================
// Lifecycle
// =============================================================================

OpenGLBackend::~OpenGLBackend() {
    if (m_initialized) {
        shutdown();
    }
}

bool OpenGLBackend::init(SDL_Window* window) {
    if (m_initialized) {
        spdlog::warn("OpenGL backend already initialized");
        return true;
    }

    if (!window) {
        spdlog::error("Null window provided to OpenGL backend");
        set_error(BackendError::InitFailed);
        return false;
    }

    m_window = window;

    // Set OpenGL attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // Create OpenGL context
    m_gl_context = SDL_GL_CreateContext(window);
    if (!m_gl_context) {
        spdlog::error("Failed to create OpenGL context: {}", SDL_GetError());
        set_error(BackendError::ContextCreationFailed);
        return false;
    }

    SDL_GL_MakeCurrent(window, m_gl_context);
    SDL_GL_SetSwapInterval(1);  // Enable vsync

    // Initialize GLAD (load OpenGL functions)
#if !defined(__APPLE__)
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        spdlog::error("Failed to initialize GLAD");
        SDL_GL_DestroyContext(m_gl_context);
        m_gl_context = nullptr;
        set_error(BackendError::InitFailed);
        return false;
    }
#endif

    // Get window size
    SDL_GetWindowSize(window, &m_window_width, &m_window_height);

    // Log OpenGL info
    spdlog::info("OpenGL initialized:");
    spdlog::info("  Vendor: {}", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
    spdlog::info("  Renderer: {}", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    spdlog::info("  Version: {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));

    m_initialized = true;
    clear_error();
    return true;
}

void OpenGLBackend::shutdown() {
    if (!m_initialized) return;

    // Destroy all textures
    for (auto& [id, tex] : m_textures) {
        if (tex.gl_id) {
            glDeleteTextures(1, &tex.gl_id);
        }
    }
    m_textures.clear();

    // Destroy OpenGL context
    if (m_gl_context) {
        SDL_GL_DestroyContext(m_gl_context);
        m_gl_context = nullptr;
    }

    m_window = nullptr;
    m_initialized = false;
    
    spdlog::debug("OpenGL backend shutdown complete");
}

// =============================================================================
// ImGui Integration
// =============================================================================

void OpenGLBackend::imgui_init() {
    if (!m_initialized) return;

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(m_window, m_gl_context);
    
    // Use GLSL version based on platform
#if defined(__APPLE__)
    const char* glsl_version = "#version 150";
#else
    const char* glsl_version = "#version 130";
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    spdlog::debug("ImGui OpenGL backend initialized");
}

void OpenGLBackend::imgui_shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
}

void OpenGLBackend::imgui_new_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
}

void OpenGLBackend::imgui_render() {
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// =============================================================================
// Frame Management
// =============================================================================

void OpenGLBackend::begin_frame() {
    // Set viewport
    glViewport(0, 0, m_window_width, m_window_height);
    
    // Clear screen
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLBackend::end_frame() {
    // Nothing special needed for OpenGL
}

void OpenGLBackend::present() {
    SDL_GL_SwapWindow(m_window);
}

void OpenGLBackend::on_resize(int width, int height) {
    m_window_width = width;
    m_window_height = height;
    glViewport(0, 0, width, height);
}

// =============================================================================
// Texture Operations
// =============================================================================

std::pair<uint32_t, uint32_t> OpenGLBackend::get_gl_format(TextureFormat format) {
    switch (format) {
        case TextureFormat::RGB8:
            return {GL_RGB, GL_RGB};
        case TextureFormat::RGBA8:
            return {GL_RGBA, GL_RGBA};
        case TextureFormat::BGR8:
            return {GL_RGB, GL_BGR};
        case TextureFormat::BGRA8:
            return {GL_RGBA, GL_BGRA};
        default:
            return {GL_RGBA, GL_RGBA};
    }
}

TextureHandle
OpenGLBackend::create_texture(const TextureDesc& desc, std::span<const uint8_t> data) {
    if (!m_initialized) {
        set_error(BackendError::InitFailed);
        return TextureHandle{};  // Invalid handle
    }

    // Create OpenGL texture
    uint32_t gl_id = 0;
    glGenTextures(1, &gl_id);
    
    if (!gl_id) {
        spdlog::error("Failed to create OpenGL texture");
        set_error(BackendError::TextureCreationFailed);
        return TextureHandle{};  // Invalid handle
    }

    glBindTexture(GL_TEXTURE_2D, gl_id);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Get format
    auto [internal_format, pixel_format] = get_gl_format(desc.format);

    // Upload data (or allocate empty texture)
    const void* pixel_data = data.empty() ? nullptr : data.data();
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, 
                 desc.width, desc.height, 0,
                 pixel_format, GL_UNSIGNED_BYTE, pixel_data);

    // Generate mipmaps if requested
    if (desc.generate_mips) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    // Create handle
    TextureHandle handle{m_next_handle_id++};
    m_textures[handle.id] = TextureData{gl_id, desc};

    spdlog::debug("Created texture {} ({}x{}, GL ID: {})", 
                  handle.id, desc.width, desc.height, gl_id);

    clear_error();
    return handle;
}

void OpenGLBackend::update_texture(TextureHandle handle, std::span<const uint8_t> data) {
    auto it = m_textures.find(handle.id);
    if (it == m_textures.end()) {
        spdlog::warn("Attempted to update invalid texture handle: {}", handle.id);
        return;
    }

    auto& tex = it->second;
    auto [internal_format, pixel_format] = get_gl_format(tex.desc.format);

    glBindTexture(GL_TEXTURE_2D, tex.gl_id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    tex.desc.width, tex.desc.height,
                    pixel_format, GL_UNSIGNED_BYTE, data.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLBackend::destroy_texture(TextureHandle handle) {
    auto it = m_textures.find(handle.id);
    if (it == m_textures.end()) {
        return;
    }

    if (it->second.gl_id) {
        glDeleteTextures(1, &it->second.gl_id);
    }
    m_textures.erase(it);

    spdlog::debug("Destroyed texture {}", handle.id);
}

void* OpenGLBackend::get_imgui_texture_id(TextureHandle handle) const {
    auto it = m_textures.find(handle.id);
    if (it == m_textures.end()) {
        return nullptr;
    }
    
    // ImGui expects texture ID as void* (intptr_t cast)
    return reinterpret_cast<void*>(static_cast<intptr_t>(it->second.gl_id));
}

// =============================================================================
// Backend Info
// =============================================================================

std::string_view OpenGLBackend::name() const noexcept {
    return "OpenGL 3.3 Core";
}

}  // namespace gwt::gui
