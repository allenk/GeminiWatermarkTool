/**
 * @file    ncnn_shim.hpp
 * @brief   NCNN include shim with Vulkan loader configuration
 * @author  AllenK (Kwyshell)
 * @license MIT
 *
 * @details
 * This header must be included BEFORE any NCNN headers.
 *
 * It solves a fundamental conflict in NCNN's Vulkan integration:
 *   - NCNN has its own Vulkan loader (simplevk.h) that defines Vulkan
 *     function pointers as globals and loads them via dlopen/LoadLibrary.
 *   - When statically linking NCNN, including simplevk.h in OUR code
 *     would create duplicate Vulkan symbol definitions.
 *
 * Solution:
 *   1. Include volk.h first (provides Vulkan type declarations)
 *   2. Define NCNN_SIMPLEVK_H guard to skip NCNN's simplevk.h
 *   3. NCNN's internal code still uses its own simplevk (compiled into lib)
 *   4. Our code uses volk for any direct Vulkan calls (if needed)
 *
 * Usage:
 *   #include "core/ncnn_shim.hpp"   // Always first!
 *   // Now safe to use ncnn::Net, ncnn::Mat, ncnn::create_gpu_instance(), etc.
 */

#pragma once

#ifdef GWT_HAS_AI_DENOISE

 // Step 1: Platform-specific Vulkan extensions
#ifdef __ANDROID__
#ifndef VK_USE_PLATFORM_ANDROID_KHR
#define VK_USE_PLATFORM_ANDROID_KHR
#endif
#endif

// Step 1: volk provides Vulkan declarations (types, enums, function pointers)
// Must come before any NCNN header that pulls in Vulkan types.
#include <volk.h>

// Step 2: Prevent NCNN from including its own simplevk.h
// NCNN's gpu.h does: #include "simplevk.h" unless this guard is defined.
// The NCNN *library* still uses its internal simplevk for actual Vulkan loading;
// this only affects headers included in our translation units.
#define NCNN_SIMPLEVK_H

// Step 3: Now safe to include NCNN headers
#include "net.h"
#include "gpu.h"
#include "cpu.h"

#endif // GWT_HAS_AI_DENOISE
