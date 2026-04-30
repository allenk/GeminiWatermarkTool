/**
 * @file    ai_denoise.cpp
 * @brief   NCNN-based AI denoiser implementation
 * @author  AllenK (Kwyshell)
 * @license MIT
 *
 * @details
 * FDnCNN inference with gradient-masked blending:
 *
 *   1. Compute gradient mask from alpha map → identifies sparkle edge pixels
 *   2. Run NCNN inference on padded ROI → FDnCNN outputs clean image directly
 *   3. Per-pixel blend: result = mask * denoised + (1 - mask) * original
 *      → clean background untouched, only edge artifacts get repaired
 *
 * The sigma channel is a uniform map filled with sigma/255.0, telling the
 * network how aggressively to denoise. Range 0-75.
 */

#ifdef GWT_HAS_AI_DENOISE

// NCNN shim must be included FIRST (volk + simplevk.h hack)
#include "core/ncnn_shim.hpp"
#include "core/ai_denoise.hpp"

#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <chrono>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

// Embedded model data (defined in ai_denoise_model.cpp)
namespace gwt::ai_model {
    const unsigned char* param_data();
    const unsigned char* bin_data();
}

namespace gwt {

namespace {

// Probe whether the Vulkan loader is dynamically loadable on this system.
// NCNN's internal simplevk dlopens the loader by these exact names; if the
// probe fails the loader is absent and ncnn::create_gpu_instance() will
// dispatch through uninitialised function pointers and crash (SIGBUS on
// macOS Apple Silicon, observed in issues #30 / #31).
bool vulkan_loader_present() {
#if defined(_WIN32)
    HMODULE h = LoadLibraryA("vulkan-1.dll");
    if (h) { FreeLibrary(h); return true; }
    return false;
#elif defined(__APPLE__)
    // macOS does not ship a Vulkan loader. The user must install MoltenVK
    // via the LunarG Vulkan SDK (or it must be bundled into the app).
    void* h = dlopen("libvulkan.1.dylib", RTLD_LAZY | RTLD_LOCAL);
    if (!h) h = dlopen("libvulkan.dylib", RTLD_LAZY | RTLD_LOCAL);
    if (h) { dlclose(h); return true; }
    return false;
#else
    void* h = dlopen("libvulkan.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (!h) h = dlopen("libvulkan.so", RTLD_LAZY | RTLD_LOCAL);
    if (h) { dlclose(h); return true; }
    return false;
#endif
}

}  // namespace

// Blob indices from binary param (string names not available in binary format)
// These correspond to model_core.id.h: BLOB_in0 = 0, BLOB_out0 = 20
static constexpr int BLOB_INPUT  = 0;
static constexpr int BLOB_OUTPUT = 20;

// ============================================================================
// Impl (PIMPL idiom hides NCNN types from header)
// ============================================================================

struct NcnnDenoiser::Impl {
    ncnn::Net net;
    bool ready{false};
    bool gpu_enabled{false};
    int gpu_device_index{-1};
    std::string device_desc;

    // ========================================================================
    // Initialization
    // ========================================================================

    bool load_model() {
        // load_param(const unsigned char*) returns bytes consumed (>0 = success)
        int ret_param = net.load_param(ai_model::param_data());
        if (ret_param <= 0) {
            spdlog::error("NcnnDenoiser: Failed to load param (ret={})", ret_param);
            return false;
        }
        spdlog::info("NcnnDenoiser: Param loaded ({} bytes)", ret_param);

        // load_model(const unsigned char*) also returns bytes consumed
        int ret_model = net.load_model(ai_model::bin_data());
        if (ret_model <= 0) {
            spdlog::error("NcnnDenoiser: Failed to load model (ret={})", ret_model);
            return false;
        }
        spdlog::info("NcnnDenoiser: Model loaded ({} bytes)", ret_model);

        return true;
    }

    bool init_gpu() {
        int gpu_count = ncnn::get_gpu_count();
        if (gpu_count <= 0) {
            spdlog::info("NcnnDenoiser: No Vulkan GPU detected, using CPU");
            return false;
        }

        gpu_device_index = ncnn::get_default_gpu_index();
        const ncnn::GpuInfo& gpu_info = ncnn::get_gpu_info(gpu_device_index);
        device_desc = gpu_info.device_name();

        net.opt.use_vulkan_compute = true;

        spdlog::info("NcnnDenoiser: Vulkan GPU #{} - {}", gpu_device_index, device_desc);
        return true;
    }

