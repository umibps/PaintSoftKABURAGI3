#include "ToolBoxWidget.hpp"
#include "BrushSettingsDialog.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QMenu>
#include <QMessageBox>
#include <QContextMenuEvent>
#include <algorithm>

namespace Kaburagi {

SaiToolButton::SaiToolButton(ToolType tool, const QString& label, bool iconOnly, QWidget* parent)
    : QPushButton(parent), m_slotIndex(-1), m_iconOnly(iconOnly) {
    m_item.isEmpty = false;
    m_item.toolType = tool;
    m_item.iconType = tool;
    m_item.name = label;
    setCheckable(true);
    if (m_iconOnly) {
        setFixedSize(26, 24);
        setToolTip(label);
    } else {
        setFixedHeight(44);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setToolTip(label);
    }
}

SaiToolButton::SaiToolButton(int slotIndex, const ToolSlotItem& item, QWidget* parent)
    : QPushButton(parent), m_slotIndex(slotIndex), m_item(item), m_iconOnly(false) {
    setFixedHeight(44);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setSlotItem(item);
}

void SaiToolButton::setSlotItem(const ToolSlotItem& item) {
    m_item = item;
    setCheckable(!m_item.isEmpty);
    if (m_item.isEmpty) {
        setChecked(false);
        setToolTip("");
    } else {
        QString tip = m_item.name;
        if (!m_item.shortcut.isEmpty()) {
            tip += QString(" [%1]").arg(m_item.shortcut.toString());
        }
        setToolTip(tip);
    }
    update();
}

void SaiToolButton::mousePressEvent(QMouseEvent* event) {
    if (m_item.isEmpty) {
        if (event->button() == Qt::LeftButton) {
            emit settingsRequested(m_slotIndex);
        }
        event->accept();
        return;
    }
    QPushButton::mousePressEvent(event);
}

void SaiToolButton::contextMenuEvent(QContextMenuEvent* event) {
    if (m_slotIndex < 0) {
        event->accept();
        return;
    }

    if (m_item.isEmpty) {
        QMenu menu(this);
        menu.setStyleSheet("QMenu { font-size: 11px; }");
        auto actNew = menu.addAction("ツールの新規作成...");
        menu.addSeparator();
        auto actReset = menu.addAction("ツールスロットを初期状態に戻す");
        QAction* selected = menu.exec(event->globalPos());
        if (selected == actNew) {
            emit settingsRequested(m_slotIndex);
        } else if (selected == actReset) {
            emit resetRequested();
        }
        event->accept();
        return;
    }

    QMenu menu(this);
    menu.setStyleSheet("QMenu { font-size: 11px; }");
    auto actSettings = menu.addAction("ツールの設定");
    auto actDuplicate = menu.addAction("ツールを複製");
    auto actDelete = menu.addAction("ツールを削除");
    menu.addSeparator();
    auto actReset = menu.addAction("ツールスロットを初期状態に戻す");

    QAction* selected = menu.exec(event->globalPos());
    if (selected == actSettings) {
        emit settingsRequested(m_slotIndex);
    } else if (selected == actDuplicate) {
        emit duplicateRequested(m_slotIndex);
    } else if (selected == actDelete) {
        emit deleteRequested(m_slotIndex);
    } else if (selected == actReset) {
        emit resetRequested();
    }
    event->accept();
}

void SaiToolButton::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QRect r = rect();

    // 空白ボタン (ツールの登録されていない箇所) の描画
    if (m_item.isEmpty) {
        painter.fillRect(r, QColor(248, 248, 248));
        painter.setPen(QPen(QColor(222, 222, 222), 1.0));
        painter.drawRect(r.adjusted(0, 0, -1, -1));
        return;
    }

    bool checked = isChecked();
    bool hover = underMouse();

    // 背景色と外枠 (SAI風)
    if (checked) {
        painter.fillRect(r, QColor(190, 210, 235)); // 水色ハイライト
        painter.setPen(QPen(QColor(50, 90, 160), 1.5));
        painter.drawRect(r.adjusted(0, 0, -1, -1));
    } else if (hover) {
        painter.fillRect(r, QColor(230, 238, 250));
        painter.setPen(QPen(QColor(120, 150, 200), 1.0));
        painter.drawRect(r.adjusted(0, 0, -1, -1));
    } else {
        painter.fillRect(r, QColor(245, 245, 245));
        painter.setPen(QPen(QColor(180, 180, 180), 1.0));
        painter.drawRect(r.adjusted(0, 0, -1, -1));
    }

    // 1. ラベル描画 (上段)
    if (!m_iconOnly && !m_item.name.isEmpty()) {
        painter.setPen(QColor(20, 20, 20));
        QFont f = painter.font();
        f.setPointSize(8);
        painter.setFont(f);
        painter.drawText(QRect(3, 2, r.width() - 6, 14), Qt::AlignLeft | Qt::AlignVCenter, m_item.name);
    }

    // 2. アイコン描画 (下段、または中央)
    painter.save();
    QRect iconRect = m_iconOnly ? r.adjusted(2, 2, -2, -2) : QRect(r.left() + 4, r.top() + 14, r.width() - 8, r.height() - 16);
    QPointF c = iconRect.center();

    QPen penDark(QColor(30, 30, 30), 1.3);
    painter.setPen(penDark);
    painter.setBrush(Qt::NoBrush);

