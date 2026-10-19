#include "TextLayer.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QImage>
#include <QFontMetricsF>
#include <QStringList>
#include <cmath>
#include <algorithm>
#include <vector>

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

namespace {

struct RubyToken {
    QString baseText;
    QString rubyText;
    bool hasRuby = false;
};

bool isKanji(QChar c) {
    ushort u = c.unicode();
    return (u >= 0x4E00 && u <= 0x9FFF) || (u >= 0x3400 && u <= 0x4DBF);
}

bool isPunctuationTopRight(QChar c) {
    ushort u = c.unicode();
    return (u == 0x3001 || u == 0x3002 || u == 0xFF0C || u == 0xFF0E || u == 0x002C || u == 0x002E);
}

bool isSmallKana(QChar c) {
    static const QString smallChars = QStringLiteral(
        "ぁぃぅぇぉっゃゅょゎ"
        "ァィゥェォッャュョヮヵヶ"
        "ｧｨｩｪｫｯｬｭｮ");
    return smallChars.contains(c);
}

bool shouldRotateVertical(QChar c) {
    static const QString rotateChars = QStringLiteral(
        "ー―—～~…‥"
        "()（）[]［］{}｛｝【】〔〕"
        ":;：；");
    return rotateChars.contains(c);
}

std::vector<RubyToken> parseRubyTokens(const QString& line) {
    std::vector<RubyToken> tokens;
    int i = 0;
    int len = line.length();

    while (i < len) {
        // 1. 青空文庫記法: ｜親文字《ルビ》 または |親文字《ルビ》
        if (line[i] == QLatin1Char('|') || line[i] == QChar(0xFF5C)) {
            int start = i + 1;
            int rubyStart = line.indexOf(QChar(0x300A), start); // 《
            if (rubyStart != -1) {
                int rubyEnd = line.indexOf(QChar(0x300B), rubyStart + 1); // 》
                if (rubyEnd != -1) {
                    RubyToken tok;
                    tok.baseText = line.mid(start, rubyStart - start);
                    tok.rubyText = line.mid(rubyStart + 1, rubyEnd - rubyStart - 1);
                    tok.hasRuby = true;
                    tokens.push_back(tok);
                    i = rubyEnd + 1;
                    continue;
                }
            }
        }

        // 2. Markdown風記法: [親文字](ルビ)
        if (line[i] == QLatin1Char('[')) {
            int closeBracket = line.indexOf(QLatin1Char(']'), i + 1);
            if (closeBracket != -1 && closeBracket + 1 < len && line[closeBracket + 1] == QLatin1Char('(')) {
                int closeParen = line.indexOf(QLatin1Char(')'), closeBracket + 2);
                if (closeParen != -1) {
                    RubyToken tok;
                    tok.baseText = line.mid(i + 1, closeBracket - i - 1);
                    tok.rubyText = line.mid(closeBracket + 2, closeParen - closeBracket - 2);
                    tok.hasRuby = true;
                    tokens.push_back(tok);
                    i = closeParen + 1;
                    continue;
                }
            }
        }

        // 3. 漢字《ルビ》 記法
        if (line[i] == QChar(0x300A)) { // 《
            int rubyEnd = line.indexOf(QChar(0x300B), i + 1); // 》
            if (rubyEnd != -1 && !tokens.empty()) {
                // 直前のトークンから末尾の漢字列を切り出す
                RubyToken& prev = tokens.back();
                if (!prev.hasRuby && !prev.baseText.isEmpty()) {
                    int k = prev.baseText.length() - 1;
                    while (k >= 0 && isKanji(prev.baseText[k])) {
                        --k;
                    }
                    int kanjiStart = k + 1;
                    if (kanjiStart < prev.baseText.length()) {
                        QString kanji = prev.baseText.mid(kanjiStart);
                        prev.baseText.truncate(kanjiStart);
                        if (prev.baseText.isEmpty()) {
                            tokens.pop_back();
                        }
                        RubyToken tok;
                        tok.baseText = kanji;
                        tok.rubyText = line.mid(i + 1, rubyEnd - i - 1);
                        tok.hasRuby = true;
                        tokens.push_back(tok);
                        i = rubyEnd + 1;
                        continue;
                    }
                }
            }
        }

        // 4. 通常文字
        if (tokens.empty() || tokens.back().hasRuby) {
            RubyToken tok;
            tok.baseText = QString(line[i]);
            tok.hasRuby = false;
            tokens.push_back(tok);
        } else {
            tokens.back().baseText.append(line[i]);
        }
        ++i;
    }

    return tokens;
}

void renderHorizontalTextWithRuby(const TextBalloonItem& item, const QRectF& textRect, QPainter& painter) {
    QFont font = item.font;
    QFont rubyFont = font;
    qreal pointSize = font.pointSizeF() > 0 ? font.pointSizeF() : static_cast<qreal>(font.pixelSize());
    if (pointSize <= 0) pointSize = 12.0;
    rubyFont.setPointSizeF(std::max(4.0, pointSize * 0.5));

    QFontMetricsF fm(font);
    QFontMetricsF rfm(rubyFont);

    QStringList lines = item.text.split(QLatin1Char('\n'));
    if (lines.isEmpty()) return;

    qreal baseLineHeight = fm.lineSpacing();
    qreal rubyLineHeight = rfm.height();

    // 全体の高さを計算
    std::vector<bool> lineHasRuby(lines.size(), false);
    std::vector<std::vector<RubyToken>> parsedLines(lines.size());
    qreal totalHeight = 0;

    for (int l = 0; l < lines.size(); ++l) {
        parsedLines[l] = parseRubyTokens(lines[l]);
        bool hasRuby = false;
        for (const auto& tok : parsedLines[l]) {
            if (tok.hasRuby) {
                hasRuby = true;
                break;
            }
        }
        lineHasRuby[l] = hasRuby;
        totalHeight += baseLineHeight + (hasRuby ? rubyLineHeight : 0.0);
    }

    // 垂直方向の配置
    qreal currentY = textRect.top();
    if (item.verticalAlign & Qt::AlignVCenter) {
        currentY = textRect.top() + std::max(0.0, (textRect.height() - totalHeight) * 0.5);
    } else if (item.verticalAlign & Qt::AlignBottom) {
        currentY = textRect.bottom() - totalHeight;
    }

    for (size_t l = 0; l < parsedLines.size(); ++l) {
        const auto& tokens = parsedLines[l];
        bool hasRuby = lineHasRuby[l];

        // 各トークンの幅を計算
        qreal lineWidth = 0.0;
        std::vector<qreal> tokWidths(tokens.size(), 0.0);
        for (size_t t = 0; t < tokens.size(); ++t) {
            qreal bw = fm.horizontalAdvance(tokens[t].baseText);
            qreal rw = tokens[t].hasRuby ? rfm.horizontalAdvance(tokens[t].rubyText) : 0.0;
            tokWidths[t] = std::max(bw, rw);
            lineWidth += tokWidths[t];
        }

        qreal currentX = textRect.left();
        if (item.horizontalAlign & Qt::AlignHCenter) {
            currentX = textRect.left() + std::max(0.0, (textRect.width() - lineWidth) * 0.5);
        } else if (item.horizontalAlign & Qt::AlignRight) {
            currentX = textRect.right() - lineWidth;
        }

        qreal rubyBaseline = currentY + rfm.ascent();
        qreal baseBaseline = (hasRuby ? currentY + rubyLineHeight : currentY) + fm.ascent();

        for (size_t t = 0; t < tokens.size(); ++t) {
            const auto& tok = tokens[t];
            qreal w = tokWidths[t];
            qreal bw = fm.horizontalAdvance(tok.baseText);

            // 親文字
            painter.setFont(font);
            qreal baseX = currentX + (w - bw) * 0.5;
            painter.drawText(QPointF(baseX, baseBaseline), tok.baseText);

            // ルビ
            if (tok.hasRuby) {
                painter.setFont(rubyFont);
                qreal rw = rfm.horizontalAdvance(tok.rubyText);
                qreal rubyX = currentX + (w - rw) * 0.5;
                painter.drawText(QPointF(rubyX, rubyBaseline), tok.rubyText);
            }

            currentX += w;
        }

        currentY += baseLineHeight + (hasRuby ? rubyLineHeight : 0.0);
    }
}

void renderVerticalTextWithRuby(const TextBalloonItem& item, const QRectF& textRect, QPainter& painter) {
    QFont font = item.font;
    QFont rubyFont = font;
    qreal pointSize = font.pointSizeF() > 0 ? font.pointSizeF() : static_cast<qreal>(font.pixelSize());
    if (pointSize <= 0) pointSize = 12.0;
    rubyFont.setPointSizeF(std::max(4.0, pointSize * 0.5));

    QFontMetricsF fm(font);
    QFontMetricsF rfm(rubyFont);

    QStringList lines = item.text.split(QLatin1Char('\n'));
    if (lines.isEmpty()) return;

    qreal charSpacing = fm.height();
    qreal rubyCharSpacing = rfm.height();
    qreal colBaseWidth = fm.horizontalAdvance(QStringLiteral("漢"));
    qreal rubyColWidth = rfm.horizontalAdvance(QStringLiteral("漢"));

    // 列（行）の幅を計算
    std::vector<std::vector<RubyToken>> parsedLines(lines.size());
    std::vector<bool> lineHasRuby(lines.size(), false);
    std::vector<qreal> colWidths(lines.size(), 0.0);
    qreal totalColumnsWidth = 0.0;

    for (int l = 0; l < lines.size(); ++l) {
        parsedLines[l] = parseRubyTokens(lines[l]);
        bool hasRuby = false;
        for (const auto& tok : parsedLines[l]) {
            if (tok.hasRuby) {
                hasRuby = true;
                break;
            }
        }
        lineHasRuby[l] = hasRuby;
        qreal cw = colBaseWidth + (hasRuby ? rubyColWidth + 2.0 : 0.0);
        colWidths[l] = cw;
        totalColumnsWidth += cw;
    }

    // 縦書きは右から左に進む
    qreal startX = textRect.right();
    if (item.horizontalAlign & Qt::AlignHCenter) {
        startX = textRect.right() - std::max(0.0, (textRect.width() - totalColumnsWidth) * 0.5);
    } else if (item.horizontalAlign & Qt::AlignLeft) {
        startX = textRect.left() + totalColumnsWidth;
    }

    qreal currentColRight = startX;

    for (size_t l = 0; l < parsedLines.size(); ++l) {
        const auto& tokens = parsedLines[l];
        bool hasRuby = lineHasRuby[l];
        qreal cw = colWidths[l];
        qreal colLeft = currentColRight - cw;

        // 親文字の列中央Xと、ルビ列の中央X
        qreal rubyCenterX = currentColRight - (rubyColWidth * 0.5);
        qreal baseCenterX = (hasRuby ? currentColRight - rubyColWidth - 2.0 : currentColRight) - (colBaseWidth * 0.5);

        // 各トークンの高さを計算
        qreal totalLineHeight = 0.0;
        for (const auto& tok : tokens) {
            qreal bh = tok.baseText.length() * charSpacing;
            qreal rh = tok.hasRuby ? tok.rubyText.length() * rubyCharSpacing : 0.0;
            totalLineHeight += std::max(bh, rh);
        }

        qreal currentY = textRect.top();
        if (item.verticalAlign & Qt::AlignVCenter) {
            currentY = textRect.top() + std::max(0.0, (textRect.height() - totalLineHeight) * 0.5);
        } else if (item.verticalAlign & Qt::AlignBottom) {
            currentY = textRect.bottom() - totalLineHeight;
        }

        for (const auto& tok : tokens) {
            qreal bh = tok.baseText.length() * charSpacing;
            qreal rh = tok.hasRuby ? tok.rubyText.length() * rubyCharSpacing : 0.0;
            qreal blockHeight = std::max(bh, rh);

            // 親文字の縦描画
            painter.setFont(font);
            qreal y = currentY;
            for (int ci = 0; ci < tok.baseText.length(); ++ci) {
                QChar ch = tok.baseText[ci];
                QRectF charBBox = fm.boundingRect(ch);
                qreal drawX = baseCenterX - (charBBox.width() * 0.5);
                qreal drawY = y + fm.ascent();

                if (isPunctuationTopRight(ch)) {
                    // 句読点は右上に寄せる
                    drawX += colBaseWidth * 0.25;
                    drawY -= charSpacing * 0.35;
                    painter.drawText(QPointF(drawX, drawY), QString(ch));
                } else if (isSmallKana(ch)) {
                    // 促音・拗音は右上に寄せる
                    drawX += colBaseWidth * 0.12;
                    drawY -= charSpacing * 0.1;
                    painter.drawText(QPointF(drawX, drawY), QString(ch));
                } else if (shouldRotateVertical(ch)) {
                    // 長音・括弧・ダッシュ等は90度回転
                    painter.save();
                    painter.translate(baseCenterX, y + charSpacing * 0.5);
                    painter.rotate(90.0);
                    painter.drawText(QPointF(-charBBox.width() * 0.5, charBBox.height() * 0.35), QString(ch));
                    painter.restore();
                } else {
                    painter.drawText(QPointF(drawX, drawY), QString(ch));
                }
                y += charSpacing;
            }

            // ルビの縦描画（親文字の右側）
            if (tok.hasRuby && !tok.rubyText.isEmpty()) {
                painter.setFont(rubyFont);
                qreal ry = currentY;
                // 親文字領域の中央または均等配置
                if (rh < blockHeight && tok.rubyText.length() > 0) {
                    ry = currentY + (blockHeight - rh) * 0.5;
                }
                for (int ri = 0; ri < tok.rubyText.length(); ++ri) {
                    QChar rch = tok.rubyText[ri];
                    QRectF rBBox = rfm.boundingRect(rch);
                    qreal rdrawX = rubyCenterX - (rBBox.width() * 0.5);
                    qreal rdrawY = ry + rfm.ascent();

                    if (shouldRotateVertical(rch)) {
                        painter.save();
                        painter.translate(rubyCenterX, ry + rubyCharSpacing * 0.5);
                        painter.rotate(90.0);
                        painter.drawText(QPointF(-rBBox.width() * 0.5, rBBox.height() * 0.35), QString(rch));
                        painter.restore();
                    } else {
                        painter.drawText(QPointF(rdrawX, rdrawY), QString(rch));
                    }
                    ry += rubyCharSpacing;
                }
            }

            currentY += blockHeight;
        }

        currentColRight = colLeft;
    }
}

} // namespace

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
        painter.save();
        painter.setPen(item.textColor);

        QRectF textRect = item.rect;
        if (item.shape != BalloonShape::None) {
            float pad = item.frameWidth + 6.0f;
            textRect.adjust(pad, pad, -pad, -pad);
        }

        if (item.direction == WritingDirection::Vertical) {
            renderVerticalTextWithRuby(item, textRect, painter);
        } else {
            renderHorizontalTextWithRuby(item, textRect, painter);
        }

        painter.restore();
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
