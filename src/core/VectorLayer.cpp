#include "VectorLayer.hpp"
#include <cmath>
#include <algorithm>

namespace Kaburagi {

int VectorStroke::findNearestPoint(const QPointF& pt, float maxDist) const {
    int nearestIdx = -1;
    float minDistSq = maxDist * maxDist;

    for (size_t i = 0; i < points.size(); ++i) {
        float dx = points[i].x - pt.x();
        float dy = points[i].y - pt.y();
        float distSq = dx * dx + dy * dy;
        if (distSq < minDistSq) {
            minDistSq = distSq;
            nearestIdx = static_cast<int>(i);
        }
    }
    return nearestIdx;
}

bool VectorStroke::adjustPressureNear(const QPointF& pt, float deltaPressure, float radius) {
    bool changed = false;
    float radSq = radius * radius;

    for (auto& p : points) {
        float dx = p.x - pt.x();
        float dy = p.y - pt.y();
        float distSq = dx * dx + dy * dy;
        if (distSq < radSq) {
            float falloff = 1.0f - std::sqrt(distSq) / radius;
            p.pressure = std::max(0.05f, std::min(3.0f, p.pressure + deltaPressure * falloff));
            changed = true;
        }
    }
    return changed;
}

VectorStroke VectorStroke::createEllipse(const QPointF& center, float rx, float ry,
                                         float width, uint32_t color, float density,
                                         int hardness, int falloff, bool isFilled) {
    VectorStroke s;
    s.baseWidth = width;
    s.color = color;
    s.density = density;
    s.hardness = hardness;
    s.falloff = falloff;
    s.isClosed = true;
    s.isFilled = isFilled;

    constexpr int NUM_PTS = 16;
    constexpr double PI = 3.14159265358979323846;
    for (int i = 0; i < NUM_PTS; ++i) {
        double angle = i * 2.0 * PI / NUM_PTS;
        float x = static_cast<float>(center.x() + rx * std::cos(angle));
        float y = static_cast<float>(center.y() + ry * std::sin(angle));
        s.points.push_back({ x, y, 1.0f, 1.0f, false, false });
    }
    return s;
}

VectorStroke VectorStroke::createPolygon(const QPointF& center, float rx, float ry,
                                         int sides, bool isRegular, float rotAngle,
                                         float width, uint32_t color, float density,
                                         int hardness, int falloff, bool isFilled) {
    VectorStroke s;
    s.baseWidth = width;
    s.color = color;
    s.density = density;
    s.hardness = hardness;
    s.falloff = falloff;
    s.isClosed = true;
    s.isFilled = isFilled;

    int n = std::clamp(sides, 3, 24);
    constexpr double PI = 3.14159265358979323846;

    float useRx = rx;
    float useRy = isRegular ? rx : ry;

    for (int i = 0; i < n; ++i) {
        double angle = rotAngle + i * 2.0 * PI / n;
        float x = static_cast<float>(center.x() + useRx * std::cos(angle));
        float y = static_cast<float>(center.y() + useRy * std::sin(angle));
        s.points.push_back({ x, y, 1.0f, 1.0f, true, false });
    }
    return s;
}

VectorLayer::VectorLayer(const QString& name, int width, int height)
    : Layer(name, width, height, LayerType::Vector) {
}

void VectorLayer::addStroke(const VectorStroke& stroke) {
    m_strokes.push_back(stroke);
    rasterizeStroke(stroke);
}

void VectorLayer::rasterizeAll() {
    m_tileManager.clear(0x00000000);
    for (const auto& stroke : m_strokes) {
        rasterizeStroke(stroke);
    }
    invalidateContentBounds();
}

// 2点間の円形ブラシスタンプ補間描画 (互換用)
void VectorLayer::drawSegment(const QPointF& p0, const QPointF& p1, float w0, float w1, uint32_t color) {
    float dx = p1.x() - p0.x();
    float dy = p1.y() - p0.y();
    float dist = std::hypot(dx, dy);
    int steps = std::max(1, static_cast<int>(dist * 2.0f));

    for (int i = 0; i <= steps; ++i) {
        float t = static_cast<float>(i) / steps;
        float x = p0.x() + dx * t;
        float y = p0.y() + dy * t;
        float r = (w0 + (w1 - w0) * t) * 0.5f;
        int ir = std::max(1, static_cast<int>(std::ceil(r)));

        int minX = std::max(0, static_cast<int>(std::floor(x - ir)));
        int maxX = std::min(m_tileManager.width() - 1, static_cast<int>(std::ceil(x + ir)));
        int minY = std::max(0, static_cast<int>(std::floor(y - ir)));
        int maxY = std::min(m_tileManager.height() - 1, static_cast<int>(std::ceil(y + ir)));

        for (int py = minY; py <= maxY; ++py) {
            for (int px = minX; px <= maxX; ++px) {
                float d = std::hypot(px - x, py - y);
                float rInner = std::max(0.0f, r - 0.5f);
                float rOuter = r + 0.5f;
                float stampAlpha = 0.0f;

                if (d <= rInner) {
                    stampAlpha = 1.0f;
                } else if (d < rOuter) {
                    float u = (rOuter - d) / (rOuter - rInner);
                    stampAlpha = u * u * (3.0f - 2.0f * u);
                }

                if (stampAlpha <= 0.0f) continue;

                uint32_t dst = m_tileManager.getPixel(px, py);
                uint32_t da = (dst >> 24) & 0xFF;

                float sa = stampAlpha * (((color >> 24) & 0xFF) / 255.0f);
                float daF = da / 255.0f;

                float outA = sa + daF * (1.0f - sa);
                if (outA <= 0.0f) continue;

                uint32_t sr = (color >> 16) & 0xFF;
                uint32_t sg = (color >> 8) & 0xFF;
                uint32_t sb = color & 0xFF;

                uint32_t dr = (dst >> 16) & 0xFF;
                uint32_t dg = (dst >> 8) & 0xFF;
                uint32_t db = dst & 0xFF;

                uint32_t outR = static_cast<uint32_t>((sr * sa + dr * daF * (1.0f - sa)) / outA + 0.5f);
                uint32_t outG = static_cast<uint32_t>((sg * sa + dg * daF * (1.0f - sa)) / outA + 0.5f);
                uint32_t outB = static_cast<uint32_t>((sb * sa + db * daF * (1.0f - sa)) / outA + 0.5f);
                uint32_t outA8 = static_cast<uint32_t>(outA * 255.0f + 0.5f);

                uint32_t outPixel = (std::min(255u, outA8) << 24) |
                                    (std::min(255u, outR) << 16) |
                                    (std::min(255u, outG) << 8) |
                                    std::min(255u, outB);

                m_tileManager.setPixel(px, py, outPixel);
            }
        }
    }
}

void VectorLayer::rasterizeStroke(const VectorStroke& stroke) {
    if (stroke.points.empty()) return;

    const auto& pts = stroke.points;
    size_t count = pts.size();

    // サンプル点列（中心座標、半径、濃度）を収集
    struct StampSample {
        QPointF pt;
        float r;
        float density;
    };
    std::vector<StampSample> samples;

    float baseDens = stroke.density;
    int hardness = stroke.hardness;
    int falloff = stroke.falloff;

    if (count == 1) {
        // 単一タップ点描画
        float w = stroke.baseWidth * pts[0].pressure;
        float dens = baseDens * pts[0].opacity;
        samples.push_back({ QPointF(pts[0].x, pts[0].y), w * 0.5f, dens });
    } else {
        auto catmullRom = [](const QPointF& p0, const QPointF& p1, const QPointF& p2, const QPointF& p3, float t) {
            float t2 = t * t;
            float t3 = t2 * t;
            return 0.5f * ((2.0f * p1) +
                          (-p0 + p2) * t +
                          (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                          (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
        };

        bool isClosed = stroke.isClosed && count >= 3;
        size_t numSegs = isClosed ? count : (count - 1);

        for (size_t i = 0; i < numSegs; ++i) {
            size_t idx0 = isClosed ? (i + count - 1) % count : (i > 0 ? i - 1 : i);
            size_t idx1 = i;
            size_t idx2 = (i + 1) % count;
            size_t idx3 = isClosed ? (i + 2) % count : (i + 2 < count ? i + 2 : idx2);

            QPointF p0(pts[idx0].x, pts[idx0].y);
            QPointF p1(pts[idx1].x, pts[idx1].y);
            QPointF p2(pts[idx2].x, pts[idx2].y);
            QPointF p3(pts[idx3].x, pts[idx3].y);

            float w1 = stroke.baseWidth * pts[idx1].pressure;
            float w2 = stroke.baseWidth * pts[idx2].pressure;
            float dens1 = baseDens * pts[idx1].opacity;
            float dens2 = baseDens * pts[idx2].opacity;

            bool isLinear = pts[idx1].isCorner || pts[idx2].isCorner;

            constexpr int SUBDIV = 10;
            QPointF prevPt = p1;
            float prevW = w1;
            float prevDens = dens1;

            if (i == 0) {
                samples.push_back({ prevPt, prevW * 0.5f, prevDens });
            }

            for (int s = 1; s <= SUBDIV; ++s) {
                float t = static_cast<float>(s) / SUBDIV;
                QPointF curPt = isLinear ? (p1 + (p2 - p1) * t) : catmullRom(p0, p1, p2, p3, t);
                float curW = w1 + (w2 - w1) * t;
                float curDens = dens1 + (dens2 - dens1) * t;

                float dx = curPt.x() - prevPt.x();
                float dy = curPt.y() - prevPt.y();
                float dist = std::hypot(dx, dy);
                float minR = std::min(prevW, curW) * 0.5f;
                float stepDist = std::max(1.0f, minR * 0.35f);
                int steps = std::max(1, static_cast<int>(std::ceil(dist / stepDist)));

                for (int st = 1; st <= steps; ++st) {
                    float subT = static_cast<float>(st) / steps;
                    QPointF pt = prevPt + (curPt - prevPt) * subT;
                    float r = (prevW + (curW - prevW) * subT) * 0.5f;
                    float dens = prevDens + (curDens - prevDens) * subT;
                    samples.push_back({ pt, r, dens });
                }

                prevPt = curPt;
                prevW = curW;
                prevDens = curDens;
            }
        }
    }

    if (samples.empty()) return;

    // ストローク全体のバウンディングボックスを計算
    float minX = samples[0].pt.x() - samples[0].r;
    float maxX = samples[0].pt.x() + samples[0].r;
    float minY = samples[0].pt.y() - samples[0].r;
    float maxY = samples[0].pt.y() + samples[0].r;

    for (const auto& s : samples) {
        minX = std::min(minX, static_cast<float>(s.pt.x() - s.r));
        maxX = std::max(maxX, static_cast<float>(s.pt.x() + s.r));
        minY = std::min(minY, static_cast<float>(s.pt.y() - s.r));
        maxY = std::max(maxY, static_cast<float>(s.pt.y() + s.r));
    }

    int bMinX = std::max(0, static_cast<int>(std::floor(minX - 1.5f)));
    int bMaxX = std::min(m_tileManager.width() - 1, static_cast<int>(std::ceil(maxX + 1.5f)));
    int bMinY = std::max(0, static_cast<int>(std::floor(minY - 1.5f)));
    int bMaxY = std::min(m_tileManager.height() - 1, static_cast<int>(std::ceil(maxY + 1.5f)));

    if (bMinX > bMaxX || bMinY > bMaxY) return;

    int bw = bMaxX - bMinX + 1;
    int bh = bMaxY - bMinY + 1;

    std::vector<float> strokeAlphaMap(bw * bh, 0.0f);

    // 各スタンプを strokeAlphaMap に蓄積（単一ストローク内での最大濃度法: 濃度やボケ足の飽和を防止）
    for (const auto& smp : samples) {
        float cx = smp.pt.x();
        float cy = smp.pt.y();
        float r = smp.r;
        if (r <= 0.001f) continue;
        float curDensity = smp.density;
        int ir = std::max(1, static_cast<int>(std::ceil(r + 1.0f)));

        int sMinX = std::max(bMinX, static_cast<int>(std::floor(cx - ir)));
        int sMaxX = std::min(bMaxX, static_cast<int>(std::ceil(cx + ir)));
        int sMinY = std::max(bMinY, static_cast<int>(std::floor(cy - ir)));
        int sMaxY = std::min(bMaxY, static_cast<int>(std::ceil(cy + ir)));

        for (int py = sMinY; py <= sMaxY; ++py) {
            int rowOffset = (py - bMinY) * bw;
            for (int px = sMinX; px <= sMaxX; ++px) {
                float d = std::hypot(px - cx, py - cy);

                // 高品質アンチエイリアス (Hermite Smoothstep)
                float rInner = std::max(0.0f, r - 0.5f);
                float rOuter = r + 0.5f;
                float stampAlpha = 0.0f;

                if (d <= rInner) {
                    stampAlpha = curDensity;
                } else if (d < rOuter) {
                    float u = (rOuter - d) / (rOuter - rInner);
                    stampAlpha = u * u * (3.0f - 2.0f * u) * curDensity;
                }

                if (stampAlpha <= 0.0f) continue;

                // 輪郭の硬さ (0〜100) と ボケ足 (0〜100) によるグラデーション
                if (hardness < 100 && falloff > 0 && r > 0.0f) {
                    float rStart = r * (hardness / 100.0f);
                    if (d > rStart) {
                        float uGrad = (r > rStart) ? std::min(1.0f, (d - rStart) / (r - rStart)) : 1.0f;
                        float minFade = 1.0f - (falloff / 100.0f);
                        float fade = 1.0f - (1.0f - minFade) * uGrad;
                        stampAlpha *= fade;
                    }
                }

                int idx = rowOffset + (px - bMinX);
                if (stampAlpha > strokeAlphaMap[idx]) {
                    strokeAlphaMap[idx] = stampAlpha;
                }
            }
        }
    }

    // 閉じたストロークで内部塗りつぶしが有効な場合、ポリゴン内部を strokeAlphaMap に蓄積
    if (stroke.isClosed && stroke.isFilled && samples.size() >= 3) {
        QPolygonF poly;
        poly.reserve(static_cast<int>(samples.size()));
        for (const auto& smp : samples) {
            poly << smp.pt;
        }

        QRectF polyRect = poly.boundingRect();
        int pMinX = std::max(bMinX, static_cast<int>(std::floor(polyRect.left())));
        int pMaxX = std::min(bMaxX, static_cast<int>(std::ceil(polyRect.right())));
        int pMinY = std::max(bMinY, static_cast<int>(std::floor(polyRect.top())));
        int pMaxY = std::min(bMaxY, static_cast<int>(std::ceil(polyRect.bottom())));

        for (int py = pMinY; py <= pMaxY; ++py) {
            int rowOffset = (py - bMinY) * bw;
            for (int px = pMinX; px <= pMaxX; ++px) {
                if (poly.containsPoint(QPointF(px + 0.5f, py + 0.5f), Qt::OddEvenFill)) {
                    int idx = rowOffset + (px - bMinX);
                    if (baseDens > strokeAlphaMap[idx]) {
                        strokeAlphaMap[idx] = baseDens;
                    }
                }
            }
        }
    }

    // レイヤーのタイルマネージャーに合成 (Porter-Duff Over)
    uint32_t color = stroke.color;
    uint32_t sr = (color >> 16) & 0xFF;
    uint32_t sg = (color >> 8) & 0xFF;
    uint32_t sb = color & 0xFF;
    float brushColorA = ((color >> 24) & 0xFF) / 255.0f;

    for (int py = bMinY; py <= bMaxY; ++py) {
        int rowOffset = (py - bMinY) * bw;
        for (int px = bMinX; px <= bMaxX; ++px) {
            float sAlpha = strokeAlphaMap[rowOffset + (px - bMinX)];
            if (sAlpha <= 0.0f) continue;

            float sa = sAlpha * brushColorA;
            if (sa <= 0.0f) continue;

            uint32_t dst = m_tileManager.getPixel(px, py);
            uint32_t da = (dst >> 24) & 0xFF;
            float daF = da / 255.0f;

            float outA = sa + daF * (1.0f - sa);
            if (outA <= 0.0f) continue;

            uint32_t dr = (dst >> 16) & 0xFF;
            uint32_t dg = (dst >> 8) & 0xFF;
            uint32_t db = dst & 0xFF;

            uint32_t outR = static_cast<uint32_t>((sr * sa + dr * daF * (1.0f - sa)) / outA + 0.5f);
            uint32_t outG = static_cast<uint32_t>((sg * sa + dg * daF * (1.0f - sa)) / outA + 0.5f);
            uint32_t outB = static_cast<uint32_t>((sb * sa + db * daF * (1.0f - sa)) / outA + 0.5f);
            uint32_t outA8 = static_cast<uint32_t>(outA * 255.0f + 0.5f);

            uint32_t outPixel = (std::min(255u, outA8) << 24) |
                                (std::min(255u, outR) << 16) |
                                (std::min(255u, outG) << 8) |
                                std::min(255u, outB);

            m_tileManager.setPixel(px, py, outPixel);
        }
    }

    invalidateContentBounds();
}

void VectorLayer::setStrokes(const std::vector<VectorStroke>& strokes) {
    m_strokes = strokes;
    rasterizeAll();
}

bool VectorLayer::removeStroke(size_t index) {
    if (index >= m_strokes.size()) return false;
    m_strokes.erase(m_strokes.begin() + index);
    rasterizeAll();
    return true;
}

static bool intersectSegments(const QPointF& a, const QPointF& b, const QPointF& c, const QPointF& d, float* outT = nullptr, float* outU = nullptr, QPointF* outPt = nullptr) {
    QPointF r = b - a;
    QPointF s = d - c;
    float cross = r.x() * s.y() - r.y() * s.x();
    if (std::abs(cross) < 1e-7f) {
        return false; // 平行または同一直線
    }
    QPointF qp = c - a;
    float t = (qp.x() * s.y() - qp.y() * s.x()) / cross;
    float u = (qp.x() * r.y() - qp.y() * r.x()) / cross;
    if (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f) {
        if (outT) *outT = t;
        if (outU) *outU = u;
        if (outPt) *outPt = a + r * t;
        return true;
    }
    return false;
}

static float pointToSegmentDistance(const QPointF& p, const QPointF& a, const QPointF& b, float* outT = nullptr) {
    QPointF ab = b - a;
    float lenSq = ab.x() * ab.x() + ab.y() * ab.y();
    if (lenSq < 1e-6f) {
        if (outT) *outT = 0.0f;
        return std::hypot(p.x() - a.x(), p.y() - a.y());
    }
    float t = ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / lenSq;
    t = std::clamp(t, 0.0f, 1.0f);
    if (outT) *outT = t;
    QPointF proj = a + ab * t;
    return std::hypot(p.x() - proj.x(), p.y() - proj.y());
}

std::vector<QPointF> VectorLayer::sampleStrokePoints(const VectorStroke& stroke, std::vector<float>* outParams, int subdiv) {
    std::vector<QPointF> sampled;
    if (stroke.points.empty()) return sampled;
    if (stroke.points.size() == 1) {
        sampled.push_back(QPointF(stroke.points[0].x, stroke.points[0].y));
        if (outParams) outParams->push_back(0.0f);
        return sampled;
    }

    const auto& pts = stroke.points;
    size_t count = pts.size();

    auto catmullRom = [](const QPointF& p0, const QPointF& p1, const QPointF& p2, const QPointF& p3, float t) {
        float t2 = t * t;
        float t3 = t2 * t;
        return 0.5f * ((2.0f * p1) +
                      (-p0 + p2) * t +
                      (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                      (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
    };

    sampled.push_back(QPointF(pts[0].x, pts[0].y));
    if (outParams) outParams->push_back(0.0f);

    bool isClosed = stroke.isClosed && count >= 3;
    size_t numSegs = isClosed ? count : (count - 1);

    for (size_t i = 0; i < numSegs; ++i) {
        size_t idx0 = isClosed ? (i + count - 1) % count : (i > 0 ? i - 1 : i);
        size_t idx1 = i;
        size_t idx2 = (i + 1) % count;
        size_t idx3 = isClosed ? (i + 2) % count : (i + 2 < count ? i + 2 : idx2);

        QPointF p0(pts[idx0].x, pts[idx0].y);
        QPointF p1(pts[idx1].x, pts[idx1].y);
        QPointF p2(pts[idx2].x, pts[idx2].y);
        QPointF p3(pts[idx3].x, pts[idx3].y);

        bool isLinear = pts[idx1].isCorner || pts[idx2].isCorner;

        for (int s = 1; s <= subdiv; ++s) {
            float t = static_cast<float>(s) / subdiv;
            QPointF curPt = isLinear ? (p1 + (p2 - p1) * t) : catmullRom(p0, p1, p2, p3, t);
            sampled.push_back(curPt);
            if (outParams) outParams->push_back(static_cast<float>(i) + t);
        }
    }
    return sampled;
}

std::vector<VectorPoint> VectorLayer::simplifyPoints(const std::vector<VectorPoint>& points, float tolerance, float pressureTol) {
    if (points.size() <= 2) {
        return points;
    }

    std::vector<bool> keep(points.size(), false);
    keep.front() = true;
    keep.back() = true;

    // ヘルパー: 再帰的 RDP (幾何偏差・筆圧偏差・不透明度偏差の統合評価)
    auto rdpRecursive = [&](auto& self, size_t start, size_t end) -> void {
        if (end <= start + 1) return;

        const auto& pA = points[start];
        const auto& pB = points[end];

        float dx = pB.x - pA.x;
        float dy = pB.y - pA.y;
        float lenSq = dx * dx + dy * dy;

        float maxScore = -1.0f;
        size_t maxIdx = start;

        for (size_t i = start + 1; i < end; ++i) {
            const auto& p = points[i];

            // 角（コーナー）フラグが付いている制御点は無条件で保持対象とする
            if (p.isCorner) {
                maxScore = 1e9f;
                maxIdx = i;
                break;
            }

            float t = 0.0f;
            if (lenSq > 1e-6f) {
                t = std::clamp(((p.x - pA.x) * dx + (p.y - pA.y) * dy) / lenSq, 0.0f, 1.0f);
            }

            float projX = pA.x + t * dx;
            float projY = pA.y + t * dy;
            float dist = std::hypot(p.x - projX, p.y - projY);

            float pInterp = pA.pressure + t * (pB.pressure - pA.pressure);
            float pDiff = std::abs(p.pressure - pInterp);

            float oInterp = pA.opacity + t * (pB.opacity - pA.opacity);
            float oDiff = std::abs(p.opacity - oInterp);

            float scoreGeom = dist / std::max(0.001f, tolerance);
            float scorePress = pDiff / std::max(0.001f, pressureTol);
            float scoreOpac = oDiff / 0.2f;

            float score = std::max({scoreGeom, scorePress, scoreOpac});
            if (score > maxScore) {
                maxScore = score;
                maxIdx = i;
            }
        }

        if (maxScore > 1.0f) {
            keep[maxIdx] = true;
            self(self, start, maxIdx);
            self(self, maxIdx, end);
        }
    };

    rdpRecursive(rdpRecursive, 0, points.size() - 1);

    std::vector<VectorPoint> result;
    result.reserve(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        if (keep[i]) {
            result.push_back(points[i]);
        }
    }
    return result;
}

void VectorLayer::clearSelection() {
    for (auto& s : m_strokes) {
        s.isSelected = false;
        for (auto& p : s.points) {
            p.isSelected = false;
        }
    }
}

void VectorLayer::selectStroke(int strokeIndex, bool select) {
    if (strokeIndex >= 0 && strokeIndex < static_cast<int>(m_strokes.size())) {
        m_strokes[strokeIndex].isSelected = select;
    }
}

void VectorLayer::selectPoint(int strokeIndex, int pointIndex, bool select) {
    if (strokeIndex >= 0 && strokeIndex < static_cast<int>(m_strokes.size())) {
        auto& s = m_strokes[strokeIndex];
        if (pointIndex >= 0 && pointIndex < static_cast<int>(s.points.size())) {
            s.points[pointIndex].isSelected = select;
        }
    }
}

int VectorLayer::selectedStrokeIndex() const {
    for (size_t i = 0; i < m_strokes.size(); ++i) {
        if (m_strokes[i].isSelected) return static_cast<int>(i);
    }
    return -1;
}

std::pair<int, int> VectorLayer::selectedPointIndex() const {
    for (size_t i = 0; i < m_strokes.size(); ++i) {
        for (size_t j = 0; j < m_strokes[i].points.size(); ++j) {
            if (m_strokes[i].points[j].isSelected) {
                return { static_cast<int>(i), static_cast<int>(j) };
            }
        }
    }
    return { -1, -1 };
}

bool VectorLayer::findNearestControlPoint(const QPointF& pt, float hitRadius, int& outStrokeIdx, int& outPtIdx, float* outDist) const {
    outStrokeIdx = -1;
    outPtIdx = -1;
    float minD = hitRadius;

    for (size_t i = 0; i < m_strokes.size(); ++i) {
        const auto& s = m_strokes[i];
        for (size_t j = 0; j < s.points.size(); ++j) {
            float dx = s.points[j].x - pt.x();
            float dy = s.points[j].y - pt.y();
            float dist = std::hypot(dx, dy);
            if (dist < minD) {
                minD = dist;
                outStrokeIdx = static_cast<int>(i);
                outPtIdx = static_cast<int>(j);
            }
        }
    }

    if (outDist) *outDist = minD;
    return (outStrokeIdx >= 0 && outPtIdx >= 0);
}

int VectorLayer::findNearestStroke(const QPointF& pt, float hitRadius, float* outDist, int* outSegIndex, float* outSegT) const {
    int bestStroke = -1;
    float minD = 1e9f;
    int bestSegIdx = -1;
    float bestSegT = 0.0f;

    for (size_t i = 0; i < m_strokes.size(); ++i) {
        const auto& s = m_strokes[i];
        if (s.points.empty()) continue;

        float effectiveHitThreshold = std::max(hitRadius, s.baseWidth * 0.5f + 4.0f);

        std::vector<float> params;
        std::vector<QPointF> sampled = sampleStrokePoints(s, &params, 8);
        if (sampled.size() < 2) {
            if (sampled.size() == 1) {
                float dist = std::hypot(sampled[0].x() - pt.x(), sampled[0].y() - pt.y());
                if (dist <= effectiveHitThreshold && dist < minD) {
                    minD = dist;
                    bestStroke = static_cast<int>(i);
                    bestSegIdx = 0;
                    bestSegT = 0.0f;
                }
            }
            continue;
        }

        for (size_t k = 0; k < sampled.size() - 1; ++k) {
            float segT = 0.0f;
            float dist = pointToSegmentDistance(pt, sampled[k], sampled[k + 1], &segT);
            if (dist <= effectiveHitThreshold && dist < minD) {
                minD = dist;
                bestStroke = static_cast<int>(i);
                float param = params[k] + (params[k + 1] - params[k]) * segT;
                bestSegIdx = static_cast<int>(std::floor(param));
                bestSegT = param - bestSegIdx;
            }
        }
    }

    if (outDist) *outDist = minD;
    if (outSegIndex) *outSegIndex = bestSegIdx;
    if (outSegT) *outSegT = bestSegT;
    return bestStroke;
}

bool VectorLayer::movePoint(int strokeIdx, int ptIdx, const QPointF& newPos) {
    if (strokeIdx < 0 || strokeIdx >= static_cast<int>(m_strokes.size())) return false;
    auto& s = m_strokes[strokeIdx];
    if (ptIdx < 0 || ptIdx >= static_cast<int>(s.points.size())) return false;
    s.points[ptIdx].x = static_cast<float>(newPos.x());
    s.points[ptIdx].y = static_cast<float>(newPos.y());
    rasterizeAll();
    return true;
}

bool VectorLayer::addPointToStroke(int strokeIdx, const QPointF& pt, int* outAddedPtIdx) {
    if (strokeIdx < 0 || strokeIdx >= static_cast<int>(m_strokes.size())) return false;
    auto& s = m_strokes[strokeIdx];
    if (s.points.empty()) {
        s.points.push_back({ static_cast<float>(pt.x()), static_cast<float>(pt.y()), 1.0f, 1.0f, false, true });
        if (outAddedPtIdx) *outAddedPtIdx = 0;
        rasterizeAll();
        return true;
    }
    if (s.points.size() == 1) {
        s.points.push_back({ static_cast<float>(pt.x()), static_cast<float>(pt.y()), 1.0f, 1.0f, false, true });
        if (outAddedPtIdx) *outAddedPtIdx = 1;
        rasterizeAll();
        return true;
    }

    std::vector<float> params;
    std::vector<QPointF> sampled = sampleStrokePoints(s, &params, 8);
    float minDist = 1e9f;
    float bestParam = 0.0f;

    for (size_t k = 0; k < sampled.size() - 1; ++k) {
        float segT = 0.0f;
        float d = pointToSegmentDistance(pt, sampled[k], sampled[k + 1], &segT);
        if (d < minDist) {
            minDist = d;
            bestParam = params[k] + (params[k + 1] - params[k]) * segT;
        }
    }

    int idx = static_cast<int>(std::floor(bestParam));
    float t = bestParam - idx;
    idx = std::clamp(idx, 0, static_cast<int>(s.points.size()) - 2);

    float p0 = s.points[idx].pressure;
    float p1 = s.points[idx + 1].pressure;
    float interpP = p0 + (p1 - p0) * t;

    float o0 = s.points[idx].opacity;
    float o1 = s.points[idx + 1].opacity;
    float interpO = o0 + (o1 - o0) * t;

    VectorPoint newPt{
        static_cast<float>(pt.x()),
        static_cast<float>(pt.y()),
        interpP,
        interpO,
        false,
        true
    };

    int insertIdx = idx + 1;
    s.points.insert(s.points.begin() + insertIdx, newPt);
    if (outAddedPtIdx) *outAddedPtIdx = insertIdx;
    rasterizeAll();
    return true;
}

bool VectorLayer::removePointFromStroke(int strokeIdx, int ptIdx) {
    if (strokeIdx < 0 || strokeIdx >= static_cast<int>(m_strokes.size())) return false;
    auto& s = m_strokes[strokeIdx];
    if (ptIdx < 0 || ptIdx >= static_cast<int>(s.points.size())) return false;

    s.points.erase(s.points.begin() + ptIdx);
    if (s.points.size() < 2) {
        m_strokes.erase(m_strokes.begin() + strokeIdx);
    }
    rasterizeAll();
    return true;
}

bool VectorLayer::moveStroke(int strokeIdx, float dx, float dy) {
    if (strokeIdx < 0 || strokeIdx >= static_cast<int>(m_strokes.size())) return false;
    auto& s = m_strokes[strokeIdx];
    for (auto& p : s.points) {
        p.x += dx;
        p.y += dy;
    }
    rasterizeAll();
    return true;
}

int VectorLayer::duplicateStroke(int strokeIdx) {
    if (strokeIdx < 0 || strokeIdx >= static_cast<int>(m_strokes.size())) return -1;
    clearSelection();
    VectorStroke copy = m_strokes[strokeIdx];
    copy.isSelected = true;
    m_strokes.push_back(copy);
    rasterizeAll();
    return static_cast<int>(m_strokes.size() - 1);
}

QPointF VectorLayer::strokeCenter(int strokeIdx) const {
    if (strokeIdx < 0 || strokeIdx >= static_cast<int>(m_strokes.size())) return QPointF(0, 0);
    const auto& s = m_strokes[strokeIdx];
    if (s.points.empty()) return QPointF(0, 0);

    float minX = s.points[0].x, maxX = s.points[0].x;
    float minY = s.points[0].y, maxY = s.points[0].y;
    for (const auto& p : s.points) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }
    return QPointF((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);
}

bool VectorLayer::rotateStroke(int strokeIdx, float angleDegrees, const QPointF& center) {
    if (strokeIdx < 0 || strokeIdx >= static_cast<int>(m_strokes.size())) return false;
    auto& s = m_strokes[strokeIdx];
    if (s.points.empty()) return false;

    float rad = angleDegrees * static_cast<float>(M_PI) / 180.0f;
    float cosA = std::cos(rad);
    float sinA = std::sin(rad);
    float cx = static_cast<float>(center.x());
    float cy = static_cast<float>(center.y());

    for (auto& p : s.points) {
        float dx = p.x - cx;
        float dy = p.y - cy;
        p.x = cx + dx * cosA - dy * sinA;
        p.y = cy + dx * sinA + dy * cosA;
    }
    rasterizeAll();
    return true;
}

bool VectorLayer::connectStrokes(int strokeIdx1, int ptIdx1, int strokeIdx2, int ptIdx2) {
    if (strokeIdx1 < 0 || strokeIdx1 >= static_cast<int>(m_strokes.size())) return false;
    if (strokeIdx2 < 0 || strokeIdx2 >= static_cast<int>(m_strokes.size())) return false;
    if (strokeIdx1 == strokeIdx2) return false;

    auto& s1 = m_strokes[strokeIdx1];
    auto s2 = m_strokes[strokeIdx2]; // コピー

    if (s1.points.empty() || s2.points.empty()) return false;
    if (s1.isClosed || s2.isClosed) return false;

    // ptIdx1 が s1 の端点かチェック (端点以外は接続不可)
    bool s1IsStart = (ptIdx1 == 0);
    bool s1IsEnd = (ptIdx1 == static_cast<int>(s1.points.size()) - 1);
    if (!s1IsStart && !s1IsEnd) return false;

    // ptIdx2 が s2 の端点かチェック (端点以外は接続不可)
    bool s2IsStart = (ptIdx2 == 0);
    bool s2IsEnd = (ptIdx2 == static_cast<int>(s2.points.size()) - 1);
    if (!s2IsStart && !s2IsEnd) return false;

    // 対象の制御点2点を接続し、1つのストロークとする
    if (s1IsEnd && s2IsStart) {
        s1.points.insert(s1.points.end(), s2.points.begin(), s2.points.end());
    } else if (s1IsEnd && s2IsEnd) {
        std::reverse(s2.points.begin(), s2.points.end());
        s1.points.insert(s1.points.end(), s2.points.begin(), s2.points.end());
    } else if (s1IsStart && s2IsEnd) {
        s1.points.insert(s1.points.begin(), s2.points.begin(), s2.points.end());
    } else { // s1IsStart && s2IsStart
        std::reverse(s2.points.begin(), s2.points.end());
        s1.points.insert(s1.points.begin(), s2.points.begin(), s2.points.end());
    }

    m_strokes.erase(m_strokes.begin() + strokeIdx2);

    clearSelection();
    int mergedIdx = (strokeIdx1 < strokeIdx2) ? strokeIdx1 : (strokeIdx1 - 1);
    if (mergedIdx >= 0 && mergedIdx < static_cast<int>(m_strokes.size())) {
        m_strokes[mergedIdx].isSelected = true;
    }

    rasterizeAll();
    return true;
}

bool VectorLayer::eraseAt(const QPointF& pt, float radius, VectorEraserMode mode) {
    return eraseByTrajectory({ pt }, radius, mode);
}

bool VectorLayer::eraseByTrajectory(const std::vector<QPointF>& trajectory, float radius, VectorEraserMode mode) {
    if (trajectory.empty() || m_strokes.empty()) return false;

    switch (mode) {
    case VectorEraserMode::WholeLine:
        return eraseWholeLineTrajectory(trajectory, radius);
    case VectorEraserMode::Intersection: {
        bool anyModified = false;
        float step = std::max(4.0f, radius * 0.5f);
        if (trajectory.size() == 1) {
            anyModified = eraseIntersection(trajectory[0], radius, false);
        } else {
            for (size_t i = 0; i < trajectory.size() - 1; ++i) {
                float dist = std::hypot(trajectory[i + 1].x() - trajectory[i].x(), trajectory[i + 1].y() - trajectory[i].y());
                int steps = std::max(1, static_cast<int>(std::ceil(dist / step)));
                for (int s = 0; s <= steps; ++s) {
                    float t = static_cast<float>(s) / steps;
                    QPointF p = trajectory[i] + (trajectory[i + 1] - trajectory[i]) * t;
                    if (eraseIntersection(p, radius, false)) {
                        anyModified = true;
                    }
                }
            }
        }
        if (anyModified) {
            rasterizeAll();
        }
        return anyModified;
    }
    case VectorEraserMode::TouchedArea:
        return eraseTouchedAreaTrajectory(trajectory, radius);
    }
    return false;
}

bool VectorLayer::eraseWholeLine(const QPointF& pt, float radius) {
    return eraseWholeLineTrajectory({ pt }, radius);
}

bool VectorLayer::eraseWholeLineTrajectory(const std::vector<QPointF>& trajectory, float radius) {
    if (m_strokes.empty() || trajectory.empty()) return false;

    float tMinX = trajectory[0].x(), tMaxX = trajectory[0].x();
    float tMinY = trajectory[0].y(), tMaxY = trajectory[0].y();
    for (const auto& p : trajectory) {
        tMinX = std::min(tMinX, static_cast<float>(p.x()));
        tMaxX = std::max(tMaxX, static_cast<float>(p.x()));
        tMinY = std::min(tMinY, static_cast<float>(p.y()));
        tMaxY = std::max(tMaxY, static_cast<float>(p.y()));
    }

    bool removed = false;
    for (int i = static_cast<int>(m_strokes.size()) - 1; i >= 0; --i) {
        const auto& stroke = m_strokes[i];
        if (stroke.points.empty()) continue;

        float hitThreshold = radius + stroke.baseWidth * 0.5f;

        float sMinX = stroke.points[0].x, sMaxX = stroke.points[0].x;
        float sMinY = stroke.points[0].y, sMaxY = stroke.points[0].y;
        for (const auto& p : stroke.points) {
            sMinX = std::min(sMinX, p.x); sMaxX = std::max(sMaxX, p.x);
            sMinY = std::min(sMinY, p.y); sMaxY = std::max(sMaxY, p.y);
        }

        if (sMaxX < tMinX - hitThreshold || sMinX > tMaxX + hitThreshold ||
            sMaxY < tMinY - hitThreshold || sMinY > tMaxY + hitThreshold) {
            continue;
        }

        auto samples = sampleStrokePoints(stroke);
        if (samples.size() < 2) continue;

        bool hit = false;
        for (size_t k = 0; k < samples.size() - 1; ++k) {
            if (trajectory.size() == 1) {
                if (pointToSegmentDistance(trajectory[0], samples[k], samples[k + 1]) <= hitThreshold) {
                    hit = true;
                    break;
                }
            } else {
                for (size_t t = 0; t < trajectory.size() - 1; ++t) {
                    float tVal = 0.0f, uVal = 0.0f;
                    if (intersectSegments(samples[k], samples[k + 1], trajectory[t], trajectory[t + 1], &tVal, &uVal)) {
                        hit = true;
                        break;
                    }
                    if (pointToSegmentDistance(trajectory[t], samples[k], samples[k + 1]) <= hitThreshold ||
                        pointToSegmentDistance(samples[k], trajectory[t], trajectory[t + 1]) <= hitThreshold) {
                        hit = true;
                        break;
                    }
                }
                if (hit) break;
            }
        }
        if (hit) {
            m_strokes.erase(m_strokes.begin() + i);
            removed = true;
        }
    }
    if (removed) {
        rasterizeAll();
    }
    return removed;
}

bool VectorLayer::eraseTouchedAreaTrajectory(const std::vector<QPointF>& trajectory, float radius) {
    if (m_strokes.empty() || trajectory.empty()) return false;

    float tMinX = trajectory[0].x(), tMaxX = trajectory[0].x();
    float tMinY = trajectory[0].y(), tMaxY = trajectory[0].y();
    for (const auto& p : trajectory) {
        tMinX = std::min(tMinX, static_cast<float>(p.x()));
        tMaxX = std::max(tMaxX, static_cast<float>(p.x()));
        tMinY = std::min(tMinY, static_cast<float>(p.y()));
        tMaxY = std::max(tMaxY, static_cast<float>(p.y()));
    }

    bool anyModified = false;
    std::vector<VectorStroke> updatedStrokes;

    for (size_t i = 0; i < m_strokes.size(); ++i) {
        const auto& stroke = m_strokes[i];
        if (stroke.points.size() < 2) {
            updatedStrokes.push_back(stroke);
            continue;
        }

        // 発生するストロークとストロークの間の隙間が現状の半分になるよう判定閾値を半減
        float hitThreshold = (radius + stroke.baseWidth * 0.5f) * 0.5f;

        float sMinX = stroke.points[0].x, sMaxX = stroke.points[0].x;
        float sMinY = stroke.points[0].y, sMaxY = stroke.points[0].y;
        for (const auto& p : stroke.points) {
            sMinX = std::min(sMinX, p.x); sMaxX = std::max(sMaxX, p.x);
            sMinY = std::min(sMinY, p.y); sMaxY = std::max(sMaxY, p.y);
        }

        if (sMaxX < tMinX - hitThreshold || sMinX > tMaxX + hitThreshold ||
            sMaxY < tMinY - hitThreshold || sMinY > tMaxY + hitThreshold) {
            updatedStrokes.push_back(stroke);
            continue;
        }

        std::vector<float> params;
        std::vector<QPointF> samples = sampleStrokePoints(stroke, &params, 8);
        if (samples.empty()) {
            updatedStrokes.push_back(stroke);
            continue;
        }

        std::vector<bool> isErased(samples.size(), false);
        bool hasErasedSample = false;

        for (size_t k = 0; k < samples.size(); ++k) {
            if (trajectory.size() == 1) {
                if (std::hypot(samples[k].x() - trajectory[0].x(), samples[k].y() - trajectory[0].y()) <= hitThreshold) {
                    isErased[k] = true;
                    hasErasedSample = true;
                }
            } else {
                for (size_t t = 0; t < trajectory.size() - 1; ++t) {
                    if (pointToSegmentDistance(samples[k], trajectory[t], trajectory[t + 1]) <= hitThreshold) {
                        isErased[k] = true;
                        hasErasedSample = true;
                        break;
                    }
                }
            }
        }

        if (!hasErasedSample) {
            updatedStrokes.push_back(stroke);
            continue;
        }

        anyModified = true;

        // 残す連続区間 [startK, endK] を抽出
        std::vector<std::pair<size_t, size_t>> keptIntervals;
        bool inInterval = false;
        size_t startK = 0;
        for (size_t k = 0; k < samples.size(); ++k) {
            if (!isErased[k]) {
                if (!inInterval) {
                    startK = k;
                    inInterval = true;
                }
            } else {
                if (inInterval) {
                    keptIntervals.push_back({ startK, k - 1 });
                    inInterval = false;
                }
            }
        }
        if (inInterval) {
            keptIntervals.push_back({ startK, samples.size() - 1 });
        }

        for (const auto& iv : keptIntervals) {
            float startParam = params[iv.first];
            float endParam = params[iv.second];

            if (endParam - startParam < 0.05f && iv.first == iv.second) {
                continue;
            }

            int i0 = std::clamp(static_cast<int>(std::floor(startParam)), 0, static_cast<int>(stroke.points.size()) - 1);
            int i1 = std::clamp(static_cast<int>(std::ceil(startParam)), 0, static_cast<int>(stroke.points.size()) - 1);
            float frac0 = (i0 == i1) ? 0.0f : (startParam - i0) / (i1 - i0);
            float pStart = stroke.points[i0].pressure * (1.0f - frac0) + stroke.points[i1].pressure * frac0;

            int j0 = std::clamp(static_cast<int>(std::floor(endParam)), 0, static_cast<int>(stroke.points.size()) - 1);
            int j1 = std::clamp(static_cast<int>(std::ceil(endParam)), 0, static_cast<int>(stroke.points.size()) - 1);
            float frac1 = (j0 == j1) ? 0.0f : (endParam - j0) / (j1 - j0);
            float pEnd = stroke.points[j0].pressure * (1.0f - frac1) + stroke.points[j1].pressure * frac1;

            VectorStroke newStroke;
            newStroke.baseWidth = stroke.baseWidth;
            newStroke.color = stroke.color;
            newStroke.density = stroke.density;
            newStroke.hardness = stroke.hardness;
            newStroke.falloff = stroke.falloff;

            newStroke.points.push_back({ static_cast<float>(samples[iv.first].x()), static_cast<float>(samples[iv.first].y()), pStart, 1.0f, false });

            for (size_t cp = 0; cp < stroke.points.size(); ++cp) {
                float cpParam = static_cast<float>(cp);
                if (cpParam > startParam + 0.05f && cpParam < endParam - 0.05f) {
                    newStroke.points.push_back(stroke.points[cp]);
                }
            }

            newStroke.points.push_back({ static_cast<float>(samples[iv.second].x()), static_cast<float>(samples[iv.second].y()), pEnd, 1.0f, false });

            std::vector<VectorPoint> cleanPoints;
            for (const auto& pt : newStroke.points) {
                if (cleanPoints.empty() || std::hypot(pt.x - cleanPoints.back().x, pt.y - cleanPoints.back().y) > 0.5f) {
                    cleanPoints.push_back(pt);
                }
            }
            newStroke.points = std::move(cleanPoints);

            if (newStroke.points.size() >= 2) {
                updatedStrokes.push_back(newStroke);
            }
        }
    }

    if (anyModified) {
        m_strokes = std::move(updatedStrokes);
        rasterizeAll();
        return true;
    }
    return false;
}

bool VectorLayer::eraseIntersection(const QPointF& pt, float radius, bool doRasterize) {
    if (m_strokes.empty()) return false;

    // 1. 各ストロークのサンプリング点列とパラメータを取得
    struct StrokeSample {
        std::vector<QPointF> points;
        std::vector<float> params;
    };
    std::vector<StrokeSample> allSamples(m_strokes.size());
    for (size_t i = 0; i < m_strokes.size(); ++i) {
        allSamples[i].points = sampleStrokePoints(m_strokes[i], &allSamples[i].params, 8);
    }

    // 2. 消しゴムの円と接触したストロークを探す
    int hitStrokeIdx = -1;
    float hitParam = 0.0f;
    float minHitDist = std::numeric_limits<float>::max();

    for (size_t i = 0; i < m_strokes.size(); ++i) {
        const auto& smp = allSamples[i];
        if (smp.points.size() < 2) continue;
        float hitThreshold = radius + m_strokes[i].baseWidth * 0.5f;

        for (size_t k = 0; k < smp.points.size() - 1; ++k) {
            float segT = 0.0f;
            float dist = pointToSegmentDistance(pt, smp.points[k], smp.points[k + 1], &segT);
            if (dist <= hitThreshold && dist < minHitDist) {
                minHitDist = dist;
                hitStrokeIdx = static_cast<int>(i);
                hitParam = smp.params[k] + (smp.params[k + 1] - smp.params[k]) * segT;
            }
        }
    }

    if (hitStrokeIdx < 0) {
        return false;
    }

    // 3. 接触したストローク (hitStrokeIdx) と他のすべてのストローク (および自己交差) との交点を計算
    const auto& hitStroke = m_strokes[hitStrokeIdx];
    const auto& hitSmp = allSamples[hitStrokeIdx];
    float maxParam = static_cast<float>(hitStroke.points.size() - 1);

    struct Crossing {
        float param; // hitStroke 上の連続パラメータ [0, maxParam]
        QPointF pt;  // 交点座標
    };
    std::vector<Crossing> crossings;

    for (size_t otherIdx = 0; otherIdx < m_strokes.size(); ++otherIdx) {
        const auto& otherSmp = allSamples[otherIdx];
        if (otherSmp.points.size() < 2) continue;

        for (size_t k = 0; k < hitSmp.points.size() - 1; ++k) {
            QPointF pA = hitSmp.points[k];
            QPointF pB = hitSmp.points[k + 1];

            size_t startJ = 0;
            if (otherIdx == static_cast<size_t>(hitStrokeIdx)) {
                // 自己交差判定
                if (k >= 2) {
                    startJ = 0;
                } else {
                    continue;
                }
            }

            for (size_t j = startJ; j < otherSmp.points.size() - 1; ++j) {
                if (otherIdx == static_cast<size_t>(hitStrokeIdx)) {
                    if (std::abs(static_cast<int>(k) - static_cast<int>(j)) <= 1) {
                        continue;
                    }
                }

                QPointF pC = otherSmp.points[j];
                QPointF pD = otherSmp.points[j + 1];

                float t = 0.0f, u = 0.0f;
                QPointF interPt;
                if (intersectSegments(pA, pB, pC, pD, &t, &u, &interPt)) {
                    float sParam = hitSmp.params[k] + (hitSmp.params[k + 1] - hitSmp.params[k]) * t;
                    crossings.push_back({ sParam, interPt });
                }
            }
        }
    }

    // 交点がない場合 -> 「交点まで消去」モードでは交点が存在しないため消去しない
    if (crossings.empty()) {
        return false;
    }

    // 交点をパラメータ順にソートし、近接する交点をマージ
    std::sort(crossings.begin(), crossings.end(), [](const Crossing& a, const Crossing& b) {
        return a.param < b.param;
    });

    std::vector<Crossing> uniqueCrossings;
    for (const auto& c : crossings) {
        if (uniqueCrossings.empty() || std::abs(c.param - uniqueCrossings.back().param) > 0.05f) {
            uniqueCrossings.push_back(c);
        }
    }

    auto getPointAndPressure = [&](float param, const QPointF* crossingPt) -> std::pair<QPointF, float> {
        if (param <= 0.001f) {
            return { QPointF(hitStroke.points.front().x, hitStroke.points.front().y), hitStroke.points.front().pressure };
        }
        if (param >= maxParam - 0.001f) {
            return { QPointF(hitStroke.points.back().x, hitStroke.points.back().y), hitStroke.points.back().pressure };
        }
        int i0 = std::clamp(static_cast<int>(std::floor(param)), 0, static_cast<int>(hitStroke.points.size()) - 1);
        int i1 = std::clamp(static_cast<int>(std::ceil(param)), 0, static_cast<int>(hitStroke.points.size()) - 1);
        float frac = (i0 == i1) ? 0.0f : (param - i0) / (i1 - i0);
        float p = hitStroke.points[i0].pressure * (1.0f - frac) + hitStroke.points[i1].pressure * frac;
        QPointF pt = crossingPt ? *crossingPt : (QPointF(hitStroke.points[i0].x, hitStroke.points[i0].y) * (1.0f - frac) + QPointF(hitStroke.points[i1].x, hitStroke.points[i1].y) * frac);
        return { pt, p };
    };

    // ストロークの両端 [0, maxParam] と交点群により分割される区間リストを作成
    struct Interval {
        float startParam;
        float endParam;
        QPointF startPt;
        QPointF endPt;
        float startPressure;
        float endPressure;
    };
    std::vector<Interval> intervals;

    float prevParam = 0.0f;
    auto [prevPt, prevPressure] = getPointAndPressure(0.0f, nullptr);

    for (const auto& c : uniqueCrossings) {
        if (c.param > prevParam + 0.001f) {
            auto [curPt, curPressure] = getPointAndPressure(c.param, &c.pt);
            intervals.push_back({ prevParam, c.param, prevPt, curPt, prevPressure, curPressure });
            prevParam = c.param;
            prevPt = curPt;
            prevPressure = curPressure;
        }
    }
    if (maxParam > prevParam + 0.001f) {
        auto [endPt, endPressure] = getPointAndPressure(maxParam, nullptr);
        intervals.push_back({ prevParam, maxParam, prevPt, endPt, prevPressure, endPressure });
    }

    // 4. hitParam が含まれる区間を探して除外
    int removeIntervalIdx = -1;
    for (size_t i = 0; i < intervals.size(); ++i) {
        if (hitParam >= intervals[i].startParam - 0.05f && hitParam <= intervals[i].endParam + 0.05f) {
            removeIntervalIdx = static_cast<int>(i);
            break;
        }
    }

    if (removeIntervalIdx < 0) {
        float minDiff = std::numeric_limits<float>::max();
        for (size_t i = 0; i < intervals.size(); ++i) {
            float mid = (intervals[i].startParam + intervals[i].endParam) * 0.5f;
            float diff = std::abs(hitParam - mid);
            if (diff < minDiff) {
                minDiff = diff;
                removeIntervalIdx = static_cast<int>(i);
            }
        }
    }

    if (removeIntervalIdx < 0) {
        return false;
    }

    // 5. 残った区間から新しいストロークを生成
    std::vector<VectorStroke> remainingStrokes;
    for (size_t i = 0; i < intervals.size(); ++i) {
        if (static_cast<int>(i) == removeIntervalIdx) {
            continue; // この区間を消去！
        }

        const auto& iv = intervals[i];
        if (iv.endParam - iv.startParam < 0.05f) {
            continue;
        }

        VectorStroke newStroke;
        newStroke.baseWidth = hitStroke.baseWidth;
        newStroke.color = hitStroke.color;
        newStroke.density = hitStroke.density;
        newStroke.hardness = hitStroke.hardness;
        newStroke.falloff = hitStroke.falloff;

        // 始点
        newStroke.points.push_back({ static_cast<float>(iv.startPt.x()), static_cast<float>(iv.startPt.y()), iv.startPressure, 1.0f, false });

        // 区間内の元制御点
        for (size_t cp = 0; cp < hitStroke.points.size(); ++cp) {
            float param = static_cast<float>(cp);
            if (param > iv.startParam + 0.05f && param < iv.endParam - 0.05f) {
                newStroke.points.push_back(hitStroke.points[cp]);
            }
        }

        // 終点
        newStroke.points.push_back({ static_cast<float>(iv.endPt.x()), static_cast<float>(iv.endPt.y()), iv.endPressure, 1.0f, false });

        // 重複・近接点の整理
        std::vector<VectorPoint> cleanPoints;
        for (const auto& pt : newStroke.points) {
            if (cleanPoints.empty() || std::hypot(pt.x - cleanPoints.back().x, pt.y - cleanPoints.back().y) > 0.5f) {
                cleanPoints.push_back(pt);
            }
        }
        newStroke.points = std::move(cleanPoints);

        if (newStroke.points.size() >= 2) {
            remainingStrokes.push_back(newStroke);
        }
    }

    m_strokes.erase(m_strokes.begin() + hitStrokeIdx);
    m_strokes.insert(m_strokes.begin() + hitStrokeIdx, remainingStrokes.begin(), remainingStrokes.end());

    if (doRasterize) {
        rasterizeAll();
    }
    return true;
}

void VectorLayer::translate(int dx, int dy) {
    if (dx == 0 && dy == 0) return;
    for (auto& stroke : m_strokes) {
        for (auto& pt : stroke.points) {
            pt.x += dx;
            pt.y += dy;
        }
    }
    rasterizeAll();
}

} // namespace Kaburagi
