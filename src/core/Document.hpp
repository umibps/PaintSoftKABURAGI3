#pragma once

#include "Layer.hpp"
#include "VectorLayer.hpp"
#include "AdjustmentLayer.hpp"
#include "TextLayer.hpp"
#include "GroupLayer.hpp"
#include "ThreeDLayer.hpp"
#include "Color.hpp"
#include "UndoCommands.hpp"
#include <QObject>
#include <QUndoStack>
#include <QPainterPath>
#include <vector>
#include <memory>

namespace Kaburagi {

class Document : public QObject {
    Q_OBJECT
public:
    Document(int width = 1600, int height = 1200, double dpi = 350.0, bool createDefaultLayers = true,
             const QColor& initialBgColor = Qt::white, QObject* parent = nullptr);
    ~Document() override = default;

    static Document* fromQImage(const QImage& img, double dpi = 350.0, QObject* parent = nullptr);

    int width() const { return m_width; }
    int height() const { return m_height; }
    double dpi() const { return m_dpi; }

    const QColor& primaryBackgroundColor() const { return m_primaryBgColor; }
    void setPrimaryBackgroundColor(const QColor& color);

    const QColor& secondaryBackgroundColor() const { return m_secondaryBgColor; }
    void setSecondaryBackgroundColor(const QColor& color);

    bool isSecondaryBgColorActive() const { return m_isSecondaryBgColorActive; }
    void setBackgroundColorActive(bool secondary);
    void switchBackgroundColor();

    LayerPtr backgroundLayer() const;
    void updateBackgroundLayerPixels();

    // レイヤー操作
    const std::vector<LayerPtr>& layers() const { return m_layers; }
    int activeLayerIndex() const { return m_activeLayerIndex; }
    LayerPtr activeLayer() const;
    LayerPtr getLayer(int index) const;
    void setActiveLayerIndex(int index);

    void clearAllLayers();
    void addExistingLayer(LayerPtr layer);

    LayerPtr addRasterLayer(const QString& name = QString());
    LayerPtr addRasterLayerWithImage(const QImage& img, const QString& name = QString(), const QPoint& offset = QPoint(0, 0), const QJsonObject& aiParams = QJsonObject());
    std::shared_ptr<VectorLayer> addVectorLayer(const QString& name = QString());
    std::shared_ptr<AdjustmentLayer> addAdjustmentLayer(AdjustmentType type, const QString& name = QString());
    std::shared_ptr<TextLayer> addTextLayer(const QString& name = QString());
    std::shared_ptr<GroupLayer> addGroupLayer(const QString& name = QString());
    std::shared_ptr<ThreeDLayer> addThreeDLayer(const QString& name = QString());
    std::shared_ptr<GroupLayer> addPanelFolder(const QPolygonF& polygon, float borderWidth = 3.0f, const QColor& borderColor = Qt::black, const QString& name = QString());
    struct PanelCreationSpec {
        QPolygonF polygon;
        float borderWidth = 3.0f;
        QColor borderColor = Qt::black;
        bool drawBorder = true;
        QString name;
    };
    std::vector<std::shared_ptr<GroupLayer>> addPanelFolders(const std::vector<PanelCreationSpec>& specs);
    bool splitPanelFolder(const std::shared_ptr<GroupLayer>& panel, const QLineF& cutLine, float marginX = 20.0f, float marginY = 40.0f);
    bool splitPanelFolderEqually(const std::shared_ptr<GroupLayer>& panel, Qt::Orientation orientation, int count = 2, float margin = 20.0f);
    std::shared_ptr<GroupLayer> findPanelFolderAt(const QPointF& docPt) const;

    struct AdjacentPanelInfo {
        std::shared_ptr<GroupLayer> panel;
        QString direction; // "上", "下", "左", "右", "隣接"
        float distance = 0.0f;
    };
    std::vector<AdjacentPanelInfo> findAdjacentPanels(const std::shared_ptr<GroupLayer>& panel, float maxDistance = 100.0f) const;
    bool mergePanelFolders(const std::shared_ptr<GroupLayer>& panelA, const std::shared_ptr<GroupLayer>& panelB, float maxGap = 80.0f);
    void setLayerParentGroup(int layerIndex, const std::shared_ptr<GroupLayer>& group);
    void insertLayer(int index, LayerPtr layer);
    void removeLayer(int index);
    void moveLayer(int fromIndex, int toIndex);
    void replaceLayer(int index, LayerPtr layer);
    LayerPtr duplicateLayer(int index);
    LayerPtr newLayerFromVisible(const QString& name = QString());
    bool mergeDown(int index);
    LayerPtr rasterizeLayer(int index);
    QPainterPath createOpaquePath(int layerIndex) const;
    void fillLayer(int index, const QColor& color);
    void convertBrightnessToOpacity(int index, const QColor& lineColor = Qt::black);

    // クリッピング判定元レイヤー (ベースレイヤー) の取得
    LayerPtr findClippingBaseLayer(int layerIndex) const;

