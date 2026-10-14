#include "BlendModes.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define KABURAGI_USE_SSE2 1
#include <emmintrin.h>
#endif

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

// pixman / Skia スタイルの高精度 255 除算 (0〜65025 で誤差 0)
static inline uint32_t div255(uint32_t v) {
    return (v + 1 + (v >> 8)) >> 8;
}

// 整数ブレンドチャンネル計算
static inline uint8_t blendChannelInt(uint32_t d, uint32_t s, BlendMode mode) {
    switch (mode) {
    case BlendMode::PassThrough:
    case BlendMode::Normal:
        return static_cast<uint8_t>(s);
    case BlendMode::Multiply:
        return static_cast<uint8_t>(div255(d * s));
    case BlendMode::Screen:
        return static_cast<uint8_t>(d + s - div255(d * s));
    case BlendMode::Addition:
        return static_cast<uint8_t>(std::min(255u, d + s));
    case BlendMode::Darken:
        return static_cast<uint8_t>(std::min(d, s));
    case BlendMode::Lighten:
        return static_cast<uint8_t>(std::max(d, s));
    case BlendMode::Difference:
        return static_cast<uint8_t>(d > s ? d - s : s - d);
    case BlendMode::LinearBurn:
        return static_cast<uint8_t>((d + s > 255) ? (d + s - 255) : 0);
    default: {
        float df = d / 255.0f;
        float sf = s / 255.0f;
        float bf = blendChannel(df, sf, mode);
        return static_cast<uint8_t>(clamp01(bf) * 255.0f + 0.5f);
    }
    }
}

// 高速整数単一ピクセルブレンド (Normal / PassThrough 専用)
static inline uint32_t blendPixelNormalFast(uint32_t dst, uint32_t src, uint32_t op256) {
    uint32_t sa = (src >> 24) & 0xFF;
    if (sa == 0) return dst;
    sa = (sa * op256) >> 8;
    if (sa == 0) return dst;

    uint32_t da = (dst >> 24) & 0xFF;
    if (sa >= 255) {
        return src;
    }
    if (da == 0) {
        return (sa << 24) | (src & 0x00FFFFFF);
    }

    uint32_t sr = (src >> 16) & 0xFF;
    uint32_t sg = (src >> 8) & 0xFF;
    uint32_t sb = src & 0xFF;

    uint32_t dr = (dst >> 16) & 0xFF;
    uint32_t dg = (dst >> 8) & 0xFF;
    uint32_t db = dst & 0xFF;

    if (da == 255) {
        // 下地が不透明（大半のキャンバス描画領域）
        uint32_t invSa = 255 - sa;
        uint32_t r = div255(sr * sa + dr * invSa);
        uint32_t g = div255(sg * sa + dg * invSa);
        uint32_t b = div255(sb * sa + db * invSa);
        return (255 << 24) | (r << 16) | (g << 8) | b;
    }

    // 一般半透明合成 (Porter-Duff Over)
    uint32_t invSa = 255 - sa;
    uint32_t da_term = div255(da * invSa);
    uint32_t outA = sa + da_term;
    if (outA == 0) return 0;

    uint32_t outR = (sr * sa + dr * da_term) / outA;
    uint32_t outG = (sg * sa + dg * da_term) / outA;
    uint32_t outB = (sb * sa + db * da_term) / outA;

    return (outA << 24) | (outR << 16) | (outG << 8) | outB;
}

uint32_t blendPixel(uint32_t dst, uint32_t src, float opacity, BlendMode mode) {
    uint32_t sa = (src >> 24) & 0xFF;
    if (sa == 0 || opacity <= 0.0f) return dst;

    if (mode == BlendMode::Normal || mode == BlendMode::PassThrough) {
        uint32_t op256 = static_cast<uint32_t>(opacity * 256.0f + 0.5f);
        return blendPixelNormalFast(dst, src, op256);
    }

    uint32_t da = (dst >> 24) & 0xFF;
    uint32_t effSa = static_cast<uint32_t>(sa * opacity + 0.5f);
    if (effSa == 0) return dst;

    if (da == 0) {
        return (effSa << 24) | (src & 0x00FFFFFF);
    }

    uint32_t sr = (src >> 16) & 0xFF;
    uint32_t sg = (src >> 8) & 0xFF;
    uint32_t sb = src & 0xFF;

    uint32_t dr = (dst >> 16) & 0xFF;
    uint32_t dg = (dst >> 8) & 0xFF;
    uint32_t db = dst & 0xFF;

    uint32_t br = blendChannelInt(dr, sr, mode);
    uint32_t bg = blendChannelInt(dg, sg, mode);
    uint32_t bb = blendChannelInt(db, sb, mode);

    uint32_t invSa = 255 - effSa;
    uint32_t da_term = div255(da * invSa);
    uint32_t outA = effSa + da_term;
    if (outA == 0) return 0;

    uint32_t outR = (br * effSa + dr * da_term) / outA;
    uint32_t outG = (bg * effSa + dg * da_term) / outA;
    uint32_t outB = (bb * effSa + db * da_term) / outA;

    return (outA << 24) | (outR << 16) | (outG << 8) | outB;
}

