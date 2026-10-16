#pragma once

#include <QUndoCommand>
#include <QRect>
#include <QPainterPath>
#include <memory>
#include <optional>
#include "TileManager.hpp"
#include "VectorLayer.hpp"
#include "TextLayer.hpp"
#include "GroupLayer.hpp"

namespace Kaburagi {

class Document;

/**
 * @brief ラスターブラシ描画の元に戻す/やり直しコマンド (CoW タイルスナップショット)
 */
class DrawStrokeCommand : public QUndoCommand {
public:
    DrawStrokeCommand(Document* doc, int layerIndex, TileManager preTiles, TileManager postTiles, const QRect& dirtyRect, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_layerIndex;
    TileManager m_preTiles;
    TileManager m_postTiles;
    QRect m_dirtyRect;
    bool m_firstRun = true;
};

/**
 * @brief 選択範囲編集モード (クイックマスク) ストロークの元に戻す/やり直しコマンド
 */
class SelectionMaskStrokeCommand : public QUndoCommand {
public:
    SelectionMaskStrokeCommand(Document* doc, TileManager preTiles, TileManager postTiles, const QRect& dirtyRect, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    TileManager m_preTiles;
    TileManager m_postTiles;
    QRect m_dirtyRect;
    bool m_firstRun = true;
};

/**
 * @brief バケツ塗りつぶしの元に戻す/やり直しコマンド
 */
class FillBucketCommand : public QUndoCommand {
public:
    FillBucketCommand(Document* doc, int layerIndex, TileManager preTiles, TileManager postTiles, const QRect& dirtyRect, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_layerIndex;
    TileManager m_preTiles;
    TileManager m_postTiles;
    QRect m_dirtyRect;
    bool m_firstRun = true;
};

/**
 * @brief ベクトルレイヤーへのストローク追加コマンド
 */
class AddVectorStrokeCommand : public QUndoCommand {
public:
    AddVectorStrokeCommand(Document* doc, int layerIndex, const VectorStroke& stroke, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_layerIndex;
    VectorStroke m_stroke;
    bool m_firstRun = true;
};

/**
 * @brief ベクトルレイヤーのストローク編集 (消去・変更) コマンド
 */
class ModifyVectorStrokesCommand : public QUndoCommand {
public:
    ModifyVectorStrokesCommand(Document* doc, int layerIndex, const std::vector<VectorStroke>& beforeStrokes, const std::vector<VectorStroke>& afterStrokes, const QString& text = "ベクター消去", QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_layerIndex;
    std::vector<VectorStroke> m_beforeStrokes;
    std::vector<VectorStroke> m_afterStrokes;
    bool m_firstRun = true;
};

/**
 * @brief レイヤー追加コマンド
 */
class AddLayerCommand : public QUndoCommand {
public:
    AddLayerCommand(Document* doc, LayerPtr layer, int index, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    LayerPtr m_layer;
    int m_index;
    bool m_firstRun = true;
};

struct PanelFolderEntry {
    std::shared_ptr<GroupLayer> panel;
    std::shared_ptr<Layer> innerLayer;
};

/**
 * @brief コマフォルダ一括追加コマンド
 */
class AddPanelFoldersCommand : public QUndoCommand {
public:
    AddPanelFoldersCommand(Document* doc, int baseIndex, const std::vector<PanelFolderEntry>& entries, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_baseIndex;
    std::vector<PanelFolderEntry> m_entries;
    bool m_firstRun = true;
};

/**
 * @brief レイヤー削除コマンド
 */
class RemoveLayerCommand : public QUndoCommand {
public:
    RemoveLayerCommand(Document* doc, LayerPtr layer, int index, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    LayerPtr m_layer;
    int m_index;
    bool m_firstRun = true;
};

/**
 * @brief レイヤー順序変更コマンド
 */
class MoveLayerCommand : public QUndoCommand {
public:
    MoveLayerCommand(Document* doc, int fromIndex, int toIndex, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_fromIndex;
    int m_toIndex;
    bool m_firstRun = true;
};

/**
 * @brief 吹き出し・テキスト追加コマンド
 */
class AddTextBalloonCommand : public QUndoCommand {
public:
    AddTextBalloonCommand(Document* doc, int layerIndex, const TextBalloonItem& item, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_layerIndex;
    TextBalloonItem m_item;
    bool m_firstRun = true;
};

/**
 * @brief 吹き出し・テキスト編集コマンド
 */
class EditTextBalloonCommand : public QUndoCommand {
public:
    EditTextBalloonCommand(Document* doc, int layerIndex, int itemIndex, const TextBalloonItem& oldItem, const TextBalloonItem& newItem, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_layerIndex;
    int m_itemIndex;
    TextBalloonItem m_oldItem;
    TextBalloonItem m_newItem;
};

/**
 * @brief レイヤー/グループ平行移動コマンド
 */
class TranslateLayerCommand : public QUndoCommand {
public:
    TranslateLayerCommand(Document* doc, int layerIndex, int dx, int dy, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_layerIndex;
    int m_dx;
    int m_dy;
    bool m_firstRun = true;
};

/**
 * @brief 選択範囲変更コマンド
 */
class SelectCommand : public QUndoCommand {
public:
    SelectCommand(Document* doc, const QPainterPath& oldPath, const QPainterPath& newPath, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    QPainterPath m_oldPath;
    QPainterPath m_newPath;
};

/**
 * @brief レイヤー結合 (下のレイヤーと結合) コマンド
 */
class MergeLayersCommand : public QUndoCommand {
public:
    MergeLayersCommand(Document* doc, int upperIndex, LayerPtr upperLayer, int lowerIndex, LayerPtr preLowerLayer, LayerPtr postLowerLayer, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_upperIndex;
    LayerPtr m_upperLayer;
    int m_lowerIndex;
    LayerPtr m_preLowerLayer;
    LayerPtr m_postLowerLayer;
    bool m_firstRun = true;
};

/**
 * @brief レイヤー置換 (ラスタライズ等) コマンド
 */
class ReplaceLayerCommand : public QUndoCommand {
public:
    ReplaceLayerCommand(Document* doc, int index, LayerPtr oldLayer, LayerPtr newLayer, const QString& text = "レイヤーをラスタライズ", QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_index;
    LayerPtr m_oldLayer;
    LayerPtr m_newLayer;
    bool m_firstRun = true;
};

/**
 * @brief レイヤー変形 (ラスター/ベクター・選択範囲連動) コマンド
 */
class TransformLayerCommand : public QUndoCommand {
public:
    // ラスターレイヤー変形用
    TransformLayerCommand(Document* doc, int layerIndex,
                          TileManager preTiles, TileManager postTiles, const QRect& dirtyRect,
                          bool hasSelection = false,
                          const QPainterPath& oldSel = QPainterPath(),
                          const QPainterPath& newSel = QPainterPath(),
                          const QString& text = "レイヤー変形",
                          QUndoCommand* parent = nullptr);