    switch (m_item.iconType) {
    case ToolType::Pen: {
        // 鉛筆 (斜めのペン)
        painter.translate(c);
        painter.rotate(-45);
        painter.setBrush(QColor(240, 210, 160));
        painter.drawRect(QRectF(-3, -7, 6, 11));
        painter.setBrush(QColor(40, 40, 40));
        QPolygonF tip; tip << QPointF(-3, 4) << QPointF(3, 4) << QPointF(0, 9);
        painter.drawPolygon(tip);
        break;
    }
    case ToolType::Airbrush: {
        // エアブラシ
        painter.translate(c);
        painter.drawLine(QPointF(-5, 5), QPointF(2, -2));
        painter.drawLine(QPointF(-3, 6), QPointF(0, 1));
        painter.drawRect(QRectF(1, -5, 4, 3));
        // スプレー点
        painter.setPen(QPen(QColor(60, 100, 180), 1.2));
        painter.drawPoint(QPointF(4, -5));
        painter.drawPoint(QPointF(6, -3));
        painter.drawPoint(QPointF(7, -6));
        break;
    }
    case ToolType::Watercolor: {
        // 水彩筆 (筆先＋水滴)
        painter.translate(c);
        painter.rotate(-40);
        painter.setBrush(QColor(60, 60, 60));
        QPainterPath path;
        path.moveTo(0, 7);
        path.quadTo(4, 2, 2, -5);
        path.lineTo(-2, -5);
        path.quadTo(-4, 2, 0, 7);
        painter.drawPath(path);
        // 水滴
        painter.setBrush(QColor(80, 140, 220));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(5, 3), 2.0, 2.5);
        break;
    }
    case ToolType::Marker: {
        // ブラシ (絵筆・刷毛風)
        painter.translate(c);
        painter.rotate(-45);
        // 柄 (ハンドル)
        painter.setBrush(QColor(140, 90, 50));
        painter.drawRect(QRectF(-2.5, -7, 5, 7));
        // 金具 (フェルール)
        painter.setBrush(QColor(180, 180, 190));
        painter.drawRect(QRectF(-3, 0, 6, 3));
        // 毛先 (ブラシ先端)
        painter.setBrush(QColor(220, 70, 70));
        QPolygonF tip;
        tip << QPointF(-3, 3) << QPointF(3, 3) << QPointF(1.5, 7.5) << QPointF(-1.5, 7.5);
        painter.drawPolygon(tip);
        break;
    }
    case ToolType::Eraser: {
        // 消しゴム
        painter.translate(c);
        painter.rotate(30);
        painter.setBrush(Qt::white);
        painter.drawRect(QRectF(-6, -4, 12, 8));
        painter.setBrush(QColor(80, 120, 180));
        painter.drawRect(QRectF(-6, -4, 6, 8)); // スリーブ
        break;
    }
    case ToolType::Bucket: {
        // 塗りつぶし (バケツ)
        painter.translate(c);
        painter.rotate(25);
        painter.setBrush(QColor(210, 210, 210));
        QPolygonF poly; poly << QPointF(-5, -5) << QPointF(5, -5) << QPointF(3, 4) << QPointF(-3, 4);
        painter.drawPolygon(poly);
        // 注がれるインク
        painter.setBrush(QColor(60, 100, 200));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(5, 3), 2.5, 3.5);
        break;
    }
    case ToolType::PatternFill: {
        // パターン塗りつぶし (市松模様のバケツ)
        painter.translate(c);
        painter.rotate(25);
        painter.setBrush(QColor(210, 210, 210));
        QPolygonF poly; poly << QPointF(-5, -5) << QPointF(5, -5) << QPointF(3, 4) << QPointF(-3, 4);
        painter.drawPolygon(poly);
        // バケツ内部に市松模様
        painter.setBrush(QColor(100, 100, 100));
        painter.setPen(Qt::NoPen);
        painter.drawRect(QRectF(-2.5, -3, 2.5, 2.5));
        painter.drawRect(QRectF(0, -0.5, 2.5, 2.5));
        // 注がれるインク
        painter.setBrush(QColor(60, 100, 200));
        painter.drawEllipse(QPointF(5, 3), 2.5, 3.5);
        break;
    }
    case ToolType::Blur: {
        // ぼかし (ぼやけた円)
        painter.translate(c);
        painter.setPen(QPen(QColor(100, 150, 220, 120), 2.0));
        painter.drawEllipse(QPointF(0, 0), 5.5, 5.5);
        painter.setPen(QPen(QColor(60, 100, 180, 200), 1.5));
        painter.drawEllipse(QPointF(0, 0), 3.0, 3.0);
        break;
    }
    case ToolType::Blend: {
        // 指先ツール (人差し指を伸ばした手とこすり跡)
        painter.translate(c);
        // 指先のこすり跡（スマッジの波線・トレイル）
        QPainterPath smearPath;
        smearPath.moveTo(-6, 3);
        smearPath.quadTo(-2, 6, 3, 4);
        painter.setPen(QPen(QColor(80, 140, 220, 180), 2.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawPath(smearPath);

        // 手・指 (肌色 / 輪郭線)
        painter.setPen(penDark);
        painter.setBrush(QColor(255, 225, 195));

        QPainterPath handPath;
        handPath.moveTo(6, -6);
        handPath.lineTo(2, -2);
        handPath.lineTo(-2, 2.5);
        handPath.arcTo(QRectF(-4, 1.0, 3.5, 3.5), 225, 180);
        handPath.lineTo(1, 0);
        handPath.lineTo(3, 3);
        handPath.lineTo(7, -1);
        handPath.closeSubpath();
        painter.drawPath(handPath);
        break;
    }
    case ToolType::Gradient: {
        // グラデーション (四角いグラデーションと方向線)
        painter.translate(c);
        QLinearGradient grad(-6, -5, 6, 5);
        grad.setColorAt(0.0, QColor(40, 80, 180));
        grad.setColorAt(1.0, QColor(240, 245, 255));
        painter.setBrush(grad);
        painter.setPen(QPen(QColor(60, 60, 60), 1.0));
        painter.drawRect(QRectF(-6.5, -5.5, 13, 11));

        // 方向線 (始点○、終点○、線)
        painter.setPen(QPen(QColor(20, 20, 20), 1.2));
        painter.drawLine(QPointF(-4, -3), QPointF(4, 3));
        painter.setBrush(Qt::white);
        painter.setPen(QPen(QColor(20, 20, 20), 1.0));
        painter.drawEllipse(QPointF(-4, -3), 1.6, 1.6);
        painter.drawEllipse(QPointF(4, 3), 1.6, 1.6);
        break;
    }
    case ToolType::CompositeBrush: {
        // 合成ブラシ (筆先＋重なり合う合成マーク)
        painter.translate(c);
        painter.save();
        painter.rotate(-35);
        painter.setBrush(QColor(70, 70, 80));
        QPainterPath path;
        path.moveTo(0, 7);
        path.quadTo(4, 2, 2, -5);
        path.lineTo(-2, -5);
        path.quadTo(-4, 2, 0, 7);
        painter.drawPath(path);
        painter.restore();
        // 重なり合う2つの小さな円（合成マーク）
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(60, 140, 240, 180));
        painter.drawEllipse(QPointF(2, 2), 3.0, 3.0);
        painter.setBrush(QColor(240, 100, 80, 180));
        painter.drawEllipse(QPointF(5, 4), 3.0, 3.0);
        break;
    }
    case ToolType::VectorControlPoint: {
        // 制御点 (ベジェ曲線と□)
        painter.translate(c);
        QPainterPath path;
        path.moveTo(-6, 4);
        path.quadTo(0, -6, 6, 4);
        painter.drawPath(path);
        painter.setBrush(QColor(255, 120, 0));
        painter.drawRect(QRectF(-2.5, -3.5, 5, 5));
        break;
    }
    case ToolType::VectorPressure: {
        // 筆圧 (細い→太い線)
        painter.translate(c);
        QPainterPath path;
        path.moveTo(-6, 3);
        path.lineTo(-3, 1);
        path.lineTo(5, -4);
        path.lineTo(7, -1);
        path.lineTo(-1, 4);
        path.closeSubpath();
        painter.setBrush(QColor(40, 40, 40));
        painter.drawPath(path);
        break;
    }
    case ToolType::VectorLineEdit: {
        // 線変更
        painter.translate(c);
        QPainterPath path;
        path.moveTo(-6, 3);
        path.quadTo(0, -5, 6, 2);
        painter.drawPath(path);
        // ハンドル矢印
        painter.setBrush(QColor(0, 120, 220));
        QPolygonF tip; tip << QPointF(0, -5) << QPointF(-2, -9) << QPointF(2, -9);
        painter.drawPolygon(tip);
        break;
    }
    case ToolType::VectorColorChange: {
        // 線色変更 (曲線＋カラードット)
        painter.translate(c);
        painter.setPen(penDark);
        QPainterPath path;
        path.moveTo(-6, 3);
        path.quadTo(0, -5, 6, 2);
        painter.drawPath(path);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(230, 60, 60)); // 赤
        painter.drawEllipse(QPointF(2, -4), 2.2, 2.2);
        painter.setBrush(QColor(60, 150, 240)); // 青
        painter.drawEllipse(QPointF(5, -1), 2.2, 2.2);
        painter.setBrush(QColor(240, 200, 40)); // 黄
        painter.drawEllipse(QPointF(3, 3), 2.2, 2.2);
        break;
    }
    case ToolType::VectorPolyline: {
        // 折れ線ツール (頂点のある折れ線)
        painter.translate(c);
        painter.setPen(penDark);
        QPolygonF poly;
        poly << QPointF(-6, 4) << QPointF(-1, -4) << QPointF(5, 3);
        painter.drawPolyline(poly);
        painter.setBrush(QColor(50, 120, 220));
        painter.drawRect(QRectF(-7, 3, 2.5, 2.5));
        painter.drawRect(QRectF(-2.25, -5.25, 2.5, 2.5));
        painter.drawRect(QRectF(4, 2, 2.5, 2.5));
        break;
    }
    case ToolType::VectorCurve: {
        // 曲線ツール (滑らかなスプライン曲線)
        painter.translate(c);
        painter.setPen(penDark);
        QPainterPath path;
        path.moveTo(-6, 4);
        path.cubicTo(-2, -6, 2, 6, 6, -4);
        painter.drawPath(path);
        painter.setBrush(QColor(50, 120, 220));
        painter.drawEllipse(QPointF(-6, 4), 1.5, 1.5);
        painter.drawEllipse(QPointF(0, 0), 1.5, 1.5);
        painter.drawEllipse(QPointF(6, -4), 1.5, 1.5);
        break;
    }
    case ToolType::VectorEllipse: {
        // 楕円ツール (青い楕円と制御点マーカー)
        painter.translate(c);
        painter.setPen(penDark);
        painter.setBrush(QColor(180, 210, 255, 120));
        painter.drawEllipse(QRectF(-6.5, -4.5, 13, 9));
        painter.setBrush(QColor(50, 120, 220));
        painter.setPen(Qt::NoPen);
        painter.drawRect(QRectF(-7.5, -1, 2, 2));
        painter.drawRect(QRectF(5.5, -1, 2, 2));
        painter.drawRect(QRectF(-1, -5.5, 2, 2));
        painter.drawRect(QRectF(-1, 3.5, 2, 2));
        break;
    }
    case ToolType::VectorCircle: {
        // 円ツール (青い真円と制御点マーカー)
        painter.translate(c);
        painter.setPen(penDark);
        painter.setBrush(QColor(180, 210, 255, 120));
        painter.drawEllipse(QRectF(-5.5, -5.5, 11, 11));
        painter.setBrush(QColor(50, 120, 220));
        painter.setPen(Qt::NoPen);
        painter.drawRect(QRectF(-6.5, -1, 2, 2));
        painter.drawRect(QRectF(4.5, -1, 2, 2));
        painter.drawRect(QRectF(-1, -6.5, 2, 2));
        painter.drawRect(QRectF(-1, 4.5, 2, 2));
        break;
    }
    case ToolType::VectorPolygon: {
        // 多角形ツール (青い五角形と制御点マーカー)
        painter.translate(c);
        painter.setPen(penDark);
        painter.setBrush(QColor(180, 210, 255, 120));
        QPolygonF poly;
        constexpr double PI = 3.14159265358979323846;
        for (int i = 0; i < 5; ++i) {
            double angle = -PI * 0.5 + i * 2.0 * PI / 5.0;
            poly << QPointF(5.5 * std::cos(angle), 5.5 * std::sin(angle));
        }
        painter.drawPolygon(poly);
        painter.setBrush(QColor(50, 120, 220));
        painter.setPen(Qt::NoPen);
        for (int i = 0; i < 5; ++i) {
            painter.drawRect(QRectF(poly[i].x() - 1, poly[i].y() - 1, 2, 2));
        }
        break;
    }
    case ToolType::Text: {
        // テキスト (T アイコン)
        painter.translate(c);
        painter.setFont(QFont("Arial", 11, QFont::Bold));
        painter.setPen(penDark);
        painter.drawText(QRectF(-8, -9, 16, 16), Qt::AlignCenter, "T");
        break;
    }
    case ToolType::TextBalloon: {
        // 吹き出し
        painter.translate(c);
        painter.setBrush(Qt::white);
        QPainterPath path;
        path.addEllipse(QRectF(-7, -6, 14, 10));
        QPolygonF tail; tail << QPointF(1, 3) << QPointF(5, 7) << QPointF(4, 1);
        path.addPolygon(tail);
        painter.drawPath(path);
        break;
    }
    case ToolType::PanelFrame: {
        // コマ枠 (太い矩形枠)
        painter.translate(c);
        painter.setPen(QPen(QColor(30, 30, 30), 2.0));
        painter.setBrush(QColor(245, 245, 245));
        painter.drawRect(QRectF(-7, -6, 14, 12));
        break;
    }
    case ToolType::PanelSplit: {
        // コマ分割 (分割された2つの枠と切断線)
        painter.translate(c);
        painter.setPen(QPen(QColor(60, 60, 60), 1.2));
        painter.setBrush(QColor(245, 245, 245));
        painter.drawRect(QRectF(-7, -6, 6, 12));
        painter.drawRect(QRectF(1, -6, 6, 12));
        // 切断を表す水色の線
        painter.setPen(QPen(QColor(30, 120, 240), 1.5, Qt::DashLine));
        painter.drawLine(QPointF(-0.5, -7), QPointF(-0.5, 7));
        break;
    }
    case ToolType::ThreeD: {
        // 3D操作 (立方体の等角投影)
        painter.translate(c);
        painter.setPen(QPen(QColor(40, 100, 200), 1.2));
        painter.setBrush(QColor(180, 210, 255, 160));
        QPolygonF topFace;
        topFace << QPointF(0, -6) << QPointF(5, -3) << QPointF(0, 0) << QPointF(-5, -3);
        painter.drawPolygon(topFace);
        painter.setBrush(QColor(140, 180, 240, 180));
        QPolygonF leftFace;
        leftFace << QPointF(-5, -3) << QPointF(0, 0) << QPointF(0, 6) << QPointF(-5, 3);
        painter.drawPolygon(leftFace);
        painter.setBrush(QColor(100, 150, 220, 200));
        QPolygonF rightFace;
        rightFace << QPointF(0, 0) << QPointF(5, -3) << QPointF(5, 3) << QPointF(0, 6);
        painter.drawPolygon(rightFace);
        break;
    }
    case ToolType::Pipette: {
        // スポイト
        painter.translate(c);
        painter.rotate(-45);
        painter.setBrush(QColor(220, 220, 220));
        painter.drawRect(QRectF(-2.5, -4, 5, 8));
        // 先端
        QPolygonF tip; tip << QPointF(-2.5, 4) << QPointF(2.5, 4) << QPointF(0, 7.5);
        painter.drawPolygon(tip);
        // 上のゴム部
        painter.setBrush(QColor(80, 80, 80));
        painter.drawRect(QRectF(-3.5, -7, 7, 3));
        break;
    }
    case ToolType::RectSelect: {
        // 矩形選択 (破線の四角)
        painter.translate(c);
        QPen dashPen(QColor(40, 40, 40), 1.2, Qt::DashLine);
        painter.setPen(dashPen);
        painter.drawRect(QRectF(-6, -5, 12, 10));
        break;
    }
    case ToolType::EllipseSelect: {
        // 楕円選択 (破線の楕円)
        painter.translate(c);
        QPen dashPen(QColor(40, 40, 40), 1.2, Qt::DashLine);
        painter.setPen(dashPen);
        painter.drawEllipse(QRectF(-6.5, -5, 13, 10));
        break;
    }
    case ToolType::LassoSelect: {
        // 投げ縄ツール (破線の自由曲線ループ)
        painter.translate(c);
        QPen dashPen(QColor(40, 40, 40), 1.2, Qt::DashLine);
        painter.setPen(dashPen);
        QPainterPath lasso;
        lasso.moveTo(-5, 4);
        lasso.cubicTo(-6, -5, 3, -6, 5, -2);
        lasso.cubicTo(6, 2, 1, 5, -2, 5);
        lasso.closeSubpath();
        painter.drawPath(lasso);
        break;
    }
    case ToolType::ColorSelect: {
        // 色域選択 (3色のグラデーション・パレットと破線選択枠)
        painter.translate(c);
        QPen dashPen(QColor(40, 40, 40), 1.1, Qt::DashLine);
        painter.setPen(dashPen);
        painter.drawRect(QRectF(-6, -6, 12, 12));
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(230, 60, 60));
        painter.drawRect(QRectF(-4, -4, 3.5, 3.5));
        painter.setBrush(QColor(60, 190, 60));
        painter.drawRect(QRectF(-1, -1, 3.5, 3.5));
        painter.setBrush(QColor(50, 110, 220));
        painter.drawRect(QRectF(2, 2, 3.5, 3.5));
        break;
    }
    case ToolType::MagicWand: {
        // 自動選択 (魔法の杖)
        painter.translate(c);
        painter.rotate(-40);
        painter.drawLine(QPointF(0, 6), QPointF(0, -3));
        // 星 / 光
        painter.setPen(QPen(QColor(255, 160, 0), 1.2));
        painter.drawLine(QPointF(0, -6), QPointF(0, -1));
        painter.drawLine(QPointF(-2.5, -3.5), QPointF(2.5, -3.5));
        break;
    }
    case ToolType::Transform: {
        // 変形ツール (少し傾いた矩形、四隅のハンドル)
        painter.translate(c);
        painter.rotate(-12);
        painter.setPen(QPen(QColor(40, 70, 140), 1.2, Qt::SolidLine));
        painter.setBrush(QColor(180, 210, 255, 120));
        painter.drawRect(QRectF(-5.5, -5.5, 11, 11));
        // ハンドル
        painter.setBrush(Qt::white);
        painter.setPen(QPen(QColor(30, 60, 120), 1.0));
        painter.drawRect(QRectF(-6.5, -6.5, 2.5, 2.5));
        painter.drawRect(QRectF(4, -6.5, 2.5, 2.5));
        painter.drawRect(QRectF(4, 4, 2.5, 2.5));
        painter.drawRect(QRectF(-6.5, 4, 2.5, 2.5));
        break;
    }
    case ToolType::LayerSelect: {
        // レイヤー選択 (重なる2枚のレイヤーシートと選択矢印カーソル)
        painter.translate(c);
        // 背景側レイヤー
        painter.setPen(QPen(QColor(130, 150, 175), 1.0));
        painter.setBrush(QColor(210, 225, 245, 190));
        painter.drawRect(QRectF(-6.0, -6.5, 9.5, 7.5));
        // 手前側アクティブレイヤー
        painter.setPen(QPen(QColor(30, 70, 160), 1.2));
        painter.setBrush(QColor(255, 255, 255, 240));
        painter.drawRect(QRectF(-2.5, -3.5, 9.5, 7.5));
        // 選択を表す黒い矢印ポインタ (白フチ付き)
        QPolygonF cursorArrow;
        cursorArrow << QPointF(-4.5, 0.0) << QPointF(-4.5, 7.0) << QPointF(-2.0, 4.5)
                    << QPointF(-0.5, 7.5) << QPointF(1.0, 6.5) << QPointF(-0.5, 3.8) << QPointF(2.0, 3.8);
        painter.setPen(QPen(Qt::white, 1.0));
        painter.setBrush(QColor(25, 25, 25));
        painter.drawPolygon(cursorArrow);
        break;
    }
    case ToolType::AdjustmentBrightnessContrast: {
        // 明るさ・コントラスト (半分白、半分黒の円)
        painter.translate(c);
        painter.setPen(QPen(QColor(50, 50, 50), 1.0));
        painter.setBrush(Qt::black);
        painter.drawPie(QRectF(-6, -6, 12, 12), 90 * 16, 180 * 16);
        painter.setBrush(Qt::white);
        painter.drawPie(QRectF(-6, -6, 12, 12), 270 * 16, 180 * 16);
        break;
    }
    case ToolType::AdjustmentHueSaturation: {
        // 色相・彩度 (カラーホイール風三色円)
        painter.translate(c);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(240, 60, 60));
        painter.drawPie(QRectF(-6, -6, 12, 12), 0 * 16, 120 * 16);
        painter.setBrush(QColor(60, 200, 60));
        painter.drawPie(QRectF(-6, -6, 12, 12), 120 * 16, 120 * 16);
        painter.setBrush(QColor(60, 100, 240));
        painter.drawPie(QRectF(-6, -6, 12, 12), 240 * 16, 120 * 16);
        break;
    }
    case ToolType::AdjustmentToneCurve: {
        // トーンカーブ (枠とS字曲線)
        painter.translate(c);
        painter.setPen(QPen(QColor(140, 140, 140), 1.0));
        painter.setBrush(QColor(245, 245, 245));
        painter.drawRect(QRectF(-6, -6, 12, 12));
        QPainterPath curve;
        curve.moveTo(-5, 5);
        curve.cubicTo(-1, 5, 1, -5, 5, -5);
        painter.setPen(QPen(QColor(30, 80, 200), 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(curve);
        break;
    }
    case ToolType::AdjustmentLevels: {
        // レベル補正 (ヒストグラム3本棒)
        painter.translate(c);
        painter.setPen(QPen(QColor(60, 60, 60), 1.0));
        painter.setBrush(QColor(100, 140, 200));
        painter.drawRect(QRectF(-6, 0, 3, 6));
        painter.drawRect(QRectF(-2, -5, 3, 11));
        painter.drawRect(QRectF(2, -2, 3, 8));
        break;
    }
    case ToolType::AdjustmentInvert: {
        // 階調の反転 (反転アイコン)
        painter.translate(c);
        painter.setPen(QPen(QColor(50, 50, 50), 1.0));
        painter.setBrush(Qt::white);
        painter.drawRect(QRectF(-6, -6, 12, 12));
        painter.setBrush(Qt::black);
        QPolygonF poly; poly << QPointF(-6, -6) << QPointF(6, -6) << QPointF(-6, 6);
        painter.drawPolygon(poly);
        break;
    }
    case ToolType::AdjustmentGrayscale: {
        // 白黒化 (グレースケール円)
        painter.translate(c);
        QLinearGradient grad(QPointF(-6, 0), QPointF(6, 0));
        grad.setColorAt(0.0, Qt::black);
        grad.setColorAt(1.0, Qt::white);
        painter.setPen(QPen(QColor(60, 60, 60), 1.0));
        painter.setBrush(grad);
        painter.drawEllipse(QRectF(-6, -6, 12, 12));
        break;
    }
    }
    painter.restore();
}

