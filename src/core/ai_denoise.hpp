/**
 * @file    ai_denoise.hpp
 * @brief   NCNN-based AI denoiser for watermark residual cleanup
 * @author  AllenK (Kwyshell)
 * @license MIT
 *
 * @details
 * Uses FDnCNN (Flexible DnCNN) converted to NCNN format for GPU-accelerated
 * denoising of residual artifacts after reverse alpha blending.
 *
 * The model is embedded into the executable via .mem.h headers generated
 * by ncnn2mem, adding ~1.3 MB to the binary size.
 *
 * Pipeline:
 *   1. Compute gradient mask from alpha map (locate sparkle edges)
 *   2. Run NCNN inference on padded ROI (FDnCNN outputs clean image)
 *   3. Per-pixel masked blend: only repair edge pixels, leave clean areas intact
 */

#pragma once

#ifdef GWT_HAS_AI_DENOISE

#include <opencv2/core.hpp>
#include <memory>
#include <string>

namespace gwt {

/**
 * NCNN-based AI denoiser using FDnCNN model
 *
 * Thread-safety: NOT thread-safe. Create one instance per thread if needed.
 */
class NcnnDenoiser {
public:
    NcnnDenoiser();
    ~NcnnDenoiser();

    // Non-copyable
    NcnnDenoiser(const NcnnDenoiser&) = delete;
    NcnnDenoiser& operator=(const NcnnDenoiser&) = delete;

    // Movable
    NcnnDenoiser(NcnnDenoiser&&) noexcept;
    NcnnDenoiser& operator=(NcnnDenoiser&&) noexcept;

    /**
     * Initialize NCNN runtime and load embedded model
     *
     * Attempts Vulkan GPU first, falls back to CPU if unavailable.
     *
     * @return true if initialization succeeded (GPU or CPU)
     */
    bool initialize();

    /**
     * Check if initialized and ready for inference
     */
    [[nodiscard]] bool is_ready() const;

    /**
     * Check if Vulkan GPU acceleration is active
     */
    [[nodiscard]] bool is_gpu_enabled() const;

    /**
     * Get device description string (e.g., "NVIDIA GeForce RTX 4090" or "CPU (4 threads)")
     */
    [[nodiscard]] std::string device_name() const;

    /**
     * Denoise watermark residual artifacts using gradient-masked AI inference.
     *
     * Uses alpha map gradient to locate sparkle edges, runs FDnCNN only on
     * the padded region, then blends per-pixel: only repairs edge artifacts
     * while preserving clean background detail.
     *
     * @param image     BGR CV_8UC3 image (modified in-place)
     * @param region    Watermark region (where reverse alpha blend was applied)
     * @param alpha_map Alpha map CV_32FC1 [0,1] (will be resized to match region)
     * @param sigma     Noise level 0-75 (higher = more aggressive denoising)
     * @param strength  Overall strength: 0.0 = keep original, 1.0 = full effect
     * @param padding   Context padding around region in pixels (default: 16)
     */
    void denoise(
        cv::Mat& image,
        const cv::Rect& region,
        const cv::Mat& alpha_map,
        float sigma = 25.0f,
        float strength = 0.85f,
        int padding = 16
    );

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace gwt

#endif // GWT_HAS_AI_DENOISE
