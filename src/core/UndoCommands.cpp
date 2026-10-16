#include "UndoCommands.hpp"
#include "Document.hpp"
#include "Layer.hpp"
#include "VectorLayer.hpp"
#include "TextLayer.hpp"

namespace Kaburagi {

DrawStrokeCommand::DrawStrokeCommand(Document* doc, int layerIndex, TileManager preTiles, TileManager postTiles, const QRect& dirtyRect, QUndoCommand* parent)
    : QUndoCommand("ブラシ描画", parent)
    , m_doc(doc)
    , m_layerIndex(layerIndex)
    , m_preTiles(std::move(preTiles))
    , m_postTiles(std::move(postTiles))
    , m_dirtyRect(dirtyRect)
{
}

void DrawStrokeCommand::undo() {
    if (!m_doc) return;
    auto layer = m_doc->getLayer(m_layerIndex);
    if (layer) {
        layer->tileManager() = m_preTiles.clone();
        layer->invalidateContentBounds();
        m_doc->setActiveLayerIndex(m_layerIndex);
        m_doc->composite(m_dirtyRect);
    }
}

void DrawStrokeCommand::redo() {
    if (m_firstRun) {
        m_firstRun = false;
        return;
    }
    if (!m_doc) return;
    auto layer = m_doc->getLayer(m_layerIndex);
    if (layer) {
        layer->tileManager() = m_postTiles.clone();
        layer->invalidateContentBounds();
        m_doc->setActiveLayerIndex(m_layerIndex);
        m_doc->composite(m_dirtyRect);
    }
}

SelectionMaskStrokeCommand::SelectionMaskStrokeCommand(Document* doc, TileManager preTiles, TileManager postTiles, const QRect& dirtyRect, QUndoCommand* parent)
    : QUndoCommand("選択範囲編集", parent)
    , m_doc(doc)
    , m_preTiles(std::move(preTiles))
    , m_postTiles(std::move(postTiles))
    , m_dirtyRect(dirtyRect)
{
}

void SelectionMaskStrokeCommand::undo() {
    if (!m_doc) return;
    auto maskL = m_doc->selectionMaskLayer();
    if (maskL) {
        maskL->tileManager() = m_preTiles.clone();
        maskL->invalidateContentBounds();
        m_doc->updateSelectionPathFromMask();
        emit m_doc->documentUpdated(m_dirtyRect);
    }
}

void SelectionMaskStrokeCommand::redo() {
    if (m_firstRun) {
        m_firstRun = false;
        return;
    }
    if (!m_doc) return;
    auto maskL = m_doc->selectionMaskLayer();
    if (maskL) {
        maskL->tileManager() = m_postTiles.clone();
        maskL->invalidateContentBounds();
        m_doc->updateSelectionPathFromMask();
        emit m_doc->documentUpdated(m_dirtyRect);
    }
}

FillBucketCommand::FillBucketCommand(Document* doc, int layerIndex, TileManager preTiles, TileManager postTiles, const QRect& dirtyRect, QUndoCommand* parent)
    : QUndoCommand("塗りつぶし", parent)
    , m_doc(doc)
    , m_layerIndex(layerIndex)
    , m_preTiles(std::move(preTiles))
    , m_postTiles(std::move(postTiles))
    , m_dirtyRect(dirtyRect)
{
}

void FillBucketCommand::undo() {
    if (!m_doc) return;
    auto layer = m_doc->getLayer(m_layerIndex);
    if (layer) {
        layer->tileManager() = m_preTiles.clone();
        layer->invalidateContentBounds();
        m_doc->setActiveLayerIndex(m_layerIndex);
        m_doc->composite(m_dirtyRect);
    }
}

void FillBucketCommand::redo() {
    if (m_firstRun) {
        m_firstRun = false;
        return;
    }
    if (!m_doc) return;
    auto layer = m_doc->getLayer(m_layerIndex);
    if (layer) {
        layer->tileManager() = m_postTiles.clone();
        layer->invalidateContentBounds();
        m_doc->setActiveLayerIndex(m_layerIndex);
        m_doc->composite(m_dirtyRect);
    }
}

AddVectorStrokeCommand::AddVectorStrokeCommand(Document* doc, int layerIndex, const VectorStroke& stroke, QUndoCommand* parent)
    : QUndoCommand("ベクトル線描画", parent)
    , m_doc(doc)
    , m_layerIndex(layerIndex)
    , m_stroke(stroke)
{
}

void AddVectorStrokeCommand::undo() {
    if (!m_doc) return;
    auto layer = m_doc->getLayer(m_layerIndex);
    auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(layer);
    if (vecLayer && !vecLayer->strokes().empty()) {
        vecLayer->strokes().pop_back();
        vecLayer->rasterizeAll();
        m_doc->setActiveLayerIndex(m_layerIndex);
        m_doc->composite();
    }
}

void AddVectorStrokeCommand::redo() {
    if (m_firstRun) {
        m_firstRun = false;
        return;
    }
    if (!m_doc) return;
    auto layer = m_doc->getLayer(m_layerIndex);
    auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(layer);
    if (vecLayer) {
        vecLayer->addStroke(m_stroke);
        m_doc->setActiveLayerIndex(m_layerIndex);
        m_doc->composite();
    }
}

ModifyVectorStrokesCommand::ModifyVectorStrokesCommand(Document* doc, int layerIndex, const std::vector<VectorStroke>& beforeStrokes, const std::vector<VectorStroke>& afterStrokes, const QString& text, QUndoCommand* parent)
    : QUndoCommand(text, parent)
    , m_doc(doc)
    , m_layerIndex(layerIndex)
    , m_beforeStrokes(beforeStrokes)
    , m_afterStrokes(afterStrokes)
{
}

void ModifyVectorStrokesCommand::undo() {
    if (!m_doc) return;
    auto layer = m_doc->getLayer(m_layerIndex);
    auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(layer);
    if (vecLayer) {
        vecLayer->setStrokes(m_beforeStrokes);
        m_doc->setActiveLayerIndex(m_layerIndex);
        m_doc->composite();
    }
}

void ModifyVectorStrokesCommand::redo() {
    if (m_firstRun) {
        m_firstRun = false;
        return;
    }
    if (!m_doc) return;
    auto layer = m_doc->getLayer(m_layerIndex);
    auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(layer);
    if (vecLayer) {
        vecLayer->setStrokes(m_afterStrokes);
        m_doc->setActiveLayerIndex(m_layerIndex);
        m_doc->composite();
    }
}

AddLayerCommand::AddLayerCommand(Document* doc, LayerPtr layer, int index, QUndoCommand* parent)
    : QUndoCommand("新規レイヤー", parent)
    , m_doc(doc)
    , m_layer(layer)
    , m_index(index)
{
}

void AddLayerCommand::undo() {
    if (!m_doc) return;
    m_doc->setUndoRedoInProgress(true);
    m_doc->removeLayer(m_index);
    m_doc->setUndoRedoInProgress(false);
}

void AddLayerCommand::redo() {
    if (m_firstRun) {
        m_firstRun = false;
        return;
    }
    if (!m_doc) return;
    m_doc->setUndoRedoInProgress(true);
    m_doc->insertLayer(m_index, m_layer);
    m_doc->setUndoRedoInProgress(false);
}

AddPanelFoldersCommand::AddPanelFoldersCommand(Document* doc, int baseIndex, const std::vector<PanelFolderEntry>& entries, QUndoCommand* parent)
    : QUndoCommand("新規コマフォルダ", parent)
    , m_doc(doc)
    , m_baseIndex(baseIndex)
    , m_entries(entries)
{
}

void AddPanelFoldersCommand::undo() {
    if (!m_doc) return;
    m_doc->setUndoRedoInProgress(true);
    // 上位インデックスから順に削除（総レイヤー数は entries.size() * 2）
    int totalLayers = static_cast<int>(m_entries.size()) * 2;
    for (int i = totalLayers - 1; i >= 0; --i) {
        m_doc->removeLayer(m_baseIndex + i);
    }
    m_doc->setUndoRedoInProgress(false);
}

void AddPanelFoldersCommand::redo() {
    if (m_firstRun) {
        m_firstRun = false;
        return;
    }
    if (!m_doc) return;
    m_doc->setUndoRedoInProgress(true);
    int currIdx = m_baseIndex;
    for (const auto& entry : m_entries) {
        m_doc->insertLayer(currIdx, entry.innerLayer);
        m_doc->insertLayer(currIdx + 1, entry.panel);
        m_doc->setLayerParentGroup(currIdx, entry.panel);
        currIdx += 2;
    }
    // コマ1の内部レイヤーをアクティブにする
    if (!m_entries.empty()) {
        int activeIdx = m_baseIndex + static_cast<int>(m_entries.size() - 1) * 2;
        m_doc->setActiveLayerIndex(activeIdx);
    }
    m_doc->setUndoRedoInProgress(false);
    m_doc->composite();
    emit m_doc->layerListChanged();
    emit m_doc->activeLayerChanged(m_doc->activeLayerIndex());
}

RemoveLayerCommand::RemoveLayerCommand(Document* doc, LayerPtr layer, int index, QUndoCommand* parent)
    : QUndoCommand("レイヤー削除", parent)
    , m_doc(doc)
    , m_layer(layer)
    , m_index(index)
{
}

void RemoveLayerCommand::undo() {
    if (!m_doc) return;
    m_doc->setUndoRedoInProgress(true);
    m_doc->insertLayer(m_index, m_layer);
    m_doc->setUndoRedoInProgress(false);
}

void RemoveLayerCommand::redo() {
    if (m_firstRun) {
        m_firstRun = false;
        return;
    }
    if (!m_doc) return;
    m_doc->setUndoRedoInProgress(true);
    m_doc->removeLayer(m_index);
    m_doc->setUndoRedoInProgress(false);
}

MoveLayerCommand::MoveLayerCommand(Document* doc, int fromIndex, int toIndex, QUndoCommand* parent)
    : QUndoCommand("レイヤー順序変更", parent)
    , m_doc(doc)
    , m_fromIndex(fromIndex)
    , m_toIndex(toIndex)
{
}

void MoveLayerCommand::undo() {
    if (!m_doc) return;
    m_doc->setUndoRedoInProgress(true);
    m_doc->moveLayer(m_toIndex, m_fromIndex);
    m_doc->setUndoRedoInProgress(false);
}

void MoveLayerCommand::redo() {
    if (m_firstRun) {
        m_firstRun = false;
        return;
    }
    if (!m_doc) return;
    m_doc->setUndoRedoInProgress(true);
    m_doc->moveLayer(m_fromIndex, m_toIndex);
    m_doc->setUndoRedoInProgress(false);
}

AddTextBalloonCommand::AddTextBalloonCommand(Document* doc, int layerIndex, const TextBalloonItem& item, QUndoCommand* parent)
    : QUndoCommand("吹き出し追加", parent)
    , m_doc(doc)
    , m_layerIndex(layerIndex)
    , m_item(item)
{
}

void AddTextBalloonCommand::undo() {
    if (!m_doc) return;
    auto layer = m_doc->getLayer(m_layerIndex);
    auto textLayer = std::dynamic_pointer_cast<TextLayer>(layer);
    if (textLayer && !textLayer->items().empty()) {
        textLayer->removeItem(static_cast<int>(textLayer->items().size()) - 1);
        m_doc->setActiveLayerIndex(m_layerIndex);
        m_doc->composite();
    }
}

void AddTextBalloonCommand::redo() {
    if (m_firstRun) {
        m_firstRun = false;
        return;
    }
    if (!m_doc) return;
    auto layer = m_doc->getLayer(m_layerIndex);
    auto textLayer = std::dynamic_pointer_cast<TextLayer>(layer);
    if (textLayer) {
        textLayer->addItem(m_item);
        m_doc->setActiveLayerIndex(m_layerIndex);
        m_doc->composite();
    }
}

EditTextBalloonCommand::EditTextBalloonCommand(Document* doc, int layerIndex, int itemIndex, const TextBalloonItem& oldItem, const TextBalloonItem& newItem, QUndoCommand* parent)
    : QUndoCommand("吹き出し編集", parent)
    , m_doc(doc)
    , m_layerIndex(layerIndex)
    , m_itemIndex(itemIndex)
    , m_oldItem(oldItem)
    , m_newItem(newItem)
{
}

void EditTextBalloonCommand::undo() {
    if (!m_doc) return;
    auto layer = m_doc->getLayer(m_layerIndex);
    auto textLayer = std::dynamic_pointer_cast<TextLayer>(layer);
    if (textLayer && m_itemIndex >= 0 && m_itemIndex < static_cast<int>(textLayer->items().size())) {
        textLayer->updateItem(m_itemIndex, m_oldItem);
        m_doc->setActiveLayerIndex(m_layerIndex);
        m_doc->composite();
    }
}

void EditTextBalloonCommand::redo() {
    if (!m_doc) return;
    auto layer = m_doc->getLayer(m_layerIndex);
    auto textLayer = std::dynamic_pointer_cast<TextLayer>(layer);
    if (textLayer && m_itemIndex >= 0 && m_itemIndex < static_cast<int>(textLayer->items().size())) {
        textLayer->updateItem(m_itemIndex, m_newItem);
        m_doc->setActiveLayerIndex(m_layerIndex);
        m_doc->composite();
    }
}

TranslateLayerCommand::TranslateLayerCommand(Document* doc, int layerIndex, int dx, int dy, QUndoCommand* parent)
    : QUndoCommand("レイヤー/グループ移動", parent)
    , m_doc(doc)
    , m_layerIndex(layerIndex)
    , m_dx(dx)
    , m_dy(dy)
{
}

void TranslateLayerCommand::undo() {
    if (!m_doc) return;
    auto layer = m_doc->getLayer(m_layerIndex);
    if (layer) {
        layer->translate(-m_dx, -m_dy);
        m_doc->setActiveLayerIndex(m_layerIndex);
        m_doc->composite();
    }
}

void TranslateLayerCommand::redo() {
    if (m_firstRun) {
        m_firstRun = false;
        return;
    }
    if (!m_doc) return;
    auto layer = m_doc->getLayer(m_layerIndex);
    if (layer) {
        layer->translate(m_dx, m_dy);
        m_doc->setActiveLayerIndex(m_layerIndex);
        m_doc->composite();
    }
}

SelectCommand::SelectCommand(Document* doc, const QPainterPath& oldPath, const QPainterPath& newPath, QUndoCommand* parent)
    : QUndoCommand("選択範囲の変更", parent)
    , m_doc(doc)
    , m_oldPath(oldPath)
    , m_newPath(newPath)
{
}

void SelectCommand::undo() {
    if (m_doc) {
        m_doc->setSelection(m_oldPath);
    }
}

void SelectCommand::redo() {
    if (m_doc) {
        m_doc->setSelection(m_newPath);
    }
}

MergeLayersCommand::MergeLayersCommand(Document* doc, int upperIndex, LayerPtr upperLayer, int lowerIndex, LayerPtr preLowerLayer, LayerPtr postLowerLayer, QUndoCommand* parent)
    : QUndoCommand("下のレイヤーと結合", parent)
    , m_doc(doc)
    , m_upperIndex(upperIndex)
    , m_upperLayer(upperLayer)
    , m_lowerIndex(lowerIndex)
    , m_preLowerLayer(preLowerLayer)
    , m_postLowerLayer(postLowerLayer)
{
}

void MergeLayersCommand::undo() {
    if (!m_doc) return;
    m_doc->setUndoRedoInProgress(true);
    m_doc->replaceLayer(m_lowerIndex, m_preLowerLayer);
    m_doc->insertLayer(m_upperIndex, m_upperLayer);
    m_doc->setActiveLayerIndex(m_upperIndex);
    m_doc->setUndoRedoInProgress(false);
    m_doc->composite();
}

void MergeLayersCommand::redo() {
    if (m_firstRun) {
        m_firstRun = false;
        return;
    }
    if (!m_doc) return;
    m_doc->setUndoRedoInProgress(true);
    m_doc->removeLayer(m_upperIndex);
    m_doc->replaceLayer(m_lowerIndex, m_postLowerLayer);
    m_doc->setActiveLayerIndex(m_lowerIndex);
    m_doc->setUndoRedoInProgress(false);
    m_doc->composite();
}

ReplaceLayerCommand::ReplaceLayerCommand(Document* doc, int index, LayerPtr oldLayer, LayerPtr newLayer, const QString& text, QUndoCommand* parent)
    : QUndoCommand(text, parent)
    , m_doc(doc)
    , m_index(index)
    , m_oldLayer(oldLayer)
    , m_newLayer(newLayer)
{
}

void ReplaceLayerCommand::undo() {
    if (!m_doc) return;
    m_doc->setUndoRedoInProgress(true);
    m_doc->replaceLayer(m_index, m_oldLayer);
    m_doc->setActiveLayerIndex(m_index);
    m_doc->setUndoRedoInProgress(false);
    m_doc->composite();
}

void ReplaceLayerCommand::redo() {
    if (m_firstRun) {
        m_firstRun = false;
        return;
    }
    if (!m_doc) return;
    m_doc->setUndoRedoInProgress(true);
    m_doc->replaceLayer(m_index, m_newLayer);
    m_doc->setActiveLayerIndex(m_index);
    m_doc->setUndoRedoInProgress(false);
    m_doc->composite();
}

TransformLayerCommand::TransformLayerCommand(Document* doc, int layerIndex,
                                             TileManager preTiles, TileManager postTiles, const QRect& dirtyRect,
                                             bool hasSelection,
                                             const QPainterPath& oldSel,
                                             const QPainterPath& newSel,
                                             const QString& text,
                                             QUndoCommand* parent)
    : QUndoCommand(text, parent)
    , m_doc(doc)
    , m_layerIndex(layerIndex)
    , m_isVector(false)
    , m_preTiles(std::move(preTiles))
    , m_postTiles(std::move(postTiles))
    , m_dirtyRect(dirtyRect)
    , m_hasSelection(hasSelection)
    , m_oldSel(oldSel)
    , m_newSel(newSel)
{
}

TransformLayerCommand::TransformLayerCommand(Document* doc, int layerIndex,
                                             const std::vector<VectorStroke>& preStrokes,
                                             const std::vector<VectorStroke>& postStrokes,
                                             bool hasSelection,
                                             const QPainterPath& oldSel,
                                             const QPainterPath& newSel,
                                             const QString& text,
                                             QUndoCommand* parent)
    : QUndoCommand(text, parent)
    , m_doc(doc)
    , m_layerIndex(layerIndex)
    , m_isVector(true)
    , m_preStrokes(preStrokes)
    , m_postStrokes(postStrokes)
    , m_hasSelection(hasSelection)
    , m_oldSel(oldSel)
    , m_newSel(newSel)
{
}

void TransformLayerCommand::undo() {
    if (!m_doc) return;
    auto layer = m_doc->getLayer(m_layerIndex);
    if (!layer) return;

    if (m_isVector) {
        auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(layer);
        if (vecLayer) {
            vecLayer->setStrokes(m_preStrokes);
        }
    } else if (m_preTiles.has_value()) {
        layer->tileManager() = m_preTiles->clone();
        layer->invalidateContentBounds();
    }

    if (m_hasSelection) {
        m_doc->setSelection(m_oldSel);
    }

    m_doc->setActiveLayerIndex(m_layerIndex);
    if (m_dirtyRect.isValid()) {
        m_doc->composite(m_dirtyRect);
    } else {
        m_doc->composite();
    }
}

void TransformLayerCommand::redo() {
    if (m_firstRun) {
        m_firstRun = false;
        return;
    }
    if (!m_doc) return;
    auto layer = m_doc->getLayer(m_layerIndex);
    if (!layer) return;

    if (m_isVector) {
        auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(layer);
        if (vecLayer) {
            vecLayer->setStrokes(m_postStrokes);
        }
    } else if (m_postTiles.has_value()) {
        layer->tileManager() = m_postTiles->clone();
        layer->invalidateContentBounds();
    }

    if (m_hasSelection) {
        m_doc->setSelection(m_newSel);
    }

    m_doc->setActiveLayerIndex(m_layerIndex);
    if (m_dirtyRect.isValid()) {
        m_doc->composite(m_dirtyRect);
    } else {
        m_doc->composite();
    }
}

// ----------------------------------------------------------------------------
// MultiLayerTransformCommand
// ----------------------------------------------------------------------------
MultiLayerTransformCommand::MultiLayerTransformCommand(Document* doc,
                                                       std::vector<RasterEntry> rasterEntries,
                                                       std::vector<VectorEntry> vectorEntries,
                                                       bool hasSelection,
                                                       const QPainterPath& oldSel,
                                                       const QPainterPath& newSel,
                                                       const QString& text,
                                                       QUndoCommand* parent)
    : QUndoCommand(text, parent)
    , m_doc(doc)
    , m_rasterEntries(std::move(rasterEntries))
    , m_vectorEntries(std::move(vectorEntries))
    , m_hasSelection(hasSelection)
    , m_oldSel(oldSel)
    , m_newSel(newSel)
{
}

void MultiLayerTransformCommand::undo() {
    if (!m_doc) return;
    QRect dirtyRect;

    for (const auto& entry : m_rasterEntries) {
        auto layer = m_doc->getLayer(entry.layerIndex);
        if (layer) {
            layer->tileManager() = entry.preTiles.clone();
            layer->invalidateContentBounds();
            if (entry.dirtyRect.isValid()) {
                dirtyRect = dirtyRect.united(entry.dirtyRect);
            }
        }
    }

    for (const auto& entry : m_vectorEntries) {
        auto layer = m_doc->getLayer(entry.layerIndex);
        auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(layer);
        if (vecLayer) {
            vecLayer->setStrokes(entry.preStrokes);
        }
    }

    if (m_hasSelection) {
        m_doc->setSelection(m_oldSel);
    }

    if (dirtyRect.isValid()) {
        m_doc->composite(dirtyRect);
    } else {
        m_doc->composite();
    }
}

void MultiLayerTransformCommand::redo() {
    if (m_firstRun) {
        m_firstRun = false;
        return;
    }
    if (!m_doc) return;
    QRect dirtyRect;

    for (const auto& entry : m_rasterEntries) {
        auto layer = m_doc->getLayer(entry.layerIndex);
        if (layer) {
            layer->tileManager() = entry.postTiles.clone();
            layer->invalidateContentBounds();
            if (entry.dirtyRect.isValid()) {
                dirtyRect = dirtyRect.united(entry.dirtyRect);
            }
        }
    }

    for (const auto& entry : m_vectorEntries) {
        auto layer = m_doc->getLayer(entry.layerIndex);
        auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(layer);
        if (vecLayer) {
            vecLayer->setStrokes(entry.postStrokes);
        }
    }

    if (m_hasSelection) {
        m_doc->setSelection(m_newSel);
    }

    if (dirtyRect.isValid()) {
        m_doc->composite(dirtyRect);
    } else {
        m_doc->composite();
    }
}

MultiLayerFilterCommand::MultiLayerFilterCommand(Document* doc, const QString& filterName, std::vector<Entry> entries, QUndoCommand* parent)