// ----------------------------------------------------------------------------
// ToolBoxWidget 実装
// ----------------------------------------------------------------------------

ToolBoxWidget::ToolBoxWidget(QWidget* parent)
    : QWidget(parent) {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(1, 1, 1, 1);
    mainLayout->setSpacing(3);

    m_buttonGroup = new QButtonGroup(this);
    m_buttonGroup->setExclusive(true);

    initDefaultSlots();

    // =========================================================================
    // 1. 上段: 共通ツールアイコンテーブル ＋ 右横に描画色・背景色ウィジェット
    // =========================================================================
    auto topContainer = new QWidget(this);
    auto topLayout = new QHBoxLayout(topContainer);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(4);

    // 共通ツール用コンテナ (3列×2行)
    m_commonToolsContainer = new QWidget(topContainer);
    m_commonToolsLayout = new QGridLayout(m_commonToolsContainer);
    m_commonToolsLayout->setContentsMargins(0, 0, 0, 0);
    m_commonToolsLayout->setSpacing(1);

    setupCommonTools();
    topLayout->addWidget(m_commonToolsContainer, 1);

    // 描画色・背景色入れ替えウィジェット
    m_colorBox = new ColorBoxWidget(topContainer);
    topLayout->addWidget(m_colorBox, 0, Qt::AlignTop | Qt::AlignRight);

    mainLayout->addWidget(topContainer);

    // =========================================================================
    // 2. 下段: レイヤー別ツールテーブル (ScrolledWindow / QScrollArea に格納)
    // =========================================================================
    m_layerToolsContainer = new QWidget();
    m_layerToolsLayout = new QGridLayout(m_layerToolsContainer);
    m_layerToolsLayout->setContentsMargins(0, 0, 0, 0);
    m_layerToolsLayout->setSpacing(1);

    m_layerToolsScrollArea = new QScrollArea(this);
    m_layerToolsScrollArea->setWidgetResizable(true);
    m_layerToolsScrollArea->setFrameShape(QFrame::NoFrame);
    m_layerToolsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_layerToolsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_layerToolsScrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    m_layerToolsScrollArea->setMinimumHeight(45);
    m_layerToolsScrollArea->setWidget(m_layerToolsContainer);

    m_groupLayerLabel = new QLabel("【グループ選択中】\nキャンバスドラッグで\n配下一括移動が可能です", this);
    m_groupLayerLabel->setAlignment(Qt::AlignCenter);
    m_groupLayerLabel->setStyleSheet("font-size: 11px; color: #555555; padding: 12px; background: #e8e8e8; border: 1px dashed #b0b0b0; border-radius: 4px;");
    m_groupLayerLabel->setVisible(false);

    mainLayout->addWidget(m_layerToolsScrollArea, 1);
    mainLayout->addWidget(m_groupLayerLabel);

    // 初期レイヤー種別 (通常レイヤー) で構築
    updateToolTableForLayer(LayerType::Raster);

    // デフォルトで「鉛筆」を選択
    setCurrentSlot(0);
}

