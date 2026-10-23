#include "Document.hpp"
#include "UndoCommands.hpp"
#include "FilterProcessor.hpp"
#include <algorithm>
#include <unordered_map>
#include <cmath>
#include <QPainter>

namespace Kaburagi {

Document::Document(int width, int height, double dpi, bool createDefaultLayers, const QColor& initialBgColor, QObject* parent)
    : QObject(parent), m_width(width), m_height(height), m_dpi(dpi), m_compositeBuffer(width, height),
      m_primaryBgColor(initialBgColor), m_secondaryBgColor(Qt::white), m_isSecondaryBgColorActive(false) {

    m_undoStack = new QUndoStack(this);
    m_undoStack->setUndoLimit(100);

    if (createDefaultLayers) {
        // 背景レイヤーと描画用レイヤーを作成
        auto bgLayer = std::make_shared<Layer>("背景", width, height, LayerType::Raster);
        uint32_t bgArgb = (initialBgColor.alpha() == 0) ? 0x00000000 : initialBgColor.rgba();
        bgLayer->tileManager().clear(bgArgb);
        m_layers.push_back(bgLayer);

        auto layer1 = std::make_shared<Layer>("レイヤー 1", width, height, LayerType::Raster);
        m_layers.push_back(layer1);

        m_activeLayerIndex = 1;

        // 初回全体合成
        composite(QRect(0, 0, width, height));
    } else {
        m_activeLayerIndex = 0;
    }
}

LayerPtr Document::backgroundLayer() const {
    for (const auto& l : m_layers) {
        if (l && l->name() == "背景") return l;
    }
    if (!m_layers.empty()) return m_layers[0];
    return nullptr;
}

void Document::updateBackgroundLayerPixels() {
    auto bg = backgroundLayer();
    if (!bg) return;
    QColor c = m_isSecondaryBgColorActive ? m_secondaryBgColor : m_primaryBgColor;
    uint32_t argb = (c.alpha() == 0) ? 0x00000000 : c.rgba();
    bg->tileManager().clear(argb);
    bg->invalidateContentBounds();
    composite(QRect(0, 0, m_width, m_height));
    emit documentUpdated(QRect(0, 0, m_width, m_height));
    emit backgroundColorChanged();
}

void Document::setPrimaryBackgroundColor(const QColor& color) {
    m_primaryBgColor = color;
    if (!m_isSecondaryBgColorActive) {
        updateBackgroundLayerPixels();
    } else {
        emit backgroundColorChanged();
    }
}

void Document::setSecondaryBackgroundColor(const QColor& color) {
    m_secondaryBgColor = color;
    if (m_isSecondaryBgColorActive) {
        updateBackgroundLayerPixels();
    } else {
        emit backgroundColorChanged();
    }
}

void Document::switchBackgroundColor() {
    m_isSecondaryBgColorActive = !m_isSecondaryBgColorActive;
    updateBackgroundLayerPixels();
}

void Document::setBackgroundColorActive(bool secondary) {
    if (m_isSecondaryBgColorActive != secondary) {
        m_isSecondaryBgColorActive = secondary;
        updateBackgroundLayerPixels();
    }
}

Document* Document::fromQImage(const QImage& img, double dpi, QObject* parent) {
    if (img.isNull()) return nullptr;
    QImage converted = img.convertToFormat(QImage::Format_ARGB32);
    auto doc = new Document(converted.width(), converted.height(), dpi, false, Qt::white, parent);

    int w = converted.width();
    int h = converted.height();

    // 1. 画像サイズと同じサイズの背景レイヤーを一番下に作成
    auto bgLayer = std::make_shared<Layer>("背景", w, h, LayerType::Raster);
    uint32_t bgArgb = (doc->primaryBackgroundColor().alpha() == 0) ? 0x00000000 : doc->primaryBackgroundColor().rgba();
    bgLayer->tileManager().clear(bgArgb);
    bgLayer->invalidateContentBounds();
    doc->addExistingLayer(bgLayer);

    // 2. 開いた画像を格納する通常レイヤーを作成
    auto layer = std::make_shared<Layer>("レイヤー 1", w, h, LayerType::Raster);
    for (int ty = 0; ty < layer->tileManager().tilesY(); ++ty) {
        for (int tx = 0; tx < layer->tileManager().tilesX(); ++tx) {
            int startX = tx * TileMemoryPool::TILE_WIDTH;
            int startY = ty * TileMemoryPool::TILE_HEIGHT;
            int copyW = std::min(static_cast<int>(TileMemoryPool::TILE_WIDTH), w - startX);
            int copyH = std::min(static_cast<int>(TileMemoryPool::TILE_HEIGHT), h - startY);

            Tile* tile = layer->tileManager().getTileForWrite(tx, ty);
            if (!tile) continue;
            uint32_t* dst = reinterpret_cast<uint32_t*>(tile->writeBuffer());
            for (int y = 0; y < copyH; ++y) {
                const uint32_t* src = reinterpret_cast<const uint32_t*>(converted.constScanLine(startY + y)) + startX;
                std::memcpy(dst + y * TileMemoryPool::TILE_WIDTH, src, copyW * sizeof(uint32_t));
            }
        }
    }
    layer->invalidateContentBounds();
    doc->addExistingLayer(layer);
    doc->setActiveLayerIndex(1);
    doc->composite(QRect(0, 0, w, h));
    return doc;
}

LayerPtr Document::activeLayer() const {
    if (m_activeLayerIndex >= 0 && m_activeLayerIndex < static_cast<int>(m_layers.size())) {
        return m_layers[m_activeLayerIndex];
    }
    return nullptr;
}

LayerPtr Document::getLayer(int index) const {
    if (index >= 0 && index < static_cast<int>(m_layers.size())) {
        return m_layers[index];
    }
    return nullptr;
}

void Document::setActiveLayerIndex(int index) {
    if (index >= 0 && index < static_cast<int>(m_layers.size()) && index != m_activeLayerIndex) {
        m_activeLayerIndex = index;
        emit activeLayerChanged(index);
    }
}

void Document::clearAllLayers() {
    m_layers.clear();
    m_activeLayerIndex = 0;
    emit layerListChanged();
}

void Document::addExistingLayer(LayerPtr layer) {
    if (!layer) return;
    m_layers.push_back(layer);
    m_activeLayerIndex = static_cast<int>(m_layers.size()) - 1;
    emit layerListChanged();
}

LayerPtr Document::addRasterLayer(const QString& name) {
    QString lName = name.isEmpty() ? QString("レイヤー %1").arg(m_layers.size() + 1) : name;
    auto layer = std::make_shared<Layer>(lName, m_width, m_height, LayerType::Raster);
    int newIndex = (m_activeLayerIndex >= 0 && m_activeLayerIndex < static_cast<int>(m_layers.size()))
                   ? (m_activeLayerIndex + 1)
                   : static_cast<int>(m_layers.size());
    insertLayer(newIndex, layer);

    if (m_undoStack && !m_isUndoRedoInProgress) {
        m_undoStack->push(new AddLayerCommand(this, layer, newIndex));
    }
    return layer;
}

LayerPtr Document::addRasterLayerWithImage(const QImage& img, const QString& name, const QPoint& offset, const QJsonObject& aiParams) {
    if (img.isNull()) {
        auto emptyLayer = addRasterLayer(name);
        if (emptyLayer && !aiParams.isEmpty()) {
            emptyLayer->setAiParameters(aiParams);
        }
        return emptyLayer;
    }

    QString lName = name.isEmpty() ? QString("レイヤー %1").arg(m_layers.size() + 1) : name;
    auto layer = std::make_shared<Layer>(lName, m_width, m_height, LayerType::Raster);
    if (!aiParams.isEmpty()) {
        layer->setAiParameters(aiParams);
    }

    QImage converted = img.convertToFormat(QImage::Format_ARGB32);
    int imgW = converted.width();
    int imgH = converted.height();
    int posX = offset.x();
    int posY = offset.y();

    int dstX0 = std::max(0, posX);
    int dstY0 = std::max(0, posY);
    int dstX1 = std::min(m_width - 1, posX + imgW - 1);
    int dstY1 = std::min(m_height - 1, posY + imgH - 1);

    if (dstX0 <= dstX1 && dstY0 <= dstY1) {
        int minTx = dstX0 / TileMemoryPool::TILE_WIDTH;
        int maxTx = dstX1 / TileMemoryPool::TILE_WIDTH;
        int minTy = dstY0 / TileMemoryPool::TILE_HEIGHT;
        int maxTy = dstY1 / TileMemoryPool::TILE_HEIGHT;

        for (int ty = minTy; ty <= maxTy; ++ty) {
            for (int tx = minTx; tx <= maxTx; ++tx) {
                int tileX0 = tx * TileMemoryPool::TILE_WIDTH;
                int tileY0 = ty * TileMemoryPool::TILE_HEIGHT;

                int px0 = std::max(dstX0, tileX0);
                int px1 = std::min(dstX1, tileX0 + static_cast<int>(TileMemoryPool::TILE_WIDTH) - 1);
                int py0 = std::max(dstY0, tileY0);
                int py1 = std::min(dstY1, tileY0 + static_cast<int>(TileMemoryPool::TILE_HEIGHT) - 1);

                if (px0 > px1 || py0 > py1) continue;

                Tile* tile = layer->tileManager().getTileForWrite(tx, ty);
                if (!tile) continue;
                uint32_t* dst = reinterpret_cast<uint32_t*>(tile->writeBuffer());

                for (int py = py0; py <= py1; ++py) {
                    int srcY = py - posY;
                    const uint32_t* srcLine = reinterpret_cast<const uint32_t*>(converted.constScanLine(srcY));
                    int ly = py - tileY0;
                    for (int px = px0; px <= px1; ++px) {
                        int srcX = px - posX;
                        int lx = px - tileX0;
                        dst[ly * TileMemoryPool::TILE_WIDTH + lx] = srcLine[srcX];
                    }
                }
            }
        }
    }

    layer->invalidateContentBounds();
    int newIndex = (m_activeLayerIndex >= 0 && m_activeLayerIndex < static_cast<int>(m_layers.size()))
                   ? (m_activeLayerIndex + 1)
                   : static_cast<int>(m_layers.size());
    insertLayer(newIndex, layer);

    if (dstX0 <= dstX1 && dstY0 <= dstY1) {
        composite(QRect(dstX0, dstY0, dstX1 - dstX0 + 1, dstY1 - dstY0 + 1));
    } else {
        composite();
    }

    if (m_undoStack && !m_isUndoRedoInProgress) {
        auto cmd = new AddLayerCommand(this, layer, newIndex);
        cmd->setText(name.isEmpty() ? tr("貼り付け") : name);
        m_undoStack->push(cmd);
    }
    return layer;
}

std::shared_ptr<VectorLayer> Document::addVectorLayer(const QString& name) {
    QString lName = name.isEmpty() ? QString("ベクトルレイヤー %1").arg(m_layers.size() + 1) : name;
    auto layer = std::make_shared<VectorLayer>(lName, m_width, m_height);
    int newIndex = (m_activeLayerIndex >= 0 && m_activeLayerIndex < static_cast<int>(m_layers.size()))
                   ? (m_activeLayerIndex + 1)
                   : static_cast<int>(m_layers.size());
    insertLayer(newIndex, layer);

    if (m_undoStack && !m_isUndoRedoInProgress) {
        m_undoStack->push(new AddLayerCommand(this, layer, newIndex));
    }
    return layer;
}

std::shared_ptr<AdjustmentLayer> Document::addAdjustmentLayer(AdjustmentType type, const QString& name) {
    QString lName = name.isEmpty() ? QString("調整レイヤー %1").arg(m_layers.size() + 1) : name;
    auto layer = std::make_shared<AdjustmentLayer>(lName, m_width, m_height, type);
    int newIndex = (m_activeLayerIndex >= 0 && m_activeLayerIndex < static_cast<int>(m_layers.size()))
                   ? (m_activeLayerIndex + 1)
                   : static_cast<int>(m_layers.size());
    insertLayer(newIndex, layer);

    if (m_undoStack && !m_isUndoRedoInProgress) {
        m_undoStack->push(new AddLayerCommand(this, layer, newIndex));
    }
    return layer;
}

std::shared_ptr<TextLayer> Document::addTextLayer(const QString& name) {
    QString lName = name.isEmpty() ? QString("テキスト %1").arg(m_layers.size() + 1) : name;
    auto layer = std::make_shared<TextLayer>(lName, m_width, m_height);
    int newIndex = (m_activeLayerIndex >= 0 && m_activeLayerIndex < static_cast<int>(m_layers.size()))
                   ? (m_activeLayerIndex + 1)
                   : static_cast<int>(m_layers.size());
    insertLayer(newIndex, layer);

    if (m_undoStack && !m_isUndoRedoInProgress) {
        m_undoStack->push(new AddLayerCommand(this, layer, newIndex));
    }
    return layer;
}

std::shared_ptr<GroupLayer> Document::addGroupLayer(const QString& name) {
    QString lName = name.isEmpty() ? QString("グループ %1").arg(m_layers.size() + 1) : name;
    auto layer = std::make_shared<GroupLayer>(lName, m_width, m_height);
    int newIndex = (m_activeLayerIndex >= 0 && m_activeLayerIndex < static_cast<int>(m_layers.size()))
                   ? (m_activeLayerIndex + 1)
                   : static_cast<int>(m_layers.size());
    insertLayer(newIndex, layer);

    if (m_undoStack && !m_isUndoRedoInProgress) {
        m_undoStack->push(new AddLayerCommand(this, layer, newIndex));
    }
    return layer;
}

std::shared_ptr<ThreeDLayer> Document::addThreeDLayer(const QString& name) {
    clearSelection();
    QString lName = name.isEmpty() ? QString("3Dレイヤー %1").arg(m_layers.size() + 1) : name;
    auto layer = std::make_shared<ThreeDLayer>(lName, m_width, m_height);
    int newIndex = (m_activeLayerIndex >= 0 && m_activeLayerIndex < static_cast<int>(m_layers.size()))
                   ? (m_activeLayerIndex + 1)
                   : static_cast<int>(m_layers.size());
    insertLayer(newIndex, layer);

    if (m_undoStack && !m_isUndoRedoInProgress) {
        m_undoStack->push(new AddLayerCommand(this, layer, newIndex));
    }
    return layer;
}

void Document::setLayerParentGroup(int layerIndex, const std::shared_ptr<GroupLayer>& group) {
    if (layerIndex < 0 || layerIndex >= static_cast<int>(m_layers.size())) return;
    auto layer = m_layers[layerIndex];
    if (!layer || layer == group) return;

    // 多階層グループ対応: 循環参照チェック (layer が group の先祖でないか)
    if (group) {
        auto cur = group;
        while (cur) {
            if (cur == layer) {
                return; // 循環参照を防止
            }
            cur = cur->parentGroup();
        }
    }

    auto oldGroup = layer->parentGroup();
    if (oldGroup) {
        oldGroup->removeChild(layer);
    }
    if (group) {
        group->addChild(layer);
    } else {
        layer->setParentGroup(nullptr);
    }

    composite(QRect(0, 0, m_width, m_height));
    emit layerListChanged();
}

void Document::insertLayer(int index, LayerPtr layer) {
    if (!layer) return;
    int idx = std::clamp(index, 0, static_cast<int>(m_layers.size()));
    m_layers.insert(m_layers.begin() + idx, layer);
    m_activeLayerIndex = idx;

    composite(QRect(0, 0, m_width, m_height));
    emit layerListChanged();
    emit activeLayerChanged(m_activeLayerIndex);
}

void Document::removeLayer(int index) {
    if (index >= 0 && index < static_cast<int>(m_layers.size()) && m_layers.size() > 1) {
        auto layer = m_layers[index];
        auto group = std::dynamic_pointer_cast<GroupLayer>(layer);
        if (group) {
            group->clearChildren();
        }
        auto parent = layer->parentGroup();
        if (parent) {
            parent->removeChild(layer);
        }
        m_layers.erase(m_layers.begin() + index);
        if (m_activeLayerIndex >= static_cast<int>(m_layers.size())) {
            m_activeLayerIndex = static_cast<int>(m_layers.size()) - 1;
        }
        composite(QRect(0, 0, m_width, m_height));
        emit layerListChanged();
        emit activeLayerChanged(m_activeLayerIndex);

        if (m_undoStack && !m_isUndoRedoInProgress) {
            m_undoStack->push(new RemoveLayerCommand(this, layer, index));
        }
    }
}

void Document::moveLayer(int fromIndex, int toIndex) {
    if (fromIndex == toIndex || fromIndex < 0 || fromIndex >= static_cast<int>(m_layers.size()) ||
        toIndex < 0 || toIndex >= static_cast<int>(m_layers.size())) {
        return;
    }

    // 背景レイヤーより下に配置できないように防止
    if (!m_layers.empty() && m_layers[0]->name() == "背景") {
        if (fromIndex == 0) {
            return; // 背景レイヤー自体は移動不可
        }
        if (toIndex == 0) {
            toIndex = 1; // 背景より下には配置できないため最下層(背景直上)にクランプ
            if (fromIndex == toIndex) return;
        }
    }

    auto layer = m_layers[fromIndex];
    m_layers.erase(m_layers.begin() + fromIndex);
    m_layers.insert(m_layers.begin() + toIndex, layer);
    m_activeLayerIndex = toIndex;

    composite(QRect(0, 0, m_width, m_height));
    emit layerListChanged();
    emit activeLayerChanged(m_activeLayerIndex);

    if (m_undoStack && !m_isUndoRedoInProgress) {
        m_undoStack->push(new MoveLayerCommand(this, fromIndex, toIndex));
    }
}

void Document::replaceLayer(int index, LayerPtr layer) {
    if (index < 0 || index >= static_cast<int>(m_layers.size()) || !layer) return;
    m_layers[index] = layer;
    m_activeLayerIndex = index;
    composite(QRect(0, 0, m_width, m_height));
    emit layerListChanged();
    emit activeLayerChanged(m_activeLayerIndex);
}

LayerPtr Document::duplicateLayer(int index) {
    if (index < 0 || index >= static_cast<int>(m_layers.size())) return nullptr;
    auto orig = m_layers[index];
    if (!orig) return nullptr;

    QString cloneName = orig->name() + " のコピー";
    LayerPtr cloned = nullptr;

    if (orig->type() == LayerType::Vector) {
        auto origVec = std::dynamic_pointer_cast<VectorLayer>(orig);
        auto vecLayer = std::make_shared<VectorLayer>(cloneName, m_width, m_height);
        if (origVec) {
            vecLayer->setStrokes(origVec->strokes());
        }
        cloned = vecLayer;
    } else if (orig->type() == LayerType::Text) {
        auto origText = std::dynamic_pointer_cast<TextLayer>(orig);
        auto textLayer = std::make_shared<TextLayer>(cloneName, m_width, m_height);
        if (origText) {
            for (const auto& item : origText->items()) {
                textLayer->addItem(item);
            }
        }
        cloned = textLayer;
    } else if (orig->type() == LayerType::Adjustment) {
        auto origAdj = std::dynamic_pointer_cast<AdjustmentLayer>(orig);
        AdjustmentType adjType = origAdj ? origAdj->adjustmentType() : AdjustmentType::BrightnessContrast;
        auto adjLayer = std::make_shared<AdjustmentLayer>(cloneName, m_width, m_height, adjType);
        if (origAdj) {
            adjLayer->setBrightnessContrast(origAdj->brightness(), origAdj->contrast());
            adjLayer->setHueSaturation(origAdj->hueShift(), origAdj->saturation(), origAdj->lightness());
            adjLayer->setCurveInterpolationMode(origAdj->curveInterpolationMode());
            for (int ch = 0; ch < 4; ++ch) {
                auto ach = static_cast<AdjustmentChannel>(ch);
                adjLayer->setCurvePoints(ach, origAdj->curvePoints(ach));
                adjLayer->setLevelParams(ach, origAdj->levelParams(ach));
            }
        }
        cloned = adjLayer;
    } else if (orig->type() == LayerType::Group) {
        auto grp = std::make_shared<GroupLayer>(cloneName, m_width, m_height);
        cloned = grp;
    } else {
        auto raster = std::make_shared<Layer>(cloneName, m_width, m_height, LayerType::Raster);
        raster->tileManager() = orig->tileManager().clone();
        cloned = raster;
    }

    if (!cloned) return nullptr;

    cloned->setVisible(orig->isVisible());
    cloned->setOpacity(orig->opacity());
    cloned->setBlendMode(orig->blendMode());
    cloned->setClippingMask(orig->isClippingMask());
    cloned->setAlphaLocked(orig->isAlphaLocked());
    cloned->setLocked(orig->isLocked());
    cloned->setParentGroup(orig->parentGroup());

    int newIndex = index + 1;
    insertLayer(newIndex, cloned);

    if (m_undoStack && !m_isUndoRedoInProgress) {
        auto cmd = new AddLayerCommand(this, cloned, newIndex);
        cmd->setText("レイヤーを複製");
        m_undoStack->push(cmd);
    }
    return cloned;
}

LayerPtr Document::newLayerFromVisible(const QString& name) {
    if (m_layers.empty()) return nullptr;

    // 最新の合成状態を保証
    composite(QRect(0, 0, m_width, m_height));

    QString layerName = name.isEmpty() ? "可視部" : name;
    auto layer = std::make_shared<Layer>(layerName, m_width, m_height, LayerType::Raster);
    layer->tileManager() = m_compositeBuffer.clone();
    layer->invalidateContentBounds();

    int newIndex = m_activeLayerIndex + 1;
    insertLayer(newIndex, layer);

    if (m_undoStack && !m_isUndoRedoInProgress) {
        auto cmd = new AddLayerCommand(this, layer, newIndex);
        cmd->setText("可視部をレイヤーに");
        m_undoStack->push(cmd);
    }
    return layer;
}

bool Document::mergeDown(int index) {
    if (index <= 0 || index >= static_cast<int>(m_layers.size())) return false;
    auto upper = m_layers[index];
    auto lower = m_layers[index - 1];
    if (!upper || !lower) return false;

    int lowerIndex = index - 1;

    auto postLower = std::make_shared<Layer>(lower->name(), m_width, m_height, LayerType::Raster);
    postLower->setVisible(lower->isVisible());
    postLower->setOpacity(lower->opacity());
    postLower->setBlendMode(lower->blendMode());
    postLower->setClippingMask(lower->isClippingMask());
    postLower->setAlphaLocked(lower->isAlphaLocked());
    postLower->setLocked(lower->isLocked());
    postLower->setParentGroup(lower->parentGroup());

    if (lower->type() == LayerType::Raster) {
        postLower->tileManager() = lower->tileManager().clone();
    } else {
        lower->renderToTarget(postLower->tileManager(), QRect(0, 0, m_width, m_height));
    }

    upper->renderToTarget(postLower->tileManager(), QRect(0, 0, m_width, m_height), postLower.get());
    postLower->invalidateContentBounds();

    m_layers[lowerIndex] = postLower;
    m_layers.erase(m_layers.begin() + index);
    m_activeLayerIndex = lowerIndex;

    composite(QRect(0, 0, m_width, m_height));
    emit layerListChanged();
    emit activeLayerChanged(m_activeLayerIndex);

    if (m_undoStack && !m_isUndoRedoInProgress) {
        m_undoStack->push(new MergeLayersCommand(this, index, upper, lowerIndex, lower, postLower));
    }

    return true;
}

LayerPtr Document::rasterizeLayer(int index) {
    if (index < 0 || index >= static_cast<int>(m_layers.size())) return nullptr;
    auto oldLayer = m_layers[index];
    if (!oldLayer) return nullptr;
    if (oldLayer->type() != LayerType::Vector && oldLayer->type() != LayerType::Text) {
        return nullptr;
    }

    auto newLayer = std::make_shared<Layer>(oldLayer->name(), m_width, m_height, LayerType::Raster);
    newLayer->tileManager() = oldLayer->tileManager().clone();
    newLayer->setVisible(oldLayer->isVisible());
    newLayer->setOpacity(oldLayer->opacity());
    newLayer->setBlendMode(oldLayer->blendMode());
    newLayer->setClippingMask(oldLayer->isClippingMask());
    newLayer->setAlphaLocked(oldLayer->isAlphaLocked());
    newLayer->setLocked(oldLayer->isLocked());
    newLayer->setParentGroup(oldLayer->parentGroup());
    newLayer->invalidateContentBounds();

    replaceLayer(index, newLayer);

    if (m_undoStack && !m_isUndoRedoInProgress) {
        m_undoStack->push(new ReplaceLayerCommand(this, index, oldLayer, newLayer, "レイヤーをラスタライズ"));
    }

    return newLayer;
}

QPainterPath Document::createOpaquePath(int layerIndex) const {
    if (layerIndex < 0 || layerIndex >= static_cast<int>(m_layers.size())) {
        return QPainterPath();
    }
    const auto& layer = m_layers[layerIndex];
    if (!layer) return QPainterPath();

    QRect bounds = layer->contentBounds();
    if (bounds.isEmpty()) {
        return QPainterPath();
    }
    bounds = bounds.intersected(QRect(0, 0, m_width, m_height));
    if (bounds.isEmpty()) {
        return QPainterPath();
    }

    int bw = bounds.width();
    int bh = bounds.height();
    int bx0 = bounds.left();
    int by0 = bounds.top();

    std::vector<uint8_t> mask(static_cast<size_t>(bw) * bh, 0);
    int opaqueCount = 0;

    int minTx = bx0 / TileMemoryPool::TILE_WIDTH;
    int maxTx = bounds.right() / TileMemoryPool::TILE_WIDTH;
    int minTy = by0 / TileMemoryPool::TILE_HEIGHT;
    int maxTy = bounds.bottom() / TileMemoryPool::TILE_HEIGHT;

    const auto& tm = layer->tileManager();

    for (int ty = minTy; ty <= maxTy; ++ty) {
        for (int tx = minTx; tx <= maxTx; ++tx) {
            const Tile* tile = tm.getTile(tx, ty);
            if (!tile || !tile->readBuffer()) continue;

            const uint32_t* src = reinterpret_cast<const uint32_t*>(tile->readBuffer());
            int tileX0 = tx * TileMemoryPool::TILE_WIDTH;
            int tileY0 = ty * TileMemoryPool::TILE_HEIGHT;
            int px0 = std::max(bx0, tileX0);
            int px1 = std::min(bounds.right(), tileX0 + static_cast<int>(TileMemoryPool::TILE_WIDTH) - 1);
            int py0 = std::max(by0, tileY0);
            int py1 = std::min(bounds.bottom(), tileY0 + static_cast<int>(TileMemoryPool::TILE_HEIGHT) - 1);

            for (int py = py0; py <= py1; ++py) {
                int ly = py - tileY0;
                int maskY = py - by0;
                for (int px = px0; px <= px1; ++px) {
                    int lx = px - tileX0;
                    uint32_t p = src[ly * TileMemoryPool::TILE_WIDTH + lx];
                    uint32_t a = (p >> 24) & 0xFF;
                    if (a >= 16) {
                        mask[static_cast<size_t>(maskY) * bw + (px - bx0)] = 1;
                        opaqueCount++;
                    }
                }
            }
        }
    }

    if (opaqueCount == 0) {
        return QPainterPath();
    }

    auto packPt = [](int x, int y) -> uint64_t {
        return (static_cast<uint64_t>(static_cast<uint32_t>(y)) << 32) | static_cast<uint32_t>(x);
    };

    std::unordered_map<uint64_t, std::vector<QPoint>> adj;

    for (int y = 0; y < bh; ++y) {
        size_t rowOffset = static_cast<size_t>(y) * bw;
        for (int x = 0; x < bw; ++x) {
            if (!mask[rowOffset + x]) continue;

            // Top edge: (x, y) -> (x+1, y)
            if (y == 0 || !mask[rowOffset - bw + x]) {
                adj[packPt(x, y)].push_back(QPoint(x + 1, y));
            }
            // Right edge: (x+1, y) -> (x+1, y+1)
            if (x == bw - 1 || !mask[rowOffset + x + 1]) {
                adj[packPt(x + 1, y)].push_back(QPoint(x + 1, y + 1));
            }
            // Bottom edge: (x+1, y+1) -> (x, y+1)
            if (y == bh - 1 || !mask[rowOffset + bw + x]) {
                adj[packPt(x + 1, y + 1)].push_back(QPoint(x, y + 1));
            }
            // Left edge: (x, y+1) -> (x, y)
            if (x == 0 || !mask[rowOffset + x - 1]) {
                adj[packPt(x, y + 1)].push_back(QPoint(x, y));
            }
        }
    }

    if (adj.empty()) {
        return QPainterPath();
    }

    QPainterPath path;
    path.setFillRule(Qt::WindingFill);

    for (auto& [key, neighbors] : adj) {
        while (!neighbors.empty()) {
            QPoint startPt(static_cast<int>(key & 0xFFFFFFFF), static_cast<int>(key >> 32));
            QPoint curPt = neighbors.back();
            neighbors.pop_back();

            QPolygonF poly;
            poly << QPointF(startPt.x() + bx0, startPt.y() + by0);

            QPoint prevDir = curPt - startPt;
            QPoint prevPt = startPt;

            int safety = 0;
            const int maxSteps = bw * bh * 4;

            while (curPt != startPt && safety++ < maxSteps) {
                uint64_t curKey = packPt(curPt.x(), curPt.y());
                auto it = adj.find(curKey);
                if (it == adj.end() || it->second.empty()) {
                    break;
                }
                QPoint nextPt = it->second.back();
                it->second.pop_back();

                QPoint curDir = nextPt - curPt;
                if (curDir == prevDir) {
                    poly.back() = QPointF(curPt.x() + bx0, curPt.y() + by0);
                } else {
                    poly << QPointF(curPt.x() + bx0, curPt.y() + by0);
                    prevDir = curDir;
                }
                prevPt = curPt;
                curPt = nextPt;
            }

            if (poly.size() >= 3) {
                poly << poly.first();
                path.addPolygon(poly);
            }
        }
    }

    return path;
}

void Document::fillLayer(int index, const QColor& color) {
    if (index < 0 || index >= static_cast<int>(m_layers.size())) return;
    auto layer = m_layers[index];
    if (!layer || layer->type() != LayerType::Raster || layer->isLocked()) return;

    QRect targetRect = hasSelection()
        ? selectionBounds().toAlignedRect().intersected(QRect(0, 0, m_width, m_height))
        : QRect(0, 0, m_width, m_height);

    if (targetRect.isEmpty()) return;

    TileManager preTiles = layer->tileManager().clone();

    bool alphaLocked = layer->isAlphaLocked();
    uint32_t fillR = color.red();
    uint32_t fillG = color.green();
    uint32_t fillB = color.blue();
    uint32_t fillA = color.alpha();
    uint32_t solidPixel = (fillA << 24) | (fillR << 16) | (fillG << 8) | fillB;

    int minTx = targetRect.left() / TileMemoryPool::TILE_WIDTH;
    int maxTx = targetRect.right() / TileMemoryPool::TILE_WIDTH;
    int minTy = targetRect.top() / TileMemoryPool::TILE_HEIGHT;
    int maxTy = targetRect.bottom() / TileMemoryPool::TILE_HEIGHT;

    bool hasSel = hasSelection();

    for (int ty = minTy; ty <= maxTy; ++ty) {
        for (int tx = minTx; tx <= maxTx; ++tx) {
            int tileX0 = tx * TileMemoryPool::TILE_WIDTH;
            int tileY0 = ty * TileMemoryPool::TILE_HEIGHT;
            int tileX1 = tileX0 + TileMemoryPool::TILE_WIDTH - 1;
            int tileY1 = tileY0 + TileMemoryPool::TILE_HEIGHT - 1;

            int px0 = std::max(targetRect.left(), tileX0);
            int px1 = std::min(targetRect.right(), tileX1);
            int py0 = std::max(targetRect.top(), tileY0);
            int py1 = std::min(targetRect.bottom(), tileY1);

            if (px0 > px1 || py0 > py1) continue;

            Tile* tile = layer->tileManager().getTileForWrite(tx, ty);
            if (!tile || !tile->writeBuffer()) continue;
            uint32_t* pixels = reinterpret_cast<uint32_t*>(tile->writeBuffer());

            for (int py = py0; py <= py1; ++py) {
                int ly = py - tileY0;
                for (int px = px0; px <= px1; ++px) {
                    uint8_t sel = hasSel ? selectionStrength(px, py) : 255;
                    if (sel == 0) continue;

                    int lx = px - tileX0;
                    int pixelIdx = ly * TileMemoryPool::TILE_WIDTH + lx;

                    if (sel == 255) {
                        if (alphaLocked) {
                            uint32_t curA = (pixels[pixelIdx] >> 24) & 0xFF;
                            if (curA > 0) {
                                pixels[pixelIdx] = (curA << 24) | (fillR << 16) | (fillG << 8) | fillB;
                            }
                        } else {
                            pixels[pixelIdx] = solidPixel;
                        }
                    } else {
                        // 0 < sel < 255: 部分選択（半透明ブレンド）
                        float t = sel / 255.0f;
                        uint32_t orig = pixels[pixelIdx];
                        uint32_t origA = (orig >> 24) & 0xFF;
                        uint32_t origR = (orig >> 16) & 0xFF;
                        uint32_t origG = (orig >> 8) & 0xFF;
                        uint32_t origB = orig & 0xFF;

                        if (alphaLocked) {
                            if (origA > 0) {
                                uint32_t newR = static_cast<uint32_t>(origR * (1.0f - t) + fillR * t);
                                uint32_t newG = static_cast<uint32_t>(origG * (1.0f - t) + fillG * t);
                                uint32_t newB = static_cast<uint32_t>(origB * (1.0f - t) + fillB * t);
                                pixels[pixelIdx] = (origA << 24) | (newR << 16) | (newG << 8) | newB;
                            }
                        } else {
                            uint32_t targetA = (solidPixel >> 24) & 0xFF;
                            uint32_t targetR = (solidPixel >> 16) & 0xFF;
                            uint32_t targetG = (solidPixel >> 8) & 0xFF;
                            uint32_t targetB = solidPixel & 0xFF;
                            uint32_t newA = static_cast<uint32_t>(origA * (1.0f - t) + targetA * t);
                            uint32_t newR = static_cast<uint32_t>(origR * (1.0f - t) + targetR * t);
                            uint32_t newG = static_cast<uint32_t>(origG * (1.0f - t) + targetG * t);
                            uint32_t newB = static_cast<uint32_t>(origB * (1.0f - t) + targetB * t);
                            pixels[pixelIdx] = (newA << 24) | (newR << 16) | (newG << 8) | newB;
                        }
                    }
                }
            }
        }
    }

    TileManager postTiles = layer->tileManager().clone();

    if (m_undoStack && !m_isUndoRedoInProgress) {
        auto cmd = new DrawStrokeCommand(this, index, std::move(preTiles), std::move(postTiles), targetRect);
        cmd->setText("レイヤーを描画色で塗りつぶす");
        m_undoStack->push(cmd);
    }

    layer->invalidateContentBounds();
    layer->tileManager().markDirtyRect(targetRect);
    composite(targetRect);
}

void Document::convertBrightnessToOpacity(int index, const QColor& lineColor) {
    if (index < 0 || index >= static_cast<int>(m_layers.size())) return;
    auto targetLayer = m_layers[index];
    if (!targetLayer || targetLayer->type() != LayerType::Raster || targetLayer->isLocked()) return;

    // 対象レイヤーの収集 (指定レイヤー + ピン留めレイヤー)
    std::vector<int> targetIndices;
    targetIndices.push_back(index);
    for (size_t i = 0; i < m_layers.size(); ++i) {
        int idx = static_cast<int>(i);
        if (idx == index) continue;
        const auto& l = m_layers[i];
        if (l && l->isPinned() && l->type() == LayerType::Raster && !l->isLocked()) {
            targetIndices.push_back(idx);
        }
    }

    QRect targetRect = hasSelection()
        ? selectionBounds().toAlignedRect().intersected(QRect(0, 0, m_width, m_height))
        : QRect(0, 0, m_width, m_height);

    if (targetRect.isEmpty()) return;

    std::vector<MultiLayerFilterCommand::Entry> entries;

    for (int idx : targetIndices) {
        auto l = m_layers[idx];
        if (!l) continue;

        MultiLayerFilterCommand::Entry entry;
        entry.layerIndex = idx;
        entry.preTiles = l->tileManager().clone();
        entry.dirtyRect = targetRect;

        QImage origImg = l->tileManager().toQImage();
        QImage converted = FilterProcessor::convertBrightnessToAlpha(origImg, lineColor);

        // 選択範囲がある場合は選択範囲内のみブレンド
        if (hasSelection()) {
            if (selectionMaskLayer()) {
                FilterProcessor::blendWithSelectionMask(converted, origImg,
                                                       selectionMaskLayer().get(),
                                                       m_width, m_height);
            } else {
                FilterProcessor::blendWithSelection(converted, origImg,
                                                    selection(),
                                                    m_width, m_height);
            }
        }

        l->tileManager().fromQImage(converted);
        entry.postTiles = l->tileManager().clone();
        l->invalidateContentBounds();
        l->tileManager().markDirtyRect(targetRect);

        entries.push_back(std::move(entry));
    }

    if (m_undoStack && !m_isUndoRedoInProgress && !entries.empty()) {
        auto cmd = new MultiLayerFilterCommand(this, tr("輝度を透明度に変換"), std::move(entries));
        m_undoStack->push(cmd);
    }

    composite(targetRect);
}

LayerPtr Document::findClippingBaseLayer(int layerIndex) const {
    if (layerIndex < 0 || layerIndex >= static_cast<int>(m_layers.size())) {
        return nullptr;
    }
    auto targetLayer = m_layers[layerIndex];
    if (!targetLayer || !targetLayer->isClippingMask()) {
        return nullptr;
    }

    auto parent = targetLayer->parentGroup();

    // layerIndex より下を走査: 同じ親グループに属する兄弟レイヤーを探す
    // 連続するクリッピングレイヤーを遡り、最初に見つかったクリッピングOFFレイヤーを判定元とする
    for (int i = layerIndex - 1; i >= 0; --i) {
        auto curLayer = m_layers[i];
        if (curLayer->parentGroup() != parent) {
            continue;
        }
        if (!curLayer->isClippingMask()) {
            // 連続の一番下のレイヤーのもうひとつ下のレイヤー
            return curLayer;
        }
    }

    return nullptr;
}

void Document::composite(const QRect& dirtyRect) {
    QRect targetRect = dirtyRect.isEmpty() ? QRect(0, 0, m_width, m_height) : dirtyRect;

    // 1. 多階層グループ対応: 全グループを深さ (depth) の大きい順 (葉から根へ) にソート
    std::vector<std::shared_ptr<GroupLayer>> groups;
    for (const auto& layer : m_layers) {
        if (layer->type() == LayerType::Group) {
            auto grp = std::dynamic_pointer_cast<GroupLayer>(layer);
            if (grp) groups.push_back(grp);
        }
    }

    auto getGroupDepth = [](const std::shared_ptr<GroupLayer>& g) {
        int d = 0;
        auto cur = g->parentGroup();
        while (cur) {
            d++;
            cur = cur->parentGroup();
        }
        return d;
    };

    std::sort(groups.begin(), groups.end(), [&](const auto& a, const auto& b) {
        return getGroupDepth(a) > getGroupDepth(b);
    });

    // 2. 各グループのピクセルデータを透明クリア
    for (const auto& grp : groups) {
        grp->clearTileBuffer(targetRect);
    }

    // 3. 深い順に各グループの内部合成を実行 (直接の子レイヤをドキュメントのスタック順で合成)
    for (const auto& grp : groups) {
        for (size_t i = 0; i < m_layers.size(); ++i) {
            const auto& layer = m_layers[i];
            if (layer->parentGroup() == grp && layer->isVisible()) {
                auto baseLayer = findClippingBaseLayer(static_cast<int>(i));
                layer->renderToTarget(grp->tileManager(), targetRect, baseLayer.get(), m_clipToHiddenBaseLayer);
            }
        }
        if (grp->isPanelFolder()) {
            grp->applyPanelMaskAndBorder(targetRect);
        }
    }

    // 4. ダーティ領域のメインコンポジットバッファをチェッカー透明（0）クリア
    int startTx = std::max(0, targetRect.left() / static_cast<int>(TileMemoryPool::TILE_WIDTH));
    int endTx = std::min(m_compositeBuffer.tilesX() - 1, targetRect.right() / static_cast<int>(TileMemoryPool::TILE_WIDTH));
    int startTy = std::max(0, targetRect.top() / static_cast<int>(TileMemoryPool::TILE_HEIGHT));
    int endTy = std::min(m_compositeBuffer.tilesY() - 1, targetRect.bottom() / static_cast<int>(TileMemoryPool::TILE_HEIGHT));

    for (int ty = startTy; ty <= endTy; ++ty) {
        for (int tx = startTx; tx <= endTx; ++tx) {
            Tile* t = m_compositeBuffer.getTileForWrite(tx, ty);
            if (t && t->writeBuffer()) {
                std::memset(t->writeBuffer(), 0, TileMemoryPool::TILE_BYTE_SIZE);
            }
        }
    }

    // 5. 親グループを持たないトップレベルのレイヤーおよびグループレイヤーを下層から順にメイン合成
    for (size_t i = 0; i < m_layers.size(); ++i) {
        const auto& layer = m_layers[i];
        if (!layer->parentGroup() && layer->isVisible()) {
            auto baseLayer = findClippingBaseLayer(static_cast<int>(i));
            layer->renderToTarget(m_compositeBuffer, targetRect, baseLayer.get(), m_clipToHiddenBaseLayer);
        }
    }

    // CMYKネイティブモード時: コンポジットバッファの対象タイルをCMYKインキ減法混色色域へマッピング
    if (m_colorMode == CanvasColorMode::CMYKNative) {
        for (int ty = startTy; ty <= endTy; ++ty) {
            for (int tx = startTx; tx <= endTx; ++tx) {
                Tile* t = m_compositeBuffer.getTileForWrite(tx, ty);
                if (t && t->writeBuffer()) {
                    uint32_t* pix = reinterpret_cast<uint32_t*>(t->writeBuffer());
                    for (size_t p = 0; p < TileMemoryPool::PIXELS_PER_TILE; ++p) {
                        uint32_t px = pix[p];
                        if ((px & 0xFF000000) != 0) {
                            uint32_t cmyk = ColorCMYK::rgbToCmykInt(px);
                            uint8_t a = (px >> 24) & 0xFF;
                            pix[p] = ColorCMYK::cmykToRgbInt(cmyk, a);
                        }
                    }
                }
            }
        }
    }

    emit documentUpdated(targetRect);
}

QColor Document::samplePixel(int x, int y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return QColor(Qt::white);
    }
    int tx = x / TileMemoryPool::TILE_WIDTH;
    int ty = y / TileMemoryPool::TILE_HEIGHT;
    const Tile* tile = m_compositeBuffer.getTile(tx, ty);
    if (!tile || !tile->isValid() || !tile->readBuffer()) {
        return QColor(Qt::white); // デフォルト白背景
    }
    int lx = x % TileMemoryPool::TILE_WIDTH;
    int ly = y % TileMemoryPool::TILE_HEIGHT;
    const uint32_t* pixels = reinterpret_cast<const uint32_t*>(tile->readBuffer());
    uint32_t p = pixels[ly * TileMemoryPool::TILE_WIDTH + lx];
    uint8_t a = (p >> 24) & 0xFF;
    uint8_t r = (p >> 16) & 0xFF;
    uint8_t g = (p >> 8) & 0xFF;
    uint8_t b = p & 0xFF;
    if (a < 255) {
        float alpha = a / 255.0f;
        r = static_cast<uint8_t>(r * alpha + 255.0f * (1.0f - alpha) + 0.5f);
        g = static_cast<uint8_t>(g * alpha + 255.0f * (1.0f - alpha) + 0.5f);
        b = static_cast<uint8_t>(b * alpha + 255.0f * (1.0f - alpha) + 0.5f);
    }
    return QColor(r, g, b);
}

