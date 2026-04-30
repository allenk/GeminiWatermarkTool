cmake_minimum_required(VERSION 3.21)
project(GeminiWatermarkTool VERSION 0.2.8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# Build Options
# =============================================================================
option(BUILD_GUI "Build with ImGui GUI support" OFF)
option(ENABLE_D3D11 "Enable Direct3D 11 backend (Windows only, requires BUILD_GUI)" OFF)
option(ENABLE_VULKAN "Enable Vulkan backend (requires BUILD_GUI)" OFF)
option(ENABLE_AI_DENOISE "Enable AI denoising for residual cleanup (NCNN + Vulkan GPU)" OFF)

# Validate options
if(ENABLE_D3D11 AND NOT BUILD_GUI)
    message(FATAL_ERROR "ENABLE_D3D11 requires BUILD_GUI=ON")
endif()

if(ENABLE_D3D11 AND NOT WIN32)
    message(FATAL_ERROR "ENABLE_D3D11 is only supported on Windows")
endif()

if(ENABLE_VULKAN AND NOT BUILD_GUI)
    message(FATAL_ERROR "ENABLE_VULKAN requires BUILD_GUI=ON")
endif()

# =============================================================================
# Application Metadata
# =============================================================================
set(APP_PRODUCT_NAME "Gemini Watermark Tool")
set(APP_DESCRIPTION "Add/Remove Gemini-style visible watermarks")
set(APP_COMPANY "Kwyshell")
set(APP_COPYRIGHT "Copyright (C) 2025 Allen Kuo. MIT License.")

# =============================================================================
# Platform Detection
# =============================================================================
if(WIN32)
    set(PLATFORM_NAME "Windows")
elseif(APPLE)
    set(PLATFORM_NAME "macOS")
elseif(ANDROID)
    set(PLATFORM_NAME "Android")
elseif(UNIX)
    set(PLATFORM_NAME "Linux")
else()
    set(PLATFORM_NAME "Unknown")
endif()

# =============================================================================
# Windows Configuration
# =============================================================================
if(WIN32)
    set(APP_ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico")

    if(EXISTS "${APP_ICON_PATH}")
        # Generate .rc from template
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc.in"
            "${CMAKE_CURRENT_BINARY_DIR}/resources/app.rc"
            @ONLY
        )
        set(WIN_RESOURCE_FILE "${CMAKE_CURRENT_BINARY_DIR}/resources/app.rc")
        message(STATUS "Windows resource file configured with icon: ${APP_ICON_PATH}")
    else()
        message(STATUS "No icon found at ${APP_ICON_PATH}, skipping resource file")
        set(WIN_RESOURCE_FILE "")
    endif()

    # MSVC specific settings
    if(MSVC)
        # Use static runtime library (/MT instead of /MD)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

        # Compiler flags
        add_compile_options(/W4 /utf-8)

        # Release optimizations
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /GL /Gy")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG /OPT:REF /OPT:ICF")
    endif()

    # GUI: Use WIN32 subsystem (no console window)
    #if(BUILD_GUI)
    #    set(WIN32_EXECUTABLE WIN32)
    #endif()
endif()

# =============================================================================
# macOS Configuration
# =============================================================================
if(APPLE)
    # macOS minimum deployment target
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

    # Support both Intel and Apple Silicon (Universal Binary)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures")

    # Compiler warnings
    add_compile_options(-Wall -Wextra -Wpedantic)

    # Release optimizations
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -DNDEBUG)
    endif()

    # Link against macOS frameworks if needed
    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    if(COREFOUNDATION_LIBRARY)
        message(STATUS "Found CoreFoundation: ${COREFOUNDATION_LIBRARY}")
    endif()
endif()

# =============================================================================
# Linux Configuration
# =============================================================================
if(UNIX AND NOT APPLE AND NOT ANDROID)
    # Compiler warnings
    add_compile_options(-Wall -Wextra -Wpedantic)

    # Release optimizations
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -DNDEBUG)
    endif()

    # Static linking options (for standalone distribution)
    # Try to link statically where possible
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ".so")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")

    # Required for std::filesystem on older GCC
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
        link_libraries(stdc++fs)
    endif()
endif()

# =============================================================================
# Android Configuration
# =============================================================================
if(ANDROID)
    # Android specific settings
    message(STATUS "Building for Android: ABI=${ANDROID_ABI}, Platform=${ANDROID_PLATFORM}")

    # Compiler warnings
    add_compile_options(-Wall -Wextra)

    # Release optimizations
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -DNDEBUG)
    endif()

    # Android uses Bionic libc, no need for special filesystem handling