static QString defaultToolName(ToolType type, int slotIndex = -1) {
    if (slotIndex == 0) return QString::fromUtf8("鉛筆");
    if (slotIndex == 1) return QString::fromUtf8("エアブラシ");
    if (slotIndex == 2) return QString::fromUtf8("水彩筆");
    if (slotIndex == 3) return QString::fromUtf8("ブラシ");
    if (slotIndex == 4) return QString::fromUtf8("消しゴム");
    if (slotIndex == 5) return QString::fromUtf8("バケツ");
    if (slotIndex == 6) return QString::fromUtf8("ぼかし");
    if (slotIndex == 7) return QString::fromUtf8("指先");
    if (slotIndex == 8) return QString::fromUtf8("グラデーション");
    if (slotIndex == 9) return QString::fromUtf8("合成ブラシ");
    if (slotIndex == 10) return QString::fromUtf8("パターン");
    if (slotIndex == 11) return QString::fromUtf8("テキスト");
    if (slotIndex == 12) return QString::fromUtf8("コマ枠");
    if (slotIndex == 13) return QString::fromUtf8("コマ分割");
    if (slotIndex == 14) return QString::fromUtf8("3D操作");

    switch (type) {
    case ToolType::Pen: return QString::fromUtf8("鉛筆");
    case ToolType::Airbrush: return QString::fromUtf8("エアブラシ");
    case ToolType::Watercolor: return QString::fromUtf8("水彩筆");
    case ToolType::Marker: return QString::fromUtf8("ブラシ");
    case ToolType::Eraser: return QString::fromUtf8("消しゴム");
    case ToolType::Bucket: return QString::fromUtf8("バケツ");
    case ToolType::PatternFill: return QString::fromUtf8("パターン");
    case ToolType::Blur: return QString::fromUtf8("ぼかし");
    case ToolType::Blend: return QString::fromUtf8("指先");
    case ToolType::Gradient: return QString::fromUtf8("グラデーション");
    case ToolType::CompositeBrush: return QString::fromUtf8("合成ブラシ");
    case ToolType::Text: return QString::fromUtf8("テキスト");
    case ToolType::PanelFrame: return QString::fromUtf8("コマ枠");
    case ToolType::PanelSplit: return QString::fromUtf8("コマ分割");
    case ToolType::ThreeD: return QString::fromUtf8("3D操作");
    case ToolType::RectSelect: return QString::fromUtf8("矩形選択");
    case ToolType::EllipseSelect: return QString::fromUtf8("楕円選択");
    case ToolType::LassoSelect: return QString::fromUtf8("投げ縄");
    case ToolType::MagicWand: return QString::fromUtf8("自動選択");
    case ToolType::ColorSelect: return QString::fromUtf8("色域選択");
    case ToolType::Pipette: return QString::fromUtf8("スポイト");
    case ToolType::Transform: return QString::fromUtf8("変形");
    case ToolType::LayerSelect: return QString::fromUtf8("レイヤー選択");
    default: return QString();
    }
}

