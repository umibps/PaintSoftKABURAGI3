#include "SpinScaleWidget.hpp"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDoubleValidator>
#include <algorithm>
#include <cmath>

namespace Kaburagi {

SpinScale::SpinScale(const QString& label, double min, double max, double step, int decimals, const QString& suffix, QWidget* parent)
    : QWidget(parent), m_label(label), m_min(min), m_max(max), m_step(step), m_decimals(decimals), m_suffix(suffix), m_value(min) {

    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(24);

    // キーボード直接入力用インラインエディタ (通常は非表示)
    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setValidator(new QDoubleValidator(min, max, decimals, this));
    m_lineEdit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_lineEdit->setStyleSheet(
        "QLineEdit { background: #1e1e1e; color: #ffffff; border: 1px solid #3060b0; font-size: 11px; padding-right: 2px; }"
    );
    m_lineEdit->hide();

    connect(m_lineEdit, &QLineEdit::editingFinished, this, &SpinScale::onLineEditFinished);

    // 上下ボタンスピン用タイマー (長押し連打対応)
    m_spinTimer = new QTimer(this);
    connect(m_spinTimer, &QTimer::timeout, this, &SpinScale::onSpinButtonTimeout);
}

void SpinScale::setRange(double min, double max) {
    m_min = min;
    m_max = max;
    setValue(m_value);
    update();
}

void SpinScale::setLabel(const QString& label) {
    m_label = label;
    update();
}

void SpinScale::setDecimals(int decimals) {
    m_decimals = decimals;
    update();
}

void SpinScale::setStep(double step) {
    m_step = step;
}

void SpinScale::setSuffix(const QString& suffix) {
    m_suffix = suffix;
    update();
}

void SpinScale::setValue(double val) {
    double clamped = std::clamp(val, m_min, m_max);
    if (std::abs(clamped - m_value) > 1e-6) {
        m_value = clamped;
        update();
        emit valueChanged(m_value);
    }
}

QRect SpinScale::arrowArea() const {
    return QRect(width() - 16, 1, 15, height() - 2);
}

QRect SpinScale::upArrowRect() const {
    QRect r = arrowArea();
    return QRect(r.left(), r.top(), r.width(), r.height() / 2);
}

QRect SpinScale::downArrowRect() const {
    QRect r = arrowArea();
    int h2 = r.height() / 2;
    return QRect(r.left(), r.top() + h2, r.width(), r.height() - h2);
}

void SpinScale::resizeEvent(QResizeEvent* /*event*/) {
    if (m_lineEdit->isVisible()) {
        QRect r = rect().adjusted(width() * 0.4, 1, -17, -1);
        m_lineEdit->setGeometry(r);
    }
}

void SpinScale::startTextEditing() {
    QRect r = rect().adjusted(width() * 0.35, 1, -17, -1);
    m_lineEdit->setGeometry(r);
    m_lineEdit->setText(QString::number(m_value, 'f', m_decimals));
    m_lineEdit->show();
    m_lineEdit->selectAll();
    m_lineEdit->setFocus();
}

void SpinScale::onLineEditFinished() {
    if (!m_lineEdit->isVisible()) return;
    bool ok = false;
    double v = m_lineEdit->text().toDouble(&ok);
    if (ok) {
        setValue(v);
    }
    m_lineEdit->hide();
    update();
}

void SpinScale::updateValueFromMousePos(int x) {
    int activeW = width() - 16;
    if (activeW <= 0) return;

    double ratio = std::clamp(static_cast<double>(x) / activeW, 0.0, 1.0);
    double newVal = m_min + ratio * (m_max - m_min);

    // ステップ刻みに丸め (必要に応じて)
    if (m_step > 0.0) {
        double steps = std::round((newVal - m_min) / m_step);
        newVal = m_min + steps * m_step;
    }
    setValue(newVal);
}

void SpinScale::onSpinButtonTimeout() {
    if (m_isUpArrow) {
        setValue(m_value + m_step);
    } else {
        setValue(m_value - m_step);
    }
}

void SpinScale::mousePressEvent(QMouseEvent* event) {
    if (m_lineEdit->isVisible()) {
        onLineEditFinished();
    }

    if (event->button() == Qt::LeftButton) {
        if (upArrowRect().contains(event->pos())) {
            m_isArrowPressed = true;
            m_isUpArrow = true;
            setValue(m_value + m_step);
            m_spinTimer->start(120);
            update();
            return;
        } else if (downArrowRect().contains(event->pos())) {
            m_isArrowPressed = true;
            m_isUpArrow = false;
            setValue(m_value - m_step);
            m_spinTimer->start(120);
            update();
            return;
        }

        // スケール領域のドラッグ開始
        m_isDragging = true;
        updateValueFromMousePos(event->pos().x());
    }
}

void SpinScale::mouseMoveEvent(QMouseEvent* event) {
    if (m_isDragging) {
        updateValueFromMousePos(event->pos().x());
    }
}

void SpinScale::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        if (m_isArrowPressed) {
            m_isArrowPressed = false;
            m_spinTimer->stop();
            update();
        }
    }
}