void Document::ensureSelectionMaskLayer() {
    int expTilesX = (m_width + TileMemoryPool::TILE_WIDTH - 1) / TileMemoryPool::TILE_WIDTH;
    int expTilesY = (m_height + TileMemoryPool::TILE_HEIGHT - 1) / TileMemoryPool::TILE_HEIGHT;
    if (!m_selectionMaskLayer || m_selectionMaskLayer->tileManager().tilesX() != expTilesX || m_selectionMaskLayer->tileManager().tilesY() != expTilesY) {
        m_selectionMaskLayer = std::make_shared<Layer>("__selection_mask__", m_width, m_height, LayerType::Raster);
    }
}

void Document::setSelection(const QPainterPath& path) {
    m_selectionPath = path;
    m_selectionBounds = path.boundingRect();
    m_hasSelection = !path.isEmpty();

    if (!path.isEmpty() && path.elementCount() <= 5) {
        QPainterPath rectTest;
        rectTest.addRect(m_selectionBounds);
        m_selectionIsRect = (path == rectTest);
    } else {
        m_selectionIsRect = false;
    }

    ensureSelectionMaskLayer();
    m_selectionMaskLayer->tileManager().clear(0x00000000);

    if (m_hasSelection) {
        // パスからアンチエイリアス付きで0〜255のグレースケールマスク画像をレンダリング
        QImage maskImg(m_width, m_height, QImage::Format_ARGB32_Premultiplied);
        maskImg.fill(0);
        QPainter p(&maskImg);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillPath(m_selectionPath, QColor(255, 255, 255, 255));
        p.end();
        m_selectionMaskLayer->tileManager().fromQImage(maskImg);
    }

    emit selectionChanged();
    emit documentUpdated(QRect());
}