    void init_cpu() {
        net.opt.use_vulkan_compute = false;
        net.opt.num_threads = std::max(1, ncnn::get_cpu_count());
        device_desc = fmt::format("CPU ({} threads)", net.opt.num_threads);
        spdlog::info("NcnnDenoiser: {}", device_desc);
    }

    // ========================================================================
    // NCNN inference helpers
    // ========================================================================

    /**
     * Build a 4-channel ncnn::Mat from an RGB float image and sigma value.
     *
     * ncnn::Mat layout is CHW (each channel is a contiguous H*W block).
     * FDnCNN expects: [R, G, B, sigma_map] where sigma_map is uniform.
     */
    static ncnn::Mat build_input(const cv::Mat& rgb_float, float sigma_norm) {
        const int h = rgb_float.rows;
        const int w = rgb_float.cols;

        ncnn::Mat input(w, h, 4);

        float* ch_r = input.channel(0);
        float* ch_g = input.channel(1);
        float* ch_b = input.channel(2);
        float* ch_s = input.channel(3);

        for (int y = 0; y < h; ++y) {
            const float* row = rgb_float.ptr<float>(y);
            const int offset = y * w;
            for (int x = 0; x < w; ++x) {
                const int idx = offset + x;
                const int src = x * 3;
                ch_r[idx] = row[src + 0];
                ch_g[idx] = row[src + 1];
                ch_b[idx] = row[src + 2];
            }
        }

        // Uniform sigma map
        const int total = h * w;
        for (int i = 0; i < total; ++i) {
            ch_s[i] = sigma_norm;
        }

        return input;
    }

    /**
     * Extract 3-channel result from ncnn::Mat to cv::Mat (RGB float).
     */
    static cv::Mat extract_output(const ncnn::Mat& output) {
        const int h = output.h;
        const int w = output.w;

        cv::Mat result(h, w, CV_32FC3);

        const float* ch_r = output.channel(0);
        const float* ch_g = output.channel(1);
        const float* ch_b = output.channel(2);

        for (int y = 0; y < h; ++y) {
            float* row = result.ptr<float>(y);
            const int offset = y * w;
            for (int x = 0; x < w; ++x) {
                const int idx = offset + x;
                const int dst = x * 3;
                row[dst + 0] = ch_r[idx];
                row[dst + 1] = ch_g[idx];
                row[dst + 2] = ch_b[idx];
            }
        }

        return result;
    }

    /**
     * Run FDnCNN inference on a BGR ROI.
     *
     * FDnCNN outputs the denoised image directly (NOT a noise residual).
     *
     * @param bgr_roi   CV_8UC3 BGR image
     * @param sigma     Noise level 0-150
     * @return          CV_8UC3 BGR denoised result (same size as input)
     */
    cv::Mat run_inference(const cv::Mat& bgr_roi, float sigma) {
        // BGR uint8 -> RGB float [0,1]
        cv::Mat rgb_float;
        cv::cvtColor(bgr_roi, rgb_float, cv::COLOR_BGR2RGB);
        rgb_float.convertTo(rgb_float, CV_32FC3, 1.0 / 255.0);

        // Build 4-channel input: [R, G, B, sigma/255]
        const float sigma_norm = sigma / 255.0f;
        ncnn::Mat ncnn_input = build_input(rgb_float, sigma_norm);

        // Inference (using blob indices, not string names)
        ncnn::Mat ncnn_output;
        {
            ncnn::Extractor ex = net.create_extractor();
            ex.input(BLOB_INPUT, ncnn_input);
            ex.extract(BLOB_OUTPUT, ncnn_output);
        }

        // FDnCNN outputs denoised image directly
        cv::Mat clean = extract_output(ncnn_output);

        // Clamp to [0, 1]
        cv::min(clean, 1.0f, clean);
        cv::max(clean, 0.0f, clean);

        // RGB float [0,1] -> BGR uint8
        cv::Mat bgr_result;
        clean.convertTo(bgr_result, CV_8UC3, 255.0);
        cv::cvtColor(bgr_result, bgr_result, cv::COLOR_RGB2BGR);

        return bgr_result;
    }

    // ========================================================================
    // Gradient mask from alpha map
    // ========================================================================

