#include "FilterProcessor.hpp"
#include "Layer.hpp"
#include <QPainter>
#include <algorithm>
#include <cmath>
#include <vector>

namespace Kaburagi {

// ----------------------------------------------------------------------------
// HSV / RGB 相互変換ユーティリティ
// ----------------------------------------------------------------------------
static inline void rgbToHsv(uint8_t r, uint8_t g, uint8_t b, float& h, float& s, float& v) {
    float rf = r / 255.0f, gf = g / 255.0f, bf = b / 255.0f;
    float max = std::max({rf, gf, bf});
    float min = std::min({rf, gf, bf});
    float delta = max - min;
    v = max;
    s = (max > 1e-5f) ? (delta / max) : 0.0f;
    if (delta <= 1e-5f) {
        h = 0.0f;
    } else if (max == rf) {
        h = 60.0f * std::fmod((gf - bf) / delta, 6.0f);
    } else if (max == gf) {
        h = 60.0f * (((bf - rf) / delta) + 2.0f);
    } else {
        h = 60.0f * (((rf - gf) / delta) + 4.0f);
    }
    if (h < 0.0f) h += 360.0f;
}

static inline void hsvToRgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b) {
    float c = v * s;
    float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float rf = 0, gf = 0, bf = 0;
    if (h < 60.0f) { rf = c; gf = x; }
    else if (h < 120.0f) { rf = x; gf = c; }
    else if (h < 180.0f) { gf = c; bf = x; }
    else if (h < 240.0f) { gf = x; bf = c; }
    else if (h < 300.0f) { rf = x; bf = c; }
    else { rf = c; bf = x; }
    r = static_cast<uint8_t>(std::clamp((rf + m) * 255.0f + 0.5f, 0.0f, 255.0f));
    g = static_cast<uint8_t>(std::clamp((gf + m) * 255.0f + 0.5f, 0.0f, 255.0f));
    b = static_cast<uint8_t>(std::clamp((bf + m) * 255.0f + 0.5f, 0.0f, 255.0f));
}

// ----------------------------------------------------------------------------
// 1. ガウスぼかし (2パス分離可能1次元畳み込み)
// ----------------------------------------------------------------------------
QImage FilterProcessor::applyGaussianBlur(const QImage& src, int radius) {
    if (src.isNull() || radius <= 0) return src;

    QImage img = src.format() == QImage::Format_ARGB32_Premultiplied
                 ? src
                 : src.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    int w = img.width();
    int h = img.height();
    if (w <= 0 || h <= 0) return src;

    // ガウシアンカーネルの作成
    int r = std::clamp(radius, 1, 100);
    float sigma = std::max(0.5f, r / 2.5f);
    int kSize = 2 * r + 1;
    std::vector<float> kernel(kSize);
    float kSum = 0.0f;
    for (int i = -r; i <= r; ++i) {
        float val = std::exp(-(i * i) / (2.0f * sigma * sigma));
        kernel[i + r] = val;
        kSum += val;
    }
    for (float& k : kernel) {
        k /= kSum;
    }

    // パス 1: 水平方向の畳み込み
    QImage temp(w, h, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < h; ++y) {
        const uint32_t* srcLine = reinterpret_cast<const uint32_t*>(img.constScanLine(y));
        uint32_t* dstLine = reinterpret_cast<uint32_t*>(temp.scanLine(y));

        for (int x = 0; x < w; ++x) {
            float a = 0.0f, red = 0.0f, green = 0.0f, blue = 0.0f;
            for (int k = -r; k <= r; ++k) {
                int sx = std::clamp(x + k, 0, w - 1);
                uint32_t px = srcLine[sx];
                float weight = kernel[k + r];
                a     += ((px >> 24) & 0xFF) * weight;
                red   += ((px >> 16) & 0xFF) * weight;
                green += ((px >>  8) & 0xFF) * weight;
                blue  += ( px        & 0xFF) * weight;
            }
            uint32_t outA = static_cast<uint32_t>(std::clamp(a + 0.5f, 0.0f, 255.0f));
            uint32_t outR = static_cast<uint32_t>(std::clamp(red + 0.5f, 0.0f, 255.0f));
            uint32_t outG = static_cast<uint32_t>(std::clamp(green + 0.5f, 0.0f, 255.0f));
            uint32_t outB = static_cast<uint32_t>(std::clamp(blue + 0.5f, 0.0f, 255.0f));
            dstLine[x] = (outA << 24) | (outR << 16) | (outG << 8) | outB;
        }
    }

    // パス 2: 垂直方向の畳み込み
    QImage result(w, h, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < h; ++y) {
        uint32_t* dstLine = reinterpret_cast<uint32_t*>(result.scanLine(y));
        for (int x = 0; x < w; ++x) {
            float a = 0.0f, red = 0.0f, green = 0.0f, blue = 0.0f;
            for (int k = -r; k <= r; ++k) {
                int sy = std::clamp(y + k, 0, h - 1);
                const uint32_t* line = reinterpret_cast<const uint32_t*>(temp.constScanLine(sy));
                uint32_t px = line[x];
                float weight = kernel[k + r];
                a     += ((px >> 24) & 0xFF) * weight;
                red   += ((px >> 16) & 0xFF) * weight;
                green += ((px >>  8) & 0xFF) * weight;
                blue  += ( px        & 0xFF) * weight;
            }
            uint32_t outA = static_cast<uint32_t>(std::clamp(a + 0.5f, 0.0f, 255.0f));
            uint32_t outR = static_cast<uint32_t>(std::clamp(red + 0.5f, 0.0f, 255.0f));
            uint32_t outG = static_cast<uint32_t>(std::clamp(green + 0.5f, 0.0f, 255.0f));
            uint32_t outB = static_cast<uint32_t>(std::clamp(blue + 0.5f, 0.0f, 255.0f));
            dstLine[x] = (outA << 24) | (outR << 16) | (outG << 8) | outB;
        }
    }

    return result;
}

