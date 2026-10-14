#include "ReferenceImageWindow.hpp"
#include "SpinScaleWidget.hpp"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QClipboard>
#include <QGuiApplication>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QCursor>
#include <QKeySequence>
#include <algorithm>
#include <cmath>

namespace Kaburagi {

// ============================================================================
// ReferenceImageView::CanvasWidget
// ============================================================================

ReferenceImageView::CanvasWidget::CanvasWidget(ReferenceImageView* view)
    : QWidget(view), m_view(view) {
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);
}

void ReferenceImageView::CanvasWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    if (!m_view || m_view->m_image.isNull()) {
        painter.fillRect(rect(), QColor(40, 40, 40));
        return;
    }

    // 背景描画（暗色）
    painter.fillRect(rect(), QColor(40, 40, 40));

    // 画像描画
    double zoom = m_view->m_zoom;
    int scaledW = std::max(1, static_cast<int>(std::round(m_view->m_image.width() * zoom)));
    int scaledH = std::max(1, static_cast<int>(std::round(m_view->m_image.height() * zoom)));
    QRect targetRect(0, 0, scaledW, scaledH);

    // 縮小時のみバイリニア、等倍以上は最近傍（ドットが綺麗に見える）
    if (zoom < 1.0) {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    } else {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    }

    // 市松模様（透過画像対策）
    static QPixmap checker;
    if (checker.isNull()) {
        checker = QPixmap(16, 16);
        QPainter cp(&checker);
        cp.fillRect(0, 0, 8, 8, QColor(220, 220, 220));
        cp.fillRect(8, 8, 8, 8, QColor(220, 220, 220));
        cp.fillRect(8, 0, 8, 8, QColor(255, 255, 255));
        cp.fillRect(0, 8, 8, 8, QColor(255, 255, 255));
    }
    painter.drawTiledPixmap(targetRect, checker);

    painter.drawImage(targetRect, m_view->m_image);

    // 外枠
    painter.setPen(QColor(80, 80, 80));
    painter.drawRect(targetRect.adjusted(0, 0, -1, -1));
}

