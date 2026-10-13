#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

namespace Kaburagi {

enum class BlendMode {
    PassThrough = -1,
    Normal = 0,
    Dissolve,
    Darken,
    Multiply,
    ColorBurn,
    LinearBurn,
    Lighten,
    Screen,
    ColorDodge,
    Addition,
    Overlay,
    SoftLight,
    HardLight,
    VividLight,
    LinearLight,
    PinLight,
    HardMix,
    Difference,
    Exclusion,
    Subtract,
    Divide,
    Hue,
    Saturation,
    Color,
    Luminance,
    GrainExtract,
    GrainMerge,
    Count
};

struct BlendModeInfo {
    BlendMode mode;
    const char* nameJa;
    const char* nameEn;
};

const std::vector<BlendModeInfo>& getAllBlendModes();

/**
 * @brief 単一ピクセル (ARGB32) のブレンド計算
 * @param dst 下地ピクセル (Premultiplied or Straight)
 * @param src 上乗せピクセル
 * @param opacity 不透明度 (0.0f ~ 1.0f)
 * @param mode 合成モード
 */
uint32_t blendPixel(uint32_t dst, uint32_t src, float opacity, BlendMode mode);
uint32_t blendPixelClipped(uint32_t dst, uint32_t src, float opacity, BlendMode mode);

/**
 * @brief タイルバッファ (128x128) の高速合成
 */
void blendTile(uint32_t* dst, const uint32_t* src, float opacity, BlendMode mode, size_t pixelCount, bool isClippingMask = false);

/**
 * @brief マスクタイル (ベースレイヤー) によるクリッピング合成
 */
void blendTileWithClipMask(
    uint32_t* dst,
    const uint32_t* src,
    const uint32_t* maskSrc,
    float opacity,
    BlendMode mode,
    size_t pixelCount,
    float maskOpacity = 1.0f
);

static inline float clamp01(float v) {
    return (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v);
}

// チャンネルブレンド補助関数 (0.0f ~ 1.0f)
static inline float blendChannel(float d, float s, BlendMode mode) {
    switch (mode) {
    case BlendMode::PassThrough:
    case BlendMode::Normal:
        return s;
    case BlendMode::Darken:
        return std::min(d, s);
    case BlendMode::Multiply:
        return d * s;
    case BlendMode::ColorBurn:
        return (s > 0.0f) ? 1.0f - std::min(1.0f, (1.0f - d) / s) : 0.0f;
    case BlendMode::LinearBurn:
        return clamp01(d + s - 1.0f);
    case BlendMode::Lighten:
        return std::max(d, s);
    case BlendMode::Screen:
        return d + s - d * s;
    case BlendMode::ColorDodge:
        return (s < 1.0f) ? std::min(1.0f, d / (1.0f - s)) : 1.0f;
    case BlendMode::Addition:
        return clamp01(d + s);
    case BlendMode::Overlay:
        return (d < 0.5f) ? (2.0f * d * s) : (1.0f - 2.0f * (1.0f - d) * (1.0f - s));
    case BlendMode::HardLight:
        return (s < 0.5f) ? (2.0f * d * s) : (1.0f - 2.0f * (1.0f - d) * (1.0f - s));
    case BlendMode::SoftLight:
        if (s < 0.5f) {
            return d - (1.0f - 2.0f * s) * d * (1.0f - d);
        } else {
            float d2 = (d <= 0.25f) ? ((16.0f * d - 12.0f) * d + 4.0f) * d : std::sqrt(d);
            return d + (2.0f * s - 1.0f) * (d2 - d);
        }
    case BlendMode::VividLight:
        if (s <= 0.5f) {
            return (s > 0.0f) ? 1.0f - std::min(1.0f, (1.0f - d) / (2.0f * s)) : 0.0f;
        } else {
            float s2 = 2.0f * (s - 0.5f);
            return (s2 < 1.0f) ? std::min(1.0f, d / (1.0f - s2)) : 1.0f;
        }
    case BlendMode::LinearLight:
        return clamp01(d + 2.0f * s - 1.0f);
    case BlendMode::PinLight:
        return (s > 0.5f) ? std::max(d, 2.0f * (s - 0.5f)) : std::min(d, 2.0f * s);
    case BlendMode::HardMix:
        return (d + s >= 1.0f) ? 1.0f : 0.0f;
    case BlendMode::Difference:
        return std::abs(d - s);
    case BlendMode::Exclusion:
        return d + s - 2.0f * d * s;
    case BlendMode::Subtract:
        return clamp01(d - s);
    case BlendMode::Divide:
        return (s > 0.0f) ? clamp01(d / s) : 1.0f;
    case BlendMode::GrainExtract:
        return clamp01(d - s + 0.5f);
    case BlendMode::GrainMerge:
        return clamp01(d + s - 0.5f);
    default:
        return s;
    }
}

} // namespace Kaburagi
