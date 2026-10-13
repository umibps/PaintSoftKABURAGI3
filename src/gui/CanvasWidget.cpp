#include "CanvasWidget.hpp"
#include "UndoCommands.hpp"
#include "TextBalloonDialog.hpp"
#include "TextSettingsWidget.hpp"
#include "PanelSettingsWidget.hpp"
#include "GroupLayer.hpp"
#include "ThreeDLayer.hpp"
#include "ThreeDSettingsWidget.hpp"
#include "core/BlendModes.hpp"
#include "DisplayFilterManager.hpp"
#include <QPainter>
#include <QTabletEvent>
#include <QMouseEvent>
#include <QTouchEvent>
#include <QWheelEvent>
#include <QGuiApplication>
#include <QRandomGenerator>
#include <cmath>
#include <algorithm>
#include <unordered_map>

namespace Kaburagi {

CanvasWidget::CanvasWidget(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_AcceptTouchEvents, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::BlankCursor); // GIMP風 ブラシサイズ輪郭カーソルを表示するため標準カーソルを非表示

    connect(&DisplayFilterManager::instance(), &DisplayFilterManager::filterChanged, this, QOverload<>::of(&QWidget::update));

    // GIMPスタイル マーチングアンツ（アリの行進）用タイマー (120ms間隔)
    m_antsTimer = new QTimer(this);
    m_antsTimer->setInterval(120);
    connect(m_antsTimer, &QTimer::timeout, this, [this]() {
        if (m_showSelection && m_document && (m_document->hasSelection() || m_isSelecting)) {
            m_antsOffset = (m_antsOffset + 1) % 8;
            update();
        } else if (!m_isSelecting) {
            m_antsTimer->stop();
        }
    });

    // エアブラシ ホールド連続噴霧タイマー (GIMP Airbrush)
    m_airbrushTimer = new QTimer(this);
    m_airbrushTimer->setInterval(m_airbrushInterval);
    connect(m_airbrushTimer, &QTimer::timeout, this, &CanvasWidget::onAirbrushTimerTimeout);
}

CanvasWidget::~CanvasWidget() {
    if (m_airbrushTimer && m_airbrushTimer->isActive()) {
        m_airbrushTimer->stop();
    }
    for (auto& pair : m_strokeAlphaTiles) {
        TileMemoryPool::instance().freeTile(pair.second);
    }
    m_strokeAlphaTiles.clear();
    m_strokeTileSnapshots.clear();
}

void CanvasWidget::setDocument(Document* doc) {
    if (m_document == doc) return;
    m_document = doc;
    if (m_document) {
        connect(m_document, &Document::documentUpdated, this, [this](const QRect&) {
            update();
        });
        connect(m_document, &Document::selectionChanged, this, [this]() {
            if (m_document && !m_document->hasSelection()) {
                m_hasActiveRect = false;
            } else if (m_document && m_document->hasSelection() && !m_hasActiveRect) {
                m_hasActiveRect = true;
                m_activeDocRect = m_document->selection().boundingRect();
                m_activeViewRect = viewTransform().mapRect(m_activeDocRect);
                m_activeRectAlignToWindow = false;
            }
            updateAntsTimer();
            update();
        });
        connect(m_document, &Document::activeLayerChanged, this, [this](int) {
            updateCursorForActiveLayer();
        });
        m_zoom = 1.0; // デフォルトの表示拡大倍率は100%
        m_rotationAngle = 0.0;
        m_panOffset = QPointF(0.0, 0.0);
        m_firstShowHandled = true;
        updateAntsTimer();
        update();
        notifyTransformChanged();
    }
    updateCursorForActiveLayer();
}

void CanvasWidget::updateCursorForActiveLayer() {
    auto activeL = m_document ? m_document->activeLayer() : nullptr;
    if (activeL && activeL->type() == LayerType::Group) {
        setCursor(Qt::SizeAllCursor);
        m_showBrushOutline = false;
    } else if ((activeL && activeL->type() == LayerType::Adjustment) ||
               m_currentTool == ToolType::AdjustmentBrightnessContrast ||
               m_currentTool == ToolType::AdjustmentHueSaturation ||
               m_currentTool == ToolType::AdjustmentToneCurve ||
               m_currentTool == ToolType::AdjustmentLevels ||
               m_currentTool == ToolType::AdjustmentInvert ||
               m_currentTool == ToolType::AdjustmentGrayscale) {
        setCursor(Qt::ArrowCursor);
        m_showBrushOutline = false;
    } else if (m_currentTool == ToolType::Pipette) {
        setCursor(Qt::CrossCursor);
        m_showBrushOutline = false;
    } else if (m_currentTool == ToolType::RectSelect || m_currentTool == ToolType::MagicWand ||
               m_currentTool == ToolType::Bucket || m_currentTool == ToolType::Gradient ||
               m_currentTool == ToolType::VectorPolyline || m_currentTool == ToolType::VectorCurve ||
               m_currentTool == ToolType::VectorEllipse || m_currentTool == ToolType::VectorCircle ||
               m_currentTool == ToolType::VectorPolygon) {
        setCursor(Qt::CrossCursor);
        m_showBrushOutline = false;
    } else if (m_currentTool == ToolType::Text) {
        setCursor(Qt::IBeamCursor);
        m_showBrushOutline = false;
    } else if (m_currentTool == ToolType::TextBalloon || m_currentTool == ToolType::VectorControlPoint ||
               m_currentTool == ToolType::VectorPressure ||
               m_currentTool == ToolType::VectorLineEdit || m_currentTool == ToolType::VectorColorChange) {
        setCursor(Qt::ArrowCursor);
        m_showBrushOutline = false;
    } else if (m_currentTool == ToolType::Transform) {
        m_showBrushOutline = false;
        updateTransformCursor(m_transformHoverHandle, QGuiApplication::keyboardModifiers());
    } else {
        setCursor(Qt::BlankCursor);
        m_showBrushOutline = true;
    }
    update();
}

void CanvasWidget::setShowSelection(bool show) {
    if (m_showSelection != show) {
        m_showSelection = show;
        updateAntsTimer();
        update();
    }
}

void CanvasWidget::updateAntsTimer() {
    bool needTimer = m_showSelection && m_document && (m_document->hasSelection() || m_isSelecting);
    if (needTimer) {
        if (!m_antsTimer->isActive()) {
            m_antsTimer->start(120);
        }
    } else {
        if (m_antsTimer->isActive()) {
            m_antsTimer->stop();
        }
    }
}

void CanvasWidget::selectAll() {
    if (!m_document) return;
    QPainterPath fullPath;
    fullPath.addRect(0, 0, m_document->width(), m_document->height());
    QPainterPath oldSel = m_document->selection();
    if (m_document->undoStack()) {
        m_document->undoStack()->push(new SelectCommand(m_document, oldSel, fullPath));
    } else {
        m_document->setSelection(fullPath);
    }
    m_hasActiveRect = true;
    m_activeDocRect = QRectF(0, 0, m_document->width(), m_document->height());
    m_activeViewRect = viewTransform().mapRect(m_activeDocRect);
    m_activeRectAlignToWindow = false;
    updateAntsTimer();
    update();
}

void CanvasWidget::clearSelection() {
    if (!m_document || !m_document->hasSelection()) return;
    QPainterPath oldSel = m_document->selection();
    QPainterPath emptySel;
    if (m_document->undoStack()) {
        m_document->undoStack()->push(new SelectCommand(m_document, oldSel, emptySel));
    } else {
        m_document->setSelection(emptySel);
    }
    m_hasActiveRect = false;
    updateAntsTimer();
    update();
}

void CanvasWidget::invertSelection() {
    if (!m_document) return;
    QPainterPath fullPath;
    fullPath.addRect(0, 0, m_document->width(), m_document->height());
    QPainterPath oldSel = m_document->selection();
    QPainterPath invertedSel = fullPath.subtracted(oldSel);
    if (m_document->undoStack()) {
        m_document->undoStack()->push(new SelectCommand(m_document, oldSel, invertedSel));
    } else {
        m_document->setSelection(invertedSel);
    }
    m_hasActiveRect = false;
    updateAntsTimer();
    update();
}

CanvasWidget::RectHandle CanvasWidget::hitTestRectHandle(const QPointF& mouseViewPos) const {
    if (!m_hasActiveRect || !m_document || !m_document->hasSelection()) {
        return RectHandle::None;
    }

    if (m_activeRectAlignToWindow) {
        QRectF r = m_activeViewRect.normalized();
        if (r.width() < 6 || r.height() < 6) return RectHandle::None;

        qreal hs = std::min({28.0, r.width() / 3.0, r.height() / 3.0});
        qreal x = mouseViewPos.x();
        qreal y = mouseViewPos.y();

        if (QRectF(r.left(), r.top(), hs, hs).contains(mouseViewPos)) return RectHandle::TopLeft;
        if (QRectF(r.right() - hs, r.top(), hs, hs).contains(mouseViewPos)) return RectHandle::TopRight;
        if (QRectF(r.left(), r.bottom() - hs, hs, hs).contains(mouseViewPos)) return RectHandle::BottomLeft;
        if (QRectF(r.right() - hs, r.bottom() - hs, hs, hs).contains(mouseViewPos)) return RectHandle::BottomRight;

        const qreal margin = 6.0;
        if (std::abs(y - r.top()) <= margin && x >= r.left() && x <= r.right()) return RectHandle::TopEdge;
        if (std::abs(y - r.bottom()) <= margin && x >= r.left() && x <= r.right()) return RectHandle::BottomEdge;
        if (std::abs(x - r.left()) <= margin && y >= r.top() && y <= r.bottom()) return RectHandle::LeftEdge;
        if (std::abs(x - r.right()) <= margin && y >= r.top() && y <= r.bottom()) return RectHandle::RightEdge;

        if (r.contains(mouseViewPos)) return RectHandle::Inside;

        return RectHandle::None;
    } else {
        QPointF docPt = viewToDoc(mouseViewPos);
        QRectF r = m_activeDocRect.normalized();
        if (r.width() < 2 || r.height() < 2) return RectHandle::None;

        qreal hs = std::min({28.0 / m_zoom, r.width() / 3.0, r.height() / 3.0});
        qreal x = docPt.x();
        qreal y = docPt.y();

        if (QRectF(r.left(), r.top(), hs, hs).contains(docPt)) return RectHandle::TopLeft;
        if (QRectF(r.right() - hs, r.top(), hs, hs).contains(docPt)) return RectHandle::TopRight;
        if (QRectF(r.left(), r.bottom() - hs, hs, hs).contains(docPt)) return RectHandle::BottomLeft;
        if (QRectF(r.right() - hs, r.bottom() - hs, hs, hs).contains(docPt)) return RectHandle::BottomRight;

        const qreal margin = 6.0 / m_zoom;
        if (std::abs(y - r.top()) <= margin && x >= r.left() && x <= r.right()) return RectHandle::TopEdge;
        if (std::abs(y - r.bottom()) <= margin && x >= r.left() && x <= r.right()) return RectHandle::BottomEdge;
        if (std::abs(x - r.left()) <= margin && y >= r.top() && y <= r.bottom()) return RectHandle::LeftEdge;
        if (std::abs(x - r.right()) <= margin && y >= r.top() && y <= r.bottom()) return RectHandle::RightEdge;

        if (r.contains(docPt)) return RectHandle::Inside;

        return RectHandle::None;
    }
}

void CanvasWidget::updateSelectionCursor(RectHandle handle) {
    if (m_currentTool != ToolType::RectSelect) return;

    switch (handle) {
    case RectHandle::TopLeft:
    case RectHandle::BottomRight:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case RectHandle::TopRight:
    case RectHandle::BottomLeft:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case RectHandle::TopEdge:
    case RectHandle::BottomEdge:
        setCursor(Qt::SizeVerCursor);
        break;
    case RectHandle::LeftEdge:
    case RectHandle::RightEdge:
        setCursor(Qt::SizeHorCursor);
        break;
    case RectHandle::Inside:
        setCursor(Qt::SizeAllCursor);
        break;
    case RectHandle::None:
    default:
        setCursor(Qt::CrossCursor);
        break;
    }
}

std::shared_ptr<TextLayer> CanvasWidget::ensureTextLayer() {
    if (!m_document) return nullptr;
    auto activeL = m_document->activeLayer();
    auto textLayer = std::dynamic_pointer_cast<TextLayer>(activeL);
    if (!textLayer) {
        // テキストレイヤーがアクティブでない場合、自動生成してアクティブにする！
        textLayer = m_document->addTextLayer();
    }
    return textLayer;
}

QTransform CanvasWidget::viewTransform() const {
    QTransform tf;
    // 1. 画面中心に原点を移動
    tf.translate(width() * 0.5, height() * 0.5);

    // 2. 画面中心を回転中心として回転
    tf.rotate(m_rotationAngle);

    // 3. 画面中心で左右反転
    if (m_flippedHorizontally) {
        tf.scale(-1.0, 1.0);
    }

    // 4. パン（平行移動）
    tf.translate(m_panOffset.x(), m_panOffset.y());

    // 5. ズーム倍率
    tf.scale(m_zoom, m_zoom);

    // 6. ドキュメントの中心を原点に合わせる
    if (m_document) {
        tf.translate(-m_document->width() * 0.5, -m_document->height() * 0.5);
    }
    return tf;
}

QPointF CanvasWidget::viewToDoc(const QPointF& viewPos) const {
    return viewTransform().inverted().map(viewPos);
}

QPointF CanvasWidget::docToView(const QPointF& docPos) const {
    return viewTransform().map(docPos);
}

void CanvasWidget::notifyTransformChanged() {
    QPointF docCenterVisible = viewToDoc(QPointF(width() * 0.5, height() * 0.5));
    emit transformChanged(m_zoom, m_rotationAngle, m_flippedHorizontally, docCenterVisible, size());
}

void CanvasWidget::setZoom(double z) {
    m_zoom = std::clamp(z, 0.05, 4.0);
    update();
    notifyTransformChanged();
}

void CanvasWidget::setRotationAngle(double angleDeg) {
    m_rotationAngle = std::fmod(angleDeg, 360.0);
    if (m_rotationAngle < 0) m_rotationAngle += 360.0;
    update();
    notifyTransformChanged();
}

void CanvasWidget::toggleFlipHorizontally() {
    m_flippedHorizontally = !m_flippedHorizontally;
    update();
    notifyTransformChanged();
}

void CanvasWidget::fitToView() {
    if (!m_document) return;
    int docW = m_document->width();
    int docH = m_document->height();
    if (docW <= 0 || docH <= 0 || width() <= 0 || height() <= 0) return;

    // キャンバス長辺の √2 倍の表示領域を確保
    double maxSide = std::max(docW, docH);
    double requiredSize = maxSide * std::sqrt(2.0);

    // 画面中央表示かつ外周マージン（左右上下20px）を持たせてすっぽり収まるズーム倍率を計算
    double availW = std::max(20.0, width() - 40.0);
    double availH = std::max(20.0, height() - 40.0);
    double fitZoom = std::min(availW / requiredSize, availH / requiredSize);

    m_zoom = std::clamp(fitZoom, 0.05, 4.0);
    m_rotationAngle = 0.0;
    m_flippedHorizontally = false;
    m_panOffset = QPointF(0.0, 0.0);

    update();
    notifyTransformChanged();
}

void CanvasWidget::resetTransform() {
    m_zoom = 1.0; // 100%
    m_rotationAngle = 0.0;
    m_flippedHorizontally = false;
    m_panOffset = QPointF(0.0, 0.0);
    update();
    notifyTransformChanged();
}

void CanvasWidget::setViewCenter(const QPointF& docCenter) {
    if (!m_document) return;
    QPointF docCenterPt(m_document->width() * 0.5, m_document->height() * 0.5);
    m_panOffset = -(docCenter - docCenterPt) * m_zoom;
    update();
    notifyTransformChanged();
}

void CanvasWidget::panByScreenDelta(double dx, double dy) {
    if (!m_document) return;
    double rad = -m_rotationAngle * M_PI / 180.0;
    double cosA = std::cos(rad);
    double sinA = std::sin(rad);
    double rx = dx * cosA - dy * sinA;
    double ry = dx * sinA + dy * cosA;
    if (m_flippedHorizontally) {
        rx = -rx;
    }
    m_panOffset += QPointF(rx, ry);
    update();
    notifyTransformChanged();
}

void CanvasWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!m_firstShowHandled) {
        m_firstShowHandled = true;
        m_zoom = 1.0; // 100%
        m_rotationAngle = 0.0;
        m_panOffset = QPointF(0.0, 0.0);
        update();
        notifyTransformChanged();
    }
}

void CanvasWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    notifyTransformChanged();
}

LayerPtr CanvasWidget::effectiveTargetLayer() const {
    if (m_document && m_document->isSelectionEditMode()) {
        return m_document->selectionMaskLayer();
    }
    return m_document ? m_document->activeLayer() : nullptr;
}

uint32_t CanvasWidget::effectiveDrawColor(uint32_t baseColor) const {
    if (m_document && m_document->isSelectionEditMode()) {
        if ((baseColor >> 24) == 0) return 0x00000000;
        return 0xFFFFFFFF; // 選択マスクへの追加は不透明白
    }
    return baseColor;
}

void CanvasWidget::strokeBegin(const QPointF& docPos, float pressure) {
    if (!m_document) return;

    float minP = 0.0f;
    if (m_currentTool == ToolType::Blur) {
        minP = m_blurMinPressure / 100.0f;
    } else if (m_currentTool == ToolType::Blend) {
        minP = m_smudgeMinPressure / 100.0f;
    } else if (m_currentTool == ToolType::Watercolor) {
        minP = m_watercolorMinPressure / 100.0f;
    } else if (m_currentTool == ToolType::CompositeBrush) {
        minP = m_compositeBrushMinPressure / 100.0f;
    } else if (m_currentTool == ToolType::Marker) {
        minP = m_imageBrushMinPressure / 100.0f;
    } else {
        minP = m_brushMinPressure / 100.0f;
    }
    if (pressure > 0.0f && pressure < minP) {
        pressure = minP;
    }

    m_isDrawing = true;
    m_smoother.reset();
    m_lastFilteredPoint = m_smoother.pushPoint(docPos, pressure);

    // 前回のストローク情報が残っていれば解放
    for (auto& pair : m_strokeAlphaTiles) {
        TileMemoryPool::instance().freeTile(pair.second);
    }
    m_strokeAlphaTiles.clear();
    m_strokeTileSnapshots.clear();

    auto activeL = effectiveTargetLayer();
    if (!activeL) return;

    m_strokeDirtyRect = QRect();
    if (activeL->type() == LayerType::Raster) {
        m_strokePreTiles = activeL->tileManager().clone();
    }

    float pEffSize = m_pressureSizeEnabled ? pressure : 1.0f;
    float pEffDens = m_pressureDensityEnabled ? std::clamp(pressure, 0.05f, 1.0f) : 1.0f;

    if (m_currentTool == ToolType::Blur) {
        m_hasDragColor = false;
        if (activeL->type() == LayerType::Raster) {
            float pBlurEffSize = m_blurPressureSizeEnabled ? pressure : 1.0f;
            float pBlurEffDens = m_blurPressureDensityEnabled ? std::clamp(pressure, 0.05f, 1.0f) : 1.0f;
            float w = m_blurSize * (m_blurMinSizeRatio + (1.0f - m_blurMinSizeRatio) * pBlurEffSize);
            float dens = m_blurDensity * pBlurEffDens;
            paintBlurSegment(docPos, docPos, w, w, dens, dens);
        }
    } else if (m_currentTool == ToolType::Blend) {
        m_hasSmudgeColor = false;
        if (activeL->type() == LayerType::Raster) {
            float pSmudgeEffSize = m_smudgePressureSizeEnabled ? pressure : 1.0f;
            float pSmudgeEffDens = m_smudgePressureDensityEnabled ? std::clamp(pressure, 0.05f, 1.0f) : 1.0f;
            float w = m_smudgeSize * (m_smudgeMinSizeRatio + (1.0f - m_smudgeMinSizeRatio) * pSmudgeEffSize);
            float dens = m_smudgeDensity * pSmudgeEffDens;
            paintSmudgeSegment(docPos, docPos, w, w, dens, dens);
        }
    } else if (m_currentTool == ToolType::Watercolor) {
        m_hasWatercolorColor = false;
        if (activeL->type() == LayerType::Raster) {
            float pWcEffSize = m_watercolorPressureSizeEnabled ? pressure : 1.0f;
            float pWcEffDens = m_watercolorPressureDensityEnabled ? std::clamp(pressure, 0.05f, 1.0f) : 1.0f;
            float w = m_watercolorSize * (m_watercolorMinSizeRatio + (1.0f - m_watercolorMinSizeRatio) * pWcEffSize);
            float dens = m_watercolorDensity * pWcEffDens;
            paintWatercolorSegment(docPos, docPos, w, w, dens, dens, pressure, pressure);
        }
    } else if (m_currentTool == ToolType::CompositeBrush) {
        m_hasCompositeBrushColor = false;
        if (activeL->type() == LayerType::Raster) {
            float pCompEffSize = m_compositeBrushPressureSizeEnabled ? pressure : 1.0f;
            float pCompEffDens = m_compositeBrushPressureDensityEnabled ? std::clamp(pressure, 0.05f, 1.0f) : 1.0f;
            float w = m_compositeBrushSize * (m_compositeBrushMinSizeRatio + (1.0f - m_compositeBrushMinSizeRatio) * pCompEffSize);
            float dens = m_compositeBrushDensity * pCompEffDens;
            paintCompositeBrushSegment(docPos, docPos, w, w, dens, dens, pressure, pressure);
        }
    } else if (m_currentTool == ToolType::Marker) {
        m_imageBrushDistRemainder = 0.0f;
        if (activeL->type() == LayerType::Raster) {
            float pImgEffSize = m_imageBrushPressureSizeEnabled ? pressure : 1.0f;
            float pImgEffDens = m_imageBrushPressureDensityEnabled ? std::clamp(pressure, 0.05f, 1.0f) : 1.0f;
            float w = m_imageBrushSize * (m_imageBrushMinSizeRatio + (1.0f - m_imageBrushMinSizeRatio) * pImgEffSize);
            float dens = m_imageBrushDensity * pImgEffDens;
            paintImageBrushSegment(docPos, docPos, w, w, dens, dens, pressure, pressure);
        }
    } else if (activeL->type() == LayerType::Vector) {
        if (m_currentTool == ToolType::Eraser) {
            auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(activeL);
            if (vecLayer) {
                m_beforeVectorStrokes = vecLayer->strokes();
                m_vectorStrokesModified = false;
                m_vectorEraserTrajectory.clear();
                m_vectorEraserTrajectory.push_back(docPos);
                m_strokePreTiles = vecLayer->tileManager().clone();
                paintBrushSegment(docPos, docPos, m_vectorEraserSize, m_vectorEraserSize, 1.0f, 1.0f, 0x00000000);
            }
        } else {
            m_currentVectorStroke = VectorStroke();
            m_currentVectorStroke.baseWidth = m_brushSize;
            m_currentVectorStroke.color = m_currentColor.rgba();
            m_currentVectorStroke.density = m_brushDensity;
            m_currentVectorStroke.hardness = m_brushHardness;
            m_currentVectorStroke.falloff = m_brushFalloff;
            m_currentVectorStroke.points.push_back({ static_cast<float>(docPos.x()), static_cast<float>(docPos.y()), pEffSize, pEffDens, false });
            m_strokePreTiles = activeL->tileManager().clone();
        }
    } else {
        // ラスター描画
        float w = m_brushSize * (m_minSizeRatio + (1.0f - m_minSizeRatio) * pEffSize);
        float dens = m_brushDensity * pEffDens;
        uint32_t col = (m_currentTool == ToolType::Eraser) ? 0x00000000 : effectiveDrawColor(m_currentColor.rgba());
        paintBrushSegment(docPos, docPos, w, w, dens, dens, col);

        if (m_currentTool == ToolType::Airbrush && activeL->type() == LayerType::Raster) {
            if (m_airbrushTimer && !m_airbrushTimer->isActive()) {
                m_airbrushTimer->start(m_airbrushInterval);
            }
        }
    }
}

void CanvasWidget::strokeMove(const QPointF& docPos, float pressure) {
    if (!m_isDrawing || !m_document) return;

    float minP = 0.0f;
    if (m_currentTool == ToolType::Blur) {
        minP = m_blurMinPressure / 100.0f;
    } else if (m_currentTool == ToolType::Blend) {
        minP = m_smudgeMinPressure / 100.0f;
    } else if (m_currentTool == ToolType::Watercolor) {
        minP = m_watercolorMinPressure / 100.0f;
    } else if (m_currentTool == ToolType::CompositeBrush) {
        minP = m_compositeBrushMinPressure / 100.0f;
    } else if (m_currentTool == ToolType::Marker) {
        minP = m_imageBrushMinPressure / 100.0f;
    } else {
        minP = m_brushMinPressure / 100.0f;
    }
    if (pressure > 0.0f && pressure < minP) {
        pressure = minP;
    }

    StrokeInputPoint cur = m_smoother.pushPoint(docPos, pressure);

    auto activeL = effectiveTargetLayer();
    if (!activeL) return;

    float p0EffSize = m_pressureSizeEnabled ? m_lastFilteredPoint.pressure : 1.0f;
    float p0EffDens = m_pressureDensityEnabled ? std::clamp(m_lastFilteredPoint.pressure, 0.05f, 1.0f) : 1.0f;

    float p1EffSize = m_pressureSizeEnabled ? cur.pressure : 1.0f;
    float p1EffDens = m_pressureDensityEnabled ? std::clamp(cur.pressure, 0.05f, 1.0f) : 1.0f;

    if (m_currentTool == ToolType::Blur) {
        if (activeL->type() == LayerType::Raster) {
            float p0BlurEffSize = m_blurPressureSizeEnabled ? m_lastFilteredPoint.pressure : 1.0f;
            float p0BlurEffDens = m_blurPressureDensityEnabled ? std::clamp(m_lastFilteredPoint.pressure, 0.05f, 1.0f) : 1.0f;
            float p1BlurEffSize = m_blurPressureSizeEnabled ? cur.pressure : 1.0f;
            float p1BlurEffDens = m_blurPressureDensityEnabled ? std::clamp(cur.pressure, 0.05f, 1.0f) : 1.0f;

            float w0 = m_blurSize * (m_blurMinSizeRatio + (1.0f - m_blurMinSizeRatio) * p0BlurEffSize);
            float w1 = m_blurSize * (m_blurMinSizeRatio + (1.0f - m_blurMinSizeRatio) * p1BlurEffSize);
            float dens0 = m_blurDensity * p0BlurEffDens;
            float dens1 = m_blurDensity * p1BlurEffDens;

            paintBlurSegment(m_lastFilteredPoint.pos, cur.pos, w0, w1, dens0, dens1);
        }
    } else if (m_currentTool == ToolType::Blend) {
        if (activeL->type() == LayerType::Raster) {
            float p0SmudgeEffSize = m_smudgePressureSizeEnabled ? m_lastFilteredPoint.pressure : 1.0f;
            float p0SmudgeEffDens = m_smudgePressureDensityEnabled ? std::clamp(m_lastFilteredPoint.pressure, 0.05f, 1.0f) : 1.0f;
            float p1SmudgeEffSize = m_smudgePressureSizeEnabled ? cur.pressure : 1.0f;
            float p1SmudgeEffDens = m_smudgePressureDensityEnabled ? std::clamp(cur.pressure, 0.05f, 1.0f) : 1.0f;

            float w0 = m_smudgeSize * (m_smudgeMinSizeRatio + (1.0f - m_smudgeMinSizeRatio) * p0SmudgeEffSize);
            float w1 = m_smudgeSize * (m_smudgeMinSizeRatio + (1.0f - m_smudgeMinSizeRatio) * p1SmudgeEffSize);
            float dens0 = m_smudgeDensity * p0SmudgeEffDens;
            float dens1 = m_smudgeDensity * p1SmudgeEffDens;

            paintSmudgeSegment(m_lastFilteredPoint.pos, cur.pos, w0, w1, dens0, dens1);
        }
    } else if (m_currentTool == ToolType::Watercolor) {
        if (activeL->type() == LayerType::Raster) {
            float p0WcEffSize = m_watercolorPressureSizeEnabled ? m_lastFilteredPoint.pressure : 1.0f;
            float p0WcEffDens = m_watercolorPressureDensityEnabled ? std::clamp(m_lastFilteredPoint.pressure, 0.05f, 1.0f) : 1.0f;
            float p1WcEffSize = m_watercolorPressureSizeEnabled ? cur.pressure : 1.0f;
            float p1WcEffDens = m_watercolorPressureDensityEnabled ? std::clamp(cur.pressure, 0.05f, 1.0f) : 1.0f;

            float w0 = m_watercolorSize * (m_watercolorMinSizeRatio + (1.0f - m_watercolorMinSizeRatio) * p0WcEffSize);
            float w1 = m_watercolorSize * (m_watercolorMinSizeRatio + (1.0f - m_watercolorMinSizeRatio) * p1WcEffSize);
            float dens0 = m_watercolorDensity * p0WcEffDens;
            float dens1 = m_watercolorDensity * p1WcEffDens;

            paintWatercolorSegment(m_lastFilteredPoint.pos, cur.pos, w0, w1, dens0, dens1, m_lastFilteredPoint.pressure, cur.pressure);
        }
    } else if (m_currentTool == ToolType::CompositeBrush) {
        if (activeL->type() == LayerType::Raster) {
            float p0CompEffSize = m_compositeBrushPressureSizeEnabled ? m_lastFilteredPoint.pressure : 1.0f;
            float p0CompEffDens = m_compositeBrushPressureDensityEnabled ? std::clamp(m_lastFilteredPoint.pressure, 0.05f, 1.0f) : 1.0f;
            float p1CompEffSize = m_compositeBrushPressureSizeEnabled ? cur.pressure : 1.0f;
            float p1CompEffDens = m_compositeBrushPressureDensityEnabled ? std::clamp(cur.pressure, 0.05f, 1.0f) : 1.0f;

            float w0 = m_compositeBrushSize * (m_compositeBrushMinSizeRatio + (1.0f - m_compositeBrushMinSizeRatio) * p0CompEffSize);
            float w1 = m_compositeBrushSize * (m_compositeBrushMinSizeRatio + (1.0f - m_compositeBrushMinSizeRatio) * p1CompEffSize);
            float dens0 = m_compositeBrushDensity * p0CompEffDens;
            float dens1 = m_compositeBrushDensity * p1CompEffDens;

            paintCompositeBrushSegment(m_lastFilteredPoint.pos, cur.pos, w0, w1, dens0, dens1, m_lastFilteredPoint.pressure, cur.pressure);
        }
    } else if (m_currentTool == ToolType::Marker) {
        if (activeL->type() == LayerType::Raster) {
            float p0ImgEffSize = m_imageBrushPressureSizeEnabled ? m_lastFilteredPoint.pressure : 1.0f;
            float p0ImgEffDens = m_imageBrushPressureDensityEnabled ? std::clamp(m_lastFilteredPoint.pressure, 0.05f, 1.0f) : 1.0f;
            float p1ImgEffSize = m_imageBrushPressureSizeEnabled ? cur.pressure : 1.0f;
            float p1ImgEffDens = m_imageBrushPressureDensityEnabled ? std::clamp(cur.pressure, 0.05f, 1.0f) : 1.0f;

            float w0 = m_imageBrushSize * (m_imageBrushMinSizeRatio + (1.0f - m_imageBrushMinSizeRatio) * p0ImgEffSize);
            float w1 = m_imageBrushSize * (m_imageBrushMinSizeRatio + (1.0f - m_imageBrushMinSizeRatio) * p1ImgEffSize);
            float dens0 = m_imageBrushDensity * p0ImgEffDens;
            float dens1 = m_imageBrushDensity * p1ImgEffDens;

            paintImageBrushSegment(m_lastFilteredPoint.pos, cur.pos, w0, w1, dens0, dens1, m_lastFilteredPoint.pressure, cur.pressure);
        }
    } else if (activeL->type() == LayerType::Vector) {
        auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(activeL);
        if (m_currentTool == ToolType::Eraser) {
            if (vecLayer) {
                float dist = std::hypot(cur.pos.x() - m_lastFilteredPoint.pos.x(), cur.pos.y() - m_lastFilteredPoint.pos.y());
                float step = std::max(4.0f, m_vectorEraserSize * 0.25f);
                int steps = std::max(1, static_cast<int>(std::ceil(dist / step)));
                for (int s = 1; s <= steps; ++s) {
                    float t = static_cast<float>(s) / steps;
                    QPointF pt = m_lastFilteredPoint.pos + (cur.pos - m_lastFilteredPoint.pos) * t;
                    m_vectorEraserTrajectory.push_back(pt);
                }
                paintBrushSegment(m_lastFilteredPoint.pos, cur.pos, m_vectorEraserSize, m_vectorEraserSize, 1.0f, 1.0f, 0x00000000);
            }
        } else {
            bool shouldAdd = true;
            if (!m_currentVectorStroke.points.empty()) {
                const auto& lastPt = m_currentVectorStroke.points.back();
                float dist = std::hypot(cur.pos.x() - lastPt.x, cur.pos.y() - lastPt.y);
                float pDiff = std::abs(p1EffSize - lastPt.pressure);
                if (dist < 1.5f && pDiff < 0.05f) {
                    shouldAdd = false;
                }
            }
            if (shouldAdd) {
                m_currentVectorStroke.points.push_back({ static_cast<float>(cur.pos.x()), static_cast<float>(cur.pos.y()), p1EffSize, p1EffDens, false });
                if (vecLayer) {
                    vecLayer->tileManager() = m_strokePreTiles.clone();
                    vecLayer->rasterizeStroke(m_currentVectorStroke);
                    m_document->composite();
                    update();
                }
            }
        }
    } else {
        float w0 = m_brushSize * (m_minSizeRatio + (1.0f - m_minSizeRatio) * p0EffSize);
        float w1 = m_brushSize * (m_minSizeRatio + (1.0f - m_minSizeRatio) * p1EffSize);

        float dens0 = m_brushDensity * p0EffDens;
        float dens1 = m_brushDensity * p1EffDens;

        uint32_t col = (m_currentTool == ToolType::Eraser) ? 0x00000000 : effectiveDrawColor(m_currentColor.rgba());

        paintBrushSegment(m_lastFilteredPoint.pos, cur.pos, w0, w1, dens0, dens1, col);
    }

    m_lastFilteredPoint = cur;
}

void CanvasWidget::strokeEnd() {
    if (m_airbrushTimer && m_airbrushTimer->isActive()) {
        m_airbrushTimer->stop();
    }
    m_hasWatercolorColor = false;
    m_hasCompositeBrushColor = false;
    if (!m_isDrawing || !m_document) return;
    m_isDrawing = false;

    // ストローク累積用バッファの解放
    for (auto& pair : m_strokeAlphaTiles) {
        TileMemoryPool::instance().freeTile(pair.second);
    }
    m_strokeAlphaTiles.clear();
    m_strokeTileSnapshots.clear();

    auto activeL = effectiveTargetLayer();
    if (!activeL) return;

    if (activeL->type() == LayerType::Vector) {
        auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(activeL);
        if (m_currentTool == ToolType::Eraser) {
            if (vecLayer && !m_vectorEraserTrajectory.empty()) {
                bool modified = vecLayer->eraseByTrajectory(m_vectorEraserTrajectory, m_vectorEraserSize * 0.5f, m_vectorEraserMode);
                if (modified) {
                    m_document->composite();
                    update();
                    if (m_document->undoStack()) {
                        m_document->undoStack()->push(new ModifyVectorStrokesCommand(
                            m_document, m_document->activeLayerIndex(), m_beforeVectorStrokes, vecLayer->strokes()));
                    }
                } else {
                    // ストロークが変更されなかった場合（空振り・交点なしなど）、ドラッグ中の表示消去を元に戻す
                    vecLayer->tileManager() = m_strokePreTiles.clone();
                    m_document->composite();
                    update();
                }
            }
            m_beforeVectorStrokes.clear();
            m_vectorEraserTrajectory.clear();
            m_strokePreTiles = TileManager(0, 0);
        } else {
            if (vecLayer && !m_currentVectorStroke.points.empty()) {
                const auto& lastPt = m_currentVectorStroke.points.back();
                float dist = std::hypot(m_lastFilteredPoint.pos.x() - lastPt.x, m_lastFilteredPoint.pos.y() - lastPt.y);
                if (dist > 0.5f) {
                    float pEffSize = m_pressureSizeEnabled ? m_lastFilteredPoint.pressure : 1.0f;
                    float pEffDens = m_pressureDensityEnabled ? std::clamp(m_lastFilteredPoint.pressure, 0.05f, 1.0f) : 1.0f;
                    m_currentVectorStroke.points.push_back({
                        static_cast<float>(m_lastFilteredPoint.pos.x()),
                        static_cast<float>(m_lastFilteredPoint.pos.y()),
                        pEffSize, pEffDens, false
                    });
                }

                // 制御点の自動簡約化 (RDPアルゴリズムによる制御点最小化)
                m_currentVectorStroke.points = VectorLayer::simplifyPoints(m_currentVectorStroke.points, 2.0f, 0.2f);

                vecLayer->tileManager() = m_strokePreTiles.clone();
                vecLayer->addStroke(m_currentVectorStroke);
                if (m_document->undoStack()) {
                    m_document->undoStack()->push(new AddVectorStrokeCommand(m_document, m_document->activeLayerIndex(), m_currentVectorStroke));
                }
            }
            m_strokePreTiles = TileManager(0, 0);
        }
    } else {
        if (!m_strokeDirtyRect.isEmpty()) {
            TileManager postTiles = activeL->tileManager().clone();
            if (m_document->undoStack()) {
                if (m_document->isSelectionEditMode()) {
                    m_document->undoStack()->push(new SelectionMaskStrokeCommand(m_document, std::move(m_strokePreTiles), std::move(postTiles), m_strokeDirtyRect));
                } else {
                    m_document->undoStack()->push(new DrawStrokeCommand(m_document, m_document->activeLayerIndex(), std::move(m_strokePreTiles), std::move(postTiles), m_strokeDirtyRect));
                }
            }
            if (m_document->isSelectionEditMode()) {
                m_document->updateSelectionPathFromMask();
            }
        }
    }
    activeL->invalidateContentBounds();
    m_document->composite();
}

void CanvasWidget::paintBrushSegment(const QPointF& p0, const QPointF& p1, float w0, float w1, float dens0, float dens1, uint32_t color) {
    auto activeL = effectiveTargetLayer();
    if (!activeL) return;

    bool hasSelection = m_document && m_document->hasSelection() && !m_document->isSelectionEditMode();
    QRectF selBounds = hasSelection ? m_document->selectionBounds() : QRectF();

    bool isAirbrush = (m_currentTool == ToolType::Airbrush);
    bool alphaLocked = activeL->isAlphaLocked();

    float dx = p1.x() - p0.x();
    float dy = p1.y() - p0.y();
    float dist = std::hypot(dx, dy);
    bool isScatter = m_brushScatter;
    int steps = 0;
    if (isAirbrush) {
        float minW = std::min(w0, w1);
        float stepDist = std::max(1.0f, minW * 0.15f);
        steps = (dist < 0.5f) ? 0 : std::max(1, static_cast<int>(std::ceil(dist / stepDist)));
    } else if (isScatter) {
        float minW = std::min(w0, w1);
        float stepDist = std::max(1.0f, minW * 0.25f);
        steps = (dist < 0.5f) ? 0 : std::max(1, static_cast<int>(std::ceil(dist / stepDist)));
    } else {
        steps = (dist < 0.5f) ? 0 : std::max(1, static_cast<int>(dist * 2.0f));
    }

    QRect dirtyRect;
    int tilesX = activeL->tileManager().tilesX();

    auto applyStamp = [&](float x, float y, float r, float curDensity) {
        int ir = std::max(1, static_cast<int>(std::ceil(r)));

        int minX = std::max(0, static_cast<int>(std::floor(x - ir)));
        int maxX = std::min(m_document->width() - 1, static_cast<int>(std::ceil(x + ir)));
        int minY = std::max(0, static_cast<int>(std::floor(y - ir)));
        int maxY = std::min(m_document->height() - 1, static_cast<int>(std::ceil(y + ir)));

        if (hasSelection) {
            minX = std::max(minX, static_cast<int>(std::floor(selBounds.left())));
            maxX = std::min(maxX, static_cast<int>(std::ceil(selBounds.right())));
            minY = std::max(minY, static_cast<int>(std::floor(selBounds.top())));
            maxY = std::min(maxY, static_cast<int>(std::ceil(selBounds.bottom())));
        }

        if (minX > maxX || minY > maxY) return;

        QRect stampRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
        dirtyRect = dirtyRect.isEmpty() ? stampRect : dirtyRect.united(stampRect);

        // 対象領域にかかるタイルのスナップショットとアルファバッファを確保（非エアブラシのみ）
        if (!isAirbrush) {
            int minTx = minX / TileMemoryPool::TILE_WIDTH;
            int maxTx = maxX / TileMemoryPool::TILE_WIDTH;
            int minTy = minY / TileMemoryPool::TILE_HEIGHT;
            int maxTy = maxY / TileMemoryPool::TILE_HEIGHT;

            for (int ty = minTy; ty <= maxTy; ++ty) {
                for (int tx = minTx; tx <= maxTx; ++tx) {
                    int tileIdx = ty * tilesX + tx;
                    if (m_strokeAlphaTiles.find(tileIdx) == m_strokeAlphaTiles.end()) {
                        const Tile* srcTile = activeL->tileManager().getTile(tx, ty);
                        if (srcTile && srcTile->isValid()) {
                            m_strokeTileSnapshots[tileIdx] = *srcTile; // COW参照
                        } else {
                            m_strokeTileSnapshots[tileIdx] = Tile();
                        }
                        uint8_t* alphaBuf = TileMemoryPool::instance().allocateTile();
                        std::memset(alphaBuf, 0, TileMemoryPool::TILE_WIDTH * TileMemoryPool::TILE_HEIGHT);
                        m_strokeAlphaTiles[tileIdx] = alphaBuf;
                    }
                }
            }
        }

        uint32_t sr = (color >> 16) & 0xFF;
        uint32_t sg = (color >> 8) & 0xFF;
        uint32_t sb = color & 0xFF;
        float brushColorA = ((color >> 24) & 0xFF) / 255.0f;

        int lastTx = -1, lastTy = -1;
        uint8_t* alphaBuf = nullptr;
        const uint32_t* basePixels = nullptr;
        uint32_t* dstPixels = nullptr;

        for (int py = minY; py <= maxY; ++py) {
            int ty = py / TileMemoryPool::TILE_HEIGHT;
            int ly = py % TileMemoryPool::TILE_HEIGHT;

            for (int px = minX; px <= maxX; ++px) {
                float d = std::hypot(px - x, py - y);
                float stampAlpha = 0.0f;

                if (m_antialias) {
                    // 高品質サブピクセル・アンチエイリアス (Hermite Smoothstep)
                    float rInner = std::max(0.0f, r - 0.5f);
                    float rOuter = r + 0.5f;
                    if (d <= rInner) {
                        stampAlpha = curDensity;
                    } else if (d < rOuter) {
                        float u = (rOuter - d) / (rOuter - rInner);
                        float smoothVal = u * u * (3.0f - 2.0f * u);
                        stampAlpha = smoothVal * curDensity;
                    }
                } else {
                    // ハードエッジ (アンチエイリアスOFF・二値化ドットペン)
                    if (d <= r) {
                        stampAlpha = curDensity;
                    }
                }

                if (stampAlpha <= 0.0f) continue;

                // 輪郭の硬さ (0〜100) と ボケ足 (0〜100) によるグラデーション
                if (m_brushHardness < 100 && m_brushFalloff > 0 && r > 0.0f) {
                    float rStart = r * (m_brushHardness / 100.0f);
                    if (d > rStart) {
                        float uGrad = (r > rStart) ? std::min(1.0f, (d - rStart) / (r - rStart)) : 1.0f;
                        float minFade = 1.0f - (m_brushFalloff / 100.0f);
                        float fade = 1.0f - (1.0f - minFade) * uGrad;
                        stampAlpha *= fade;
                    }
                }

                // 選択範囲がある場合、選択強度に応じてブラシ効果を適用
                if (hasSelection) {
                    uint8_t sel = m_document->selectionStrength(px, py);
                    if (sel == 0) continue;
                    stampAlpha *= (sel / 255.0f);
                }

                int tx = px / TileMemoryPool::TILE_WIDTH;
                int lx = px % TileMemoryPool::TILE_WIDTH;

                if (tx != lastTx || ty != lastTy) {
                    lastTx = tx;
                    lastTy = ty;
                    if (!isAirbrush) {
                        int tileIdx = ty * tilesX + tx;
                        alphaBuf = m_strokeAlphaTiles[tileIdx];
                        const Tile& snapTile = m_strokeTileSnapshots[tileIdx];
                        basePixels = snapTile.isValid() ? reinterpret_cast<const uint32_t*>(snapTile.readBuffer()) : nullptr;
                    }
                    Tile* dstTile = activeL->tileManager().getTileForWrite(tx, ty);
                    dstPixels = dstTile ? reinterpret_cast<uint32_t*>(dstTile->writeBuffer()) : nullptr;
                }

                if (!dstPixels) continue;
                if (!isAirbrush && !alphaBuf) continue;

                int localIdx = ly * TileMemoryPool::TILE_WIDTH + lx;

                if (isAirbrush) {
                    float sa = stampAlpha * brushColorA * 0.15f;
                    if (sa <= 0.0001f) continue;

                    uint32_t curPixel = dstPixels[localIdx];
                    uint32_t da_cur = (curPixel >> 24) & 0xFF;

                    if (alphaLocked) {
                        if (da_cur == 0) continue;
                        uint32_t dr = (curPixel >> 16) & 0xFF;
                        uint32_t dg = (curPixel >> 8) & 0xFF;
                        uint32_t db = curPixel & 0xFF;

                        float blendR = blendChannel(dr / 255.0f, sr / 255.0f, m_brushBlendMode) * 255.0f;
                        float blendG = blendChannel(dg / 255.0f, sg / 255.0f, m_brushBlendMode) * 255.0f;
                        float blendB = blendChannel(db / 255.0f, sb / 255.0f, m_brushBlendMode) * 255.0f;

                        uint32_t outR = static_cast<uint32_t>(blendR * sa + dr * (1.0f - sa) + 0.5f);
                        uint32_t outG = static_cast<uint32_t>(blendG * sa + dg * (1.0f - sa) + 0.5f);
                        uint32_t outB = static_cast<uint32_t>(blendB * sa + db * (1.0f - sa) + 0.5f);

                        dstPixels[localIdx] = (da_cur << 24) |
                                              (std::min(255u, outR) << 16) |
                                              (std::min(255u, outG) << 8) |
                                              std::min(255u, outB);
                    } else {
                        float daF = da_cur / 255.0f;
                        float outA = sa + daF * (1.0f - sa);
                        if (outA <= 0.0f) {
                            dstPixels[localIdx] = 0;
                            continue;
                        }

                        uint32_t dr = (curPixel >> 16) & 0xFF;
                        uint32_t dg = (curPixel >> 8) & 0xFF;
                        uint32_t db = curPixel & 0xFF;

                        float blendR = (da_cur > 0) ? (blendChannel(dr / 255.0f, sr / 255.0f, m_brushBlendMode) * 255.0f) : sr;
                        float blendG = (da_cur > 0) ? (blendChannel(dg / 255.0f, sg / 255.0f, m_brushBlendMode) * 255.0f) : sg;
                        float blendB = (da_cur > 0) ? (blendChannel(db / 255.0f, sb / 255.0f, m_brushBlendMode) * 255.0f) : sb;

                        uint32_t outR = static_cast<uint32_t>((blendR * sa + dr * daF * (1.0f - sa)) / outA + 0.5f);
                        uint32_t outG = static_cast<uint32_t>((blendG * sa + dg * daF * (1.0f - sa)) / outA + 0.5f);
                        uint32_t outB = static_cast<uint32_t>((blendB * sa + db * daF * (1.0f - sa)) / outA + 0.5f);
                        uint32_t outA8 = static_cast<uint32_t>(outA * 255.0f + 0.5f);

                        dstPixels[localIdx] = (std::min(255u, outA8) << 24) |
                                              (std::min(255u, outR) << 16) |
                                              (std::min(255u, outG) << 8) |
                                              std::min(255u, outB);
                    }
                } else {
                    uint8_t curStrokeAlpha = alphaBuf[localIdx];
                    uint8_t newStampAlpha = static_cast<uint8_t>(std::clamp(stampAlpha * 255.0f, 0.0f, 255.0f) + 0.5f);

                    // 単一ストローク内では max(alpha) を保持し、過剰な重なり蓄積（濃度100%化）を防止 (SAI/Photoshop不透明度方式)
                    if (newStampAlpha <= curStrokeAlpha) {
                        continue; // 既に同等以上の濃度で描画済み
                    }

                    alphaBuf[localIdx] = newStampAlpha;

                    // ベースピクセル（ストローク開始前のピクセル）を取得
                    uint32_t basePixel = basePixels ? basePixels[localIdx] : 0;

                    if (m_currentTool == ToolType::Eraser) {
                        uint32_t da_base = (basePixel >> 24) & 0xFF;
                        if (da_base > 0) {
                            float eraseRatio = newStampAlpha / 255.0f;
                            uint32_t newA = static_cast<uint32_t>(da_base * (1.0f - eraseRatio) + 0.5f);
                            dstPixels[localIdx] = (std::min(255u, newA) << 24) | (basePixel & 0x00FFFFFF);
                        } else {
                            dstPixels[localIdx] = 0;
                        }
                    } else {
                        float sa = (newStampAlpha / 255.0f) * brushColorA;
                        uint32_t da_base = (basePixel >> 24) & 0xFF;
                        float daF = da_base / 255.0f;

                        // Porter-Duff Over 合成: (ストローク色 with sa) OVER (ベースピクセル with daF)
                        float outA = sa + daF * (1.0f - sa);
                        if (outA <= 0.0f) {
                            dstPixels[localIdx] = 0;
                            continue;
                        }

                        uint32_t dr = (basePixel >> 16) & 0xFF;
                        uint32_t dg = (basePixel >> 8) & 0xFF;
                        uint32_t db = basePixel & 0xFF;

                        float blendR = (da_base > 0) ? (blendChannel(dr / 255.0f, sr / 255.0f, m_brushBlendMode) * 255.0f) : sr;
                        float blendG = (da_base > 0) ? (blendChannel(dg / 255.0f, sg / 255.0f, m_brushBlendMode) * 255.0f) : sg;
                        float blendB = (da_base > 0) ? (blendChannel(db / 255.0f, sb / 255.0f, m_brushBlendMode) * 255.0f) : sb;

                        uint32_t outR = static_cast<uint32_t>((blendR * sa + dr * daF * (1.0f - sa)) / outA + 0.5f);
                        uint32_t outG = static_cast<uint32_t>((blendG * sa + dg * daF * (1.0f - sa)) / outA + 0.5f);
                        uint32_t outB = static_cast<uint32_t>((blendB * sa + db * daF * (1.0f - sa)) / outA + 0.5f);
                        uint32_t outA8 = static_cast<uint32_t>(outA * 255.0f + 0.5f);

                        if (alphaLocked) {
                            outA8 = da_base;
                        }

                        uint32_t outPixel = (std::min(255u, outA8) << 24) |
                                            (std::min(255u, outR) << 16) |
                                            (std::min(255u, outG) << 8) |
                                            std::min(255u, outB);

                        dstPixels[localIdx] = outPixel;
                    }
                }
            }
        }
    };

    for (int i = 0; i <= steps; ++i) {
        float t = (steps > 0) ? (static_cast<float>(i) / steps) : 0.0f;
        float x = p0.x() + dx * t;
        float y = p0.y() + dy * t;
        float r = (w0 + (w1 - w0) * t) * 0.5f;
        float curDensity = dens0 + (dens1 - dens0) * t;

        if (!isScatter) {
            applyStamp(x, y, r, curDensity);
        } else {
            float baseSize = r * 2.0f;
            float maxDist = baseSize * (m_brushScatterRange / 100.0f);
            float k = m_brushScatterSizeRandom / 100.0f;

            for (int s = 0; s < m_brushScatterAmount; ++s) {
                float angle = static_cast<float>(QRandomGenerator::global()->generateDouble() * 2.0 * 3.14159265358979323846);
                float distOffset = maxDist * static_cast<float>(QRandomGenerator::global()->generateDouble());
                float sx = x + std::cos(angle) * distOffset;
                float sy = y + std::sin(angle) * distOffset;

                float mult = 1.0f;
                if (k > 0.0f) {
                    if (m_brushScatterScaleUp) {
                        mult = 1.0f + k * static_cast<float>(QRandomGenerator::global()->generateDouble() * 2.0 - 1.0);
                    } else {
                        mult = 1.0f - k * static_cast<float>(QRandomGenerator::global()->generateDouble());
                    }
                }
                float sw = std::max(0.5f, baseSize * mult);
                float sr = sw * 0.5f;

                applyStamp(sx, sy, sr, curDensity);
            }
        }
    }

    if (!dirtyRect.isEmpty()) {
        m_strokeDirtyRect = m_strokeDirtyRect.isEmpty() ? dirtyRect : m_strokeDirtyRect.united(dirtyRect);
        activeL->tileManager().markDirtyRect(dirtyRect);
        m_document->composite(dirtyRect);
    }
}

void CanvasWidget::setAirbrushInterval(int ms) {
    m_airbrushInterval = std::max(5, ms);
    if (m_airbrushTimer && m_airbrushTimer->isActive()) {
        m_airbrushTimer->setInterval(m_airbrushInterval);
    }
}

void CanvasWidget::onAirbrushTimerTimeout() {
    if (!m_isDrawing || m_currentTool != ToolType::Airbrush || !m_document) {
        if (m_airbrushTimer && m_airbrushTimer->isActive()) {
            m_airbrushTimer->stop();
        }
        return;
    }
    auto activeL = m_document->activeLayer();
    if (!activeL || activeL->type() != LayerType::Raster || activeL->isLocked()) return;

    float pressure = m_lastFilteredPoint.pressure;
    float pEffSize = m_pressureSizeEnabled ? pressure : 1.0f;
    float pEffDens = m_pressureDensityEnabled ? std::clamp(pressure, 0.05f, 1.0f) : 1.0f;

    float w = m_brushSize * (m_minSizeRatio + (1.0f - m_minSizeRatio) * pEffSize);
    float dens = m_brushDensity * pEffDens;
    uint32_t col = (m_currentTool == ToolType::Eraser) ? 0x00000000 : m_currentColor.rgba();

    paintBrushSegment(m_lastFilteredPoint.pos, m_lastFilteredPoint.pos, w, w, dens, dens, col);
}

void CanvasWidget::paintBlurSegment(const QPointF& p0, const QPointF& p1, float w0, float w1, float dens0, float dens1) {
    auto activeL = effectiveTargetLayer();
    if (!activeL || activeL->type() != LayerType::Raster) return;

    bool hasSelection = m_document && m_document->hasSelection() && !m_document->isSelectionEditMode();
    QRectF selBounds = hasSelection ? m_document->selectionBounds() : QRectF();

    float dx = p1.x() - p0.x();
    float dy = p1.y() - p0.y();
    float dist = std::hypot(dx, dy);

    float minW = std::min(w0, w1);
    float stepDist = std::max(1.0f, minW * 0.25f);
    int steps = std::max(1, static_cast<int>(std::ceil(dist / stepDist)));

    QRect dirtyRect;

    auto getPixel = [&](int px, int py) -> uint32_t {
        if (px < 0 || px >= m_document->width() || py < 0 || py >= m_document->height()) return 0;
        int tx = px / TileMemoryPool::TILE_WIDTH;
        int ty = py / TileMemoryPool::TILE_HEIGHT;
        const Tile* tile = activeL->tileManager().getTile(tx, ty);
        if (!tile || !tile->isValid()) return 0;
        int lx = px % TileMemoryPool::TILE_WIDTH;
        int ly = py % TileMemoryPool::TILE_HEIGHT;
        const uint32_t* ptr = reinterpret_cast<const uint32_t*>(tile->readBuffer());
        return ptr[ly * TileMemoryPool::TILE_WIDTH + lx];
    };

    for (int i = 0; i <= steps; ++i) {
        float t = (steps > 0) ? (static_cast<float>(i) / steps) : 0.0f;
        float x = p0.x() + dx * t;
        float y = p0.y() + dy * t;
        float r = (w0 + (w1 - w0) * t) * 0.5f;
        float curDensity = dens0 + (dens1 - dens0) * t;
        int ir = std::max(1, static_cast<int>(std::ceil(r)));

        int minX = std::max(0, static_cast<int>(std::floor(x - ir)));
        int maxX = std::min(m_document->width() - 1, static_cast<int>(std::ceil(x + ir)));
        int minY = std::max(0, static_cast<int>(std::floor(y - ir)));
        int maxY = std::min(m_document->height() - 1, static_cast<int>(std::ceil(y + ir)));

        if (hasSelection) {
            minX = std::max(minX, static_cast<int>(std::floor(selBounds.left())));
            maxX = std::min(maxX, static_cast<int>(std::ceil(selBounds.right())));
            minY = std::max(minY, static_cast<int>(std::floor(selBounds.top())));
            maxY = std::min(maxY, static_cast<int>(std::ceil(selBounds.bottom())));
        }

        if (minX > maxX || minY > maxY) continue;

        // 1. 周辺領域の平均色サンプリング (距離加重平均)
        int stride = (r > 60.0f) ? 4 : ((r > 30.0f) ? 3 : ((r > 15.0f) ? 2 : 1));
        float sumR = 0.0f, sumG = 0.0f, sumB = 0.0f;
        float sumColorWeight = 0.0f;
        float sumA = 0.0f;
        float sumAlphaWeight = 0.0f;

        for (int sy = minY; sy <= maxY; sy += stride) {
            for (int sx = minX; sx <= maxX; sx += stride) {
                float d = std::hypot(sx - x, sy - y);
                if (d > r) continue;
                float u = (r - d) / r;
                uint32_t pix = getPixel(sx, sy);
                uint32_t a = (pix >> 24) & 0xFF;
                
                sumA += a * u;
                sumAlphaWeight += u;

                if (a > 0) {
                    float w = u * (a / 255.0f);
                    sumR += ((pix >> 16) & 0xFF) * w;
                    sumG += ((pix >> 8) & 0xFF) * w;
                    sumB += (pix & 0xFF) * w;
                    sumColorWeight += w;
                }
            }
        }

        float avgR = 0.0f, avgG = 0.0f, avgB = 0.0f, avgA = 0.0f;
        if (sumColorWeight > 0.0001f) {
            avgR = sumR / sumColorWeight;
            avgG = sumG / sumColorWeight;
            avgB = sumB / sumColorWeight;
        }
        if (sumAlphaWeight > 0.0001f) {
            avgA = sumA / sumAlphaWeight;
        }

        // 2. 色延び (Color Dragging / 0〜100)
        if (!m_hasDragColor) {
            m_dragR = avgR;
            m_dragG = avgG;
            m_dragB = avgB;
            m_dragA = avgA;
            m_hasDragColor = true;
        }

        float stretchRatio = std::clamp(m_blurColorStretch / 100.0f, 0.0f, 1.0f);
        float targetR = avgR * (1.0f - stretchRatio) + m_dragR * stretchRatio;
        float targetG = avgG * (1.0f - stretchRatio) + m_dragG * stretchRatio;
        float targetB = avgB * (1.0f - stretchRatio) + m_dragB * stretchRatio;
        float targetA = avgA * (1.0f - stretchRatio) + m_dragA * stretchRatio;

        // 次のスタンプへドラッグ色を引き継ぐ
        m_dragR = targetR;
        m_dragG = targetG;
        m_dragB = targetB;
        m_dragA = targetA;

        // 3. 混色 (Color Mixing / 0〜100) & ピクセル適用
        float mixRatio = std::clamp(m_blurColorMix / 100.0f, 0.0f, 1.0f);
        if (mixRatio <= 0.0f) {
            // 混色0%の場合は元の色を100%保持（変化なし）
            continue;
        }

        QRect stampRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
        dirtyRect = dirtyRect.isEmpty() ? stampRect : dirtyRect.united(stampRect);

        int lastTx = -1, lastTy = -1;
        uint32_t* dstPixels = nullptr;
        bool alphaLocked = activeL->isAlphaLocked();

        for (int py = minY; py <= maxY; ++py) {
            int ty = py / TileMemoryPool::TILE_HEIGHT;
            int ly = py % TileMemoryPool::TILE_HEIGHT;

            for (int px = minX; px <= maxX; ++px) {
                float d = std::hypot(px - x, py - y);
                if (d > r) continue;

                float selWeight = 1.0f;
                if (hasSelection) {
                    uint8_t sel = m_document->selectionStrength(px, py);
                    if (sel == 0) continue;
                    selWeight = sel / 255.0f;
                }

                int tx = px / TileMemoryPool::TILE_WIDTH;
                int lx = px % TileMemoryPool::TILE_WIDTH;

                if (tx != lastTx || ty != lastTy) {
                    lastTx = tx;
                    lastTy = ty;
                    Tile* dstTile = activeL->tileManager().getTileForWrite(tx, ty);
                    dstPixels = dstTile ? reinterpret_cast<uint32_t*>(dstTile->writeBuffer()) : nullptr;
                }

                if (!dstPixels) continue;

                int localIdx = ly * TileMemoryPool::TILE_WIDTH + lx;
                uint32_t origPixel = dstPixels[localIdx];
                uint32_t origA = (origPixel >> 24) & 0xFF;
                uint32_t origR = (origPixel >> 16) & 0xFF;
                uint32_t origG = (origPixel >> 8) & 0xFF;
                uint32_t origB = origPixel & 0xFF;

                if (origA == 0 && targetA <= 0.001f) {
                    continue;
                }

                // 輪郭の硬さ (0〜100) と ボケ足 (0〜100) によるグラデーション
                float stampAtten = 1.0f;
                if (m_blurHardness < 100 && m_blurFalloff > 0 && r > 0.0f) {
                    float rStart = r * (m_blurHardness / 100.0f);
                    if (d > rStart) {
                        float uGrad = (r > rStart) ? std::min(1.0f, (d - rStart) / (r - rStart)) : 1.0f;
                        float minFade = 1.0f - (m_blurFalloff / 100.0f);
                        stampAtten = 1.0f - (1.0f - minFade) * uGrad;
                    }
                }
                float blendRate = std::clamp(stampAtten * curDensity * mixRatio * 0.4f * selWeight, 0.0f, 1.0f);

                uint32_t newA = alphaLocked ? origA : static_cast<uint32_t>(origA * (1.0f - blendRate) + targetA * blendRate + 0.5f);
                if (newA == 0) {
                    dstPixels[localIdx] = 0;
                    continue;
                }

                uint32_t newR = static_cast<uint32_t>(origR * (1.0f - blendRate) + targetR * blendRate + 0.5f);
                uint32_t newG = static_cast<uint32_t>(origG * (1.0f - blendRate) + targetG * blendRate + 0.5f);
                uint32_t newB = static_cast<uint32_t>(origB * (1.0f - blendRate) + targetB * blendRate + 0.5f);

                dstPixels[localIdx] = (std::min(255u, newA) << 24) |
                                      (std::min(255u, newR) << 16) |
                                      (std::min(255u, newG) << 8)  |
                                      std::min(255u, newB);
            }
        }
    }

    if (!dirtyRect.isEmpty()) {
        m_strokeDirtyRect = m_strokeDirtyRect.isEmpty() ? dirtyRect : m_strokeDirtyRect.united(dirtyRect);
        activeL->tileManager().markDirtyRect(dirtyRect);
        m_document->composite(dirtyRect);
    }
}

void CanvasWidget::paintSmudgeSegment(const QPointF& p0, const QPointF& p1, float w0, float w1, float dens0, float dens1) {
    auto activeL = effectiveTargetLayer();
    if (!activeL || activeL->type() != LayerType::Raster) return;

    bool hasSelection = m_document && m_document->hasSelection() && !m_document->isSelectionEditMode();
    QRectF selBounds = hasSelection ? m_document->selectionBounds() : QRectF();

    float dx = p1.x() - p0.x();
    float dy = p1.y() - p0.y();
    float dist = std::hypot(dx, dy);

    float minW = std::min(w0, w1);
    float stepDist = std::max(1.0f, minW * 0.2f);
    int steps = std::max(1, static_cast<int>(std::ceil(dist / stepDist)));

    QRect dirtyRect;

    auto getPixel = [&](int px, int py) -> uint32_t {
        if (px < 0 || px >= m_document->width() || py < 0 || py >= m_document->height()) return 0;
        int tx = px / TileMemoryPool::TILE_WIDTH;
        int ty = py / TileMemoryPool::TILE_HEIGHT;
        const Tile* tile = activeL->tileManager().getTile(tx, ty);
        if (!tile || !tile->isValid()) return 0;
        int lx = px % TileMemoryPool::TILE_WIDTH;
        int ly = py % TileMemoryPool::TILE_HEIGHT;
        const uint32_t* ptr = reinterpret_cast<const uint32_t*>(tile->readBuffer());
        return ptr[ly * TileMemoryPool::TILE_WIDTH + lx];
    };

    auto sampleAverageColor = [&](float cx, float cy, float cr) -> std::tuple<float, float, float, float, bool> {
        int ir = std::max(1, static_cast<int>(std::ceil(cr)));
        int sMinX = std::max(0, static_cast<int>(std::floor(cx - ir)));
        int sMaxX = std::min(m_document->width() - 1, static_cast<int>(std::ceil(cx + ir)));
        int sMinY = std::max(0, static_cast<int>(std::floor(cy - ir)));
        int sMaxY = std::min(m_document->height() - 1, static_cast<int>(std::ceil(cy + ir)));

        int stride = (cr > 60.0f) ? 4 : ((cr > 30.0f) ? 3 : ((cr > 15.0f) ? 2 : 1));
        float sumR = 0.0f, sumG = 0.0f, sumB = 0.0f;
        float sumWeight = 0.0f;
        float sumA = 0.0f;
        float sumAlphaWeight = 0.0f;

        for (int sy = sMinY; sy <= sMaxY; sy += stride) {
            for (int sx = sMinX; sx <= sMaxX; sx += stride) {
                float d = std::hypot(sx - cx, sy - cy);
                if (d > cr) continue;
                float u = (cr - d) / cr;
                uint32_t pix = getPixel(sx, sy);
                uint32_t a = (pix >> 24) & 0xFF;
                sumA += a * u;
                sumAlphaWeight += u;
                if (a > 0) {
                    float w = u * (a / 255.0f);
                    sumR += ((pix >> 16) & 0xFF) * w;
                    sumG += ((pix >> 8) & 0xFF) * w;
                    sumB += (pix & 0xFF) * w;
                    sumWeight += w;
                }
            }
        }

        if (sumWeight > 0.0001f) {
            float avgR = sumR / sumWeight;
            float avgG = sumG / sumWeight;
            float avgB = sumB / sumWeight;
            float avgA = (sumAlphaWeight > 0.0001f) ? (sumA / sumAlphaWeight) : 255.0f;
            return { avgR, avgG, avgB, avgA, true };
        }
        return { 0.0f, 0.0f, 0.0f, 0.0f, false };
    };

    float rateNorm = std::clamp(m_smudgeRate / 100.0f, 0.01f, 1.0f);

    for (int i = 0; i <= steps; ++i) {
        float t = (steps > 0) ? (static_cast<float>(i) / steps) : 0.0f;
        float x = p0.x() + dx * t;
        float y = p0.y() + dy * t;
        float r = (w0 + (w1 - w0) * t) * 0.5f;
        float curDensity = dens0 + (dens1 - dens0) * t;
        int ir = std::max(1, static_cast<int>(std::ceil(r)));

        int minX = std::max(0, static_cast<int>(std::floor(x - ir)));
        int maxX = std::min(m_document->width() - 1, static_cast<int>(std::ceil(x + ir)));
        int minY = std::max(0, static_cast<int>(std::floor(y - ir)));
        int maxY = std::min(m_document->height() - 1, static_cast<int>(std::ceil(y + ir)));

        if (hasSelection) {
            minX = std::max(minX, static_cast<int>(std::floor(selBounds.left())));
            maxX = std::min(maxX, static_cast<int>(std::ceil(selBounds.right())));
            minY = std::max(minY, static_cast<int>(std::floor(selBounds.top())));
            maxY = std::min(maxY, static_cast<int>(std::ceil(selBounds.bottom())));
        }

        if (minX > maxX || minY > maxY) continue;

        // 1. 指先に色がまだない場合、現在の地点のキャンバスから絵の具をすくい取る
        if (!m_hasSmudgeColor) {
            auto [initR, initG, initB, initA, hasColor] = sampleAverageColor(x, y, r);
            if (hasColor) {
                m_smudgeR = initR;
                m_smudgeG = initG;
                m_smudgeB = initB;
                m_smudgeA = initA;
                m_hasSmudgeColor = true;
            } else {
                m_smudgeR = 0.0f;
                m_smudgeG = 0.0f;
                m_smudgeB = 0.0f;
                m_smudgeA = 0.0f;
            }
        }

        // 2. 指先に乗っている色をキャンバスに塗布 (GIMP Smudge)
        QRect stampRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
        dirtyRect = dirtyRect.isEmpty() ? stampRect : dirtyRect.united(stampRect);

        int lastTx = -1, lastTy = -1;
        uint32_t* dstPixels = nullptr;
        bool alphaLocked = activeL->isAlphaLocked();

        for (int py = minY; py <= maxY; ++py) {
            int ty = py / TileMemoryPool::TILE_HEIGHT;
            int ly = py % TileMemoryPool::TILE_HEIGHT;

            for (int px = minX; px <= maxX; ++px) {
                float d = std::hypot(px - x, py - y);
                if (d > r) continue;

                float selWeight = 1.0f;
                if (hasSelection) {
                    uint8_t sel = m_document->selectionStrength(px, py);
                    if (sel == 0) continue;
                    selWeight = sel / 255.0f;
                }

                int tx = px / TileMemoryPool::TILE_WIDTH;
                int lx = px % TileMemoryPool::TILE_WIDTH;

                if (tx != lastTx || ty != lastTy) {
                    lastTx = tx;
                    lastTy = ty;
                    Tile* dstTile = activeL->tileManager().getTileForWrite(tx, ty);
                    dstPixels = dstTile ? reinterpret_cast<uint32_t*>(dstTile->writeBuffer()) : nullptr;
                }

                if (!dstPixels) continue;

                int localIdx = ly * TileMemoryPool::TILE_WIDTH + lx;
                uint32_t origPixel = dstPixels[localIdx];
                uint32_t origA = (origPixel >> 24) & 0xFF;
                uint32_t origR = (origPixel >> 16) & 0xFF;
                uint32_t origG = (origPixel >> 8) & 0xFF;
                uint32_t origB = origPixel & 0xFF;

                if (origA == 0 && m_smudgeA <= 0.001f) {
                    continue;
                }

                // 輪郭の硬さ (0〜100) と ボケ足 (0〜100) によるグラデーション
                float stampAtten = 1.0f;
                if (m_smudgeHardness < 100 && m_smudgeFalloff > 0 && r > 0.0f) {
                    float rStart = r * (m_smudgeHardness / 100.0f);
                    if (d > rStart) {
                        float uGrad = (r > rStart) ? std::min(1.0f, (d - rStart) / (r - rStart)) : 1.0f;
                        float minFade = 1.0f - (m_smudgeFalloff / 100.0f);
                        stampAtten = 1.0f - (1.0f - minFade) * uGrad;
                    }
                }
                float blendRate = std::clamp(stampAtten * curDensity * rateNorm * 0.45f * selWeight, 0.0f, 1.0f);

                uint32_t newA = alphaLocked ? origA : static_cast<uint32_t>(origA * (1.0f - blendRate) + m_smudgeA * blendRate + 0.5f);
                if (newA == 0) {
                    dstPixels[localIdx] = 0;
                    continue;
                }

                uint32_t newR = static_cast<uint32_t>(origR * (1.0f - blendRate) + m_smudgeR * blendRate + 0.5f);
                uint32_t newG = static_cast<uint32_t>(origG * (1.0f - blendRate) + m_smudgeG * blendRate + 0.5f);
                uint32_t newB = static_cast<uint32_t>(origB * (1.0f - blendRate) + m_smudgeB * blendRate + 0.5f);

                dstPixels[localIdx] = (std::min(255u, newA) << 24) |
                                      (std::min(255u, newR) << 16) |
                                      (std::min(255u, newG) << 8)  |
                                      std::min(255u, newB);
            }
        }

        // 3. 移動先から指先への絵の具の付着・更新 (GIMP Smudge Pick-up)
        // Rateが高いほど移動先の絵の具を取り込まず、最初の色が長く伸びる
        // Rateが低いほど移動先の絵の具を取り込んで、すぐに移動先の色に置き換わる
        float pickupRatio = std::clamp((1.0f - rateNorm) * 0.35f, 0.0f, 1.0f);
        if (pickupRatio > 0.001f) {
            auto [curR, curG, curB, curA, hasColor] = sampleAverageColor(x, y, r);
            if (hasColor) {
                m_smudgeR = m_smudgeR * (1.0f - pickupRatio) + curR * pickupRatio;
                m_smudgeG = m_smudgeG * (1.0f - pickupRatio) + curG * pickupRatio;
                m_smudgeB = m_smudgeB * (1.0f - pickupRatio) + curB * pickupRatio;
                m_smudgeA = m_smudgeA * (1.0f - pickupRatio) + curA * pickupRatio;
                m_hasSmudgeColor = true;
            }
        }
    }

    if (!dirtyRect.isEmpty()) {
        m_strokeDirtyRect = m_strokeDirtyRect.isEmpty() ? dirtyRect : m_strokeDirtyRect.united(dirtyRect);
        activeL->tileManager().markDirtyRect(dirtyRect);
        m_document->composite(dirtyRect);
    }
}

void CanvasWidget::paintWatercolorSegment(const QPointF& p0, const QPointF& p1, float w0, float w1, float dens0, float dens1, float p0Raw, float p1Raw) {
    if (!m_document) return;
    auto activeL = effectiveTargetLayer();
    if (!activeL || activeL->type() != LayerType::Raster) return;

    bool hasSelection = m_document && m_document->hasSelection() && !m_document->isSelectionEditMode();
    QRectF selBounds = hasSelection ? m_document->selectionBounds() : QRectF();

    float dx = p1.x() - p0.x();
    float dy = p1.y() - p0.y();
    float dist = std::hypot(dx, dy);

    float minW = std::min(w0, w1);
    float stepDist = std::max(1.0f, minW * 0.25f);
    int steps = std::max(1, static_cast<int>(std::ceil(dist / stepDist)));

    QRect dirtyRect;

    auto getPixel = [&](int px, int py) -> uint32_t {
        if (px < 0 || px >= m_document->width() || py < 0 || py >= m_document->height()) return 0;
        int tx = px / TileMemoryPool::TILE_WIDTH;
        int ty = py / TileMemoryPool::TILE_HEIGHT;
        const Tile* tile = activeL->tileManager().getTile(tx, ty);
        if (!tile || !tile->isValid()) return 0;
        int lx = px % TileMemoryPool::TILE_WIDTH;
        int ly = py % TileMemoryPool::TILE_HEIGHT;
        const uint32_t* ptr = reinterpret_cast<const uint32_t*>(tile->readBuffer());
        return ptr[ly * TileMemoryPool::TILE_WIDTH + lx];
    };

    auto sampleAverageColor = [&](float cx, float cy, float cr) -> std::tuple<float, float, float, float, bool> {
        int ir = std::max(1, static_cast<int>(std::ceil(cr)));
        int sMinX = std::max(0, static_cast<int>(std::floor(cx - ir)));
        int sMaxX = std::min(m_document->width() - 1, static_cast<int>(std::ceil(cx + ir)));
        int sMinY = std::max(0, static_cast<int>(std::floor(cy - ir)));
        int sMaxY = std::min(m_document->height() - 1, static_cast<int>(std::ceil(cy + ir)));

        int stride = (cr > 60.0f) ? 4 : ((cr > 30.0f) ? 3 : ((cr > 15.0f) ? 2 : 1));
        float sumR = 0.0f, sumG = 0.0f, sumB = 0.0f;
        float sumWeight = 0.0f;
        float sumA = 0.0f;
        float sumAlphaWeight = 0.0f;

        for (int sy = sMinY; sy <= sMaxY; sy += stride) {
            for (int sx = sMinX; sx <= sMaxX; sx += stride) {
                float d = std::hypot(sx - cx, sy - cy);
                if (d > cr) continue;
                float u = (cr - d) / cr;
                uint32_t pix = getPixel(sx, sy);
                uint32_t a = (pix >> 24) & 0xFF;
                sumA += a * u;
                sumAlphaWeight += u;
                if (a > 0) {
                    float w = u * (a / 255.0f);
                    sumR += ((pix >> 16) & 0xFF) * w;
                    sumG += ((pix >> 8) & 0xFF) * w;
                    sumB += (pix & 0xFF) * w;
                    sumWeight += w;
                }
            }
        }

        if (sumWeight > 0.0001f) {
            float avgR = sumR / sumWeight;
            float avgG = sumG / sumWeight;
            float avgB = sumB / sumWeight;
            float avgA = (sumAlphaWeight > 0.0001f) ? (sumA / sumAlphaWeight) : 255.0f;
            return { avgR, avgG, avgB, avgA, true };
        }
        return { 0.0f, 0.0f, 0.0f, 0.0f, false };
    };

    float fgR = m_currentColor.redF() * 255.0f;
    float fgG = m_currentColor.greenF() * 255.0f;
    float fgB = m_currentColor.blueF() * 255.0f;
    float fgA = m_currentColor.alphaF() * 255.0f;

    float stretch = std::clamp(m_watercolorColorStretch / 100.0f, 0.0f, 1.0f);
    float waterFactor = std::clamp(1.0f - (m_watercolorWater / 100.0f), 0.0f, 1.0f);
    float blurPressureThresh = m_watercolorBlurPressure / 100.0f;

    bool isScatter = m_watercolorScatter;

    auto applyStamp = [&](float x, float y, float r, float curDensity, float curP) {
        int ir = std::max(1, static_cast<int>(std::ceil(r)));

        int minX = std::max(0, static_cast<int>(std::floor(x - ir)));
        int maxX = std::min(m_document->width() - 1, static_cast<int>(std::ceil(x + ir)));
        int minY = std::max(0, static_cast<int>(std::floor(y - ir)));
        int maxY = std::min(m_document->height() - 1, static_cast<int>(std::ceil(y + ir)));

        if (hasSelection) {
            minX = std::max(minX, static_cast<int>(std::floor(selBounds.left())));
            maxX = std::min(maxX, static_cast<int>(std::ceil(selBounds.right())));
            minY = std::max(minY, static_cast<int>(std::floor(selBounds.top())));
            maxY = std::min(maxY, static_cast<int>(std::ceil(selBounds.bottom())));
        }

        if (minX > maxX || minY > maxY) return;

        // 1. 周辺色サンプリング (レイヤー上の既存色)
        auto [sampR, sampG, sampB, sampA, hasSample] = sampleAverageColor(x, y, r);

        // 2. 筆強度とぼかし筆圧の判定
        // 筆強度 0〜100: 描画色と既にレイヤーに存在する色の比率 (0:全てレイヤー色, 100:全て描画色)
        // ぼかし筆圧 0〜100: 設定された筆圧以下の場合は筆強度の設定値を無視して既にレイヤーに存在する色を適用する
        float effStrength = 0.0f;
        if (curP <= blurPressureThresh) {
            effStrength = 0.0f; // 既存色を100%適用
        } else {
            effStrength = std::clamp(m_watercolorStrength / 100.0f, 0.0f, 1.0f);
        }

        // 目標色 (targetColor): 描画色と既存色のブレンド
        // ※ もしレイヤーに既存色がない（透明）場合は描画色のみを使用
        float targetR = fgR, targetG = fgG, targetB = fgB, targetA = fgA;
        if (hasSample && sampA > 0.001f) {
            targetR = fgR * effStrength + sampR * (1.0f - effStrength);
            targetG = fgG * effStrength + sampG * (1.0f - effStrength);
            targetB = fgB * effStrength + sampB * (1.0f - effStrength);
            targetA = fgA * effStrength + sampA * (1.0f - effStrength);
        }

        // 3. 色延び (Color Stretch): ブラシ移動前の色を反映
        if (!m_hasWatercolorColor) {
            m_wcR = targetR;
            m_wcG = targetG;
            m_wcB = targetB;
            m_wcA = targetA;
            m_hasWatercolorColor = true;
        } else {
            m_wcR = m_wcR * stretch + targetR * (1.0f - stretch);
            m_wcG = m_wcG * stretch + targetG * (1.0f - stretch);
            m_wcB = m_wcB * stretch + targetB * (1.0f - stretch);
            m_wcA = m_wcA * stretch + targetA * (1.0f - stretch);
        }

        // 4. キャンバスへの描画 (スタンプ処理)
        QRect stampRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
        dirtyRect = dirtyRect.isEmpty() ? stampRect : dirtyRect.united(stampRect);

        int lastTx = -1, lastTy = -1;
        uint32_t* dstPixels = nullptr;
        bool alphaLocked = activeL->isAlphaLocked();

        for (int py = minY; py <= maxY; ++py) {
            int ty = py / TileMemoryPool::TILE_HEIGHT;
            int ly = py % TileMemoryPool::TILE_HEIGHT;

            for (int px = minX; px <= maxX; ++px) {
                float d = std::hypot(px - x, py - y);
                if (d > r) continue;

                float selWeight = 1.0f;
                if (hasSelection) {
                    uint8_t sel = m_document->selectionStrength(px, py);
                    if (sel == 0) continue;
                    selWeight = sel / 255.0f;
                }

                // アンチエイリアス処理
                float stampAlpha = curDensity * selWeight;
                if (m_watercolorAntialias) {
                    float edgeDist = r - d;
                    if (edgeDist < 1.0f) {
                        stampAlpha *= std::clamp(edgeDist, 0.0f, 1.0f);
                    }
                }
                if (stampAlpha <= 0.0f) continue;

                // 輪郭の硬さ (0〜100) と ボケ足 (0〜100) によるグラデーション
                if (m_watercolorHardness < 100 && m_watercolorFalloff > 0 && r > 0.0f) {
                    float rStart = r * (m_watercolorHardness / 100.0f);
                    if (d > rStart) {
                        float uGrad = (r > rStart) ? std::min(1.0f, (d - rStart) / (r - rStart)) : 1.0f;
                        float minFade = 1.0f - (m_watercolorFalloff / 100.0f);
                        float fade = 1.0f - (1.0f - minFade) * uGrad;
                        stampAlpha *= fade;
                    }
                }
                if (stampAlpha <= 0.0f) continue;

                int tx = px / TileMemoryPool::TILE_WIDTH;
                int lx = px % TileMemoryPool::TILE_WIDTH;

                if (tx != lastTx || ty != lastTy) {
                    lastTx = tx;
                    lastTy = ty;
                    Tile* dstTile = activeL->tileManager().getTileForWrite(tx, ty);
                    dstPixels = dstTile ? reinterpret_cast<uint32_t*>(dstTile->writeBuffer()) : nullptr;
                }

                if (!dstPixels) continue;

                int localIdx = ly * TileMemoryPool::TILE_WIDTH + lx;
                uint32_t origPixel = dstPixels[localIdx];
                uint32_t origA = (origPixel >> 24) & 0xFF;
                uint32_t origR = (origPixel >> 16) & 0xFF;
                uint32_t origG = (origPixel >> 8) & 0xFF;
                uint32_t origB = origPixel & 0xFF;

                // 水分量 0〜100: 不透明度がどれ程上がるか (0:最も上がりやすい, 100:全く上がらない)
                // stampAlpha によるアルファ上昇量を waterFactor でスケーリング
                // water == 100 (waterFactor == 0) のときは deltaA = 0 で origA から全く上がらない
                float deltaA = (255.0f - origA) * (stampAlpha * 0.35f) * waterFactor;
                float newA_f = origA + deltaA;
                uint32_t newA = alphaLocked ? origA : static_cast<uint32_t>(std::clamp(newA_f, 0.0f, 255.0f) + 0.5f);

                if (newA == 0) {
                    dstPixels[localIdx] = 0;
                    continue;
                }

                // 色の混合: ブラシ色 (m_wc*) と 既存ピクセル色 (origRGB) のブレンド
                float blendRate = std::clamp(stampAlpha * 0.35f, 0.0f, 1.0f);
                if (origA == 0) {
                    // 下地が透明だった場合はブラシ色をそのまま配置
                    dstPixels[localIdx] = (std::min(255u, newA) << 24) |
                                          (std::min(255u, static_cast<uint32_t>(m_wcR + 0.5f)) << 16) |
                                          (std::min(255u, static_cast<uint32_t>(m_wcG + 0.5f)) << 8) |
                                          std::min(255u, static_cast<uint32_t>(m_wcB + 0.5f));
                } else {
                    float blendR = blendChannel(origR / 255.0f, m_wcR / 255.0f, m_watercolorBlendMode) * 255.0f;
                    float blendG = blendChannel(origG / 255.0f, m_wcG / 255.0f, m_watercolorBlendMode) * 255.0f;
                    float blendB = blendChannel(origB / 255.0f, m_wcB / 255.0f, m_watercolorBlendMode) * 255.0f;

                    uint32_t newR = static_cast<uint32_t>(origR * (1.0f - blendRate) + blendR * blendRate + 0.5f);
                    uint32_t newG = static_cast<uint32_t>(origG * (1.0f - blendRate) + blendG * blendRate + 0.5f);
                    uint32_t newB = static_cast<uint32_t>(origB * (1.0f - blendRate) + blendB * blendRate + 0.5f);

                    dstPixels[localIdx] = (std::min(255u, newA) << 24) |
                                          (std::min(255u, newR) << 16) |
                                          (std::min(255u, newG) << 8) |
                                          std::min(255u, newB);
                }
            }
        }
    };

    for (int i = 0; i <= steps; ++i) {
        float t = (steps > 0) ? (static_cast<float>(i) / steps) : 0.0f;
        float x = p0.x() + dx * t;
        float y = p0.y() + dy * t;
        float r = (w0 + (w1 - w0) * t) * 0.5f;
        float curDensity = dens0 + (dens1 - dens0) * t;
        float curP = p0Raw + (p1Raw - p0Raw) * t;

        if (!isScatter) {
            applyStamp(x, y, r, curDensity, curP);
        } else {
            float baseSize = r * 2.0f;
            float maxDist = baseSize * (m_watercolorScatterRange / 100.0f);
            float k = m_watercolorScatterSizeRandom / 100.0f;

            for (int s = 0; s < m_watercolorScatterAmount; ++s) {
                float angle = static_cast<float>(QRandomGenerator::global()->generateDouble() * 2.0 * 3.14159265358979323846);
                float distOffset = maxDist * static_cast<float>(QRandomGenerator::global()->generateDouble());
                float sx = x + std::cos(angle) * distOffset;
                float sy = y + std::sin(angle) * distOffset;

                float mult = 1.0f;
                if (k > 0.0f) {
                    if (m_watercolorScatterScaleUp) {
                        mult = 1.0f + k * static_cast<float>(QRandomGenerator::global()->generateDouble() * 2.0 - 1.0);
                    } else {
                        mult = 1.0f - k * static_cast<float>(QRandomGenerator::global()->generateDouble());
                    }
                }
                float sw = std::max(0.5f, baseSize * mult);
                float sr = sw * 0.5f;

                applyStamp(sx, sy, sr, curDensity, curP);
            }
        }
    }

    if (!dirtyRect.isEmpty()) {
        m_strokeDirtyRect = m_strokeDirtyRect.isEmpty() ? dirtyRect : m_strokeDirtyRect.united(dirtyRect);
        activeL->tileManager().markDirtyRect(dirtyRect);
        m_document->composite(dirtyRect);
    }
}

void CanvasWidget::paintCompositeBrushSegment(const QPointF& p0, const QPointF& p1, float w0, float w1, float dens0, float dens1, float p0Raw, float p1Raw) {
    if (!m_document) return;
    auto activeL = effectiveTargetLayer();
    if (!activeL || activeL->type() != LayerType::Raster) return;

    bool hasSelection = m_document && m_document->hasSelection() && !m_document->isSelectionEditMode();
    QRectF selBounds = hasSelection ? m_document->selectionBounds() : QRectF();

    float dx = p1.x() - p0.x();
    float dy = p1.y() - p0.y();
    float dist = std::hypot(dx, dy);

    float minW = std::min(w0, w1);
    float stepDist = std::max(1.0f, minW * 0.25f);
    int steps = std::max(1, static_cast<int>(std::ceil(dist / stepDist)));

    QRect dirtyRect;

    // ひとつ下のレイヤーの取得（LayerBelow時用）
    int activeIdx = m_document->activeLayerIndex();
    std::shared_ptr<Layer> belowLayer = nullptr;
    if (m_compositeBrushReference == CompositeBrushReference::LayerBelow) {
        for (int i = activeIdx - 1; i >= 0; --i) {
            auto l = m_document->getLayer(i);
            if (l && l->isVisible()) {
                belowLayer = l;
                break;
            }
        }
    }

    // 対象ピクセルの取得
    auto getTargetPixel = [&](int px, int py) -> uint32_t {
        if (px < 0 || px >= m_document->width() || py < 0 || py >= m_document->height()) return 0;
        int tx = px / TileMemoryPool::TILE_WIDTH;
        int ty = py / TileMemoryPool::TILE_HEIGHT;
        if (m_compositeBrushReference == CompositeBrushReference::Canvas) {
            const Tile* tile = m_document->compositeBuffer().getTile(tx, ty);
            if (!tile || !tile->isValid()) return 0;
            int lx = px % TileMemoryPool::TILE_WIDTH;
            int ly = py % TileMemoryPool::TILE_HEIGHT;
            const uint32_t* ptr = reinterpret_cast<const uint32_t*>(tile->readBuffer());
            return ptr[ly * TileMemoryPool::TILE_WIDTH + lx];
        } else {
            // LayerBelow
            if (!belowLayer) return 0;
            const Tile* tile = belowLayer->tileManager().getTile(tx, ty);
            if (!tile || !tile->isValid()) return 0;
            int lx = px % TileMemoryPool::TILE_WIDTH;
            int ly = py % TileMemoryPool::TILE_HEIGHT;
            const uint32_t* ptr = reinterpret_cast<const uint32_t*>(tile->readBuffer());
            return ptr[ly * TileMemoryPool::TILE_WIDTH + lx];
        }
    };

    // アクティブレイヤーピクセルの取得
    auto getActivePixel = [&](int px, int py) -> uint32_t {
        if (px < 0 || px >= m_document->width() || py < 0 || py >= m_document->height()) return 0;
        int tx = px / TileMemoryPool::TILE_WIDTH;
        int ty = py / TileMemoryPool::TILE_HEIGHT;
        const Tile* tile = activeL->tileManager().getTile(tx, ty);
        if (!tile || !tile->isValid()) return 0;
        int lx = px % TileMemoryPool::TILE_WIDTH;
        int ly = py % TileMemoryPool::TILE_HEIGHT;
        const uint32_t* ptr = reinterpret_cast<const uint32_t*>(tile->readBuffer());
        return ptr[ly * TileMemoryPool::TILE_WIDTH + lx];
    };

    // 周辺色サンプリング（筆強度・混色用）:
    // アクティブレイヤーに描画色があればアクティブレイヤーから、なければ対象（キャンバスまたは下レイヤー）からサンプリング
    auto sampleAverageColor = [&](float cx, float cy, float cr) -> std::tuple<float, float, float, float, bool> {
        int ir = std::max(1, static_cast<int>(std::ceil(cr)));
        int sMinX = std::max(0, static_cast<int>(std::floor(cx - ir)));
        int sMaxX = std::min(m_document->width() - 1, static_cast<int>(std::ceil(cx + ir)));
        int sMinY = std::max(0, static_cast<int>(std::floor(cy - ir)));
        int sMaxY = std::min(m_document->height() - 1, static_cast<int>(std::ceil(cy + ir)));

        int stride = (cr > 60.0f) ? 4 : ((cr > 30.0f) ? 3 : ((cr > 15.0f) ? 2 : 1));
        float sumR = 0.0f, sumG = 0.0f, sumB = 0.0f;
        float sumWeight = 0.0f;
        float sumA = 0.0f;
        float sumAlphaWeight = 0.0f;

        for (int sy = sMinY; sy <= sMaxY; sy += stride) {
            for (int sx = sMinX; sx <= sMaxX; sx += stride) {
                float d = std::hypot(sx - cx, sy - cy);
                if (d > cr) continue;
                float u = (cr - d) / cr;

                uint32_t pix = getActivePixel(sx, sy);
                if (((pix >> 24) & 0xFF) == 0) {
                    pix = getTargetPixel(sx, sy);
                }

                uint32_t a = (pix >> 24) & 0xFF;
                sumA += a * u;
                sumAlphaWeight += u;
                if (a > 0) {
                    float w = u * (a / 255.0f);
                    sumR += ((pix >> 16) & 0xFF) * w;
                    sumG += ((pix >> 8) & 0xFF) * w;
                    sumB += (pix & 0xFF) * w;
                    sumWeight += w;
                }
            }
        }

        if (sumWeight > 0.0001f) {
            float avgR = sumR / sumWeight;
            float avgG = sumG / sumWeight;
            float avgB = sumB / sumWeight;
            float avgA = (sumAlphaWeight > 0.0001f) ? (sumA / sumAlphaWeight) : 255.0f;
            return { avgR, avgG, avgB, avgA, true };
        }
        return { 0.0f, 0.0f, 0.0f, 0.0f, false };
    };

    float fgR = m_currentColor.redF() * 255.0f;
    float fgG = m_currentColor.greenF() * 255.0f;
    float fgB = m_currentColor.blueF() * 255.0f;
    float fgA = m_currentColor.alphaF() * 255.0f;

    float stretch = std::clamp(m_compositeBrushColorStretch / 100.0f, 0.0f, 1.0f);
    float waterFactor = std::clamp(1.0f - (m_compositeBrushWater / 100.0f), 0.0f, 1.0f);
    float blurPressureThresh = m_compositeBrushBlurPressure / 100.0f;

    bool isScatter = m_compositeBrushScatter;

    auto applyStamp = [&](float x, float y, float r, float curDensity, float curP) {
        int ir = std::max(1, static_cast<int>(std::ceil(r)));

        int minX = std::max(0, static_cast<int>(std::floor(x - ir)));
        int maxX = std::min(m_document->width() - 1, static_cast<int>(std::ceil(x + ir)));
        int minY = std::max(0, static_cast<int>(std::floor(y - ir)));
        int maxY = std::min(m_document->height() - 1, static_cast<int>(std::ceil(y + ir)));

        if (hasSelection) {
            minX = std::max(minX, static_cast<int>(std::floor(selBounds.left())));
            maxX = std::min(maxX, static_cast<int>(std::ceil(selBounds.right())));
            minY = std::max(minY, static_cast<int>(std::floor(selBounds.top())));
            maxY = std::min(maxY, static_cast<int>(std::ceil(selBounds.bottom())));
        }

        if (minX > maxX || minY > maxY) return;

        // 1. 周辺色サンプリング
        auto [sampR, sampG, sampB, sampA, hasSample] = sampleAverageColor(x, y, r);

        // 2. 筆強度とぼかし筆圧の判定
        float effStrength = 0.0f;
        if (curP <= blurPressureThresh) {
            effStrength = 0.0f;
        } else {
            effStrength = std::clamp(m_compositeBrushStrength / 100.0f, 0.0f, 1.0f);
        }

        // 目標色 (targetColor): 描画色と既存色のブレンド
        float targetR = fgR, targetG = fgG, targetB = fgB, targetA = fgA;
        if (hasSample && sampA > 0.001f) {
            targetR = fgR * effStrength + sampR * (1.0f - effStrength);
            targetG = fgG * effStrength + sampG * (1.0f - effStrength);
            targetB = fgB * effStrength + sampB * (1.0f - effStrength);
            targetA = fgA * effStrength + sampA * (1.0f - effStrength);
        }

        // 3. 色延び (Color Stretch): ブラシ移動前の色を反映
        if (!m_hasCompositeBrushColor) {
            m_compWcR = targetR;
            m_compWcG = targetG;
            m_compWcB = targetB;
            m_compWcA = targetA;
            m_hasCompositeBrushColor = true;
        } else {
            m_compWcR = m_compWcR * stretch + targetR * (1.0f - stretch);
            m_compWcG = m_compWcG * stretch + targetG * (1.0f - stretch);
            m_compWcB = m_compWcB * stretch + targetB * (1.0f - stretch);
            m_compWcA = m_compWcA * stretch + targetA * (1.0f - stretch);
        }

        // 4. キャンバスへの描画 (スタンプ処理)
        QRect stampRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
        dirtyRect = dirtyRect.isEmpty() ? stampRect : dirtyRect.united(stampRect);

        int lastTx = -1, lastTy = -1;
        uint32_t* dstPixels = nullptr;
        bool alphaLocked = activeL->isAlphaLocked();

        for (int py = minY; py <= maxY; ++py) {
            int ty = py / TileMemoryPool::TILE_HEIGHT;
            int ly = py % TileMemoryPool::TILE_HEIGHT;

            for (int px = minX; px <= maxX; ++px) {
                float d = std::hypot(px - x, py - y);
                if (d > r) continue;

                float selWeight = 1.0f;
                if (hasSelection) {
                    uint8_t sel = m_document->selectionStrength(px, py);
                    if (sel == 0) continue;
                    selWeight = sel / 255.0f;
                }

                // アンチエイリアス処理
                float stampAlpha = curDensity * selWeight;
                if (m_compositeBrushAntialias) {
                    float edgeDist = r - d;
                    if (edgeDist < 1.0f) {
                        stampAlpha *= std::clamp(edgeDist, 0.0f, 1.0f);
                    }
                }
                if (stampAlpha <= 0.0f) continue;

                // 輪郭の硬さ (0〜100) と ボケ足 (0〜100) によるグラデーション
                if (m_compositeBrushHardness < 100 && m_compositeBrushFalloff > 0 && r > 0.0f) {
                    float rStart = r * (m_compositeBrushHardness / 100.0f);
                    if (d > rStart) {
                        float uGrad = (r > rStart) ? std::min(1.0f, (d - rStart) / (r - rStart)) : 1.0f;
                        float minFade = 1.0f - (m_compositeBrushFalloff / 100.0f);
                        float fade = 1.0f - (1.0f - minFade) * uGrad;
                        stampAlpha *= fade;
                    }
                }
                if (stampAlpha <= 0.0f) continue;

                int tx = px / TileMemoryPool::TILE_WIDTH;
                int lx = px % TileMemoryPool::TILE_WIDTH;

                if (tx != lastTx || ty != lastTy) {
                    lastTx = tx;
                    lastTy = ty;
                    Tile* dstTile = activeL->tileManager().getTileForWrite(tx, ty);
                    dstPixels = dstTile ? reinterpret_cast<uint32_t*>(dstTile->writeBuffer()) : nullptr;
                }

                if (!dstPixels) continue;

                int localIdx = ly * TileMemoryPool::TILE_WIDTH + lx;
                uint32_t origPixel = dstPixels[localIdx];
                uint32_t origA = (origPixel >> 24) & 0xFF;
                uint32_t origR = (origPixel >> 16) & 0xFF;
                uint32_t origG = (origPixel >> 8) & 0xFF;
                uint32_t origB = origPixel & 0xFF;

                // 対象（キャンバス or ひとつ下のレイヤー）のピクセルを取得
                uint32_t targetPixel = getTargetPixel(px, py);
                uint32_t targetA = (targetPixel >> 24) & 0xFF;
                uint32_t targetR = (targetPixel >> 16) & 0xFF;
                uint32_t targetG = (targetPixel >> 8) & 0xFF;
                uint32_t targetB = targetPixel & 0xFF;

                // 対象にツールの合成モードで合成された結果をシミュレート
                float simR = m_compWcR;
                float simG = m_compWcG;
                float simB = m_compWcB;
                if (targetA > 0) {
                    simR = blendChannel(targetR / 255.0f, m_compWcR / 255.0f, m_compositeBrushBlendMode) * 255.0f;
                    simG = blendChannel(targetG / 255.0f, m_compWcG / 255.0f, m_compositeBrushBlendMode) * 255.0f;
                    simB = blendChannel(targetB / 255.0f, m_compWcB / 255.0f, m_compositeBrushBlendMode) * 255.0f;
                }

                // 水分量 0〜100: 不透明度がどれ程上がるか (0:最も上がりやすい, 100:全く上がらない)
                float deltaA = (255.0f - origA) * (stampAlpha * 0.35f) * waterFactor;
                float newA_f = origA + deltaA;
                uint32_t newA = alphaLocked ? origA : static_cast<uint32_t>(std::clamp(newA_f, 0.0f, 255.0f) + 0.5f);

                if (newA == 0) {
                    dstPixels[localIdx] = 0;
                    continue;
                }

                // 色の混合: シミュレート色 (simRGB) をアクティブレイヤーに反映
                float blendRate = std::clamp(stampAlpha * 0.35f, 0.0f, 1.0f);
                if (origA == 0) {
                    dstPixels[localIdx] = (std::min(255u, newA) << 24) |
                                          (std::min(255u, static_cast<uint32_t>(simR + 0.5f)) << 16) |
                                          (std::min(255u, static_cast<uint32_t>(simG + 0.5f)) << 8) |
                                          std::min(255u, static_cast<uint32_t>(simB + 0.5f));
                } else {
                    uint32_t newR = static_cast<uint32_t>(origR * (1.0f - blendRate) + simR * blendRate + 0.5f);
                    uint32_t newG = static_cast<uint32_t>(origG * (1.0f - blendRate) + simG * blendRate + 0.5f);
                    uint32_t newB = static_cast<uint32_t>(origB * (1.0f - blendRate) + simB * blendRate + 0.5f);

                    dstPixels[localIdx] = (std::min(255u, newA) << 24) |
                                          (std::min(255u, newR) << 16) |
                                          (std::min(255u, newG) << 8) |
                                          std::min(255u, newB);
                }
            }
        }
    };

    for (int i = 0; i <= steps; ++i) {
        float t = (steps > 0) ? (static_cast<float>(i) / steps) : 0.0f;
        float x = p0.x() + dx * t;
        float y = p0.y() + dy * t;
        float r = (w0 + (w1 - w0) * t) * 0.5f;
        float curDensity = dens0 + (dens1 - dens0) * t;
        float curP = p0Raw + (p1Raw - p0Raw) * t;

        if (!isScatter) {
            applyStamp(x, y, r, curDensity, curP);
        } else {
            float baseSize = r * 2.0f;
            float maxDist = baseSize * (m_compositeBrushScatterRange / 100.0f);
            float k = m_compositeBrushScatterSizeRandom / 100.0f;

            for (int s = 0; s < m_compositeBrushScatterAmount; ++s) {
                float angle = static_cast<float>(QRandomGenerator::global()->generateDouble() * 2.0 * 3.14159265358979323846);
                float distOffset = maxDist * static_cast<float>(QRandomGenerator::global()->generateDouble());
                float sx = x + std::cos(angle) * distOffset;
                float sy = y + std::sin(angle) * distOffset;

                float mult = 1.0f;
                if (k > 0.0f) {
                    if (m_compositeBrushScatterScaleUp) {
                        mult = 1.0f + k * static_cast<float>(QRandomGenerator::global()->generateDouble() * 2.0 - 1.0);
                    } else {
                        mult = 1.0f - k * static_cast<float>(QRandomGenerator::global()->generateDouble());
                    }
                }
                float sw = std::max(0.5f, baseSize * mult);
                float sr = sw * 0.5f;

                applyStamp(sx, sy, sr, curDensity, curP);
            }
        }
    }

    if (!dirtyRect.isEmpty()) {
        m_strokeDirtyRect = m_strokeDirtyRect.isEmpty() ? dirtyRect : m_strokeDirtyRect.united(dirtyRect);
        activeL->tileManager().markDirtyRect(dirtyRect);
        m_document->composite(dirtyRect);
    }
}

namespace {

enum class BrushImageType {
    FullColor,
    Grayscale,
    GrayscaleAlpha
};

static BrushImageType detectBrushImageType(const QImage& img) {
    if (img.isNull()) return BrushImageType::FullColor;

    QImage argb = (img.format() == QImage::Format_ARGB32 || img.format() == QImage::Format_ARGB32_Premultiplied)
                  ? img : img.convertToFormat(QImage::Format_ARGB32);

    bool hasNonGray = false;
    bool hasAlpha = false;

    int w = argb.width();
    int h = argb.height();
    for (int y = 0; y < h; ++y) {
        const uint32_t* line = reinterpret_cast<const uint32_t*>(argb.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            uint32_t p = line[x];
            uint32_t a = (p >> 24) & 0xFF;
            if (a > 0) {
                uint32_t r = (p >> 16) & 0xFF;
                uint32_t g = (p >> 8) & 0xFF;
                uint32_t b = p & 0xFF;
                if (r != g || g != b) {
                    hasNonGray = true;
                    break;
                }
            }
            if (a < 255) {
                hasAlpha = true;
            }
        }
        if (hasNonGray) break;
    }

    if (hasNonGray) {
        return BrushImageType::FullColor;
    }
    if (hasAlpha) {
        return BrushImageType::GrayscaleAlpha;
    }
    return BrushImageType::Grayscale;
}

static QImage colorizeBrushImage(const QImage& src, BrushImageType type, const QColor& fg, const QColor& bg) {
    if (src.isNull()) return src;
    QImage argb = src.convertToFormat(QImage::Format_ARGB32);
    int w = argb.width();
    int h = argb.height();

    if (type == BrushImageType::FullColor) {
        return argb;
    }

    QImage result(w, h, QImage::Format_ARGB32);
    uint32_t fgR = fg.red();
    uint32_t fgG = fg.green();
    uint32_t fgB = fg.blue();
    uint32_t bgR = bg.red();
    uint32_t bgG = bg.green();
    uint32_t bgB = bg.blue();

    for (int y = 0; y < h; ++y) {
        const uint32_t* srcLine = reinterpret_cast<const uint32_t*>(argb.constScanLine(y));
        uint32_t* dstLine = reinterpret_cast<uint32_t*>(result.scanLine(y));
        for (int x = 0; x < w; ++x) {
            uint32_t p = srcLine[x];
            uint32_t origA = (p >> 24) & 0xFF;
            uint32_t gray = (p >> 16) & 0xFF;

            if (type == BrushImageType::Grayscale) {
                // 黒(0)を不透明、白(255)を透明として描画色でブラシにする
                uint32_t a = static_cast<uint32_t>((255 - gray) * (origA / 255.0f) + 0.5f);
                dstLine[x] = (a << 24) | (fgR << 16) | (fgG << 8) | fgB;
            } else {
                // GrayscaleAlpha: 黒を描画色、白を背景色としてブラシにする
                float t = gray / 255.0f;
                uint32_t r = static_cast<uint32_t>(fgR * (1.0f - t) + bgR * t + 0.5f);
                uint32_t g = static_cast<uint32_t>(fgG * (1.0f - t) + bgG * t + 0.5f);
                uint32_t b = static_cast<uint32_t>(fgB * (1.0f - t) + bgB * t + 0.5f);
                dstLine[x] = (origA << 24) | (r << 16) | (g << 8) | b;
            }
        }
    }
    return result;
}

} // namespace

void CanvasWidget::setImageBrushImage(const QImage& img) {
    m_imageBrushImage = img;
    updateImageBrushOutline();
    update();
}

void CanvasWidget::updateImageBrushOutline() {
    m_imageBrushOutlinePath = QPainterPath();
    if (m_imageBrushImage.isNull()) {
        return;
    }

    BrushImageType imgType = detectBrushImageType(m_imageBrushImage);
    QImage colorized = colorizeBrushImage(m_imageBrushImage, imgType, Qt::black, Qt::white);
    int origW = colorized.width();
    int origH = colorized.height();
    int maxDim = std::max(origW, origH);
    if (origW <= 0 || origH <= 0 || maxDim <= 0) {
        return;
    }

    // パフォーマンスと描画精度を両立するため最大解像度を256に制限
    int scaleDim = std::min(maxDim, 256);
    float scaleDown = static_cast<float>(scaleDim) / static_cast<float>(maxDim);
    int sw = std::max(1, static_cast<int>(std::round(origW * scaleDown)));
    int sh = std::max(1, static_cast<int>(std::round(origH * scaleDown)));

    QImage img = (sw == origW && sh == origH)
                     ? colorized
                     : colorized.scaled(sw, sh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    if (img.format() != QImage::Format_ARGB32 && img.format() != QImage::Format_ARGB32_Premultiplied) {
        img = img.convertToFormat(QImage::Format_ARGB32);
    }

    std::vector<uint8_t> mask(sw * sh, 0);
    int opaqueCount = 0;
    for (int y = 0; y < sh; ++y) {
        const uint32_t* scan = reinterpret_cast<const uint32_t*>(img.constScanLine(y));
        for (int x = 0; x < sw; ++x) {
            uint32_t a = (scan[x] >> 24) & 0xFF;
            if (a >= 20) {
                mask[y * sw + x] = 1;
                opaqueCount++;
            }
        }
    }

    if (opaqueCount == 0) {
        return;
    }

    auto ptKey = [sw](int x, int y) { return y * (sw + 1) + x; };
    auto keyToPt = [sw](int k) { return QPoint(k % (sw + 1), k / (sw + 1)); };

    std::vector<std::vector<int>> adj((sw + 1) * (sh + 1));

    for (int y = 0; y < sh; ++y) {
        for (int x = 0; x < sw; ++x) {
            if (!mask[y * sw + x]) continue;

            // 上辺: (x, y) -> (x+1, y)
            if (y == 0 || !mask[(y - 1) * sw + x]) {
                adj[ptKey(x, y)].push_back(ptKey(x + 1, y));
            }
            // 右辺: (x+1, y) -> (x+1, y+1)
            if (x == sw - 1 || !mask[y * sw + (x + 1)]) {
                adj[ptKey(x + 1, y)].push_back(ptKey(x + 1, y + 1));
            }
            // 下辺: (x+1, y+1) -> (x, y+1)
            if (y == sh - 1 || !mask[(y + 1) * sw + x]) {
                adj[ptKey(x + 1, y + 1)].push_back(ptKey(x, y + 1));
            }
            // 左辺: (x, y+1) -> (x, y)
            if (x == 0 || !mask[y * sw + (x - 1)]) {
                adj[ptKey(x, y + 1)].push_back(ptKey(x, y));
            }
        }
    }

    QPainterPath fullPath;
    float maxDimScaled = static_cast<float>(std::max(sw, sh));
    float halfW = sw * 0.5f;
    float halfH = sh * 0.5f;

    auto toNorm = [&](const QPoint& pt) -> QPointF {
        return QPointF((pt.x() - halfW) / maxDimScaled,
                       (pt.y() - halfH) / maxDimScaled);
    };

    for (int startK = 0; startK < static_cast<int>(adj.size()); ++startK) {
        while (!adj[startK].empty()) {
            std::vector<QPoint> loop;
            int curK = startK;
            loop.push_back(keyToPt(curK));

            while (!adj[curK].empty()) {
                int nextK = adj[curK].back();
                adj[curK].pop_back();
                curK = nextK;
                loop.push_back(keyToPt(curK));
                if (curK == startK) {
                    break;
                }
            }

            if (loop.size() < 4) continue;

            // 直線上の頂点を簡約 (Collinear reduction)
            std::vector<QPoint> reduced;
            reduced.push_back(loop[0]);
            for (size_t i = 1; i + 1 < loop.size(); ++i) {
                QPoint prev = reduced.back();
                QPoint curr = loop[i];
                QPoint next = loop[i + 1];
                int dx1 = curr.x() - prev.x();
                int dy1 = curr.y() - prev.y();
                int dx2 = next.x() - curr.x();
                int dy2 = next.y() - curr.y();
                if (dx1 * dy2 == dy1 * dx2 && (dx1 * dx2 >= 0 && dy1 * dy2 >= 0)) {
                    continue;
                }
                reduced.push_back(curr);
            }
            reduced.push_back(loop.back());

            if (reduced.size() < 3) continue;

            fullPath.moveTo(toNorm(reduced[0]));
            for (size_t i = 1; i < reduced.size(); ++i) {
                fullPath.lineTo(toNorm(reduced[i]));
            }
            fullPath.closeSubpath();
        }
    }

    m_imageBrushOutlinePath = fullPath;
}

void CanvasWidget::paintImageBrushSegment(const QPointF& p0, const QPointF& p1, float w0, float w1, float dens0, float dens1, float p0Raw, float p1Raw) {
    Q_UNUSED(p0Raw);
    Q_UNUSED(p1Raw);
    if (!m_document) return;
    auto activeL = effectiveTargetLayer();
    if (!activeL || activeL->type() != LayerType::Raster) return;
    if (activeL->isLocked()) return;

    if (m_imageBrushImage.isNull()) {
        QImage fallback(64, 64, QImage::Format_ARGB32);
        fallback.fill(Qt::transparent);
        QPainter p(&fallback);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setBrush(Qt::black);
        p.setPen(Qt::NoPen);
        p.drawEllipse(0, 0, 64, 64);
        m_imageBrushImage = fallback;
    }

    bool hasSelection = m_document && m_document->hasSelection() && !m_document->isSelectionEditMode();
    QRectF selBounds = hasSelection ? m_document->selectionBounds() : QRectF();
    bool alphaLocked = activeL->isAlphaLocked();

    BrushImageType imgType = detectBrushImageType(m_imageBrushImage);
    QImage colorized = colorizeBrushImage(m_imageBrushImage, imgType, m_currentColor, m_secondaryColor);
    int origImgW = colorized.width();
    int origImgH = colorized.height();
    int maxDim = std::max(origImgW, origImgH);
    if (maxDim <= 0) return;

    float dx = p1.x() - p0.x();
    float dy = p1.y() - p0.y();
    float dist = std::hypot(dx, dy);

    if (dist > 0.5f) {
        m_imageBrushLastFollowAngle = static_cast<float>(std::atan2(dy, dx) * 180.0 / 3.14159265358979323846);
    }

    QRect dirtyRect;

    int lastTx = -1;
    int lastTy = -1;
    uint32_t* dstPixels = nullptr;

    auto applySingleStamp = [&](float cx, float cy, float targetSize, float stampDensity, float extraAngle) {
        if (targetSize < 1.0f || stampDensity <= 0.0001f) return;

        float scale = targetSize / static_cast<float>(maxDim);
        int sw = std::max(1, static_cast<int>(std::round(origImgW * scale)));
        int sh = std::max(1, static_cast<int>(std::round(origImgH * scale)));

        QTransform xform;
        if (m_imageBrushFlipH || m_imageBrushFlipV) {
            xform.scale(m_imageBrushFlipH ? -1.0 : 1.0, m_imageBrushFlipV ? -1.0 : 1.0);
        }
        float totalAngle = static_cast<float>(m_imageBrushAngle) + extraAngle;
        if (m_imageBrushFollowCursor) {
            totalAngle += m_imageBrushLastFollowAngle;
        }
        while (totalAngle >= 360.0f) totalAngle -= 360.0f;
        while (totalAngle < 0.0f) totalAngle += 360.0f;

        if (std::abs(totalAngle) > 0.01f) {
            xform.rotate(totalAngle);
        }

        QImage scaled = colorized.scaled(sw, sh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        QImage stampImg = xform.isIdentity() ? scaled : scaled.transformed(xform, Qt::SmoothTransformation);
        if (stampImg.format() != QImage::Format_ARGB32) {
            stampImg = stampImg.convertToFormat(QImage::Format_ARGB32);
        }

        int stampW = stampImg.width();
        int stampH = stampImg.height();
        int left = static_cast<int>(std::round(cx - stampW * 0.5f));
        int top = static_cast<int>(std::round(cy - stampH * 0.5f));

        int docW = m_document->width();
        int docH = m_document->height();

        int xStart = std::max(0, left);
        int xEnd = std::min(docW - 1, left + stampW - 1);
        int yStart = std::max(0, top);
        int yEnd = std::min(docH - 1, top + stampH - 1);

        if (xStart > xEnd || yStart > yEnd) return;

        for (int py = yStart; py <= yEnd; ++py) {
            int sy = py - top;
            const uint32_t* stampScan = reinterpret_cast<const uint32_t*>(stampImg.constScanLine(sy));
            int ty = py / TileMemoryPool::TILE_HEIGHT;
            int ly = py % TileMemoryPool::TILE_HEIGHT;

            for (int px = xStart; px <= xEnd; ++px) {
                float selWeight = 1.0f;
                if (hasSelection) {
                    uint8_t sel = m_document->selectionStrength(px, py);
                    if (sel == 0) continue;
                    selWeight = sel / 255.0f;
                }

                int sx = px - left;
                uint32_t sp = stampScan[sx];
                uint32_t sa_orig = (sp >> 24) & 0xFF;
                if (sa_orig == 0) continue;

                float stampAlpha = (sa_orig / 255.0f) * stampDensity * selWeight;
                if (stampAlpha <= 0.0001f) continue;

                uint32_t sr = (sp >> 16) & 0xFF;
                uint32_t sg = (sp >> 8) & 0xFF;
                uint32_t sb = sp & 0xFF;

                int tx = px / TileMemoryPool::TILE_WIDTH;
                int lx = px % TileMemoryPool::TILE_WIDTH;

                if (tx != lastTx || ty != lastTy) {
                    lastTx = tx;
                    lastTy = ty;
                    Tile* dstTile = activeL->tileManager().getTileForWrite(tx, ty);
                    dstPixels = dstTile ? reinterpret_cast<uint32_t*>(dstTile->writeBuffer()) : nullptr;
                }
                if (!dstPixels) continue;

                int localIdx = ly * TileMemoryPool::TILE_WIDTH + lx;
                uint32_t curPixel = dstPixels[localIdx];
                uint32_t da_cur = (curPixel >> 24) & 0xFF;

                if (alphaLocked) {
                    if (da_cur == 0) continue;
                    uint32_t dr = (curPixel >> 16) & 0xFF;
                    uint32_t dg = (curPixel >> 8) & 0xFF;
                    uint32_t db = curPixel & 0xFF;

                    float blendR = blendChannel(dr / 255.0f, sr / 255.0f, m_imageBrushBlendMode) * 255.0f;
                    float blendG = blendChannel(dg / 255.0f, sg / 255.0f, m_imageBrushBlendMode) * 255.0f;
                    float blendB = blendChannel(db / 255.0f, sb / 255.0f, m_imageBrushBlendMode) * 255.0f;

                    uint32_t outR = static_cast<uint32_t>(blendR * stampAlpha + dr * (1.0f - stampAlpha) + 0.5f);
                    uint32_t outG = static_cast<uint32_t>(blendG * stampAlpha + dg * (1.0f - stampAlpha) + 0.5f);
                    uint32_t outB = static_cast<uint32_t>(blendB * stampAlpha + db * (1.0f - stampAlpha) + 0.5f);

                    dstPixels[localIdx] = (da_cur << 24) |
                                          (std::min(255u, outR) << 16) |
                                          (std::min(255u, outG) << 8) |
                                          std::min(255u, outB);
                } else {
                    float daF = da_cur / 255.0f;
                    float outA = stampAlpha + daF * (1.0f - stampAlpha);
                    if (outA <= 0.0f) {
                        dstPixels[localIdx] = 0;
                        continue;
                    }

                    uint32_t dr = (curPixel >> 16) & 0xFF;
                    uint32_t dg = (curPixel >> 8) & 0xFF;
                    uint32_t db = curPixel & 0xFF;

                    float blendR = (da_cur > 0) ? (blendChannel(dr / 255.0f, sr / 255.0f, m_imageBrushBlendMode) * 255.0f) : sr;
                    float blendG = (da_cur > 0) ? (blendChannel(dg / 255.0f, sg / 255.0f, m_imageBrushBlendMode) * 255.0f) : sg;
                    float blendB = (da_cur > 0) ? (blendChannel(db / 255.0f, sb / 255.0f, m_imageBrushBlendMode) * 255.0f) : sb;

                    uint32_t outR = static_cast<uint32_t>((blendR * stampAlpha + dr * daF * (1.0f - stampAlpha)) / outA + 0.5f);
                    uint32_t outG = static_cast<uint32_t>((blendG * stampAlpha + dg * daF * (1.0f - stampAlpha)) / outA + 0.5f);
                    uint32_t outB = static_cast<uint32_t>((blendB * stampAlpha + db * daF * (1.0f - stampAlpha)) / outA + 0.5f);
                    uint32_t outA8 = static_cast<uint32_t>(outA * 255.0f + 0.5f);

                    dstPixels[localIdx] = (std::min(255u, outA8) << 24) |
                                          (std::min(255u, outR) << 16) |
                                          (std::min(255u, outG) << 8) |
                                          std::min(255u, outB);
                }
            }
        }

        QRect stampRect(xStart, yStart, xEnd - xStart + 1, yEnd - yStart + 1);
        dirtyRect = dirtyRect.isEmpty() ? stampRect : dirtyRect.united(stampRect);
    };

    auto stampAtPoint = [&](float cx, float cy, float baseSize, float curDens) {
        float finalSize = baseSize;
        if (m_imageBrushRandomSize && m_imageBrushRandomSizeAmount > 0) {
            float k = m_imageBrushRandomSizeAmount / 100.0f;
            float mult = 1.0f;
            if (m_imageBrushRandomSizeScaleUp) {
                mult = 1.0f + k * static_cast<float>(QRandomGenerator::global()->generateDouble() * 2.0 - 1.0);
            } else {
                mult = 1.0f - k * static_cast<float>(QRandomGenerator::global()->generateDouble());
            }
            finalSize = std::max(1.0f, baseSize * mult);
        }

        float randRot = 0.0f;
        if (m_imageBrushRandomRotate && m_imageBrushRandomRotateAmount > 0) {
            randRot = static_cast<float>((QRandomGenerator::global()->generateDouble() * 2.0 - 1.0) * m_imageBrushRandomRotateAmount);
        }

        if (!m_imageBrushScatter) {
            applySingleStamp(cx, cy, finalSize, curDens, randRot);
        } else {
            float maxDist = finalSize * (m_imageBrushScatterRange / 100.0f);
            float sk = m_imageBrushScatterSizeRandom / 100.0f;

            for (int s = 0; s < m_imageBrushScatterAmount; ++s) {
                float angle = static_cast<float>(QRandomGenerator::global()->generateDouble() * 2.0 * 3.14159265358979323846);
                float distOff = maxDist * static_cast<float>(QRandomGenerator::global()->generateDouble());
                float sx = cx + std::cos(angle) * distOff;
                float sy = cy + std::sin(angle) * distOff;

                float smult = 1.0f;
                if (sk > 0.0f) {
                    if (m_imageBrushScatterScaleUp) {
                        smult = 1.0f + sk * static_cast<float>(QRandomGenerator::global()->generateDouble() * 2.0 - 1.0);
                    } else {
                        smult = 1.0f - sk * static_cast<float>(QRandomGenerator::global()->generateDouble());
                    }
                }
                float scatterFinalSize = std::max(1.0f, finalSize * smult);

                float sRot = randRot;
                if (m_imageBrushRandomRotate && m_imageBrushRandomRotateAmount > 0) {
                    sRot = static_cast<float>((QRandomGenerator::global()->generateDouble() * 2.0 - 1.0) * m_imageBrushRandomRotateAmount);
                }

                applySingleStamp(sx, sy, scatterFinalSize, curDens, sRot);
            }
        }
    };

    if (dist < 0.0001f) {
        stampAtPoint(p0.x(), p0.y(), w0, dens0);
        m_imageBrushDistRemainder = 0.0f;
    } else {
        float avgW = (w0 + w1) * 0.5f;
        float stepDist = std::max(1.0f, avgW * (m_imageBrushSpacing / 100.0f));

        float d = m_imageBrushDistRemainder;
        while (d <= dist) {
            float t = (dist > 0.0001f) ? (d / dist) : 0.0f;
            float curX = p0.x() + dx * t;
            float curY = p0.y() + dy * t;
            float curW = w0 + (w1 - w0) * t;
            float curDens = dens0 + (dens1 - dens0) * t;

            stampAtPoint(curX, curY, curW, curDens);
            d += stepDist;
        }
        m_imageBrushDistRemainder = d - dist;
    }

    if (!dirtyRect.isEmpty()) {
        m_strokeDirtyRect = m_strokeDirtyRect.isEmpty() ? dirtyRect : m_strokeDirtyRect.united(dirtyRect);
        activeL->tileManager().markDirtyRect(dirtyRect);
        m_document->composite(dirtyRect);
    }
}

CanvasWidget::FloodFillResult CanvasWidget::computeFloodFillMask(const QPointF& docPos, const FloodFillParams& params) {
    FloodFillResult result;
    if (!m_document) return result;

    int docW = m_document->width();
    int docH = m_document->height();
    int sx = static_cast<int>(std::floor(docPos.x()));
    int sy = static_cast<int>(std::floor(docPos.y()));

    if (sx < 0 || sx >= docW || sy < 0 || sy >= docH) return result;

    auto activeL = m_document->activeLayer();
    if (params.reference == BucketReference::ActiveLayer && !activeL) {
        return result;
    }

    bool hasSelection = m_document->hasSelection();
    if (params.restrictToSelection && hasSelection && !m_document->isPixelSelected(sx, sy)) {
        return result;
    }

    auto getRefPixel = [&](int x, int y) -> uint32_t {
        if (x < 0 || x >= docW || y < 0 || y >= docH) return 0;
        if (params.reference == BucketReference::Canvas) {
            int tx = x / TileMemoryPool::TILE_WIDTH;
            int ty = y / TileMemoryPool::TILE_HEIGHT;
            const Tile* tile = m_document->compositeBuffer().getTile(tx, ty);
            if (!tile || !tile->isValid() || !tile->readBuffer()) {
                return 0xFFFFFFFF; // 背景白
            }
            int lx = x % TileMemoryPool::TILE_WIDTH;
            int ly = y % TileMemoryPool::TILE_HEIGHT;
            const uint32_t* p = reinterpret_cast<const uint32_t*>(tile->readBuffer());
            uint32_t pix = p[ly * TileMemoryPool::TILE_WIDTH + lx];
            uint32_t a = (pix >> 24) & 0xFF;
            if (a < 255) {
                float alpha = a / 255.0f;
                uint32_t r = static_cast<uint32_t>(((pix >> 16) & 0xFF) * alpha + 255.0f * (1.0f - alpha) + 0.5f);
                uint32_t g = static_cast<uint32_t>(((pix >> 8) & 0xFF) * alpha + 255.0f * (1.0f - alpha) + 0.5f);
                uint32_t b = static_cast<uint32_t>((pix & 0xFF) * alpha + 255.0f * (1.0f - alpha) + 0.5f);
                return (0xFF000000) | (r << 16) | (g << 8) | b;
            }
            return pix;
        } else {
            if (!activeL) return 0x00000000;
            int tx = x / TileMemoryPool::TILE_WIDTH;
            int ty = y / TileMemoryPool::TILE_HEIGHT;
            const Tile* tile = activeL->tileManager().getTile(tx, ty);
            if (!tile || !tile->isValid() || !tile->readBuffer()) {
                return 0x00000000; // 未描画部分は完全透明
            }
            int lx = x % TileMemoryPool::TILE_WIDTH;
            int ly = y % TileMemoryPool::TILE_HEIGHT;
            const uint32_t* p = reinterpret_cast<const uint32_t*>(tile->readBuffer());
            return p[ly * TileMemoryPool::TILE_WIDTH + lx];
        }
    };

    uint32_t seedColor = getRefPixel(sx, sy);

    auto colorDiff = [](uint32_t c1, uint32_t c2) -> int {
        int a1 = (c1 >> 24) & 0xFF;
        int a2 = (c2 >> 24) & 0xFF;
        if (a1 == 0 && a2 == 0) return 0;
        int r1 = (c1 >> 16) & 0xFF;
        int g1 = (c1 >> 8) & 0xFF;
        int b1 = c1 & 0xFF;
        int r2 = (c2 >> 16) & 0xFF;
        int g2 = (c2 >> 8) & 0xFF;
        int b2 = c2 & 0xFF;
        int da = std::abs(a1 - a2);
        int dr = std::abs(r1 - r2);
        int dg = std::abs(g1 - g2);
        int db = std::abs(b1 - b2);
        return std::max({dr, dg, db, da});
    };

    // BFS
    std::vector<uint8_t> mask(static_cast<size_t>(docW) * docH, 0);
    std::vector<QPoint> queue;
    queue.reserve(10000);
    queue.push_back(QPoint(sx, sy));
    mask[static_cast<size_t>(sy) * docW + sx] = 1;

    int minX = sx, maxX = sx, minY = sy, maxY = sy;
    size_t head = 0;

    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};

    while (head < queue.size()) {
        QPoint pt = queue[head++];
        int cx = pt.x();
        int cy = pt.y();

        for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];

            if (nx < 0 || nx >= docW || ny < 0 || ny >= docH) continue;
            size_t idx = static_cast<size_t>(ny) * docW + nx;
            if (mask[idx]) continue;

            if (params.restrictToSelection && hasSelection && !m_document->isPixelSelected(nx, ny)) continue;

            uint32_t c = getRefPixel(nx, ny);
            if (colorDiff(c, seedColor) <= params.tolerance) {
                mask[idx] = 1;
                queue.push_back(QPoint(nx, ny));
                minX = std::min(minX, nx);
                maxX = std::max(maxX, nx);
                minY = std::min(minY, ny);
                maxY = std::max(maxY, ny);
            }
        }
    }

    // 細い領域にしみこむ
    if (params.narrowAreaFill) {
        int wallThreshold = std::max(params.tolerance * 2, 60);

        std::vector<QPoint> seepQueue;
        for (const auto& pt : queue) {
            for (int i = 0; i < 4; ++i) {
                int nx = pt.x() + dx[i];
                int ny = pt.y() + dy[i];
                if (nx < 0 || nx >= docW || ny < 0 || ny >= docH) continue;
                size_t idx = static_cast<size_t>(ny) * docW + nx;
                if (!mask[idx]) {
                    if (!params.restrictToSelection || !hasSelection || m_document->isPixelSelected(nx, ny)) {
                        seepQueue.push_back(QPoint(nx, ny));
                    }
                }
            }
        }

        std::sort(seepQueue.begin(), seepQueue.end(), [](const QPoint& a, const QPoint& b) {
            return (a.y() == b.y()) ? (a.x() < b.x()) : (a.y() < b.y());
        });
        seepQueue.erase(std::unique(seepQueue.begin(), seepQueue.end()), seepQueue.end());

        const int axisDx[4] = {1, 0, 1, 1};
        const int axisDy[4] = {0, 1, 1, -1};
        const int maxRay = 14;
        const int maxNarrowGap = 16;

        size_t seepHead = 0;
        while (seepHead < seepQueue.size()) {
            QPoint cur = seepQueue[seepHead++];
            int px = cur.x();
            int py = cur.y();
            size_t pIdx = static_cast<size_t>(py) * docW + px;
            if (mask[pIdx]) continue;

            int selfD = colorDiff(getRefPixel(px, py), seedColor);
            if (selfD >= wallThreshold + 30) continue;

            bool isNarrow = false;
            for (int a = 0; a < 4; ++a) {
                int adx = axisDx[a];
                int ady = axisDy[a];

                int d1 = 0;
                int maxD1 = 0;
                for (int step = 1; step <= maxRay; ++step) {
                    int rx = px + step * adx;
                    int ry = py + step * ady;
                    if (rx < 0 || rx >= docW || ry < 0 || ry >= docH) break;
                    int dVal = colorDiff(getRefPixel(rx, ry), seedColor);
                    maxD1 = std::max(maxD1, dVal);
                    if (dVal >= wallThreshold) {
                        d1 = step;
                        break;
                    }
                }

                int d2 = 0;
                int maxD2 = 0;
                for (int step = 1; step <= maxRay; ++step) {
                    int rx = px - step * adx;
                    int ry = py - step * ady;
                    if (rx < 0 || rx >= docW || ry < 0 || ry >= docH) break;
                    int dVal = colorDiff(getRefPixel(rx, ry), seedColor);
                    maxD2 = std::max(maxD2, dVal);
                    if (dVal >= wallThreshold) {
                        d2 = step;
                        break;
                    }
                }

                if (d1 > 0 && d2 > 0 && (d1 + d2) <= maxNarrowGap) {
                    if (selfD <= std::max(maxD1, maxD2) - 8) {
                        isNarrow = true;
                        break;
                    }
                }
            }

            if (isNarrow) {
                mask[pIdx] = 1;
                minX = std::min(minX, px);
                maxX = std::max(maxX, px);
                minY = std::min(minY, py);
                maxY = std::max(maxY, py);

                for (int i = 0; i < 4; ++i) {
                    int nx = px + dx[i];
                    int ny = py + dy[i];
                    if (nx < 0 || nx >= docW || ny < 0 || ny >= docH) continue;
                    size_t nIdx = static_cast<size_t>(ny) * docW + nx;
                    if (!mask[nIdx]) {
                        if (!params.restrictToSelection || !hasSelection || m_document->isPixelSelected(nx, ny)) {
                            seepQueue.push_back(QPoint(nx, ny));
                        }
                    }
                }
            }
        }
    }

    // 領域拡大 / 縮小
    int expand = params.expandPixels;
    if (expand > 0) {
        std::vector<QPoint> offsets;
        int r2 = expand * expand;
        for (int ey = -expand; ey <= expand; ++ey) {
            for (int ex = -expand; ex <= expand; ++ex) {
                if (ex * ex + ey * ey <= r2) {
                    offsets.push_back(QPoint(ex, ey));
                }
            }
        }

        std::vector<QPoint> boundary;
        int bMinX = std::max(0, minX - 1);
        int bMaxX = std::min(docW - 1, maxX + 1);
        int bMinY = std::max(0, minY - 1);
        int bMaxY = std::min(docH - 1, maxY + 1);

        for (int y = bMinY; y <= bMaxY; ++y) {
            for (int x = bMinX; x <= bMaxX; ++x) {
                if (mask[static_cast<size_t>(y) * docW + x]) {
                    bool isEdge = false;
                    for (int i = 0; i < 4; ++i) {
                        int nx = x + dx[i];
                        int ny = y + dy[i];
                        if (nx < 0 || nx >= docW || ny < 0 || ny >= docH || !mask[static_cast<size_t>(ny) * docW + nx]) {
                            isEdge = true;
                            break;
                        }
                    }
                    if (isEdge) {
                        boundary.push_back(QPoint(x, y));
                    }
                }
            }
        }

        std::vector<uint8_t> expandedMask = mask;
        for (const auto& bp : boundary) {
            for (const auto& off : offsets) {
                int nx = bp.x() + off.x();
                int ny = bp.y() + off.y();
                if (nx < 0 || nx >= docW || ny < 0 || ny >= docH) continue;
                if (params.restrictToSelection && hasSelection && !m_document->isPixelSelected(nx, ny)) continue;
                expandedMask[static_cast<size_t>(ny) * docW + nx] = 1;
                minX = std::min(minX, nx);
                maxX = std::max(maxX, nx);
                minY = std::min(minY, ny);
                maxY = std::max(maxY, ny);
            }
        }
        mask = std::move(expandedMask);
    } else if (expand < 0) {
        int shrink = -expand;
        std::vector<QPoint> offsets;
        int r2 = shrink * shrink;
        for (int ey = -shrink; ey <= shrink; ++ey) {
            for (int ex = -shrink; ex <= shrink; ++ex) {
                if (ex * ex + ey * ey <= r2) {
                    offsets.push_back(QPoint(ex, ey));
                }
            }
        }

        std::vector<QPoint> outerBoundary;
        int bMinX = std::max(0, minX - 1);
        int bMaxX = std::min(docW - 1, maxX + 1);
        int bMinY = std::max(0, minY - 1);
        int bMaxY = std::min(docH - 1, maxY + 1);

        for (int y = bMinY; y <= bMaxY; ++y) {
            for (int x = bMinX; x <= bMaxX; ++x) {
                if (!mask[static_cast<size_t>(y) * docW + x]) {
                    bool touches = false;
                    for (int i = 0; i < 4; ++i) {
                        int nx = x + dx[i];
                        int ny = y + dy[i];
                        if (nx >= 0 && nx < docW && ny >= 0 && ny < docH && mask[static_cast<size_t>(ny) * docW + nx]) {
                            touches = true;
                            break;
                        }
                    }
                    if (touches) {
                        outerBoundary.push_back(QPoint(x, y));
                    }
                }
            }
        }

        std::vector<uint8_t> erodedMask = mask;
        for (const auto& ob : outerBoundary) {
            for (const auto& off : offsets) {
                int nx = ob.x() + off.x();
                int ny = ob.y() + off.y();
                if (nx >= 0 && nx < docW && ny >= 0 && ny < docH) {
                    erodedMask[static_cast<size_t>(ny) * docW + nx] = 0;
                }
            }
        }
        mask = std::move(erodedMask);
    }

    int actualMinX = docW, actualMaxX = -1, actualMinY = docH, actualMaxY = -1;
    for (int y = std::max(0, minY); y <= std::min(docH - 1, maxY); ++y) {
        for (int x = std::max(0, minX); x <= std::min(docW - 1, maxX); ++x) {
            if (mask[static_cast<size_t>(y) * docW + x]) {
                actualMinX = std::min(actualMinX, x);
                actualMaxX = std::max(actualMaxX, x);
                actualMinY = std::min(actualMinY, y);
                actualMaxY = std::max(actualMaxY, y);
            }
        }
    }

    if (actualMaxX < actualMinX || actualMaxY < actualMinY) {
        return result;
    }

    result.success = true;
    result.mask = std::move(mask);
    result.minX = actualMinX;
    result.maxX = actualMaxX;
    result.minY = actualMinY;
    result.maxY = actualMaxY;
    result.docW = docW;
    result.docH = docH;
    return result;
}

QPainterPath CanvasWidget::maskToPath(const FloodFillResult& res) {
    if (!res.success || res.minX > res.maxX || res.minY > res.maxY) {
        return QPainterPath();
    }

    int docW = res.docW;
    int docH = res.docH;
    const auto& mask = res.mask;

    auto packPt = [](int x, int y) -> uint64_t {
        return (static_cast<uint64_t>(static_cast<uint32_t>(y)) << 32) | static_cast<uint32_t>(x);
    };

    std::unordered_map<uint64_t, std::vector<QPoint>> adj;

    for (int y = res.minY; y <= res.maxY; ++y) {
        size_t rowOffset = static_cast<size_t>(y) * docW;
        for (int x = res.minX; x <= res.maxX; ++x) {
            if (!mask[rowOffset + x]) continue;

            // Top edge: (x, y) -> (x+1, y)
            if (y == 0 || !mask[rowOffset - docW + x]) {
                adj[packPt(x, y)].push_back(QPoint(x + 1, y));
            }
            // Right edge: (x+1, y) -> (x+1, y+1)
            if (x == docW - 1 || !mask[rowOffset + x + 1]) {
                adj[packPt(x + 1, y)].push_back(QPoint(x + 1, y + 1));
            }
            // Bottom edge: (x+1, y+1) -> (x, y+1)
            if (y == docH - 1 || !mask[rowOffset + docW + x]) {
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
            poly << QPointF(startPt);

            QPoint prevDir = curPt - startPt;
            QPoint prevPt = startPt;

            int safety = 0;
            const int maxSteps = docW * docH * 4;

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
                    poly.back() = QPointF(curPt);
                } else {
                    poly << QPointF(curPt);
                    prevDir = curDir;
                }
                prevPt = curPt;
                curPt = nextPt;
            }

            if (poly.size() >= 3) {
                QPointF d1 = poly[1] - poly[0];
                QPointF dLast = poly[0] - poly.back();
                if ((d1.x() == 0 && dLast.x() == 0 && ((d1.y() > 0) == (dLast.y() > 0))) ||
                    (d1.y() == 0 && dLast.y() == 0 && ((d1.x() > 0) == (dLast.x() > 0)))) {
                    poly.removeFirst();
                }
            }

            if (poly.size() >= 3) {
                path.addPolygon(poly);
                path.closeSubpath();
            }
        }
    }

    return path;
}

void CanvasWidget::fillBucket(const QPointF& docPos) {
    if (!m_document) return;
    auto activeL = m_document->activeLayer();
    if (!activeL || activeL->type() == LayerType::Group || activeL->type() == LayerType::Vector) {
        return; // バケツは通常ラスターレイヤーのみ対象
    }

    FloodFillParams params;
    params.reference = m_bucketReference;
    params.tolerance = m_bucketTolerance;
    params.expandPixels = m_bucketExpandPixels;
    params.narrowAreaFill = m_bucketNarrowAreaFill;
    params.restrictToSelection = true;

    FloodFillResult res = computeFloodFillMask(docPos, params);
    if (!res.success) return;

    int minX = res.minX;
    int maxX = res.maxX;
    int minY = res.minY;
    int maxY = res.maxY;
    int docW = res.docW;
    int docH = res.docH;
    const auto& mask = res.mask;

    QRect dirtyRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
    TileManager preTiles = activeL->tileManager().clone();

    bool alphaLocked = activeL->isAlphaLocked();
    uint32_t fillColor = m_currentColor.rgba();
    int minTx = minX / TileMemoryPool::TILE_WIDTH;
    int maxTx = maxX / TileMemoryPool::TILE_WIDTH;
    int minTy = minY / TileMemoryPool::TILE_HEIGHT;
    int maxTy = maxY / TileMemoryPool::TILE_HEIGHT;

    for (int ty = minTy; ty <= maxTy; ++ty) {
        for (int tx = minTx; tx <= maxTx; ++tx) {
            int tLeft = tx * TileMemoryPool::TILE_WIDTH;
            int tTop = ty * TileMemoryPool::TILE_HEIGHT;
            int tRight = std::min(docW - 1, tLeft + static_cast<int>(TileMemoryPool::TILE_WIDTH) - 1);
            int tBottom = std::min(docH - 1, tTop + static_cast<int>(TileMemoryPool::TILE_HEIGHT) - 1);

            bool tileHasMask = false;
            for (int y = std::max(minY, tTop); y <= std::min(maxY, tBottom) && !tileHasMask; ++y) {
                for (int x = std::max(minX, tLeft); x <= std::min(maxX, tRight); ++x) {
                    if (mask[static_cast<size_t>(y) * docW + x]) {
                        tileHasMask = true;
                        break;
                    }
                }
            }
            if (!tileHasMask) continue;

            Tile* tile = activeL->tileManager().getTileForWrite(tx, ty);
            if (!tile || !tile->writeBuffer()) continue;

            uint32_t* pixels = reinterpret_cast<uint32_t*>(tile->writeBuffer());
            for (int y = std::max(minY, tTop); y <= std::min(maxY, tBottom); ++y) {
                int ly = y - tTop;
                for (int x = std::max(minX, tLeft); x <= std::min(maxX, tRight); ++x) {
                    if (mask[static_cast<size_t>(y) * docW + x]) {
                        int lx = x - tLeft;
                        int localIdx = ly * TileMemoryPool::TILE_WIDTH + lx;
                        uint32_t basePixel = pixels[localIdx];
                        float selWeight = 1.0f;
                        if (m_document && m_document->hasSelection()) {
                            uint8_t sel = m_document->selectionStrength(x, y);
                            if (sel == 0) continue;
                            selWeight = sel / 255.0f;
                        }
                        if (alphaLocked) {
                            uint32_t origA = (basePixel >> 24) & 0xFF;
                            if (origA == 0) continue;
                            uint32_t blended = blendPixel(basePixel, fillColor, selWeight, m_bucketBlendMode);
                            pixels[localIdx] = (origA << 24) | (blended & 0x00FFFFFF);
                        } else {
                            pixels[localIdx] = blendPixel(basePixel, fillColor, selWeight, m_bucketBlendMode);
                        }
                    }
                }
            }
        }
    }

    TileManager postTiles = activeL->tileManager().clone();
    if (m_document->undoStack()) {
        m_document->undoStack()->push(new FillBucketCommand(m_document, m_document->activeLayerIndex(), std::move(preTiles), std::move(postTiles), dirtyRect));
    }
    activeL->invalidateContentBounds();
    activeL->tileManager().markDirtyRect(dirtyRect);
    m_document->composite(dirtyRect);
    update();
}

void CanvasWidget::autoSelect(const QPointF& docPos) {
    if (!m_document) return;

    SelectionMode effectiveMode = m_autoSelectMode;
    if (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier) {
        effectiveMode = SelectionMode::Add;
    } else if (QGuiApplication::keyboardModifiers() & Qt::AltModifier) {
        effectiveMode = SelectionMode::Subtract;
    }

    int docW = m_document->width();
    int docH = m_document->height();
    int sx = static_cast<int>(std::floor(docPos.x()));
    int sy = static_cast<int>(std::floor(docPos.y()));

    QPainterPath oldSelection = m_document->selection();
    QPainterPath newSelection;

    if (sx < 0 || sx >= docW || sy < 0 || sy >= docH) {
        if (effectiveMode == SelectionMode::New) {
            newSelection = QPainterPath();
            m_hasActiveRect = false;
        } else {
            return;
        }
    } else {
        FloodFillParams params;
        params.reference = m_autoSelectReference;
        params.tolerance = m_autoSelectTolerance;
        params.expandPixels = m_autoSelectExpandPixels;
        params.narrowAreaFill = m_autoSelectNarrowAreaFill;
        params.restrictToSelection = false;

        FloodFillResult res = computeFloodFillMask(docPos, params);
        if (!res.success) {
            if (effectiveMode == SelectionMode::New) {
                newSelection = QPainterPath();
                m_hasActiveRect = false;
            } else {
                return;
            }
        } else {
            QPainterPath magicWandPath = maskToPath(res);
            if (magicWandPath.isEmpty()) {
                if (effectiveMode == SelectionMode::New) {
                    newSelection = QPainterPath();
                    m_hasActiveRect = false;
                } else {
                    return;
                }
            } else {
                if (effectiveMode == SelectionMode::New) {
                    newSelection = magicWandPath;
                    m_hasActiveRect = false;
                } else if (effectiveMode == SelectionMode::Add) {
                    newSelection = oldSelection.united(magicWandPath);
                    m_hasActiveRect = false;
                } else if (effectiveMode == SelectionMode::Subtract) {
                    newSelection = oldSelection.subtracted(magicWandPath);
                    m_hasActiveRect = false;
                }
            }
        }
    }

    if (m_document->undoStack()) {
        m_document->undoStack()->push(new SelectCommand(m_document, oldSelection, newSelection));
    } else {
        m_document->setSelection(newSelection);
    }

    updateAntsTimer();
    update();
}

void CanvasWidget::executeColorSelection(const QPointF& docPos) {
    if (!m_document) return;

    SelectionMode effectiveMode = m_colorSelectMode;
    if (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier) {
        effectiveMode = SelectionMode::Add;
    } else if (QGuiApplication::keyboardModifiers() & Qt::AltModifier) {
        effectiveMode = SelectionMode::Subtract;
    }

    int docW = m_document->width();
    int docH = m_document->height();
    int sx = static_cast<int>(std::floor(docPos.x()));
    int sy = static_cast<int>(std::floor(docPos.y()));

    QPainterPath oldSelection = m_document->selection();
    QPainterPath newSelection;

    if (sx < 0 || sx >= docW || sy < 0 || sy >= docH) {
        if (effectiveMode == SelectionMode::New) {
            newSelection = QPainterPath();
            m_hasActiveRect = false;
        } else {
            return;
        }
    } else {
        // 参照元画像の取得
        QImage srcImg;
        if (m_colorSelectReference == ColorSelectReference::ActiveLayer) {
            auto activeL = m_document->activeLayer();
            if (activeL && activeL->type() == LayerType::Raster) {
                srcImg = activeL->tileManager().toQImage();
            } else {
                srcImg = m_document->compositeBuffer().toQImage();
            }
        } else {
            srcImg = m_document->compositeBuffer().toQImage();
        }

        if (srcImg.isNull() || srcImg.width() != docW || srcImg.height() != docH) {
            return;
        }

        QRgb targetRgb = srcImg.pixel(sx, sy);
        int tr = qRed(targetRgb);
        int tg = qGreen(targetRgb);
        int tb = qBlue(targetRgb);
        int ta = qAlpha(targetRgb);

        FloodFillResult res;
        res.docW = docW;
        res.docH = docH;
        res.minX = docW;
        res.maxX = -1;
        res.minY = docH;
        res.maxY = -1;
        res.mask.assign(static_cast<size_t>(docW) * docH, 0);

        int tol = m_colorSelectTolerance;
        bool isTargetTransparent = (ta < 10);

        for (int y = 0; y < docH; ++y) {
            size_t rowOffset = static_cast<size_t>(y) * docW;
            for (int x = 0; x < docW; ++x) {
                QRgb p = srcImg.pixel(x, y);
                int pa = qAlpha(p);

                bool matched = false;
                if (isTargetTransparent) {
                    if (m_colorSelectTransparent && pa < 10) {
                        matched = true;
                    }
                } else {
                    if (pa >= 10) {
                        int pr = qRed(p);
                        int pg = qGreen(p);
                        int pb = qBlue(p);
                        int diff = std::max({std::abs(pr - tr), std::abs(pg - tg), std::abs(pb - tb)});
                        if (diff <= tol) {
                            matched = true;
                        }
                    }
                }

                if (matched) {
                    res.mask[rowOffset + x] = 1;
                    res.minX = std::min(res.minX, x);
                    res.maxX = std::max(res.maxX, x);
                    res.minY = std::min(res.minY, y);
                    res.maxY = std::max(res.maxY, y);
                }
            }
        }

        if (res.minX <= res.maxX && res.minY <= res.maxY) {
            res.success = true;
            QPainterPath colorPath = maskToPath(res);
            if (colorPath.isEmpty()) {
                if (effectiveMode == SelectionMode::New) {
                    newSelection = QPainterPath();
                    m_hasActiveRect = false;
                } else {
                    return;
                }
            } else {
                if (effectiveMode == SelectionMode::New) {
                    newSelection = colorPath;
                    m_hasActiveRect = false;
                } else if (effectiveMode == SelectionMode::Add) {
                    newSelection = oldSelection.united(colorPath);
                    m_hasActiveRect = false;
                } else if (effectiveMode == SelectionMode::Subtract) {
                    newSelection = oldSelection.subtracted(colorPath);
                    m_hasActiveRect = false;
                }
            }
        } else {
            if (effectiveMode == SelectionMode::New) {
                newSelection = QPainterPath();
                m_hasActiveRect = false;
            } else {
                return;
            }
        }
    }

    if (m_document->undoStack()) {
        m_document->undoStack()->push(new SelectCommand(m_document, oldSelection, newSelection));
    } else {
        m_document->setSelection(newSelection);
    }

    updateAntsTimer();
    update();
}

void CanvasWidget::tabletEvent(QTabletEvent* event) {
    auto activeL = m_document ? m_document->activeLayer() : nullptr;
    if ((activeL && activeL->type() == LayerType::Group) || m_currentTool == ToolType::Text || m_currentTool == ToolType::TextBalloon ||
        m_currentTool == ToolType::Pipette || m_currentTool == ToolType::RectSelect || m_currentTool == ToolType::EllipseSelect ||
        m_currentTool == ToolType::LassoSelect || m_currentTool == ToolType::MagicWand || m_currentTool == ToolType::ColorSelect ||
        m_currentTool == ToolType::Bucket || m_currentTool == ToolType::Gradient ||
        m_currentTool == ToolType::VectorControlPoint ||
        m_currentTool == ToolType::VectorPolyline || m_currentTool == ToolType::VectorCurve) {
        if (m_currentTool == ToolType::VectorControlPoint ||
            m_currentTool == ToolType::VectorPolyline || m_currentTool == ToolType::VectorCurve) {
            event->ignore();
            return;
        }
        if (m_currentTool == ToolType::Pipette && m_document) {
            QPointF docPt = viewToDoc(event->position());
            QColor sampled = m_document->samplePixel(static_cast<int>(docPt.x()), static_cast<int>(docPt.y()));
            emit colorPicked(sampled);
        } else if (m_currentTool == ToolType::Bucket && event->type() == QEvent::TabletPress) {
            QPointF docPt = viewToDoc(event->position());
            if (m_document && m_document->hasSelection()) {
                if (!m_document->isPixelSelected(static_cast<int>(docPt.x()), static_cast<int>(docPt.y()))) {
                    event->accept();
                    return;
                }
            }
            fillBucket(docPt);
        } else if (m_currentTool == ToolType::MagicWand && event->type() == QEvent::TabletPress) {
            QPointF docPt = viewToDoc(event->position());
            autoSelect(docPt);
        } else if (m_currentTool == ToolType::Gradient) {
            QPointF docPt = viewToDoc(event->position());
            if (event->type() == QEvent::TabletPress) {
                if (activeL && activeL->type() == LayerType::Raster) {
                    m_isDraggingGradient = true;
                    m_gradientStartDocPos = docPt;
                    m_gradientEndDocPos = docPt;
                    update();
                }
            } else if (event->type() == QEvent::TabletMove && m_isDraggingGradient) {
                m_gradientEndDocPos = docPt;
                update();
            } else if (event->type() == QEvent::TabletRelease && m_isDraggingGradient) {
                m_isDraggingGradient = false;
                m_gradientEndDocPos = docPt;
                renderGradient(m_gradientStartDocPos, m_gradientEndDocPos);
                update();
            }
        }
        event->accept();
        return;
    }
    QPointF docPt = viewToDoc(event->position());
    m_cursorDocPos = docPt;
    m_showBrushOutline = true;
    float pressure = static_cast<float>(event->pressure());

    switch (event->type()) {
    case QEvent::TabletPress:
        strokeBegin(docPt, pressure > 0.0f ? pressure : 1.0f);
        event->accept();
        break;
    case QEvent::TabletMove:
        if (m_isDrawing) {
            strokeMove(docPt, pressure > 0.0f ? pressure : 1.0f);
            event->accept();
        } else {
            if (m_currentTool == ToolType::Marker && m_imageBrushFollowCursor) {
                float dx = static_cast<float>(docPt.x() - m_lastHoverDocPos.x());
                float dy = static_cast<float>(docPt.y() - m_lastHoverDocPos.y());
                if (dx * dx + dy * dy > 1.0f) {
                    m_imageBrushLastFollowAngle = static_cast<float>(std::atan2(dy, dx) * 180.0 / 3.14159265358979323846);
                }
            }
            m_lastHoverDocPos = docPt;
        }
        break;
    case QEvent::TabletRelease:
        if (m_isDrawing) {
            strokeEnd();
            event->accept();
        }
        break;
    default:
        break;
    }
    update();
}

void CanvasWidget::enterEvent(QEnterEvent* /*event*/) {
    updateCursorForActiveLayer();
}

void CanvasWidget::leaveEvent(QEvent* /*event*/) {
    m_showBrushOutline = false;
    m_hoveredStrokeIdx = -1;
    m_hoveredPointStrokeIdx = -1;
    m_hoveredPointIdx = -1;
    update();
}

void CanvasWidget::mousePressEvent(QMouseEvent* event) {
    // 1. キャンバス回転: Shift + 中ボタン、または (Space または Alt) + Shift + 左ボタン
    bool isShift = (event->modifiers() & Qt::ShiftModifier);
    bool isAlt = (event->modifiers() & Qt::AltModifier);
    bool isPanOrRotateAlt = isAlt && (m_currentTool != ToolType::RectSelect && m_currentTool != ToolType::MagicWand);
    if ((event->button() == Qt::MiddleButton && isShift) ||
        (event->button() == Qt::LeftButton && (m_isSpacePressed || isPanOrRotateAlt) && isShift)) {
        m_isRotating = true;
        m_lastMouseViewPos = event->position();
        setCursor(Qt::SizeAllCursor);
        event->accept();
        return;
    }

    // 2. キャンバス平行移動（パン）: 中ボタン、または (Space または Alt) + 左ボタン
    if (event->button() == Qt::MiddleButton ||
        (event->button() == Qt::LeftButton && (m_isSpacePressed || isPanOrRotateAlt))) {
        m_isPanning = true;
        m_lastMouseViewPos = event->position();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    // 3Dレイヤー選択時の3Dカメラ操作 (左ドラッグ: Orbit回転, Shift+左または中ボタン: Pan平行移動, 右ドラッグ: Zoom)
    auto activeL = m_document ? m_document->activeLayer() : nullptr;
    if ((activeL && activeL->type() == LayerType::ThreeD) || m_currentTool == ToolType::ThreeD) {
        auto threeD = currentThreeDLayer();
        if (threeD) {
            m_last3DMousePos = event->position();
            if (event->button() == Qt::LeftButton) {
                if (event->modifiers() & Qt::ShiftModifier) {
                    m_isPanning3D = true;
                } else {
                    m_isOrbiting3D = true;
                }
                event->accept();
                return;
            } else if (event->button() == Qt::MiddleButton) {
                m_isPanning3D = true;
                event->accept();
                return;
            } else if (event->button() == Qt::RightButton) {
                m_isZooming3D = true;
                event->accept();
                return;
            }
        }
    }

    if (event->button() == Qt::RightButton) {
        if (m_isCreatingVectorLine) {
            finishVectorLine();
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::LeftButton) {
        QPointF docPt = viewToDoc(event->position());
        m_cursorDocPos = docPt;

        if (m_currentTool == ToolType::Transform) {
            if (!m_isTransforming) {
                startTransform();
            }
            if (m_isTransforming) {
                if (m_transformMode == TransformMode::Unified) {
                    TransformHandle handle = hitTestTransformHandle(event->position());
                    if (handle != TransformHandle::None) {
                        m_transformDragHandle = handle;
                        m_transformDragStartViewPos = event->position();
                        m_transformDragStartDocPos = docPt;
                        m_transformDragStartQuad = m_transformCurrentQuad;
                        m_transformDragStartPivot = m_transformPivot;
                        m_transformDragStartAngle = std::atan2(docPt.y() - m_transformPivot.y(), docPt.x() - m_transformPivot.x());
                        updateTransformCursor(m_transformDragHandle, event->modifiers());
                        event->accept();
                        return;
                    }
                } else if (m_transformMode == TransformMode::Warp) {
                    if (m_warpSubMode == WarpSubMode::Mesh) {
                        int hitIdx = -1;
                        qreal minD = 12.0;
                        for (int i = 0; i < static_cast<int>(m_meshCurrentPoints.size()); ++i) {
                            QPointF vp = docToView(m_meshCurrentPoints[i]);
                            qreal d = std::hypot(vp.x() - event->position().x(), vp.y() - event->position().y());
                            if (d < minD) { minD = d; hitIdx = i; }
                        }
                        if (hitIdx >= 0) {
                            m_meshDragIdx = hitIdx;
                            m_transformDragStartDocPos = docPt;
                            event->accept();
                            return;
                        }
                    } else { // Brush
                        m_isWarpDragging = true;
                        m_warpLastMouseDocPos = docPt;
                        event->accept();
                        return;
                    }
                } else if (m_transformMode == TransformMode::Cage) {
                    if (m_isCageClosed) {
                        int hitIdx = -1;
                        qreal minD = 12.0;
                        for (int i = 0; i < static_cast<int>(m_cageCurrentPoints.size()); ++i) {
                            QPointF vp = docToView(m_cageCurrentPoints[i]);
                            qreal d = std::hypot(vp.x() - event->position().x(), vp.y() - event->position().y());
                            if (d < minD) { minD = d; hitIdx = i; }
                        }
                        if (hitIdx >= 0) {
                            m_cageDragIdx = hitIdx;
                            m_transformDragStartDocPos = docPt;
                            event->accept();
                            return;
                        }
                    } else {
                        // ケージ作成中: 始点付近をクリックで閉じる、それ以外は頂点追加
                        if (!m_cageCurrentPoints.empty()) {
                            QPointF vp0 = docToView(m_cageCurrentPoints[0]);
                            qreal d0 = std::hypot(vp0.x() - event->position().x(), vp0.y() - event->position().y());
                            if (d0 < 12.0 && m_cageCurrentPoints.size() >= 3) {
                                m_isCageClosed = true;
                                update();
                                event->accept();
                                return;
                            }
                        }
                        m_cageOrigPoints.push_back(docPt);
                        m_cageCurrentPoints.push_back(docPt);
                        update();
                        event->accept();
                        return;
                    }
                }
            }
        }

        if (m_currentTool == ToolType::AdjustmentBrightnessContrast ||
            m_currentTool == ToolType::AdjustmentHueSaturation ||
            m_currentTool == ToolType::AdjustmentToneCurve ||
            m_currentTool == ToolType::AdjustmentLevels ||
            m_currentTool == ToolType::AdjustmentInvert ||
            m_currentTool == ToolType::AdjustmentGrayscale) {
            event->accept();
            return;
        }

        if (m_currentTool == ToolType::VectorPolyline || m_currentTool == ToolType::VectorCurve) {
            auto activeL = m_document ? m_document->activeLayer() : nullptr;
            auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(activeL);
            if (vecLayer) {
                if (!m_isCreatingVectorLine) {
                    m_isCreatingVectorLine = true;
                    m_inProgressVectorStroke = VectorStroke();
                    m_inProgressVectorStroke.baseWidth = m_vectorLineSize;
                    m_inProgressVectorStroke.color = m_currentColor.rgba();
                    m_inProgressVectorStroke.density = m_vectorLineDensity;
                    m_inProgressVectorStroke.hardness = m_brushHardness;
                    m_inProgressVectorStroke.falloff = m_brushFalloff;
                    m_inProgressVectorStroke.isClosed = m_vectorClosePath;
                }
                bool isCorner = (m_currentTool == ToolType::VectorPolyline);
                m_inProgressVectorStroke.points.push_back({
                    static_cast<float>(docPt.x()),
                    static_cast<float>(docPt.y()),
                    1.0f,
                    1.0f,
                    isCorner,
                    false
                });
                m_vectorLineCurrentMousePos = docPt;
                update();
            }
            event->accept();
            return;
        }

        if (m_currentTool == ToolType::VectorLineEdit) {
            auto activeL = m_document ? m_document->activeLayer() : nullptr;
            auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(activeL);
            if (vecLayer) {
                float strokeHitRadius = std::max(8.0f / static_cast<float>(m_zoom), 5.0f);
                int hitStrokeIdx = vecLayer->findNearestStroke(docPt, strokeHitRadius);
                if (hitStrokeIdx >= 0 && hitStrokeIdx < static_cast<int>(vecLayer->strokes().size())) {
                    auto beforeStrokes = vecLayer->strokes();
                    auto& s = vecLayer->strokes()[hitStrokeIdx];
                    s.baseWidth = m_changeLineSize;
                    s.density = m_changeLineDensity;
                    s.hardness = m_changeLineHardness;
                    s.falloff = m_changeLineFalloff;

                    vecLayer->rasterizeAll();
                    m_document->composite();
                    if (m_document->undoStack()) {
                        m_document->undoStack()->push(new ModifyVectorStrokesCommand(
                            m_document, m_document->activeLayerIndex(), beforeStrokes, vecLayer->strokes(), "線変更"));
                    }
                    update();
                }
            }
            event->accept();
            return;
        }

        if (m_currentTool == ToolType::VectorColorChange) {
            auto activeL = m_document ? m_document->activeLayer() : nullptr;
            auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(activeL);
            if (vecLayer) {
                float strokeHitRadius = std::max(8.0f / static_cast<float>(m_zoom), 5.0f);
                int hitStrokeIdx = vecLayer->findNearestStroke(docPt, strokeHitRadius);
                if (hitStrokeIdx >= 0 && hitStrokeIdx < static_cast<int>(vecLayer->strokes().size())) {
                    auto beforeStrokes = vecLayer->strokes();
                    auto& s = vecLayer->strokes()[hitStrokeIdx];
                    s.color = m_currentColor.rgba();

                    vecLayer->rasterizeAll();
                    m_document->composite();
                    if (m_document->undoStack()) {
                        m_document->undoStack()->push(new ModifyVectorStrokesCommand(
                            m_document, m_document->activeLayerIndex(), beforeStrokes, vecLayer->strokes(), "線色変更"));
                    }
                    update();
                }
            }
            event->accept();
            return;
        }

        if (m_currentTool == ToolType::VectorEllipse) {
            auto activeL = m_document ? m_document->activeLayer() : nullptr;
            if (activeL && activeL->type() == LayerType::Vector) {
                m_isCreatingVectorEllipse = true;
                m_vectorEllipseCenter = docPt;
                m_vectorEllipseCurrentPos = docPt;
                update();
            }
            event->accept();
            return;
        }

        if (m_currentTool == ToolType::VectorCircle) {
            auto activeL = m_document ? m_document->activeLayer() : nullptr;
            if (activeL && activeL->type() == LayerType::Vector) {
                m_isCreatingVectorCircle = true;
                m_vectorCircleCenter = docPt;
                m_vectorCircleCurrentPos = docPt;
                update();
            }
            event->accept();
            return;
        }

        if (m_currentTool == ToolType::VectorPolygon) {
            auto activeL = m_document ? m_document->activeLayer() : nullptr;
            if (activeL && activeL->type() == LayerType::Vector) {
                m_isCreatingVectorPolygon = true;
                m_vectorPolygonCenter = docPt;
                m_vectorPolygonCurrentPos = docPt;
                update();
            }
            event->accept();
            return;
        }

        auto activeL = m_document ? m_document->activeLayer() : nullptr;
        if (activeL && activeL->type() == LayerType::Group) {
            m_isMovingLayer = true;
            m_layerMoveStartDocPos = docPt;
            m_accumulatedDx = 0;
            m_accumulatedDy = 0;
            event->accept();
            return;
        }

        if (m_currentTool == ToolType::Pipette) {
            if (m_document) {
                QColor sampled = m_document->samplePixel(static_cast<int>(docPt.x()), static_cast<int>(docPt.y()));
                emit colorPicked(sampled);
            }
            event->accept();
            return;
        }

        if (m_currentTool == ToolType::RectSelect || m_currentTool == ToolType::EllipseSelect) {
            // 既存の選択矩形が存在し、ハンドルまたは内部がクリックされた場合は変形（リサイズ/移動）
            if (m_hasActiveRect && m_document && m_document->hasSelection()) {
                RectHandle handle = hitTestRectHandle(event->position());
                if (handle != RectHandle::None) {
                    m_isModifyingRect = true;
                    m_dragHandle = handle;
                    m_modifyStartViewPos = event->position();
                    m_modifyStartDocPos = docPt;
                    m_modifyOrigDocRect = m_activeDocRect;
                    m_modifyOrigViewRect = m_activeViewRect;
                    m_modifyOrigSelection = m_document->selection();
                    updateSelectionCursor(handle);
                    event->accept();
                    return;
                }
            }

            // ハンドル以外（外側）をクリックした場合は新規選択（または追加・削除選択）
            m_isSelecting = true;
            m_selectStartDocPos = docPt;
            m_selectCurDocPos = docPt;
            m_selectStartViewPos = event->position();
            m_selectCurViewPos = event->position();
            updateAntsTimer();
            event->accept();
            return;
        }

        if (m_currentTool == ToolType::LassoSelect) {
            m_isLassoSelecting = true;
            m_lassoPointsDoc.clear();
            m_lassoPointsDoc.push_back(docPt);
            m_lassoPointsView.clear();
            m_lassoPointsView.push_back(event->position());
            updateAntsTimer();
            event->accept();
            return;
        }

        if (m_currentTool == ToolType::ColorSelect) {
            executeColorSelection(docPt);
            event->accept();
            return;
        }

        if (m_currentTool == ToolType::MagicWand) {
            autoSelect(docPt);
            event->accept();
            return;
        }

        if (m_currentTool == ToolType::Bucket) {
            if (m_document && m_document->hasSelection()) {
                if (!m_document->isPixelSelected(static_cast<int>(docPt.x()), static_cast<int>(docPt.y()))) {
                    event->accept();
                    return; // 選択範囲外のクリックは無視
                }
            }
            fillBucket(docPt);
            event->accept();
            return;
        }

        if (m_currentTool == ToolType::TextBalloon || m_currentTool == ToolType::Text) {
            std::shared_ptr<TextLayer> textLayer = nullptr;
            int hitIdx = -1;
            auto activeL = m_document ? m_document->activeLayer() : nullptr;
            textLayer = std::dynamic_pointer_cast<TextLayer>(activeL);
            if (textLayer) {
                hitIdx = textLayer->findItemAt(docPt);
            }
            if (hitIdx < 0 && m_document) {
                for (size_t i = 0; i < m_document->layers().size(); ++i) {
                    auto tl = std::dynamic_pointer_cast<TextLayer>(m_document->layers()[i]);
                    if (tl && tl->isVisible()) {
                        int h = tl->findItemAt(docPt);
                        if (h >= 0) {
                            textLayer = tl;
                            hitIdx = h;
                            m_document->setActiveLayerIndex(static_cast<int>(i));
                            break;
                        }
                    }
                }
            }

            if (hitIdx >= 0 && textLayer) {
                m_selectedBalloonIndex = hitIdx;
                auto& item = textLayer->items()[hitIdx];
                m_balloonPreEditItem = item;

                m_balloonDragHandle = BalloonHandle::Body;
                if (item.hasTail) {
                    qreal handleRad = 15.0 / m_zoom;
                    QPointF center = item.rect.center();
                    QPointF tip = item.tailTip;
                    QPointF dir = tip - center;
                    float len = std::hypot(dir.x(), dir.y());
                    if (len > 1.0f) {
                        QPointF norm(-dir.y() / len, dir.x() / len);
                        float baseW = std::clamp(item.tailBaseWidth, 4.0f, static_cast<float>(std::min(item.rect.width(), item.rect.height())));
                        QPointF p1 = center + norm * (baseW * 0.5f);
                        float curveFactor = item.tailCurvature * 0.005f * len;
                        QPointF ctrl = (center + tip) * 0.5f + norm * curveFactor;

                        if (std::hypot(tip.x() - docPt.x(), tip.y() - docPt.y()) <= handleRad) {
                            m_balloonDragHandle = BalloonHandle::TailTip;
                        } else if (std::hypot(ctrl.x() - docPt.x(), ctrl.y() - docPt.y()) <= handleRad) {
                            m_balloonDragHandle = BalloonHandle::TailCurve;
                        } else if (std::hypot(p1.x() - docPt.x(), p1.y() - docPt.y()) <= handleRad) {
                            m_balloonDragHandle = BalloonHandle::TailBaseWidth;
                        }
                    }
                }

                if (m_balloonDragHandle == BalloonHandle::Body) {
                    m_isMovingBalloon = true;
                    m_balloonMoveOffset = docPt - item.rect.topLeft();
                } else {
                    m_isMovingTail = true;
                }
                emit textItemSelected(item);
            } else {
                m_selectedBalloonIndex = -1;
                m_isCreatingBalloon = true;
                m_balloonStartPos = docPt;
                m_balloonCurrentPos = docPt;
            }
            update();
            event->accept();
            return;
        }

        if (m_currentTool == ToolType::PanelFrame || m_currentTool == ToolType::PanelSplit) {
            std::shared_ptr<GroupLayer> targetPanel = m_editingPanel;
            if (!targetPanel) {
                auto activeL = m_document ? m_document->activeLayer() : nullptr;
                if (activeL && activeL->type() == LayerType::Group) {
                    auto grp = std::dynamic_pointer_cast<GroupLayer>(activeL);
                    if (grp && grp->isPanelFolder()) targetPanel = grp;
                }
            }
            if (!targetPanel && m_document) {
                targetPanel = m_document->findPanelFolderAt(docPt);
            }

            if (targetPanel && targetPanel->isPanelFolder() && !targetPanel->panelPolygon().isEmpty()) {
                const auto& poly = targetPanel->panelPolygon();
                qreal hitR = 10.0 / m_zoom;

                int hitVertex = -1;
                for (int i = 0; i < poly.size(); ++i) {
                    if (std::hypot(poly[i].x() - docPt.x(), poly[i].y() - docPt.y()) <= hitR) {
                        hitVertex = i;
                        break;
                    }
                }
                if (hitVertex >= 0) {
                    m_isModifyingPanel = true;
                    m_panelDragHandle = PanelHandle::Vertex;
                    m_panelDragIndex = hitVertex;
                    m_panelModifyStartDocPos = docPt;
                    m_panelOrigPolygon = poly;
                    m_editingPanel = targetPanel;
                    if (m_panelSettings) m_panelSettings->setFromPanel(targetPanel.get());
                    update();
                    event->accept();
                    return;
                }

                int hitEdge = -1;
                for (int i = 0; i < poly.size(); ++i) {
                    int next = (i + 1) % poly.size();
                    QPointF mid = (poly[i] + poly[next]) * 0.5f;
                    if (std::hypot(mid.x() - docPt.x(), mid.y() - docPt.y()) <= hitR) {
                        hitEdge = i;
                        break;
                    }
                }
                if (hitEdge >= 0) {
                    m_isModifyingPanel = true;
                    m_panelDragHandle = PanelHandle::Edge;
                    m_panelDragIndex = hitEdge;
                    m_panelModifyStartDocPos = docPt;
                    m_panelOrigPolygon = poly;
                    m_editingPanel = targetPanel;
                    if (m_panelSettings) m_panelSettings->setFromPanel(targetPanel.get());
                    update();
                    event->accept();
                    return;
                }
            }

            if (m_currentTool == ToolType::PanelFrame) {
                m_editingPanel = nullptr;
                m_isCreatingPanel = true;
                m_panelCreateStartDocPos = snapToBleed(docPt);
                m_panelCreateCurDocPos = m_panelCreateStartDocPos;
            } else if (m_currentTool == ToolType::PanelSplit) {
                m_isSplittingPanel = true;
                m_panelSplitStartDocPos = snapToBleed(docPt);
                m_panelSplitCurDocPos = m_panelSplitStartDocPos;
                if (targetPanel) m_editingPanel = targetPanel;
            }
            update();
            event->accept();
            return;
        }

        if (m_currentTool == ToolType::ThreeD) {
            auto threeD = currentThreeDLayer();
            if (threeD) {
                m_last3DMousePos = event->position();
                if (event->button() == Qt::LeftButton) {
                    if (event->modifiers() & Qt::ShiftModifier) {
                        m_isPanning3D = true;
                    } else {
                        m_isOrbiting3D = true;
                    }
                } else if (event->button() == Qt::MiddleButton) {
                    m_isPanning3D = true;
                } else if (event->button() == Qt::RightButton) {
                    m_isZooming3D = true;
                }
                event->accept();
                return;
            }
        }

        if (m_currentTool == ToolType::Gradient) {
            auto activeL = m_document ? m_document->activeLayer() : nullptr;
            if (activeL && activeL->type() == LayerType::Raster) {
                m_isDraggingGradient = true;
                m_gradientStartDocPos = docPt;
                m_gradientEndDocPos = docPt;
                update();
                event->accept();
                return;
            }
        }

        if (m_currentTool == ToolType::VectorControlPoint) {
            auto activeL = m_document ? m_document->activeLayer() : nullptr;
            auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(activeL);
            if (vecLayer) {
                float hitRadius = 8.0f / static_cast<float>(m_zoom);
                int hitStroke = -1, hitPt = -1;
                bool hitPoint = vecLayer->findNearestControlPoint(docPt, hitRadius, hitStroke, hitPt);

                float strokeHitRadius = std::max(6.0f / static_cast<float>(m_zoom), 4.0f);
                int hitStrokeIdx = hitPoint ? hitStroke : vecLayer->findNearestStroke(docPt, strokeHitRadius);

                switch (m_vectorControlPointMode) {
                case VectorControlPointMode::MoveOrAddPoint:
                    if (hitPoint) {
                        m_isDraggingControlPoint = true;
                        m_isAddedPointInMoveOrAdd = false;
                        m_dragStrokeIdx = hitStroke;
                        m_dragPointIdx = hitPt;
                        m_dragStartDocPos = docPt;
                        m_beforeVectorStrokes = vecLayer->strokes();
                        vecLayer->clearSelection();
                        vecLayer->selectStroke(hitStroke, true);
                        vecLayer->selectPoint(hitStroke, hitPt, true);
                    } else if (hitStrokeIdx >= 0) {
                        m_beforeVectorStrokes = vecLayer->strokes();
                        int newPtIdx = -1;
                        if (vecLayer->addPointToStroke(hitStrokeIdx, docPt, &newPtIdx)) {
                            m_isDraggingControlPoint = true;
                            m_isAddedPointInMoveOrAdd = true;
                            m_dragStrokeIdx = hitStrokeIdx;
                            m_dragPointIdx = newPtIdx;
                            m_dragStartDocPos = docPt;
                            vecLayer->clearSelection();
                            vecLayer->selectStroke(hitStrokeIdx, true);
                            vecLayer->selectPoint(hitStrokeIdx, newPtIdx, true);
                            m_document->composite();
                        }
                    } else {
                        vecLayer->clearSelection();
                    }
                    update();
                    break;

                case VectorControlPointMode::MovePoint:
                    m_isAddedPointInMoveOrAdd = false;
                    if (hitPoint) {
                        m_isDraggingControlPoint = true;
                        m_dragStrokeIdx = hitStroke;
                        m_dragPointIdx = hitPt;
                        m_dragStartDocPos = docPt;
                        m_beforeVectorStrokes = vecLayer->strokes();
                        vecLayer->clearSelection();
                        vecLayer->selectStroke(hitStroke, true);
                        vecLayer->selectPoint(hitStroke, hitPt, true);
                    } else if (hitStrokeIdx >= 0) {
                        vecLayer->clearSelection();
                        vecLayer->selectStroke(hitStrokeIdx, true);
                    } else {
                        vecLayer->clearSelection();
                    }
                    update();
                    break;

                case VectorControlPointMode::AddPoint:
                    if (hitStrokeIdx >= 0) {
                        m_beforeVectorStrokes = vecLayer->strokes();
                        if (vecLayer->addPointToStroke(hitStrokeIdx, docPt)) {
                            m_document->composite();
                            if (m_document->undoStack()) {
                                m_document->undoStack()->push(new ModifyVectorStrokesCommand(
                                    m_document, m_document->activeLayerIndex(), m_beforeVectorStrokes, vecLayer->strokes(), "制御点追加"));
                            }
                        }
                    }
                    update();
                    break;

                case VectorControlPointMode::DeletePoint:
                    if (hitPoint) {
                        m_beforeVectorStrokes = vecLayer->strokes();
                        if (vecLayer->removePointFromStroke(hitStroke, hitPt)) {
                            m_hoveredPointStrokeIdx = -1;
                            m_hoveredPointIdx = -1;
                            m_document->composite();
                            if (m_document->undoStack()) {
                                m_document->undoStack()->push(new ModifyVectorStrokesCommand(
                                    m_document, m_document->activeLayerIndex(), m_beforeVectorStrokes, vecLayer->strokes(), "制御点削除"));
                            }
                        }
                    }
                    update();
                    break;

                case VectorControlPointMode::MoveStroke:
                    if (hitStrokeIdx >= 0) {
                        m_isDraggingVectorStroke = true;
                        m_dragStrokeIdx = hitStrokeIdx;
                        m_dragStartDocPos = docPt;
                        m_dragLastDocPos = docPt;
                        m_beforeVectorStrokes = vecLayer->strokes();
                        vecLayer->clearSelection();
                        vecLayer->selectStroke(hitStrokeIdx, true);
                    } else {
                        vecLayer->clearSelection();
                    }
                    update();
                    break;

                case VectorControlPointMode::DuplicateMoveStroke:
                    if (hitStrokeIdx >= 0) {
                        m_beforeVectorStrokes = vecLayer->strokes();
                        int dupIdx = vecLayer->duplicateStroke(hitStrokeIdx);
                        if (dupIdx >= 0) {
                            m_isDraggingVectorStroke = true;
                            m_dragStrokeIdx = dupIdx;
                            m_dragStartDocPos = docPt;
                            m_dragLastDocPos = docPt;
                            m_document->composite();
                        }
                    }
                    update();
                    break;

                case VectorControlPointMode::RotateStroke:
                    if (hitStrokeIdx >= 0) {
                        m_isRotatingVectorStroke = true;
                        m_dragStrokeIdx = hitStrokeIdx;
                        m_dragStartDocPos = docPt;
                        m_rotateStrokeCenter = vecLayer->strokeCenter(hitStrokeIdx);
                        m_rotateStrokePrevAngle = static_cast<float>(std::atan2(docPt.y() - m_rotateStrokeCenter.y(), docPt.x() - m_rotateStrokeCenter.x()) * 180.0 / M_PI);
                        m_accumulatedRotateAngle = 0.0f;
                        m_beforeVectorStrokes = vecLayer->strokes();
                        vecLayer->clearSelection();
                        vecLayer->selectStroke(hitStrokeIdx, true);
                    } else {
                        vecLayer->clearSelection();
                    }
                    update();
                    break;

                case VectorControlPointMode::ConnectStrokes:
                    if (hitPoint) {
                        const auto& s = vecLayer->strokes()[hitStroke];
                        bool isEndPoint = !s.isClosed && (hitPt == 0 || hitPt == static_cast<int>(s.points.size()) - 1);
                        if (isEndPoint) {
                            m_isConnectingStrokes = true;
                            m_connectSourceStrokeIdx = hitStroke;
                            m_connectSourcePtIdx = hitPt;
                            m_connectTargetStrokeIdx = -1;
                            m_connectTargetPtIdx = -1;
                            m_beforeVectorStrokes = vecLayer->strokes();
                            vecLayer->clearSelection();
                            vecLayer->selectPoint(hitStroke, hitPt, true);
                        } else {
                            // ストロークの端以外の点がアクティブなときは何もしない
                            m_isConnectingStrokes = false;
                            m_connectSourceStrokeIdx = -1;
                            m_connectSourcePtIdx = -1;
                        }
                    } else {
                        m_isConnectingStrokes = false;
                        m_connectSourceStrokeIdx = -1;
                        m_connectSourcePtIdx = -1;
                    }
                    update();
                    break;

                case VectorControlPointMode::DeleteStroke:
                    if (hitStrokeIdx >= 0) {
                        m_beforeVectorStrokes = vecLayer->strokes();
                        if (vecLayer->removeStroke(hitStrokeIdx)) {
                            m_hoveredStrokeIdx = -1;
                            m_document->composite();
                            if (m_document->undoStack()) {
                                m_document->undoStack()->push(new ModifyVectorStrokesCommand(
                                    m_document, m_document->activeLayerIndex(), m_beforeVectorStrokes, vecLayer->strokes(), "ストローク削除"));
                            }
                        }
                    }
                    update();
                    break;
                }
            }
            event->accept();
            return;
        }

        if (m_currentTool == ToolType::VectorPressure) {
            auto activeL = m_document ? m_document->activeLayer() : nullptr;
            auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(activeL);
            if (vecLayer) {
                float hitRadius = 8.0f / static_cast<float>(m_zoom);
                int hitStroke = -1, hitPt = -1;
                bool hitPoint = vecLayer->findNearestControlPoint(docPt, hitRadius, hitStroke, hitPt);

                if (hitPoint) {
                    m_isDraggingPressure = true;
                    m_pressureStrokeIdx = hitStroke;
                    m_pressurePointIdx = hitPt;
                    m_pressureStartMouseViewPos = event->position();
                    m_pressureStartVal = vecLayer->strokes()[hitStroke].points[hitPt].pressure;
                    m_currentPressurePercent = std::clamp(static_cast<int>(std::round(m_pressureStartVal * 100.0f)), 0, 200);
                    m_beforeVectorStrokes = vecLayer->strokes();

                    vecLayer->clearSelection();
                    vecLayer->selectStroke(hitStroke, true);
                    vecLayer->selectPoint(hitStroke, hitPt, true);
                } else {
                    float strokeHitRadius = std::max(6.0f / static_cast<float>(m_zoom), 4.0f);
                    int hitStrokeIdx = vecLayer->findNearestStroke(docPt, strokeHitRadius);
                    if (hitStrokeIdx >= 0) {
                        vecLayer->clearSelection();
                        vecLayer->selectStroke(hitStrokeIdx, true);
                    } else {
                        vecLayer->clearSelection();
                    }
                }
                update();
            }
            event->accept();
            return;
        }

        m_showBrushOutline = true;
        m_lastHoverDocPos = docPt;
        strokeBegin(docPt, 1.0f);
        event->accept();
    }
}

void CanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    m_cursorDocPos = viewToDoc(event->position());

    if (m_isRotating) {
        QPointF center(width() * 0.5, height() * 0.5);
        QPointF prevVec = m_lastMouseViewPos - center;
        QPointF curVec = event->position() - center;
        if (prevVec.manhattanLength() > 4.0 && curVec.manhattanLength() > 4.0) {
            double prevAngle = std::atan2(prevVec.y(), prevVec.x()) * 180.0 / M_PI;
            double curAngle = std::atan2(curVec.y(), curVec.x()) * 180.0 / M_PI;
            double delta = curAngle - prevAngle;
            while (delta > 180.0) delta -= 360.0;
            while (delta < -180.0) delta += 360.0;

            double newAngle = m_rotationAngle + delta;
            if (event->modifiers() & Qt::ControlModifier) {
                newAngle = std::round(newAngle / 15.0) * 15.0;
            }
            setRotationAngle(newAngle);
        }
        m_lastMouseViewPos = event->position();
        event->accept();
        return;
    }

    if (m_isPanning) {
        QPointF delta = event->position() - m_lastMouseViewPos;
        panByScreenDelta(delta.x(), delta.y());
        m_lastMouseViewPos = event->position();
        event->accept();
        return;
    }

    if (m_isOrbiting3D || m_isPanning3D || m_isZooming3D) {
        auto threeD = currentThreeDLayer();
        if (threeD) {
            float dx = event->position().x() - m_last3DMousePos.x();
            float dy = event->position().y() - m_last3DMousePos.y();
            if (m_isOrbiting3D) {
                threeD->camera().orbit(dx * 0.5f, -dy * 0.5f);
            } else if (m_isPanning3D) {
                threeD->camera().pan(-dx * 0.005f, dy * 0.005f);
            } else if (m_isZooming3D) {
                threeD->camera().zoom(1.0f + dy * 0.01f);
            }
            threeD->markDirty();
            if (m_document) m_document->composite();
            if (m_threeDSettings) m_threeDSettings->setFromLayer(threeD.get());
            update();
        }
        m_last3DMousePos = event->position();
        event->accept();
        return;
    }

    if (m_currentTool == ToolType::Transform && m_isTransforming) {
        if (m_transformMode == TransformMode::Unified) {
            if (m_transformDragHandle != TransformHandle::None && (event->buttons() & Qt::LeftButton)) {
                QPointF curDocPos = m_cursorDocPos;
                Qt::KeyboardModifiers mods = event->modifiers();
                bool keepAspect = m_transformKeepAspect || (mods & Qt::ShiftModifier);
                bool fromCenter = m_transformFromCenter || (mods & Qt::AltModifier);
                bool isCtrl = (mods & Qt::ControlModifier);

                if (m_transformDragHandle == TransformHandle::Inside) {
                    QPointF delta = curDocPos - m_transformDragStartDocPos;
                    for (int i = 0; i < 4; ++i) {
                        m_transformCurrentQuad[i] = m_transformDragStartQuad[i] + delta;
                    }
                    m_transformPivot = m_transformDragStartPivot + delta;

                } else if (m_transformDragHandle == TransformHandle::Pivot) {
                    m_transformPivot = curDocPos;

                } else if (m_transformDragHandle == TransformHandle::RotateOuter) {
                    double curAngle = std::atan2(curDocPos.y() - m_transformPivot.y(), curDocPos.x() - m_transformPivot.x());
                    double deltaAngle = curAngle - m_transformDragStartAngle;

                    if (mods & Qt::ShiftModifier) {
                        double deg = deltaAngle * 180.0 / M_PI;
                        deg = std::round(deg / 15.0) * 15.0;
                        deltaAngle = deg * M_PI / 180.0;
                    }

                    QTransform rotT;
                    rotT.translate(m_transformPivot.x(), m_transformPivot.y());
                    rotT.rotateRadians(deltaAngle);
                    rotT.translate(-m_transformPivot.x(), -m_transformPivot.y());

                    for (int i = 0; i < 4; ++i) {
                        m_transformCurrentQuad[i] = rotT.map(m_transformDragStartQuad[i]);
                    }

                } else if (m_transformDragHandle >= TransformHandle::TopLeft && m_transformDragHandle <= TransformHandle::BottomLeft) {
                    int idx = 0;
                    if (m_transformDragHandle == TransformHandle::TopLeft) idx = 0;
                    else if (m_transformDragHandle == TransformHandle::TopRight) idx = 1;
                    else if (m_transformDragHandle == TransformHandle::BottomRight) idx = 2;
                    else if (m_transformDragHandle == TransformHandle::BottomLeft) idx = 3;

                    if (isCtrl) {
                        m_transformCurrentQuad[idx] = curDocPos;
                    } else {
                        int oppIdx = (idx + 2) % 4;
                        QPointF basePt = fromCenter ? m_transformPivot : m_transformDragStartQuad[oppIdx];
                        QPointF startVec = m_transformDragStartQuad[idx] - basePt;
                        QPointF curVec = curDocPos - basePt;

                        qreal lenStart = std::hypot(startVec.x(), startVec.y());
                        if (lenStart > 1.0) {
                            qreal scaleX = 1.0, scaleY = 1.0;
                            if (keepAspect) {
                                qreal proj = (curVec.x() * startVec.x() + curVec.y() * startVec.y()) / (lenStart * lenStart);
                                scaleX = scaleY = proj;
                            } else {
                                scaleX = curVec.x() / (startVec.x() == 0 ? 1.0 : startVec.x());
                                scaleY = curVec.y() / (startVec.y() == 0 ? 1.0 : startVec.y());
                            }

                            QTransform scaleT;
                            scaleT.translate(basePt.x(), basePt.y());
                            scaleT.scale(scaleX, scaleY);
                            scaleT.translate(-basePt.x(), -basePt.y());

                            for (int i = 0; i < 4; ++i) {
                                m_transformCurrentQuad[i] = scaleT.map(m_transformDragStartQuad[i]);
                            }
                        }
                    }

                } else if (m_transformDragHandle >= TransformHandle::TopEdge && m_transformDragHandle <= TransformHandle::LeftEdge) {
                    int i0 = 0, i1 = 1, opp0 = 3, opp1 = 2;
                    if (m_transformDragHandle == TransformHandle::TopEdge) { i0 = 0; i1 = 1; opp0 = 3; opp1 = 2; }
                    else if (m_transformDragHandle == TransformHandle::RightEdge) { i0 = 1; i1 = 2; opp0 = 0; opp1 = 3; }
                    else if (m_transformDragHandle == TransformHandle::BottomEdge) { i0 = 3; i1 = 2; opp0 = 0; opp1 = 1; }
                    else if (m_transformDragHandle == TransformHandle::LeftEdge) { i0 = 0; i1 = 3; opp0 = 1; opp1 = 2; }

                    if (isCtrl) {
                        QPointF delta = curDocPos - m_transformDragStartDocPos;
                        m_transformCurrentQuad[i0] = m_transformDragStartQuad[i0] + delta;
                        m_transformCurrentQuad[i1] = m_transformDragStartQuad[i1] + delta;
                    } else {
                        QPointF midOpp = (m_transformDragStartQuad[opp0] + m_transformDragStartQuad[opp1]) * 0.5;
                        QPointF basePt = fromCenter ? m_transformPivot : midOpp;
                        QPointF midEdge = (m_transformDragStartQuad[i0] + m_transformDragStartQuad[i1]) * 0.5;

                        QPointF startVec = midEdge - basePt;
                        QPointF curVec = curDocPos - basePt;
                        qreal lenStart = std::hypot(startVec.x(), startVec.y());

                        if (lenStart > 1.0) {
                            qreal scale = (curVec.x() * startVec.x() + curVec.y() * startVec.y()) / (lenStart * lenStart);
                            QPointF n = startVec / lenStart;
                            qreal dist = lenStart * (scale - 1.0);

                            m_transformCurrentQuad[i0] = m_transformDragStartQuad[i0] + n * dist;
                            m_transformCurrentQuad[i1] = m_transformDragStartQuad[i1] + n * dist;
                            if (fromCenter) {
                                m_transformCurrentQuad[opp0] = m_transformDragStartQuad[opp0] - n * dist;
                                m_transformCurrentQuad[opp1] = m_transformDragStartQuad[opp1] - n * dist;
                            }
                        }
                    }
                }

                update();
                event->accept();
                return;
            } else {
                TransformHandle h = hitTestTransformHandle(event->position());
                if (h != m_transformHoverHandle) {
                    m_transformHoverHandle = h;
                    updateTransformCursor(m_transformHoverHandle, event->modifiers());
                    update();
                }
            }
        } else if (m_transformMode == TransformMode::Warp) {
            if (m_warpSubMode == WarpSubMode::Mesh) {
                if (m_meshDragIdx >= 0 && (event->buttons() & Qt::LeftButton)) {
                    QPointF delta = m_cursorDocPos - m_transformDragStartDocPos;
                    m_meshCurrentPoints[m_meshDragIdx] += delta;
                    m_transformDragStartDocPos = m_cursorDocPos;
                    update();
                    event->accept();
                    return;
                } else {
                    int hoverIdx = -1;
                    qreal minD = 12.0;
                    for (int i = 0; i < static_cast<int>(m_meshCurrentPoints.size()); ++i) {
                        QPointF vp = docToView(m_meshCurrentPoints[i]);
                        qreal d = std::hypot(vp.x() - event->position().x(), vp.y() - event->position().y());
                        if (d < minD) { minD = d; hoverIdx = i; }
                    }
                    if (hoverIdx != m_meshHoverIdx) {
                        m_meshHoverIdx = hoverIdx;
                        setCursor(m_meshHoverIdx >= 0 ? Qt::CrossCursor : Qt::ArrowCursor);
                        update();
                    }
                }
            } else { // Brush
                if (m_isWarpDragging && (event->buttons() & Qt::LeftButton)) {
                    qreal deltaX = m_cursorDocPos.x() - m_warpLastMouseDocPos.x();
                    qreal deltaY = m_cursorDocPos.y() - m_warpLastMouseDocPos.y();
                    m_warpLastMouseDocPos = m_cursorDocPos;

                    int w = m_transformSourceRect.width();
                    int h = m_transformSourceRect.height();
                    if (w > 0 && h > 0 && !m_warpDx.empty()) {
                        qreal radius = m_warpBrushSize * 0.5;
                        qreal r2 = radius * radius;
                        qreal strength = (m_warpBrushStrength / 100.0);

                        qreal localMouseX = m_cursorDocPos.x() - m_transformSourceRect.left();
                        qreal localMouseY = m_cursorDocPos.y() - m_transformSourceRect.top();

                        int minX = std::max(0, static_cast<int>(std::floor(localMouseX - radius)));
                        int maxX = std::min(w - 1, static_cast<int>(std::ceil(localMouseX + radius)));
                        int minY = std::max(0, static_cast<int>(std::floor(localMouseY - radius)));
                        int maxY = std::min(h - 1, static_cast<int>(std::ceil(localMouseY + radius)));

                        for (int py = minY; py <= maxY; ++py) {
                            for (int px = minX; px <= maxX; ++px) {
                                qreal dx = px - localMouseX;
                                qreal dy = py - localMouseY;
                                qreal d2 = dx * dx + dy * dy;
                                if (d2 < r2) {
                                    qreal t = 1.0 - (d2 / r2);
                                    qreal weight = t * t * strength;
                                    int idx = py * w + px;
                                    m_warpDx[idx] += static_cast<float>(deltaX * weight);
                                    m_warpDy[idx] += static_cast<float>(deltaY * weight);
                                }
                            }
                        }
                    }
                    update();
                    event->accept();
                    return;
                } else {
                    update(); // ブラシ円追従のため
                }
            }
        } else if (m_transformMode == TransformMode::Cage) {
            if (m_cageDragIdx >= 0 && (event->buttons() & Qt::LeftButton)) {
                QPointF delta = m_cursorDocPos - m_transformDragStartDocPos;
                m_cageCurrentPoints[m_cageDragIdx] += delta;
                m_transformDragStartDocPos = m_cursorDocPos;
                update();
                event->accept();
                return;
            } else if (m_isCageClosed) {
                int hoverIdx = -1;
                qreal minD = 12.0;
                for (int i = 0; i < static_cast<int>(m_cageCurrentPoints.size()); ++i) {
                    QPointF vp = docToView(m_cageCurrentPoints[i]);
                    qreal d = std::hypot(vp.x() - event->position().x(), vp.y() - event->position().y());
                    if (d < minD) { minD = d; hoverIdx = i; }
                }
                if (hoverIdx != m_cageHoverIdx) {
                    m_cageHoverIdx = hoverIdx;
                    setCursor(m_cageHoverIdx >= 0 ? Qt::CrossCursor : Qt::ArrowCursor);
                    update();
                }
            } else {
                update(); // 作成中のケージ線追従のため
            }
        }
    }

    if (m_isModifyingRect && (event->buttons() & Qt::LeftButton)) {
        if (m_activeRectAlignToWindow) {
            QRectF r = m_modifyOrigViewRect;
            QPointF curView = event->position();
            qreal dx = curView.x() - m_modifyStartViewPos.x();
            qreal dy = curView.y() - m_modifyStartViewPos.y();

            switch (m_dragHandle) {
            case RectHandle::TopLeft:
                r.setTop(r.top() + dy);
                r.setLeft(r.left() + dx);
                break;
            case RectHandle::TopRight:
                r.setTop(r.top() + dy);
                r.setRight(r.right() + dx);
                break;
            case RectHandle::BottomLeft:
                r.setBottom(r.bottom() + dy);
                r.setLeft(r.left() + dx);
                break;
            case RectHandle::BottomRight:
                r.setBottom(r.bottom() + dy);
                r.setRight(r.right() + dx);
                break;
            case RectHandle::TopEdge:
                r.setTop(r.top() + dy);
                break;
            case RectHandle::BottomEdge:
                r.setBottom(r.bottom() + dy);
                break;
            case RectHandle::LeftEdge:
                r.setLeft(r.left() + dx);
                break;
            case RectHandle::RightEdge:
                r.setRight(r.right() + dx);
                break;
            case RectHandle::Inside:
                r.translate(dx, dy);
                break;
            default:
                break;
            }

            m_activeViewRect = r;
            QRectF normR = r.normalized();
            QPointF p0 = viewToDoc(normR.topLeft());
            QPointF p1 = viewToDoc(normR.topRight());
            QPointF p2 = viewToDoc(normR.bottomRight());
            QPointF p3 = viewToDoc(normR.bottomLeft());
            QPainterPath newPath;
            QPolygonF poly;
            poly << p0 << p1 << p2 << p3;
            newPath.addPolygon(poly);
            newPath.closeSubpath();
            if (m_document) {
                m_document->setSelection(newPath);
            }
        } else {
            QRectF r = m_modifyOrigDocRect;
            QPointF curDoc = m_cursorDocPos;
            qreal dx = curDoc.x() - m_modifyStartDocPos.x();
            qreal dy = curDoc.y() - m_modifyStartDocPos.y();

            switch (m_dragHandle) {
            case RectHandle::TopLeft:
                r.setTop(r.top() + dy);
                r.setLeft(r.left() + dx);
                break;
            case RectHandle::TopRight:
                r.setTop(r.top() + dy);
                r.setRight(r.right() + dx);
                break;
            case RectHandle::BottomLeft:
                r.setBottom(r.bottom() + dy);
                r.setLeft(r.left() + dx);
                break;
            case RectHandle::BottomRight:
                r.setBottom(r.bottom() + dy);
                r.setRight(r.right() + dx);
                break;
            case RectHandle::TopEdge:
                r.setTop(r.top() + dy);
                break;
            case RectHandle::BottomEdge:
                r.setBottom(r.bottom() + dy);
                break;
            case RectHandle::LeftEdge:
                r.setLeft(r.left() + dx);
                break;
            case RectHandle::RightEdge:
                r.setRight(r.right() + dx);
                break;
            case RectHandle::Inside:
                r.translate(dx, dy);
                break;
            default:
                break;
            }

            m_activeDocRect = r;
            QRectF normR = r.normalized();
            QPainterPath newPath;
            if (m_currentTool == ToolType::EllipseSelect) {
                newPath.addEllipse(normR);
            } else {
                newPath.addRect(normR);
            }
            if (m_document) {
                m_document->setSelection(newPath);
            }
        }
        update();
        event->accept();
        return;
    }

    if (m_isSelecting && (event->buttons() & Qt::LeftButton)) {
        m_selectCurDocPos = m_cursorDocPos;
        m_selectCurViewPos = event->position();
        update();
        event->accept();
        return;
    }

    if (m_isLassoSelecting && (event->buttons() & Qt::LeftButton)) {
        if (m_lassoPointsDoc.empty() ||
            (m_cursorDocPos - m_lassoPointsDoc.back()).manhattanLength() >= 1.5) {
            m_lassoPointsDoc.push_back(m_cursorDocPos);
            m_lassoPointsView.push_back(event->position());
            update();
        }
        event->accept();
        return;
    }

    if (m_isMovingLayer) {
        auto activeL = m_document ? m_document->activeLayer() : nullptr;
        if (activeL) {
            int curDx = static_cast<int>(std::round(m_cursorDocPos.x() - m_layerMoveStartDocPos.x()));
            int curDy = static_cast<int>(std::round(m_cursorDocPos.y() - m_layerMoveStartDocPos.y()));
            int stepDx = curDx - m_accumulatedDx;
            int stepDy = curDy - m_accumulatedDy;
            if (stepDx != 0 || stepDy != 0) {
                activeL->translate(stepDx, stepDy);
                m_accumulatedDx += stepDx;
                m_accumulatedDy += stepDy;
                m_document->composite();
                update();
            }
        }
        event->accept();
        return;
    }

    if (m_currentTool == ToolType::Pipette && (event->buttons() & Qt::LeftButton)) {
        if (m_document) {
            QColor sampled = m_document->samplePixel(static_cast<int>(m_cursorDocPos.x()), static_cast<int>(m_cursorDocPos.y()));
            emit colorPicked(sampled);
        }
        event->accept();
        return;
    }

    if (m_currentTool == ToolType::TextBalloon || m_currentTool == ToolType::Text) {
        if (m_isCreatingBalloon) {
            m_balloonCurrentPos = m_cursorDocPos;
            update();
            event->accept();
            return;
        }
        if (m_isMovingBalloon && m_selectedBalloonIndex >= 0) {
            auto textLayer = std::dynamic_pointer_cast<TextLayer>(m_document->activeLayer());
            if (textLayer && m_selectedBalloonIndex < static_cast<int>(textLayer->items().size())) {
                auto& item = textLayer->items()[m_selectedBalloonIndex];
                QPointF newTopLeft = m_cursorDocPos - m_balloonMoveOffset;
                QPointF delta = newTopLeft - item.rect.topLeft();
                item.rect.moveTopLeft(newTopLeft);
                if (item.hasTail) {
                    item.tailTip += delta;
                }
                textLayer->rasterizeAll();
                m_document->composite();
                update();
            }
            event->accept();
            return;
        }
        if (m_isMovingTail && m_selectedBalloonIndex >= 0) {
            auto textLayer = std::dynamic_pointer_cast<TextLayer>(m_document->activeLayer());
            if (textLayer && m_selectedBalloonIndex < static_cast<int>(textLayer->items().size())) {
                auto& item = textLayer->items()[m_selectedBalloonIndex];
                QPointF center = item.rect.center();

                if (m_balloonDragHandle == BalloonHandle::TailTip) {
                    item.tailTip = m_cursorDocPos;
                } else if (m_balloonDragHandle == BalloonHandle::TailCurve) {
                    QPointF tip = item.tailTip;
                    QPointF dir = tip - center;
                    float len = std::hypot(dir.x(), dir.y());
                    if (len > 1.0f) {
                        QPointF norm(-dir.y() / len, dir.x() / len);
                        QPointF mid = (center + tip) * 0.5f;
                        QPointF rel = m_cursorDocPos - mid;
                        float proj = rel.x() * norm.x() + rel.y() * norm.y();
                        item.tailCurvature = std::clamp(proj / (len * 0.005f), -100.0f, 100.0f);
                    }
                } else if (m_balloonDragHandle == BalloonHandle::TailBaseWidth) {
                    QPointF tip = item.tailTip;
                    QPointF dir = tip - center;
                    float len = std::hypot(dir.x(), dir.y());
                    if (len > 1.0f) {
                        QPointF norm(-dir.y() / len, dir.x() / len);
                        QPointF rel = m_cursorDocPos - center;
                        float proj = std::abs(rel.x() * norm.x() + rel.y() * norm.y());
                        item.tailBaseWidth = std::clamp(proj * 2.0f, 4.0f, 150.0f);
                    }
                }

                if (m_textSettings) {
                    m_textSettings->setFromItem(item);
                }

                textLayer->rasterizeAll();
                m_document->composite();
                update();
            }
            event->accept();
            return;
        }
        update();
        return;
    }

    if (m_currentTool == ToolType::PanelFrame || m_currentTool == ToolType::PanelSplit) {
        if (m_isModifyingPanel && m_editingPanel) {
            if (m_panelDragHandle == PanelHandle::Vertex && m_panelDragIndex >= 0 &&
                m_panelDragIndex < m_panelOrigPolygon.size()) {
                QPolygonF cur = m_panelOrigPolygon;
                cur[m_panelDragIndex] = snapToBleed(m_cursorDocPos);
                m_editingPanel->setPanelPolygon(cur);
                m_document->composite();
                update();
                event->accept();
                return;
            } else if (m_panelDragHandle == PanelHandle::Edge && m_panelDragIndex >= 0 &&
                       m_panelDragIndex < m_panelOrigPolygon.size()) {
                int idx1 = m_panelDragIndex;
                int idx2 = (m_panelDragIndex + 1) % m_panelOrigPolygon.size();
                QPointF delta = snapToBleed(m_cursorDocPos) - m_panelModifyStartDocPos;
                QPointF p1 = m_panelOrigPolygon[idx1];
                QPointF p2 = m_panelOrigPolygon[idx2];
                QPointF edgeVec = p2 - p1;
                double len = std::hypot(edgeVec.x(), edgeVec.y());
                if (len > 1e-4) {
                    QPointF norm(-edgeVec.y() / len, edgeVec.x() / len);
                    double proj = delta.x() * norm.x() + delta.y() * norm.y();
                    QPolygonF cur = m_panelOrigPolygon;
                    cur[idx1] = snapToBleed(p1 + norm * proj);
                    cur[idx2] = snapToBleed(p2 + norm * proj);
                    m_editingPanel->setPanelPolygon(cur);
                    m_document->composite();
                    update();
                    event->accept();
                    return;
                }
            }
        }
        if (m_isCreatingPanel) {
            m_panelCreateCurDocPos = snapToBleed(m_cursorDocPos);
            update();
            event->accept();
            return;
        }
        if (m_isSplittingPanel) {
            m_panelSplitCurDocPos = snapToBleed(m_cursorDocPos);
            update();
            event->accept();
            return;
        }

        std::shared_ptr<GroupLayer> targetPanel = m_editingPanel;
        if (!targetPanel && m_document) {
            auto activeL = m_document->activeLayer();
            if (activeL && activeL->type() == LayerType::Group) {
                auto grp = std::dynamic_pointer_cast<GroupLayer>(activeL);
                if (grp && grp->isPanelFolder()) targetPanel = grp;
            }
        }
        if (targetPanel && targetPanel->isPanelFolder() && !targetPanel->panelPolygon().isEmpty()) {
            const auto& poly = targetPanel->panelPolygon();
            qreal hitR = 10.0 / m_zoom;
            bool hit = false;
            for (const auto& pt : poly) {
                if (std::hypot(pt.x() - m_cursorDocPos.x(), pt.y() - m_cursorDocPos.y()) <= hitR) {
                    setCursor(Qt::SizeAllCursor);
                    hit = true;
                    break;
                }
            }
            if (!hit) {
                for (int i = 0; i < poly.size(); ++i) {
                    int next = (i + 1) % poly.size();
                    QPointF mid = (poly[i] + poly[next]) * 0.5f;
                    if (std::hypot(mid.x() - m_cursorDocPos.x(), mid.y() - m_cursorDocPos.y()) <= hitR) {
                        setCursor(Qt::CrossCursor);
                        hit = true;
                        break;
                    }
                }
            }
            if (!hit) {
                setCursor(Qt::ArrowCursor);
            }
        } else {
            setCursor(Qt::ArrowCursor);
        }
        update();
        return;
    }

    if (m_currentTool == ToolType::RectSelect || m_currentTool == ToolType::EllipseSelect) {
        if (m_hasActiveRect && !m_isSelecting && !m_isModifyingRect) {
            m_hoverHandle = hitTestRectHandle(event->position());
            updateSelectionCursor(m_hoverHandle);
        }
        update();
        return;
    }

    if (m_currentTool == ToolType::Gradient && m_isDraggingGradient) {
        m_gradientEndDocPos = m_cursorDocPos;
        update();
        event->accept();
        return;
    }

    if (m_isCreatingVectorLine) {
        m_vectorLineCurrentMousePos = m_cursorDocPos;
        update();
        event->accept();
        return;
    }

    if (m_isCreatingVectorEllipse) {
        m_vectorEllipseCurrentPos = m_cursorDocPos;
        update();
        event->accept();
        return;
    }

    if (m_isCreatingVectorCircle) {
        m_vectorCircleCurrentPos = m_cursorDocPos;
        update();
        event->accept();
        return;
    }

    if (m_isCreatingVectorPolygon) {
        m_vectorPolygonCurrentPos = m_cursorDocPos;
        update();
        event->accept();
        return;
    }

    if (m_currentTool == ToolType::VectorLineEdit || m_currentTool == ToolType::VectorColorChange) {
        auto activeL = m_document ? m_document->activeLayer() : nullptr;
        auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(activeL);
        if (vecLayer) {
            float strokeHitRadius = std::max(8.0f / static_cast<float>(m_zoom), 5.0f);
            m_hoveredStrokeIdx = vecLayer->findNearestStroke(m_cursorDocPos, strokeHitRadius);
            update();
        }
        event->accept();
        return;
    }

    if (m_currentTool == ToolType::VectorControlPoint) {
        auto activeL = m_document ? m_document->activeLayer() : nullptr;
        auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(activeL);
        if (vecLayer) {
            if (m_isDraggingControlPoint) {
                vecLayer->movePoint(m_dragStrokeIdx, m_dragPointIdx, m_cursorDocPos);
                m_document->composite();
                update();
            } else if (m_isDraggingVectorStroke) {
                float dx = static_cast<float>(m_cursorDocPos.x() - m_dragLastDocPos.x());
                float dy = static_cast<float>(m_cursorDocPos.y() - m_dragLastDocPos.y());
                m_dragLastDocPos = m_cursorDocPos;
                vecLayer->moveStroke(m_dragStrokeIdx, dx, dy);
                m_document->composite();
                update();
            } else if (m_isRotatingVectorStroke) {
                QPointF curVec = m_cursorDocPos - m_rotateStrokeCenter;
                if (curVec.manhattanLength() > 2.0) {
                    float curAngle = static_cast<float>(std::atan2(curVec.y(), curVec.x()) * 180.0 / M_PI);
                    float delta = curAngle - m_rotateStrokePrevAngle;
                    while (delta > 180.0f) delta -= 360.0f;
                    while (delta < -180.0f) delta += 360.0f;

                    m_rotateStrokePrevAngle = curAngle;
                    m_accumulatedRotateAngle += delta;

                    vecLayer->rotateStroke(m_dragStrokeIdx, delta, m_rotateStrokeCenter);
                    m_document->composite();
                    update();
                }
            } else if (m_isConnectingStrokes) {
                float snapRadius = std::max(20.0f / static_cast<float>(m_zoom), 12.0f);
                int targetStroke = -1;
                int targetPt = -1;
                float minD = snapRadius;
                for (size_t i = 0; i < vecLayer->strokes().size(); ++i) {
                    if (static_cast<int>(i) == m_connectSourceStrokeIdx) continue;
                    const auto& s = vecLayer->strokes()[i];
                    if (s.points.empty() || s.isClosed) continue;

                    // 別のストロークの端点のみ (始点0 または 終点末尾) を対象とする
                    float d0 = std::hypot(s.points.front().x - m_cursorDocPos.x(), s.points.front().y - m_cursorDocPos.y());
                    if (d0 < minD) {
                        minD = d0;
                        targetStroke = static_cast<int>(i);
                        targetPt = 0;
                    }
                    if (s.points.size() > 1) {
                        float d1 = std::hypot(s.points.back().x - m_cursorDocPos.x(), s.points.back().y - m_cursorDocPos.y());
                        if (d1 < minD) {
                            minD = d1;
                            targetStroke = static_cast<int>(i);
                            targetPt = static_cast<int>(s.points.size() - 1);
                        }
                    }
                }
                m_connectTargetStrokeIdx = targetStroke;
                m_connectTargetPtIdx = targetPt;
                update();
            } else {
                float hitRadius = 8.0f / static_cast<float>(m_zoom);
                int strokeIdx = -1, ptIdx = -1;
                if (vecLayer->findNearestControlPoint(m_cursorDocPos, hitRadius, strokeIdx, ptIdx)) {
                    // ConnectStrokes モードの場合、端点のみをアクティブ（ホバー）にする
                    if (m_vectorControlPointMode == VectorControlPointMode::ConnectStrokes) {
                        const auto& s = vecLayer->strokes()[strokeIdx];
                        bool isEndPoint = !s.isClosed && (ptIdx == 0 || ptIdx == static_cast<int>(s.points.size()) - 1);
                        if (isEndPoint) {
                            m_hoveredPointStrokeIdx = strokeIdx;
                            m_hoveredPointIdx = ptIdx;
                            m_hoveredStrokeIdx = -1;
                        } else {
                            m_hoveredPointStrokeIdx = -1;
                            m_hoveredPointIdx = -1;
                            m_hoveredStrokeIdx = -1;
                        }
                    } else {
                        m_hoveredPointStrokeIdx = strokeIdx;
                        m_hoveredPointIdx = ptIdx;
                        m_hoveredStrokeIdx = -1;
                    }
                } else {
                    m_hoveredPointStrokeIdx = -1;
                    m_hoveredPointIdx = -1;
                    if (m_vectorControlPointMode == VectorControlPointMode::ConnectStrokes) {
                        m_hoveredStrokeIdx = -1;
                    } else {
                        float strokeHitRadius = std::max(6.0f / static_cast<float>(m_zoom), 4.0f);
                        m_hoveredStrokeIdx = vecLayer->findNearestStroke(m_cursorDocPos, strokeHitRadius);
                    }
                }
                update();
            }
        }
        event->accept();
        return;
    }

    if (m_currentTool == ToolType::VectorPressure) {
        auto activeL = m_document ? m_document->activeLayer() : nullptr;
        auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(activeL);
        if (vecLayer) {
            if (m_isDraggingPressure && m_pressureStrokeIdx >= 0 &&
                m_pressureStrokeIdx < static_cast<int>(vecLayer->strokes().size())) {
                auto& stroke = vecLayer->strokes()[m_pressureStrokeIdx];
                if (m_pressurePointIdx >= 0 && m_pressurePointIdx < static_cast<int>(stroke.points.size())) {
                    float deltaX = event->position().x() - m_pressureStartMouseViewPos.x();
                    int startPercent = static_cast<int>(std::round(m_pressureStartVal * 100.0f));
                    int newPercent = std::clamp(startPercent + static_cast<int>(std::round(deltaX)), 0, 200);
                    m_currentPressurePercent = newPercent;
                    float newPressure = newPercent / 100.0f;

                    stroke.points[m_pressurePointIdx].pressure = newPressure;
                    vecLayer->rasterizeAll();
                    m_document->composite();
                    update();
                }
            } else {
                float hitRadius = 8.0f / static_cast<float>(m_zoom);
                int strokeIdx = -1, ptIdx = -1;
                if (vecLayer->findNearestControlPoint(m_cursorDocPos, hitRadius, strokeIdx, ptIdx)) {
                    m_hoveredPointStrokeIdx = strokeIdx;
                    m_hoveredPointIdx = ptIdx;
                    m_hoveredStrokeIdx = -1;
                } else {
                    m_hoveredPointStrokeIdx = -1;
                    m_hoveredPointIdx = -1;
                    float strokeHitRadius = std::max(6.0f / static_cast<float>(m_zoom), 4.0f);
                    m_hoveredStrokeIdx = vecLayer->findNearestStroke(m_cursorDocPos, strokeHitRadius);
                }
                update();
            }
        }
        event->accept();
        return;
    }

    m_showBrushOutline = true;
    if (m_isDrawing) {
        QPointF docPt = m_cursorDocPos;
        strokeMove(docPt, 1.0f);
        event->accept();
    } else {
        if (m_currentTool == ToolType::Marker && m_imageBrushFollowCursor) {
            float dx = static_cast<float>(m_cursorDocPos.x() - m_lastHoverDocPos.x());
            float dy = static_cast<float>(m_cursorDocPos.y() - m_lastHoverDocPos.y());
            if (dx * dx + dy * dy > 1.0f) {
                m_imageBrushLastFollowAngle = static_cast<float>(std::atan2(dy, dx) * 180.0 / 3.14159265358979323846);
            }
        }
        m_lastHoverDocPos = m_cursorDocPos;
        update(); // 輪郭カーソルの追従再描画
    }
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (m_isRotating && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        m_isRotating = false;
        if (m_isSpacePressed) {
            setCursor(Qt::OpenHandCursor);
        } else {
            updateCursorForActiveLayer();
        }
        event->accept();
        return;
    }

    if (m_isPanning && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        m_isPanning = false;
        if (m_isSpacePressed) {
            setCursor(Qt::OpenHandCursor);
        } else {
            updateCursorForActiveLayer();
        }
        event->accept();
        return;
    }

    if (m_isOrbiting3D || m_isPanning3D || m_isZooming3D) {
        m_isOrbiting3D = false;
        m_isPanning3D = false;
        m_isZooming3D = false;
        updateCursorForActiveLayer();
        event->accept();
        return;
    }

    if (m_isMovingLayer) {
        m_isMovingLayer = false;
        if (m_accumulatedDx != 0 || m_accumulatedDy != 0) {
            if (m_document && m_document->undoStack()) {
                m_document->undoStack()->push(new TranslateLayerCommand(m_document, m_document->activeLayerIndex(), m_accumulatedDx, m_accumulatedDy));
            }
        }
        updateCursorForActiveLayer();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_currentTool == ToolType::Transform) {
        if (m_transformDragHandle != TransformHandle::None) {
            m_transformDragHandle = TransformHandle::None;
            m_transformHoverHandle = hitTestTransformHandle(event->position());
            updateTransformCursor(m_transformHoverHandle, event->modifiers());
            update();
            event->accept();
            return;
        }
        if (m_meshDragIdx >= 0 || m_isWarpDragging || m_cageDragIdx >= 0) {
            m_meshDragIdx = -1;
            m_isWarpDragging = false;
            m_cageDragIdx = -1;
            update();
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::LeftButton && m_isModifyingRect) {
        m_isModifyingRect = false;
        m_activeDocRect = m_activeDocRect.normalized();
        m_activeViewRect = m_activeViewRect.normalized();

        if (m_document) {
            QPainterPath newSelection = m_document->selection();
            if (m_document->undoStack()) {
                m_document->undoStack()->push(new SelectCommand(m_document, m_modifyOrigSelection, newSelection));
            }
        }
        updateAntsTimer();
        m_hoverHandle = hitTestRectHandle(event->position());
        updateSelectionCursor(m_hoverHandle);
        update();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_isSelecting) {
        m_isSelecting = false;
        if (m_document) {
            m_selectCurDocPos = viewToDoc(event->position());
            m_selectCurViewPos = event->position();

            // 修飾キー判定 (Shift: 追加選択, Alt: 範囲削除, どちらでもなければ 各ツールのモード)
            SelectionMode baseMode = (m_currentTool == ToolType::EllipseSelect) ? m_ellipseSelectMode : m_selectionMode;
            SelectionMode effectiveMode = baseMode;
            if (event->modifiers() & Qt::ShiftModifier) {
                effectiveMode = SelectionMode::Add;
            } else if (event->modifiers() & Qt::AltModifier) {
                effectiveMode = SelectionMode::Subtract;
            }

            // 新規パス（矩形または楕円）の作成
            QPainterPath shapePath;
            if (m_currentTool == ToolType::EllipseSelect) {
                if (m_ellipseSelectMethod == EllipseSelectMethod::FromCenter) {
                    // ベクトルレイヤーの楕円ツールと同じ方式: 開始位置を中心、現在の位置との差を半径
                    qreal rx = std::abs(m_selectCurDocPos.x() - m_selectStartDocPos.x());
                    qreal ry = std::abs(m_selectCurDocPos.y() - m_selectStartDocPos.y());
                    if (rx >= 1.0 && ry >= 1.0) {
                        shapePath.addEllipse(m_selectStartDocPos, rx, ry);
                    }
                } else {
                    // BoundingBox 方式（外接矩形）
                    if (m_selectionAlignToWindow) {
                        qreal vx0 = std::min(m_selectStartViewPos.x(), m_selectCurViewPos.x());
                        qreal vx1 = std::max(m_selectStartViewPos.x(), m_selectCurViewPos.x());
                        qreal vy0 = std::min(m_selectStartViewPos.y(), m_selectCurViewPos.y());
                        qreal vy1 = std::max(m_selectStartViewPos.y(), m_selectCurViewPos.y());
                        if (std::abs(vx1 - vx0) >= 2.0 && std::abs(vy1 - vy0) >= 2.0) {
                            QRectF viewRect(vx0, vy0, vx1 - vx0, vy1 - vy0);
                            QPainterPath viewEllipse;
                            viewEllipse.addEllipse(viewRect);
                            shapePath = viewTransform().inverted().map(viewEllipse);
                        }
                    } else {
                        qreal dx0 = std::min(m_selectStartDocPos.x(), m_selectCurDocPos.x());
                        qreal dx1 = std::max(m_selectStartDocPos.x(), m_selectCurDocPos.x());
                        qreal dy0 = std::min(m_selectStartDocPos.y(), m_selectCurDocPos.y());
                        qreal dy1 = std::max(m_selectStartDocPos.y(), m_selectCurDocPos.y());
                        if (std::abs(dx1 - dx0) >= 2.0 && std::abs(dy1 - dy0) >= 2.0) {
                            shapePath.addEllipse(QRectF(dx0, dy0, dx1 - dx0, dy1 - dy0));
                        }
                    }
                }
            } else {
                // RectSelect
                if (m_selectionAlignToWindow) {
                    // 表示ウィンドウのX軸/Y軸に平行:
                    qreal vx0 = std::min(m_selectStartViewPos.x(), m_selectCurViewPos.x());
                    qreal vx1 = std::max(m_selectStartViewPos.x(), m_selectCurViewPos.x());
                    qreal vy0 = std::min(m_selectStartViewPos.y(), m_selectCurViewPos.y());
                    qreal vy1 = std::max(m_selectStartViewPos.y(), m_selectCurViewPos.y());

                    if (std::abs(vx1 - vx0) >= 2.0 && std::abs(vy1 - vy0) >= 2.0) {
                        QPointF p0 = viewToDoc(QPointF(vx0, vy0));
                        QPointF p1 = viewToDoc(QPointF(vx1, vy0));
                        QPointF p2 = viewToDoc(QPointF(vx1, vy1));
                        QPointF p3 = viewToDoc(QPointF(vx0, vy1));

                        QPolygonF poly;
                        poly << p0 << p1 << p2 << p3;
                        shapePath.addPolygon(poly);
                        shapePath.closeSubpath();
                    }
                } else {
                    // キャンバスのX軸/Y軸に平行:
                    qreal dx0 = std::min(m_selectStartDocPos.x(), m_selectCurDocPos.x());
                    qreal dx1 = std::max(m_selectStartDocPos.x(), m_selectCurDocPos.x());
                    qreal dy0 = std::min(m_selectStartDocPos.y(), m_selectCurDocPos.y());
                    qreal dy1 = std::max(m_selectStartDocPos.y(), m_selectCurDocPos.y());

                    if (std::abs(dx1 - dx0) >= 2.0 && std::abs(dy1 - dy0) >= 2.0) {
                        shapePath.addRect(QRectF(dx0, dy0, dx1 - dx0, dy1 - dy0));
                    }
                }
            }

            QPainterPath oldSelection = m_document->selection();
            QPainterPath newSelection;

            if (shapePath.isEmpty()) {
                // 微小ドラッグまたは単なるクリック
                if (effectiveMode == SelectionMode::New) {
                    // 通常選択で空クリックした場合は選択解除
                    newSelection = QPainterPath();
                    m_hasActiveRect = false;
                } else {
                    // 追加または削除で空クリックの場合は何もしない（元のまま）
                    newSelection = oldSelection;
                }
            } else {
                if (effectiveMode == SelectionMode::New) {
                    newSelection = shapePath;
                    m_hasActiveRect = true;
                    m_activeRectAlignToWindow = (m_currentTool == ToolType::EllipseSelect && m_ellipseSelectMethod == EllipseSelectMethod::FromCenter) ? false : m_selectionAlignToWindow;
                    if (m_activeRectAlignToWindow) {
                        qreal vx0 = std::min(m_selectStartViewPos.x(), m_selectCurViewPos.x());
                        qreal vx1 = std::max(m_selectStartViewPos.x(), m_selectCurViewPos.x());
                        qreal vy0 = std::min(m_selectStartViewPos.y(), m_selectCurViewPos.y());
                        qreal vy1 = std::max(m_selectStartViewPos.y(), m_selectCurViewPos.y());
                        m_activeViewRect = QRectF(vx0, vy0, vx1 - vx0, vy1 - vy0);
                        m_activeDocRect = newSelection.boundingRect();
                    } else {
                        m_activeDocRect = newSelection.boundingRect();
                        m_activeViewRect = viewTransform().mapRect(m_activeDocRect);
                    }
                } else if (effectiveMode == SelectionMode::Add) {
                    newSelection = oldSelection.united(shapePath);
                    m_hasActiveRect = true;
                    m_activeRectAlignToWindow = false;
                    m_activeDocRect = newSelection.boundingRect();
                    m_activeViewRect = viewTransform().mapRect(m_activeDocRect);
                } else if (effectiveMode == SelectionMode::Subtract) {
                    newSelection = oldSelection.subtracted(shapePath);
                    if (newSelection.isEmpty()) {
                        m_hasActiveRect = false;
                    } else {
                        m_hasActiveRect = true;
                        m_activeRectAlignToWindow = false;
                        m_activeDocRect = newSelection.boundingRect();
                        m_activeViewRect = viewTransform().mapRect(m_activeDocRect);
                    }
                }
            }

            if (m_document->undoStack()) {
                m_document->undoStack()->push(new SelectCommand(m_document, oldSelection, newSelection));
            } else {
                m_document->setSelection(newSelection);
            }
        }
        updateAntsTimer();
        m_hoverHandle = hitTestRectHandle(event->position());
        updateSelectionCursor(m_hoverHandle);
        update();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_isLassoSelecting) {
        m_isLassoSelecting = false;
        if (m_document && m_lassoPointsDoc.size() >= 3) {
            SelectionMode effectiveMode = m_lassoSelectMode;
            if (event->modifiers() & Qt::ShiftModifier) {
                effectiveMode = SelectionMode::Add;
            } else if (event->modifiers() & Qt::AltModifier) {
                effectiveMode = SelectionMode::Subtract;
            }

            QPolygonF docPoly(m_lassoPointsDoc.begin(), m_lassoPointsDoc.end());
            QPainterPath lassoPath;
            lassoPath.addPolygon(docPoly);
            lassoPath.closeSubpath();

            QPainterPath oldSelection = m_document->selection();
            QPainterPath newSelection;

            if (effectiveMode == SelectionMode::New) {
                newSelection = lassoPath;
            } else if (effectiveMode == SelectionMode::Add) {
                newSelection = oldSelection.united(lassoPath);
            } else if (effectiveMode == SelectionMode::Subtract) {
                newSelection = oldSelection.subtracted(lassoPath);
            }

            if (m_document->undoStack()) {
                m_document->undoStack()->push(new SelectCommand(m_document, oldSelection, newSelection));
            } else {
                m_document->setSelection(newSelection);
            }
            m_hasActiveRect = false;
        }
        m_lassoPointsDoc.clear();
        updateAntsTimer();
        update();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && (m_currentTool == ToolType::TextBalloon || m_currentTool == ToolType::Text)) {
        if (m_isCreatingBalloon) {
            m_isCreatingBalloon = false;
            auto textLayer = ensureTextLayer();
            if (textLayer) {
                QRectF r(m_balloonStartPos, m_balloonCurrentPos);
                r = r.normalized();
                if (m_currentTool == ToolType::Text) {
                    if (r.width() < 10 || r.height() < 10) {
                        r = QRectF(m_balloonStartPos.x(), m_balloonStartPos.y(), 200, 80);
                    }
                    TextBalloonItem newItem;
                    newItem.rect = r;
                    newItem.shape = BalloonShape::None;
                    newItem.hasTail = false;
                    newItem.text = tr("テキストを入力");
                    newItem.textColor = m_currentColor;
                    if (m_textSettings) {
                        m_textSettings->applyToItem(newItem);
                        if (!m_textSettings->text().isEmpty()) {
                            newItem.text = m_textSettings->text();
                        }
                    }
                    int idx = textLayer->addItem(newItem);
                    m_selectedBalloonIndex = idx;
                    if (m_document->undoStack()) {
                        m_document->undoStack()->push(new AddTextBalloonCommand(m_document, m_document->activeLayerIndex(), newItem));
                    }
                    m_document->composite();
                    emit textItemSelected(newItem);
                    if (m_textSettings) {
                        m_textSettings->focusTextEdit();
                    }
                } else {
                    if (r.width() < 20 || r.height() < 20) {
                        r = QRectF(m_balloonStartPos.x() - 100, m_balloonStartPos.y() - 65, 200, 130);
                    }
                    TextBalloonItem newItem;
                    newItem.rect = r;
                    newItem.shape = BalloonShape::Ellipse;
                    newItem.hasTail = true;
                    newItem.frameColor = m_currentColor;
                    newItem.tailTip = r.bottomRight() + QPointF(20, 30);
                    newItem.text = tr("セリフ");
                    if (m_textSettings) {
                        m_textSettings->applyToItem(newItem);
                        if (newItem.shape == BalloonShape::None) {
                            newItem.shape = BalloonShape::Ellipse;
                        }
                    }
                    int idx = textLayer->addItem(newItem);
                    m_selectedBalloonIndex = idx;
                    if (m_document->undoStack()) {
                        m_document->undoStack()->push(new AddTextBalloonCommand(m_document, m_document->activeLayerIndex(), newItem));
                    }
                    m_document->composite();
                    emit textItemSelected(newItem);
                    if (m_textSettings) {
                        m_textSettings->focusTextEdit();
                    }
                }
            }
            update();
            event->accept();
            return;
        }
        if (m_isMovingBalloon || m_isMovingTail) {
            bool moved = m_isMovingBalloon || m_isMovingTail;
            m_isMovingBalloon = false;
            m_isMovingTail = false;
            m_balloonDragHandle = BalloonHandle::None;
            auto textLayer = std::dynamic_pointer_cast<TextLayer>(m_document->activeLayer());
            if (moved && textLayer && m_selectedBalloonIndex >= 0 && m_selectedBalloonIndex < static_cast<int>(textLayer->items().size())) {
                const auto& curItem = textLayer->items()[m_selectedBalloonIndex];
                if (m_document->undoStack()) {
                    m_document->undoStack()->push(new EditTextBalloonCommand(m_document, m_document->activeLayerIndex(), m_selectedBalloonIndex, m_balloonPreEditItem, curItem));
                }
            }
            update();
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::LeftButton && (m_currentTool == ToolType::PanelFrame || m_currentTool == ToolType::PanelSplit)) {
        if (m_isCreatingPanel) {
            m_isCreatingPanel = false;
            QRectF r = QRectF(m_panelCreateStartDocPos, m_panelCreateCurDocPos).normalized();
            if (r.width() >= 10 && r.height() >= 10 && m_document) {
                QPolygonF poly;
                poly << r.topLeft() << r.topRight() << r.bottomRight() << r.bottomLeft();
                float bWidth = m_panelSettings ? m_panelSettings->borderWidth() : 3.0f;
                QColor bColor = m_panelSettings ? m_panelSettings->borderColor() : Qt::black;
                auto panel = m_document->addPanelFolder(poly, bWidth, bColor);
                if (panel && m_panelSettings) {
                    panel->setDrawPanelBorder(m_panelSettings->drawBorder());
                    m_panelSettings->setFromPanel(panel.get());
                }
                m_editingPanel = panel;
                m_document->composite();
            }
            update();
            event->accept();
            return;
        }
        if (m_isSplittingPanel) {
            m_isSplittingPanel = false;
            QLineF cutLine(m_panelSplitStartDocPos, m_panelSplitCurDocPos);
            if (cutLine.length() >= 10 && m_document) {
                auto targetPanel = m_editingPanel;
                if (!targetPanel || !targetPanel->isPanelFolder() || !targetPanel->panelPolygon().containsPoint(cutLine.center(), Qt::OddEvenFill)) {
                    targetPanel = m_document->findPanelFolderAt(cutLine.center());
                    if (!targetPanel) targetPanel = m_document->findPanelFolderAt(cutLine.p1());
                    if (!targetPanel) targetPanel = m_document->findPanelFolderAt(cutLine.p2());
                }
                if (targetPanel) {
                    float mx = m_panelSettings ? m_panelSettings->marginHorizontal() : 20.0f;
                    float my = m_panelSettings ? m_panelSettings->marginVertical() : 40.0f;
                    m_document->splitPanelFolder(targetPanel, cutLine, mx, my);
                    m_editingPanel = nullptr;
                    m_document->composite();
                }
            }
            update();
            event->accept();
            return;
        }
        if (m_isModifyingPanel) {
            m_isModifyingPanel = false;
            m_panelDragHandle = PanelHandle::None;
            m_panelDragIndex = -1;
            if (m_document) m_document->composite();
            update();
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::LeftButton && m_currentTool == ToolType::Gradient && m_isDraggingGradient) {
        m_isDraggingGradient = false;
        m_gradientEndDocPos = viewToDoc(event->position());
        renderGradient(m_gradientStartDocPos, m_gradientEndDocPos);
        update();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_currentTool == ToolType::VectorControlPoint) {
        auto activeL = m_document ? m_document->activeLayer() : nullptr;
        auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(activeL);
        if (vecLayer) {
            if (m_isDraggingControlPoint) {
                m_isDraggingControlPoint = false;
                if (m_dragStrokeIdx >= 0 && m_dragPointIdx >= 0) {
                    QPointF curDocPt = viewToDoc(event->position());
                    bool moved = std::hypot(curDocPt.x() - m_dragStartDocPos.x(), curDocPt.y() - m_dragStartDocPos.y()) > 0.1;
                    if (m_isAddedPointInMoveOrAdd) {
                        m_isAddedPointInMoveOrAdd = false;
                        if (m_document && m_document->undoStack()) {
                            m_document->undoStack()->push(new ModifyVectorStrokesCommand(
                                m_document, m_document->activeLayerIndex(), m_beforeVectorStrokes, vecLayer->strokes(), "制御点追加"));
                        }
                    } else if (moved) {
                        if (m_document && m_document->undoStack()) {
                            m_document->undoStack()->push(new ModifyVectorStrokesCommand(
                                m_document, m_document->activeLayerIndex(), m_beforeVectorStrokes, vecLayer->strokes(), "制御点移動"));
                        }
                    }
                }
            } else if (m_isDraggingVectorStroke) {
                m_isDraggingVectorStroke = false;
                QPointF curDocPt = viewToDoc(event->position());
                if (std::hypot(curDocPt.x() - m_dragStartDocPos.x(), curDocPt.y() - m_dragStartDocPos.y()) > 0.1 ||
                    m_vectorControlPointMode == VectorControlPointMode::DuplicateMoveStroke) {
                    if (m_document && m_document->undoStack()) {
                        QString desc = (m_vectorControlPointMode == VectorControlPointMode::DuplicateMoveStroke) ? "ストローク複製移動" : "ストローク移動";
                        m_document->undoStack()->push(new ModifyVectorStrokesCommand(
                            m_document, m_document->activeLayerIndex(), m_beforeVectorStrokes, vecLayer->strokes(), desc));
                    }
                }
            } else if (m_isRotatingVectorStroke) {
                m_isRotatingVectorStroke = false;
                if (std::abs(m_accumulatedRotateAngle) > 0.05f) {
                    if (m_document && m_document->undoStack()) {
                        m_document->undoStack()->push(new ModifyVectorStrokesCommand(
                            m_document, m_document->activeLayerIndex(), m_beforeVectorStrokes, vecLayer->strokes(), "ストローク回転"));
                    }
                }
                m_accumulatedRotateAngle = 0.0f;
            } else if (m_isConnectingStrokes) {
                m_isConnectingStrokes = false;
                QPointF curDocPt = viewToDoc(event->position());
                // ドラッグ終了位置で別のストロークの端点判定を再確認
                float snapRadius = std::max(20.0f / static_cast<float>(m_zoom), 12.0f);
                int targetStroke = -1;
                int targetPt = -1;
                float minD = snapRadius;
                for (size_t i = 0; i < vecLayer->strokes().size(); ++i) {
                    if (static_cast<int>(i) == m_connectSourceStrokeIdx) continue;
                    const auto& s = vecLayer->strokes()[i];
                    if (s.points.empty() || s.isClosed) continue;

                    float d0 = std::hypot(s.points.front().x - curDocPt.x(), s.points.front().y - curDocPt.y());
                    if (d0 < minD) {
                        minD = d0;
                        targetStroke = static_cast<int>(i);
                        targetPt = 0;
                    }
                    if (s.points.size() > 1) {
                        float d1 = std::hypot(s.points.back().x - curDocPt.x(), s.points.back().y - curDocPt.y());
                        if (d1 < minD) {
                            minD = d1;
                            targetStroke = static_cast<int>(i);
                            targetPt = static_cast<int>(s.points.size() - 1);
                        }
                    }
                }
                if (targetStroke < 0) {
                    targetStroke = m_connectTargetStrokeIdx;
                    targetPt = m_connectTargetPtIdx;
                }

                // 別のストロークの端点でドラッグ終了した場合、対象の制御点2点を接続し1つのストロークとする
                if (targetStroke >= 0 && targetPt >= 0 && m_connectSourceStrokeIdx >= 0) {
                    if (vecLayer->connectStrokes(m_connectSourceStrokeIdx, m_connectSourcePtIdx, targetStroke, targetPt)) {
                        m_document->composite();
                        if (m_document && m_document->undoStack()) {
                            m_document->undoStack()->push(new ModifyVectorStrokesCommand(
                                m_document, m_document->activeLayerIndex(), m_beforeVectorStrokes, vecLayer->strokes(), "ストローク連結"));
                        }
                    }
                }
                m_connectSourceStrokeIdx = -1;
                m_connectSourcePtIdx = -1;
                m_connectTargetStrokeIdx = -1;
                m_connectTargetPtIdx = -1;
            }
        }
        update();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_currentTool == ToolType::VectorPressure) {
        auto activeL = m_document ? m_document->activeLayer() : nullptr;
        auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(activeL);
        if (vecLayer && m_isDraggingPressure) {
            m_isDraggingPressure = false;
            if (m_pressureStrokeIdx >= 0 && m_pressureStrokeIdx < static_cast<int>(vecLayer->strokes().size())) {
                const auto& stroke = vecLayer->strokes()[m_pressureStrokeIdx];
                if (m_pressurePointIdx >= 0 && m_pressurePointIdx < static_cast<int>(stroke.points.size())) {
                    float finalPressure = stroke.points[m_pressurePointIdx].pressure;
                    if (std::abs(finalPressure - m_pressureStartVal) > 0.001f) {
                        if (m_document && m_document->undoStack()) {
                            m_document->undoStack()->push(new ModifyVectorStrokesCommand(
                                m_document, m_document->activeLayerIndex(), m_beforeVectorStrokes, vecLayer->strokes(), "筆圧変更"));
                        }
                    }
                }
            }
            update();
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_isCreatingVectorEllipse) {
        m_isCreatingVectorEllipse = false;
        auto activeL = m_document ? m_document->activeLayer() : nullptr;
        auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(activeL);
        if (vecLayer) {
            float rx = std::abs(static_cast<float>(m_vectorEllipseCurrentPos.x() - m_vectorEllipseCenter.x()));
            float ry = std::abs(static_cast<float>(m_vectorEllipseCurrentPos.y() - m_vectorEllipseCenter.y()));
            if (rx >= 1.0f || ry >= 1.0f) {
                VectorStroke stroke = VectorStroke::createEllipse(
                    m_vectorEllipseCenter, rx, ry,
                    m_vectorEllipseSize, m_currentColor.rgba(),
                    m_vectorEllipseDensity, m_vectorEllipseHardness,
                    m_vectorEllipseFalloff, m_vectorEllipseFilled);
                vecLayer->addStroke(stroke);
                m_document->composite();
                if (m_document->undoStack()) {
                    m_document->undoStack()->push(new AddVectorStrokeCommand(
                        m_document, m_document->activeLayerIndex(), stroke));
                }
            }
        }
        update();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_isCreatingVectorCircle) {
        m_isCreatingVectorCircle = false;
        auto activeL = m_document ? m_document->activeLayer() : nullptr;
        auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(activeL);
        if (vecLayer) {
            float r = std::hypot(static_cast<float>(m_vectorCircleCurrentPos.x() - m_vectorCircleCenter.x()),
                                 static_cast<float>(m_vectorCircleCurrentPos.y() - m_vectorCircleCenter.y()));
            if (r >= 1.0f) {
                VectorStroke stroke = VectorStroke::createEllipse(
                    m_vectorCircleCenter, r, r,
                    m_vectorCircleSize, m_currentColor.rgba(),
                    m_vectorCircleDensity, m_vectorCircleHardness,
                    m_vectorCircleFalloff, m_vectorCircleFilled);
                vecLayer->addStroke(stroke);
                m_document->composite();
                if (m_document->undoStack()) {
                    m_document->undoStack()->push(new AddVectorStrokeCommand(
                        m_document, m_document->activeLayerIndex(), stroke));
                }
            }
        }
        update();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_isCreatingVectorPolygon) {
        m_isCreatingVectorPolygon = false;
        auto activeL = m_document ? m_document->activeLayer() : nullptr;
        auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(activeL);
        if (vecLayer) {
            float dx = static_cast<float>(m_vectorPolygonCurrentPos.x() - m_vectorPolygonCenter.x());
            float dy = static_cast<float>(m_vectorPolygonCurrentPos.y() - m_vectorPolygonCenter.y());
            float rx = std::abs(dx);
            float ry = std::abs(dy);
            float dist = std::hypot(dx, dy);

            if ((m_vectorPolygonRegular && dist >= 1.0f) || (!m_vectorPolygonRegular && (rx >= 1.0f || ry >= 1.0f))) {
                constexpr double PI = 3.14159265358979323846;
                float rotAngle = m_vectorPolygonRegular ? static_cast<float>(std::atan2(dy, dx)) : static_cast<float>(-PI * 0.5);
                float useRx = m_vectorPolygonRegular ? dist : rx;
                float useRy = m_vectorPolygonRegular ? dist : ry;

                VectorStroke stroke = VectorStroke::createPolygon(
                    m_vectorPolygonCenter, useRx, useRy,
                    m_vectorPolygonSides, m_vectorPolygonRegular, rotAngle,
                    m_vectorPolygonSize, m_currentColor.rgba(),
                    m_vectorPolygonDensity, m_vectorPolygonHardness,
                    m_vectorPolygonFalloff, m_vectorPolygonFilled);
                vecLayer->addStroke(stroke);
                m_document->composite();
                if (m_document->undoStack()) {
                    m_document->undoStack()->push(new AddVectorStrokeCommand(
                        m_document, m_document->activeLayerIndex(), stroke));
                }
            }
        }
        update();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_isDrawing) {
        strokeEnd();
        event->accept();
    }
}

void CanvasWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_isCreatingVectorLine) {
        if (m_inProgressVectorStroke.points.size() >= 2) {
            const auto& last = m_inProgressVectorStroke.points.back();
            const auto& prev = m_inProgressVectorStroke.points[m_inProgressVectorStroke.points.size() - 2];
            if (std::hypot(last.x - prev.x, last.y - prev.y) < 2.0f) {
                m_inProgressVectorStroke.points.pop_back();
            }
        }
        finishVectorLine();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && (m_currentTool == ToolType::TextBalloon || m_currentTool == ToolType::Text) && m_document) {
        auto activeL = m_document->activeLayer();
        auto textLayer = std::dynamic_pointer_cast<TextLayer>(activeL);
        if (textLayer) {
            QPointF docPt = viewToDoc(event->position());
            int hitIdx = textLayer->findItemAt(docPt);
            if (hitIdx >= 0) {
                m_selectedBalloonIndex = hitIdx;
                auto& item = textLayer->items()[hitIdx];
                emit textItemSelected(item);
                if (m_currentTool == ToolType::Text) {
                    if (m_textSettings) {
                        m_textSettings->focusTextEdit();
                    }
                } else {
                    auto oldItem = item;
                    TextBalloonDialog dlg(oldItem, this);
                    if (dlg.exec() == QDialog::Accepted) {
                        auto newItem = dlg.getItem();
                        textLayer->updateItem(hitIdx, newItem);
                        if (m_document->undoStack()) {
                            m_document->undoStack()->push(new EditTextBalloonCommand(m_document, m_document->activeLayerIndex(), hitIdx, oldItem, newItem));
                        }
                        m_document->composite();
                    }
                }
                update();
                event->accept();
                return;
            }
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

void CanvasWidget::wheelEvent(QWheelEvent* event) {
    auto threeD = currentThreeDLayer();
    if (threeD) {
        float factor = (event->angleDelta().y() > 0) ? 0.9f : 1.1f;
        threeD->camera().zoom(factor);
        threeD->markDirty();
        if (m_document) m_document->composite();
        if (m_threeDSettings) m_threeDSettings->setFromLayer(threeD.get());
        update();
        event->accept();
        return;
    }

    if (event->modifiers() & Qt::AltModifier) {
        // Alt + ホイールでキャンバス回転 (5°ステップ)
        double delta = (event->angleDelta().y() > 0) ? -5.0 : 5.0;
        setRotationAngle(m_rotationAngle + delta);
        event->accept();
        return;
    }
    double factor = std::pow(1.0015, event->angleDelta().y());
    setZoom(m_zoom * factor);
    event->accept();
}

void CanvasWidget::keyPressEvent(QKeyEvent* event) {
    if (m_isTransforming) {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            applyTransform();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Escape) {
            cancelTransform();
            event->accept();
            return;
        }
    }

    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && m_isCreatingVectorLine) {
        finishVectorLine();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && m_isCreatingVectorLine) {
        cancelVectorLine();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_isSpacePressed = true;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }

    switch (event->key()) {
    case Qt::Key_Delete: // SAI互換: 反時計回りに15°回転
        setRotationAngle(m_rotationAngle - 15.0);
        event->accept();
        return;
    case Qt::Key_End: // SAI互換: 時計回りに15°回転
        setRotationAngle(m_rotationAngle + 15.0);
        event->accept();
        return;
    case Qt::Key_Insert: // SAI互換: 回転リセット
    case Qt::Key_Home:
        setRotationAngle(0.0);
        event->accept();
        return;
    case Qt::Key_H: // 左右反転表示
        toggleFlipHorizontally();
        event->accept();
        return;
    case Qt::Key_PageDown: // ズームイン
        setZoom(m_zoom * 1.2);
        event->accept();
        return;
    case Qt::Key_PageUp: // ズームアウト
        setZoom(m_zoom / 1.2);
        event->accept();
        return;
    default:
        break;
    }

    QWidget::keyPressEvent(event);
}

void CanvasWidget::keyReleaseEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_isSpacePressed = false;
        if (!m_isPanning && !m_isRotating) {
            updateCursorForActiveLayer();
        }
        event->accept();
        return;
    }
    QWidget::keyReleaseEvent(event);
}

bool CanvasWidget::event(QEvent* event) {
    // マルチタッチ (Android & ペンタブレット) 処理
    if (event->type() == QEvent::TouchBegin || event->type() == QEvent::TouchUpdate || event->type() == QEvent::TouchEnd) {
        QTouchEvent* touch = static_cast<QTouchEvent*>(event);
        const auto& points = touch->points();

        if (points.size() == 2) {
            // 2本指によるピンチズーム・回転・パン
            auto p1 = points[0];
            auto p2 = points[1];

            QPointF curCenter = (p1.position() + p2.position()) * 0.5;
            QPointF prevCenter = (p1.pressPosition() + p2.pressPosition()) * 0.5;
            m_panOffset += (curCenter - prevCenter) * 0.1;

            qreal prevDist = std::hypot(p1.pressPosition().x() - p2.pressPosition().x(), p1.pressPosition().y() - p2.pressPosition().y());
            qreal curDist = std::hypot(p1.position().x() - p2.position().x(), p1.position().y() - p2.position().y());
            if (prevDist > 10.0) {
                m_zoom = std::clamp(m_zoom * (curDist / prevDist), 0.05, 4.0);
            }

            // 2本指によるピンチ回転
            QPointF prevVec = p2.pressPosition() - p1.pressPosition();
            QPointF curVec = p2.position() - p1.position();
            if (prevVec.manhattanLength() > 10.0 && curVec.manhattanLength() > 10.0) {
                double prevAngle = std::atan2(prevVec.y(), prevVec.x()) * 180.0 / M_PI;
                double curAngle = std::atan2(curVec.y(), curVec.x()) * 180.0 / M_PI;
                double delta = curAngle - prevAngle;
                while (delta > 180.0) delta -= 360.0;
                while (delta < -180.0) delta += 360.0;
                m_rotationAngle = std::fmod(m_rotationAngle + delta * 0.5, 360.0);
                if (m_rotationAngle < 0) m_rotationAngle += 360.0;
            }

            update();
            notifyTransformChanged();
            return true;
        }
    }
    return QWidget::event(event);
}

float CanvasWidget::currentBrushOutlineRadius() const {
    auto activeL = m_document ? m_document->activeLayer() : nullptr;
    if (activeL && activeL->type() == LayerType::Vector && m_currentTool == ToolType::Eraser) {
        return m_vectorEraserSize * 0.5f;
    }
    if (m_currentTool == ToolType::Blur) {
        return m_blurSize * 0.5f;
    } else if (m_currentTool == ToolType::Blend) {
        return m_smudgeSize * 0.5f;
    } else if (m_currentTool == ToolType::Watercolor) {
        return m_watercolorSize * 0.5f;
    } else if (m_currentTool == ToolType::CompositeBrush) {
        return m_compositeBrushSize * 0.5f;
    } else if (m_currentTool == ToolType::Marker) {
        return m_imageBrushSize * 0.5f;
    } else {
        return m_brushSize * 0.5f;
    }
}

void CanvasWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    // タイル境界の継ぎ目（アンチエイリアスによる白線の隙間・格子状の線）の発生を完全に防ぐため、
    // タイル画像描画時は Antialiasing を無効化する（ピクセル補間は SmoothPixmapTransform が担当）
    painter.setRenderHint(QPainter::Antialiasing, false);

    // キャンバス背景 (SAI風のグレー #888888)
    painter.fillRect(rect(), QColor(136, 136, 136));

    if (!m_document) return;

    painter.save();
    painter.setTransform(viewTransform());

    const auto& filterMgr = DisplayFilterManager::instance();
    bool hasDisplayFilter = (filterMgr.filterType() != DisplayFilterType::None);

    // キャンバス境界 (白い用紙枠)
    QRect docRect(0, 0, m_document->width(), m_document->height());
    painter.fillRect(docRect, hasDisplayFilter ? filterMgr.applyFilter(Qt::white) : Qt::white);

    // タイルマネージャから描画
    const auto& comp = m_document->compositeBuffer();
    for (int ty = 0; ty < comp.tilesY(); ++ty) {
        for (int tx = 0; tx < comp.tilesX(); ++tx) {
            const Tile* tile = comp.getTile(tx, ty);
            if (!tile || !tile->readBuffer()) continue;

            QImage tileImg(tile->readBuffer(), TileMemoryPool::TILE_WIDTH, TileMemoryPool::TILE_HEIGHT,
                           TileMemoryPool::TILE_WIDTH * 4, QImage::Format_ARGB32_Premultiplied);

            int x = tx * TileMemoryPool::TILE_WIDTH;
            int y = ty * TileMemoryPool::TILE_HEIGHT;
            if (hasDisplayFilter) {
                painter.drawImage(x, y, filterMgr.applyFilter(tileImg));
            } else {
                painter.drawImage(x, y, tileImg);
            }
        }
    }

    // 選択範囲編集モードオーバーレイ（青色半透明マスク表示）
    if (m_document->isSelectionEditMode() && m_document->selectionMaskLayer()) {
        auto maskLayer = m_document->selectionMaskLayer();
        const auto& maskMgr = maskLayer->tileManager();
        for (int ty = 0; ty < maskMgr.tilesY(); ++ty) {
            for (int tx = 0; tx < maskMgr.tilesX(); ++tx) {
                const Tile* tile = maskMgr.getTile(tx, ty);
                if (!tile || !tile->readBuffer()) continue;

                QImage maskImg(TileMemoryPool::TILE_WIDTH, TileMemoryPool::TILE_HEIGHT, QImage::Format_ARGB32_Premultiplied);
                maskImg.fill(0);
                const uint32_t* src = reinterpret_cast<const uint32_t*>(tile->readBuffer());
                uint32_t* dst = reinterpret_cast<uint32_t*>(maskImg.bits());
                int count = TileMemoryPool::TILE_WIDTH * TileMemoryPool::TILE_HEIGHT;
                for (int i = 0; i < count; ++i) {
                    uint32_t a = (src[i] >> 24) & 0xFF;
                    if (a > 0) {
                        uint32_t outA = (a * 120) / 255; // 半透明アルファ (最大120)
                        uint32_t outR = (0 * outA) / 255;
                        uint32_t outG = (100 * outA) / 255;
                        uint32_t outB = (255 * outA) / 255;
                        dst[i] = (outA << 24) | (outR << 16) | (outG << 8) | outB;
                    } else {
                        dst[i] = 0;
                    }
                }
                int x = tx * TileMemoryPool::TILE_WIDTH;
                int y = ty * TileMemoryPool::TILE_HEIGHT;
                painter.drawImage(x, y, maskImg);
            }
        }
    }

    // キャンバス外枠線
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QColor(80, 80, 80));
    painter.drawRect(docRect);

    // 4. テキスト・吹き出し操作オーバーレイ (プレビュー / 選択枠)
    if (m_currentTool == ToolType::TextBalloon || m_currentTool == ToolType::Text) {
        painter.setRenderHint(QPainter::Antialiasing, true);
        qreal invZoom = 1.0 / m_zoom;

        if (m_isCreatingBalloon) {
            QRectF r = QRectF(m_balloonStartPos, m_balloonCurrentPos).normalized();
            painter.setPen(QPen(QColor(0, 120, 215), 1.5 * invZoom, Qt::DashLine));
            painter.setBrush(QColor(0, 120, 215, 30));
            if (m_currentTool == ToolType::Text) {
                painter.drawRect(r);
            } else {
                painter.drawEllipse(r);
            }
        }

        auto textLayer = std::dynamic_pointer_cast<TextLayer>(m_document->activeLayer());
        if (textLayer && m_selectedBalloonIndex >= 0 && m_selectedBalloonIndex < static_cast<int>(textLayer->items().size())) {
            const auto& item = textLayer->items()[m_selectedBalloonIndex];
            painter.setPen(QPen(QColor(0, 120, 215), 1.5 * invZoom, Qt::DashLine));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(item.rect);

            if (item.hasTail) {
                QPointF center = item.rect.center();
                QPointF tip = item.tailTip;
                QPointF dir = tip - center;
                float len = std::hypot(dir.x(), dir.y());
                if (len > 1.0f) {
                    QPointF norm(-dir.y() / len, dir.x() / len);
                    float baseW = std::clamp(item.tailBaseWidth, 4.0f, static_cast<float>(std::min(item.rect.width(), item.rect.height())));
                    QPointF p1 = center + norm * (baseW * 0.5f);
                    float curveFactor = item.tailCurvature * 0.005f * len;
                    QPointF ctrl = (center + tip) * 0.5f + norm * curveFactor;

                    // ガイドライン (薄い点線)
                    painter.setPen(QPen(QColor(0, 150, 255, 120), 1.0 * invZoom, Qt::DotLine));
                    painter.drawLine(center, tip);
                    painter.drawLine(p1, ctrl);

                    // 1. しっぽ先端ハンドル (橙丸)
                    painter.setPen(QPen(Qt::white, 1.2 * invZoom));
                    painter.setBrush(QColor(255, 120, 0));
                    painter.drawEllipse(tip, 6.0 * invZoom, 6.0 * invZoom);

                    // 2. カーブ曲率ハンドル (水色丸)
                    painter.setPen(QPen(Qt::white, 1.2 * invZoom));
                    painter.setBrush(QColor(0, 180, 255));
                    painter.drawEllipse(ctrl, 5.0 * invZoom, 5.0 * invZoom);

                    // 3. 根元太さハンドル (緑丸)
                    painter.setPen(QPen(Qt::white, 1.2 * invZoom));
                    painter.setBrush(QColor(50, 205, 50));
                    painter.drawEllipse(p1, 5.0 * invZoom, 5.0 * invZoom);
                }
            }
        }
    }

    // 4-2. グラデーション操作中のガイドライン表示 (GIMP風: 始点○・終点○・破線)
    if (m_currentTool == ToolType::Gradient && m_isDraggingGradient) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        qreal invZoom = 1.0 / m_zoom;

        // ガイドライン（下地黒、表層白の破線）
        QPen blackPen(QColor(0, 0, 0, 180), 1.5 * invZoom, Qt::DashLine);
        painter.setPen(blackPen);
        painter.drawLine(m_gradientStartDocPos, m_gradientEndDocPos);

        QPen whitePen(QColor(255, 255, 255, 220), 1.0 * invZoom, Qt::DashLine);
        painter.setPen(whitePen);
        painter.drawLine(m_gradientStartDocPos, m_gradientEndDocPos);

        // 始点ハンドル (青○)
        qreal rOuter = 5.0 * invZoom;
        qreal rInner = 2.5 * invZoom;
        painter.setPen(QPen(Qt::black, 1.2 * invZoom));
        painter.setBrush(QColor(0, 120, 215));
        painter.drawEllipse(m_gradientStartDocPos, rOuter, rOuter);
        painter.setBrush(Qt::white);
        painter.drawEllipse(m_gradientStartDocPos, rInner, rInner);

        // 終点ハンドル (橙○)
        painter.setPen(QPen(Qt::black, 1.2 * invZoom));
        painter.setBrush(QColor(255, 120, 0));
        painter.drawEllipse(m_gradientEndDocPos, rOuter, rOuter);
        painter.setBrush(Qt::white);
        painter.drawEllipse(m_gradientEndDocPos, rInner, rInner);

        painter.restore();
    }

    // 4.5. ベクター制御点ツール / 筆圧ツール / 線変更ツール / 線色変更ツールのオーバーレイ描画
    auto activeL = m_document ? m_document->activeLayer() : nullptr;
    if ((m_currentTool == ToolType::VectorControlPoint ||
         m_currentTool == ToolType::VectorPressure ||
         m_currentTool == ToolType::VectorLineEdit ||
         m_currentTool == ToolType::VectorColorChange) &&
        activeL && activeL->type() == LayerType::Vector) {
        auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(activeL);
        if (vecLayer) {
            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);
            qreal invZoom = 1.0 / m_zoom;

            const auto& strokes = vecLayer->strokes();

            // (1) ホバー中または選択中のストロークを青色で強調表示
            for (size_t sIdx = 0; sIdx < strokes.size(); ++sIdx) {
                const auto& stroke = strokes[sIdx];
                bool isHovered = (static_cast<int>(sIdx) == m_hoveredStrokeIdx);
                bool isSelected = stroke.isSelected;
                if (!isHovered && !isSelected) continue;

                auto samplePts = VectorLayer::sampleStrokePoints(stroke);
                if (samplePts.size() >= 2) {
                    QPainterPath path;
                    path.moveTo(samplePts[0]);
                    for (size_t i = 1; i < samplePts.size(); ++i) {
                        path.lineTo(samplePts[i]);
                    }

                    // 青色強調グロー線 (外側: 青半透明、内側: 明るい水色)
                    float haloWidth = stroke.baseWidth + 5.0f * static_cast<float>(invZoom);
                    painter.setBrush(Qt::NoBrush);
                    painter.setPen(QPen(QColor(0, 120, 255, 150), haloWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                    painter.drawPath(path);

                    painter.setPen(QPen(QColor(100, 200, 255, 230), 2.0 * invZoom, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                    painter.drawPath(path);
                }
            }

            if (m_currentTool == ToolType::VectorControlPoint || m_currentTool == ToolType::VectorPressure) {
                // (2) キャンバス上のすべての制御点を黄色の円で表示
            for (size_t sIdx = 0; sIdx < strokes.size(); ++sIdx) {
                const auto& stroke = strokes[sIdx];
                for (size_t pIdx = 0; pIdx < stroke.points.size(); ++pIdx) {
                    const auto& pt = stroke.points[pIdx];
                    QPointF pos(pt.x, pt.y);

                    qreal r = 4.0 * invZoom;
                    QColor fillColor = pt.isSelected ? QColor(255, 160, 0) : QColor(255, 230, 0);

                    // 通常制御点 (黄色円、黒極細枠)
                    painter.setPen(QPen(QColor(0, 0, 0, 200), 1.0 * invZoom));
                    painter.setBrush(fillColor);
                    painter.drawEllipse(pos, r, r);
                }
            }

            // (3) マウスカーソルが制御点と接しているときはその制御点を赤色で強調表示
            if (m_hoveredPointStrokeIdx >= 0 && m_hoveredPointStrokeIdx < static_cast<int>(strokes.size())) {
                const auto& stroke = strokes[m_hoveredPointStrokeIdx];
                if (m_hoveredPointIdx >= 0 && m_hoveredPointIdx < static_cast<int>(stroke.points.size())) {
                    const auto& pt = stroke.points[m_hoveredPointIdx];
                    QPointF pos(pt.x, pt.y);
                    qreal r = 6.5 * invZoom;

                    // 赤色強調円 (白枠 + 赤塗り)
                    painter.setPen(QPen(Qt::white, 1.8 * invZoom));
                    painter.setBrush(QColor(255, 30, 30));
                    painter.drawEllipse(pos, r, r);

                    // 外側に細い黒枠
                    painter.setPen(QPen(QColor(0, 0, 0, 180), 0.8 * invZoom));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawEllipse(pos, r + 0.9 * invZoom, r + 0.9 * invZoom);
                }
            }

            // (4) ストローク連結ドラッグ中のガイド線
            if (m_isConnectingStrokes && m_connectSourceStrokeIdx >= 0 &&
                m_connectSourceStrokeIdx < static_cast<int>(strokes.size())) {
                const auto& sStroke = strokes[m_connectSourceStrokeIdx];
                if (m_connectSourcePtIdx >= 0 && m_connectSourcePtIdx < static_cast<int>(sStroke.points.size())) {
                    QPointF p1(sStroke.points[m_connectSourcePtIdx].x, sStroke.points[m_connectSourcePtIdx].y);
                    QPointF p2 = m_cursorDocPos;

                    if (m_connectTargetStrokeIdx >= 0 && m_connectTargetStrokeIdx < static_cast<int>(strokes.size())) {
                        const auto& tStroke = strokes[m_connectTargetStrokeIdx];
                        if (m_connectTargetPtIdx >= 0 && m_connectTargetPtIdx < static_cast<int>(tStroke.points.size())) {
                            p2 = QPointF(tStroke.points[m_connectTargetPtIdx].x, tStroke.points[m_connectTargetPtIdx].y);

                            // 連結ターゲット候補の緑色円
                            qreal r = 7.0 * invZoom;
                            painter.setPen(QPen(Qt::white, 1.5 * invZoom));
                            painter.setBrush(QColor(50, 220, 50));
                            painter.drawEllipse(p2, r, r);
                        }
                    }

                    // 破線ガイドライン
                    painter.setPen(QPen(QColor(0, 180, 255), 1.8 * invZoom, Qt::DashLine));
                    painter.drawLine(p1, p2);
                }
            }

            // (5) ストローク回転ドラッグ中の回転中心インジケータ
            if (m_isRotatingVectorStroke && m_dragStrokeIdx >= 0) {
                QPointF cp = m_rotateStrokeCenter;
                qreal r = 5.5 * invZoom;
                painter.setPen(QPen(Qt::white, 1.8 * invZoom));
                painter.setBrush(QColor(255, 140, 0, 220));
                painter.drawEllipse(cp, r, r);

                qreal crossLen = 9.0 * invZoom;
                painter.setPen(QPen(Qt::white, 1.6 * invZoom));
                painter.drawLine(QPointF(cp.x() - crossLen, cp.y()), QPointF(cp.x() + crossLen, cp.y()));
                painter.drawLine(QPointF(cp.x(), cp.y() - crossLen), QPointF(cp.x(), cp.y() + crossLen));

                painter.setPen(QPen(QColor(60, 60, 60), 1.0 * invZoom));
                painter.drawLine(QPointF(cp.x() - crossLen, cp.y()), QPointF(cp.x() + crossLen, cp.y()));
                painter.drawLine(QPointF(cp.x(), cp.y() - crossLen), QPointF(cp.x(), cp.y() + crossLen));
            }
            }

            // (6) 筆圧ドラッグ中のパーセント表示テキスト
            if (m_currentTool == ToolType::VectorPressure && m_isDraggingPressure &&
                m_pressureStrokeIdx >= 0 && m_pressureStrokeIdx < static_cast<int>(strokes.size())) {
                const auto& stroke = strokes[m_pressureStrokeIdx];
                if (m_pressurePointIdx >= 0 && m_pressurePointIdx < static_cast<int>(stroke.points.size())) {
                    const auto& pt = stroke.points[m_pressurePointIdx];
                    QPointF docPos(pt.x, pt.y);
                    QPointF viewPos = docToView(docPos);
                    QString labelText = QString::asprintf("%03d%%/100%%", m_currentPressurePercent);

                    painter.save();
                    painter.resetTransform();
                    QFont font = painter.font();
                    font.setFamily("sans-serif");
                    font.setPointSize(9);
                    font.setBold(false);
                    painter.setFont(font);

                    QPointF textPos = viewPos + QPointF(16, 4);
                    // 白色の縁取り（背景色に関わらず視認可能にする）
                    painter.setPen(QColor(255, 255, 255, 220));
                    for (int dx = -1; dx <= 1; ++dx) {
                        for (int dy = -1; dy <= 1; ++dy) {
                            if (dx != 0 || dy != 0) {
                                painter.drawText(textPos + QPointF(dx, dy), labelText);
                            }
                        }
                    }
                    // 黒色文字
                    painter.setPen(Qt::black);
                    painter.drawText(textPos, labelText);
                    painter.restore();
                }
            }

            painter.restore();
        }
    }

    // 4-2. ベクター折れ線・曲線ツールの作成中プレビュー描画
    if (m_isCreatingVectorLine && !m_inProgressVectorStroke.points.empty()) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);

        qreal invZoom = 1.0 / m_zoom;
        const auto& inPts = m_inProgressVectorStroke.points;

        // プレビュー用の一時ストローク作成
        VectorStroke previewStroke = m_inProgressVectorStroke;
        bool isCorner = (m_currentTool == ToolType::VectorPolyline);
        previewStroke.points.push_back({
            static_cast<float>(m_vectorLineCurrentMousePos.x()),
            static_cast<float>(m_vectorLineCurrentMousePos.y()),
            1.0f,
            1.0f,
            isCorner,
            false
        });
        previewStroke.baseWidth = m_vectorLineSize;
        previewStroke.isClosed = m_vectorClosePath;

        // サンプル点を生成してパスを作成
        std::vector<QPointF> previewPts = VectorLayer::sampleStrokePoints(previewStroke, nullptr, 10);
        if (previewPts.size() >= 2) {
            QPainterPath path;
            path.moveTo(previewPts[0]);
            for (size_t k = 1; k < previewPts.size(); ++k) {
                path.lineTo(previewPts[k]);
            }

            // 線描画 (色: 現在の色、線幅: 設定サイズ)
            QColor strokeCol = m_currentColor;
            strokeCol.setAlphaF(std::clamp(m_vectorLineDensity, 0.2f, 1.0f));
            painter.setPen(QPen(strokeCol, m_vectorLineSize, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(path);
        }

        // パスを閉じるオプションが有効で、点が2個以上ある場合、現在のマウス位置から始点への破線ガイド
        if (m_vectorClosePath && previewStroke.points.size() >= 3) {
            QPointF pStart(inPts[0].x, inPts[0].y);
            QPointF pCur = m_vectorLineCurrentMousePos;
            painter.setPen(QPen(QColor(0, 140, 255, 180), std::max(1.5, 1.5 * invZoom), Qt::DashLine));
            painter.drawLine(pCur, pStart);
        }

        // 確定済み制御点を描画（黄色の丸）
        for (size_t pIdx = 0; pIdx < inPts.size(); ++pIdx) {
            QPointF pos(inPts[pIdx].x, inPts[pIdx].y);
            qreal r = 4.5 * invZoom;
            painter.setPen(QPen(QColor(0, 0, 0, 220), 1.0 * invZoom));
            painter.setBrush(QColor(255, 230, 0));
            painter.drawEllipse(pos, r, r);
        }

        // 現在のマウス位置の仮点（水色の丸）
        qreal rCur = 3.5 * invZoom;
        painter.setPen(QPen(QColor(0, 0, 0, 200), 1.0 * invZoom));
        painter.setBrush(QColor(80, 200, 255));
        painter.drawEllipse(m_vectorLineCurrentMousePos, rCur, rCur);

        painter.restore();
    }

    // 4-3. ベクター楕円ツールのドラッグ中プレビュー描画
    if (m_isCreatingVectorEllipse) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);

        qreal invZoom = 1.0 / m_zoom;
        float rx = std::abs(static_cast<float>(m_vectorEllipseCurrentPos.x() - m_vectorEllipseCenter.x()));
        float ry = std::abs(static_cast<float>(m_vectorEllipseCurrentPos.y() - m_vectorEllipseCenter.y()));
        QRectF rect(m_vectorEllipseCenter.x() - rx, m_vectorEllipseCenter.y() - ry, rx * 2.0f, ry * 2.0f);

        if (m_vectorEllipseFilled) {
            QColor fillCol = m_currentColor;
            fillCol.setAlphaF(std::clamp(m_vectorEllipseDensity * 0.5f, 0.1f, 0.8f));
            painter.setBrush(fillCol);
        } else {
            painter.setBrush(Qt::NoBrush);
        }

        // 輪郭線 (描画色 + 半透明)
        QColor strokeCol = m_currentColor;
        strokeCol.setAlphaF(std::clamp(m_vectorEllipseDensity, 0.2f, 1.0f));
        painter.setPen(QPen(strokeCol, std::max(1.0f, m_vectorEllipseSize), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawEllipse(rect);

        // 中心インジケータ（青い小さな十字線）
        qreal crossLen = 6.0 * invZoom;
        painter.setPen(QPen(QColor(0, 120, 255, 220), 1.2 * invZoom));
        painter.drawLine(QPointF(m_vectorEllipseCenter.x() - crossLen, m_vectorEllipseCenter.y()),
                         QPointF(m_vectorEllipseCenter.x() + crossLen, m_vectorEllipseCenter.y()));
        painter.drawLine(QPointF(m_vectorEllipseCenter.x(), m_vectorEllipseCenter.y() - crossLen),
                         QPointF(m_vectorEllipseCenter.x(), m_vectorEllipseCenter.y() + crossLen));

        painter.restore();
    }

    // 4-4. ベクター円ツールのドラッグ中プレビュー描画
    if (m_isCreatingVectorCircle) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);

        qreal invZoom = 1.0 / m_zoom;
        float r = std::hypot(static_cast<float>(m_vectorCircleCurrentPos.x() - m_vectorCircleCenter.x()),
                             static_cast<float>(m_vectorCircleCurrentPos.y() - m_vectorCircleCenter.y()));
        QRectF rect(m_vectorCircleCenter.x() - r, m_vectorCircleCenter.y() - r, r * 2.0f, r * 2.0f);

        if (m_vectorCircleFilled) {
            QColor fillCol = m_currentColor;
            fillCol.setAlphaF(std::clamp(m_vectorCircleDensity * 0.5f, 0.1f, 0.8f));
            painter.setBrush(fillCol);
        } else {
            painter.setBrush(Qt::NoBrush);
        }

        // 輪郭線 (描画色 + 半透明)
        QColor strokeCol = m_currentColor;
        strokeCol.setAlphaF(std::clamp(m_vectorCircleDensity, 0.2f, 1.0f));
        painter.setPen(QPen(strokeCol, std::max(1.0f, m_vectorCircleSize), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawEllipse(rect);

        // 中心インジケータ（青い小さな十字線）
        qreal crossLen = 6.0 * invZoom;
        painter.setPen(QPen(QColor(0, 120, 255, 220), 1.2 * invZoom));
        painter.drawLine(QPointF(m_vectorCircleCenter.x() - crossLen, m_vectorCircleCenter.y()),
                         QPointF(m_vectorCircleCenter.x() + crossLen, m_vectorCircleCenter.y()));
        painter.drawLine(QPointF(m_vectorCircleCenter.x(), m_vectorCircleCenter.y() - crossLen),
                         QPointF(m_vectorCircleCenter.x(), m_vectorCircleCenter.y() + crossLen));

        // 半径方向のガイド線（破線）
        if (r > 2.0f) {
            painter.setPen(QPen(QColor(0, 140, 255, 180), std::max(1.0, 1.2 * invZoom), Qt::DashLine));
            painter.drawLine(m_vectorCircleCenter, m_vectorCircleCurrentPos);
        }

        painter.restore();
    }

    // 4-5. ベクター多角形ツールのドラッグ中プレビュー描画
    if (m_isCreatingVectorPolygon) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);

        qreal invZoom = 1.0 / m_zoom;
        float dx = static_cast<float>(m_vectorPolygonCurrentPos.x() - m_vectorPolygonCenter.x());
        float dy = static_cast<float>(m_vectorPolygonCurrentPos.y() - m_vectorPolygonCenter.y());
        float dist = std::hypot(dx, dy);
        float rx = m_vectorPolygonRegular ? dist : std::abs(dx);
        float ry = m_vectorPolygonRegular ? dist : std::abs(dy);

        int sides = std::clamp(m_vectorPolygonSides, 3, 24);
        constexpr double PI = 3.14159265358979323846;
        float rotAngle = m_vectorPolygonRegular ? static_cast<float>(std::atan2(dy, dx)) : static_cast<float>(-PI * 0.5);

        QPolygonF poly;
        poly.reserve(sides);
        for (int i = 0; i < sides; ++i) {
            double angle = rotAngle + i * 2.0 * PI / sides;
            float px = static_cast<float>(m_vectorPolygonCenter.x() + rx * std::cos(angle));
            float py = static_cast<float>(m_vectorPolygonCenter.y() + ry * std::sin(angle));
            poly << QPointF(px, py);
        }

        if (m_vectorPolygonFilled) {
            QColor fillCol = m_currentColor;
            fillCol.setAlphaF(std::clamp(m_vectorPolygonDensity * 0.5f, 0.1f, 0.8f));
            painter.setBrush(fillCol);
        } else {
            painter.setBrush(Qt::NoBrush);
        }

        // 輪郭線 (描画色 + 半透明)
        QColor strokeCol = m_currentColor;
        strokeCol.setAlphaF(std::clamp(m_vectorPolygonDensity, 0.2f, 1.0f));
        painter.setPen(QPen(strokeCol, std::max(1.0f, m_vectorPolygonSize), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPolygon(poly);

        // 中心インジケータ（青い小さな十字線）
        qreal crossLen = 6.0 * invZoom;
        painter.setPen(QPen(QColor(0, 120, 255, 220), 1.2 * invZoom));
        painter.drawLine(QPointF(m_vectorPolygonCenter.x() - crossLen, m_vectorPolygonCenter.y()),
                         QPointF(m_vectorPolygonCenter.x() + crossLen, m_vectorPolygonCenter.y()));
        painter.drawLine(QPointF(m_vectorPolygonCenter.x(), m_vectorPolygonCenter.y() - crossLen),
                         QPointF(m_vectorPolygonCenter.x(), m_vectorPolygonCenter.y() + crossLen));

        // 正N角形ドラッグ時の半径方向ガイド線（破線）
        if (m_vectorPolygonRegular && dist > 2.0f) {
            painter.setPen(QPen(QColor(0, 140, 255, 180), std::max(1.0, 1.2 * invZoom), Qt::DashLine));
            painter.drawLine(m_vectorPolygonCenter, m_vectorPolygonCurrentPos);
        }

        painter.restore();
    }

    // 4-6. コマ枠・コマ分割オーバーレイ描画
    if (m_currentTool == ToolType::PanelFrame || m_currentTool == ToolType::PanelSplit ||
        m_isCreatingPanel || m_isSplittingPanel || m_isModifyingPanel ||
        (activeL && activeL->type() == LayerType::Group)) {
        renderPanelOverlay(painter);
    }

    // 5. GIMP風 ブラシサイズ輪郭カーソル (Brush Outline)
    bool isGroup = activeL && activeL->type() == LayerType::Group;
    bool isSpecialTool = (m_currentTool == ToolType::Text || m_currentTool == ToolType::TextBalloon ||
                          m_currentTool == ToolType::PanelFrame || m_currentTool == ToolType::PanelSplit ||
                          m_currentTool == ToolType::Pipette ||
                          m_currentTool == ToolType::RectSelect || m_currentTool == ToolType::EllipseSelect ||
                          m_currentTool == ToolType::LassoSelect || m_currentTool == ToolType::ColorSelect ||
                          m_currentTool == ToolType::MagicWand ||
                          m_currentTool == ToolType::Bucket || m_currentTool == ToolType::Gradient ||
                          m_currentTool == ToolType::VectorControlPoint ||
                          m_currentTool == ToolType::VectorPressure ||
                          m_currentTool == ToolType::VectorLineEdit ||
                          m_currentTool == ToolType::VectorColorChange ||
                          m_currentTool == ToolType::VectorPolyline || m_currentTool == ToolType::VectorCurve ||
                          m_currentTool == ToolType::VectorEllipse ||
                          m_currentTool == ToolType::VectorCircle ||
                          m_currentTool == ToolType::VectorPolygon ||
                          m_currentTool == ToolType::Transform);
    if (m_showBrushOutline && !m_isPanning && !isGroup && !isSpecialTool) {
        painter.setRenderHint(QPainter::Antialiasing, true);

        float r = currentBrushOutlineRadius();
        qreal invZoom = 1.0 / m_zoom;

        painter.setBrush(Qt::NoBrush);

        if (m_currentTool == ToolType::Marker && !m_imageBrushOutlinePath.isEmpty()) {
            float totalAngle = static_cast<float>(m_imageBrushAngle);
            if (m_imageBrushFollowCursor) {
                totalAngle += m_imageBrushLastFollowAngle;
            }
            while (totalAngle >= 360.0f) totalAngle -= 360.0f;
            while (totalAngle < 0.0f) totalAngle -= 360.0f;

            float sz = m_imageBrushSize;
            QTransform tr;
            tr.translate(m_cursorDocPos.x(), m_cursorDocPos.y());
            tr.rotate(totalAngle);
            tr.scale(m_imageBrushFlipH ? -sz : sz, m_imageBrushFlipV ? -sz : sz);

            QPainterPath mappedPath = tr.map(m_imageBrushOutlinePath);

            painter.setPen(QPen(QColor(0, 0, 0, 160), 1.6 * invZoom));
            painter.drawPath(mappedPath);

            painter.setPen(QPen(QColor(255, 255, 255, 230), 1.0 * invZoom));
            painter.drawPath(mappedPath);
        } else {
            // 輪郭円: 外側黒 (1.6px)、内側白 (1.0px) の二重線でどんな背景でも視認可能
            painter.setPen(QPen(QColor(0, 0, 0, 160), 1.6 * invZoom));
            painter.drawEllipse(m_cursorDocPos, r, r);

            painter.setPen(QPen(QColor(255, 255, 255, 230), 1.0 * invZoom));
            painter.drawEllipse(m_cursorDocPos, r, r);
        }

        // 中心のクロスヘア (十字マーク)
        float crossLen = std::max(2.5f, static_cast<float>(3.5 * invZoom));
        painter.setPen(QPen(QColor(0, 0, 0, 180), 1.4 * invZoom));
        painter.drawLine(QPointF(m_cursorDocPos.x() - crossLen, m_cursorDocPos.y()),
                         QPointF(m_cursorDocPos.x() + crossLen, m_cursorDocPos.y()));
        painter.drawLine(QPointF(m_cursorDocPos.x(), m_cursorDocPos.y() - crossLen),
                         QPointF(m_cursorDocPos.x(), m_cursorDocPos.y() + crossLen));

        painter.setPen(QPen(QColor(255, 255, 255, 240), 1.0 * invZoom));
        painter.drawLine(QPointF(m_cursorDocPos.x() - crossLen + invZoom, m_cursorDocPos.y()),
                         QPointF(m_cursorDocPos.x() + crossLen - invZoom, m_cursorDocPos.y()));
        painter.drawLine(QPointF(m_cursorDocPos.x(), m_cursorDocPos.y() - crossLen + invZoom),
                         QPointF(m_cursorDocPos.x(), m_cursorDocPos.y() + crossLen - invZoom));
    }

    // 変形ツール プレビュー描画 (ドキュメント座標系)
    renderTransformPreview(painter);

    painter.restore();

    // 6. GIMPスタイル マーチングアンツ（確定済み選択範囲境界線表示）
    if (m_showSelection && m_document && m_document->hasSelection() && !m_document->isSelectionEditMode()) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);

        // ドキュメント境界（キャンバス領域内）にクリップしたパスをビュー座標系にマッピング
        QPainterPath docRectPath;
        docRectPath.addRect(0, 0, m_document->width(), m_document->height());
        QPainterPath boundedDocSel = m_document->selection().intersected(docRectPath);
        QPainterPath viewSelPath = viewTransform().map(boundedDocSel);

        // 6-1. 選択範囲内部の微細ハイライト（選択ツール選択中のみ水色ハイライトを表示）
        if (m_currentTool == ToolType::RectSelect || m_currentTool == ToolType::EllipseSelect ||
            m_currentTool == ToolType::LassoSelect || m_currentTool == ToolType::ColorSelect ||
            m_currentTool == ToolType::MagicWand) {
            painter.fillPath(viewSelPath, QColor(0, 120, 255, 25));
        }

        // 6-2. GIMP特有の白黒交互マーチングアンツ（アリの行進）
        // 下地: 黒破線 (4px実線, 4px空白, オフセット m_antsOffset)
        QPen blackPen(Qt::black, 1.0, Qt::CustomDashLine);
        blackPen.setDashPattern({4, 4});
        blackPen.setDashOffset(m_antsOffset);
        painter.strokePath(viewSelPath, blackPen);

        // 表層: 白破線 (4px空白部分を埋める位相差+4の白破線)
        QPen whitePen(Qt::white, 1.0, Qt::CustomDashLine);
        whitePen.setDashPattern({4, 4});
        whitePen.setDashOffset(m_antsOffset + 4);
        painter.strokePath(viewSelPath, whitePen);

        // 6-3. GIMPスタイル: 矩形・楕円選択ツール選択時のみ、4隅のハンドル枠を描画
        // 他のツールが選択されている場合は描画されず、点線のみになる
        if ((m_currentTool == ToolType::RectSelect || m_currentTool == ToolType::EllipseSelect) && m_hasActiveRect && !m_isSelecting) {
            auto drawCornerBoxes = [&](const QRectF& r, const QTransform& tf) {
                QRectF nr = r.normalized();
                qreal hs = std::min({26.0, nr.width() * 0.35, nr.height() * 0.35});
                if (hs < 4.0) return;

                QRectF corners[4] = {
                    QRectF(nr.left(), nr.top(), hs, hs),
                    QRectF(nr.right() - hs, nr.top(), hs, hs),
                    QRectF(nr.left(), nr.bottom() - hs, hs, hs),
                    QRectF(nr.right() - hs, nr.bottom() - hs, hs, hs)
                };

                for (const auto& c : corners) {
                    QPolygonF poly = tf.map(QPolygonF(c));
                    painter.setPen(QPen(QColor(80, 80, 80, 220), 1.0));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawPolygon(poly);
                }
            };

            if (m_activeRectAlignToWindow) {
                drawCornerBoxes(m_activeViewRect, QTransform());
            } else {
                drawCornerBoxes(m_activeDocRect, viewTransform());
            }
        }

        painter.restore();
    }

    // 7-1. 矩形選択ツール ドラッグ中プレビュー（GIMP風マーチングアンツ）
    if (m_isSelecting && m_currentTool == ToolType::RectSelect) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);

        // 修飾キー判定 (Shift: 追加選択, Alt: 範囲削除, どちらでもなければ m_selectionMode)
        Qt::KeyboardModifiers mods = QGuiApplication::keyboardModifiers();
        SelectionMode effectiveMode = m_selectionMode;
        if (mods & Qt::ShiftModifier) {
            effectiveMode = SelectionMode::Add;
        } else if (mods & Qt::AltModifier) {
            effectiveMode = SelectionMode::Subtract;
        }

        QPainterPath previewPath;
        if (m_selectionAlignToWindow) {
            // 表示ウィンドウ軸平行: 画面座標矩形
            QRectF r(m_selectStartViewPos, m_selectCurViewPos);
            previewPath.addRect(r.normalized());
        } else {
            // キャンバス軸平行: ドキュメント座標矩形をビュー座標系にマッピング
            qreal dx0 = std::min(m_selectStartDocPos.x(), m_selectCurDocPos.x());
            qreal dx1 = std::max(m_selectStartDocPos.x(), m_selectCurDocPos.x());
            qreal dy0 = std::min(m_selectStartDocPos.y(), m_selectCurDocPos.y());
            qreal dy1 = std::max(m_selectStartDocPos.y(), m_selectCurDocPos.y());
            QPainterPath docRectPath;
            docRectPath.addRect(QRectF(dx0, dy0, dx1 - dx0, dy1 - dy0));
            previewPath = viewTransform().map(docRectPath);
        }

        if (effectiveMode == SelectionMode::Subtract) {
            // 範囲削除プレビュー: 赤黒のマーチングアンツ ＋ 半透明赤ハイライト
            painter.fillPath(previewPath, QColor(255, 60, 60, 35));

            QPen blackPen(Qt::black, 1.0, Qt::CustomDashLine);
            blackPen.setDashPattern({4, 4});
            blackPen.setDashOffset(m_antsOffset);
            painter.strokePath(previewPath, blackPen);

            QPen redPen(QColor(255, 50, 50), 1.0, Qt::CustomDashLine);
            redPen.setDashPattern({4, 4});
            redPen.setDashOffset(m_antsOffset + 4);
            painter.strokePath(previewPath, redPen);
        } else {
            // 通常/追加選択プレビュー: 白黒のマーチングアンツ ＋ 半透明青ハイライト
            painter.fillPath(previewPath, QColor(0, 120, 255, 35));

            QPen blackPen(Qt::black, 1.0, Qt::CustomDashLine);
            blackPen.setDashPattern({4, 4});
            blackPen.setDashOffset(m_antsOffset);
            painter.strokePath(previewPath, blackPen);

            QPen whitePen(Qt::white, 1.0, Qt::CustomDashLine);
            whitePen.setDashPattern({4, 4});
            whitePen.setDashOffset(m_antsOffset + 4);
            painter.strokePath(previewPath, whitePen);
        }

        painter.restore();
    }

    // 7-2. 楕円選択ツール ドラッグ中プレビュー
    if (m_isSelecting && m_currentTool == ToolType::EllipseSelect) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);

        Qt::KeyboardModifiers mods = QGuiApplication::keyboardModifiers();
        SelectionMode effectiveMode = m_ellipseSelectMode;
        if (mods & Qt::ShiftModifier) {
            effectiveMode = SelectionMode::Add;
        } else if (mods & Qt::AltModifier) {
            effectiveMode = SelectionMode::Subtract;
        }

        QPainterPath previewPath;
        if (m_ellipseSelectMethod == EllipseSelectMethod::FromCenter) {
            qreal rx = std::abs(m_selectCurDocPos.x() - m_selectStartDocPos.x());
            qreal ry = std::abs(m_selectCurDocPos.y() - m_selectStartDocPos.y());
            QPainterPath docEllipse;
            docEllipse.addEllipse(m_selectStartDocPos, rx, ry);
            previewPath = viewTransform().map(docEllipse);

            // 中心インジケータ（青十字線）
            QPointF centerView = viewTransform().map(m_selectStartDocPos);
            painter.setPen(QPen(QColor(0, 120, 255, 200), 1.0));
            painter.drawLine(QPointF(centerView.x() - 6, centerView.y()), QPointF(centerView.x() + 6, centerView.y()));
            painter.drawLine(QPointF(centerView.x(), centerView.y() - 6), QPointF(centerView.x(), centerView.y() + 6));
        } else {
            if (m_selectionAlignToWindow) {
                QRectF r(m_selectStartViewPos, m_selectCurViewPos);
                previewPath.addEllipse(r.normalized());
            } else {
                qreal dx0 = std::min(m_selectStartDocPos.x(), m_selectCurDocPos.x());
                qreal dx1 = std::max(m_selectStartDocPos.x(), m_selectCurDocPos.x());
                qreal dy0 = std::min(m_selectStartDocPos.y(), m_selectCurDocPos.y());
                qreal dy1 = std::max(m_selectStartDocPos.y(), m_selectCurDocPos.y());
                QPainterPath docRectPath;
                docRectPath.addEllipse(QRectF(dx0, dy0, dx1 - dx0, dy1 - dy0));
                previewPath = viewTransform().map(docRectPath);
            }
        }

        if (effectiveMode == SelectionMode::Subtract) {
            painter.fillPath(previewPath, QColor(255, 60, 60, 35));
            QPen blackPen(Qt::black, 1.0, Qt::CustomDashLine);
            blackPen.setDashPattern({4, 4});
            blackPen.setDashOffset(m_antsOffset);
            painter.strokePath(previewPath, blackPen);

            QPen redPen(QColor(255, 50, 50), 1.0, Qt::CustomDashLine);
            redPen.setDashPattern({4, 4});
            redPen.setDashOffset(m_antsOffset + 4);
            painter.strokePath(previewPath, redPen);
        } else {
            painter.fillPath(previewPath, QColor(0, 120, 255, 35));
            QPen blackPen(Qt::black, 1.0, Qt::CustomDashLine);
            blackPen.setDashPattern({4, 4});
            blackPen.setDashOffset(m_antsOffset);
            painter.strokePath(previewPath, blackPen);

            QPen whitePen(Qt::white, 1.0, Qt::CustomDashLine);
            whitePen.setDashPattern({4, 4});
            whitePen.setDashOffset(m_antsOffset + 4);
            painter.strokePath(previewPath, whitePen);
        }

        painter.restore();
    }

    // 7-3. 投げ縄選択ツール ドラッグ中プレビュー
    if (m_isLassoSelecting && m_currentTool == ToolType::LassoSelect && m_lassoPointsDoc.size() >= 2) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);

        Qt::KeyboardModifiers mods = QGuiApplication::keyboardModifiers();
        SelectionMode effectiveMode = m_lassoSelectMode;
        if (mods & Qt::ShiftModifier) {
            effectiveMode = SelectionMode::Add;
        } else if (mods & Qt::AltModifier) {
            effectiveMode = SelectionMode::Subtract;
        }

        QPolygonF docPoly(m_lassoPointsDoc.begin(), m_lassoPointsDoc.end());
        QPolygonF viewPoly = viewTransform().map(docPoly);
        QPainterPath previewPath;
        previewPath.addPolygon(viewPoly);

        // 閉じるためのガイド線（現在位置から始点への破線）
        QPen guidePen(QColor(120, 120, 120, 180), 1.0, Qt::DashLine);
        painter.setPen(guidePen);
        painter.drawLine(viewPoly.last(), viewPoly.first());

        if (effectiveMode == SelectionMode::Subtract) {
            painter.fillPath(previewPath, QColor(255, 60, 60, 30));
            QPen blackPen(Qt::black, 1.0, Qt::CustomDashLine);
            blackPen.setDashPattern({4, 4});
            blackPen.setDashOffset(m_antsOffset);
            painter.strokePath(previewPath, blackPen);

            QPen redPen(QColor(255, 50, 50), 1.0, Qt::CustomDashLine);
            redPen.setDashPattern({4, 4});
            redPen.setDashOffset(m_antsOffset + 4);
            painter.strokePath(previewPath, redPen);
        } else {
            painter.fillPath(previewPath, QColor(0, 120, 255, 30));
            QPen blackPen(Qt::black, 1.0, Qt::CustomDashLine);
            blackPen.setDashPattern({4, 4});
            blackPen.setDashOffset(m_antsOffset);
            painter.strokePath(previewPath, blackPen);

            QPen whitePen(Qt::white, 1.0, Qt::CustomDashLine);
            whitePen.setDashPattern({4, 4});
            whitePen.setDashOffset(m_antsOffset + 4);
            painter.strokePath(previewPath, whitePen);
        }

        painter.restore();
    }

    // 8. 変形ツール ギズモ描画 (ビュー座標系)
    renderTransformGizmo(painter);
}

void CanvasWidget::renderGradient(const QPointF& startPos, const QPointF& endPos) {
    if (!m_document) return;
    auto activeL = m_document->activeLayer();
    if (!activeL || activeL->type() != LayerType::Raster) {
        return;
    }

    int docW = m_document->width();
    int docH = m_document->height();
    if (docW <= 0 || docH <= 0) return;

    // 描画範囲の決定 (選択範囲があればそのバウンディングボックスと交差、なければキャンバス全体)
    QRect targetRect(0, 0, docW, docH);
    bool hasSelection = m_document->hasSelection();
    if (hasSelection) {
        QRect selBounds = m_document->selection().boundingRect().toAlignedRect();
        targetRect = targetRect.intersected(selBounds);
        if (targetRect.isEmpty()) return;
    }

    // Undo用 PreTiles クローン
    TileManager preTiles = activeL->tileManager().clone();

    // ベクトル計算
    float x0 = static_cast<float>(startPos.x());
    float y0 = static_cast<float>(startPos.y());
    float x1 = static_cast<float>(endPos.x());
    float y1 = static_cast<float>(endPos.y());

    float vx = x1 - x0;
    float vy = y1 - y0;
    float lenSq = vx * vx + vy * vy;
    float len = std::sqrt(lenSq);

    // Conical用 基準角
    float angle0 = (lenSq > 1e-6f) ? std::atan2(vy, vx) : 0.0f;

    // Bayer 4x4 行列 (ディザリング用)
    static const float bayer4x4[4][4] = {
        {  0.0f/16.0f,  8.0f/16.0f,  2.0f/16.0f, 10.0f/16.0f },
        { 12.0f/16.0f,  4.0f/16.0f, 14.0f/16.0f,  6.0f/16.0f },
        {  3.0f/16.0f, 11.0f/16.0f,  1.0f/16.0f,  9.0f/16.0f },
        { 15.0f/16.0f,  7.0f/16.0f, 13.0f/16.0f,  5.0f/16.0f }
    };

    QColor fgCol = m_currentColor;
    QColor bgCol = m_secondaryColor;
    float fgR = fgCol.redF();
    float fgG = fgCol.greenF();
    float fgB = fgCol.blueF();
    float fgA = fgCol.alphaF();

    float bgR = bgCol.redF();
    float bgG = bgCol.greenF();
    float bgB = bgCol.blueF();
    float bgA = bgCol.alphaF();

    bool alphaLocked = activeL->isAlphaLocked();

    int minTx = targetRect.left() / TileMemoryPool::TILE_WIDTH;
    int maxTx = targetRect.right() / TileMemoryPool::TILE_WIDTH;
    int minTy = targetRect.top() / TileMemoryPool::TILE_HEIGHT;
    int maxTy = targetRect.bottom() / TileMemoryPool::TILE_HEIGHT;

    // タイル単位でループ
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

            Tile* tile = activeL->tileManager().getTileForWrite(tx, ty);
            if (!tile || !tile->writeBuffer()) continue;
            uint32_t* pixels = reinterpret_cast<uint32_t*>(tile->writeBuffer());

            for (int py = py0; py <= py1; ++py) {
                int ly = py - tileY0;
                for (int px = px0; px <= px1; ++px) {
                    float selWeight = 1.0f;
                    if (hasSelection) {
                        uint8_t sel = m_document->selectionStrength(px, py);
                        if (sel == 0) continue;
                        selWeight = sel / 255.0f;
                    }

                    int lx = px - tileX0;
                    int pixelIdx = ly * TileMemoryPool::TILE_WIDTH + lx;

                    // 1. パラメータ t の計算
                    float t = 0.0f;
                    switch (m_gradientShape) {
                    case GradientShape::Linear: {
                        if (lenSq > 1e-6f) {
                            t = ((px - x0) * vx + (py - y0) * vy) / lenSq;
                        } else {
                            t = 0.0f;
                        }
                        break;
                    }
                    case GradientShape::BiLinear: {
                        if (lenSq > 1e-6f) {
                            t = std::abs(((px - x0) * vx + (py - y0) * vy) / lenSq);
                        } else {
                            t = 0.0f;
                        }
                        break;
                    }
                    case GradientShape::Radial: {
                        if (len > 1e-4f) {
                            t = std::hypot(px - x0, py - y0) / len;
                        } else {
                            t = 0.0f;
                        }
                        break;
                    }
                    case GradientShape::Conical: {
                        float curAngle = std::atan2(py - y0, px - x0) - angle0;
                        constexpr float TWO_PI = 2.0f * 3.14159265358979323846f;
                        curAngle = std::fmod(std::fmod(curAngle, TWO_PI) + TWO_PI, TWO_PI);
                        t = curAngle / TWO_PI;
                        break;
                    }
                    }

                    // 2. 繰り返し処理 (Repeat Mode)
                    switch (m_gradientRepeatMode) {
                    case GradientRepeatMode::None:
                        t = std::clamp(t, 0.0f, 1.0f);
                        break;
                    case GradientRepeatMode::Sawtooth: {
                        t = t - std::floor(t);
                        break;
                    }
                    case GradientRepeatMode::Triangular: {
                        float mod2 = std::fmod(std::fmod(t, 2.0f) + 2.0f, 2.0f);
                        t = (mod2 <= 1.0f) ? mod2 : (2.0f - mod2);
                        break;
                    }
                    }

                    // 3. 反転 (Reverse)
                    if (m_gradientReverse) {
                        t = 1.0f - t;
                    }

                    // 4. ディザリング (Dither)
                    if (m_gradientDither) {
                        float d = (bayer4x4[py & 3][px & 3] - 0.5f) * (1.0f / 255.0f);
                        t = std::clamp(t + d, 0.0f, 1.0f);
                    } else {
                        t = std::clamp(t, 0.0f, 1.0f);
                    }

                    // 5. 色とアルファの計算
                    float srcR, srcG, srcB, srcA;
                    if (m_gradientColorMode == GradientColorMode::FgToTransparent) {
                        srcR = fgR;
                        srcG = fgG;
                        srcB = fgB;
                        srcA = fgA * (1.0f - t) * m_gradientOpacity;
                    } else { // FgToBg
                        srcR = fgR + (bgR - fgR) * t;
                        srcG = fgG + (bgG - fgG) * t;
                        srcB = fgB + (bgB - fgB) * t;
                        srcA = (fgA + (bgA - fgA) * t) * m_gradientOpacity;
                    }
                    srcA *= selWeight;

                    if (srcA <= 0.0001f) continue;

                    // 6. レイヤーへの合成 (Porter-Duff Over)
                    uint32_t dstPixel = pixels[pixelIdx];
                    float dstA = ((dstPixel >> 24) & 0xFF) / 255.0f;

                    if (alphaLocked) {
                        if (dstA <= 0.0f) continue; // 透明度保護で完全透明ならスキップ
                    }

                    float dstR = ((dstPixel >> 16) & 0xFF) / 255.0f;
                    float dstG = ((dstPixel >> 8) & 0xFF) / 255.0f;
                    float dstB = (dstPixel & 0xFF) / 255.0f;

                    float br = (dstA > 0.0f) ? blendChannel(dstR, srcR, m_gradientBlendMode) : srcR;
                    float bg = (dstA > 0.0f) ? blendChannel(dstG, srcG, m_gradientBlendMode) : srcG;
                    float bb = (dstA > 0.0f) ? blendChannel(dstB, srcB, m_gradientBlendMode) : srcB;

                    float outA = srcA + dstA * (1.0f - srcA);
                    if (outA > 0.0f) {
                        float outR = (br * srcA + dstR * dstA * (1.0f - srcA)) / outA;
                        float outG = (bg * srcA + dstG * dstA * (1.0f - srcA)) / outA;
                        float outB = (bb * srcA + dstB * dstA * (1.0f - srcA)) / outA;

                        if (alphaLocked) {
                            outA = dstA;
                        }

                        uint32_t ur = std::clamp(static_cast<int>(std::round(outR * 255.0f)), 0, 255);
                        uint32_t ug = std::clamp(static_cast<int>(std::round(outG * 255.0f)), 0, 255);
                        uint32_t ub = std::clamp(static_cast<int>(std::round(outB * 255.0f)), 0, 255);
                        uint32_t ua = std::clamp(static_cast<int>(std::round(outA * 255.0f)), 0, 255);

                        pixels[pixelIdx] = (ua << 24) | (ur << 16) | (ug << 8) | ub;
                    }
                }
            }
        }
    }

    TileManager postTiles = activeL->tileManager().clone();
    if (m_document->undoStack()) {
        auto cmd = new DrawStrokeCommand(m_document, m_document->activeLayerIndex(), std::move(preTiles), std::move(postTiles), targetRect);
        cmd->setText(tr("グラデーション"));
        m_document->undoStack()->push(cmd);
    }
    activeL->invalidateContentBounds();
    activeL->tileManager().markDirtyRect(targetRect);
    m_document->composite(targetRect);
    update();
}

void CanvasWidget::deleteSelectedPixels() {
    if (!m_document) return;
    if (!m_document->hasSelection()) return;

    auto activeL = m_document->activeLayer();
    if (!activeL || activeL->type() != LayerType::Raster) {
        return;
    }

    int docW = m_document->width();
    int docH = m_document->height();
    if (docW <= 0 || docH <= 0) return;

    QRect selBounds = m_document->selection().boundingRect().toAlignedRect();
    QRect targetRect = QRect(0, 0, docW, docH).intersected(selBounds);
    if (targetRect.isEmpty()) return;

    // Undo用スナップショット
    TileManager preTiles = activeL->tileManager().clone();

    bool alphaLocked = activeL->isAlphaLocked();
    QColor bgCol = m_secondaryColor;
    uint32_t bgR = bgCol.red();
    uint32_t bgG = bgCol.green();
    uint32_t bgB = bgCol.blue();

    int minTx = targetRect.left() / TileMemoryPool::TILE_WIDTH;
    int maxTx = targetRect.right() / TileMemoryPool::TILE_WIDTH;
    int minTy = targetRect.top() / TileMemoryPool::TILE_HEIGHT;
    int maxTy = targetRect.bottom() / TileMemoryPool::TILE_HEIGHT;

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

            Tile* tile = activeL->tileManager().getTileForWrite(tx, ty);
            if (!tile || !tile->writeBuffer()) continue;
            uint32_t* pixels = reinterpret_cast<uint32_t*>(tile->writeBuffer());

            for (int py = py0; py <= py1; ++py) {
                int ly = py - tileY0;
                for (int px = px0; px <= px1; ++px) {
                    uint8_t sel = m_document->selectionStrength(px, py);
                    if (sel == 0) continue;
                    float selRatio = sel / 255.0f;

                    int lx = px - tileX0;
                    int pixelIdx = ly * TileMemoryPool::TILE_WIDTH + lx;

                    uint32_t curPix = pixels[pixelIdx];
                    uint32_t curA = (curPix >> 24) & 0xFF;
                    if (curA == 0) continue;

                    if (alphaLocked) {
                        // 透明度保護時: アルファ値を保持し背景色で消去 (GIMP準拠)
                        uint32_t curR = (curPix >> 16) & 0xFF;
                        uint32_t curG = (curPix >> 8) & 0xFF;
                        uint32_t curB = curPix & 0xFF;
                        uint32_t newR = static_cast<uint32_t>(curR * (1.0f - selRatio) + bgR * selRatio + 0.5f);
                        uint32_t newG = static_cast<uint32_t>(curG * (1.0f - selRatio) + bgG * selRatio + 0.5f);
                        uint32_t newB = static_cast<uint32_t>(curB * (1.0f - selRatio) + bgB * selRatio + 0.5f);
                        pixels[pixelIdx] = (curA << 24) | (newR << 16) | (newG << 8) | newB;
                    } else {
                        // 通常: 選択強度に応じてアルファ値を減衰消去
                        uint32_t newA = static_cast<uint32_t>(curA * (1.0f - selRatio) + 0.5f);
                        if (newA == 0) {
                            pixels[pixelIdx] = 0x00000000;
                        } else {
                            pixels[pixelIdx] = (newA << 24) | (curPix & 0x00FFFFFF);
                        }
                    }
                }
            }
        }
    }

    TileManager postTiles = activeL->tileManager().clone();
    if (m_document->undoStack()) {
        auto cmd = new DrawStrokeCommand(m_document, m_document->activeLayerIndex(), std::move(preTiles), std::move(postTiles), targetRect);
        cmd->setText(tr("ピクセルデータを削除"));
        m_document->undoStack()->push(cmd);
    }
    activeL->invalidateContentBounds();
    activeL->tileManager().markDirtyRect(targetRect);
    m_document->composite(targetRect);
    update();
}

void CanvasWidget::finishVectorLine() {
    if (!m_isCreatingVectorLine) return;
    m_isCreatingVectorLine = false;

    auto activeL = m_document ? m_document->activeLayer() : nullptr;
    auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(activeL);
    if (vecLayer && m_inProgressVectorStroke.points.size() >= 2) {
        m_inProgressVectorStroke.baseWidth = m_vectorLineSize;
        m_inProgressVectorStroke.density = m_vectorLineDensity;
        m_inProgressVectorStroke.isClosed = m_vectorClosePath;
        vecLayer->addStroke(m_inProgressVectorStroke);
        if (m_document->undoStack()) {
            auto cmd = new AddVectorStrokeCommand(m_document, m_document->activeLayerIndex(), m_inProgressVectorStroke);
            QString cmdName = (m_currentTool == ToolType::VectorPolyline) ? tr("折れ線") : tr("曲線");
            cmd->setText(cmdName);
            m_document->undoStack()->push(cmd);
        }
        m_document->composite();
    }
    m_inProgressVectorStroke = VectorStroke();
    update();
}

void CanvasWidget::cancelVectorLine() {
    m_isCreatingVectorLine = false;
    m_inProgressVectorStroke = VectorStroke();
    update();
}

void CanvasWidget::startTransform() {
    if (!m_document) return;
    auto activeL = m_document->activeLayer();
    if (!activeL || activeL->isLocked()) return;
    if (activeL->type() == LayerType::Text) {
        // テキストレイヤー: 何もしない
        return;
    }

    if (m_isTransforming) {
        return;
    }

    m_transformTargetLayers.clear();
    m_transformHasSelection = m_document->hasSelection();
    if (m_transformHasSelection) {
        m_transformOrigSelection = m_document->selection();
    } else {
        m_transformOrigSelection = QPainterPath();
    }

    // 対象レイヤーの収集 (アクティブレイヤー + ピン留めされた非ロック通常/ベクターレイヤー)
    // 描画順を保つためインデックス昇順で収集
    int activeIdx = m_document->activeLayerIndex();
    int numLayers = static_cast<int>(m_document->layers().size());
    for (int i = 0; i < numLayers; ++i) {

        auto l = m_document->getLayer(i);
        if (!l || l->isLocked()) continue;
        if (l->type() != LayerType::Raster && l->type() != LayerType::Vector) continue;

        if (i == activeIdx || l->isPinned()) {
            TransformTargetLayer tl;
            tl.layerIndex = i;
            tl.layer = l;
            tl.isRaster = (l->type() == LayerType::Raster);
            tl.isVector = (l->type() == LayerType::Vector);
            m_transformTargetLayers.push_back(tl);
        }
    }

    if (m_transformTargetLayers.empty()) {
        return;
    }

    // 共通バウンディングボックスの決定
    QRect targetRect;
    if (m_transformHasSelection) {
        QRect selBounds = m_document->selection().boundingRect().toAlignedRect();
        targetRect = selBounds.intersected(QRect(0, 0, m_document->width(), m_document->height()));
    } else {
        QRect unitedRect;
        for (const auto& tl : m_transformTargetLayers) {
            if (tl.isRaster) {
                QRect bounds = tl.layer->contentBounds();
                if (!bounds.isEmpty()) {
                    unitedRect = unitedRect.isNull() ? bounds : unitedRect.united(bounds);
                }
            } else if (tl.isVector) {
                auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(tl.layer);
                if (vecLayer) {
                    for (const auto& stroke : vecLayer->strokes()) {
                        for (const auto& pt : stroke.points) {
                            QPointF p(pt.x, pt.y);
                            unitedRect = unitedRect.isNull() ? QRectF(p, QSizeF(1, 1)).toAlignedRect()
                                                             : unitedRect.united(QRectF(p, QSizeF(1, 1)).toAlignedRect());
                        }
                    }
                }
            }
        }
        if (unitedRect.isEmpty()) {
            targetRect = QRect(0, 0, m_document->width(), m_document->height());
        } else {
            targetRect = unitedRect.intersected(QRect(0, 0, m_document->width(), m_document->height()));
        }
    }

    if (targetRect.isEmpty() || targetRect.width() <= 0 || targetRect.height() <= 0) {
        return;
    }

    m_transformSourceRect = targetRect;

    // 各対象レイヤーからデータを切り出し、元レイヤー領域を一時消去
    int minTx = targetRect.left() / TileMemoryPool::TILE_WIDTH;
    int maxTx = targetRect.right() / TileMemoryPool::TILE_WIDTH;
    int minTy = targetRect.top() / TileMemoryPool::TILE_HEIGHT;
    int maxTy = targetRect.bottom() / TileMemoryPool::TILE_HEIGHT;

    for (auto& tl : m_transformTargetLayers) {
        if (tl.isRaster) {
            tl.preTiles = tl.layer->tileManager().clone();
            QImage srcImg(targetRect.size(), QImage::Format_ARGB32_Premultiplied);
            srcImg.fill(Qt::transparent);

            for (int ty = minTy; ty <= maxTy; ++ty) {
                for (int tx = minTx; tx <= maxTx; ++tx) {
                    const Tile* tile = tl.layer->tileManager().getTile(tx, ty);
                    if (!tile || !tile->isValid() || !tile->readBuffer()) continue;
                    const uint32_t* srcBuf = reinterpret_cast<const uint32_t*>(tile->readBuffer());

                    int tileX0 = tx * TileMemoryPool::TILE_WIDTH;
                    int tileY0 = ty * TileMemoryPool::TILE_HEIGHT;
                    int px0 = std::max(targetRect.left(), tileX0);
                    int px1 = std::min(targetRect.right(), tileX0 + static_cast<int>(TileMemoryPool::TILE_WIDTH) - 1);
                    int py0 = std::max(targetRect.top(), tileY0);
                    int py1 = std::min(targetRect.bottom(), tileY0 + static_cast<int>(TileMemoryPool::TILE_HEIGHT) - 1);

                    for (int py = py0; py <= py1; ++py) {
                        int ly = py - tileY0;
                        int imgY = py - targetRect.top();
                        uint32_t* dstScan = reinterpret_cast<uint32_t*>(srcImg.scanLine(imgY));

                        for (int px = px0; px <= px1; ++px) {
                            uint8_t sel = m_transformHasSelection ? m_document->selectionStrength(px, py) : 255;
                            if (sel == 0) continue;
                            int lx = px - tileX0;
                            int imgX = px - targetRect.left();
                            uint32_t sp = srcBuf[ly * TileMemoryPool::TILE_WIDTH + lx];
                            if (sel < 255) {
                                uint32_t sa = (sp >> 24) & 0xFF;
                                sa = static_cast<uint32_t>(sa * (sel / 255.0f) + 0.5f);
                                dstScan[imgX] = (sa << 24) | (sp & 0x00FFFFFF);
                            } else {
                                dstScan[imgX] = sp;
                            }
                        }
                    }
                }
            }
            tl.sourceImage = srcImg;

            // 一時消去
            for (int ty = minTy; ty <= maxTy; ++ty) {
                for (int tx = minTx; tx <= maxTx; ++tx) {
                    Tile* tile = tl.layer->tileManager().getTileForWrite(tx, ty);
                    if (!tile || !tile->isValid() || !tile->writeBuffer()) continue;
                    uint32_t* tileBuf = reinterpret_cast<uint32_t*>(tile->writeBuffer());

                    int tileX0 = tx * TileMemoryPool::TILE_WIDTH;
                    int tileY0 = ty * TileMemoryPool::TILE_HEIGHT;
                    int px0 = std::max(targetRect.left(), tileX0);
                    int px1 = std::min(targetRect.right(), tileX0 + static_cast<int>(TileMemoryPool::TILE_WIDTH) - 1);
                    int py0 = std::max(targetRect.top(), tileY0);
                    int py1 = std::min(targetRect.bottom(), tileY0 + static_cast<int>(TileMemoryPool::TILE_HEIGHT) - 1);

                    for (int py = py0; py <= py1; ++py) {
                        int ly = py - tileY0;
                        for (int px = px0; px <= px1; ++px) {
                            uint8_t sel = m_transformHasSelection ? m_document->selectionStrength(px, py) : 255;
                            if (sel == 0) continue;
                            int lx = px - tileX0;
                            if (sel < 255) {
                                uint32_t curP = tileBuf[ly * TileMemoryPool::TILE_WIDTH + lx];
                                uint32_t curA = (curP >> 24) & 0xFF;
                                uint32_t newA = static_cast<uint32_t>(curA * (1.0f - (sel / 255.0f)) + 0.5f);
                                tileBuf[ly * TileMemoryPool::TILE_WIDTH + lx] = (newA << 24) | (curP & 0x00FFFFFF);
                            } else {
                                tileBuf[ly * TileMemoryPool::TILE_WIDTH + lx] = 0;
                            }
                        }
                    }
                }
            }
            tl.layer->invalidateContentBounds();

            if (tl.layer == activeL) {
                m_transformSourceImage = tl.sourceImage;
                m_transformPreTiles = tl.preTiles;
            }
        } else if (tl.isVector) {
            auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(tl.layer);
            if (vecLayer) {
                tl.origStrokes = vecLayer->strokes();
                vecLayer->setStrokes({}); // プレビュー描画に任せるため一時クリア
                if (tl.layer == activeL) {
                    m_transformOrigStrokes = tl.origStrokes;
                }
            }
        }
    }

    m_document->composite(targetRect);

    QRectF r = m_transformSourceRect;
    m_transformOrigQuad.clear();
    m_transformOrigQuad << r.topLeft() << r.topRight() << r.bottomRight() << r.bottomLeft();
    m_transformCurrentQuad = m_transformOrigQuad;
    m_transformPivot = r.center();

    m_isTransforming = true;
    m_transformHoverHandle = TransformHandle::None;
    m_transformDragHandle = TransformHandle::None;

    if (m_transformMode == TransformMode::Warp) {
        if (m_warpSubMode == WarpSubMode::Mesh) {
            initMeshGrid();
        } else {
            initBrushWarp();
        }
    } else if (m_transformMode == TransformMode::Cage) {
        if (m_cageSubMode == CageSubMode::AutoBox) {
            initCageAutoBox();
        } else {
            resetCage();
        }
    }

    updateCursorForActiveLayer();
    update();
}

void CanvasWidget::cancelTransform() {
    if (!m_isTransforming) return;
    if (m_document) {
        for (auto& tl : m_transformTargetLayers) {
            if (tl.isRaster && tl.preTiles.has_value()) {
                tl.layer->tileManager() = tl.preTiles->clone();
                tl.layer->invalidateContentBounds();
            } else if (tl.isVector) {
                auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(tl.layer);
                if (vecLayer) {
                    vecLayer->setStrokes(tl.origStrokes);
                }
            }
        }
        m_document->composite(m_transformSourceRect);
    }

    m_isTransforming = false;
    m_transformDragHandle = TransformHandle::None;
    m_transformHoverHandle = TransformHandle::None;
    m_transformSourceImage = QImage();
    m_transformOrigStrokes.clear();
    m_transformPreTiles.reset();
    m_transformTargetLayers.clear();

    m_meshOrigPoints.clear();
    m_meshCurrentPoints.clear();
    m_meshDragIdx = -1;
    m_meshHoverIdx = -1;
    m_warpDx.clear();
    m_warpDy.clear();
    m_isWarpDragging = false;
    m_cageOrigPoints.clear();
    m_cageCurrentPoints.clear();
    m_cageDragIdx = -1;
    m_cageHoverIdx = -1;
    m_isCageClosed = false;

    updateCursorForActiveLayer();
    update();
}


void CanvasWidget::applyTransform() {
    if (!m_isTransforming || !m_document) return;
    if (m_transformTargetLayers.empty()) {
        cancelTransform();
        return;
    }

    auto activeL = m_document->activeLayer();
    if (!activeL || activeL->isLocked()) {
        cancelTransform();
        return;
    }

    std::vector<MultiLayerTransformCommand::RasterEntry> rasterEntries;
    std::vector<MultiLayerTransformCommand::VectorEntry> vectorEntries;
    QPainterPath newSel = m_transformOrigSelection;
    QRect totalDirtyRect = m_transformSourceRect;

    // タイルへの書き込み共通処理
    auto writeRenderedImageToLayer = [&](std::shared_ptr<Layer> layer, const QImage& renderedImg, const QRect& clipBounds) {
        if (renderedImg.isNull() || clipBounds.isEmpty() || !layer) return;
        int minTx = clipBounds.left() / TileMemoryPool::TILE_WIDTH;
        int maxTx = clipBounds.right() / TileMemoryPool::TILE_WIDTH;
        int minTy = clipBounds.top() / TileMemoryPool::TILE_HEIGHT;
        int maxTy = clipBounds.bottom() / TileMemoryPool::TILE_HEIGHT;

        for (int ty = minTy; ty <= maxTy; ++ty) {
            for (int tx = minTx; tx <= maxTx; ++tx) {
                Tile* tile = layer->tileManager().getTileForWrite(tx, ty);
                if (!tile || !tile->isValid() || !tile->writeBuffer()) continue;
                uint32_t* dstBuf = reinterpret_cast<uint32_t*>(tile->writeBuffer());

                int tileX0 = tx * TileMemoryPool::TILE_WIDTH;
                int tileY0 = ty * TileMemoryPool::TILE_HEIGHT;
                int px0 = std::max(clipBounds.left(), tileX0);
                int px1 = std::min(clipBounds.right(), tileX0 + static_cast<int>(TileMemoryPool::TILE_WIDTH) - 1);
                int py0 = std::max(clipBounds.top(), tileY0);
                int py1 = std::min(clipBounds.bottom(), tileY0 + static_cast<int>(TileMemoryPool::TILE_HEIGHT) - 1);

                for (int py = py0; py <= py1; ++py) {
                    int ly = py - tileY0;
                    int imgY = py - clipBounds.top();
                    const uint32_t* srcScan = reinterpret_cast<const uint32_t*>(renderedImg.constScanLine(imgY));

                    for (int px = px0; px <= px1; ++px) {
                        int lx = px - tileX0;
                        int imgX = px - clipBounds.left();
                        uint32_t srcPixel = srcScan[imgX];
                        if ((srcPixel >> 24) == 0) continue;

                        if (layer->isAlphaLocked()) {
                            uint32_t dstPixel = dstBuf[ly * TileMemoryPool::TILE_WIDTH + lx];
                            uint32_t dstAlpha = (dstPixel >> 24) & 0xFF;
                            if (dstAlpha == 0) continue;
                            dstBuf[ly * TileMemoryPool::TILE_WIDTH + lx] = blendPixel(dstPixel, srcPixel, dstAlpha / 255.0f, BlendMode::Normal);
                        } else {
                            uint32_t dstPixel = dstBuf[ly * TileMemoryPool::TILE_WIDTH + lx];
                            dstBuf[ly * TileMemoryPool::TILE_WIDTH + lx] = blendPixel(dstPixel, srcPixel, 1.0f, BlendMode::Normal);
                        }
                    }
                }
            }
        }
    };

    if (m_transformMode == TransformMode::Unified) {
        QTransform transformMatrix;
        bool invertible = QTransform::quadToQuad(m_transformOrigQuad, m_transformCurrentQuad, transformMatrix);
        if (!invertible) {
            cancelTransform();
            return;
        }

        QRectF transformedBoundsF = transformMatrix.map(m_transformOrigQuad).boundingRect();
        QRect transformedBounds = transformedBoundsF.toAlignedRect();
        QRect clipBounds = transformedBounds.intersected(QRect(0, 0, m_document->width(), m_document->height()));

        if (m_transformHasSelection) {
            newSel = transformMatrix.map(m_transformOrigSelection);
        }

        double det = std::abs(transformMatrix.m11() * transformMatrix.m22() - transformMatrix.m12() * transformMatrix.m21());
        float scaleFactor = static_cast<float>(std::sqrt(det));
        if (scaleFactor <= 0.0001f) scaleFactor = 1.0f;

        for (auto& tl : m_transformTargetLayers) {
            if (tl.isRaster && tl.preTiles.has_value()) {
                if (!clipBounds.isEmpty() && clipBounds.width() > 0 && clipBounds.height() > 0 && !tl.sourceImage.isNull()) {
                    QImage renderedImg(clipBounds.size(), QImage::Format_ARGB32_Premultiplied);
                    renderedImg.fill(Qt::transparent);

                    QPainter p(&renderedImg);
                    if (m_transformInterpolation == InterpolationMode::NearestNeighbor) {
                        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
                        p.setRenderHint(QPainter::Antialiasing, false);
                    } else if (m_transformInterpolation == InterpolationMode::Bicubic) {
                        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
                        p.setRenderHint(QPainter::Antialiasing, true);
                    } else {
                        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
                        p.setRenderHint(QPainter::Antialiasing, false);
                    }

                    QTransform renderT = transformMatrix * QTransform().translate(-clipBounds.left(), -clipBounds.top());
                    p.setTransform(renderT);
                    p.drawImage(m_transformSourceRect.topLeft(), tl.sourceImage);
                    p.end();

                    writeRenderedImageToLayer(tl.layer, renderedImg, clipBounds);
                }

                tl.layer->invalidateContentBounds();
                QRect dirtyRect = m_transformSourceRect.united(clipBounds);
                totalDirtyRect = totalDirtyRect.united(dirtyRect);
                TileManager postTiles = tl.layer->tileManager().clone();
                rasterEntries.push_back({tl.layerIndex, tl.preTiles->clone(), postTiles, dirtyRect});

            } else if (tl.isVector) {
                auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(tl.layer);
                if (vecLayer) {
                    std::vector<VectorStroke> newStrokes = tl.origStrokes;
                    for (auto& stroke : newStrokes) {
                        stroke.baseWidth *= scaleFactor;
                        for (auto& pt : stroke.points) {
                            QPointF mapped = transformMatrix.map(QPointF(pt.x, pt.y));
                            pt.x = static_cast<float>(mapped.x());
                            pt.y = static_cast<float>(mapped.y());
                        }
                    }
                    vecLayer->setStrokes(newStrokes);
                    vectorEntries.push_back({tl.layerIndex, tl.origStrokes, newStrokes});
                }
            }
        }

    } else if (m_transformMode == TransformMode::Warp) {
        if (m_warpSubMode == WarpSubMode::Mesh) {
            QRectF meshBounds;
            for (const auto& pt : m_meshCurrentPoints) {
                if (meshBounds.isNull()) meshBounds = QRectF(pt, QSizeF(1, 1));
                else meshBounds = meshBounds.united(QRectF(pt, QSizeF(1, 1)));
            }
            if (meshBounds.isNull()) meshBounds = m_transformSourceRect;
            QRect clipBounds = meshBounds.toAlignedRect().intersected(QRect(0, 0, m_document->width(), m_document->height()));

            int N = m_warpMeshDivisions;
            int pts = N + 1;

            for (auto& tl : m_transformTargetLayers) {
                if (tl.isRaster && tl.preTiles.has_value()) {
                    if (!clipBounds.isEmpty() && clipBounds.width() > 0 && clipBounds.height() > 0 && !tl.sourceImage.isNull()) {
                        QImage renderedImg(clipBounds.size(), QImage::Format_ARGB32_Premultiplied);
                        renderedImg.fill(Qt::transparent);

                        QPainter p(&renderedImg);
                        p.translate(-clipBounds.left(), -clipBounds.top());
                        p.setRenderHint(QPainter::SmoothPixmapTransform, m_transformInterpolation != InterpolationMode::NearestNeighbor);
                        p.setRenderHint(QPainter::Antialiasing, true);

                        for (int j = 0; j < N; ++j) {
                            for (int i = 0; i < N; ++i) {
                                int idx00 = j * pts + i;
                                int idx10 = j * pts + (i + 1);
                                int idx11 = (j + 1) * pts + (i + 1);
                                int idx01 = (j + 1) * pts + i;

                                QPolygonF srcQuad, dstQuad;
                                srcQuad << m_meshOrigPoints[idx00] << m_meshOrigPoints[idx10] << m_meshOrigPoints[idx11] << m_meshOrigPoints[idx01];
                                dstQuad << m_meshCurrentPoints[idx00] << m_meshCurrentPoints[idx10] << m_meshCurrentPoints[idx11] << m_meshCurrentPoints[idx01];

                                QTransform cellT;
                                if (QTransform::quadToQuad(srcQuad, dstQuad, cellT)) {
                                    p.save();
                                    QPainterPath clipPath;
                                    clipPath.addPolygon(dstQuad);
                                    p.setClipPath(clipPath);
                                    p.setTransform(cellT, true);
                                    p.drawImage(m_transformSourceRect.topLeft(), tl.sourceImage);
                                    p.restore();
                                }
                            }
                        }
                        p.end();

                        writeRenderedImageToLayer(tl.layer, renderedImg, clipBounds);
                    }

                    tl.layer->invalidateContentBounds();
                    QRect dirtyRect = m_transformSourceRect.united(clipBounds);
                    totalDirtyRect = totalDirtyRect.united(dirtyRect);
                    TileManager postTiles = tl.layer->tileManager().clone();
                    rasterEntries.push_back({tl.layerIndex, tl.preTiles->clone(), postTiles, dirtyRect});

                } else if (tl.isVector) {
                    auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(tl.layer);
                    if (vecLayer) {
                        std::vector<VectorStroke> newStrokes = tl.origStrokes;
                        for (auto& stroke : newStrokes) {
                            for (auto& pt : stroke.points) {
                                QPointF p(pt.x, pt.y);
                                qreal u = (p.x() - m_transformSourceRect.left()) / (m_transformSourceRect.width() > 0 ? m_transformSourceRect.width() : 1);
                                qreal v = (p.y() - m_transformSourceRect.top()) / (m_transformSourceRect.height() > 0 ? m_transformSourceRect.height() : 1);
                                int ci = std::clamp(static_cast<int>(std::floor(u * N)), 0, N - 1);
                                int cj = std::clamp(static_cast<int>(std::floor(v * N)), 0, N - 1);

                                int idx00 = cj * pts + ci;
                                int idx10 = cj * pts + (ci + 1);
                                int idx11 = (cj + 1) * pts + (ci + 1);
                                int idx01 = (cj + 1) * pts + ci;

                                QPolygonF srcQuad, dstQuad;
                                srcQuad << m_meshOrigPoints[idx00] << m_meshOrigPoints[idx10] << m_meshOrigPoints[idx11] << m_meshOrigPoints[idx01];
                                dstQuad << m_meshCurrentPoints[idx00] << m_meshCurrentPoints[idx10] << m_meshCurrentPoints[idx11] << m_meshCurrentPoints[idx01];

                                QTransform cellT;
                                if (QTransform::quadToQuad(srcQuad, dstQuad, cellT)) {
                                    QPointF mapped = cellT.map(p);
                                    pt.x = static_cast<float>(mapped.x());
                                    pt.y = static_cast<float>(mapped.y());
                                }
                            }
                        }
                        vecLayer->setStrokes(newStrokes);
                        vectorEntries.push_back({tl.layerIndex, tl.origStrokes, newStrokes});
                    }
                }
            }

        } else { // Brush Warp
            QRect clipBounds = m_transformSourceRect.intersected(QRect(0, 0, m_document->width(), m_document->height()));
            int w = m_transformSourceRect.width();
            int h = m_transformSourceRect.height();
            int step = 16;
            int nx = (w + step - 1) / step;
            int ny = (h + step - 1) / step;

            for (auto& tl : m_transformTargetLayers) {
                if (tl.isRaster && tl.preTiles.has_value()) {
                    if (!clipBounds.isEmpty() && clipBounds.width() > 0 && clipBounds.height() > 0 && !tl.sourceImage.isNull()) {
                        QImage renderedImg(clipBounds.size(), QImage::Format_ARGB32_Premultiplied);
                        renderedImg.fill(Qt::transparent);

                        QPainter p(&renderedImg);
                        p.translate(-clipBounds.left(), -clipBounds.top());
                        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
                        p.setRenderHint(QPainter::Antialiasing, true);

                        if (!m_warpDx.empty() && !m_warpDy.empty()) {
                            for (int cy = 0; cy < ny; ++cy) {
                                int y0 = cy * step;
                                int y1 = std::min(h, (cy + 1) * step);
                                if (y0 >= y1) continue;

                                for (int cx = 0; cx < nx; ++cx) {
                                    int x0 = cx * step;
                                    int x1 = std::min(w, (cx + 1) * step);
                                    if (x0 >= x1) continue;

                                    auto getPt = [&](int px, int py) {
                                        int cxClamped = std::clamp(px, 0, w - 1);
                                        int cyClamped = std::clamp(py, 0, h - 1);
                                        int idx = cyClamped * w + cxClamped;
                                        return QPointF(m_transformSourceRect.left() + px + m_warpDx[idx],
                                                       m_transformSourceRect.top() + py + m_warpDy[idx]);
                                    };

                                    QPolygonF srcQuad, dstQuad;
                                    srcQuad << QPointF(m_transformSourceRect.left() + x0, m_transformSourceRect.top() + y0)
                                            << QPointF(m_transformSourceRect.left() + x1, m_transformSourceRect.top() + y0)
                                            << QPointF(m_transformSourceRect.left() + x1, m_transformSourceRect.top() + y1)
                                            << QPointF(m_transformSourceRect.left() + x0, m_transformSourceRect.top() + y1);
                                    dstQuad << getPt(x0, y0) << getPt(x1, y0) << getPt(x1, y1) << getPt(x0, y1);

                                    QTransform t;
                                    if (QTransform::quadToQuad(srcQuad, dstQuad, t)) {
                                        p.save();
                                        QPainterPath clip;
                                        clip.addPolygon(dstQuad);
                                        p.setClipPath(clip);
                                        p.setTransform(t, true);
                                        p.drawImage(m_transformSourceRect.topLeft(), tl.sourceImage);
                                        p.restore();
                                    }
                                }
                            }
                        } else {
                            p.drawImage(m_transformSourceRect.topLeft(), tl.sourceImage);
                        }
                        p.end();

                        writeRenderedImageToLayer(tl.layer, renderedImg, clipBounds);
                    }

                    tl.layer->invalidateContentBounds();
                    QRect dirtyRect = m_transformSourceRect.united(clipBounds);
                    totalDirtyRect = totalDirtyRect.united(dirtyRect);
                    TileManager postTiles = tl.layer->tileManager().clone();
                    rasterEntries.push_back({tl.layerIndex, tl.preTiles->clone(), postTiles, dirtyRect});

                } else if (tl.isVector) {
                    auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(tl.layer);
                    if (vecLayer) {
                        vecLayer->setStrokes(tl.origStrokes);
                        vectorEntries.push_back({tl.layerIndex, tl.origStrokes, tl.origStrokes});
                    }
                }
            }
        }

    } else if (m_transformMode == TransformMode::Cage) {
        QRectF cageBounds;
        for (const auto& pt : m_cageCurrentPoints) {
            if (cageBounds.isNull()) cageBounds = QRectF(pt, QSizeF(1, 1));
            else cageBounds = cageBounds.united(QRectF(pt, QSizeF(1, 1)));
        }
        cageBounds = cageBounds.united(m_transformSourceRect);
        QRect clipBounds = cageBounds.toAlignedRect().intersected(QRect(0, 0, m_document->width(), m_document->height()));

        int N = 16;
        QRectF r = m_transformSourceRect;
        qreal stepX = r.width() / N;
        qreal stepY = r.height() / N;
        int pts = N + 1;
        std::vector<QPointF> mappedGrid;
        if (!m_cageOrigPoints.empty() && !m_cageCurrentPoints.empty()) {
            mappedGrid.resize(pts * pts);
            for (int j = 0; j <= N; ++j) {
                qreal y = r.top() + j * stepY;
                for (int i = 0; i <= N; ++i) {
                    qreal x = r.left() + i * stepX;
                    mappedGrid[j * pts + i] = evaluateMLS(QPointF(x, y), m_cageOrigPoints, m_cageCurrentPoints);
                }
            }
        }

        for (auto& tl : m_transformTargetLayers) {
            if (tl.isRaster && tl.preTiles.has_value()) {
                if (!clipBounds.isEmpty() && clipBounds.width() > 0 && clipBounds.height() > 0 && !tl.sourceImage.isNull()) {
                    QImage renderedImg(clipBounds.size(), QImage::Format_ARGB32_Premultiplied);
                    renderedImg.fill(Qt::transparent);

                    QPainter p(&renderedImg);
                    p.translate(-clipBounds.left(), -clipBounds.top());
                    p.setRenderHint(QPainter::SmoothPixmapTransform, m_transformInterpolation != InterpolationMode::NearestNeighbor);
                    p.setRenderHint(QPainter::Antialiasing, true);

                    if (!mappedGrid.empty()) {
                        for (int j = 0; j < N; ++j) {
                            for (int i = 0; i < N; ++i) {
                                int idx00 = j * pts + i;
                                int idx10 = j * pts + (i + 1);
                                int idx11 = (j + 1) * pts + (i + 1);
                                int idx01 = (j + 1) * pts + i;

                                QPointF p00 = mappedGrid[idx00], p10 = mappedGrid[idx10];
                                QPointF p11 = mappedGrid[idx11], p01 = mappedGrid[idx01];

                                QPolygonF srcQuad, dstQuad;
                                srcQuad << QPointF(r.left() + i * stepX, r.top() + j * stepY)
                                        << QPointF(r.left() + (i + 1) * stepX, r.top() + j * stepY)
                                        << QPointF(r.left() + (i + 1) * stepX, r.top() + (j + 1) * stepY)
                                        << QPointF(r.left() + i * stepX, r.top() + (j + 1) * stepY);
                                dstQuad << p00 << p10 << p11 << p01;

                                QTransform t;
                                if (QTransform::quadToQuad(srcQuad, dstQuad, t)) {
                                    p.save();
                                    QPainterPath clip;
                                    clip.addPolygon(dstQuad);
                                    p.setClipPath(clip);
                                    p.setTransform(t, true);
                                    p.drawImage(m_transformSourceRect.topLeft(), tl.sourceImage);
                                    p.restore();
                                }
                            }
                        }
                    } else {
                        p.drawImage(m_transformSourceRect.topLeft(), tl.sourceImage);
                    }
                    p.end();

                    writeRenderedImageToLayer(tl.layer, renderedImg, clipBounds);
                }

                tl.layer->invalidateContentBounds();
                QRect dirtyRect = m_transformSourceRect.united(clipBounds);
                totalDirtyRect = totalDirtyRect.united(dirtyRect);
                TileManager postTiles = tl.layer->tileManager().clone();
                rasterEntries.push_back({tl.layerIndex, tl.preTiles->clone(), postTiles, dirtyRect});

            } else if (tl.isVector) {
                auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(tl.layer);
                if (vecLayer) {
                    std::vector<VectorStroke> newStrokes = tl.origStrokes;
                    if (!m_cageOrigPoints.empty() && !m_cageCurrentPoints.empty()) {
                        for (auto& stroke : newStrokes) {
                            for (auto& pt : stroke.points) {
                                QPointF m = evaluateMLS(QPointF(pt.x, pt.y), m_cageOrigPoints, m_cageCurrentPoints);
                                pt.x = static_cast<float>(m.x());
                                pt.y = static_cast<float>(m.y());
                            }
                        }
                    }
                    vecLayer->setStrokes(newStrokes);
                    vectorEntries.push_back({tl.layerIndex, tl.origStrokes, newStrokes});
                }
            }
        }
    }

    if (m_transformHasSelection) {
        m_document->setSelection(newSel);
    }

    m_document->composite(totalDirtyRect);

    if (m_document->undoStack()) {
        m_document->undoStack()->push(new MultiLayerTransformCommand(
            m_document, std::move(rasterEntries), std::move(vectorEntries),
            m_transformHasSelection, m_transformOrigSelection, newSel
        ));
    }

    m_isTransforming = false;
    m_transformDragHandle = TransformHandle::None;
    m_transformHoverHandle = TransformHandle::None;
    m_transformSourceImage = QImage();
    m_transformOrigStrokes.clear();
    m_transformPreTiles.reset();
    m_transformTargetLayers.clear();
    m_meshOrigPoints.clear();
    m_meshCurrentPoints.clear();
    m_warpDx.clear();
    m_warpDy.clear();
    m_cageOrigPoints.clear();
    m_cageCurrentPoints.clear();
    updateCursorForActiveLayer();
    update();
}


CanvasWidget::TransformHandle CanvasWidget::hitTestTransformHandle(const QPointF& mouseViewPos) const {
    if (!m_isTransforming || m_transformCurrentQuad.size() < 4) return TransformHandle::None;

    QTransform tf = viewTransform();
    QPointF p0 = tf.map(m_transformCurrentQuad[0]);
    QPointF p1 = tf.map(m_transformCurrentQuad[1]);
    QPointF p2 = tf.map(m_transformCurrentQuad[2]);
    QPointF p3 = tf.map(m_transformCurrentQuad[3]);
    QPointF pivot = tf.map(m_transformPivot);

    qreal handleRadius = 8.0;

    // 1. ピボット
    if (QLineF(mouseViewPos, pivot).length() <= handleRadius + 2.0) {
        return TransformHandle::Pivot;
    }

    // 2. コーナーハンドル
    if (QLineF(mouseViewPos, p0).length() <= handleRadius) return TransformHandle::TopLeft;
    if (QLineF(mouseViewPos, p1).length() <= handleRadius) return TransformHandle::TopRight;
    if (QLineF(mouseViewPos, p2).length() <= handleRadius) return TransformHandle::BottomRight;
    if (QLineF(mouseViewPos, p3).length() <= handleRadius) return TransformHandle::BottomLeft;

    // 3. 辺ハンドル
    QPointF topMid = (p0 + p1) * 0.5;
    QPointF rightMid = (p1 + p2) * 0.5;
    QPointF bottomMid = (p2 + p3) * 0.5;
    QPointF leftMid = (p3 + p0) * 0.5;

    if (QLineF(mouseViewPos, topMid).length() <= handleRadius) return TransformHandle::TopEdge;
    if (QLineF(mouseViewPos, rightMid).length() <= handleRadius) return TransformHandle::RightEdge;
    if (QLineF(mouseViewPos, bottomMid).length() <= handleRadius) return TransformHandle::BottomEdge;
    if (QLineF(mouseViewPos, leftMid).length() <= handleRadius) return TransformHandle::LeftEdge;

    // 4. 四角形の内側判定
    QPolygonF viewQuad;
    viewQuad << p0 << p1 << p2 << p3;
    if (viewQuad.containsPoint(mouseViewPos, Qt::OddEvenFill)) {
        return TransformHandle::Inside;
    }

    // 5. 外側回転エリア判定 (外枠から 8px〜40px の範囲)
    auto distToSegment = [](const QPointF& pt, const QPointF& a, const QPointF& b) {
        QPointF ab = b - a;
        qreal lenSq = ab.x() * ab.x() + ab.y() * ab.y();
        if (lenSq == 0.0) return QLineF(pt, a).length();
        qreal t = std::clamp(((pt.x() - a.x()) * ab.x() + (pt.y() - a.y()) * ab.y()) / lenSq, 0.0, 1.0);
        QPointF proj = a + t * ab;
        return QLineF(pt, proj).length();
    };

    qreal d0 = distToSegment(mouseViewPos, p0, p1);
    qreal d1 = distToSegment(mouseViewPos, p1, p2);
    qreal d2 = distToSegment(mouseViewPos, p2, p3);
    qreal d3 = distToSegment(mouseViewPos, p3, p0);
    qreal minDist = std::min({d0, d1, d2, d3});

    if (minDist >= 8.0 && minDist <= 40.0) {
        return TransformHandle::RotateOuter;
    }

    return TransformHandle::None;
}

void CanvasWidget::updateTransformCursor(TransformHandle handle, Qt::KeyboardModifiers mods) {
    if (m_currentTool != ToolType::Transform) return;
    if (!m_isTransforming) {
        setCursor(Qt::CrossCursor);
        return;
    }

    switch (handle) {
    case TransformHandle::Inside:
        setCursor(Qt::SizeAllCursor);
        break;
    case TransformHandle::Pivot:
        setCursor(Qt::CrossCursor);
        break;
    case TransformHandle::RotateOuter:
        setCursor(Qt::PointingHandCursor);
        break;
    case TransformHandle::TopLeft:
    case TransformHandle::BottomRight:
        if (mods & Qt::ControlModifier) {
            setCursor(Qt::CrossCursor);
        } else {
            setCursor(Qt::SizeFDiagCursor);
        }
        break;
    case TransformHandle::TopRight:
    case TransformHandle::BottomLeft:
        if (mods & Qt::ControlModifier) {
            setCursor(Qt::CrossCursor);
        } else {
            setCursor(Qt::SizeBDiagCursor);
        }
        break;
    case TransformHandle::TopEdge:
    case TransformHandle::BottomEdge:
        setCursor(Qt::SizeVerCursor);
        break;
    case TransformHandle::LeftEdge:
    case TransformHandle::RightEdge:
        setCursor(Qt::SizeHorCursor);
        break;
    case TransformHandle::None:
    default:
        setCursor(Qt::ArrowCursor);
        break;
    }
}

void CanvasWidget::setTransformMode(TransformMode mode) {
    if (m_transformMode == mode) return;
    m_transformMode = mode;
    if (m_isTransforming) {
        if (m_transformMode == TransformMode::Warp) {
            if (m_warpSubMode == WarpSubMode::Mesh) {
                initMeshGrid();
            } else {
                initBrushWarp();
            }
        } else if (m_transformMode == TransformMode::Cage) {
            if (m_cageSubMode == CageSubMode::AutoBox) {
                initCageAutoBox();
            } else {
                resetCage();
            }
        }
    }
    update();
}

void CanvasWidget::setWarpSubMode(WarpSubMode mode) {
    if (m_warpSubMode == mode) return;
    m_warpSubMode = mode;
    if (m_isTransforming && m_transformMode == TransformMode::Warp) {
        if (m_warpSubMode == WarpSubMode::Mesh) {
            initMeshGrid();
        } else {
            initBrushWarp();
        }
    }
    update();
}

void CanvasWidget::setWarpMeshDivisions(int div) {
    int clamped = std::clamp(div, 2, 16);
    if (m_warpMeshDivisions == clamped) return;
    m_warpMeshDivisions = clamped;
    if (m_isTransforming && m_transformMode == TransformMode::Warp && m_warpSubMode == WarpSubMode::Mesh) {
        initMeshGrid();
    }
    update();
}

void CanvasWidget::setCageSubMode(CageSubMode mode) {
    if (m_cageSubMode == mode) return;
    m_cageSubMode = mode;
    if (m_isTransforming && m_transformMode == TransformMode::Cage) {
        if (m_cageSubMode == CageSubMode::AutoBox) {
            initCageAutoBox();
        } else {
            resetCage();
        }
    }
    update();
}

void CanvasWidget::resetCage() {
    m_cageOrigPoints.clear();
    m_cageCurrentPoints.clear();
    m_cageDragIdx = -1;
    m_cageHoverIdx = -1;
    m_isCageClosed = false;
    if (m_cageSubMode == CageSubMode::AutoBox) {
        initCageAutoBox();
    }
    update();
}

void CanvasWidget::flipTransformHorizontal() {
    if (!m_isTransforming) return;
    qreal cx = m_transformPivot.x();

    // 1. Unified Quad
    for (int i = 0; i < m_transformCurrentQuad.size(); ++i) {
        qreal x = m_transformCurrentQuad[i].x();
        m_transformCurrentQuad[i].setX(2.0 * cx - x);
    }
    if (m_transformCurrentQuad.size() == 4) {
        std::swap(m_transformCurrentQuad[0], m_transformCurrentQuad[1]);
        std::swap(m_transformCurrentQuad[3], m_transformCurrentQuad[2]);
    }

    // 2. Mesh Warp
    if (!m_meshCurrentPoints.empty()) {
        int N = m_warpMeshDivisions;
        int pts = N + 1;
        for (auto& pt : m_meshCurrentPoints) {
            pt.setX(2.0 * cx - pt.x());
        }
        for (int row = 0; row < pts; ++row) {
            for (int col = 0; col < pts / 2; ++col) {
                int leftIdx = row * pts + col;
                int rightIdx = row * pts + (pts - 1 - col);
                std::swap(m_meshCurrentPoints[leftIdx], m_meshCurrentPoints[rightIdx]);
            }
        }
    }

    // 3. Brush Warp
    if (!m_warpDx.empty()) {
        int w = m_transformSourceRect.width();
        int h = m_transformSourceRect.height();
        if (w > 0 && h > 0) {
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w / 2; ++x) {
                    int idxL = y * w + x;
                    int idxR = y * w + (w - 1 - x);
                    float dxL = -m_warpDx[idxR];
                    float dxR = -m_warpDx[idxL];
                    float dyL = m_warpDy[idxR];
                    float dyR = m_warpDy[idxL];
                    m_warpDx[idxL] = dxL;
                    m_warpDx[idxR] = dxR;
                    m_warpDy[idxL] = dyL;
                    m_warpDy[idxR] = dyR;
                }
            }
            m_transformSourceImage = m_transformSourceImage.mirrored(true, false);
        }
    }

    // 4. Cage
    for (auto& pt : m_cageCurrentPoints) {
        pt.setX(2.0 * cx - pt.x());
    }

    update();
}

void CanvasWidget::flipTransformVertical() {
    if (!m_isTransforming) return;
    qreal cy = m_transformPivot.y();

    // 1. Unified Quad
    for (int i = 0; i < m_transformCurrentQuad.size(); ++i) {
        qreal y = m_transformCurrentQuad[i].y();
        m_transformCurrentQuad[i].setY(2.0 * cy - y);
    }
    if (m_transformCurrentQuad.size() == 4) {
        std::swap(m_transformCurrentQuad[0], m_transformCurrentQuad[3]);
        std::swap(m_transformCurrentQuad[1], m_transformCurrentQuad[2]);
    }

    // 2. Mesh Warp
    if (!m_meshCurrentPoints.empty()) {
        int N = m_warpMeshDivisions;
        int pts = N + 1;
        for (auto& pt : m_meshCurrentPoints) {
            pt.setY(2.0 * cy - pt.y());
        }
        for (int row = 0; row < pts / 2; ++row) {
            for (int col = 0; col < pts; ++col) {
                int topIdx = row * pts + col;
                int btmIdx = (pts - 1 - row) * pts + col;
                std::swap(m_meshCurrentPoints[topIdx], m_meshCurrentPoints[btmIdx]);
            }
        }
    }

    // 3. Brush Warp
    if (!m_warpDx.empty()) {
        int w = m_transformSourceRect.width();
        int h = m_transformSourceRect.height();
        if (w > 0 && h > 0) {
            for (int y = 0; y < h / 2; ++y) {
                for (int x = 0; x < w; ++x) {
                    int idxT = y * w + x;
                    int idxB = (h - 1 - y) * w + x;
                    float dxT = m_warpDx[idxB];
                    float dxB = m_warpDx[idxT];
                    float dyT = -m_warpDy[idxB];
                    float dyB = -m_warpDy[idxT];
                    m_warpDx[idxT] = dxT;
                    m_warpDx[idxB] = dxB;
                    m_warpDy[idxT] = dyT;
                    m_warpDy[idxB] = dyB;
                }
            }
            m_transformSourceImage = m_transformSourceImage.mirrored(false, true);
        }
    }

    // 4. Cage
    for (auto& pt : m_cageCurrentPoints) {
        pt.setY(2.0 * cy - pt.y());
    }

    update();
}

void CanvasWidget::initMeshGrid() {
    m_meshOrigPoints.clear();
    m_meshCurrentPoints.clear();
    m_meshDragIdx = -1;
    m_meshHoverIdx = -1;

    int N = m_warpMeshDivisions;
    if (N < 2) N = 2;
    QRectF r = m_transformSourceRect;
    int pts = N + 1;
    m_meshOrigPoints.reserve(pts * pts);
    m_meshCurrentPoints.reserve(pts * pts);

    for (int j = 0; j <= N; ++j) {
        qreal ty = static_cast<qreal>(j) / N;
        qreal y = r.top() + ty * r.height();
        for (int i = 0; i <= N; ++i) {
            qreal tx = static_cast<qreal>(i) / N;
            qreal x = r.left() + tx * r.width();
            QPointF p(x, y);
            m_meshOrigPoints.push_back(p);
            m_meshCurrentPoints.push_back(p);
        }
    }
}

void CanvasWidget::initBrushWarp() {
    int w = m_transformSourceRect.width();
    int h = m_transformSourceRect.height();
    if (w <= 0 || h <= 0) return;
    m_warpDx.assign(w * h, 0.0f);
    m_warpDy.assign(w * h, 0.0f);
    m_isWarpDragging = false;
}

void CanvasWidget::initCageAutoBox() {
    m_cageOrigPoints.clear();
    m_cageCurrentPoints.clear();
    m_cageDragIdx = -1;
    m_cageHoverIdx = -1;
    m_isCageClosed = true;

    QRectF r = m_transformSourceRect;
    QPointF pTL = r.topLeft();
    QPointF pTM(r.center().x(), r.top());
    QPointF pTR = r.topRight();
    QPointF pRM(r.right(), r.center().y());
    QPointF pBR = r.bottomRight();
    QPointF pBM(r.center().x(), r.bottom());
    QPointF pBL = r.bottomLeft();
    QPointF pLM(r.left(), r.center().y());

    std::vector<QPointF> pts = { pTL, pTM, pTR, pRM, pBR, pBM, pBL, pLM };
    m_cageOrigPoints = pts;
    m_cageCurrentPoints = pts;
}

QPointF CanvasWidget::evaluateMLS(const QPointF& pt, const std::vector<QPointF>& origPts, const std::vector<QPointF>& curPts) const {
    size_t n = origPts.size();
    if (n == 0 || curPts.size() != n) return pt;
    if (n == 1) {
        return pt + (curPts[0] - origPts[0]);
    }

    double totalW = 0.0;
    std::vector<double> weights(n, 0.0);
    for (size_t i = 0; i < n; ++i) {
        double dx = origPts[i].x() - pt.x();
        double dy = origPts[i].y() - pt.y();
        double d2 = dx * dx + dy * dy;
        if (d2 < 1e-6) {
            return curPts[i];
        }
        double w = 1.0 / d2;
        weights[i] = w;
        totalW += w;
    }

    double pStarX = 0.0, pStarY = 0.0;
    double qStarX = 0.0, qStarY = 0.0;
    for (size_t i = 0; i < n; ++i) {
        double w = weights[i];
        pStarX += w * origPts[i].x();
        pStarY += w * origPts[i].y();
        qStarX += w * curPts[i].x();
        qStarY += w * curPts[i].y();
    }
    pStarX /= totalW;
    pStarY /= totalW;
    qStarX /= totalW;
    qStarY /= totalW;

    double A00 = 0.0, A01 = 0.0, A11 = 0.0;
    double B00 = 0.0, B01 = 0.0, B10 = 0.0, B11 = 0.0;

    for (size_t i = 0; i < n; ++i) {
        double w = weights[i];
        double phX = origPts[i].x() - pStarX;
        double phY = origPts[i].y() - pStarY;
        double qhX = curPts[i].x() - qStarX;
        double qhY = curPts[i].y() - qStarY;

        A00 += w * phX * phX;
        A01 += w * phX * phY;
        A11 += w * phY * phY;

        B00 += w * qhX * phX;
        B01 += w * qhX * phY;
        B10 += w * qhY * phX;
        B11 += w * qhY * phY;
    }

    double detA = A00 * A11 - A01 * A01;
    if (std::abs(detA) < 1e-9) {
        return pt + QPointF(qStarX - pStarX, qStarY - pStarY);
    }

    double invA00 = A11 / detA;
    double invA01 = -A01 / detA;
    double invA11 = A00 / detA;

    double M00 = B00 * invA00 + B01 * invA01;
    double M01 = B00 * invA01 + B01 * invA11;
    double M10 = B10 * invA00 + B11 * invA01;
    double M11 = B10 * invA01 + B11 * invA11;

    double vhX = pt.x() - pStarX;
    double vhY = pt.y() - pStarY;

    double resX = vhX * M00 + vhY * M01 + qStarX;
    double resY = vhX * M10 + vhY * M11 + qStarY;

    return QPointF(resX, resY);
}

void CanvasWidget::renderTransformPreview(QPainter& painter) {
    if (!m_isTransforming || !m_document) return;
    if (m_transformTargetLayers.empty()) return;

    const auto& filterMgr = DisplayFilterManager::instance();
    bool hasDisplayFilter = (filterMgr.filterType() != DisplayFilterType::None);

    if (m_transformMode == TransformMode::Unified) {
        QTransform transformMatrix;
        bool ok = QTransform::quadToQuad(m_transformOrigQuad, m_transformCurrentQuad, transformMatrix);
        if (!ok) return;

        double det = std::abs(transformMatrix.m11() * transformMatrix.m22() - transformMatrix.m12() * transformMatrix.m21());
        float scaleFactor = static_cast<float>(std::sqrt(det));
        if (scaleFactor <= 0.0001f) scaleFactor = 1.0f;

        for (const auto& tl : m_transformTargetLayers) {
            if (tl.isRaster && !tl.sourceImage.isNull()) {
                painter.save();
                painter.setTransform(transformMatrix, true);
                if (m_transformInterpolation == InterpolationMode::NearestNeighbor) {
                    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
                    painter.setRenderHint(QPainter::Antialiasing, false);
                } else {
                    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
                    painter.setRenderHint(QPainter::Antialiasing, true);
                }
                QImage displayImg = hasDisplayFilter ? filterMgr.applyFilter(tl.sourceImage) : tl.sourceImage;
                painter.drawImage(m_transformSourceRect.topLeft(), displayImg);
                painter.restore();

            } else if (tl.isVector) {
                painter.save();
                painter.setRenderHint(QPainter::Antialiasing, true);
                for (const auto& stroke : tl.origStrokes) {
                    VectorStroke mappedStroke = stroke;
                    mappedStroke.baseWidth *= scaleFactor;
                    for (auto& pt : mappedStroke.points) {
                        QPointF m = transformMatrix.map(QPointF(pt.x, pt.y));
                        pt.x = static_cast<float>(m.x());
                        pt.y = static_cast<float>(m.y());
                    }

                    std::vector<QPointF> samplePts = VectorLayer::sampleStrokePoints(mappedStroke);
                    if (samplePts.size() >= 2) {
                        QPainterPath path;
                        path.moveTo(samplePts[0]);
                        for (size_t i = 1; i < samplePts.size(); ++i) {
                            path.lineTo(samplePts[i]);
                        }
                        QColor col = QColor::fromRgba(mappedStroke.color);
                        col.setAlphaF(std::clamp(mappedStroke.density, 0.0f, 1.0f));
                        painter.setPen(QPen(col, mappedStroke.baseWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                        painter.setBrush(Qt::NoBrush);
                        painter.drawPath(path);
                    }
                }
                painter.restore();
            }
        }
    } else if (m_transformMode == TransformMode::Warp) {
        if (m_warpSubMode == WarpSubMode::Mesh) {
            renderMeshWarpPreview(painter);
        } else {
            renderBrushWarpPreview(painter);
        }
    } else if (m_transformMode == TransformMode::Cage) {
        renderCageTransformPreview(painter);
    }
}

void CanvasWidget::renderMeshWarpPreview(QPainter& painter) {
    if (m_meshOrigPoints.empty() || m_meshCurrentPoints.empty() || !m_document) return;
    int N = m_warpMeshDivisions;
    int pts = N + 1;

    for (const auto& tl : m_transformTargetLayers) {
        if (tl.isRaster && !tl.sourceImage.isNull()) {
            painter.save();
            painter.setRenderHint(QPainter::SmoothPixmapTransform, m_transformInterpolation != InterpolationMode::NearestNeighbor);
            painter.setRenderHint(QPainter::Antialiasing, true);

            for (int j = 0; j < N; ++j) {
                for (int i = 0; i < N; ++i) {
                    int idx00 = j * pts + i;
                    int idx10 = j * pts + (i + 1);
                    int idx11 = (j + 1) * pts + (i + 1);
                    int idx01 = (j + 1) * pts + i;

                    QPolygonF srcQuad, dstQuad;
                    srcQuad << m_meshOrigPoints[idx00] << m_meshOrigPoints[idx10] << m_meshOrigPoints[idx11] << m_meshOrigPoints[idx01];
                    dstQuad << m_meshCurrentPoints[idx00] << m_meshCurrentPoints[idx10] << m_meshCurrentPoints[idx11] << m_meshCurrentPoints[idx01];

                    QTransform cellT;
                    if (QTransform::quadToQuad(srcQuad, dstQuad, cellT)) {
                        painter.save();
                        QPainterPath clipPath;
                        clipPath.addPolygon(dstQuad);
                        painter.setClipPath(clipPath);
                        painter.setTransform(cellT, true);
                        painter.drawImage(m_transformSourceRect.topLeft(), tl.sourceImage);
                        painter.restore();
                    }
                }
            }
            painter.restore();

        } else if (tl.isVector) {
            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);
            for (const auto& stroke : tl.origStrokes) {
                VectorStroke mappedStroke = stroke;
                for (auto& pt : mappedStroke.points) {
                    QPointF p(pt.x, pt.y);
                    qreal u = (p.x() - m_transformSourceRect.left()) / (m_transformSourceRect.width() > 0 ? m_transformSourceRect.width() : 1);
                    qreal v = (p.y() - m_transformSourceRect.top()) / (m_transformSourceRect.height() > 0 ? m_transformSourceRect.height() : 1);
                    int ci = std::clamp(static_cast<int>(std::floor(u * N)), 0, N - 1);
                    int cj = std::clamp(static_cast<int>(std::floor(v * N)), 0, N - 1);

                    int idx00 = cj * pts + ci;
                    int idx10 = cj * pts + (ci + 1);
                    int idx11 = (cj + 1) * pts + (ci + 1);
                    int idx01 = (cj + 1) * pts + ci;

                    QPolygonF srcQuad, dstQuad;
                    srcQuad << m_meshOrigPoints[idx00] << m_meshOrigPoints[idx10] << m_meshOrigPoints[idx11] << m_meshOrigPoints[idx01];
                    dstQuad << m_meshCurrentPoints[idx00] << m_meshCurrentPoints[idx10] << m_meshCurrentPoints[idx11] << m_meshCurrentPoints[idx01];

                    QTransform cellT;
                    if (QTransform::quadToQuad(srcQuad, dstQuad, cellT)) {
                        QPointF mapped = cellT.map(p);
                        pt.x = static_cast<float>(mapped.x());
                        pt.y = static_cast<float>(mapped.y());
                    }
                }

                std::vector<QPointF> samplePts = VectorLayer::sampleStrokePoints(mappedStroke);
                if (samplePts.size() >= 2) {
                    QPainterPath path;
                    path.moveTo(samplePts[0]);
                    for (size_t k = 1; k < samplePts.size(); ++k) path.lineTo(samplePts[k]);
                    QColor col = QColor::fromRgba(mappedStroke.color);
                    col.setAlphaF(std::clamp(mappedStroke.density, 0.0f, 1.0f));
                    painter.setPen(QPen(col, mappedStroke.baseWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawPath(path);
                }
            }
            painter.restore();
        }
    }
}

void CanvasWidget::renderMeshWarpGizmo(QPainter& painter) {
    if (m_meshCurrentPoints.empty()) return;
    int N = m_warpMeshDivisions;
    int pts = N + 1;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    QTransform tf = viewTransform();

    // 格子線
    QPen linePen(QColor(60, 140, 240, 200), 1.0, Qt::DashLine);
    painter.setPen(linePen);
    for (int j = 0; j <= N; ++j) {
        for (int i = 0; i < N; ++i) {
            QPointF pA = tf.map(m_meshCurrentPoints[j * pts + i]);
            QPointF pB = tf.map(m_meshCurrentPoints[j * pts + (i + 1)]);
            painter.drawLine(pA, pB);
        }
    }
    for (int i = 0; i <= N; ++i) {
        for (int j = 0; j < N; ++j) {
            QPointF pA = tf.map(m_meshCurrentPoints[j * pts + i]);
            QPointF pB = tf.map(m_meshCurrentPoints[(j + 1) * pts + i]);
            painter.drawLine(pA, pB);
        }
    }

    // 制御点
    for (int idx = 0; idx < static_cast<int>(m_meshCurrentPoints.size()); ++idx) {
        QPointF p = tf.map(m_meshCurrentPoints[idx]);
        bool isHover = (idx == m_meshHoverIdx || idx == m_meshDragIdx);
        qreal r = isHover ? 6.0 : 4.0;
        painter.setPen(QPen(isHover ? QColor(255, 200, 0) : QColor(30, 30, 30), 1.5));
        painter.setBrush(isHover ? QBrush(QColor(255, 240, 100)) : QBrush(QColor(255, 255, 255)));
        painter.drawEllipse(p, r, r);
    }
    painter.restore();
}

void CanvasWidget::renderBrushWarpPreview(QPainter& painter) {
    if (m_warpDx.empty() || m_warpDy.empty()) {
        for (const auto& tl : m_transformTargetLayers) {
            if (tl.isRaster && !tl.sourceImage.isNull()) {
                painter.drawImage(m_transformSourceRect.topLeft(), tl.sourceImage);
            }
        }
        return;
    }
    int w = m_transformSourceRect.width();
    int h = m_transformSourceRect.height();
    if (w <= 0 || h <= 0) return;

    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::Antialiasing, true);

    int step = 16;
    int nx = (w + step - 1) / step;
    int ny = (h + step - 1) / step;

    for (const auto& tl : m_transformTargetLayers) {
        if (!tl.isRaster || tl.sourceImage.isNull()) continue;

        for (int cy = 0; cy < ny; ++cy) {
            int y0 = cy * step;
            int y1 = std::min(h, (cy + 1) * step);
            if (y0 >= y1) continue;

            for (int cx = 0; cx < nx; ++cx) {
                int x0 = cx * step;
                int x1 = std::min(w, (cx + 1) * step);
                if (x0 >= x1) continue;

                auto getPt = [&](int px, int py) {
                    int cxClamped = std::clamp(px, 0, w - 1);
                    int cyClamped = std::clamp(py, 0, h - 1);
                    int idx = cyClamped * w + cxClamped;
                    return QPointF(m_transformSourceRect.left() + px + m_warpDx[idx],
                                   m_transformSourceRect.top() + py + m_warpDy[idx]);
                };

                QPolygonF srcQuad;
                srcQuad << QPointF(m_transformSourceRect.left() + x0, m_transformSourceRect.top() + y0)
                        << QPointF(m_transformSourceRect.left() + x1, m_transformSourceRect.top() + y0)
                        << QPointF(m_transformSourceRect.left() + x1, m_transformSourceRect.top() + y1)
                        << QPointF(m_transformSourceRect.left() + x0, m_transformSourceRect.top() + y1);

                QPolygonF dstQuad;
                dstQuad << getPt(x0, y0) << getPt(x1, y0) << getPt(x1, y1) << getPt(x0, y1);

                QTransform t;
                if (QTransform::quadToQuad(srcQuad, dstQuad, t)) {
                    painter.save();
                    QPainterPath clip;
                    clip.addPolygon(dstQuad);
                    painter.setClipPath(clip);
                    painter.setTransform(t, true);
                    painter.drawImage(m_transformSourceRect.topLeft(), tl.sourceImage);
                    painter.restore();
                }
            }
        }
    }
    painter.restore();
}

void CanvasWidget::renderBrushWarpGizmo(QPainter& painter) {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    QTransform tf = viewTransform();
    QPointF viewPos = tf.map(m_cursorDocPos);
    qreal viewRadius = (m_warpBrushSize * 0.5) * tf.m11();

    painter.setPen(QPen(QColor(30, 140, 240, 220), 1.5, Qt::DashLine));
    painter.setBrush(QBrush(QColor(30, 140, 240, 30)));
    painter.drawEllipse(viewPos, viewRadius, viewRadius);
    painter.restore();
}

void CanvasWidget::renderCageTransformPreview(QPainter& painter) {
    if (m_cageOrigPoints.empty() || m_cageCurrentPoints.empty()) {
        for (const auto& tl : m_transformTargetLayers) {
            if (tl.isRaster && !tl.sourceImage.isNull()) {
                painter.drawImage(m_transformSourceRect.topLeft(), tl.sourceImage);
            }
        }
        return;
    }

    int N = 16;
    QRectF r = m_transformSourceRect;
    qreal stepX = r.width() / N;
    qreal stepY = r.height() / N;

    int pts = N + 1;
    std::vector<QPointF> mappedGrid(pts * pts);
    for (int j = 0; j <= N; ++j) {
        qreal y = r.top() + j * stepY;
        for (int i = 0; i <= N; ++i) {
            qreal x = r.left() + i * stepX;
            mappedGrid[j * pts + i] = evaluateMLS(QPointF(x, y), m_cageOrigPoints, m_cageCurrentPoints);
        }
    }

    for (const auto& tl : m_transformTargetLayers) {
        if (tl.isRaster && !tl.sourceImage.isNull()) {
            painter.save();
            painter.setRenderHint(QPainter::SmoothPixmapTransform, m_transformInterpolation != InterpolationMode::NearestNeighbor);
            painter.setRenderHint(QPainter::Antialiasing, true);

            for (int j = 0; j < N; ++j) {
                for (int i = 0; i < N; ++i) {
                    qreal x0 = r.left() + i * stepX;
                    qreal x1 = r.left() + (i + 1) * stepX;
                    qreal y0 = r.top() + j * stepY;
                    qreal y1 = r.top() + (j + 1) * stepY;

                    QPolygonF srcQuad;
                    srcQuad << QPointF(x0, y0) << QPointF(x1, y0) << QPointF(x1, y1) << QPointF(x0, y1);

                    QPolygonF dstQuad;
                    dstQuad << mappedGrid[j * pts + i]
                            << mappedGrid[j * pts + (i + 1)]
                            << mappedGrid[(j + 1) * pts + (i + 1)]
                            << mappedGrid[(j + 1) * pts + i];

                    QTransform cellT;
                    if (QTransform::quadToQuad(srcQuad, dstQuad, cellT)) {
                        painter.save();
                        QPainterPath clip;
                        clip.addPolygon(dstQuad);
                        painter.setClipPath(clip);
                        painter.setTransform(cellT, true);
                        painter.drawImage(m_transformSourceRect.topLeft(), tl.sourceImage);
                        painter.restore();
                    }
                }
            }
            painter.restore();

        } else if (tl.isVector) {
            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);
            for (const auto& stroke : tl.origStrokes) {
                VectorStroke mappedStroke = stroke;
                for (auto& pt : mappedStroke.points) {
                    QPointF m = evaluateMLS(QPointF(pt.x, pt.y), m_cageOrigPoints, m_cageCurrentPoints);
                    pt.x = static_cast<float>(m.x());
                    pt.y = static_cast<float>(m.y());
                }

                std::vector<QPointF> samplePts = VectorLayer::sampleStrokePoints(mappedStroke);
                if (samplePts.size() >= 2) {
                    QPainterPath path;
                    path.moveTo(samplePts[0]);
                    for (size_t k = 1; k < samplePts.size(); ++k) path.lineTo(samplePts[k]);
                    QColor col = QColor::fromRgba(mappedStroke.color);
                    col.setAlphaF(std::clamp(mappedStroke.density, 0.0f, 1.0f));
                    painter.setPen(QPen(col, mappedStroke.baseWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawPath(path);
                }
            }
            painter.restore();
        }
    }
}

void CanvasWidget::renderCageTransformGizmo(QPainter& painter) {

    if (m_cageCurrentPoints.empty()) return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    QTransform tf = viewTransform();

    // ケージポリゴン線
    QPen polyPen(QColor(50, 180, 80, 220), 1.5, m_isCageClosed ? Qt::SolidLine : Qt::DashLine);
    painter.setPen(polyPen);
    painter.setBrush(Qt::NoBrush);

    QPolygonF viewPoly;
    for (const auto& pt : m_cageCurrentPoints) {
        viewPoly << tf.map(pt);
    }
    if (m_isCageClosed) {
        painter.drawPolygon(viewPoly);
    } else {
        painter.drawPolyline(viewPoly);
        if (!viewPoly.isEmpty()) {
            painter.drawLine(viewPoly.last(), tf.map(m_cursorDocPos));
        }
    }

    // 制御点
    for (int idx = 0; idx < static_cast<int>(m_cageCurrentPoints.size()); ++idx) {
        QPointF p = tf.map(m_cageCurrentPoints[idx]);
        bool isHover = (idx == m_cageHoverIdx || idx == m_cageDragIdx);
        qreal r = isHover ? 6.5 : 4.5;
        painter.setPen(QPen(isHover ? QColor(255, 200, 0) : QColor(30, 30, 30), 1.5));
        painter.setBrush(isHover ? QBrush(QColor(255, 240, 100)) : (idx == 0 && !m_isCageClosed ? QBrush(QColor(100, 255, 100)) : QBrush(QColor(255, 255, 255))));
        painter.drawEllipse(p, r, r);
    }
    painter.restore();
}

void CanvasWidget::renderTransformGizmo(QPainter& painter) {
    if (!m_isTransforming) return;

    if (m_transformMode == TransformMode::Unified) {
        if (m_transformCurrentQuad.size() < 4) return;

        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);

        QTransform tf = viewTransform();
        QPointF p0 = tf.map(m_transformCurrentQuad[0]);
        QPointF p1 = tf.map(m_transformCurrentQuad[1]);
        QPointF p2 = tf.map(m_transformCurrentQuad[2]);
        QPointF p3 = tf.map(m_transformCurrentQuad[3]);
        QPointF pivot = tf.map(m_transformPivot);

        QPolygonF viewQuad;
        viewQuad << p0 << p1 << p2 << p3;

        // 1. GIMP風 3x3 グリッド線
        QPen gridPen(QColor(100, 150, 240, 140), 1.0, Qt::DashLine);
        painter.setPen(gridPen);
        painter.setBrush(Qt::NoBrush);

        for (int i = 1; i <= 2; ++i) {
            qreal t = i / 3.0;
            QPointF topPt = p0 + (p1 - p0) * t;
            QPointF btmPt = p3 + (p2 - p3) * t;
            painter.drawLine(topPt, btmPt);

            QPointF leftPt = p0 + (p3 - p0) * t;
            QPointF rightPt = p1 + (p2 - p1) * t;
            painter.drawLine(leftPt, rightPt);
        }

        // 2. 外枠線
        painter.setPen(QPen(QColor(40, 40, 40, 200), 2.0));
        painter.drawPolygon(viewQuad);
        painter.setPen(QPen(QColor(255, 255, 255, 230), 1.0));
        painter.drawPolygon(viewQuad);

        // 3. 8個のハンドル
        auto drawHandleBox = [&](const QPointF& pt, bool isHovered) {
            qreal hs = 4.5;
            QRectF hr(pt.x() - hs, pt.y() - hs, hs * 2.0, hs * 2.0);
            painter.setPen(QPen(isHovered ? QColor(255, 120, 0) : QColor(20, 20, 20), 1.2));
            painter.setBrush(isHovered ? QColor(255, 220, 100) : QColor(255, 255, 255));
            painter.drawRect(hr);
        };

        drawHandleBox(p0, m_transformHoverHandle == TransformHandle::TopLeft);
        drawHandleBox(p1, m_transformHoverHandle == TransformHandle::TopRight);
        drawHandleBox(p2, m_transformHoverHandle == TransformHandle::BottomRight);
        drawHandleBox(p3, m_transformHoverHandle == TransformHandle::BottomLeft);

        drawHandleBox((p0 + p1) * 0.5, m_transformHoverHandle == TransformHandle::TopEdge);
        drawHandleBox((p1 + p2) * 0.5, m_transformHoverHandle == TransformHandle::RightEdge);
        drawHandleBox((p2 + p3) * 0.5, m_transformHoverHandle == TransformHandle::BottomEdge);
        drawHandleBox((p3 + p0) * 0.5, m_transformHoverHandle == TransformHandle::LeftEdge);

        // 4. ピボット点
        qreal pivR = 6.0;
        bool isPivotHover = (m_transformHoverHandle == TransformHandle::Pivot);
        painter.setPen(QPen(isPivotHover ? QColor(255, 80, 0) : QColor(0, 120, 240), 1.5));
        painter.setBrush(QColor(255, 255, 255, 180));
        painter.drawEllipse(pivot, pivR, pivR);
        painter.drawLine(pivot.x() - pivR - 3.0, pivot.y(), pivot.x() + pivR + 3.0, pivot.y());
        painter.drawLine(pivot.x(), pivot.y() - pivR - 3.0, pivot.x(), pivot.y() + pivR + 3.0);

        // 5. 外側回転エリアのガイド
        if (m_transformHoverHandle == TransformHandle::RotateOuter || m_transformDragHandle == TransformHandle::RotateOuter) {
            painter.setPen(QPen(QColor(0, 150, 255, 160), 1.0, Qt::DotLine));
            qreal r = QLineF(pivot, tf.map(m_cursorDocPos)).length();
            painter.drawEllipse(pivot, r, r);
        }

        painter.restore();
    } else if (m_transformMode == TransformMode::Warp) {
        if (m_warpSubMode == WarpSubMode::Mesh) {
            renderMeshWarpGizmo(painter);
        } else {
            renderBrushWarpGizmo(painter);
        }
    } else if (m_transformMode == TransformMode::Cage) {
        renderCageTransformGizmo(painter);
    }
}

void CanvasWidget::applyTextSettingsToSelected() {
    if (!m_document || !m_textSettings) return;
    auto activeL = m_document->activeLayer();
    auto textLayer = std::dynamic_pointer_cast<TextLayer>(activeL);
    if (!textLayer) return;
    if (m_selectedBalloonIndex < 0 || m_selectedBalloonIndex >= static_cast<int>(textLayer->items().size())) return;

    const auto& oldItem = textLayer->items()[m_selectedBalloonIndex];
    TextBalloonItem newItem = oldItem;
    m_textSettings->applyToItem(newItem);
    if (!m_textSettings->text().isEmpty()) {
        newItem.text = m_textSettings->text();
    }
    bool changed = (oldItem.font != newItem.font ||
                    oldItem.horizontalAlign != newItem.horizontalAlign ||
                    oldItem.verticalAlign != newItem.verticalAlign ||
                    oldItem.text != newItem.text ||
                    oldItem.shape != newItem.shape ||
                    oldItem.spikeCount != newItem.spikeCount ||
                    std::abs(oldItem.spikeDepth - newItem.spikeDepth) > 0.001f ||
                    std::abs(oldItem.spikePhase - newItem.spikePhase) > 0.001f ||
                    oldItem.hasTail != newItem.hasTail ||
                    std::abs(oldItem.tailBaseWidth - newItem.tailBaseWidth) > 0.001f ||
                    std::abs(oldItem.tailCurvature - newItem.tailCurvature) > 0.001f ||
                    std::abs(oldItem.frameWidth - newItem.frameWidth) > 0.001f ||
                    oldItem.frameColor != newItem.frameColor ||
                    oldItem.fillColor != newItem.fillColor ||
                    oldItem.hasOutline != newItem.hasOutline ||
                    std::abs(oldItem.outlineWidth - newItem.outlineWidth) > 0.001f ||
                    oldItem.outlineColor != newItem.outlineColor);
    if (changed) {
        textLayer->updateItem(m_selectedBalloonIndex, newItem);
        if (m_document->undoStack()) {
            m_document->undoStack()->push(new EditTextBalloonCommand(m_document, m_document->activeLayerIndex(), m_selectedBalloonIndex, oldItem, newItem));
        }
        m_document->composite();
        update();
    }
}

QPointF CanvasWidget::snapToBleed(const QPointF& pt, float threshold) const {
    if (!m_document) return pt;
    if (m_panelSettings && !m_panelSettings->snapToBleed()) return pt;
    QPointF res = pt;
    float w = static_cast<float>(m_document->width());
    float h = static_cast<float>(m_document->height());
    if (std::abs(res.x() - 0.0f) <= threshold) res.setX(0.0f);
    else if (std::abs(res.x() - w) <= threshold) res.setX(w);
    if (std::abs(res.y() - 0.0f) <= threshold) res.setY(0.0f);
    else if (std::abs(res.y() - h) <= threshold) res.setY(h);
    return res;
}

void CanvasWidget::applyPanelSettingsToSelected() {
    if (!m_document || !m_panelSettings) return;
    std::shared_ptr<GroupLayer> targetPanel = m_editingPanel;
    if (!targetPanel) {
        auto activeL = m_document->activeLayer();
        if (activeL && activeL->type() == LayerType::Group) {
            targetPanel = std::dynamic_pointer_cast<GroupLayer>(activeL);
        }
    }
    if (targetPanel && targetPanel->isPanelFolder()) {
        m_panelSettings->applyToPanel(targetPanel.get());
        m_document->composite();
        update();
    }
}

void CanvasWidget::renderPanelOverlay(QPainter& painter) {
    if (!m_document) return;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    qreal invZoom = 1.0 / m_zoom;

    // 1. コマ枠作成中の矩形プレビュー
    if (m_isCreatingPanel) {
        QRectF r = QRectF(m_panelCreateStartDocPos, m_panelCreateCurDocPos).normalized();
        painter.setPen(QPen(QColor(0, 120, 240, 220), 2.0 * invZoom, Qt::DashLine));
        painter.setBrush(QColor(0, 120, 240, 30));
        painter.drawRect(r);
    }

    // 2. コマ分割中の切断線プレビュー
    if (m_isSplittingPanel) {
        QPointF p1 = m_panelSplitStartDocPos;
        QPointF p2 = m_panelSplitCurDocPos;
        QPen cutPen(QColor(255, 60, 60, 220), 2.5 * invZoom, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(cutPen);
        painter.drawLine(p1, p2);

        // 両端点ハンドル
        painter.setPen(QPen(Qt::white, 1.5 * invZoom));
        painter.setBrush(QColor(255, 60, 60));
        painter.drawEllipse(p1, 5.0 * invZoom, 5.0 * invZoom);
        painter.drawEllipse(p2, 5.0 * invZoom, 5.0 * invZoom);
    }

    // 3. 編集対象パネル（またはアクティブコマフォルダ）の頂点・辺ハンドル
    std::shared_ptr<GroupLayer> activePanel = m_editingPanel;
    if (!activePanel) {
        auto activeL = m_document->activeLayer();
        if (activeL && activeL->type() == LayerType::Group) {
            auto grp = std::dynamic_pointer_cast<GroupLayer>(activeL);
            if (grp && grp->isPanelFolder()) {
                activePanel = grp;
            }
        }
    }

    if (activePanel && activePanel->isPanelFolder() && !activePanel->panelPolygon().isEmpty()) {
        const QPolygonF& poly = activePanel->panelPolygon();

        // 輪郭ガイド線 (青色破線)
        painter.setPen(QPen(QColor(0, 150, 255, 180), 1.5 * invZoom, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(poly);

        // 各辺の中点ハンドル (長方形/円)
        for (int i = 0; i < poly.size(); ++i) {
            int next = (i + 1) % poly.size();
            QPointF mid = (poly[i] + poly[next]) * 0.5f;
            painter.setPen(QPen(QColor(30, 30, 30), 1.0 * invZoom));
            painter.setBrush(QColor(0, 200, 255));
            painter.drawRect(QRectF(mid.x() - 3.5 * invZoom, mid.y() - 3.5 * invZoom, 7.0 * invZoom, 7.0 * invZoom));
        }

        // 各頂点ハンドル (四角形ハンドル ■)
        for (int i = 0; i < poly.size(); ++i) {
            QPointF pt = poly[i];
            painter.setPen(QPen(Qt::white, 1.2 * invZoom));
            painter.setBrush(QColor(0, 100, 240));
            painter.drawRect(QRectF(pt.x() - 4.5 * invZoom, pt.y() - 4.5 * invZoom, 9.0 * invZoom, 9.0 * invZoom));
        }
    }

    painter.restore();
}

std::shared_ptr<ThreeDLayer> CanvasWidget::currentThreeDLayer() const {
    if (!m_document) return nullptr;
    auto l = m_document->activeLayer();
    if (l && l->type() == LayerType::ThreeD) {
        return std::dynamic_pointer_cast<ThreeDLayer>(l);
    }
    for (const auto& layer : m_document->layers()) {
        if (layer && layer->type() == LayerType::ThreeD) {
            return std::dynamic_pointer_cast<ThreeDLayer>(layer);
        }
    }
    return nullptr;
}

} // namespace Kaburagi