void ReferenceImageView::CanvasWidget::mousePressEvent(QMouseEvent* event) {
    if (!m_view || m_view->m_image.isNull()) {
        QWidget::mousePressEvent(event);
        return;
    }

    // 中ボタンドラッグまたはスペース押下時ドラッグでパンスクロール
    if (event->button() == Qt::MiddleButton ||
        (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ShiftModifier || event->modifiers() & Qt::AltModifier))) {
        m_isPanning = true;
        m_panStartPos = event->globalPosition().toPoint();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    double zoom = m_view->m_zoom;
    int imgX = static_cast<int>(event->pos().x() / zoom);
    int imgY = static_cast<int>(event->pos().y() / zoom);

    bool inside = (imgX >= 0 && imgX < m_view->m_image.width() &&
                   imgY >= 0 && imgY < m_view->m_image.height());

    QColor pickedColor;
    if (inside) {
        pickedColor = m_view->m_image.pixelColor(imgX, imgY);
    }

    if (event->button() == Qt::LeftButton) {
        if (inside && pickedColor.isValid()) {
            emit m_view->colorPicked(pickedColor);
        }
        event->accept();
    } else if (event->button() == Qt::RightButton) {
        // 右クリックメニューの表示
        QMenu menu(this);
        menu.setStyleSheet("QMenu { font-size: 11px; }");

        QAction* actPalette = menu.addAction(tr("Register to Palette"));
        if (!inside || !pickedColor.isValid()) {
            actPalette->setEnabled(false);
        } else {
            connect(actPalette, &QAction::triggered, this, [this, pickedColor]() {
                emit m_view->addToPalette(pickedColor);
            });
        }

        menu.addSeparator();

        QAction* actClose = menu.addAction(tr("Close Image"));
        connect(actClose, &QAction::triggered, this, [this]() {
            emit m_view->closeRequested();
        });

        menu.exec(event->globalPosition().toPoint());
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void ReferenceImageView::CanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_isPanning) {
        QPoint delta = event->globalPosition().toPoint() - m_panStartPos;
        m_panStartPos = event->globalPosition().toPoint();

        if (m_view->horizontalScrollBar()) {
            m_view->horizontalScrollBar()->setValue(m_view->horizontalScrollBar()->value() - delta.x());
        }
        if (m_view->verticalScrollBar()) {
            m_view->verticalScrollBar()->setValue(m_view->verticalScrollBar()->value() - delta.y());
        }
        event->accept();
        return;
    }

    if (m_view && !m_view->m_image.isNull()) {
        double zoom = m_view->m_zoom;
        int imgX = static_cast<int>(event->pos().x() / zoom);
        int imgY = static_cast<int>(event->pos().y() / zoom);
        if (imgX >= 0 && imgX < m_view->m_image.width() &&
            imgY >= 0 && imgY < m_view->m_image.height()) {
            setCursor(Qt::CrossCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
    } else {
        setCursor(Qt::ArrowCursor);
    }

    QWidget::mouseMoveEvent(event);
}

void ReferenceImageView::CanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (m_isPanning && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

// ============================================================================
// ReferenceImageView
// ============================================================================

ReferenceImageView::ReferenceImageView(const QImage& image, QWidget* parent)
    : QScrollArea(parent), m_image(image) {
    m_canvas = new CanvasWidget(this);
    setWidget(m_canvas);
    setWidgetResizable(false);
    setAlignment(Qt::AlignCenter);
    setStyleSheet("QScrollArea { background-color: #282828; border: none; }");

    setZoom(1.0);
}

void ReferenceImageView::setZoom(double zoom, const QPoint& anchorViewPos) {
    zoom = std::clamp(zoom, 0.1, 16.0);
    if (std::abs(m_zoom - zoom) < 0.001) return;

    double oldZoom = m_zoom;
    m_zoom = zoom;

    int newW = std::max(1, static_cast<int>(std::round(m_image.width() * m_zoom)));
    int newH = std::max(1, static_cast<int>(std::round(m_image.height() * m_zoom)));

    QPoint anchor = anchorViewPos;
    if (anchor.x() < 0 || anchor.y() < 0) {
        anchor = viewport()->rect().center();
    }

    int hVal = horizontalScrollBar()->value();
    int vVal = verticalScrollBar()->value();

    // アンカー位置におけるキャンバス座標
    double oldCanvasX = hVal + anchor.x();
    double oldCanvasY = vVal + anchor.y();

    // 新キャンバス座標
    double ratio = m_zoom / oldZoom;
    double newCanvasX = oldCanvasX * ratio;
    double newCanvasY = oldCanvasY * ratio;

    m_canvas->setFixedSize(newW, newH);
    m_canvas->update();

    horizontalScrollBar()->setValue(static_cast<int>(std::round(newCanvasX - anchor.x())));
    verticalScrollBar()->setValue(static_cast<int>(std::round(newCanvasY - anchor.y())));

    emit zoomChanged(m_zoom);
}

void ReferenceImageView::wheelEvent(QWheelEvent* event) {
    int numDegrees = event->angleDelta().y() / 8;
    int numSteps = numDegrees / 15;
    if (numSteps != 0) {
        double factor = (numSteps > 0) ? 1.15 : (1.0 / 1.15);
        double newZoom = m_zoom * factor;
        setZoom(newZoom, event->position().toPoint());
        event->accept();
    } else {
        QScrollArea::wheelEvent(event);
    }
}

// ============================================================================
// ReferenceImageWindow
// ============================================================================

ReferenceImageWindow::ReferenceImageWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle(tr("Reference Image"));
    resize(500, 450);

    setupUi();
    updatePlaceholder();
}

void ReferenceImageWindow::setupUi() {
    // 1. メニューバー
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QAction* actOpen = fileMenu->addAction(tr("&Open Image..."), this, &ReferenceImageWindow::openImage);
    actOpen->setShortcut(QKeySequence::Open);

    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    QAction* actPaste = editMenu->addAction(tr("&Paste"), this, &ReferenceImageWindow::pasteImage);
    actPaste->setShortcut(QKeySequence::Paste);

    // 2. 中央レイアウト
    QWidget* central = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // 中央スタック（タブウィジェット / 空のプレースホルダー）
    m_centralStack = new QStackedWidget(central);

    // プレースホルダー
    m_placeholderLabel = new QLabel(tr("Open an image via [File] > [Open Image] or [Edit] > [Paste].\n\n"
                                       "・Left click: Sample pixel color and set as drawing color\n"
                                       "・Right click: Register to palette / Close image\n"
                                       "・Middle drag: Pan view\n"
                                       "・Mouse wheel: Zoom in / out"), m_centralStack);
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    m_placeholderLabel->setStyleSheet("QLabel { color: #888888; font-size: 12px; }");
    m_centralStack->addWidget(m_placeholderLabel);

    // タブウィジェット
    m_tabWidget = new QTabWidget(m_centralStack);
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->setStyleSheet("QTabWidget::pane { border: 1px solid #3c3c3c; }");
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &ReferenceImageWindow::closeTab);
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &ReferenceImageWindow::onTabChanged);
    m_centralStack->addWidget(m_tabWidget);

    mainLayout->addWidget(m_centralStack, 1);

    // 3. タブウィジェットの下側: 拡大率設定 SpinScale
    QWidget* bottomBar = new QWidget(central);
    QHBoxLayout* bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(2, 2, 2, 2);
    bottomLayout->setSpacing(6);

    m_zoomScale = new SpinScale(tr("Zoom"), 10.0, 1600.0, 10.0, 0, "%", bottomBar);
    m_zoomScale->setValue(100.0);
    connect(m_zoomScale, &SpinScale::valueChanged, this, &ReferenceImageWindow::onZoomScaleChanged);
    bottomLayout->addWidget(m_zoomScale);

    mainLayout->addWidget(bottomBar, 0);

    setCentralWidget(central);
}