void Document::setSelectionMask(const QImage& mask) {
    ensureSelectionMaskLayer();
    m_selectionMaskLayer->tileManager().clear(0x00000000);

    if (!mask.isNull()) {
        QImage argbMask = mask.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        m_selectionMaskLayer->tileManager().fromQImage(argbMask);
        updateSelectionPathFromMask();
    } else {
        clearSelection();
    }
}

void Document::clearSelection() {
    if (!m_hasSelection && m_selectionPath.isEmpty()) return;
    m_hasSelection = false;
    m_selectionPath = QPainterPath();
    m_selectionBounds = QRectF();
    m_selectionIsRect = false;

    if (m_selectionMaskLayer) {
        m_selectionMaskLayer->tileManager().clear(0x00000000);
    }

    emit selectionChanged();
    emit documentUpdated(QRect());
}

uint8_t Document::selectionStrength(int x, int y) const {
    if (!m_hasSelection) return 255;
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) return 0;
    if (!m_selectionBounds.contains(QPointF(x + 0.5, y + 0.5))) {
        return 0;
    }
    if (!m_selectionMaskLayer) return 255;

    int tx = x / TileMemoryPool::TILE_WIDTH;
    int ty = y / TileMemoryPool::TILE_HEIGHT;
    const Tile* tile = m_selectionMaskLayer->tileManager().getTile(tx, ty);
    if (!tile || !tile->readBuffer()) return 0;

    int lx = x % TileMemoryPool::TILE_WIDTH;
    int ly = y % TileMemoryPool::TILE_HEIGHT;
    const uint32_t* buf = reinterpret_cast<const uint32_t*>(tile->readBuffer());
    uint32_t px = buf[ly * TileMemoryPool::TILE_WIDTH + lx];
    return static_cast<uint8_t>((px >> 24) & 0xFF);
}