void SpinScale::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && !arrowArea().contains(event->pos())) {
        startTextEditing();
    }
}

void SpinScale::wheelEvent(QWheelEvent* event) {
    double steps = event->angleDelta().y() / 120.0;
    setValue(m_value + steps * m_step);
    event->accept();
}

void SpinScale::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    int w = width();
    int h = height();

    // 1. 背景描画 (暗い背景)
    painter.fillRect(rect(), QColor(42, 42, 42));

    // 2. プログレスバー背景 (明るく見やすい設定位置バー)
    int maxBarW = w - 16;
    int barW = 0;
    if (m_max > m_min && maxBarW > 0) {
        double ratio = (m_value - m_min) / (m_max - m_min);
        barW = static_cast<int>(maxBarW * ratio);
        if (barW > 0) {
            // 明るく視認性の高いハイライトブルー
            QRect barRect(1, 1, barW, h - 2);
            painter.fillRect(barRect, QColor(78, 126, 186));

            // プログレスバー右端のインジケーターライン (現在位置の目印)
            painter.setPen(QColor(165, 205, 255));
            painter.drawLine(barRect.right(), 1, barRect.right(), h - 2);
        }
    }

    // 3. 外枠ボーダー
    painter.setPen(QColor(24, 24, 24));
    painter.drawRect(0, 0, w - 1, h - 1);
    painter.setPen(QColor(60, 60, 60));
    painter.drawRect(1, 1, w - 3, h - 3);

    // 4. 右端スピン矢印ボタン領域
    QRect arrowR = arrowArea();
    painter.fillRect(arrowR, QColor(36, 36, 36));
    painter.setPen(QColor(20, 20, 20));
    painter.drawLine(arrowR.left(), 0, arrowR.left(), h - 1);

    // 上下スピンボタンの矢印 (▲ / ▼)
    painter.setRenderHint(QPainter::Antialiasing, true);
    QRect upR = upArrowRect();
    QRect downR = downArrowRect();

    // 上矢印 ▲
    painter.setPen(Qt::NoPen);
    painter.setBrush((m_isArrowPressed && m_isUpArrow) ? QColor(140, 180, 240) : QColor(190, 190, 190));
    QPolygonF upTri;
    qreal midX = upR.center().x();
    qreal upY = upR.center().y();
    upTri << QPointF(midX, upY - 2.5) << QPointF(midX - 3.5, upY + 2.5) << QPointF(midX + 3.5, upY + 2.5);
    painter.drawPolygon(upTri);

    // 下矢印 ▼
    painter.setBrush((m_isArrowPressed && !m_isUpArrow) ? QColor(140, 180, 240) : QColor(190, 190, 190));
    QPolygonF downTri;
    qreal downY = downR.center().y();
    downTri << QPointF(midX, downY + 2.5) << QPointF(midX - 3.5, downY - 2.5) << QPointF(midX + 3.5, downY - 2.5);
    painter.drawPolygon(downTri);

    // 5. テキスト描画 (ラベル & 数値) - クリッピング反転描画
    if (!m_lineEdit->isVisible()) {
        QFont f = font();
        f.setPixelSize(11);
        f.setBold(true);
        painter.setFont(f);

        QRect labelRect(6, 0, w * 0.45, h);
        QRect valRect(w * 0.45, 0, w - (w * 0.45) - 20, h);
        QString valStr = QString::number(m_value, 'f', m_decimals) + m_suffix;

        // --- パス 1: プログレスバー上（明るい背景）→ 反転した濃色文字 ---
        if (barW > 0) {
            painter.save();
            painter.setClipRect(1, 1, barW, h - 2);

            // 反転文字色 (バーが青なので暗いネイビーブラックで抜群のコントラスト)
            painter.setPen(QColor(10, 20, 35));
            painter.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, m_label);
            painter.drawText(valRect, Qt::AlignRight | Qt::AlignVCenter, valStr);

            painter.restore();
        }

        // --- パス 2: 未到達領域（暗い背景）→ 明るい文字 ---
        if (barW < maxBarW) {
            painter.save();
            painter.setClipRect(1 + barW, 1, maxBarW - barW, h - 2);

            // 明るい文字色 (ラベル: 明るいグレー、数値: 爽やかなライトスカイブルー)
            painter.setPen(QColor(230, 230, 230));
            painter.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, m_label);

            painter.setPen(QColor(120, 195, 255));
            painter.drawText(valRect, Qt::AlignRight | Qt::AlignVCenter, valStr);

            painter.restore();
        }
    }
}

} // namespace Kaburagi
