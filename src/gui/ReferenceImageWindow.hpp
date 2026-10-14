#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QScrollArea>
#include <QWidget>
#include <QImage>
#include <QColor>
#include <QLabel>
#include <QStackedWidget>

namespace Kaburagi {

class SpinScale;

class ReferenceImageView : public QScrollArea {
    Q_OBJECT
public:
    explicit ReferenceImageView(const QImage& image, QWidget* parent = nullptr);
    ~ReferenceImageView() override = default;

    const QImage& image() const { return m_image; }
    double zoom() const { return m_zoom; }
    void setZoom(double zoom, const QPoint& anchorViewPos = QPoint(-1, -1));

signals:
    void colorPicked(const QColor& color);
    void addToPalette(const QColor& color);
    void closeRequested();
    void zoomChanged(double zoom);

protected:
    void wheelEvent(QWheelEvent* event) override;

private:
    class CanvasWidget : public QWidget {
    public:
        explicit CanvasWidget(ReferenceImageView* view);
    protected:
        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
    private:
        ReferenceImageView* m_view = nullptr;
        bool m_isPanning = false;
        QPoint m_panStartPos;
    };

    friend class CanvasWidget;

    QImage m_image;
    double m_zoom = 1.0;
    CanvasWidget* m_canvas = nullptr;
};

class ReferenceImageWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit ReferenceImageWindow(QWidget* parent = nullptr);
    ~ReferenceImageWindow() override = default;

    void openImageFile(const QString& path);
    void addImage(const QImage& image, const QString& title);
    int tabCount() const;
    QImage currentImage() const;

signals:
    void colorPicked(const QColor& color);
    void addToPalette(const QColor& color);
    void windowClosed();

public slots:
    void openImage();
    void pasteImage();
    void closeTab(int index);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onTabChanged(int index);
    void onZoomScaleChanged(double val);
    void onChildViewZoomChanged(double zoom);

private:
    void setupUi();
    void updatePlaceholder();
    ReferenceImageView* currentView() const;

    QStackedWidget* m_centralStack = nullptr;
    QTabWidget* m_tabWidget = nullptr;
    QLabel* m_placeholderLabel = nullptr;
    SpinScale* m_zoomScale = nullptr;
    int m_pasteCounter = 1;
    bool m_updatingZoomScale = false;
};

} // namespace Kaburagi