void Document::setSelectionEditMode(bool enabled) {
    if (m_isSelectionEditMode == enabled) return;
    m_isSelectionEditMode = enabled;

    if (m_isSelectionEditMode) {
        updateSelectionMaskFromPath();
    } else {
        updateSelectionPathFromMask();
    }

    emit selectionEditModeChanged(m_isSelectionEditMode);
    emit documentUpdated(QRect());
}

void Document::updateSelectionMaskFromPath() {
    ensureSelectionMaskLayer();
    m_selectionMaskLayer->tileManager().clear(0x00000000);

    if (!m_selectionPath.isEmpty()) {
        QImage maskImg(m_width, m_height, QImage::Format_ARGB32_Premultiplied);
        maskImg.fill(0);
        QPainter p(&maskImg);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillPath(m_selectionPath, QColor(255, 255, 255, 255));
        p.end();
        m_selectionMaskLayer->tileManager().fromQImage(maskImg);
    }
}

void Document::updateSelectionPathFromMask() {
    if (!m_selectionMaskLayer) {
        clearSelection();
        return;
    }

    QRect bounds = m_selectionMaskLayer->contentBounds();
    if (bounds.isEmpty()) {
        clearSelection();
        return;
    }

    bounds = bounds.intersected(QRect(0, 0, m_width, m_height));
    if (bounds.isEmpty()) {
        clearSelection();
        return;
    }

    int bw = bounds.width();
    int bh = bounds.height();
    int bx0 = bounds.left();
    int by0 = bounds.top();

    std::vector<uint8_t> mask(static_cast<size_t>(bw) * bh, 0);
    int opaqueCount = 0;

    int minTx = bx0 / TileMemoryPool::TILE_WIDTH;
    int maxTx = bounds.right() / TileMemoryPool::TILE_WIDTH;
    int minTy = by0 / TileMemoryPool::TILE_HEIGHT;
    int maxTy = bounds.bottom() / TileMemoryPool::TILE_HEIGHT;

    const auto& tm = m_selectionMaskLayer->tileManager();

    for (int ty = minTy; ty <= maxTy; ++ty) {
        for (int tx = minTx; tx <= maxTx; ++tx) {
            const Tile* tile = tm.getTile(tx, ty);
            if (!tile || !tile->readBuffer()) continue;

            const uint32_t* src = reinterpret_cast<const uint32_t*>(tile->readBuffer());
            int tileX0 = tx * TileMemoryPool::TILE_WIDTH;
            int tileY0 = ty * TileMemoryPool::TILE_HEIGHT;
            int px0 = std::max(bx0, tileX0);
            int px1 = std::min(bounds.right(), tileX0 + static_cast<int>(TileMemoryPool::TILE_WIDTH) - 1);
            int py0 = std::max(by0, tileY0);
            int py1 = std::min(bounds.bottom(), tileY0 + static_cast<int>(TileMemoryPool::TILE_HEIGHT) - 1);

            for (int py = py0; py <= py1; ++py) {
                int ly = py - tileY0;
                int maskY = py - by0;
                for (int px = px0; px <= px1; ++px) {
                    int lx = px - tileX0;
                    uint32_t p = src[ly * TileMemoryPool::TILE_WIDTH + lx];
                    uint32_t a = (p >> 24) & 0xFF;
                    // アルファが64以上(約25%)を選択と判定
                    if (a >= 64) {
                        mask[static_cast<size_t>(maskY) * bw + (px - bx0)] = 1;
                        opaqueCount++;
                    }
                }
            }
        }
    }

    if (opaqueCount == 0) {
        clearSelection();
        return;
    }

    QPainterPath fullPath = maskToPath(mask.data(), bw, bh, bx0, by0);
    if (fullPath.isEmpty()) {
        clearSelection();
        return;
    }

    m_selectionPath = fullPath;
    m_selectionBounds = fullPath.boundingRect();
    m_hasSelection = !fullPath.isEmpty();
    m_selectionIsRect = false;

    emit selectionChanged();
    emit documentUpdated(QRect());
}