// ----------------------------------------------------------------------------
// 2. 焦点ぼかし (放射状フォーカス / チルトシフト)
// ----------------------------------------------------------------------------
QImage FilterProcessor::applyFocusBlur(const QImage& src, int blurRadius,
                                      float focusRadiusRatio, float featherRatio,
                                      const QPointF& centerNorm, bool isTiltShift) {
    if (src.isNull()) return src;
    QImage orig = src.format() == QImage::Format_ARGB32_Premultiplied
                  ? src
                  : src.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    int w = orig.width();
    int h = orig.height();
    if (w <= 0 || h <= 0) return orig;

    QImage blurred = applyGaussianBlur(orig, blurRadius);
    QImage result(w, h, QImage::Format_ARGB32_Premultiplied);

    float cx = std::clamp(centerNorm.x(), 0.0, 1.0);
    float cy = std::clamp(centerNorm.y(), 0.0, 1.0);
    float fRadius = std::max(0.01f, focusRadiusRatio);
    float feather = std::max(0.01f, featherRatio);
    float aspect = (h > 0) ? (static_cast<float>(w) / h) : 1.0f;

    for (int y = 0; y < h; ++y) {
        float ny = static_cast<float>(y) / h;
        const uint32_t* origLine = reinterpret_cast<const uint32_t*>(orig.constScanLine(y));
        const uint32_t* blurLine = reinterpret_cast<const uint32_t*>(blurred.constScanLine(y));
        uint32_t* dstLine = reinterpret_cast<uint32_t*>(result.scanLine(y));

        for (int x = 0; x < w; ++x) {
            float nx = static_cast<float>(x) / w;
            float dist = 0.0f;
            if (isTiltShift) {
                // 水平チルトシフト: Y座標の距離
                dist = std::abs(ny - cy);
            } else {
                // 放射状・円形フォーカス: 中心からのアスペクト補正距離
                float dx = (nx - cx) * aspect;
                float dy = (ny - cy);
                dist = std::sqrt(dx * dx + dy * dy);
            }

            float t = 0.0f;
            if (dist <= fRadius) {
                t = 0.0f; // フォーカス内（鮮明）
            } else if (dist >= fRadius + feather) {
                t = 1.0f; // 完全なぼかし
            } else {
                // スムーズステップ補間
                float u = (dist - fRadius) / feather;
                t = u * u * (3.0f - 2.0f * u);
            }

            uint32_t p0 = origLine[x];
            uint32_t p1 = blurLine[x];

            uint32_t a0 = (p0 >> 24) & 0xFF, r0 = (p0 >> 16) & 0xFF, g0 = (p0 >> 8) & 0xFF, b0 = p0 & 0xFF;
            uint32_t a1 = (p1 >> 24) & 0xFF, r1 = (p1 >> 16) & 0xFF, g1 = (p1 >> 8) & 0xFF, b1 = p1 & 0xFF;

            uint32_t outA = static_cast<uint32_t>(a0 * (1.0f - t) + a1 * t + 0.5f);
            uint32_t outR = static_cast<uint32_t>(r0 * (1.0f - t) + r1 * t + 0.5f);
            uint32_t outG = static_cast<uint32_t>(g0 * (1.0f - t) + g1 * t + 0.5f);
            uint32_t outB = static_cast<uint32_t>(b0 * (1.0f - t) + b1 * t + 0.5f);

            dstLine[x] = (outA << 24) | (outR << 16) | (outG << 8) | outB;
        }
    }

    return result;
}