static bool isLikelyMojibake(const QString& str) {
    if (str.isEmpty()) return false;
    for (const QChar& c : str) {
        if (c == QChar(0xFFFD) || c == '?') return true;
    }
    // 文字化け特有の文字や部分文字列の検出 (UTF-8をShift-JIS/CP932で解釈した際に出現する文字)
    static const QStringList suspiciousPatterns = {
        QString::fromUtf8("驥"), QString::fromUtf8("繝"), QString::fromUtf8("繧"),
        QString::fromUtf8("遲"), QString::fromUtf8("豌"), QString::fromUtf8("豸"),
        QString::fromUtf8("謖"), QString::fromUtf8("蜷"), QString::fromUtf8("・ｸ"),
        QString::fromUtf8("チE"), QString::fromUtf8("鉛筁"), QString::fromUtf8("水彩筁"),
        QString::fromUtf8("バケチ"), QString::fromUtf8("持E"), QString::fromUtf8("合E")
    };
    for (const auto& pat : suspiciousPatterns) {
        if (str.contains(pat)) return true;
    }
    return false;
}

void ToolBoxWidget::initDefaultSlots() {
    m_rasterSlots.clear();
    m_rasterSlots.resize(TOTAL_SLOTS);

    auto setSlot = [this](int idx, ToolType t, const QString& name) {
        m_rasterSlots[idx].isEmpty = false;
        m_rasterSlots[idx].name = name;
        m_rasterSlots[idx].toolType = t;
        m_rasterSlots[idx].iconType = t;
        m_rasterSlots[idx].shortcut = QKeySequence();
    };

    setSlot(0, ToolType::Pen, QString::fromUtf8("鉛筆"));
    setSlot(1, ToolType::Airbrush, QString::fromUtf8("エアブラシ"));
    setSlot(2, ToolType::Watercolor, QString::fromUtf8("水彩筆"));
    setSlot(3, ToolType::Marker, QString::fromUtf8("ブラシ"));

    setSlot(4, ToolType::Eraser, QString::fromUtf8("消しゴム"));
    setSlot(5, ToolType::Bucket, QString::fromUtf8("バケツ"));
    setSlot(6, ToolType::Blur, QString::fromUtf8("ぼかし"));
    setSlot(7, ToolType::Blend, QString::fromUtf8("指先"));

    setSlot(8, ToolType::Gradient, QString::fromUtf8("グラデーション"));
    setSlot(9, ToolType::CompositeBrush, QString::fromUtf8("合成ブラシ"));
    setSlot(10, ToolType::PatternFill, QString::fromUtf8("パターン"));
    setSlot(11, ToolType::Text, QString::fromUtf8("テキスト"));
    setSlot(12, ToolType::PanelFrame, QString::fromUtf8("コマ枠"));
    setSlot(13, ToolType::PanelSplit, QString::fromUtf8("コマ分割"));
    setSlot(14, ToolType::ThreeD, QString::fromUtf8("3D操作"));

    for (int i = 15; i < TOTAL_SLOTS; ++i) {
        m_rasterSlots[i].isEmpty = true;
        m_rasterSlots[i].name = QString();
        m_rasterSlots[i].toolType = ToolType::Pen;
        m_rasterSlots[i].iconType = ToolType::Pen;
        m_rasterSlots[i].shortcut = QKeySequence();
    }
}

