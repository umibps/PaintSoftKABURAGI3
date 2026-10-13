#include "Document.hpp"
#include "UndoCommands.hpp"
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
    QImage converted = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    auto doc = new Document(converted.width(), converted.height(), dpi, false, Qt::white, parent);

    auto layer = std::make_shared<Layer>("レイヤー 1", converted.width(), converted.height(), LayerType::Raster);
    int w = converted.width();
    int h = converted.height();
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
    doc->setActiveLayerIndex(0);
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
    int newIndex = static_cast<int>(m_layers.size());
    m_layers.push_back(layer);
    m_activeLayerIndex = newIndex;

    emit layerListChanged();
    emit activeLayerChanged(m_activeLayerIndex);

    if (m_undoStack && !m_isUndoRedoInProgress) {
        m_undoStack->push(new AddLayerCommand(this, layer, newIndex));
    }
    return layer;
}

LayerPtr Document::addRasterLayerWithImage(const QImage& img, const QString& name, const QPoint& offset) {
    if (img.isNull()) {
        return addRasterLayer(name);
    }

    QString lName = name.isEmpty() ? QString("レイヤー %1").arg(m_layers.size() + 1) : name;
    auto layer = std::make_shared<Layer>(lName, m_width, m_height, LayerType::Raster);

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
    int newIndex = static_cast<int>(m_layers.size());
    m_layers.push_back(layer);
    m_activeLayerIndex = newIndex;

    emit layerListChanged();
    emit activeLayerChanged(m_activeLayerIndex);

    if (dstX0 <= dstX1 && dstY0 <= dstY1) {
        composite(QRect(dstX0, dstY0, dstX1 - dstX0 + 1, dstY1 - dstY0 + 1));
    } else {
        composite();
    }

    if (m_undoStack && !m_isUndoRedoInProgress) {
        auto cmd = new AddLayerCommand(this, layer, newIndex);
        cmd->setText(tr("貼り付け"));
        m_undoStack->push(cmd);
    }
    return layer;
}

std::shared_ptr<VectorLayer> Document::addVectorLayer(const QString& name) {
    QString lName = name.isEmpty() ? QString("ベクトルレイヤー %1").arg(m_layers.size() + 1) : name;
    auto layer = std::make_shared<VectorLayer>(lName, m_width, m_height);
    int newIndex = static_cast<int>(m_layers.size());
    m_layers.push_back(layer);
    m_activeLayerIndex = newIndex;

    emit layerListChanged();
    emit activeLayerChanged(m_activeLayerIndex);

    if (m_undoStack && !m_isUndoRedoInProgress) {
        m_undoStack->push(new AddLayerCommand(this, layer, newIndex));
    }
    return layer;
}

std::shared_ptr<AdjustmentLayer> Document::addAdjustmentLayer(AdjustmentType type, const QString& name) {
    QString lName = name.isEmpty() ? QString("調整レイヤー %1").arg(m_layers.size() + 1) : name;
    auto layer = std::make_shared<AdjustmentLayer>(lName, m_width, m_height, type);
    int newIndex = static_cast<int>(m_layers.size());
    m_layers.push_back(layer);
    m_activeLayerIndex = newIndex;

    composite(QRect(0, 0, m_width, m_height));
    emit layerListChanged();
    emit activeLayerChanged(m_activeLayerIndex);

    if (m_undoStack && !m_isUndoRedoInProgress) {
        m_undoStack->push(new AddLayerCommand(this, layer, newIndex));
    }
    return layer;
}

std::shared_ptr<TextLayer> Document::addTextLayer(const QString& name) {
    QString lName = name.isEmpty() ? QString("テキスト %1").arg(m_layers.size() + 1) : name;
    auto layer = std::make_shared<TextLayer>(lName, m_width, m_height);
    int newIndex = static_cast<int>(m_layers.size());
    m_layers.push_back(layer);
    m_activeLayerIndex = newIndex;

    composite(QRect(0, 0, m_width, m_height));
    emit layerListChanged();
    emit activeLayerChanged(m_activeLayerIndex);

    if (m_undoStack && !m_isUndoRedoInProgress) {
        m_undoStack->push(new AddLayerCommand(this, layer, newIndex));
    }
    return layer;
}

std::shared_ptr<GroupLayer> Document::addGroupLayer(const QString& name) {
    QString lName = name.isEmpty() ? QString("グループ %1").arg(m_layers.size() + 1) : name;
    auto layer = std::make_shared<GroupLayer>(lName, m_width, m_height);
    int newIndex = static_cast<int>(m_layers.size());
    m_layers.push_back(layer);
    m_activeLayerIndex = newIndex;

    composite(QRect(0, 0, m_width, m_height));
    emit layerListChanged();
    emit activeLayerChanged(m_activeLayerIndex);

    if (m_undoStack && !m_isUndoRedoInProgress) {
        m_undoStack->push(new AddLayerCommand(this, layer, newIndex));
    }
    return layer;
}