// ----------------------------------------------------------------------------
// 3. 明るさ - コントラスト (-100 ~ +100)
// ----------------------------------------------------------------------------
QImage FilterProcessor::applyBrightnessContrast(const QImage& src, float brightness, float contrast) {
    if (src.isNull()) return src;
    QImage orig = src.format() == QImage::Format_ARGB32_Premultiplied
                  ? src
                  : src.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    // 256要素LUTの生成
    float b = std::clamp(brightness, -100.0f, 100.0f) / 100.0f;
    float c = std::clamp(contrast, -100.0f, 100.0f) / 100.0f;
    float factor = (c >= 0.0f) ? (1.0f + c) / (1.0f - c + 1e-4f) : (1.0f + c);

    std::array<uint8_t, 256> lut{};
    for (int i = 0; i < 256; ++i) {
        float val = i / 255.0f;
        val = (val - 0.5f) * factor + 0.5f + b;
        lut[i] = static_cast<uint8_t>(std::clamp(val, 0.0f, 1.0f) * 255.0f + 0.5f);
    }

    int w = orig.width();
    int h = orig.height();
    QImage result(w, h, QImage::Format_ARGB32_Premultiplied);

    for (int y = 0; y < h; ++y) {
        const uint32_t* srcLine = reinterpret_cast<const uint32_t*>(orig.constScanLine(y));
        uint32_t* dstLine = reinterpret_cast<uint32_t*>(result.scanLine(y));

        for (int x = 0; x < w; ++x) {
            uint32_t px = srcLine[x];
            uint32_t a = (px >> 24) & 0xFF;
            if (a == 0) {
                dstLine[x] = 0;
                continue;
            }
            // プレマルチプライド解除
            float invA = 255.0f / a;
            uint8_t r = static_cast<uint8_t>(std::min(255.0f, ((px >> 16) & 0xFF) * invA + 0.5f));
            uint8_t g = static_cast<uint8_t>(std::min(255.0f, ((px >>  8) & 0xFF) * invA + 0.5f));
            uint8_t bColor = static_cast<uint8_t>(std::min(255.0f, (px & 0xFF) * invA + 0.5f));

            // LUT適用
            uint8_t nr = lut[r];
            uint8_t ng = lut[g];
            uint8_t nb = lut[bColor];

            // プレマルチプライド再適用
            float aRatio = a / 255.0f;
            uint32_t outR = static_cast<uint32_t>(nr * aRatio + 0.5f);
            uint32_t outG = static_cast<uint32_t>(ng * aRatio + 0.5f);
            uint32_t outB = static_cast<uint32_t>(nb * aRatio + 0.5f);

            dstLine[x] = (a << 24) | (outR << 16) | (outG << 8) | outB;
        }
    }

    return result;
}

// ----------------------------------------------------------------------------
// 4. 色相 / 彩度 / 明度
// ----------------------------------------------------------------------------
QImage FilterProcessor::applyHueSaturationLightness(const QImage& src, float hue, float saturation, float lightness) {
    if (src.isNull()) return src;
    QImage orig = src.format() == QImage::Format_ARGB32_Premultiplied
                  ? src
                  : src.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    int w = orig.width();
    int h = orig.height();
    QImage result(w, h, QImage::Format_ARGB32_Premultiplied);

    float hShift = std::clamp(hue, -180.0f, 180.0f);
    float sRatio = 1.0f + std::clamp(saturation, -100.0f, 100.0f) / 100.0f;
    float lShift = std::clamp(lightness, -100.0f, 100.0f) / 100.0f;

    for (int y = 0; y < h; ++y) {
        const uint32_t* srcLine = reinterpret_cast<const uint32_t*>(orig.constScanLine(y));
        uint32_t* dstLine = reinterpret_cast<uint32_t*>(result.scanLine(y));

        for (int x = 0; x < w; ++x) {
            uint32_t px = srcLine[x];
            uint32_t a = (px >> 24) & 0xFF;
            if (a == 0) {
                dstLine[x] = 0;
                continue;
            }

            // プレマルチプライド解除
            float invA = 255.0f / a;
            uint8_t r = static_cast<uint8_t>(std::min(255.0f, ((px >> 16) & 0xFF) * invA + 0.5f));
            uint8_t g = static_cast<uint8_t>(std::min(255.0f, ((px >>  8) & 0xFF) * invA + 0.5f));
            uint8_t bColor = static_cast<uint8_t>(std::min(255.0f, (px & 0xFF) * invA + 0.5f));

            float curH, curS, curV;
            rgbToHsv(r, g, bColor, curH, curS, curV);

            // 色相シフト
            curH = std::fmod(curH + hShift + 360.0f, 360.0f);
            // 彩度調整
            curS = std::clamp(curS * sRatio, 0.0f, 1.0f);
            // 明度調整
            curV = std::clamp(curV + lShift, 0.0f, 1.0f);

            uint8_t outR, outG, outB;
            hsvToRgb(curH, curS, curV, outR, outG, outB);

            // プレマルチプライド再適用
            float aRatio = a / 255.0f;
            uint32_t pr = static_cast<uint32_t>(outR * aRatio + 0.5f);
            uint32_t pg = static_cast<uint32_t>(outG * aRatio + 0.5f);
            uint32_t pb = static_cast<uint32_t>(outB * aRatio + 0.5f);

            dstLine[x] = (a << 24) | (pr << 16) | (pg << 8) | pb;
        }
    }

    return result;
}