void ReferenceImageWindow::updatePlaceholder() {
    if (m_tabWidget->count() == 0) {
        m_centralStack->setCurrentWidget(m_placeholderLabel);
        m_zoomScale->setEnabled(false);
    } else {
        m_centralStack->setCurrentWidget(m_tabWidget);
        m_zoomScale->setEnabled(true);
    }
}

ReferenceImageView* ReferenceImageWindow::currentView() const {
    if (m_tabWidget->count() == 0) return nullptr;
    return qobject_cast<ReferenceImageView*>(m_tabWidget->currentWidget());
}

int ReferenceImageWindow::tabCount() const {
    return m_tabWidget ? m_tabWidget->count() : 0;
}

QImage ReferenceImageWindow::currentImage() const {
    auto view = currentView();
    return view ? view->image() : QImage();
}

void ReferenceImageWindow::addImage(const QImage& image, const QString& title) {
    if (image.isNull()) return;

    ReferenceImageView* view = new ReferenceImageView(image, m_tabWidget);
    connect(view, &ReferenceImageView::colorPicked, this, &ReferenceImageWindow::colorPicked);
    connect(view, &ReferenceImageView::addToPalette, this, &ReferenceImageWindow::addToPalette);
    connect(view, &ReferenceImageView::closeRequested, this, [this, view]() {
        int idx = m_tabWidget->indexOf(view);
        if (idx >= 0) {
            closeTab(idx);
        }
    });
    connect(view, &ReferenceImageView::zoomChanged, this, &ReferenceImageWindow::onChildViewZoomChanged);

    int idx = m_tabWidget->addTab(view, title);
    m_tabWidget->setCurrentIndex(idx);
    updatePlaceholder();

    // ズーム初期化
    m_updatingZoomScale = true;
    m_zoomScale->setValue(100.0);
    m_updatingZoomScale = false;
}

void ReferenceImageWindow::openImageFile(const QString& path) {
    if (path.isEmpty()) return;
    QImage img(path);
    if (!img.isNull()) {
        addImage(img, QFileInfo(path).fileName());
    }
}

void ReferenceImageWindow::openImage() {
    QString filter = tr("Image Files (*.png *.jpg *.jpeg *.bmp *.webp *.gif *.tga);;All Files (*)");
    QString path = QFileDialog::getOpenFileName(this, tr("Open Image"), "", filter);
    if (!path.isEmpty()) {
        openImageFile(path);
    }
}

void ReferenceImageWindow::pasteImage() {
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (!clipboard) return;

    QImage img = clipboard->image();
    if (img.isNull()) {
        QPixmap pm = clipboard->pixmap();
        if (!pm.isNull()) {
            img = pm.toImage();
        }
    }

    if (!img.isNull()) {
        addImage(img, tr("Pasted Image %1").arg(m_pasteCounter++));
    }
}

void ReferenceImageWindow::closeTab(int index) {
    if (index >= 0 && index < m_tabWidget->count()) {
        QWidget* w = m_tabWidget->widget(index);
        m_tabWidget->removeTab(index);
        delete w;
        updatePlaceholder();
        onTabChanged(m_tabWidget->currentIndex());
    }
}

void ReferenceImageWindow::onTabChanged(int index) {
    if (index >= 0 && index < m_tabWidget->count()) {
        ReferenceImageView* view = qobject_cast<ReferenceImageView*>(m_tabWidget->widget(index));
        if (view) {
            m_updatingZoomScale = true;
            m_zoomScale->setValue(std::round(view->zoom() * 100.0));
            m_updatingZoomScale = false;
        }
    }
}

void ReferenceImageWindow::onZoomScaleChanged(double val) {
    if (m_updatingZoomScale) return;
    ReferenceImageView* view = currentView();
    if (view) {
        view->setZoom(val / 100.0);
    }
}

void ReferenceImageWindow::onChildViewZoomChanged(double zoom) {
    ReferenceImageView* view = currentView();
    if (view && sender() == view) {
        m_updatingZoomScale = true;
        m_zoomScale->setValue(std::round(zoom * 100.0));
        m_updatingZoomScale = false;
    }
}

void ReferenceImageWindow::closeEvent(QCloseEvent* event) {
    emit windowClosed();
    QMainWindow::closeEvent(event);
}

} // namespace Kaburagi
