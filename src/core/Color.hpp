#pragma once

#include <cstdint>
#include <algorithm>
#include <cmath>

namespace Kaburagi {

enum class ColorModel {
    RGB,
    CMYK
};

enum class CanvasColorMode {
    RGB = 0,        // 通常の RGB モード
    CMYKProof = 1,  // RGB 作業 + CMYK 印刷プルーフ (ソフトプルーフ) 表示
    CMYKNative = 2  // CMYK ネイティブ (減法混色インキモデル / CMYK 色域制限)
};

struct ColorRGBA {
    float r = 0.0f; // 0.0 ~ 1.0
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;

    static ColorRGBA fromUint32(uint32_t argb) {
        ColorRGBA c;
        c.a = ((argb >> 24) & 0xFF) / 255.0f;
        c.r = ((argb >> 16) & 0xFF) / 255.0f;
        c.g = ((argb >> 8) & 0xFF) / 255.0f;
        c.b = (argb & 0xFF) / 255.0f;
        return c;
    }

    uint32_t toUint32() const {
        uint8_t a8 = static_cast<uint8_t>(std::clamp(a, 0.0f, 1.0f) * 255.0f + 0.5f);
        uint8_t r8 = static_cast<uint8_t>(std::clamp(r, 0.0f, 1.0f) * 255.0f + 0.5f);
        uint8_t g8 = static_cast<uint8_t>(std::clamp(g, 0.0f, 1.0f) * 255.0f + 0.5f);
        uint8_t b8 = static_cast<uint8_t>(std::clamp(b, 0.0f, 1.0f) * 255.0f + 0.5f);
        return (a8 << 24) | (r8 << 16) | (g8 << 8) | b8;
    }
};

struct ColorCMYK {
    float c = 0.0f; // 0.0 ~ 1.0
    float m = 0.0f;
    float y = 0.0f;
    float k = 0.0f;
    float a = 1.0f;

    static ColorCMYK fromRGB(const ColorRGBA& rgb) {
        ColorCMYK cmyk;
        cmyk.a = rgb.a;
        float r = std::clamp(rgb.r, 0.0f, 1.0f);
        float g = std::clamp(rgb.g, 0.0f, 1.0f);
        float b = std::clamp(rgb.b, 0.0f, 1.0f);
        cmyk.k = 1.0f - std::max({r, g, b});
        if (cmyk.k < 1.0f) {
            cmyk.c = (1.0f - r - cmyk.k) / (1.0f - cmyk.k);
            cmyk.m = (1.0f - g - cmyk.k) / (1.0f - cmyk.k);
            cmyk.y = (1.0f - b - cmyk.k) / (1.0f - cmyk.k);
        } else {
            cmyk.c = 0.0f;
            cmyk.m = 0.0f;
            cmyk.y = 0.0f;
        }
        return cmyk;
    }

    ColorRGBA toRGB() const {
        ColorRGBA rgb;
        rgb.a = a;
        float cClamped = std::clamp(c, 0.0f, 1.0f);
        float mClamped = std::clamp(m, 0.0f, 1.0f);
        float yClamped = std::clamp(y, 0.0f, 1.0f);
        float kClamped = std::clamp(k, 0.0f, 1.0f);
        rgb.r = (1.0f - cClamped) * (1.0f - kClamped);
        rgb.g = (1.0f - mClamped) * (1.0f - kClamped);
        rgb.b = (1.0f - yClamped) * (1.0f - kClamped);
        return rgb;
    }

    // ARGB32 <-> CMYKA パック変換
    static uint32_t rgbToCmykInt(uint32_t argb) {
        ColorRGBA rgba = ColorRGBA::fromUint32(argb);
        ColorCMYK cmyk = fromRGB(rgba);
        uint8_t c8 = static_cast<uint8_t>(std::clamp(cmyk.c, 0.0f, 1.0f) * 255.0f + 0.5f);
        uint8_t m8 = static_cast<uint8_t>(std::clamp(cmyk.m, 0.0f, 1.0f) * 255.0f + 0.5f);
        uint8_t y8 = static_cast<uint8_t>(std::clamp(cmyk.y, 0.0f, 1.0f) * 255.0f + 0.5f);
        uint8_t k8 = static_cast<uint8_t>(std::clamp(cmyk.k, 0.0f, 1.0f) * 255.0f + 0.5f);
        return (c8 << 24) | (m8 << 16) | (y8 << 8) | k8;
    }

    static uint32_t cmykToRgbInt(uint32_t cmykPack, uint8_t alpha = 255) {
        float c = ((cmykPack >> 24) & 0xFF) / 255.0f;
        float m = ((cmykPack >> 16) & 0xFF) / 255.0f;
        float y = ((cmykPack >> 8) & 0xFF) / 255.0f;
        float k = (cmykPack & 0xFF) / 255.0f;
        ColorCMYK cmyk{c, m, y, k, alpha / 255.0f};
        return cmyk.toRGB().toUint32();
    }
};

} // namespace Kaburagi