// ----------------------------------------------------------------------------
// 5. トーンカーブ
// ----------------------------------------------------------------------------
QImage FilterProcessor::applyToneCurve(const QImage& src,
                                      const std::array<std::vector<QPointF>, 4>& curves,
                                      CurveInterpolationMode mode) {
    if (src.isNull()) return src;
    QImage orig = src.format() == QImage::Format_ARGB32_Premultiplied
                  ? src
                  : src.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    auto lutRGB = AdjustmentLayer::calculateCurveLUT(curves[0], mode);
    auto lutR   = AdjustmentLayer::calculateCurveLUT(curves[1], mode);
    auto lutG   = AdjustmentLayer::calculateCurveLUT(curves[2], mode);
    auto lutB   = AdjustmentLayer::calculateCurveLUT(curves[3], mode);

    std::array<uint8_t, 256> finalR{};
    std::array<uint8_t, 256> finalG{};
    std::array<uint8_t, 256> finalB{};

    for (int i = 0; i < 256; ++i) {
        finalR[i] = lutR[lutRGB[i]];
        finalG[i] = lutG[lutRGB[i]];
        finalB[i] = lutB[lutRGB[i]];
    }

    int w = orig.width();
    int h = orig.height();
    QImage result(w, h, QImage::Format_ARGB32_Premultiplied);

    for (int y = 0; y < h; ++y) {
        const uint32_t* srcLine = reinterpret_cast<const uint32_t*>(orig.constScanLine(y));
        uint32_t* dstLine = reinterpret_cast<uint32_t*>(result.scanLine(y));

        for (int x = 0; x < w; ++x) {
            uint32_t px = srcLine[x];
            uint32_t a = (px >> 24) & 0xFF;
            if (a == 0) {
                dstLine[x] = 0;
                continue;
            }

            // プレマルチプライド解除
            float invA = 255.0f / a;
            uint8_t r = static_cast<uint8_t>(std::min(255.0f, ((px >> 16) & 0xFF) * invA + 0.5f));
            uint8_t g = static_cast<uint8_t>(std::min(255.0f, ((px >>  8) & 0xFF) * invA + 0.5f));
            uint8_t bColor = static_cast<uint8_t>(std::min(255.0f, (px & 0xFF) * invA + 0.5f));

            uint8_t nr = finalR[r];
            uint8_t ng = finalG[g];
            uint8_t nb = finalB[bColor];

            // プレマルチプライド再適用
            float aRatio = a / 255.0f;
            uint32_t pr = static_cast<uint32_t>(nr * aRatio + 0.5f);
            uint32_t pg = static_cast<uint32_t>(ng * aRatio + 0.5f);
            uint32_t pb = static_cast<uint32_t>(nb * aRatio + 0.5f);

            dstLine[x] = (a << 24) | (pr << 16) | (pg << 8) | pb;
        }
    }

    return result;
}