QPainterPath Document::maskToPath(const uint8_t* mask, int bw, int bh, int offsetX, int offsetY) {
    if (!mask || bw <= 0 || bh <= 0) return QPainterPath();

    auto packPt = [](int x, int y) -> uint64_t {
        return (static_cast<uint64_t>(static_cast<uint32_t>(y)) << 32) | static_cast<uint32_t>(x);
    };

    std::unordered_map<uint64_t, std::vector<QPoint>> adj;

    for (int y = 0; y < bh; ++y) {
        size_t rowOffset = static_cast<size_t>(y) * bw;
        for (int x = 0; x < bw; ++x) {
            if (!mask[rowOffset + x]) continue;

            if (y == 0 || !mask[rowOffset - bw + x]) {
                adj[packPt(x, y)].push_back(QPoint(x + 1, y));
            }
            if (x == bw - 1 || !mask[rowOffset + x + 1]) {
                adj[packPt(x + 1, y)].push_back(QPoint(x + 1, y + 1));
            }
            if (y == bh - 1 || !mask[rowOffset + bw + x]) {
                adj[packPt(x + 1, y + 1)].push_back(QPoint(x, y + 1));
            }
            if (x == 0 || !mask[rowOffset + x - 1]) {
                adj[packPt(x, y + 1)].push_back(QPoint(x, y));
            }
        }
    }

    if (adj.empty()) {
        return QPainterPath();
    }

    QPainterPath fullPath;
    fullPath.setFillRule(Qt::OddEvenFill);

    while (!adj.empty()) {
        auto it = adj.begin();
        uint64_t startKey = it->first;
        int sx = static_cast<int>(startKey & 0xFFFFFFFF);
        int sy = static_cast<int>(startKey >> 32);

        QPoint startPt(sx, sy);
        QPoint cur = it->second.back();
        it->second.pop_back();
        if (it->second.empty()) {
            adj.erase(it);
        }

        QPolygon poly;
        poly << QPoint(offsetX + startPt.x(), offsetY + startPt.y());

        int loopGuard = 0;
        const int maxLoop = bw * bh * 4 + 1000;

        while (cur != startPt && loopGuard++ < maxLoop) {
            poly << QPoint(offsetX + cur.x(), offsetY + cur.y());
            uint64_t key = packPt(cur.x(), cur.y());
            auto fit = adj.find(key);
            if (fit == adj.end() || fit->second.empty()) {
                break;
            }
            QPoint next = fit->second.back();
            fit->second.pop_back();
            if (fit->second.empty()) {
                adj.erase(fit);
            }
            cur = next;
        }

        if (poly.size() >= 3) {
            fullPath.addPolygon(poly);
        }
    }

    return fullPath;
}

std::vector<std::shared_ptr<GroupLayer>> Document::addPanelFolders(const std::vector<PanelCreationSpec>& specs) {
    std::vector<std::shared_ptr<GroupLayer>> createdPanels;
    if (specs.empty()) return createdPanels;

    int baseIdx = (m_activeLayerIndex >= 0 && m_activeLayerIndex < static_cast<int>(m_layers.size()))
                  ? (m_activeLayerIndex + 1)
                  : static_cast<int>(m_layers.size());

    int panelCount = 0;
    for (const auto& l : m_layers) {
        if (l->type() == LayerType::Group) {
            auto g = std::dynamic_pointer_cast<GroupLayer>(l);
            if (g && g->isPanelFolder()) panelCount++;
        }
    }

    std::vector<PanelFolderEntry> entries;
    entries.reserve(specs.size());

    // specs は「コマ 1」「コマ 2」...「コマ N」の順。
    // UI上では「コマ 1」が一番上、「コマ N」が一番下になるよう、
    // m_layers 内ではコマ N (下位インデックス) から コマ 1 (上位インデックス) へ積み上げる。
    int currIdx = baseIdx;
    for (int i = static_cast<int>(specs.size()) - 1; i >= 0; --i) {
        const auto& spec = specs[i];
        QString folderName = spec.name.isEmpty() ? QString("コマ %1").arg(panelCount + i + 1) : spec.name;
        auto panel = std::make_shared<GroupLayer>(folderName, m_width, m_height);
        panel->setPanelFolder(true);
        panel->setPanelPolygon(spec.polygon);
        panel->setPanelBorderWidth(spec.borderWidth);
        panel->setPanelBorderColor(spec.borderColor);
        panel->setDrawPanelBorder(spec.drawBorder);

        auto innerLayer = std::make_shared<Layer>("レイヤー 1", m_width, m_height, LayerType::Raster);

        m_layers.insert(m_layers.begin() + currIdx, innerLayer);
        m_layers.insert(m_layers.begin() + currIdx + 1, panel);
        setLayerParentGroup(currIdx, panel);

        entries.push_back({panel, innerLayer});
        createdPanels.push_back(panel);
        currIdx += 2;
    }

    // コマ 1 の内部レイヤーをアクティブレイヤーにする
    m_activeLayerIndex = currIdx - 2;

    // Undo スタックに登録
    if (m_undoStack && !m_isUndoRedoInProgress) {
        m_undoStack->push(new AddPanelFoldersCommand(this, baseIdx, entries));
    }

    composite(QRect(0, 0, m_width, m_height));
    emit layerListChanged();
    emit activeLayerChanged(m_activeLayerIndex);
    emit documentUpdated(QRect(0, 0, m_width, m_height));

    // createdPanels を specs と同じ順（コマ 1 から順）にして返す
    std::reverse(createdPanels.begin(), createdPanels.end());
    return createdPanels;
}

std::shared_ptr<GroupLayer> Document::addPanelFolder(const QPolygonF& polygon, float borderWidth, const QColor& borderColor, const QString& name) {
    PanelCreationSpec spec;
    spec.polygon = polygon;
    spec.borderWidth = borderWidth;
    spec.borderColor = borderColor;
    spec.drawBorder = true;
    spec.name = name;
    auto panels = addPanelFolders({spec});
    return panels.empty() ? nullptr : panels[0];
}

std::shared_ptr<GroupLayer> Document::findPanelFolderAt(const QPointF& docPt) const {
    for (int i = static_cast<int>(m_layers.size()) - 1; i >= 0; --i) {
        if (m_layers[i]->type() == LayerType::Group && m_layers[i]->isVisible()) {
            auto grp = std::dynamic_pointer_cast<GroupLayer>(m_layers[i]);
            if (grp && grp->isPanelFolder()) {
                if (grp->panelPolygon().containsPoint(docPt, Qt::OddEvenFill)) {
                    return grp;
                }
            }
        }
    }
    return nullptr;
}

