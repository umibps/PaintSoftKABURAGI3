#include "AdjustmentLayer.hpp"
#include <algorithm>
#include <cmath>

namespace Kaburagi {

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

AdjustmentLayer::AdjustmentLayer(const QString& name, int width, int height, AdjustmentType adjType)
    : Layer(name, width, height, LayerType::Adjustment), m_adjType(adjType) {
    resetAllCurves();
    resetAllLevels();
    updateLUT();
}

void AdjustmentLayer::setBrightnessContrast(float brightness, float contrast) {
    m_brightness = brightness;
    m_contrast = contrast;
    updateLUT();
}

void AdjustmentLayer::setHueSaturation(float hueShift, float saturationRatio, float lightnessShift) {
    m_hueShift = hueShift;
    m_saturation = saturationRatio;
    m_lightness = lightnessShift;
}

void AdjustmentLayer::setCurvePoints(AdjustmentChannel ch, const std::vector<QPointF>& points) {
    m_curvePoints[static_cast<size_t>(ch)] = points;
    updateLUT();
}

void AdjustmentLayer::setCurveInterpolationMode(CurveInterpolationMode mode) {
    m_curveInterpMode = mode;
    updateLUT();
}

void AdjustmentLayer::resetCurve(AdjustmentChannel ch) {
    m_curvePoints[static_cast<size_t>(ch)] = { QPointF(0.0, 0.0), QPointF(255.0, 255.0) };
    updateLUT();
}

void AdjustmentLayer::resetAllCurves() {
    for (size_t i = 0; i < 4; ++i) {
        m_curvePoints[i] = { QPointF(0.0, 0.0), QPointF(255.0, 255.0) };
    }
    updateLUT();
}

void AdjustmentLayer::setLevelParams(AdjustmentChannel ch, const LevelParams& params) {
    m_levelParams[static_cast<size_t>(ch)] = params;
    updateLUT();
}

void AdjustmentLayer::resetLevels(AdjustmentChannel ch) {
    m_levelParams[static_cast<size_t>(ch)] = LevelParams();
    updateLUT();
}

void AdjustmentLayer::resetAllLevels() {
    for (size_t i = 0; i < 4; ++i) {
        m_levelParams[i] = LevelParams();
    }
    updateLUT();
}

std::array<uint8_t, 256> AdjustmentLayer::calculateCurveLUT(const std::vector<QPointF>& points, CurveInterpolationMode mode) {
    std::array<uint8_t, 256> lut;
    for (int i = 0; i < 256; ++i) lut[i] = static_cast<uint8_t>(i);
    if (points.size() < 2) return lut;

    std::vector<QPointF> sortedPts = points;
    std::sort(sortedPts.begin(), sortedPts.end(), [](const QPointF& a, const QPointF& b) {
        return a.x() < b.x();
    });

    int n = static_cast<int>(sortedPts.size());

    if (mode == CurveInterpolationMode::Linear) {
        int ptIdx = 0;
        for (int x = 0; x < 256; ++x) {
            while (ptIdx < n - 1 && sortedPts[ptIdx + 1].x() < x) {
                ptIdx++;
            }
            if (ptIdx >= n - 1) {
                lut[x] = static_cast<uint8_t>(std::clamp(sortedPts.back().y(), 0.0, 255.0));
            } else if (x <= sortedPts[0].x()) {
                lut[x] = static_cast<uint8_t>(std::clamp(sortedPts[0].y(), 0.0, 255.0));
            } else {
                qreal x0 = sortedPts[ptIdx].x();
                qreal x1 = sortedPts[ptIdx + 1].x();
                qreal y0 = sortedPts[ptIdx].y();
                qreal y1 = sortedPts[ptIdx + 1].y();
                qreal t = (x1 > x0) ? ((x - x0) / (x1 - x0)) : 0.0;
                qreal y = y0 + t * (y1 - y0);
                lut[x] = static_cast<uint8_t>(std::clamp(y + 0.5, 0.0, 255.0));
            }
        }
    } else {
        // 単調3次エルミートスプライン (Monotone Cubic Spline)
        std::vector<double> d(n), m(n - 1);
        for (int i = 0; i < n - 1; ++i) {
            double dx = sortedPts[i + 1].x() - sortedPts[i].x();
            double dy = sortedPts[i + 1].y() - sortedPts[i].y();
            m[i] = (dx != 0.0) ? (dy / dx) : 0.0;
        }

        d[0] = m[0];
        for (int i = 1; i < n - 1; ++i) {
            if (m[i - 1] * m[i] <= 0.0) {
                d[i] = 0.0;
            } else {
                d[i] = (m[i - 1] + m[i]) * 0.5;
            }
        }
        d[n - 1] = m[n - 2];

        // Fritsch & Carlson による単調性制約
        for (int i = 0; i < n - 1; ++i) {
            if (m[i] == 0.0) {
                d[i] = 0.0;
                d[i + 1] = 0.0;
            } else {
                double a = d[i] / m[i];
                double b = d[i + 1] / m[i];
                if (a * a + b * b > 9.0) {
                    double tau = 3.0 / std::hypot(a, b);
                    d[i] = tau * a * m[i];
                    d[i + 1] = tau * b * m[i];
                }
            }
        }

        int ptIdx = 0;
        for (int x = 0; x < 256; ++x) {
            while (ptIdx < n - 1 && sortedPts[ptIdx + 1].x() < x) {
                ptIdx++;
            }
            if (ptIdx >= n - 1) {
                lut[x] = static_cast<uint8_t>(std::clamp(sortedPts.back().y(), 0.0, 255.0));
            } else if (x <= sortedPts[0].x()) {
                lut[x] = static_cast<uint8_t>(std::clamp(sortedPts[0].y(), 0.0, 255.0));
            } else {
                double h = sortedPts[ptIdx + 1].x() - sortedPts[ptIdx].x();
                if (h <= 0.0) {
                    lut[x] = static_cast<uint8_t>(std::clamp(sortedPts[ptIdx].y(), 0.0, 255.0));
                } else {
                    double t = (x - sortedPts[ptIdx].x()) / h;
                    double t2 = t * t;
                    double t3 = t2 * t;
                    double h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
                    double h10 = t3 - 2.0 * t2 + t;
                    double h01 = -2.0 * t3 + 3.0 * t2;
                    double h11 = t3 - t2;
                    double y = h00 * sortedPts[ptIdx].y() + h10 * h * d[ptIdx] + h01 * sortedPts[ptIdx + 1].y() + h11 * h * d[ptIdx + 1];
                    lut[x] = static_cast<uint8_t>(std::clamp(y + 0.5, 0.0, 255.0));
                }
            }
        }
    }

    return lut;
}

std::array<uint8_t, 256> AdjustmentLayer::calculateLevelLUT(const LevelParams& params) {
    std::array<uint8_t, 256> lut;
    float inRange = std::max(1, params.inHighlight - params.inShadow);
    float outRange = static_cast<float>(params.outHighlight - params.outShadow);
    float invGamma = 1.0f / std::max(0.01f, params.inGamma);

    for (int i = 0; i < 256; ++i) {
        float norm = std::clamp((i - params.inShadow) / inRange, 0.0f, 1.0f);
        norm = std::pow(norm, invGamma);
        float outVal = params.outShadow + norm * outRange;
        lut[i] = static_cast<uint8_t>(std::clamp(outVal + 0.5f, 0.0f, 255.0f));
    }
    return lut;
}

void AdjustmentLayer::updateLUT() {
    if (m_adjType == AdjustmentType::BrightnessContrast) {
        float factor = (1.0f + m_contrast) / (1.0f - m_contrast + 1e-5f);
        for (int i = 0; i < 256; ++i) {
            float val = i / 255.0f;
            val = (val - 0.5f) * factor + 0.5f + m_brightness;
            uint8_t outVal = static_cast<uint8_t>(std::clamp(val, 0.0f, 1.0f) * 255.0f + 0.5f);
            m_lutR[i] = outVal;
            m_lutG[i] = outVal;
            m_lutB[i] = outVal;
        }
    } else if (m_adjType == AdjustmentType::Invert) {
        for (int i = 0; i < 256; ++i) {
            uint8_t outVal = static_cast<uint8_t>(255 - i);
            m_lutR[i] = outVal;
            m_lutG[i] = outVal;
            m_lutB[i] = outVal;
        }
    } else if (m_adjType == AdjustmentType::ToneCurve) {
        auto lutRGB = calculateCurveLUT(m_curvePoints[0], m_curveInterpMode);
        auto lutR = calculateCurveLUT(m_curvePoints[1], m_curveInterpMode);
        auto lutG = calculateCurveLUT(m_curvePoints[2], m_curveInterpMode);
        auto lutB = calculateCurveLUT(m_curvePoints[3], m_curveInterpMode);

        for (int i = 0; i < 256; ++i) {
            m_lutR[i] = lutR[lutRGB[i]];
            m_lutG[i] = lutG[lutRGB[i]];
            m_lutB[i] = lutB[lutRGB[i]];
        }
    } else if (m_adjType == AdjustmentType::Levels) {
        auto lutRGB = calculateLevelLUT(m_levelParams[0]);
        auto lutR = calculateLevelLUT(m_levelParams[1]);
        auto lutG = calculateLevelLUT(m_levelParams[2]);
        auto lutB = calculateLevelLUT(m_levelParams[3]);

        for (int i = 0; i < 256; ++i) {
            m_lutR[i] = lutR[lutRGB[i]];
            m_lutG[i] = lutG[lutRGB[i]];
            m_lutB[i] = lutB[lutRGB[i]];
        }
    } else {
        for (int i = 0; i < 256; ++i) {
            m_lutR[i] = static_cast<uint8_t>(i);
            m_lutG[i] = static_cast<uint8_t>(i);
            m_lutB[i] = static_cast<uint8_t>(i);
        }
    }
}

void AdjustmentLayer::renderToTarget(TileManager& target, const QRect& dirtyRect, const Layer* baseLayer) {
    if (!m_visible || m_opacity <= 0.0f) return;

    if (m_isClippingMask) {
        if (!baseLayer || baseLayer->effectiveOpacity() <= 0.0f) {
            return;
        }
    }

    int startTx = std::max(0, dirtyRect.left() / static_cast<int>(TileMemoryPool::TILE_WIDTH));
    int endTx = std::min(target.tilesX() - 1, dirtyRect.right() / static_cast<int>(TileMemoryPool::TILE_WIDTH));
    int startTy = std::max(0, dirtyRect.top() / static_cast<int>(TileMemoryPool::TILE_HEIGHT));
    int endTy = std::min(target.tilesY() - 1, dirtyRect.bottom() / static_cast<int>(TileMemoryPool::TILE_HEIGHT));

    constexpr size_t PIXELS_PER_TILE = TileMemoryPool::TILE_WIDTH * TileMemoryPool::TILE_HEIGHT;

    for (int ty = startTy; ty <= endTy; ++ty) {
        for (int tx = startTx; tx <= endTx; ++tx) {
            Tile* dstTile = target.getTileForWrite(tx, ty);
            if (!dstTile || !dstTile->readBuffer()) continue;

            const Tile* maskTile = nullptr;
            if (m_isClippingMask && baseLayer) {
                maskTile = baseLayer->tileManager().getTile(tx, ty);
                if (!maskTile || !maskTile->readBuffer()) {
                    continue;
                }
            }

            uint32_t* buf = reinterpret_cast<uint32_t*>(dstTile->writeBuffer());
            const uint32_t* maskBuf = maskTile ? reinterpret_cast<const uint32_t*>(maskTile->readBuffer()) : nullptr;

            for (size_t i = 0; i < PIXELS_PER_TILE; ++i) {
                uint32_t px = buf[i];
                uint32_t a = (px >> 24) & 0xFF;
                if (a == 0) continue;

                float effOpacity = m_opacity;
                if (m_isClippingMask) {
                    if (!maskBuf) continue;
                    uint32_t ma = (maskBuf[i] >> 24) & 0xFF;
                    if (ma == 0) continue;
                    effOpacity *= (ma / 255.0f) * baseLayer->effectiveOpacity();
                    if (effOpacity <= 0.0f) continue;
                }

                uint8_t r = (px >> 16) & 0xFF;
                uint8_t g = (px >> 8) & 0xFF;
                uint8_t b = px & 0xFF;

                uint8_t nr = r, ng = g, nb = b;

                if (m_adjType == AdjustmentType::HueSaturation) {
                    float h, s, v;
                    rgbToHsv(r, g, b, h, s, v);
                    h = std::fmod(h + m_hueShift + 360.0f, 360.0f);
                    s = std::clamp(s * m_saturation, 0.0f, 1.0f);
                    if (m_lightness != 0.0f) {
                        v = std::clamp(v + m_lightness, 0.0f, 1.0f);
                    }
                    hsvToRgb(h, s, v, nr, ng, nb);
                } else if (m_adjType == AdjustmentType::Grayscale) {
                    uint8_t gray = static_cast<uint8_t>(0.299f * r + 0.587f * g + 0.114f * b + 0.5f);
                    nr = ng = nb = gray;
                } else {
                    nr = m_lutR[r];
                    ng = m_lutG[g];
                    nb = m_lutB[b];
                }

                if (effOpacity < 1.0f) {
                    nr = static_cast<uint8_t>(r + (nr - r) * effOpacity + 0.5f);
                    ng = static_cast<uint8_t>(g + (ng - g) * effOpacity + 0.5f);
                    nb = static_cast<uint8_t>(b + (nb - b) * effOpacity + 0.5f);
                }

                buf[i] = (a << 24) | (nr << 16) | (ng << 8) | nb;
            }
        }
    }
}

void AdjustmentLayer::computeHistogram(const TileManager& source, const QRect& bounds,
                                       std::array<int, 256>& histR,
                                       std::array<int, 256>& histG,
                                       std::array<int, 256>& histB,
                                       std::array<int, 256>& histLum) {
    histR.fill(0);
    histG.fill(0);
    histB.fill(0);
    histLum.fill(0);

    int startTx = std::max(0, bounds.left() / static_cast<int>(TileMemoryPool::TILE_WIDTH));
    int endTx = std::min(source.tilesX() - 1, bounds.right() / static_cast<int>(TileMemoryPool::TILE_WIDTH));
    int startTy = std::max(0, bounds.top() / static_cast<int>(TileMemoryPool::TILE_HEIGHT));
    int endTy = std::min(source.tilesY() - 1, bounds.bottom() / static_cast<int>(TileMemoryPool::TILE_HEIGHT));

    constexpr size_t PIXELS_PER_TILE = TileMemoryPool::TILE_WIDTH * TileMemoryPool::TILE_HEIGHT;

    for (int ty = startTy; ty <= endTy; ++ty) {
        for (int tx = startTx; tx <= endTx; ++tx) {
            const Tile* tile = source.getTile(tx, ty);
            if (!tile || !tile->readBuffer()) continue;

            const uint32_t* buf = reinterpret_cast<const uint32_t*>(tile->readBuffer());
            for (size_t i = 0; i < PIXELS_PER_TILE; ++i) {
                uint32_t px = buf[i];
                uint32_t a = (px >> 24) & 0xFF;
                if (a == 0) continue;

                uint8_t r = (px >> 16) & 0xFF;
                uint8_t g = (px >> 8) & 0xFF;
                uint8_t b = px & 0xFF;
                uint8_t lum = static_cast<uint8_t>(0.299f * r + 0.587f * g + 0.114f * b + 0.5f);

                histR[r]++;
                histG[g]++;
                histB[b]++;
                histLum[lum]++;
            }
        }
    }
}

} // namespace Kaburagi