// ----------------------------------------------------------------------------
// 6. モーションぼかし (移動距離 1~100px, 角度 -180°~180°)
// ----------------------------------------------------------------------------
QImage FilterProcessor::applyMotionBlur(const QImage& src, int distance, float angleDeg) {
    if (src.isNull()) return src;
    if (distance <= 1) return src.copy();

    QImage orig = src.format() == QImage::Format_ARGB32_Premultiplied
                  ? src
                  : src.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    int w = orig.width();
    int h = orig.height();
    QImage result(w, h, QImage::Format_ARGB32_Premultiplied);

    float rad = qDegreesToRadians(angleDeg);
    float cosA = std::cos(rad);
    float sinA = std::sin(rad);

    int numSamples = std::max(3, distance);
    float halfDist = distance * 0.5f;

    for (int y = 0; y < h; ++y) {
        uint32_t* dstLine = reinterpret_cast<uint32_t*>(result.scanLine(y));

        for (int x = 0; x < w; ++x) {
            float sumA = 0.0f, sumR = 0.0f, sumG = 0.0f, sumB = 0.0f;
            float totalWeight = 0.0f;

            for (int s = 0; s < numSamples; ++s) {
                float t = (numSamples > 1) ? (float(s) / (numSamples - 1) - 0.5f) : 0.0f;
                float distOffset = t * 2.0f * halfDist;
                float sx = x + distOffset * cosA;
                float sy = y + distOffset * sinA;

                // バイリニア補間サンプリング
                int x0 = static_cast<int>(std::floor(sx));
                int y0 = static_cast<int>(std::floor(sy));
                int x1 = x0 + 1;
                int y1 = y0 + 1;

                float fx = sx - x0;
                float fy = sy - y0;

                x0 = std::clamp(x0, 0, w - 1);
                x1 = std::clamp(x1, 0, w - 1);
                y0 = std::clamp(y0, 0, h - 1);
                y1 = std::clamp(y1, 0, h - 1);

                const uint32_t* l0 = reinterpret_cast<const uint32_t*>(orig.constScanLine(y0));
                const uint32_t* l1 = reinterpret_cast<const uint32_t*>(orig.constScanLine(y1));

                uint32_t p00 = l0[x0], p10 = l0[x1];
                uint32_t p01 = l1[x0], p11 = l1[x1];

                auto interpChannel = [&](int shift) {
                    float v00 = (p00 >> shift) & 0xFF;
                    float v10 = (p10 >> shift) & 0xFF;
                    float v01 = (p01 >> shift) & 0xFF;
                    float v11 = (p11 >> shift) & 0xFF;
                    return (v00 * (1.0f - fx) + v10 * fx) * (1.0f - fy) +
                           (v01 * (1.0f - fx) + v11 * fx) * fy;
                };

                sumA += interpChannel(24);
                sumR += interpChannel(16);
                sumG += interpChannel(8);
                sumB += interpChannel(0);
                totalWeight += 1.0f;
            }

            float invW = 1.0f / totalWeight;
            uint32_t outA = std::clamp(static_cast<int>(sumA * invW + 0.5f), 0, 255);
            uint32_t outR = std::clamp(static_cast<int>(sumR * invW + 0.5f), 0, 255);
            uint32_t outG = std::clamp(static_cast<int>(sumG * invW + 0.5f), 0, 255);
            uint32_t outB = std::clamp(static_cast<int>(sumB * invW + 0.5f), 0, 255);

            dstLine[x] = (outA << 24) | (outR << 16) | (outG << 8) | outB;
        }
    }

    return result;
}