bool Document::splitPanelFolderInternal(const std::shared_ptr<GroupLayer>& panel, const QLineF& cutLine,
                                        float marginX, float marginY,
                                        std::shared_ptr<GroupLayer>& outNewPanel,
                                        std::shared_ptr<Layer>& outNewInnerLayer,
                                        int& outInsertIndex) {
    if (!panel || !panel->isPanelFolder()) return false;
    const QPolygonF& origPoly = panel->panelPolygon();
    if (origPoly.size() < 3) return false;

    QPointF d = cutLine.p2() - cutLine.p1();
    double len = std::hypot(d.x(), d.y());
    if (len < 1e-4) return false;

    QPointF norm(-d.y(), d.x());
    QPointF u = norm / len;
    QPointF dir = d / len;

    // 法線の傾きに基づき、X間隔とY間隔を補間
    double normAngle = std::atan2(std::abs(u.y()), std::abs(u.x())); // 0=水平法線(垂直切断), pi/2=垂直法線(水平切断)
    float gap = static_cast<float>(marginX * std::cos(normAngle) + marginY * std::sin(normAngle));
    float halfGap = gap * 0.5f;

    QPointF cutCenter = (cutLine.p1() + cutLine.p2()) * 0.5f;
    double L = 200000.0;

    // 正側半平面
    QPolygonF P_pos;
    P_pos << (cutCenter + halfGap * u - dir * L)
          << (cutCenter + halfGap * u + dir * L)
          << (cutCenter + halfGap * u + dir * L + u * L)
          << (cutCenter + halfGap * u - dir * L + u * L);

    // 負側半平面
    QPolygonF P_neg;
    P_neg << (cutCenter - halfGap * u + dir * L)
          << (cutCenter - halfGap * u - dir * L)
          << (cutCenter - halfGap * u - dir * L - u * L)
          << (cutCenter - halfGap * u + dir * L - u * L);

    QPainterPath origPath;
    origPath.addPolygon(origPoly);
    origPath.closeSubpath();

    QPainterPath posPath;
    posPath.addPolygon(P_pos);
    posPath.closeSubpath();

    QPainterPath negPath;
    negPath.addPolygon(P_neg);
    negPath.closeSubpath();

    QPainterPath resPath1 = origPath.intersected(posPath);
    QPainterPath resPath2 = origPath.intersected(negPath);

    QPolygonF poly1 = resPath1.toFillPolygon();
    QPolygonF poly2 = resPath2.toFillPolygon();

    auto polyArea = [](const QPolygonF& p) {
        if (p.size() < 3) return 0.0;
        double a = 0.0;
        for (int i = 0; i < p.size(); ++i) {
            int next = (i + 1) % p.size();
            a += p[i].x() * p[next].y() - p[next].x() * p[i].y();
        }
        return std::abs(a) * 0.5;
    };

    if (polyArea(poly1) < 20.0 || polyArea(poly2) < 20.0) {
        return false;
    }

    panel->setPanelPolygon(poly1);

    int panelIdx = -1;
    for (size_t i = 0; i < m_layers.size(); ++i) {
        if (m_layers[i] == panel) {
            panelIdx = static_cast<int>(i);
            break;
        }
    }
    if (panelIdx < 0) return false;

    int panelCount = 0;
    for (const auto& l : m_layers) {
        if (l->type() == LayerType::Group) {
            auto g = std::dynamic_pointer_cast<GroupLayer>(l);
            if (g && g->isPanelFolder()) panelCount++;
        }
    }
    auto newPanel = std::make_shared<GroupLayer>(QString("コマ %1").arg(panelCount + 1), m_width, m_height);
    newPanel->setPanelFolder(true);
    newPanel->setPanelPolygon(poly2);
    newPanel->setPanelBorderWidth(panel->panelBorderWidth());
    newPanel->setPanelBorderColor(panel->panelBorderColor());
    newPanel->setDrawPanelBorder(panel->drawPanelBorder());

    int insertIdx = panelIdx + 1;
    while (insertIdx < static_cast<int>(m_layers.size()) && m_layers[insertIdx]->parentGroup() == panel) {
        insertIdx++;
    }

    auto innerLayer = std::make_shared<Layer>("レイヤー 1", m_width, m_height, LayerType::Raster);
    m_layers.insert(m_layers.begin() + insertIdx, innerLayer);
    m_layers.insert(m_layers.begin() + insertIdx + 1, newPanel);
    setLayerParentGroup(insertIdx, newPanel);

    outNewPanel = newPanel;
    outNewInnerLayer = innerLayer;
    outInsertIndex = insertIdx;
    return true;
}

bool Document::splitPanelFolder(const std::shared_ptr<GroupLayer>& panel, const QLineF& cutLine, float marginX, float marginY) {
    if (!panel || !panel->isPanelFolder()) return false;
    QPolygonF oldPoly = panel->panelPolygon();
    std::shared_ptr<GroupLayer> newPanel;
    std::shared_ptr<Layer> innerLayer;
    int insertIdx = -1;

    if (!splitPanelFolderInternal(panel, cutLine, marginX, marginY, newPanel, innerLayer, insertIdx)) {
        return false;
    }

    if (m_undoStack && !m_isUndoRedoInProgress) {
        std::vector<SplitAddedPanelEntry> entries;
        entries.push_back({newPanel, innerLayer, insertIdx});
        m_undoStack->push(new SplitPanelCommand(this, panel, oldPoly, panel->panelPolygon(), entries));
    }

    emit layerListChanged();
    emit documentUpdated(QRect(0, 0, m_width, m_height));
    return true;
}

bool Document::splitPanelFolderEqually(const std::shared_ptr<GroupLayer>& panel, Qt::Orientation orientation, int count, float margin) {
    if (!panel || !panel->isPanelFolder() || count < 2) return false;
    QPolygonF origPoly = panel->panelPolygon();
    QRectF bbox = origPoly.boundingRect();
    if (bbox.width() <= 1.0 || bbox.height() <= 1.0) return false;

    float marginX = (orientation == Qt::Vertical) ? margin : 0.0f;
    float marginY = (orientation == Qt::Horizontal) ? margin : 0.0f;

    double totalDim = (orientation == Qt::Vertical) ? bbox.width() : bbox.height();
    double totalMargin = (count - 1) * margin;
    if (totalMargin >= totalDim - 5.0) return false;

    double pieceDim = (totalDim - totalMargin) / count;

    QPolygonF oldPoly = origPoly;
    std::vector<SplitAddedPanelEntry> addedEntries;

    auto currentTarget = panel;

    for (int i = 1; i < count; ++i) {
        double cutPos = 0.0;
        QLineF cutLine;
        if (orientation == Qt::Vertical) {
            cutPos = bbox.left() + i * pieceDim + (i - 0.5) * margin;
            cutLine = QLineF(QPointF(cutPos, bbox.top() - 10000.0), QPointF(cutPos, bbox.bottom() + 10000.0));
        } else {
            cutPos = bbox.top() + i * pieceDim + (i - 0.5) * margin;
            cutLine = QLineF(QPointF(bbox.left() - 10000.0, cutPos), QPointF(bbox.right() + 10000.0, cutPos));
        }

        std::shared_ptr<GroupLayer> newP;
        std::shared_ptr<Layer> newInner;
        int insIdx = -1;

        if (!splitPanelFolderInternal(currentTarget, cutLine, marginX, marginY, newP, newInner, insIdx)) {
            // ロールバック
            for (auto it = addedEntries.rbegin(); it != addedEntries.rend(); ++it) {
                for (int j = 0; j < static_cast<int>(m_layers.size()); ++j) {
                    if (m_layers[j] == it->panel) {
                        removeLayer(j);
                        break;
                    }
                }
                for (int j = 0; j < static_cast<int>(m_layers.size()); ++j) {
                    if (m_layers[j] == it->innerLayer) {
                        removeLayer(j);
                        break;
                    }
                }
            }
            panel->setPanelPolygon(oldPoly);
            return false;
        }

        // 幾何学的にソート:
        // currentTarget を左側 (または上側)、newP を右側 (または下側) に設定
        QPolygonF poly1 = currentTarget->panelPolygon();
        QPolygonF poly2 = newP->panelPolygon();
        bool needSwap = false;
        if (orientation == Qt::Vertical) {
            if (poly1.boundingRect().center().x() > poly2.boundingRect().center().x()) {
                needSwap = true;
            }
        } else {
            if (poly1.boundingRect().center().y() > poly2.boundingRect().center().y()) {
                needSwap = true;
            }
        }
        if (needSwap) {
            currentTarget->setPanelPolygon(poly2);
            newP->setPanelPolygon(poly1);
        }

        addedEntries.push_back({newP, newInner, insIdx});
        // 次の切断対象は右側/下側の newP
        currentTarget = newP;
    }

    if (m_undoStack && !m_isUndoRedoInProgress) {
        QString text = (orientation == Qt::Vertical)
                       ? QString("コマ枠を縦に%1分割").arg(count)
                       : QString("コマ枠を横に%1分割").arg(count);
        m_undoStack->push(new SplitPanelCommand(this, panel, oldPoly, panel->panelPolygon(), addedEntries, text));
    }

    composite(QRect(0, 0, m_width, m_height));
    emit layerListChanged();
    emit documentUpdated(QRect(0, 0, m_width, m_height));
    return true;
}

std::vector<Document::AdjacentPanelInfo> Document::findAdjacentPanels(const std::shared_ptr<GroupLayer>& panel, float maxDistance) const {
    std::vector<AdjacentPanelInfo> result;
    if (!panel || !panel->isPanelFolder() || panel->panelPolygon().isEmpty()) return result;

    QRectF rA = panel->panelPolygon().boundingRect();

    for (const auto& l : m_layers) {
        auto grp = std::dynamic_pointer_cast<GroupLayer>(l);
        if (!grp || !grp->isPanelFolder() || grp == panel || grp->panelPolygon().isEmpty()) continue;

        QRectF rB = grp->panelPolygon().boundingRect();

        float distTop = rA.top() - rB.bottom();
        float distBottom = rB.top() - rA.bottom();
        float distLeft = rA.left() - rB.right();
        float distRight = rB.left() - rA.right();

        float overlapX = std::min(rA.right(), rB.right()) - std::max(rA.left(), rB.left());
        float overlapY = std::min(rA.bottom(), rB.bottom()) - std::max(rA.top(), rB.top());

        QString dir;
        float d = 999999.0f;

        if (distBottom >= -5.0f && distBottom <= maxDistance && overlapX > 0) {
            dir = "下";
            d = std::max(0.0f, distBottom);
        } else if (distTop >= -5.0f && distTop <= maxDistance && overlapX > 0) {
            dir = "上";
            d = std::max(0.0f, distTop);
        } else if (distLeft >= -5.0f && distLeft <= maxDistance && overlapY > 0) {
            dir = "左";
            d = std::max(0.0f, distLeft);
        } else if (distRight >= -5.0f && distRight <= maxDistance && overlapY > 0) {
            dir = "右";
            d = std::max(0.0f, distRight);
        } else {
            QPointF cA = rA.center();
            QPointF cB = rB.center();
            float centerDist = std::hypot(cA.x() - cB.x(), cA.y() - cB.y());
            float approxDist = std::max(0.0f, static_cast<float>(centerDist - (rA.width() + rB.width() + rA.height() + rB.height()) * 0.25f));
            if (approxDist <= maxDistance) {
                dir = "隣接";
                d = approxDist;
            }
        }

        if (!dir.isEmpty()) {
            AdjacentPanelInfo info;
            info.panel = grp;
            info.direction = dir;
            info.distance = d;
            result.push_back(info);
        }
    }

    std::sort(result.begin(), result.end(), [](const AdjacentPanelInfo& a, const AdjacentPanelInfo& b) {
        return a.distance < b.distance;
    });

    return result;
}