    : QUndoCommand(filterName, parent)
    , m_doc(doc)
    , m_entries(std::move(entries))
{
}

void MultiLayerFilterCommand::undo() {
    if (!m_doc) return;
    for (const auto& entry : m_entries) {
        auto layer = m_doc->getLayer(entry.layerIndex);
        if (layer) {
            layer->tileManager() = entry.preTiles.clone();
            layer->invalidateContentBounds();
        }
    }
    m_doc->composite();
}

void MultiLayerFilterCommand::redo() {
    if (m_firstRun) {
        m_firstRun = false;
        return;
    }
    if (!m_doc) return;
    for (const auto& entry : m_entries) {
        auto layer = m_doc->getLayer(entry.layerIndex);
        if (layer) {
            layer->tileManager() = entry.postTiles.clone();
            layer->invalidateContentBounds();
        }
    }
    m_doc->composite();
}

CanvasTransformCommand::CanvasTransformCommand(Document* doc,
                                               int oldW, int oldH,
                                               int newW, int newH,
                                               std::vector<CanvasLayerSnapshot> oldSnapshots,
                                               std::vector<CanvasLayerSnapshot> newSnapshots,
                                               const QPainterPath& oldSel,
                                               const QPainterPath& newSel,
                                               const QString& text,
                                               QUndoCommand* parent)
    : QUndoCommand(text, parent)
    , m_doc(doc)
    , m_oldW(oldW), m_oldH(oldH)
    , m_newW(newW), m_newH(newH)
    , m_oldSnapshots(std::move(oldSnapshots))
    , m_newSnapshots(std::move(newSnapshots))
    , m_oldSel(oldSel), m_newSel(newSel)
{
}

void CanvasTransformCommand::undo() {
    if (!m_doc) return;
    m_doc->applyCanvasTransform(m_oldW, m_oldH, m_oldSnapshots, m_oldSel);
}

void CanvasTransformCommand::redo() {
    if (m_firstRun) {
        m_firstRun = false;
        return;
    }
    if (!m_doc) return;
    m_doc->applyCanvasTransform(m_newW, m_newH, m_newSnapshots, m_newSel);
}

// ----------------------------------------------------------------------------
// ModifyPanelPolygonsCommand
// ----------------------------------------------------------------------------
ModifyPanelPolygonsCommand::ModifyPanelPolygonsCommand(Document* doc,
                                                       std::vector<PanelPolygonSnapshot> snapshots,
                                                       const QString& text,
                                                       QUndoCommand* parent)
    : QUndoCommand(text, parent)
    , m_doc(doc)
    , m_snapshots(std::move(snapshots))
{
}

void ModifyPanelPolygonsCommand::undo() {
    if (!m_doc) return;
    for (const auto& s : m_snapshots) {
        if (s.panel) {
            s.panel->setPanelPolygon(s.oldPoly);
        }
    }
    m_doc->composite();
}

void ModifyPanelPolygonsCommand::redo() {
    if (m_firstRun) {
        m_firstRun = false;
        return;
    }
    if (!m_doc) return;
    for (const auto& s : m_snapshots) {
        if (s.panel) {
            s.panel->setPanelPolygon(s.newPoly);
        }
    }
    m_doc->composite();
}

} // namespace Kaburagi