// ----------------------------------------------------------------------------
// 7. グラデーションマップ
// ----------------------------------------------------------------------------
QImage FilterProcessor::applyGradientMap(const QImage& src,
                                         const std::vector<QPair<float, QColor>>& stops,
                                         bool reverse, float opacity) {
    if (src.isNull()) return src;
    QImage orig = src.format() == QImage::Format_ARGB32_Premultiplied
                  ? src
                  : src.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    // ソート済みストップの準備
    std::vector<QPair<float, QColor>> sortedStops = stops;
    if (sortedStops.empty()) {
        sortedStops.push_back({0.0f, QColor(0, 0, 0)});
        sortedStops.push_back({1.0f, QColor(255, 255, 255)});
    }
    std::sort(sortedStops.begin(), sortedStops.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    if (sortedStops.front().first > 0.0f) {
        sortedStops.insert(sortedStops.begin(), {0.0f, sortedStops.front().second});
    }
    if (sortedStops.back().first < 1.0f) {
        sortedStops.push_back({1.0f, sortedStops.back().second});
    }

    // 256要素LUTの生成
    std::array<QColor, 256> lut;
    for (int i = 0; i < 256; ++i) {
        float pos = i / 255.0f;
        if (reverse) pos = 1.0f - pos;

        // 線形補間
        QColor col = sortedStops.back().second;
        for (size_t s = 0; s + 1 < sortedStops.size(); ++s) {
            float p0 = sortedStops[s].first;
            float p1 = sortedStops[s + 1].first;
            if (pos >= p0 && pos <= p1) {
                float t = (p1 > p0) ? (pos - p0) / (p1 - p0) : 0.0f;
                const QColor& c0 = sortedStops[s].second;
                const QColor& c1 = sortedStops[s + 1].second;
                int r = static_cast<int>(c0.red() * (1.0f - t) + c1.red() * t + 0.5f);
                int g = static_cast<int>(c0.green() * (1.0f - t) + c1.green() * t + 0.5f);
                int b = static_cast<int>(c0.blue() * (1.0f - t) + c1.blue() * t + 0.5f);
                col = QColor(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255));
                break;
            }
        }
        lut[i] = col;
    }

    int w = orig.width();
    int h = orig.height();
    QImage result(w, h, QImage::Format_ARGB32_Premultiplied);
    float clampedOpacity = std::clamp(opacity, 0.0f, 1.0f);

    for (int y = 0; y < h; ++y) {
        const uint32_t* srcLine = reinterpret_cast<const uint32_t*>(orig.constScanLine(y));
        uint32_t* dstLine = reinterpret_cast<uint32_t*>(result.scanLine(y));

        for (int x = 0; x < w; ++x) {
            uint32_t px = srcLine[x];
            uint32_t a = (px >> 24) & 0xFF;
            if (a == 0) {
                dstLine[x] = 0;
                continue;
            }

            float invA = 255.0f / a;
            float rOrig = std::min(255.0f, ((px >> 16) & 0xFF) * invA);
            float gOrig = std::min(255.0f, ((px >> 8) & 0xFF) * invA);
            float bOrig = std::min(255.0f, (px & 0xFF) * invA);

            // BT.601 輝度計算
            int lum = std::clamp(static_cast<int>(0.299f * rOrig + 0.587f * gOrig + 0.114f * bOrig + 0.5f), 0, 255);
            const QColor& mappedColor = lut[lum];

            float rTarget = mappedColor.red();
            float gTarget = mappedColor.green();
            float bTarget = mappedColor.blue();

            // 不透明度ブレンド
            float finalR = rOrig * (1.0f - clampedOpacity) + rTarget * clampedOpacity;
            float finalG = gOrig * (1.0f - clampedOpacity) + gTarget * clampedOpacity;
            float finalB = bOrig * (1.0f - clampedOpacity) + bTarget * clampedOpacity;

            // プレマルチプライド再適用
            float aRatio = a / 255.0f;
            uint32_t outR = std::clamp(static_cast<int>(finalR * aRatio + 0.5f), 0, 255);
            uint32_t outG = std::clamp(static_cast<int>(finalG * aRatio + 0.5f), 0, 255);
            uint32_t outB = std::clamp(static_cast<int>(finalB * aRatio + 0.5f), 0, 255);

            dstLine[x] = (a << 24) | (outR << 16) | (outG << 8) | outB;
        }
    }

    return result;
}

// ----------------------------------------------------------------------------
// 選択範囲がある場合のブレンド合成
// ----------------------------------------------------------------------------
void FilterProcessor::blendWithSelection(QImage& dst, const QImage& orig,
                                        const QPainterPath& selectionPath, int width, int height) {
    if (selectionPath.isEmpty() || dst.isNull() || orig.isNull()) return;

    // 選択範囲マスクの生成 (白: 選択範囲内、黒: 選択範囲外)
    QImage mask(width, height, QImage::Format_Grayscale8);
    mask.fill(0);
    {
        QPainter p(&mask);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillPath(selectionPath, Qt::white);
    }

    int w = std::min({dst.width(), orig.width(), width});
    int h = std::min({dst.height(), orig.height(), height});

    for (int y = 0; y < h; ++y) {
        const uint8_t* maskLine = mask.constScanLine(y);
        const uint32_t* origLine = reinterpret_cast<const uint32_t*>(orig.constScanLine(y));
        uint32_t* dstLine = reinterpret_cast<uint32_t*>(dst.scanLine(y));

        for (int x = 0; x < w; ++x) {
            uint8_t m = maskLine[x];
            if (m == 0) {
                // 選択範囲外: 元のピクセルを保持
                dstLine[x] = origLine[x];
            } else if (m < 255) {
                // アンチエイリアス境界: アルファブレンド
                float t = m / 255.0f;
                uint32_t p0 = origLine[x];
                uint32_t p1 = dstLine[x];
                uint32_t a0 = (p0 >> 24) & 0xFF, r0 = (p0 >> 16) & 0xFF, g0 = (p0 >> 8) & 0xFF, b0 = p0 & 0xFF;
                uint32_t a1 = (p1 >> 24) & 0xFF, r1 = (p1 >> 16) & 0xFF, g1 = (p1 >> 8) & 0xFF, b1 = p1 & 0xFF;

                uint32_t outA = static_cast<uint32_t>(a0 * (1.0f - t) + a1 * t + 0.5f);
                uint32_t outR = static_cast<uint32_t>(r0 * (1.0f - t) + r1 * t + 0.5f);
                uint32_t outG = static_cast<uint32_t>(g0 * (1.0f - t) + g1 * t + 0.5f);
                uint32_t outB = static_cast<uint32_t>(b0 * (1.0f - t) + b1 * t + 0.5f);
                dstLine[x] = (outA << 24) | (outR << 16) | (outG << 8) | outB;
            }
            // m == 255 の場合は既に dstLine[x] にフィルター結果が入っているためそのまま
        }
    }
}