std::shared_ptr<ThreeDLayer> Document::addThreeDLayer(const QString& name) {
    clearSelection();
    QString lName = name.isEmpty() ? QString("3Dレイヤー %1").arg(m_layers.size() + 1) : name;
    auto layer = std::make_shared<ThreeDLayer>(lName, m_width, m_height);
    int newIndex = static_cast<int>(m_layers.size());
    m_layers.push_back(layer);
    m_activeLayerIndex = newIndex;

    composite(QRect(0, 0, m_width, m_height));
    emit layerListChanged();
    emit activeLayerChanged(m_activeLayerIndex);

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
                    if (hasSel && !isPixelSelected(px, py)) {
                        continue;
                    }

                    int lx = px - tileX0;
                    int pixelIdx = ly * TileMemoryPool::TILE_WIDTH + lx;

                    if (alphaLocked) {
                        uint32_t curA = (pixels[pixelIdx] >> 24) & 0xFF;
                        if (curA > 0) {
                            pixels[pixelIdx] = (curA << 24) | (fillR << 16) | (fillG << 8) | fillB;
                        }
                    } else {
                        pixels[pixelIdx] = solidPixel;
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
                layer->renderToTarget(grp->tileManager(), targetRect, baseLayer.get());
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
            layer->renderToTarget(m_compositeBuffer, targetRect, baseLayer.get());
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

void Document::setSelection(const QPainterPath& path) {
    m_selectionPath = path;
    m_selectionBounds = path.boundingRect();

    if (!path.isEmpty() && path.elementCount() <= 5) {
        QPainterPath rectTest;
        rectTest.addRect(m_selectionBounds);
        m_selectionIsRect = (path == rectTest);
    } else {
        m_selectionIsRect = false;
    }

    if (m_isSelectionEditMode) {
        updateSelectionMaskFromPath();
    }

    emit selectionChanged();
    emit documentUpdated(QRect());
}

void Document::clearSelection() {
    if (m_selectionPath.isEmpty()) return;
    m_selectionPath = QPainterPath();
    m_selectionBounds = QRectF();
    m_selectionIsRect = false;

    if (m_isSelectionEditMode) {
        updateSelectionMaskFromPath();
    }

    emit selectionChanged();
    emit documentUpdated(QRect());
}

bool Document::isPixelSelected(int x, int y) const {
    if (m_selectionPath.isEmpty()) return true;
    QPointF pt(x + 0.5, y + 0.5);
    if (!m_selectionBounds.contains(pt)) {
        return false;
    }
    if (m_selectionIsRect) return true;
    return m_selectionPath.contains(pt);
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
    int expTilesX = (m_width + TileMemoryPool::TILE_WIDTH - 1) / TileMemoryPool::TILE_WIDTH;
    int expTilesY = (m_height + TileMemoryPool::TILE_HEIGHT - 1) / TileMemoryPool::TILE_HEIGHT;
    if (!m_selectionMaskLayer || m_selectionMaskLayer->tileManager().tilesX() != expTilesX || m_selectionMaskLayer->tileManager().tilesY() != expTilesY) {
        m_selectionMaskLayer = std::make_shared<Layer>("__selection_mask__", m_width, m_height, LayerType::Raster);
    }
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
        clearSelection();
        return;
    }

    QPainterPath fullPath;

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
        poly << QPoint(bx0 + startPt.x(), by0 + startPt.y());

        int loopGuard = 0;
        const int maxLoop = bw * bh * 4 + 1000;

        while (cur != startPt && loopGuard++ < maxLoop) {
            poly << QPoint(bx0 + cur.x(), by0 + cur.y());
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

    setSelection(fullPath);
}

std::shared_ptr<GroupLayer> Document::addPanelFolder(const QPolygonF& polygon, float borderWidth, const QColor& borderColor, const QString& name) {
    int panelCount = 0;
    for (const auto& l : m_layers) {
        if (l->type() == LayerType::Group) {
            auto g = std::dynamic_pointer_cast<GroupLayer>(l);
            if (g && g->isPanelFolder()) panelCount++;
        }
    }
    QString folderName = name.isEmpty() ? QString("コマ %1").arg(panelCount + 1) : name;
    auto panel = std::make_shared<GroupLayer>(folderName, m_width, m_height);
    panel->setPanelFolder(true);
    panel->setPanelPolygon(polygon);
    panel->setPanelBorderWidth(borderWidth);
    panel->setPanelBorderColor(borderColor);
    panel->setDrawPanelBorder(true);

    int insertIdx = m_activeLayerIndex + 1;
    if (insertIdx > static_cast<int>(m_layers.size())) {
        insertIdx = static_cast<int>(m_layers.size());
    }
    m_layers.insert(m_layers.begin() + insertIdx, panel);

    // コマフォルダ内に初期描画用ラスターレイヤーを1枚作成
    auto innerLayer = std::make_shared<Layer>("レイヤー 1", m_width, m_height, LayerType::Raster);
    m_layers.insert(m_layers.begin() + insertIdx + 1, innerLayer);
    setLayerParentGroup(insertIdx + 1, panel);

    m_activeLayerIndex = insertIdx + 1;

    emit layerListChanged();
    emit activeLayerChanged(m_activeLayerIndex);
    emit documentUpdated(QRect(0, 0, m_width, m_height));
    return panel;
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

bool Document::splitPanelFolder(const std::shared_ptr<GroupLayer>& panel, const QLineF& cutLine, float marginX, float marginY) {
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

    m_layers.insert(m_layers.begin() + insertIdx, newPanel);

    auto innerLayer = std::make_shared<Layer>("レイヤー 1", m_width, m_height, LayerType::Raster);
    m_layers.insert(m_layers.begin() + insertIdx + 1, innerLayer);
    setLayerParentGroup(insertIdx + 1, newPanel);

    emit layerListChanged();
    emit documentUpdated(QRect(0, 0, m_width, m_height));
    return true;
}

} // namespace Kaburagi