void ToolBoxWidget::setupCommonTools() {
    auto addCommonBtn = [this](ToolType tool, const QString& label, int row, int col) {
        auto btn = new SaiToolButton(tool, label, true, m_commonToolsContainer);
        m_buttonGroup->addButton(btn, static_cast<int>(tool));
        m_commonToolsLayout->addWidget(btn, row, col);

        connect(btn, &QPushButton::clicked, this, [this, tool]() {
            m_currentTool = tool;
            emit toolChanged(tool);
        });
    };

    // 縦2行 (上段: 矩形選択、楕円選択、投げ縄、自動選択、下段: 色域選択、スポイト、変形、レイヤー選択)
    addCommonBtn(ToolType::RectSelect, "矩形選択", 0, 0);
    addCommonBtn(ToolType::EllipseSelect, "楕円選択", 0, 1);
    addCommonBtn(ToolType::LassoSelect, "投げ縄", 0, 2);
    addCommonBtn(ToolType::MagicWand, "自動選択", 0, 3);

    addCommonBtn(ToolType::ColorSelect, "色域選択", 1, 0);
    addCommonBtn(ToolType::Pipette, "スポイト", 1, 1);
    addCommonBtn(ToolType::Transform, "変形", 1, 2);
    addCommonBtn(ToolType::LayerSelect, "レイヤー選択", 1, 3);
}

void ToolBoxWidget::addToolToTable(ToolType tool, const QString& label, int row, int col) {
    auto btn = new SaiToolButton(tool, label, false, m_layerToolsContainer);
    m_buttonGroup->addButton(btn, static_cast<int>(tool));
    m_layerToolsLayout->addWidget(btn, row, col);

    connect(btn, &QPushButton::clicked, this, [this, tool]() {
        m_currentTool = tool;
        emit toolChanged(tool);
    });
}

void ToolBoxWidget::setLayerType(LayerType type) {
    if (m_layerType == type) return;
    m_layerType = type;
    updateToolTableForLayer(type);
}