void FilterProcessor::blendWithSelectionMask(QImage& dst, const QImage& orig,
                                            const Layer* maskLayer, int width, int height) {
    if (!maskLayer) return;
    int w = std::min({dst.width(), orig.width(), width});
    int h = std::min({dst.height(), orig.height(), height});

    int lastTx = -1, lastTy = -1;
    const uint32_t* maskPixels = nullptr;

    for (int y = 0; y < h; ++y) {
        int ty = y / TileMemoryPool::TILE_HEIGHT;
        int ly = y % TileMemoryPool::TILE_HEIGHT;
        const uint32_t* origLine = reinterpret_cast<const uint32_t*>(orig.constScanLine(y));
        uint32_t* dstLine = reinterpret_cast<uint32_t*>(dst.scanLine(y));

        for (int x = 0; x < w; ++x) {
            int tx = x / TileMemoryPool::TILE_WIDTH;
            int lx = x % TileMemoryPool::TILE_WIDTH;

            if (tx != lastTx || ty != lastTy) {
                lastTx = tx;
                lastTy = ty;
                const Tile* t = maskLayer->tileManager().getTile(tx, ty);
                maskPixels = (t && t->readBuffer()) ? reinterpret_cast<const uint32_t*>(t->readBuffer()) : nullptr;
            }

            uint8_t m = 0;
            if (maskPixels) {
                m = static_cast<uint8_t>((maskPixels[ly * TileMemoryPool::TILE_WIDTH + lx] >> 24) & 0xFF);
            }

            if (m == 0) {
                dstLine[x] = origLine[x];
            } else if (m < 255) {
                float t = m / 255.0f;
                uint32_t p0 = origLine[x];
                uint32_t p1 = dstLine[x];
                uint32_t a0 = (p0 >> 24) & 0xFF, r0 = (p0 >> 16) & 0xFF, g0 = (p0 >> 8) & 0xFF, b0 = p0 & 0xFF;
                uint32_t a1 = (p1 >> 24) & 0xFF, r1 = (p1 >> 16) & 0xFF, g1 = (p1 >> 8) & 0xFF, b1 = p1 & 0xFF;

                uint32_t outA = static_cast<uint32_t>(a0 * (1.0f - t) + a1 * t + 0.5f);
                uint32_t outR = static_cast<uint32_t>(r0 * (1.0f - t) + r1 * t + 0.5f);
                uint32_t outG = static_cast<uint32_t>(g0 * (1.0f - t) + g1 * t + 0.5f);
                uint32_t outB = static_cast<uint32_t>(b0 * (1.0f - t) + b1 * t + 0.5f);
                dstLine[x] = (outA << 24) | (outR << 16) | (outG << 8) | outB;
            }
        }
    }
}

// ----------------------------------------------------------------------------
// 8. 輝度を透明度に
// ----------------------------------------------------------------------------
QImage FilterProcessor::convertBrightnessToAlpha(const QImage& src, const QColor& lineColor) {
    if (src.isNull()) return src;

    QImage img = src.convertToFormat(QImage::Format_ARGB32);
    int width = img.width();
    int height = img.height();

    QImage result(width, height, QImage::Format_ARGB32_Premultiplied);
    int lr = lineColor.red();
    int lg = lineColor.green();
    int lb = lineColor.blue();

    for (int y = 0; y < height; ++y) {
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        QRgb* dstLine = reinterpret_cast<QRgb*>(result.scanLine(y));

        for (int x = 0; x < width; ++x) {
            QRgb p = srcLine[x];
            int origA = qAlpha(p);
            if (origA == 0) {
                dstLine[x] = 0;
                continue;
            }

            // ITU-R BT.601 輝度
            int brightness = static_cast<int>(0.299 * qRed(p) + 0.587 * qGreen(p) + 0.114 * qBlue(p));
            int alpha = std::clamp(255 - brightness, 0, 255);
            alpha = (alpha * origA) / 255;

            if (alpha <= 0) {
                dstLine[x] = 0;
            } else {
                // Premultiplied ARGB32
                dstLine[x] = qRgba((lr * alpha) / 255, (lg * alpha) / 255, (lb * alpha) / 255, alpha);
            }
        }
    }

    return result;
}