uint32_t blendPixelClipped(uint32_t dst, uint32_t src, float opacity, BlendMode mode) {
    uint32_t da = (dst >> 24) & 0xFF;
    if (da == 0) return 0; // 下地が透明ならクリッピングにより描画されない

    uint32_t sa = (src >> 24) & 0xFF;
    uint32_t effSa = static_cast<uint32_t>(sa * opacity + 0.5f);
    if (effSa == 0) return dst;

    uint32_t sr = (src >> 16) & 0xFF;
    uint32_t sg = (src >> 8) & 0xFF;
    uint32_t sb = src & 0xFF;

    uint32_t dr = (dst >> 16) & 0xFF;
    uint32_t dg = (dst >> 8) & 0xFF;
    uint32_t db = dst & 0xFF;

    uint32_t br = blendChannelInt(dr, sr, mode);
    uint32_t bg = blendChannelInt(dg, sg, mode);
    uint32_t bb = blendChannelInt(db, sb, mode);

    uint32_t clipSa = div255(effSa * da);
    uint32_t invClipSa = 255 - clipSa;
    uint32_t outR = (br * clipSa + dr * invClipSa + 128) >> 8;
    uint32_t outG = (bg * clipSa + dg * invClipSa + 128) >> 8;
    uint32_t outB = (bb * clipSa + db * invClipSa + 128) >> 8;

    return (da << 24) | (outR << 16) | (outG << 8) | outB;
}

#if defined(KABURAGI_USE_SSE2)
// SSE2 による 4 ピクセル並列ブレンド (下地が完全不透明 da == 255 の高速パス)
static inline void blend4_SSE2_OpaqueDst(uint32_t* dst, const uint32_t* src, __m128i op256_vec) {
    __m128i d = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst));
    __m128i s = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));

    __m128i zero = _mm_setzero_si128();

    __m128i s_lo = _mm_unpacklo_epi8(s, zero);
    __m128i s_hi = _mm_unpackhi_epi8(s, zero);

    __m128i d_lo = _mm_unpacklo_epi8(d, zero);
    __m128i d_hi = _mm_unpackhi_epi8(d, zero);

    // 各ピクセルのアルファ (ワード 3) を複製
    __m128i sa_lo = _mm_shufflelo_epi16(s_lo, _MM_SHUFFLE(3, 3, 3, 3));
    sa_lo = _mm_shufflehi_epi16(sa_lo, _MM_SHUFFLE(3, 3, 3, 3));
    sa_lo = _mm_srli_epi16(_mm_mullo_epi16(sa_lo, op256_vec), 8);

    __m128i sa_hi = _mm_shufflelo_epi16(s_hi, _MM_SHUFFLE(3, 3, 3, 3));
    sa_hi = _mm_shufflehi_epi16(sa_hi, _MM_SHUFFLE(3, 3, 3, 3));
    sa_hi = _mm_srli_epi16(_mm_mullo_epi16(sa_hi, op256_vec), 8);

    __m128i c255 = _mm_set1_epi16(255);
    __m128i invSa_lo = _mm_sub_epi16(c255, sa_lo);
    __m128i invSa_hi = _mm_sub_epi16(c255, sa_hi);

    // out = div255(s * sa + d * (255 - sa))
    // div255(v) = (v + 1 + (v >> 8)) >> 8
    __m128i term1_lo = _mm_mullo_epi16(s_lo, sa_lo);
    __m128i term2_lo = _mm_mullo_epi16(d_lo, invSa_lo);
    __m128i sum_lo = _mm_add_epi16(term1_lo, term2_lo);
    __m128i c1 = _mm_set1_epi16(1);
    __m128i res_lo = _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(sum_lo, c1), _mm_srli_epi16(sum_lo, 8)), 8);

    __m128i term1_hi = _mm_mullo_epi16(s_hi, sa_hi);
    __m128i term2_hi = _mm_mullo_epi16(d_hi, invSa_hi);
    __m128i sum_hi = _mm_add_epi16(term1_hi, term2_hi);
    __m128i res_hi = _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(sum_hi, c1), _mm_srli_epi16(sum_hi, 8)), 8);

    __m128i alphaMask = _mm_set1_epi32(0xFF000000);
    __m128i packed = _mm_packus_epi16(res_lo, res_hi);
    packed = _mm_or_si128(packed, alphaMask);

    _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), packed);
}
#endif