bool Document::mergePanelFolders(const std::shared_ptr<GroupLayer>& panelA, const std::shared_ptr<GroupLayer>& panelB, float maxGap) {
    if (!panelA || !panelB || panelA == panelB) return false;
    if (!panelA->isPanelFolder() || !panelB->isPanelFolder()) return false;

    const QPolygonF& polyA = panelA->panelPolygon();
    const QPolygonF& polyB = panelB->panelPolygon();
    if (polyA.isEmpty() || polyB.isEmpty()) return false;

    // 1. ポリゴンの結合
    QPolygonF mergedPoly;
    QRectF rA = polyA.boundingRect();
    QRectF rB = polyB.boundingRect();

    bool isRectA = (polyA.size() == 4);
    bool isRectB = (polyB.size() == 4);

    if (isRectA && isRectB) {
        bool hAligned = std::abs(rA.left() - rB.left()) <= 8.0 && std::abs(rA.right() - rB.right()) <= 8.0;
        bool vAdjacent = (rA.bottom() <= rB.top() + 5.0 && (rB.top() - rA.bottom()) <= maxGap) ||
                         (rB.bottom() <= rA.top() + 5.0 && (rA.top() - rB.bottom()) <= maxGap);
        if (hAligned && vAdjacent) {
            QRectF u = rA.united(rB);
            mergedPoly << u.topLeft() << u.topRight() << u.bottomRight() << u.bottomLeft();
        } else {
            bool vAligned = std::abs(rA.top() - rB.top()) <= 8.0 && std::abs(rA.bottom() - rB.bottom()) <= 8.0;
            bool hAdjacent = (rA.right() <= rB.left() + 5.0 && (rB.left() - rA.right()) <= maxGap) ||
                             (rB.right() <= rA.left() + 5.0 && (rA.left() - rB.right()) <= maxGap);
            if (vAligned && hAdjacent) {
                QRectF u = rA.united(rB);
                mergedPoly << u.topLeft() << u.topRight() << u.bottomRight() << u.bottomLeft();
            }
        }
    }

    if (mergedPoly.isEmpty()) {
        QPainterPath pathA;
        pathA.addPolygon(polyA);
        pathA.closeSubpath();

        QPainterPath pathB;
        pathB.addPolygon(polyB);
        pathB.closeSubpath();

        QPainterPath bridgePath;
        for (int i = 0; i < polyA.size(); ++i) {
            QPointF a1 = polyA[i];
            QPointF a2 = polyA[(i + 1) % polyA.size()];
            for (int j = 0; j < polyB.size(); ++j) {
                QPointF b1 = polyB[j];
                QPointF b2 = polyB[(j + 1) % polyB.size()];

                QPointF midA = (a1 + a2) * 0.5;
                QPointF midB = (b1 + b2) * 0.5;
                double dist = std::hypot(midA.x() - midB.x(), midA.y() - midB.y());
                if (dist <= maxGap * 1.5) {
                    double d1 = std::hypot(a1.x() - b1.x(), a1.y() - b1.y()) + std::hypot(a2.x() - b2.x(), a2.y() - b2.y());
                    double d2 = std::hypot(a1.x() - b2.x(), a1.y() - b2.y()) + std::hypot(a2.x() - b1.x(), a2.y() - b1.y());
                    QPolygonF br;
                    if (d1 <= d2) {
                        br << a1 << a2 << b2 << b1;
                    } else {
                        br << a1 << a2 << b1 << b2;
                    }
                    bridgePath.addPolygon(br);
                    bridgePath.closeSubpath();
                }
            }
        }

        QPainterPath combined = pathA.united(pathB).united(bridgePath).simplified();
        auto subPolys = combined.toSubpathPolygons();
        if (!subPolys.isEmpty()) {
            int bestIdx = 0;
            double maxArea = 0;
            for (int k = 0; k < subPolys.size(); ++k) {
                QRectF br = subPolys[k].boundingRect();
                double area = br.width() * br.height();
                if (area > maxArea) {
                    maxArea = area;
                    bestIdx = k;
                }
            }
            mergedPoly = subPolys[bestIdx];
        } else {
            QRectF u = rA.united(rB);
            mergedPoly << u.topLeft() << u.topRight() << u.bottomRight() << u.bottomLeft();
        }
    }

    // 2. パネルBの子レイヤーを収集し、親をパネルAに付け替える
    std::vector<LayerPtr> childLayersB;
    for (auto& l : m_layers) {
        if (l && l->parentGroup() == panelB) {
            childLayersB.push_back(l);
            l->setParentGroup(panelA);
        }
    }

    // 3. パネルBのインデックスを特定してレイヤーリストから削除
    int panelBIndex = -1;
    for (int i = 0; i < static_cast<int>(m_layers.size()); ++i) {
        if (m_layers[i] == panelB) {
            panelBIndex = i;
            break;
        }
    }
    if (panelBIndex < 0) return false;

    QPolygonF oldPolyA = polyA;
    panelA->setPanelPolygon(mergedPoly);
    m_layers.erase(m_layers.begin() + panelBIndex);

    if (m_activeLayerIndex == panelBIndex) {
        for (int i = 0; i < static_cast<int>(m_layers.size()); ++i) {
            if (m_layers[i] == panelA) {
                m_activeLayerIndex = i;
                break;
            }
        }
    } else if (m_activeLayerIndex > panelBIndex) {
        m_activeLayerIndex--;
    }

    // 4. Undo コマンドの登録
    if (m_undoStack && !m_isUndoRedoInProgress) {
        m_undoStack->push(new MergePanelFoldersCommand(this, panelA, oldPolyA, mergedPoly, panelB, panelBIndex, childLayersB));
    }

    composite(QRect(0, 0, m_width, m_height));
    emit layerListChanged();
    emit activeLayerChanged(m_activeLayerIndex);
    return true;
}

void Document::setColorMode(CanvasColorMode mode) {
    if (m_colorMode == mode) return;
    m_colorMode = mode;

    if (m_colorMode == CanvasColorMode::CMYKNative) {
        // 全ラスターレイヤーのピクセルを CMYK 空間 (CMYK色域制限) にクランプ変換
        for (auto& l : m_layers) {
            if (l->type() == LayerType::Raster) {
                auto& tm = l->tileManager();
                for (int ty = 0; ty < tm.tilesY(); ++ty) {
                    for (int tx = 0; tx < tm.tilesX(); ++tx) {
                        Tile* t = tm.getTileForWrite(tx, ty);
                        if (t && t->writeBuffer()) {
                            uint32_t* pix = reinterpret_cast<uint32_t*>(t->writeBuffer());
                            for (size_t p = 0; p < TileMemoryPool::PIXELS_PER_TILE; ++p) {
                                if ((pix[p] & 0xFF000000) != 0) {
                                    uint32_t cmyk = ColorCMYK::rgbToCmykInt(pix[p]);
                                    uint8_t a = (pix[p] >> 24) & 0xFF;
                                    pix[p] = ColorCMYK::cmykToRgbInt(cmyk, a);
                                }
                            }
                        }
                    }
                }
                l->invalidateContentBounds();
            }
        }
    }

    composite(QRect(0, 0, m_width, m_height));
    emit colorModeChanged(m_colorMode);
}

void Document::setClipToHiddenBaseLayer(bool enabled) {
    if (m_clipToHiddenBaseLayer == enabled) return;
    m_clipToHiddenBaseLayer = enabled;
    composite(QRect(0, 0, m_width, m_height));
    emit clipToHiddenBaseLayerChanged(m_clipToHiddenBaseLayer);
}

std::vector<CanvasLayerSnapshot> Document::snapshotAllLayers() const {
    std::vector<CanvasLayerSnapshot> snapshots;
    snapshots.reserve(m_layers.size());
    for (const auto& l : m_layers) {
        CanvasLayerSnapshot s;
        s.tiles = l->tileManager().clone();
        if (l->type() == LayerType::Vector) {
            auto vec = std::dynamic_pointer_cast<VectorLayer>(l);
            if (vec) s.vectorStrokes = vec->strokes();
        } else if (l->type() == LayerType::Text) {
            auto text = std::dynamic_pointer_cast<TextLayer>(l);
            if (text) s.textItems = text->items();
        } else if (l->type() == LayerType::Group) {
            auto grp = std::dynamic_pointer_cast<GroupLayer>(l);
            if (grp) s.panelPolygon = grp->panelPolygon();
        }
        snapshots.push_back(std::move(s));
    }
    return snapshots;
}

void Document::applyCanvasTransform(int w, int h, const std::vector<CanvasLayerSnapshot>& snapshots, const QPainterPath& sel) {
    m_width = w;
    m_height = h;
    for (size_t i = 0; i < m_layers.size() && i < snapshots.size(); ++i) {
        auto& l = m_layers[i];
        l->tileManager() = snapshots[i].tiles.clone();
        if (l->type() == LayerType::Vector) {
            auto vec = std::dynamic_pointer_cast<VectorLayer>(l);
            if (vec) {
                vec->setStrokes(snapshots[i].vectorStrokes);
                vec->rasterizeAll();
            }
        } else if (l->type() == LayerType::Text) {
            auto text = std::dynamic_pointer_cast<TextLayer>(l);
            if (text) {
                text->items() = snapshots[i].textItems;
                text->rasterizeAll();
            }
        } else if (l->type() == LayerType::Group) {
            auto grp = std::dynamic_pointer_cast<GroupLayer>(l);
            if (grp) {
                grp->setPanelPolygon(snapshots[i].panelPolygon);
            }
        }
        l->invalidateContentBounds();
    }
    m_compositeBuffer = TileManager(m_width, m_height);
    m_selectionPath = sel;
    m_hasSelection = !sel.isEmpty();
    m_selectionBounds = m_hasSelection ? sel.boundingRect() : QRectF();
    updateSelectionMaskFromPath();

    composite(QRect(0, 0, m_width, m_height));
    emit canvasResized(m_width, m_height);
    emit layerListChanged();
    emit documentUpdated(QRect(0, 0, m_width, m_height));
}

void Document::changeCanvasSize(int newWidth, int newHeight, int offsetX, int offsetY) {
    if (newWidth <= 0 || newHeight <= 0) return;
    if (newWidth == m_width && newHeight == m_height && offsetX == 0 && offsetY == 0) return;

    int oldW = m_width;
    int oldH = m_height;
    QPainterPath oldSel = m_selectionPath;
    auto oldSnapshots = snapshotAllLayers();

    QColor bgColor = m_isSecondaryBgColorActive ? m_secondaryBgColor : m_primaryBgColor;
    auto bg = backgroundLayer();

    for (size_t i = 0; i < m_layers.size(); ++i) {
        auto& l = m_layers[i];
        if (l->type() == LayerType::Raster || l->type() == LayerType::ThreeD) {
            TileManager newTm(newWidth, newHeight);
            if (l == bg) {
                QImage oldImg = l->tileManager().toQImage();
                QImage newImg(newWidth, newHeight, QImage::Format_ARGB32);
                newImg.fill(bgColor);
                QPainter p(&newImg);
                p.drawImage(offsetX, offsetY, oldImg);
                p.end();
                newTm.fromQImage(newImg);
            } else {
                QImage oldImg = l->tileManager().toQImage();
                QImage newImg(newWidth, newHeight, QImage::Format_ARGB32);
                newImg.fill(0);
                QPainter p(&newImg);
                p.drawImage(offsetX, offsetY, oldImg);
                p.end();
                newTm.fromQImage(newImg);
            }
            l->tileManager() = std::move(newTm);
            l->invalidateContentBounds();
        } else if (l->type() == LayerType::Vector) {
            auto vec = std::dynamic_pointer_cast<VectorLayer>(l);
            if (vec) {
                TileManager newTm(newWidth, newHeight);
                vec->tileManager() = std::move(newTm);
                for (auto& stroke : vec->strokes()) {
                    for (auto& pt : stroke.points) {
                        pt.x += offsetX;
                        pt.y += offsetY;
                    }
                }
                vec->rasterizeAll();
                vec->invalidateContentBounds();
            }
        } else if (l->type() == LayerType::Text) {
            auto text = std::dynamic_pointer_cast<TextLayer>(l);
            if (text) {
                TileManager newTm(newWidth, newHeight);
                text->tileManager() = std::move(newTm);
                for (auto& item : text->items()) {
                    item.rect.translate(offsetX, offsetY);
                    if (item.hasTail) {
                        item.tailTip += QPointF(offsetX, offsetY);
                    }
                }
                text->rasterizeAll();
                text->invalidateContentBounds();
            }
        } else if (l->type() == LayerType::Group) {
            auto grp = std::dynamic_pointer_cast<GroupLayer>(l);
            if (grp) {
                TileManager newTm(newWidth, newHeight);
                grp->tileManager() = std::move(newTm);
                if (grp->isPanelFolder() && !grp->panelPolygon().isEmpty()) {
                    QPolygonF poly = grp->panelPolygon();
                    poly.translate(offsetX, offsetY);
                    grp->setPanelPolygon(poly);
                }
                grp->invalidateContentBounds();
            }
        }
    }

    m_width = newWidth;
    m_height = newHeight;
    m_compositeBuffer = TileManager(m_width, m_height);

    if (m_hasSelection) {
        m_selectionPath.translate(offsetX, offsetY);
        m_selectionBounds = m_selectionPath.boundingRect();
        updateSelectionMaskFromPath();
    }

    composite(QRect(0, 0, m_width, m_height));

    auto newSnapshots = snapshotAllLayers();
    if (m_undoStack && !m_isUndoRedoInProgress) {
        m_undoStack->push(new CanvasTransformCommand(this, oldW, oldH, m_width, m_height,
                                                    oldSnapshots, newSnapshots,
                                                    oldSel, m_selectionPath,
                                                    tr("Change Canvas Size")));
    }

    emit canvasResized(m_width, m_height);
    emit layerListChanged();
    emit documentUpdated(QRect(0, 0, m_width, m_height));
}