    /**
     * Compute gradient weight mask from alpha map.
     *
     * Same algorithm as WatermarkEngine::inpaint_residual() GAUSSIAN mode:
     *   1. Resize alpha map to match watermark region
     *   2. Sobel gradient → magnitude → normalize
     *   3. sqrt (gamma correction to expand weak edges)
     *   4. Dilate + GaussianBlur for smooth transitions
     *
     * @param alpha_map  Source alpha map (CV_32FC1, any size)
     * @param region_w   Target width (watermark region)
     * @param region_h   Target height (watermark region)
     * @param strength   Overall strength multiplier
     * @return           CV_32FC1 weight mask [0,1], size = (region_w, region_h)
     */
    static cv::Mat compute_gradient_mask(
        const cv::Mat& alpha_map,
        int region_w, int region_h,
        float strength)
    {
        // Resize alpha to match region
        cv::Mat alpha_resized;
        int interp = (region_w > alpha_map.cols) ? cv::INTER_LINEAR : cv::INTER_AREA;
        cv::resize(alpha_map, alpha_resized,
                   cv::Size(region_w, region_h), 0, 0, interp);

        // Sobel gradient magnitude
        cv::Mat grad_x, grad_y, grad_mag;
        cv::Sobel(alpha_resized, grad_x, CV_32F, 1, 0, 3);
        cv::Sobel(alpha_resized, grad_y, CV_32F, 0, 1, 3);
        cv::magnitude(grad_x, grad_y, grad_mag);

        // Normalize to [0, 1]
        double grad_min, grad_max;
        cv::minMaxLoc(grad_mag, &grad_min, &grad_max);
        if (grad_max <= grad_min) {
            return cv::Mat::zeros(region_h, region_w, CV_32F);
        }
        cv::Mat grad_norm = (grad_mag - grad_min) / (grad_max - grad_min);

        // Gamma correction: sqrt expands weak gradient values
        cv::Mat grad_weight;
        cv::sqrt(grad_norm, grad_weight);

        // Dilate to cover residual spread (5x5 ellipse)
        cv::Mat dk = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
        cv::dilate(grad_weight, grad_weight, dk);

        // Smooth for natural transitions
        cv::GaussianBlur(grad_weight, grad_weight, cv::Size(0, 0), 2.0);

        // Scale by strength and clamp
        grad_weight *= strength;
        cv::threshold(grad_weight, grad_weight, 1.0, 1.0, cv::THRESH_TRUNC);

        return grad_weight;
    }
};

// ============================================================================
// NcnnDenoiser public API
// ============================================================================

NcnnDenoiser::NcnnDenoiser() : m_impl(std::make_unique<Impl>()) {}

NcnnDenoiser::~NcnnDenoiser() = default;

NcnnDenoiser::NcnnDenoiser(NcnnDenoiser&&) noexcept = default;
NcnnDenoiser& NcnnDenoiser::operator=(NcnnDenoiser&&) noexcept = default;

bool NcnnDenoiser::initialize() {
    if (m_impl->ready) {
        spdlog::warn("NcnnDenoiser: Already initialized");
        return true;
    }

    // Probe the Vulkan loader BEFORE calling into NCNN. On systems without
    // it (macOS without MoltenVK is the common case), NCNN's
    // create_gpu_instance() dispatches through uninitialised function
    // pointers and crashes -- see issues #30 and #31.
    const bool vulkan_available = vulkan_loader_present();
    if (vulkan_available) {
        ncnn::create_gpu_instance();
    } else {
#if defined(__APPLE__)
        spdlog::info("NcnnDenoiser: Vulkan loader unavailable on macOS "
                     "(install MoltenVK or the LunarG Vulkan SDK for GPU "
                     "acceleration); falling back to CPU");
#else
        spdlog::info("NcnnDenoiser: Vulkan loader unavailable, falling back to CPU");
#endif
    }

    // FP16 storage for efficiency, FP32 compute for accuracy
    m_impl->net.opt.use_fp16_packed = true;
    m_impl->net.opt.use_fp16_storage = true;
    m_impl->net.opt.use_fp16_arithmetic = false;
    m_impl->net.opt.use_packing_layout = true;

    // Try GPU first, fall back to CPU
    m_impl->gpu_enabled = vulkan_available && m_impl->init_gpu();
    if (!m_impl->gpu_enabled) {
        m_impl->init_cpu();
    }

    if (!m_impl->load_model()) {
        return false;
    }

    m_impl->ready = true;
    spdlog::info("NcnnDenoiser: Initialized ({})", m_impl->device_desc);
    return true;
}

bool NcnnDenoiser::is_ready() const {
    return m_impl->ready;
}

bool NcnnDenoiser::is_gpu_enabled() const {
    return m_impl->gpu_enabled;
}

std::string NcnnDenoiser::device_name() const {
    return m_impl->device_desc;
}

void NcnnDenoiser::denoise(
    cv::Mat& image,
    const cv::Rect& region,
    const cv::Mat& alpha_map,
    float sigma,
    float strength,
    int padding)
{
    if (!m_impl->ready) {
        spdlog::warn("NcnnDenoiser: Not initialized, skipping");
        return;
    }

    if (image.empty() || image.type() != CV_8UC3) {
        spdlog::warn("NcnnDenoiser: Invalid image");
        return;
    }

    if (alpha_map.empty()) {
        spdlog::warn("NcnnDenoiser: No alpha map provided");
        return;
    }

    sigma = std::clamp(sigma, 0.0f, 150.0f);
    strength = std::clamp(strength, 0.0f, 3.0f);
    if (strength < 0.001f) return;

    // =====================================================================
    // Step 1: Compute gradient mask from alpha map
    // =====================================================================
    cv::Mat grad_weight = Impl::compute_gradient_mask(
        alpha_map, region.width, region.height, strength);

    int active_pixels = cv::countNonZero(grad_weight > 0.01f);
    if (active_pixels == 0) {
        spdlog::info("NcnnDenoiser: No edge pixels found, skipping");
        return;
    }

    // =====================================================================
    // Step 2: Calculate padded region for NCNN inference
    // =====================================================================
    cv::Rect padded_roi(
        region.x - padding,
        region.y - padding,
        region.width + padding * 2,
        region.height + padding * 2
    );
    padded_roi &= cv::Rect(0, 0, image.cols, image.rows);

    if (padded_roi.width < 4 || padded_roi.height < 4) {
        spdlog::warn("NcnnDenoiser: ROI too small");
        return;
    }

    // Inner rect: where the watermark region sits within the padded area
    cv::Rect inner_rect(
        region.x - padded_roi.x,
        region.y - padded_roi.y,
        region.width,
        region.height
    );
    inner_rect &= cv::Rect(0, 0, padded_roi.width, padded_roi.height);

    // =====================================================================
    // Step 3: Run NCNN inference on padded ROI
    // =====================================================================
    cv::Mat roi_original = image(padded_roi).clone();

    auto t0 = std::chrono::high_resolution_clock::now();
    cv::Mat roi_denoised = m_impl->run_inference(roi_original, sigma);
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // =====================================================================
    // Step 4: Gradient-masked blend (only repair edge pixels)
    //
    // Embed gradient weight into padded coordinate system, then:
    //   result = weight * denoised + (1 - weight) * original
    //
    // Pixels with weight ≈ 0 (clean background) → unchanged
    // Pixels with weight ≈ 1 (sparkle edges)    → AI denoised
    // =====================================================================

    // Embed gradient mask into padded coordinate space
    cv::Mat weight = cv::Mat::zeros(padded_roi.size(), CV_32F);
    grad_weight.copyTo(weight(inner_rect));

    // Soften boundary transition (σ=1.0, affects ~3px at edge)
    cv::GaussianBlur(weight, weight, cv::Size(0, 0), 1.0);

    // Per-pixel weighted blend
    cv::Mat weight_3ch;
    cv::merge(std::vector<cv::Mat>{weight, weight, weight}, weight_3ch);

    cv::Mat orig_f, denoised_f, result_f;
    roi_original.convertTo(orig_f, CV_32FC3);
    roi_denoised.convertTo(denoised_f, CV_32FC3);

    cv::Mat one_minus_w = cv::Scalar(1.0, 1.0, 1.0) - weight_3ch;
    cv::multiply(orig_f, one_minus_w, orig_f);
    cv::multiply(denoised_f, weight_3ch, denoised_f);
    result_f = orig_f + denoised_f;

    cv::Mat result;
    result_f.convertTo(result, CV_8UC3);

    // Write back to image
    result.copyTo(image(padded_roi));

    spdlog::info("NcnnDenoiser: sigma={:.0f}, strength={:.0f}%, roi={}x{}, "
                 "{} edge pixels, {:.1f}ms ({})",
                 sigma, strength * 100.0f,
                 padded_roi.width, padded_roi.height,
                 active_pixels, ms, m_impl->device_desc);
}

} // namespace gwt

#endif // GWT_HAS_AI_DENOISE
