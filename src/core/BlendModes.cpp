#include "BlendModes.hpp"
#include <algorithm>
#include <cmath>

namespace Kaburagi {

const std::vector<BlendModeInfo>& getAllBlendModes() {
    static const std::vector<BlendModeInfo> s_modes = {
        { BlendMode::PassThrough,  "通過",             "Pass Through" },
        { BlendMode::Normal,       "通常",             "Normal" },
        { BlendMode::Dissolve,     "ディゾルブ",       "Dissolve" },
        { BlendMode::Darken,       "比較(暗)",         "Darken" },
        { BlendMode::Multiply,     "乗算",             "Multiply" },
        { BlendMode::ColorBurn,    "焼き込み",         "Color Burn" },
        { BlendMode::LinearBurn,   "リニア焼き込み",   "Linear Burn" },
        { BlendMode::Lighten,      "比較(明)",         "Lighten" },
        { BlendMode::Screen,       "スクリーン",       "Screen" },
        { BlendMode::ColorDodge,   "覆い焼き",         "Color Dodge" },
        { BlendMode::Addition,     "加算 (発光)",      "Addition" },
        { BlendMode::Overlay,      "オーバーレイ",     "Overlay" },
        { BlendMode::SoftLight,    "ソフトライト",     "Soft Light" },
        { BlendMode::HardLight,    "ハードライト",     "Hard Light" },
        { BlendMode::VividLight,   "ビビッドライト",   "Vivid Light" },
        { BlendMode::LinearLight,  "リニアライト",     "Linear Light" },
        { BlendMode::PinLight,     "ピンライト",       "Pin Light" },
        { BlendMode::HardMix,      "ハードミックス",   "Hard Mix" },
        { BlendMode::Difference,   "差の絶対値",       "Difference" },
        { BlendMode::Exclusion,    "除外",             "Exclusion" },
        { BlendMode::Subtract,     "減算",             "Subtract" },
        { BlendMode::Divide,       "除算",             "Divide" },
        { BlendMode::Hue,          "色相",             "Hue" },
        { BlendMode::Saturation,   "彩度",             "Saturation" },
        { BlendMode::Color,        "カラー",           "Color" },
        { BlendMode::Luminance,    "輝度",             "Luminance" },
        { BlendMode::GrainExtract, "粒子抽出",         "Grain Extract" },
        { BlendMode::GrainMerge,   "粒子結合",         "Grain Merge" }
    };
    return s_modes;
}


uint32_t blendPixel(uint32_t dst, uint32_t src, float opacity, BlendMode mode) {
    uint32_t sa = (src >> 24) & 0xFF;
    if (sa == 0 || opacity <= 0.0f) return dst;

    uint32_t da = (dst >> 24) & 0xFF;
    float saF = (sa / 255.0f) * opacity;
    float daF = da / 255.0f;

    float sr = ((src >> 16) & 0xFF) / 255.0f;
    float sg = ((src >> 8) & 0xFF) / 255.0f;
    float sb = (src & 0xFF) / 255.0f;

    if (da == 0) {
        // 下地が完全透明の場合は上乗せ色そのもの
        uint8_t outA = static_cast<uint8_t>(saF * 255.0f + 0.5f);
        return (outA << 24) | (src & 0x00FFFFFF);
    }

    float dr = ((dst >> 16) & 0xFF) / 255.0f;
    float dg = ((dst >> 8) & 0xFF) / 255.0f;
    float db = (dst & 0xFF) / 255.0f;

    // ブレンド計算
    float br = blendChannel(dr, sr, mode);
    float bg = blendChannel(dg, sg, mode);
    float bb = blendChannel(db, sb, mode);

    // アルファ合成 (Porter-Duff Over)
    float outA = saF + daF * (1.0f - saF);
    if (outA <= 0.0f) return 0;

    float outR = (br * saF + dr * daF * (1.0f - saF)) / outA;
    float outG = (bg * saF + dg * daF * (1.0f - saF)) / outA;
    float outB = (bb * saF + db * daF * (1.0f - saF)) / outA;

    uint8_t r8 = static_cast<uint8_t>(clamp01(outR) * 255.0f + 0.5f);
    uint8_t g8 = static_cast<uint8_t>(clamp01(outG) * 255.0f + 0.5f);
    uint8_t b8 = static_cast<uint8_t>(clamp01(outB) * 255.0f + 0.5f);
    uint8_t a8 = static_cast<uint8_t>(clamp01(outA) * 255.0f + 0.5f);

    return (a8 << 24) | (r8 << 16) | (g8 << 8) | b8;
}

uint32_t blendPixelClipped(uint32_t dst, uint32_t src, float opacity, BlendMode mode) {
    float daF = ((dst >> 24) & 0xFF) / 255.0f;
    if (daF <= 0.0f) return 0; // 下地が透明ならクリッピングにより描画されない

    float saF = (((src >> 24) & 0xFF) / 255.0f) * opacity;
    if (saF <= 0.0f) return dst;

    float sr = ((src >> 16) & 0xFF) / 255.0f;
    float sg = ((src >> 8) & 0xFF) / 255.0f;
    float sb = (src & 0xFF) / 255.0f;

    float dr = ((dst >> 16) & 0xFF) / 255.0f;
    float dg = ((dst >> 8) & 0xFF) / 255.0f;
    float db = (dst & 0xFF) / 255.0f;

    float br = blendChannel(dr, sr, mode);
    float bg = blendChannel(dg, sg, mode);
    float bb = blendChannel(db, sb, mode);

    float effSa = saF * daF;
    float outR = br * effSa + dr * (1.0f - effSa);
    float outG = bg * effSa + dg * (1.0f - effSa);
    float outB = bb * effSa + db * (1.0f - effSa);
    float outA = daF; // 下地のアルファ値を超えない

    uint8_t r8 = static_cast<uint8_t>(clamp01(outR) * 255.0f + 0.5f);
    uint8_t g8 = static_cast<uint8_t>(clamp01(outG) * 255.0f + 0.5f);
    uint8_t b8 = static_cast<uint8_t>(clamp01(outB) * 255.0f + 0.5f);
    uint8_t a8 = static_cast<uint8_t>(clamp01(outA) * 255.0f + 0.5f);

    return (a8 << 24) | (r8 << 16) | (g8 << 8) | b8;
}

void blendTile(uint32_t* dst, const uint32_t* src, float opacity, BlendMode mode, size_t pixelCount, bool isClippingMask) {
    if (!dst || !src || opacity <= 0.0f) return;

    if (isClippingMask) {
        for (size_t i = 0; i < pixelCount; ++i) {
            dst[i] = blendPixelClipped(dst[i], src[i], opacity, mode);
        }
    } else {
        for (size_t i = 0; i < pixelCount; ++i) {
            dst[i] = blendPixel(dst[i], src[i], opacity, mode);
        }
    }
}

void blendTileWithClipMask(
    uint32_t* dst,
    const uint32_t* src,
    const uint32_t* maskSrc,
    float opacity,
    BlendMode mode,
    size_t pixelCount,
    float maskOpacity
) {
    if (!dst || !src || !maskSrc || opacity <= 0.0f || maskOpacity <= 0.0f) return;

    for (size_t i = 0; i < pixelCount; ++i) {
        uint32_t srcPix = src[i];
        uint8_t sa = (srcPix >> 24) & 0xFF;
        if (sa == 0) continue;

        uint32_t maskPix = maskSrc[i];
        uint8_t ma = (maskPix >> 24) & 0xFF;
        if (ma == 0) continue;

        float maskAlphaF = (ma / 255.0f) * maskOpacity;
        if (maskAlphaF <= 0.0f) continue;

        float srcAlphaF = (sa / 255.0f) * opacity;
        float effSa = srcAlphaF * maskAlphaF;
        if (effSa <= 0.0f) continue;

        float sr = ((srcPix >> 16) & 0xFF) / 255.0f;
        float sg = ((srcPix >> 8) & 0xFF) / 255.0f;
        float sb = (srcPix & 0xFF) / 255.0f;

        uint32_t dstPix = dst[i];
        uint8_t da = (dstPix >> 24) & 0xFF;
        float daF = da / 255.0f;
        float dr = ((dstPix >> 16) & 0xFF) / 255.0f;
        float dg = ((dstPix >> 8) & 0xFF) / 255.0f;
        float db = (dstPix & 0xFF) / 255.0f;

        float br = blendChannel(dr, sr, mode);
        float bg = blendChannel(dg, sg, mode);
        float bb = blendChannel(db, sb, mode);

        float outA = effSa + daF * (1.0f - effSa);
        if (outA <= 0.0f) continue;

        float outR = (br * effSa + dr * daF * (1.0f - effSa)) / outA;
        float outG = (bg * effSa + dg * daF * (1.0f - effSa)) / outA;
        float outB = (bb * effSa + db * daF * (1.0f - effSa)) / outA;

        uint8_t r8 = static_cast<uint8_t>(clamp01(outR) * 255.0f + 0.5f);
        uint8_t g8 = static_cast<uint8_t>(clamp01(outG) * 255.0f + 0.5f);
        uint8_t b8 = static_cast<uint8_t>(clamp01(outB) * 255.0f + 0.5f);
        uint8_t a8 = static_cast<uint8_t>(clamp01(outA) * 255.0f + 0.5f);

        dst[i] = (a8 << 24) | (r8 << 16) | (g8 << 8) | b8;
    }
}

} // namespace Kaburagi
