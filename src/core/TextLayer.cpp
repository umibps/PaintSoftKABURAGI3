#include "TextLayer.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QImage>
#include <cmath>
#include <algorithm>

namespace Kaburagi {

TextLayer::TextLayer(const QString& name, int width, int height)
    : Layer(name, width, height, LayerType::Text) {
}

int TextLayer::addItem(const TextBalloonItem& item) {
    m_items.push_back(item);
    rasterizeAll();
    return static_cast<int>(m_items.size()) - 1;
}

void TextLayer::removeItem(int index) {
    if (index >= 0 && index < static_cast<int>(m_items.size())) {
        m_items.erase(m_items.begin() + index);
        rasterizeAll();
    }
}

void TextLayer::updateItem(int index, const TextBalloonItem& item) {
    if (index >= 0 && index < static_cast<int>(m_items.size())) {
        m_items[index] = item;
        rasterizeAll();
    }
}

int TextLayer::findItemAt(const QPointF& docPos) const {
    // 上に描画されたアイテム（リストの後方）を優先してヒットテスト
    for (int i = static_cast<int>(m_items.size()) - 1; i >= 0; --i) {
        const auto& item = m_items[i];
        if (item.rect.contains(docPos)) {
            return i;
        }
        if (item.hasTail) {
            // しっぽの先端付近（半径15px以内）もヒットとみなす
            float dx = item.tailTip.x() - docPos.x();
            float dy = item.tailTip.y() - docPos.y();
            if (dx * dx + dy * dy <= 15.0f * 15.0f) {
                return i;
            }
        }
    }
    return -1;
}

void TextLayer::renderItem(const TextBalloonItem& item, QPainter& painter) {
    constexpr double PI = 3.14159265358979323846;
    QPainterPath path;

    if (item.shape == BalloonShape::Ellipse) {
        path.addEllipse(item.rect);
    } else if (item.shape == BalloonShape::RoundedRect) {
        path.addRoundedRect(item.rect, 16.0, 16.0);
    } else if (item.shape == BalloonShape::Rectangle) {
        path.addRect(item.rect);
    } else if (item.shape == BalloonShape::Spike) {
        // ウニフラッシュ / 叫び
        int n = std::clamp(item.spikeCount, 4, 64);
        float phaseRad = item.spikePhase * static_cast<float>(PI / 180.0);
        QPointF c = item.rect.center();
        float rx = static_cast<float>(item.rect.width() * 0.5);
        float ry = static_cast<float>(item.rect.height() * 0.5);
        float depth = std::min(item.spikeDepth, std::min(rx, ry) * 0.8f);

        QPolygonF poly;
        poly.reserve(n * 2);
        for (int i = 0; i < n; ++i) {
            float angleOut = phaseRad + (static_cast<float>(i) * 2.0f * static_cast<float>(PI)) / static_cast<float>(n);
            float pxOut = c.x() + rx * std::cos(angleOut);
            float pyOut = c.y() + ry * std::sin(angleOut);
            poly << QPointF(pxOut, pyOut);

            float angleIn = angleOut + static_cast<float>(PI) / static_cast<float>(n);
            float pxIn = c.x() + (rx - depth) * std::cos(angleIn);
            float pyIn = c.y() + (ry - depth) * std::sin(angleIn);
            poly << QPointF(pxIn, pyIn);
        }
        path.addPolygon(poly);
        path.closeSubpath();
    } else if (item.shape == BalloonShape::Cloud) {
        // モコモコ / 思考
        int n = std::clamp(item.spikeCount, 4, 48);
        float phaseRad = item.spikePhase * static_cast<float>(PI / 180.0);
        QPointF c = item.rect.center();
        float rx = static_cast<float>(item.rect.width() * 0.5);
        float ry = static_cast<float>(item.rect.height() * 0.5);
        float bump = item.spikeDepth;

        for (int i = 0; i < n; ++i) {
            float a0 = phaseRad + (static_cast<float>(i) * 2.0f * static_cast<float>(PI)) / static_cast<float>(n);
            float a1 = phaseRad + (static_cast<float>(i + 1) * 2.0f * static_cast<float>(PI)) / static_cast<float>(n);
            float aMid = (a0 + a1) * 0.5f;

            QPointF p0(c.x() + rx * std::cos(a0), c.y() + ry * std::sin(a0));
            QPointF p1(c.x() + rx * std::cos(a1), c.y() + ry * std::sin(a1));
            QPointF ctrl(c.x() + (rx + bump) * std::cos(aMid), c.y() + (ry + bump) * std::sin(aMid));

            if (i == 0) {
                path.moveTo(p0);
            }
            path.quadTo(ctrl, p1);
        }
        path.closeSubpath();
    }

    if (item.shape != BalloonShape::None && item.hasTail) {
        QPointF center = item.rect.center();
        QPointF tip = item.tailTip;
        QPointF dir = tip - center;
        float len = std::hypot(dir.x(), dir.y());
        if (len > 1.0f) {
            QPointF norm(-dir.y() / len, dir.x() / len);
            float baseW = std::clamp(item.tailBaseWidth, 4.0f, static_cast<float>(std::min(item.rect.width(), item.rect.height())));

            QPointF p1 = center + norm * (baseW * 0.5f);
            QPointF p2 = center - norm * (baseW * 0.5f);

            // カーブ曲率オフセット
            float curveFactor = item.tailCurvature * 0.005f * len;
            QPointF curveOffset = norm * curveFactor;

            QPointF ctrl1 = (p1 + tip) * 0.5f + curveOffset;
            QPointF ctrl2 = (p2 + tip) * 0.5f + curveOffset;

            QPainterPath tailPath;
            tailPath.moveTo(p1);
            tailPath.quadTo(ctrl1, tip);
            tailPath.quadTo(ctrl2, p2);
            tailPath.closeSubpath();

            path = path.united(tailPath);
        }
    }

    if (item.shape != BalloonShape::None) {
        // フチ取り（外側アウトライン描画）
        if (item.hasOutline && item.outlineWidth > 0.0f && item.outlineColor.alpha() > 0) {
            float totalWidth = item.frameWidth + item.outlineWidth * 2.0f;
            QPen outlinePen(item.outlineColor, totalWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.strokePath(path, outlinePen);
        }

        // 背景の塗りつぶし
        if (item.fillColor.alpha() > 0) {
            painter.fillPath(path, item.fillColor);
        }

        // 枠線の描画
        if (item.frameWidth > 0.0f && item.frameColor.alpha() > 0) {
            QPen pen(item.frameColor, item.frameWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.strokePath(path, pen);
        }
    }

    // テキスト描画
    if (!item.text.isEmpty()) {
        painter.setFont(item.font);
        painter.setPen(item.textColor);

        QRectF textRect = item.rect;
        if (item.shape != BalloonShape::None) {
            float pad = item.frameWidth + 6.0f;
            textRect.adjust(pad, pad, -pad, -pad);
        }
        int alignFlags = item.horizontalAlign | item.verticalAlign | Qt::TextWordWrap;
        painter.drawText(textRect, alignFlags, item.text);
    }
}

void TextLayer::rasterizeAll() {
    m_tileManager.clear(0x00000000);
    if (m_items.empty()) return;

    int w = m_tileManager.width();
    int h = m_tileManager.height();

    // 一時画像に全アイテムをアンチエイリアス描画
    QImage canvasImg(w, h, QImage::Format_ARGB32_Premultiplied);
    canvasImg.fill(Qt::transparent);

    {
        QPainter painter(&canvasImg);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        for (const auto& item : m_items) {
            renderItem(item, painter);
        }
    }

    // タイルマネージャにピクセルを転送
    int tilesX = m_tileManager.tilesX();
    int tilesY = m_tileManager.tilesY();

    for (int ty = 0; ty < tilesY; ++ty) {
        for (int tx = 0; tx < tilesX; ++tx) {
            int startX = tx * 128;
            int startY = ty * 128;
            int endX = std::min(w, startX + 128);
            int endY = std::min(h, startY + 128);

            // このタイル範囲にピクセルが存在するか高速チェック
            bool hasPixel = false;
            for (int y = startY; y < endY; ++y) {
                const uint32_t* line = reinterpret_cast<const uint32_t*>(canvasImg.constScanLine(y));
                for (int x = startX; x < endX; ++x) {
                    if (line[x] != 0) {
                        hasPixel = true;
                        break;
                    }
                }
                if (hasPixel) break;
            }

            if (hasPixel) {
                Tile* tile = m_tileManager.getTileForWrite(tx, ty);
                if (tile) {
                    uint32_t* buf = reinterpret_cast<uint32_t*>(tile->writeBuffer());
                    for (int y = startY; y < endY; ++y) {
                        const uint32_t* line = reinterpret_cast<const uint32_t*>(canvasImg.constScanLine(y));
                        int localY = y - startY;
                        for (int x = startX; x < endX; ++x) {
                            int localX = x - startX;
                            buf[localY * 128 + localX] = line[x];
                        }
                    }
                }
            }
        }
    }

    m_tileManager.markDirtyRect(QRect(0, 0, w, h));
    invalidateContentBounds();
}

void TextLayer::translate(int dx, int dy) {
    if (dx == 0 && dy == 0) return;
    for (auto& item : m_items) {
        item.rect.translate(dx, dy);
        if (item.hasTail) {
            item.tailTip += QPointF(dx, dy);
        }
    }
    rasterizeAll();
}

} // namespace Kaburagi