void blendTile(uint32_t* dst, const uint32_t* src, float opacity, BlendMode mode, size_t pixelCount, bool isClippingMask) {
    if (!dst || !src || opacity <= 0.0f) return;

    uint32_t op256 = static_cast<uint32_t>(opacity * 256.0f + 0.5f);
    if (op256 == 0) return;

    if (isClippingMask) {
        for (size_t i = 0; i < pixelCount; ++i) {
            dst[i] = blendPixelClipped(dst[i], src[i], opacity, mode);
        }
        return;
    }

    // 1. 最頻出パス: BlendMode::Normal / PassThrough
    if (mode == BlendMode::Normal || mode == BlendMode::PassThrough) {
        size_t i = 0;

#if defined(KABURAGI_USE_SSE2)
        __m128i opVec = _mm_set1_epi16(static_cast<short>(op256));

        // 4 ピクセル単位で高速 SIMD 判定
        for (; i + 4 <= pixelCount; i += 4) {
            uint32_t s0 = src[i], s1 = src[i+1], s2 = src[i+2], s3 = src[i+3];
            // 4 つとも完全透明ならスキップ
            if (((s0 | s1 | s2 | s3) & 0xFF000000) == 0) {
                continue;
            }

            // 4 つとも完全不透明かつレイヤー不透明度100%なら直接コピー（最高速）
            if (op256 >= 256 && (s0 & s1 & s2 & s3 & 0xFF000000) == 0xFF000000) {
                _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst[i]), _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src[i])));
                continue;
            }

            uint32_t d0 = dst[i], d1 = dst[i+1], d2 = dst[i+2], d3 = dst[i+3];
            // 4 つとも下地が完全不透明 (da == 255) なら SSE2 高速ベクトル演算
            if ((d0 & d1 & d2 & d3 & 0xFF000000) == 0xFF000000) {
                blend4_SSE2_OpaqueDst(&dst[i], &src[i], opVec);
            } else {
                // 部分透明を含む場合は整数ブレンド
                dst[i]   = blendPixelNormalFast(d0, s0, op256);
                dst[i+1] = blendPixelNormalFast(d1, s1, op256);
                dst[i+2] = blendPixelNormalFast(d2, s2, op256);
                dst[i+3] = blendPixelNormalFast(d3, s3, op256);
            }
        }
#endif

        for (; i < pixelCount; ++i) {
            dst[i] = blendPixelNormalFast(dst[i], src[i], op256);
        }
        return;
    }

    // 2. その他のブレンドモード
    for (size_t i = 0; i < pixelCount; ++i) {
        dst[i] = blendPixel(dst[i], src[i], opacity, mode);
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

    uint32_t op256 = static_cast<uint32_t>(opacity * 256.0f + 0.5f);
    uint32_t maskOp256 = static_cast<uint32_t>(maskOpacity * 256.0f + 0.5f);
    if (op256 == 0 || maskOp256 == 0) return;

    for (size_t i = 0; i < pixelCount; ++i) {
        uint32_t maskPix = maskSrc[i];
        uint32_t ma = (maskPix >> 24) & 0xFF;
        if (ma == 0) continue;

        uint32_t srcPix = src[i];
        uint32_t sa = (srcPix >> 24) & 0xFF;
        if (sa == 0) continue;

        uint32_t effMa = (ma * maskOp256) >> 8;
        uint32_t effSa = (sa * op256) >> 8;
        uint32_t totalSa = div255(effSa * effMa);
        if (totalSa == 0) continue;

        uint32_t dstPix = dst[i];
        uint32_t da = (dstPix >> 24) & 0xFF;

        uint32_t sr = (srcPix >> 16) & 0xFF;
        uint32_t sg = (srcPix >> 8) & 0xFF;
        uint32_t sb = srcPix & 0xFF;

        uint32_t dr = (dstPix >> 16) & 0xFF;
        uint32_t dg = (dstPix >> 8) & 0xFF;
        uint32_t db = dstPix & 0xFF;

        uint32_t br = blendChannelInt(dr, sr, mode);
        uint32_t bg = blendChannelInt(dg, sg, mode);
        uint32_t bb = blendChannelInt(db, sb, mode);

        if (da == 255) {
            uint32_t invTotalSa = 255 - totalSa;
            uint32_t r = (br * totalSa + dr * invTotalSa + 128) >> 8;
            uint32_t g = (bg * totalSa + dg * invTotalSa + 128) >> 8;
            uint32_t b = (bb * totalSa + db * invTotalSa + 128) >> 8;
            dst[i] = (255 << 24) | (r << 16) | (g << 8) | b;
        } else {
            uint32_t invSa = 255 - totalSa;
            uint32_t da_term = div255(da * invSa);
            uint32_t outA = totalSa + da_term;
            if (outA == 0) continue;

            uint32_t outR = (br * totalSa + dr * da_term) / outA;
            uint32_t outG = (bg * totalSa + dg * da_term) / outA;
            uint32_t outB = (bb * totalSa + db * da_term) / outA;

            dst[i] = (outA << 24) | (outR << 16) | (outG << 8) | outB;
        }
    }
}

} // namespace Kaburagi
