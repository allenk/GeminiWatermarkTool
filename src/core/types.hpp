/**
 * @file    types.hpp
 * @brief   Shared type definitions for Gemini Watermark Tool
 * @author  AllenK (Kwyshell)
 * @license MIT
 */

#pragma once

#include <cstdint>
#include <string>

namespace gwt {

// Version info
inline constexpr const char* kVersion = "0.2.0";

// Result type for operations
enum class [[nodiscard]] ResultCode {
    Success,
    FileNotFound,
    InvalidFormat,
    ProcessingFailed,
    SaveFailed,
    Cancelled
};

// Convert result code to string
[[nodiscard]] constexpr const char* to_string(ResultCode code) noexcept {
    switch (code) {
        case ResultCode::Success:          return "Success";
        case ResultCode::FileNotFound:     return "File not found";
        case ResultCode::InvalidFormat:    return "Invalid format";
        case ResultCode::ProcessingFailed: return "Processing failed";
        case ResultCode::SaveFailed:       return "Save failed";
        case ResultCode::Cancelled:        return "Cancelled";
        default:                           return "Unknown";
    }
}

}  // namespace gwt