    // ベクターレイヤー変形用
    TransformLayerCommand(Document* doc, int layerIndex,
                          const std::vector<VectorStroke>& preStrokes,
                          const std::vector<VectorStroke>& postStrokes,
                          bool hasSelection = false,
                          const QPainterPath& oldSel = QPainterPath(),
                          const QPainterPath& newSel = QPainterPath(),
                          const QString& text = "レイヤー変形",
                          QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_layerIndex;
    bool m_isVector = false;

    // ラスター
    std::optional<TileManager> m_preTiles;
    std::optional<TileManager> m_postTiles;
    QRect m_dirtyRect;

    // ベクター
    std::vector<VectorStroke> m_preStrokes;
    std::vector<VectorStroke> m_postStrokes;

    // 選択範囲連動
    bool m_hasSelection = false;
    QPainterPath m_oldSel;
    QPainterPath m_newSel;

    bool m_firstRun = true;
};

/**
 * @brief 複数レイヤー同時変形用Undo/Redoコマンド (ピン留めレイヤー連動対応)
 */
class MultiLayerTransformCommand : public QUndoCommand {
public:
    struct RasterEntry {
        int layerIndex;
        TileManager preTiles;
        TileManager postTiles;
        QRect dirtyRect;
    };
    struct VectorEntry {
        int layerIndex;
        std::vector<VectorStroke> preStrokes;
        std::vector<VectorStroke> postStrokes;
    };

    MultiLayerTransformCommand(Document* doc,
                               std::vector<RasterEntry> rasterEntries,
                               std::vector<VectorEntry> vectorEntries,
                               bool hasSelection = false,
                               const QPainterPath& oldSel = QPainterPath(),
                               const QPainterPath& newSel = QPainterPath(),
                               const QString& text = "レイヤー変形",
                               QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    std::vector<RasterEntry> m_rasterEntries;
    std::vector<VectorEntry> m_vectorEntries;
    bool m_hasSelection = false;
    QPainterPath m_oldSel;
    QPainterPath m_newSel;
    bool m_firstRun = true;
};


/**
 * @brief 複数レイヤー (アクティブ + ピン留め) へのフィルター一括適用コマンド
 */
class MultiLayerFilterCommand : public QUndoCommand {
public:
    struct Entry {
        int layerIndex;
        TileManager preTiles;
        TileManager postTiles;
        QRect dirtyRect;
    };

    MultiLayerFilterCommand(Document* doc, const QString& filterName, std::vector<Entry> entries, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    std::vector<Entry> m_entries;
    bool m_firstRun = true;
};

struct CanvasLayerSnapshot {
    TileManager tiles;
    std::vector<VectorStroke> vectorStrokes;
    std::vector<TextBalloonItem> textItems;
    QPolygonF panelPolygon;
};

/**
 * @brief キャンバス変形 (サイズ変更 / 拡大縮小 / 回転 / 反転) の元に戻す/やり直しコマンド
 */
class CanvasTransformCommand : public QUndoCommand {
public:
    CanvasTransformCommand(Document* doc,
                           int oldW, int oldH,
                           int newW, int newH,
                           std::vector<CanvasLayerSnapshot> oldSnapshots,
                           std::vector<CanvasLayerSnapshot> newSnapshots,
                           const QPainterPath& oldSel,
                           const QPainterPath& newSel,
                           const QString& text = "キャンバス変形",
                           QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_oldW;
    int m_oldH;
    int m_newW;
    int m_newH;
    std::vector<CanvasLayerSnapshot> m_oldSnapshots;
    std::vector<CanvasLayerSnapshot> m_newSnapshots;
    QPainterPath m_oldSel;
    QPainterPath m_newSel;
    bool m_firstRun = true;
};

} // namespace Kaburagi