    // アンドゥ/リドゥ管理
    QUndoStack* undoStack() { return m_undoStack; }
    const QUndoStack* undoStack() const { return m_undoStack; }
    bool isUndoRedoInProgress() const { return m_isUndoRedoInProgress; }
    void setUndoRedoInProgress(bool inProgress) { m_isUndoRedoInProgress = inProgress; }

    // コンポジット (全レイヤー合成)
    const TileManager& compositeBuffer() const { return m_compositeBuffer; }
    void composite(const QRect& dirtyRect = QRect());
    QColor samplePixel(int x, int y) const;

    // 選択範囲管理 (ピクセル単位 0〜255 グレースケールマスク)
    const QPainterPath& selection() const { return m_selectionPath; }
    bool hasSelection() const { return m_hasSelection; }
    void setSelection(const QPainterPath& path);
    void setSelectionMask(const QImage& mask);
    void clearSelection();
    uint8_t selectionStrength(int x, int y) const;
    bool isPixelSelected(int x, int y) const { return selectionStrength(x, y) > 0; }
    const QRectF& selectionBounds() const { return m_selectionBounds; }
    void ensureSelectionMaskLayer();

    // 選択範囲編集モード (クイックマスク)
    bool isSelectionEditMode() const { return m_isSelectionEditMode; }
    void setSelectionEditMode(bool enabled);
    LayerPtr selectionMaskLayer() const { return m_selectionMaskLayer; }
    void updateSelectionPathFromMask();
    void updateSelectionMaskFromPath();
    static QPainterPath maskToPath(const uint8_t* mask, int bw, int bh, int offsetX = 0, int offsetY = 0);

    // カラーモード管理 (RGB, CMYKプルーフ, CMYKネイティブ)
    CanvasColorMode colorMode() const { return m_colorMode; }
    void setColorMode(CanvasColorMode mode);

    // キャンバス変形 (サイズ変更 / 拡大縮小 / 回転 / 反転)
    enum class CanvasTransformType {
        Rotate90CW,
        Rotate90CCW,
        Rotate180,
        FlipHorizontal,
        FlipVertical
    };

    std::vector<CanvasLayerSnapshot> snapshotAllLayers() const;
    void applyCanvasTransform(int w, int h, const std::vector<CanvasLayerSnapshot>& snapshots, const QPainterPath& sel);

    void changeCanvasSize(int newWidth, int newHeight, int offsetX, int offsetY);
    void scaleCanvas(int newWidth, int newHeight, Qt::TransformationMode mode = Qt::SmoothTransformation);
    void transformCanvas(CanvasTransformType type);
    void rotateCanvas90CW() { transformCanvas(CanvasTransformType::Rotate90CW); }
    void rotateCanvas90CCW() { transformCanvas(CanvasTransformType::Rotate90CCW); }
    void rotateCanvas180() { transformCanvas(CanvasTransformType::Rotate180); }
    void flipCanvasHorizontal() { transformCanvas(CanvasTransformType::FlipHorizontal); }
    void flipCanvasVertical() { transformCanvas(CanvasTransformType::FlipVertical); }

    bool isClipToHiddenBaseLayerEnabled() const { return m_clipToHiddenBaseLayer; }
    void setClipToHiddenBaseLayer(bool enabled);

    // キャンバス更新シグナル
signals:
    void layerListChanged();
    void activeLayerChanged(int index);
    void documentUpdated(const QRect& dirtyRect);
    void selectionChanged();
    void selectionEditModeChanged(bool enabled);
    void backgroundColorChanged();
    void colorModeChanged(CanvasColorMode mode);
    void canvasResized(int width, int height);
    void clipToHiddenBaseLayerChanged(bool enabled);

private:
    int m_width;
    int m_height;
    double m_dpi;
    CanvasColorMode m_colorMode = CanvasColorMode::RGB;
    bool m_clipToHiddenBaseLayer = true;
    QColor m_primaryBgColor = Qt::white;
    QColor m_secondaryBgColor = Qt::white;
    bool m_isSecondaryBgColorActive = false;

    std::vector<LayerPtr> m_layers;
    int m_activeLayerIndex = 0;
    TileManager m_compositeBuffer;
    bool m_hasSelection = false;
    QPainterPath m_selectionPath;
    QRectF m_selectionBounds;
    bool m_selectionIsRect = false;

    QUndoStack* m_undoStack = nullptr;
    bool m_isUndoRedoInProgress = false;

    bool m_isSelectionEditMode = false;
    mutable LayerPtr m_selectionMaskLayer;

    bool splitPanelFolderInternal(const std::shared_ptr<GroupLayer>& panel, const QLineF& cutLine,
                                  float marginX, float marginY,
                                  std::shared_ptr<GroupLayer>& outNewPanel,
                                  std::shared_ptr<Layer>& outNewInnerLayer,
                                  int& outInsertIndex);
};

} // namespace Kaburagi