void ToolBoxWidget::updateToolTableForLayer(LayerType type) {
    // 既存の下段ボタンを破棄（共通ツールボタンは維持）
    m_slotButtons.clear();
    QLayoutItem* item;
    while ((item = m_layerToolsLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            m_buttonGroup->removeButton(qobject_cast<QAbstractButton*>(item->widget()));
            delete item->widget();
        }
        delete item;
    }

    if (m_layerToolsScrollArea) m_layerToolsScrollArea->setVisible(true);
    m_groupLayerLabel->setVisible(false);

    // 3Dレイヤー選択時は共通ツール（矩形選択ツール等）を無効化
    if (m_commonToolsContainer) {
        m_commonToolsContainer->setEnabled(type != LayerType::ThreeD);
    }

    bool currentToolValid = false;

    // 共通ツールが選択中ならそのまま維持 (ただし3Dレイヤー時は共通ツールが無効化されるため除外)
    if (type != LayerType::ThreeD && (
        m_currentTool == ToolType::Pipette || m_currentTool == ToolType::RectSelect ||
        m_currentTool == ToolType::EllipseSelect || m_currentTool == ToolType::LassoSelect ||
        m_currentTool == ToolType::MagicWand || m_currentTool == ToolType::ColorSelect ||
        m_currentTool == ToolType::Transform || m_currentTool == ToolType::LayerSelect)) {
        currentToolValid = true;
    }

    if (type == LayerType::Group) {
        // コマ枠操作ツール（コマ枠、コマ分割）
        addToolToTable(ToolType::PanelFrame, "コマ枠", 0, 0);
        addToolToTable(ToolType::PanelSplit, "コマ分割", 0, 1);

        if (m_currentTool == ToolType::PanelFrame || m_currentTool == ToolType::PanelSplit) {
            currentToolValid = true;
        } else if (!currentToolValid) {
            setCurrentTool(ToolType::PanelFrame);
            emit toolChanged(ToolType::PanelFrame);
        }
        return;
    }

    if (type == LayerType::Raster) {
        // 通常レイヤー: 横4 × 縦16 (計64個) のグリッドツールテーブル
        m_slotButtons.reserve(TOTAL_SLOTS);
        for (int i = 0; i < TOTAL_SLOTS; ++i) {
            int row = i / GRID_COLS;
            int col = i % GRID_COLS;

            auto btn = new SaiToolButton(i, m_rasterSlots[i], m_layerToolsContainer);
            m_buttonGroup->addButton(btn, 1000 + i);
            m_layerToolsLayout->addWidget(btn, row, col);
            m_slotButtons.push_back(btn);

            connect(btn, &QPushButton::clicked, this, [this, i]() {
                if (!m_rasterSlots[i].isEmpty) {
                    m_currentSlotIndex = i;
                    m_currentTool = m_rasterSlots[i].toolType;
                    emit toolChanged(m_currentTool);
                    emit slotSelected(i, m_currentTool);
                }
            });

            connect(btn, &SaiToolButton::settingsRequested, this, &ToolBoxWidget::onSlotSettingsRequested);
            connect(btn, &SaiToolButton::duplicateRequested, this, &ToolBoxWidget::onSlotDuplicateRequested);
            connect(btn, &SaiToolButton::deleteRequested, this, &ToolBoxWidget::onSlotDeleteRequested);
            connect(btn, &SaiToolButton::resetRequested, this, &ToolBoxWidget::resetSlotsToDefault);
        }

        if (m_currentSlotIndex >= 0 && m_currentSlotIndex < TOTAL_SLOTS &&
            !m_rasterSlots[m_currentSlotIndex].isEmpty) {
            currentToolValid = true;
            m_slotButtons[m_currentSlotIndex]->setChecked(true);
        } else if (!currentToolValid) {
            setCurrentSlot(0);
        }
    } else if (type == LayerType::Vector) {
        // ベクトルレイヤー (4列×2行)
        addToolToTable(ToolType::Pen, "ペン", 0, 0);
        addToolToTable(ToolType::Eraser, "消しゴム", 0, 1);
        addToolToTable(ToolType::VectorControlPoint, "制御点", 0, 2);
        addToolToTable(ToolType::VectorPressure, "筆圧", 0, 3);
        addToolToTable(ToolType::VectorLineEdit, "線変更", 1, 0);
        addToolToTable(ToolType::VectorPolyline, "折れ線", 1, 1);
        addToolToTable(ToolType::VectorCurve, "曲線", 1, 2);
        addToolToTable(ToolType::VectorColorChange, "線色変更", 1, 3);
        addToolToTable(ToolType::VectorEllipse, "楕円", 2, 0);
        addToolToTable(ToolType::VectorCircle, "円", 2, 1);
        addToolToTable(ToolType::VectorPolygon, "多角形", 2, 2);

        if (m_currentTool == ToolType::Pen ||
            m_currentTool == ToolType::Eraser ||
            m_currentTool == ToolType::VectorControlPoint ||
            m_currentTool == ToolType::VectorPressure ||
            m_currentTool == ToolType::VectorLineEdit ||
            m_currentTool == ToolType::VectorColorChange ||
            m_currentTool == ToolType::VectorPolyline ||
            m_currentTool == ToolType::VectorCurve ||
            m_currentTool == ToolType::VectorEllipse ||
            m_currentTool == ToolType::VectorCircle ||
            m_currentTool == ToolType::VectorPolygon) {
            currentToolValid = true;
        } else if (!currentToolValid) {
            setCurrentTool(ToolType::Pen);
            emit toolChanged(ToolType::Pen);
        }
    } else if (type == LayerType::Text) {
        // テキストレイヤー (4列×1行)
        addToolToTable(ToolType::Text, "テキスト", 0, 0);
        addToolToTable(ToolType::TextBalloon, "吹き出し", 0, 1);

        if (m_currentTool == ToolType::Text || m_currentTool == ToolType::TextBalloon) {
            currentToolValid = true;
        } else if (!currentToolValid) {
            setCurrentTool(ToolType::Text);
            emit toolChanged(ToolType::Text);
        }
    } else if (type == LayerType::Adjustment) {
        // 調整レイヤー (4列×2行)
        addToolToTable(ToolType::AdjustmentBrightnessContrast, "明度・対比", 0, 0);
        addToolToTable(ToolType::AdjustmentHueSaturation, "色相・彩度", 0, 1);
        addToolToTable(ToolType::AdjustmentToneCurve, "トーンカーブ", 0, 2);
        addToolToTable(ToolType::AdjustmentLevels, "レベル補正", 0, 3);
        addToolToTable(ToolType::AdjustmentInvert, "階調の反転", 1, 0);
        addToolToTable(ToolType::AdjustmentGrayscale, "白黒化", 1, 1);

        if (m_currentTool == ToolType::AdjustmentBrightnessContrast ||
            m_currentTool == ToolType::AdjustmentHueSaturation ||
            m_currentTool == ToolType::AdjustmentToneCurve ||
            m_currentTool == ToolType::AdjustmentLevels ||
            m_currentTool == ToolType::AdjustmentInvert ||
            m_currentTool == ToolType::AdjustmentGrayscale) {
            currentToolValid = true;
        } else if (!currentToolValid) {
            setCurrentTool(ToolType::AdjustmentBrightnessContrast);
            emit toolChanged(ToolType::AdjustmentBrightnessContrast);
        }
    } else if (type == LayerType::ThreeD) {
        // 3Dレイヤー: 矩形選択等のツールは無効化し、3D操作を強制
        addToolToTable(ToolType::ThreeD, QString::fromUtf8("3D操作"), 0, 0);
        setCurrentTool(ToolType::ThreeD);
        emit toolChanged(ToolType::ThreeD);
        currentToolValid = true;
    }

    // 選択状態を再反映 (共通ツールまたは個別ツール)
    if (m_layerType != LayerType::Raster) {
        auto btn = m_buttonGroup->button(static_cast<int>(m_currentTool));
        if (btn) {
            btn->setChecked(true);
        }
    }
}

void ToolBoxWidget::setCurrentTool(ToolType tool) {
    m_currentTool = tool;
    if (m_layerType == LayerType::Raster) {
        // 現在のスロットが該当ツールならそのまま維持
        if (m_currentSlotIndex >= 0 && m_currentSlotIndex < static_cast<int>(m_rasterSlots.size()) &&
            !m_rasterSlots[m_currentSlotIndex].isEmpty && m_rasterSlots[m_currentSlotIndex].toolType == tool) {
            if (m_currentSlotIndex < static_cast<int>(m_slotButtons.size())) {
                m_slotButtons[m_currentSlotIndex]->setChecked(true);
            }
            emit toolChanged(m_currentTool);
            return;
        }
        // 該当ツールを持つ最初のスロットを選択
        for (size_t i = 0; i < m_rasterSlots.size(); ++i) {
            if (!m_rasterSlots[i].isEmpty && m_rasterSlots[i].toolType == tool) {
                m_currentSlotIndex = static_cast<int>(i);
                if (i < m_slotButtons.size()) {
                    m_slotButtons[i]->setChecked(true);
                }
                emit toolChanged(m_currentTool);
                return;
            }
        }
    }

    // 共通ツール等のチェック
    auto btn = m_buttonGroup->button(static_cast<int>(tool));
    if (btn) {
        btn->setChecked(true);
    }
    emit toolChanged(m_currentTool);
}

void ToolBoxWidget::setCurrentSlot(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= static_cast<int>(m_rasterSlots.size())) return;
    if (m_rasterSlots[slotIndex].isEmpty) return;

    m_currentSlotIndex = slotIndex;
    m_currentTool = m_rasterSlots[slotIndex].toolType;
    if (slotIndex < static_cast<int>(m_slotButtons.size())) {
        m_slotButtons[slotIndex]->setChecked(true);
    }
    emit toolChanged(m_currentTool);
    emit slotSelected(slotIndex, m_currentTool);
}