void Document::scaleCanvas(int newWidth, int newHeight, Qt::TransformationMode mode) {
    if (newWidth <= 0 || newHeight <= 0) return;
    if (newWidth == m_width && newHeight == m_height) return;

    int oldW = m_width;
    int oldH = m_height;
    QPainterPath oldSel = m_selectionPath;
    auto oldSnapshots = snapshotAllLayers();

    float sx = static_cast<float>(newWidth) / oldW;
    float sy = static_cast<float>(newHeight) / oldH;

    for (size_t i = 0; i < m_layers.size(); ++i) {
        auto& l = m_layers[i];
        if (l->type() == LayerType::Raster || l->type() == LayerType::ThreeD) {
            QImage oldImg = l->tileManager().toQImage();
            QImage scaledImg = oldImg.scaled(newWidth, newHeight, Qt::IgnoreAspectRatio, mode);
            TileManager newTm(newWidth, newHeight);
            newTm.fromQImage(scaledImg);
            l->tileManager() = std::move(newTm);
            l->invalidateContentBounds();
        } else if (l->type() == LayerType::Vector) {
            auto vec = std::dynamic_pointer_cast<VectorLayer>(l);
            if (vec) {
                TileManager newTm(newWidth, newHeight);
                vec->tileManager() = std::move(newTm);
                for (auto& stroke : vec->strokes()) {
                    for (auto& pt : stroke.points) {
                        pt.x *= sx;
                        pt.y *= sy;
                    }
                    stroke.baseWidth *= (sx + sy) * 0.5f;
                }
                vec->rasterizeAll();
                vec->invalidateContentBounds();
            }
        } else if (l->type() == LayerType::Text) {
            auto text = std::dynamic_pointer_cast<TextLayer>(l);
            if (text) {
                TileManager newTm(newWidth, newHeight);
                text->tileManager() = std::move(newTm);
                for (auto& item : text->items()) {
                    item.rect = QRectF(item.rect.x() * sx, item.rect.y() * sy,
                                       item.rect.width() * sx, item.rect.height() * sy);
                    if (item.hasTail) {
                        item.tailTip = QPointF(item.tailTip.x() * sx, item.tailTip.y() * sy);
                        item.tailBaseWidth *= (sx + sy) * 0.5f;
                    }
                    item.frameWidth *= (sx + sy) * 0.5f;
                    item.outlineWidth *= (sx + sy) * 0.5f;
                    qreal ptSize = item.font.pointSizeF();
                    if (ptSize > 0) {
                        item.font.setPointSizeF(std::max(1.0, ptSize * ((sx + sy) * 0.5)));
                    }
                }
                text->rasterizeAll();
                text->invalidateContentBounds();
            }
        } else if (l->type() == LayerType::Group) {
            auto grp = std::dynamic_pointer_cast<GroupLayer>(l);
            if (grp) {
                TileManager newTm(newWidth, newHeight);
                grp->tileManager() = std::move(newTm);
                if (grp->isPanelFolder() && !grp->panelPolygon().isEmpty()) {
                    QPolygonF poly;
                    for (const auto& pt : grp->panelPolygon()) {
                        poly << QPointF(pt.x() * sx, pt.y() * sy);
                    }
                    grp->setPanelPolygon(poly);
                }
                grp->invalidateContentBounds();
            }
        }
    }

    m_width = newWidth;
    m_height = newHeight;
    m_compositeBuffer = TileManager(m_width, m_height);

    if (m_hasSelection) {
        QTransform t;
        t.scale(sx, sy);
        m_selectionPath = t.map(m_selectionPath);
        m_selectionBounds = m_selectionPath.boundingRect();
        updateSelectionMaskFromPath();
    }

    composite(QRect(0, 0, m_width, m_height));

    auto newSnapshots = snapshotAllLayers();
    if (m_undoStack && !m_isUndoRedoInProgress) {
        m_undoStack->push(new CanvasTransformCommand(this, oldW, oldH, m_width, m_height,
                                                    oldSnapshots, newSnapshots,
                                                    oldSel, m_selectionPath,
                                                    tr("Scale/Resize Canvas")));
    }

    emit canvasResized(m_width, m_height);
    emit layerListChanged();
    emit documentUpdated(QRect(0, 0, m_width, m_height));
}

void Document::transformCanvas(CanvasTransformType type) {
    int oldW = m_width;
    int oldH = m_height;
    int newW = oldW;
    int newH = oldH;
    if (type == CanvasTransformType::Rotate90CW || type == CanvasTransformType::Rotate90CCW) {
        newW = oldH;
        newH = oldW;
    }

    QPainterPath oldSel = m_selectionPath;
    auto oldSnapshots = snapshotAllLayers();

    auto transformImage = [&](const QImage& src) -> QImage {
        switch (type) {
            case CanvasTransformType::Rotate90CW: {
                int w = src.width();
                int h = src.height();
                QImage dst(h, w, QImage::Format_ARGB32);
                for (int y = 0; y < h; ++y) {
                    const uint32_t* srcLine = reinterpret_cast<const uint32_t*>(src.constScanLine(y));
                    for (int x = 0; x < w; ++x) {
                        int dx = h - 1 - y;
                        int dy = x;
                        *reinterpret_cast<uint32_t*>(dst.scanLine(dy) + dx * 4) = srcLine[x];
                    }
                }
                return dst;
            }
            case CanvasTransformType::Rotate90CCW: {
                int w = src.width();
                int h = src.height();
                QImage dst(h, w, QImage::Format_ARGB32);
                for (int y = 0; y < h; ++y) {
                    const uint32_t* srcLine = reinterpret_cast<const uint32_t*>(src.constScanLine(y));
                    for (int x = 0; x < w; ++x) {
                        int dx = y;
                        int dy = w - 1 - x;
                        *reinterpret_cast<uint32_t*>(dst.scanLine(dy) + dx * 4) = srcLine[x];
                    }
                }
                return dst;
            }
            case CanvasTransformType::Rotate180:
                return src.mirrored(true, true);
            case CanvasTransformType::FlipHorizontal:
                return src.mirrored(true, false);
            case CanvasTransformType::FlipVertical:
                return src.mirrored(false, true);
        }
        return src;
    };

    auto mapPoint = [&](const QPointF& pt) -> QPointF {
        switch (type) {
            case CanvasTransformType::Rotate90CW:
                return QPointF(oldH - 1 - pt.y(), pt.x());
            case CanvasTransformType::Rotate90CCW:
                return QPointF(pt.y(), oldW - 1 - pt.x());
            case CanvasTransformType::Rotate180:
                return QPointF(oldW - 1 - pt.x(), oldH - 1 - pt.y());
            case CanvasTransformType::FlipHorizontal:
                return QPointF(oldW - 1 - pt.x(), pt.y());
            case CanvasTransformType::FlipVertical:
                return QPointF(pt.x(), oldH - 1 - pt.y());
        }
        return pt;
    };

    for (size_t i = 0; i < m_layers.size(); ++i) {
        auto& l = m_layers[i];
        if (l->type() == LayerType::Raster || l->type() == LayerType::ThreeD) {
            QImage oldImg = l->tileManager().toQImage();
            QImage transImg = transformImage(oldImg);
            TileManager newTm(newW, newH);
            newTm.fromQImage(transImg);
            l->tileManager() = std::move(newTm);
            l->invalidateContentBounds();
        } else if (l->type() == LayerType::Vector) {
            auto vec = std::dynamic_pointer_cast<VectorLayer>(l);
            if (vec) {
                TileManager newTm(newW, newH);
                vec->tileManager() = std::move(newTm);
                for (auto& stroke : vec->strokes()) {
                    for (auto& pt : stroke.points) {
                        QPointF np = mapPoint(QPointF(pt.x, pt.y));
                        pt.x = np.x();
                        pt.y = np.y();
                    }
                }
                vec->rasterizeAll();
                vec->invalidateContentBounds();
            }
        } else if (l->type() == LayerType::Text) {
            auto text = std::dynamic_pointer_cast<TextLayer>(l);
            if (text) {
                TileManager newTm(newW, newH);
                text->tileManager() = std::move(newTm);
                for (auto& item : text->items()) {
                    QPointF p1 = mapPoint(item.rect.topLeft());
                    QPointF p2 = mapPoint(item.rect.bottomRight());
                    item.rect = QRectF(std::min(p1.x(), p2.x()), std::min(p1.y(), p2.y()),
                                       std::abs(p2.x() - p1.x()), std::abs(p2.y() - p1.y()));
                    if (item.hasTail) {
                        item.tailTip = mapPoint(item.tailTip);
                    }
                }
                text->rasterizeAll();
                text->invalidateContentBounds();
            }
        } else if (l->type() == LayerType::Group) {
            auto grp = std::dynamic_pointer_cast<GroupLayer>(l);
            if (grp) {
                TileManager newTm(newW, newH);
                grp->tileManager() = std::move(newTm);
                if (grp->isPanelFolder() && !grp->panelPolygon().isEmpty()) {
                    QPolygonF poly;
                    for (const auto& pt : grp->panelPolygon()) {
                        poly << mapPoint(pt);
                    }
                    grp->setPanelPolygon(poly);
                }
                grp->invalidateContentBounds();
            }
        }
    }

    m_width = newW;
    m_height = newH;
    m_compositeBuffer = TileManager(m_width, m_height);

    if (m_hasSelection) {
        QTransform pathT;
        switch (type) {
            case CanvasTransformType::Rotate90CW:
                pathT.translate(oldH - 1, 0);
                pathT.rotate(90);
                break;
            case CanvasTransformType::Rotate90CCW:
                pathT.translate(0, oldW - 1);
                pathT.rotate(-90);
                break;
            case CanvasTransformType::Rotate180:
                pathT.translate(oldW - 1, oldH - 1);
                pathT.rotate(180);
                break;
            case CanvasTransformType::FlipHorizontal:
                pathT.translate(oldW - 1, 0);
                pathT.scale(-1, 1);
                break;
            case CanvasTransformType::FlipVertical:
                pathT.translate(0, oldH - 1);
                pathT.scale(1, -1);
                break;
        }
        m_selectionPath = pathT.map(m_selectionPath);
        m_selectionBounds = m_selectionPath.boundingRect();
        updateSelectionMaskFromPath();
    }

    composite(QRect(0, 0, m_width, m_height));

    QString actionText;
    switch (type) {
        case CanvasTransformType::Rotate90CW: actionText = tr("Rotate 90° Clockwise"); break;
        case CanvasTransformType::Rotate90CCW: actionText = tr("Rotate 90° Counter-Clockwise"); break;
        case CanvasTransformType::Rotate180: actionText = tr("Rotate 180°"); break;
        case CanvasTransformType::FlipHorizontal: actionText = tr("Flip Horizontally"); break;
        case CanvasTransformType::FlipVertical: actionText = tr("Flip Vertically"); break;
    }

    auto newSnapshots = snapshotAllLayers();
    if (m_undoStack && !m_isUndoRedoInProgress) {
        m_undoStack->push(new CanvasTransformCommand(this, oldW, oldH, m_width, m_height,
                                                    oldSnapshots, newSnapshots,
                                                    oldSel, m_selectionPath,
                                                    actionText));
    }

    emit canvasResized(m_width, m_height);
    emit layerListChanged();
    emit documentUpdated(QRect(0, 0, m_width, m_height));
}

} // namespace Kaburagi