endif()

# =============================================================================
# Dependencies (via vcpkg)
# =============================================================================
find_package(OpenCV REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# GUI Dependencies
if(BUILD_GUI)
    find_package(imgui CONFIG REQUIRED)
    find_package(SDL3 CONFIG REQUIRED)
    find_package(implot CONFIG REQUIRED)
    find_package(OpenGL REQUIRED)
    find_package(glad CONFIG REQUIRED)
    find_package(nfd CONFIG REQUIRED)

    if(ENABLE_VULKAN)
        find_package(Vulkan REQUIRED)
        find_package(VulkanMemoryAllocator CONFIG REQUIRED)
    endif()

    # D3D11 uses Windows SDK, no additional find_package needed
endif()

# =============================================================================
# NCNN (AI Denoise - build from source, static library)
#
# Architecture:
#   - NCNN static lib with Vulkan GPU acceleration
#   - volk provides dynamic Vulkan loading (no hard link to vulkan-1.lib)
#   - glslang from vcpkg for runtime SPIR-V shader compilation
#   - simplevk.h hack: #define NCNN_SIMPLEVK_H to skip NCNN's built-in
#     Vulkan loader in our headers, volk takes over instead
#   - Model weights embedded via .mem.h (compiled into exe, ~1.3 MB)
# =============================================================================
if(ENABLE_AI_DENOISE)

    # OpenMP disabled — MSVC vcomp140.dll cannot be statically linked.
    # Enabling OpenMP would require users to install VC++ Redistributable.
    # NCNN uses Vulkan GPU for inference; OpenMP only affects CPU fallback path.
    # Uncomment below to re-enable (adds ~10% CPU fallback speed, but requires vcomp140.dll):
    #
    # find_package(OpenMP)
    # if(OPENMP_FOUND)
    #     set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    #     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    #     set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
    # endif()

    set(NCNN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/ncnn/ncnn-20260113-src")

    if(NOT EXISTS "${NCNN_ROOT}/CMakeLists.txt")
        message(FATAL_ERROR
            "NCNN source not found at: ${NCNN_ROOT}\n"
            "Please extract NCNN 20260113 source to external/ncnn/ncnn-20260113-src/")
    endif()

    message(STATUS "Building NCNN from source: ${NCNN_ROOT}")

    # -------------------------------------------------------------------------
    # NCNN build configuration
    # -------------------------------------------------------------------------

    # Static library (embedded into exe, no ncnn.dll needed)
    set(NCNN_SHARED_LIB OFF CACHE BOOL "" FORCE)

    # MSVC compatibility (NCNN defaults to disabling RTTI/exceptions)
    if(MSVC)
        set(NCNN_DISABLE_RTTI OFF CACHE BOOL "" FORCE)
        set(NCNN_DISABLE_EXCEPTION OFF CACHE BOOL "" FORCE)
    endif()

    # find_package(glslang CONFIG REQUIRED)
    # set(NCNN_SYSTEM_GLSLANG ON CACHE BOOL "" FORCE)

    # Vulkan GPU acceleration (dynamic loading via NCNN's internal simplevk)
    set(NCNN_VULKAN ON CACHE BOOL "" FORCE)
    set(NCNN_VULKAN_ONLINE_SPIRV OFF CACHE BOOL "" FORCE)

    # Don't build NCNN tools/tests/examples (we only need the library)
    set(NCNN_INSTALL_SDK OFF CACHE BOOL "" FORCE)
    set(NCNN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(NCNN_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(NCNN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(NCNN_BENCHMARK OFF CACHE BOOL "" FORCE)
    set(NCNN_BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)

    # Performance options
    set(NCNN_ENABLE_LTO ON CACHE BOOL "" FORCE)
    set(NCNN_OPENMP OFF CACHE BOOL "" FORCE)

    # -------------------------------------------------------------------------
    # Minimal layer set for FDnCNN
    #
    # FDnCNN architecture: 20x Conv2d with ReLU activation (no BN)
    # Only needs: Convolution, ReLU, and NCNN infrastructure layers
    # -------------------------------------------------------------------------
    set(WITH_LAYER_convolution ON CACHE BOOL "" FORCE)
    set(WITH_LAYER_relu ON CACHE BOOL "" FORCE)
    set(WITH_LAYER_input ON CACHE BOOL "" FORCE)
    set(WITH_LAYER_concat ON CACHE BOOL "" FORCE)
    set(WITH_LAYER_split ON CACHE BOOL "" FORCE)
    set(WITH_LAYER_packing ON CACHE BOOL "" FORCE)
    set(WITH_LAYER_cast ON CACHE BOOL "" FORCE)
    set(WITH_LAYER_flatten ON CACHE BOOL "" FORCE)
    set(WITH_LAYER_padding ON CACHE BOOL "" FORCE)
    set(WITH_LAYER_binaryop ON CACHE BOOL "" FORCE)

    # Future: enable for FFDNet / larger models
    # set(WITH_LAYER_pixelshuffle ON CACHE BOOL "" FORCE)
    # set(WITH_LAYER_batchnorm ON CACHE BOOL "" FORCE)
    # set(WITH_LAYER_deconvolution ON CACHE BOOL "" FORCE)

    # -------------------------------------------------------------------------
    # Build NCNN
    # -------------------------------------------------------------------------
    add_subdirectory(${NCNN_ROOT} ncnn)

    # Keep NCNN build outputs alongside main binary
    set_target_properties(ncnn PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    # Volk: meta-loader for Vulkan (provides function declarations
    # without hard-linking vulkan-1.lib; pairs with simplevk.h hack)
    find_package(volk CONFIG REQUIRED)

    message(STATUS "NCNN: Static library, Vulkan GPU, FDnCNN layers")
endif()

# =============================================================================
# Source Files
# =============================================================================

# Core sources (always built)
set(CORE_SOURCES
    src/core/watermark_engine.cpp
    src/core/blend_modes.cpp
    src/core/watermark_detector.cpp
)

set(CORE_HEADERS
    src/core/watermark_engine.hpp
    src/core/blend_modes.hpp
    src/core/watermark_detector.hpp
    src/core/types.hpp
)

# AI Denoise sources (conditional on ENABLE_AI_DENOISE)
if(ENABLE_AI_DENOISE)
    list(APPEND CORE_SOURCES
        src/core/ai_denoise.cpp
        src/core/ai_denoise_model.cpp   # Isolated TU for .mem.h (heavy header)
    )
    list(APPEND CORE_HEADERS
        src/core/ai_denoise.hpp
        src/core/ncnn_shim.hpp
    )
endif()

# CLI sources (always built)
set(CLI_SOURCES
    src/cli/cli_app.cpp
)

set(CLI_HEADERS
    src/cli/cli_app.hpp
)

# Utils headers
set(UTILS_HEADERS
    src/utils/ascii_logo.hpp
    src/utils/path_formatter.hpp
)

# Main entry point
set(MAIN_SOURCE
    src/main.cpp
)

# GUI sources (conditional)
if(BUILD_GUI)
    set(GUI_SOURCES
        src/gui/gui_app.cpp
        src/gui/app/app_controller.cpp
        src/gui/widgets/main_window.cpp
        src/gui/widgets/image_preview.cpp
        src/gui/backend/render_backend.cpp
        src/gui/backend/opengl_backend.cpp
    )

    set(GUI_HEADERS
        src/gui/gui_app.hpp
        src/gui/app/app_state.hpp
        src/gui/app/app_controller.hpp
        src/gui/widgets/main_window.hpp
        src/gui/widgets/image_preview.hpp
        src/gui/backend/render_backend.hpp
        src/gui/backend/opengl_backend.hpp
        src/gui/resources/style.hpp
    )

    # D3D11 backend (Windows only)
    if(ENABLE_D3D11)
        list(APPEND GUI_SOURCES
            src/gui/backend/d3d11_backend.cpp
        )
        list(APPEND GUI_HEADERS
            src/gui/backend/d3d11_backend.hpp
        )
    endif()

    # Vulkan backend
    if(ENABLE_VULKAN)
        list(APPEND GUI_SOURCES
            src/gui/backend/vulkan_backend.cpp
        )
        list(APPEND GUI_HEADERS
            src/gui/backend/vulkan_backend.hpp
        )
    endif()
endif()

# =============================================================================
# Main Executable
# =============================================================================
add_executable(${PROJECT_NAME} ${WIN32_EXECUTABLE}
    ${MAIN_SOURCE}
    ${CORE_SOURCES}
    ${CORE_HEADERS}
    ${CLI_SOURCES}
    ${CLI_HEADERS}
    ${UTILS_HEADERS}
    ${WIN_RESOURCE_FILE}
)

if(BUILD_GUI)
    target_sources(${PROJECT_NAME} PRIVATE
        ${GUI_SOURCES}
        ${GUI_HEADERS}
    )
endif()

# =============================================================================
# Include Directories
# =============================================================================
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
)

if(ENABLE_AI_DENOISE)
    target_include_directories(${PROJECT_NAME} PRIVATE
        # NCNN model output headers (.id.h, .mem.h)
        ${CMAKE_CURRENT_SOURCE_DIR}/external/ncnn/model-convert/output
    )
endif()

# =============================================================================
# Link Libraries
# =============================================================================
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${OpenCV_LIBS}
    fmt::fmt
    CLI11::CLI11
    spdlog::spdlog
)

if(BUILD_GUI)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        imgui::imgui
        $<IF:$<TARGET_EXISTS:SDL3::SDL3-static>,SDL3::SDL3-static,SDL3::SDL3>
        implot::implot
        OpenGL::GL
        glad::glad
        nfd::nfd
    )

    # D3D11 system libraries (Windows SDK)
    if(ENABLE_D3D11)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            d3d11
            dxgi
        )
    endif()

    if(ENABLE_VULKAN)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            Vulkan::Vulkan
            GPUOpen::VulkanMemoryAllocator
        )
    endif()
endif()

# AI Denoise: NCNN static lib + volk dynamic Vulkan loader
if(ENABLE_AI_DENOISE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ncnn
        volk::volk
    )
endif()

# macOS framework linking
if(APPLE AND COREFOUNDATION_LIBRARY)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${COREFOUNDATION_LIBRARY})
endif()

