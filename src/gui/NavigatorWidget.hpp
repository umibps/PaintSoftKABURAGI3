#pragma once

#include <QWidget>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include "Document.hpp"

#include "SpinScaleWidget.hpp"

namespace Kaburagi {

class NavPreviewWidget;

class NavigatorWidget : public QWidget {
    Q_OBJECT
    friend class NavPreviewWidget;
public:
    explicit NavigatorWidget(QWidget* parent = nullptr);
    ~NavigatorWidget() override = default;

    void setDocument(Document* doc);
    void updateViewTransform(double zoom, double angleDeg, bool flipped, const QPointF& docCenterVisible, const QSizeF& viewportSize);

    QWidget* previewWidget() const;

signals:
    void zoomChanged(double zoom);
    void rotationChanged(double angleDeg);
    void flipHorizontallyTriggered();
    void resetTransformTriggered();
    void viewCenterChanged(const QPointF& docCenter);

protected:
    void resizeEvent(QResizeEvent* event) override;
    QSize sizeHint() const override { return QSize(160, 220); }
    QSize minimumSizeHint() const override { return QSize(140, 180); }

private:
    double getNavScale() const;
    QPointF getNavCenter() const;
    QPointF previewPosToDocPos(const QPointF& viewPt) const;
    void updateDocPreview();

    NavPreviewWidget* m_previewWidget = nullptr;

    Document* m_document = nullptr;
    double m_zoom = 1.0;
    double m_angleDeg = 0.0;
    bool m_flippedHorizontally = false;
    QPointF m_docCenterVisible;
    QSizeF m_viewportSize;

    QImage m_docPreviewImage;
    bool m_previewDirty = true;
    bool m_isDraggingPreview = false;

    SpinScale* m_zoomScale = nullptr;
    QPushButton* m_btnZoom50 = nullptr;
    QPushButton* m_btnZoom100 = nullptr;
    QPushButton* m_btnZoom200 = nullptr;
    SpinScale* m_rotScale = nullptr;

    QPushButton* m_btnZoomIn = nullptr;
    QPushButton* m_btnZoomOut = nullptr;
    QPushButton* m_btnRotCCW = nullptr;
    QPushButton* m_btnRotCW = nullptr;
    QPushButton* m_btnResetRot = nullptr;
    QPushButton* m_btnFlip = nullptr;
};

} // namespace Kaburagi