// ----------------------------------------------------------------------------
// 9. 色を透明度に (GIMP gegl:color-to-alpha 準拠)
// ----------------------------------------------------------------------------
QImage FilterProcessor::colorToAlpha(const QImage& src, const QColor& targetColor,
                                     float transparencyThreshold, float opacityThreshold) {
    if (src.isNull()) return src;

    QImage img = src.convertToFormat(QImage::Format_ARGB32);
    int width = img.width();
    int height = img.height();

    QImage result(width, height, QImage::Format_ARGB32_Premultiplied);

    float r0 = targetColor.redF();
    float g0 = targetColor.greenF();
    float b0 = targetColor.blueF();

    float tLow = std::clamp(transparencyThreshold, 0.0f, 1.0f);
    float tHigh = std::clamp(opacityThreshold, 0.0f, 1.0f);
    if (tHigh < tLow) std::swap(tLow, tHigh);

    for (int y = 0; y < height; ++y) {
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        QRgb* dstLine = reinterpret_cast<QRgb*>(result.scanLine(y));

        for (int x = 0; x < width; ++x) {
            QRgb p = srcLine[x];
            int origA = qAlpha(p);
            if (origA == 0) {
                dstLine[x] = 0;
                continue;
            }

            float r = qRed(p) / 255.0f;
            float g = qGreen(p) / 255.0f;
            float b = qBlue(p) / 255.0f;

            // GIMP (GEGL) color-to-alpha: 各チャンネルの距離比率を計算
            float dr = 0.0f;
            if (r > r0) {
                if (r0 < 1.0f) dr = (r - r0) / (1.0f - r0);
            } else if (r < r0) {
                if (r0 > 0.0f) dr = (r0 - r) / r0;
            }

            float dg = 0.0f;
            if (g > g0) {
                if (g0 < 1.0f) dg = (g - g0) / (1.0f - g0);
            } else if (g < g0) {
                if (g0 > 0.0f) dg = (g0 - g) / g0;
            }

            float db = 0.0f;
            if (b > b0) {
                if (b0 < 1.0f) db = (b - b0) / (1.0f - b0);
            } else if (b < b0) {
                if (b0 > 0.0f) db = (b0 - b) / b0;
            }

            float alphaFactor = std::max({dr, dg, db});

            // しきい値の適用
            if (alphaFactor <= tLow) {
                alphaFactor = 0.0f;
            } else if (alphaFactor >= tHigh) {
                alphaFactor = 1.0f;
            } else if (tHigh - tLow > 1e-5f) {
                alphaFactor = (alphaFactor - tLow) / (tHigh - tLow);
            }

            float newAlphaF = (origA / 255.0f) * alphaFactor;
            int newAlpha = std::clamp(static_cast<int>(newAlphaF * 255.0f + 0.5f), 0, 255);

            if (newAlpha <= 0 || alphaFactor <= 1e-5f) {
                dstLine[x] = 0;
            } else {
                // 元の色からターゲット色成分を除去して色を再構築
                // C' = C_0 + (C - C_0) / alphaFactor
                float newR = r0 + (r - r0) / alphaFactor;
                float newG = g0 + (g - g0) / alphaFactor;
                float newB = b0 + (b - b0) / alphaFactor;

                newR = std::clamp(newR, 0.0f, 1.0f);
                newG = std::clamp(newG, 0.0f, 1.0f);
                newB = std::clamp(newB, 0.0f, 1.0f);

                // Premultiplied ARGB32
                int pr = static_cast<int>(newR * newAlpha + 0.5f);
                int pg = static_cast<int>(newG * newAlpha + 0.5f);
                int pb = static_cast<int>(newB * newAlpha + 0.5f);
                dstLine[x] = qRgba(pr, pg, pb, newAlpha);
            }
        }
    }

    return result;
}

} // namespace Kaburagi