# =============================================================================
# Compile Definitions
# =============================================================================
target_compile_definitions(${PROJECT_NAME} PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    APP_NAME="${PROJECT_NAME}"
    $<$<BOOL:${BUILD_GUI}>:GWT_HAS_GUI=1>
    $<$<BOOL:${ENABLE_D3D11}>:GWT_HAS_D3D11=1>
    $<$<BOOL:${ENABLE_VULKAN}>:GWT_HAS_VULKAN=1>
    $<$<BOOL:${ENABLE_AI_DENOISE}>:GWT_HAS_AI_DENOISE=1>
)

# =============================================================================
# Strip binary for release (Linux/macOS/Android)
# =============================================================================
if(NOT WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_STRIP} $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Stripping binary for release"
    )
endif()
# =============================================================================
# Install
# =============================================================================
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# =============================================================================
# Configuration Summary
# =============================================================================
message(STATUS "")
message(STATUS "=== Gemini Watermark Tool Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Platform: ${PLATFORM_NAME}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "OpenCV: ${OpenCV_VERSION}")
message(STATUS "")
message(STATUS "Features:")
message(STATUS "  BUILD_GUI: ${BUILD_GUI}")
message(STATUS "  OPENGL_FOUND: ${OPENGL_FOUND}")
message(STATUS "  OPENGL_opengl_LIBRARY: ${OPENGL_opengl_LIBRARY}")
message(STATUS "  OPENGL_glx_LIBRARY: ${OPENGL_glx_LIBRARY}")
message(STATUS "  ENABLE_D3D11: ${ENABLE_D3D11}")
message(STATUS "  ENABLE_VULKAN: ${ENABLE_VULKAN}")
message(STATUS "  ENABLE_AI_DENOISE: ${ENABLE_AI_DENOISE}")
if(ENABLE_AI_DENOISE)
    message(STATUS "    NCNN: Static lib, Vulkan GPU + CPU fallback")
    message(STATUS "    Model: FDnCNN Color (embedded FP16, ~1.3 MB)")
    message(STATUS "    OpenMP: OFF (standalone, no VCOMP140.DLL dependency)")
endif()
if(APPLE)
    message(STATUS "")
    message(STATUS "macOS:")
    message(STATUS "  Architectures: ${CMAKE_OSX_ARCHITECTURES}")
    message(STATUS "  Deployment Target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()
if(ANDROID)
    message(STATUS "")
    message(STATUS "Android:")
    message(STATUS "  ABI: ${ANDROID_ABI}")
    message(STATUS "  Platform: ${ANDROID_PLATFORM}")
endif()
message(STATUS "=============================================")