void ToolBoxWidget::setSlotItem(int slotIndex, const ToolSlotItem& item) {
    if (slotIndex < 0 || slotIndex >= static_cast<int>(m_rasterSlots.size())) return;
    m_rasterSlots[slotIndex] = item;
    if (slotIndex < static_cast<int>(m_slotButtons.size())) {
        m_slotButtons[slotIndex]->setSlotItem(item);
    }
}

void ToolBoxWidget::onSlotSettingsRequested(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= static_cast<int>(m_rasterSlots.size())) return;

    ToolSlotItem initItem = m_rasterSlots[slotIndex];
    if (initItem.isEmpty) {
        initItem.name = "鉛筆";
        initItem.toolType = ToolType::Pen;
        initItem.iconType = ToolType::Pen;
        initItem.shortcut = QKeySequence();
    }

    BrushSettingsDialog dlg(initItem, this);
    if (dlg.exec() == QDialog::Accepted) {
        ToolSlotItem newItem = dlg.slotItem();
        newItem.isEmpty = false;
        m_rasterSlots[slotIndex] = newItem;
        if (slotIndex < static_cast<int>(m_slotButtons.size())) {
            m_slotButtons[slotIndex]->setSlotItem(m_rasterSlots[slotIndex]);
        }
        // 設定されたスロットをアクティブにする
        setCurrentSlot(slotIndex);
    }
}

void ToolBoxWidget::onSlotDuplicateRequested(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= static_cast<int>(m_rasterSlots.size())) return;

    // 最初の空きスロットを探す
    int targetSlot = -1;
    for (size_t i = 0; i < m_rasterSlots.size(); ++i) {
        if (m_rasterSlots[i].isEmpty) {
            targetSlot = static_cast<int>(i);
            break;
        }
    }

    if (targetSlot < 0) {
        QMessageBox::information(this, "ブラシを複製", "ツールの登録されていない空きボタンがありません。");
        return;
    }

    ToolSlotItem copyItem = m_rasterSlots[slotIndex];
    m_rasterSlots[targetSlot] = copyItem;
    m_rasterSlots[targetSlot].isEmpty = false;

    if (targetSlot < static_cast<int>(m_slotButtons.size())) {
        m_slotButtons[targetSlot]->setSlotItem(m_rasterSlots[targetSlot]);
    }

    setCurrentSlot(targetSlot);
}

void ToolBoxWidget::onSlotDeleteRequested(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= static_cast<int>(m_rasterSlots.size())) return;

    m_rasterSlots[slotIndex] = ToolSlotItem();
    m_rasterSlots[slotIndex].isEmpty = true;

    if (slotIndex < static_cast<int>(m_slotButtons.size())) {
        m_slotButtons[slotIndex]->setSlotItem(m_rasterSlots[slotIndex]);
    }

    // もし削除されたスロットが現在アクティブだった場合は別の有効スロットへ遷移
    if (m_currentSlotIndex == slotIndex) {
        for (size_t i = 0; i < m_rasterSlots.size(); ++i) {
            if (!m_rasterSlots[i].isEmpty) {
                setCurrentSlot(static_cast<int>(i));
                return;
            }
        }
    }
}

void ToolBoxWidget::resetSlotsToDefault() {
    auto reply = QMessageBox::question(
        this,
        "ツールスロットの初期化",
        "ツールスロットの配置を初期状態に戻しますか？\n（カスタム追加したブラシ設定もリセットされます）",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    if (reply != QMessageBox::Yes) return;

    initDefaultSlots();

    for (size_t i = 0; i < m_slotButtons.size() && i < m_rasterSlots.size(); ++i) {
        m_slotButtons[i]->setSlotItem(m_rasterSlots[i]);
    }

    QSettings settings;
    saveSlots(settings);

    setCurrentSlot(0);
}

bool ToolBoxWidget::handleShortcutKey(const QKeySequence& seq) {
    if (seq.isEmpty()) return false;

    std::vector<int> matchingSlots;
    for (size_t i = 0; i < m_rasterSlots.size(); ++i) {
        if (!m_rasterSlots[i].isEmpty && m_rasterSlots[i].shortcut == seq) {
            matchingSlots.push_back(static_cast<int>(i));
        }
    }

    if (matchingSlots.empty()) return false;

    // 現在選択中のスロットが matchingSlots に含まれているか調べる
    int nextSlot = matchingSlots[0];
    auto it = std::find(matchingSlots.begin(), matchingSlots.end(), m_currentSlotIndex);
    if (it != matchingSlots.end()) {
        size_t nextIdx = (std::distance(matchingSlots.begin(), it) + 1) % matchingSlots.size();
        nextSlot = matchingSlots[nextIdx];
    }

    setCurrentSlot(nextSlot);
    return true;
}

void ToolBoxWidget::saveSlots(QSettings& settings) const {
    settings.beginGroup("RasterToolSlots");
    settings.setValue("count", static_cast<int>(m_rasterSlots.size()));
    for (size_t i = 0; i < m_rasterSlots.size(); ++i) {
        QString prefix = QString("slot_%1_").arg(i);
        const auto& item = m_rasterSlots[i];
        settings.setValue(prefix + "isEmpty", item.isEmpty);
        if (!item.isEmpty) {
            settings.setValue(prefix + "name", item.name);
            settings.setValue(prefix + "toolType", static_cast<int>(item.toolType));
            settings.setValue(prefix + "iconType", static_cast<int>(item.iconType));
            settings.setValue(prefix + "shortcut", item.shortcut.toString());
        }
    }
    settings.endGroup();
}

void ToolBoxWidget::loadSlots(QSettings& settings) {
    settings.beginGroup("RasterToolSlots");
    if (!settings.contains("count")) {
        settings.endGroup();
        return;
    }

    int count = settings.value("count", TOTAL_SLOTS).toInt();
    for (int i = 0; i < TOTAL_SLOTS && i < count; ++i) {
        QString prefix = QString("slot_%1_").arg(i);
        if (!settings.contains(prefix + "isEmpty")) continue;
        bool empty = settings.value(prefix + "isEmpty", true).toBool();
        m_rasterSlots[i].isEmpty = empty;
        if (!empty) {
            QString name = settings.value(prefix + "name").toString();
            auto tType = static_cast<ToolType>(settings.value(prefix + "toolType").toInt());
            auto iType = static_cast<ToolType>(settings.value(prefix + "iconType", static_cast<int>(tType)).toInt());

            if (isLikelyMojibake(name) || name.trimmed().isEmpty()) {
                name = defaultToolName(tType, i);
            }

            m_rasterSlots[i].name = name;
            m_rasterSlots[i].toolType = tType;
            m_rasterSlots[i].iconType = iType;
            QString scStr = settings.value(prefix + "shortcut").toString();
            m_rasterSlots[i].shortcut = scStr.isEmpty() ? QKeySequence() : QKeySequence(scStr);
        }
    }
    settings.endGroup();

    for (size_t i = 0; i < m_slotButtons.size() && i < m_rasterSlots.size(); ++i) {
        m_slotButtons[i]->setSlotItem(m_rasterSlots[i]);
    }
}

} // namespace Kaburagi
