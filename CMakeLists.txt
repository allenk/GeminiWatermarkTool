cmake_minimum_required(VERSION 3.20)
project(GeminiWatermarkTool VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# Build Options
# =============================================================================

# =============================================================================
# Application Metadata (for Windows resources)
# =============================================================================
set(APP_PRODUCT_NAME "Gemini Watermark Tool")
set(APP_DESCRIPTION "Add/Remove Gemini-style visible watermarks")
set(APP_COMPANY "Allen Wu")
set(APP_COPYRIGHT "Copyright (C) 2024 Allen Wu. MIT License.")

# =============================================================================
# Windows Resource File (Icon + Version Info)
# =============================================================================
set(WIN_RESOURCE_FILE "")  # Default: empty for non-Windows platforms
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
endif()

# =============================================================================
# Platform-Specific Configuration
# =============================================================================

# Windows Configuration
if(MSVC)
    # Use static runtime library (/MT instead of /MD)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    # Compiler flags
    add_compile_options(/W4 /utf-8)

    # Release optimizations
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /GL /Gy")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG /OPT:REF /OPT:ICF")
endif()

# Linux Configuration
if(UNIX AND NOT APPLE)
    # Compiler flags
    add_compile_options(-Wall -Wextra -Wpedantic)
    
    # Release optimizations
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
    
    # Position Independent Code (PIC) for shared libraries
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# macOS Configuration
if(APPLE)
    # Compiler flags
    add_compile_options(-Wall -Wextra -Wpedantic)
    
    # Release optimizations
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
    
    # macOS deployment target (adjust as needed)
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
    endif()
    
    # Architecture support: automatically detects current architecture (x86_64 or arm64)
    # To build universal binary (both x86_64 and arm64), uncomment and use:
    # set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS architectures")
    # 
    # By default, CMake will build for the current architecture:
    # - x86_64 on Intel Macs
    # - arm64 on Apple Silicon (M1/M2/M3) Macs
    
    # Log detected architecture
    if(CMAKE_OSX_ARCHITECTURES)
        message(STATUS "macOS architectures: ${CMAKE_OSX_ARCHITECTURES}")
    else()
        # CMake will auto-detect the current architecture
        # Use CMAKE_SYSTEM_PROCESSOR to show what was detected
        message(STATUS "macOS architecture (auto-detected): ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
endif()


# =============================================================================
# Dependencies
# =============================================================================
# Note: These can be provided by:
#   - System package managers (Linux: apt/dnf, macOS: Homebrew)
#   - vcpkg (Windows recommended, cross-platform option)
#   - Manual installation

find_package(OpenCV REQUIRED)

# Try CONFIG mode first (vcpkg, newer system packages), fallback to MODULE mode
find_package(fmt QUIET CONFIG)
if(NOT fmt_FOUND)
    find_package(fmt REQUIRED)
endif()

find_package(spdlog QUIET CONFIG)
if(NOT spdlog_FOUND)
    find_package(spdlog REQUIRED)
endif()

# CLI11 is header-only, try to find it or use FetchContent
find_package(CLI11 QUIET CONFIG)
if(NOT CLI11_FOUND)
    find_package(CLI11 QUIET)
    if(NOT CLI11_FOUND)
        # Fallback: download CLI11 if not found
        include(FetchContent)
        FetchContent_Declare(
            CLI11
            GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
            GIT_TAG        v2.4.2
        )
        FetchContent_MakeAvailable(CLI11)
    endif()
endif()

# =============================================================================
# Main Executable
# =============================================================================
set(SOURCES
    src/main.cpp
    src/watermark_engine.cpp
    src/blend_modes.cpp
)

set(HEADERS
    src/watermark_engine.hpp
    src/blend_modes.hpp
    src/ascii_logo.hpp
)

add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${WIN_RESOURCE_FILE}  # Windows icon + version info (empty on non-Windows)
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${OpenCV_LIBS}
    fmt::fmt
    CLI11::CLI11
    spdlog::spdlog
)

# =============================================================================
# Compile Definitions
# =============================================================================
target_compile_definitions(${PROJECT_NAME} PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    APP_NAME="${PROJECT_NAME}"
)

# =============================================================================
# Install
# =============================================================================
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

message(STATUS "")
message(STATUS "=== Gemini Watermark Tool Configuration ===")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "OpenCV: ${OpenCV_VERSION}")
if(WIN32)
    message(STATUS "Windows resource file: ${WIN_RESOURCE_FILE}")
endif()
message(STATUS "=============================================")

