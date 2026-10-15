#pragma once

#include <QWidget>
#include <QComboBox>
#include <QSlider>
#include <QCheckBox>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QDropEvent>
#include <QDragMoveEvent>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include "Document.hpp"
#include "SpinScaleWidget.hpp"

namespace Kaburagi {

class LayerItemDelegate;
class LayerPanelWidget;

class LayerListWidget : public QListWidget {
    Q_OBJECT
public:
    explicit LayerListWidget(QWidget* parent = nullptr);
    void setPanel(LayerPanelWidget* panel) { m_panel = panel; }
    int hoverEyeRow() const { return m_hoverEyeRow; }
    int hoverPinRow() const { return m_hoverPinRow; }
    void setScrollBarPosition(bool onLeft);
    bool isScrollBarOnLeft() const;

signals:
    void layerReordered(int fromRow, int toRow);

protected:
    void startDrag(Qt::DropActions supportedActions) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    int getBackgroundRow() const;

    LayerPanelWidget* m_panel = nullptr;
    int m_dragStartRow = -1;
    int m_dropSlot = -1;
    int m_hoverEyeRow = -1;
    int m_hoverPinRow = -1;
};

class LayerPanelWidget : public QWidget {
    Q_OBJECT
    friend class LayerItemDelegate;
    friend class LayerListWidget;
public:
    explicit LayerPanelWidget(QWidget* parent = nullptr);
    ~LayerPanelWidget() override = default;

    void setDocument(Document* doc);
    Document* document() const { return m_document; }

    void updateScrollBarPosition();
    void setScrollBarPosition(bool onLeft);
    QMenu* createContextMenu(const QPoint& pos);
    void scrollToLayer(int index, bool center = true);

signals:
    void loadAiParametersRequested(const std::shared_ptr<Layer>& layer);

public slots:
    void onLayerListChanged();

private slots:
    void onLayerReordered(int fromRow, int toRow);
    void onActiveLayerChanged(int index);
    void onBlendModeChanged(int index);
    void onOpacityScaleChanged(double value);
    void onClippingToggled(bool checked);
    void onAlphaLockToggled(bool checked);

    void onAddRasterLayer();
    void onAddVectorLayer();
    void onAddTextLayer();
    void onAddGroupLayer();
    void onAddPanelFolder();
    void onAddThreeDLayer();
    void onAddAdjustmentLayer();
    void onMergeDownLayer();
    void onDeleteLayer();
    void updateThumbnails();

private:
    void updateUIFromActiveLayer();

    Document* m_document = nullptr;
    QTimer* m_thumbnailUpdateTimer = nullptr;

    QComboBox* m_blendCombo = nullptr;
    SpinScale* m_opacityScale = nullptr;
    QCheckBox* m_clipCheck = nullptr;
    QCheckBox* m_alphaLockCheck = nullptr;

    LayerListWidget* m_layerListWidget = nullptr;

    QPushButton* m_btnNewRaster = nullptr;
    QPushButton* m_btnNewVector = nullptr;
    QPushButton* m_btnNewGroup = nullptr;
    QPushButton* m_btnNewPanel = nullptr;
    QPushButton* m_btnNewThreeD = nullptr;
    QPushButton* m_btnNewAdj = nullptr;
    QPushButton* m_btnMergeDown = nullptr;
    QPushButton* m_btnDelete = nullptr;
};

} // namespace Kaburagi
