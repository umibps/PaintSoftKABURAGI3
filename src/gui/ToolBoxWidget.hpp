#pragma once

#include <QWidget>
#include <QButtonGroup>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QKeySequence>
#include <QVariantMap>
#include <QSettings>
#include <vector>
#include "ColorBoxWidget.hpp"
#include "Layer.hpp"

class QScrollArea;

namespace Kaburagi {

enum class ToolType {
    // 描画ツール (通常レイヤー)
    Pen,
    Airbrush,
    Watercolor,
    Marker,
    Eraser,
    Bucket,
    PatternFill,
    Blur,
    Blend,
    Gradient,
    CompositeBrush,

    // ベクトルツール (ベクトルレイヤー)
    VectorControlPoint,
    VectorPressure,
    VectorLineEdit,
    VectorColorChange,
    VectorPolyline,
    VectorCurve,
    VectorEllipse,
    VectorCircle,
    VectorPolygon,

    // テキストツール (通常レイヤー / テキストレイヤー)
    Text,
    TextBalloon,

    // コマ割りツール
    PanelFrame,
    PanelSplit,

    // 3Dツール (3Dレイヤー)
    ThreeD,

    // 調整ツール (調整レイヤー)
    AdjustmentBrightnessContrast,
    AdjustmentHueSaturation,
    AdjustmentToneCurve,
    AdjustmentLevels,
    AdjustmentInvert,
    AdjustmentGrayscale,

    // 共通ツール (全レイヤー)
    Pipette,
    RectSelect,
    EllipseSelect,
    LassoSelect,
    MagicWand,
    ColorSelect,
    Transform
};

struct ToolSlotItem {
    bool isEmpty = true;
    QString name;                       // ブラシの名前 (例: "鉛筆")
    ToolType toolType = ToolType::Pen;  // ブラシのタイプ
    ToolType iconType = ToolType::Pen;  // ブラシのアイコン
    QKeySequence shortcut;              // ショートカットキー
    QVariantMap customSettings;         // 複製時の設定保持用
};

/**
 * @brief SAI風のテキスト＋アイコン表示ツールボタン
 */
class SaiToolButton : public QPushButton {
    Q_OBJECT
public:
    SaiToolButton(ToolType tool, const QString& label = QString(), bool iconOnly = false, QWidget* parent = nullptr);
    SaiToolButton(int slotIndex, const ToolSlotItem& item, QWidget* parent = nullptr);

    ToolType tool() const { return m_item.toolType; }
    ToolType iconType() const { return m_item.iconType; }
    const ToolSlotItem& slotItem() const { return m_item; }
    void setSlotItem(const ToolSlotItem& item);

    bool isEmpty() const { return m_item.isEmpty; }
    int slotIndex() const { return m_slotIndex; }

signals:
    void settingsRequested(int slotIndex);
    void duplicateRequested(int slotIndex);
    void deleteRequested(int slotIndex);

protected:
    void paintEvent(QPaintEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    int m_slotIndex = -1;
    ToolSlotItem m_item;
    bool m_iconOnly = false;
};

class ToolBoxWidget : public QWidget {
    Q_OBJECT
public:
    static constexpr int GRID_COLS = 4;
    static constexpr int GRID_ROWS = 16;
    static constexpr int TOTAL_SLOTS = GRID_COLS * GRID_ROWS; // 64

    explicit ToolBoxWidget(QWidget* parent = nullptr);
    ~ToolBoxWidget() override = default;

    ToolType currentTool() const { return m_currentTool; }
    void setCurrentTool(ToolType tool);

    int currentSlotIndex() const { return m_currentSlotIndex; }
    void setCurrentSlot(int slotIndex);

    void setLayerType(LayerType type);
    LayerType currentLayerType() const { return m_layerType; }

    ColorBoxWidget* colorBox() const { return m_colorBox; }

    const std::vector<ToolSlotItem>& rasterSlots() const { return m_rasterSlots; }
    int totalSlotCount() const { return TOTAL_SLOTS; }
    const ToolSlotItem& slotItem(int slotIndex) const { return m_rasterSlots.at(slotIndex); }
    void setSlotItem(int slotIndex, const ToolSlotItem& item);

    void duplicateSlot(int slotIndex) { onSlotDuplicateRequested(slotIndex); }
    void deleteSlot(int slotIndex) { onSlotDeleteRequested(slotIndex); }

    bool handleShortcutKey(const QKeySequence& seq);

    void saveSlots(QSettings& settings) const;
    void loadSlots(QSettings& settings);

signals:
    void toolChanged(ToolType tool);
    void slotSelected(int slotIndex, ToolType tool);

private slots:
    void onSlotSettingsRequested(int slotIndex);
    void onSlotDuplicateRequested(int slotIndex);
    void onSlotDeleteRequested(int slotIndex);

private:
    void setupCommonTools();
    void initDefaultSlots();
    void updateToolTableForLayer(LayerType type);
    void addToolToTable(ToolType tool, const QString& label, int row, int col);

    ToolType m_currentTool = ToolType::Pen;
    int m_currentSlotIndex = 0;
    LayerType m_layerType = LayerType::Raster;

    ColorBoxWidget* m_colorBox = nullptr;
    QButtonGroup* m_buttonGroup = nullptr;

    QWidget* m_commonToolsContainer = nullptr;
    QGridLayout* m_commonToolsLayout = nullptr;

    QWidget* m_layerToolsContainer = nullptr;
    QGridLayout* m_layerToolsLayout = nullptr;
    QScrollArea* m_layerToolsScrollArea = nullptr;
    QLabel* m_groupLayerLabel = nullptr;

    std::vector<ToolSlotItem> m_rasterSlots;
    std::vector<SaiToolButton*> m_slotButtons;
};

} // namespace Kaburagi
