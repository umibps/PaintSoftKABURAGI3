#include "SplitPanelDialog.hpp"
#include <QPainterPath>
#include <cmath>
#include <algorithm>

namespace Kaburagi {

SplitPanelPreviewWidget::SplitPanelPreviewWidget(QWidget* parent)
    : QWidget(parent) {
    setFixedSize(180, 180);
    setStyleSheet("background-color: #2b2b2b; border: 1px solid #555; border-radius: 4px;");
}

void SplitPanelPreviewWidget::updatePreview(const QPolygonF& origPoly, Qt::Orientation orientation, int count, float margin) {
    m_origPoly = origPoly;
    m_orientation = orientation;
    m_count = std::max(2, count);
    m_margin = std::max(0.0f, margin);
    update();
}

void SplitPanelPreviewWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 背景塗りつぶし
    painter.fillRect(rect(), QColor(40, 40, 44));

    if (m_origPoly.isEmpty()) return;

    QRectF bbox = m_origPoly.boundingRect();
    if (bbox.width() <= 0 || bbox.height() <= 0) return;

    // プレビュー内にフィットさせるスケール計算
    double marginArea = 14.0;
    double availW = width() - marginArea * 2.0;
    double availH = height() - marginArea * 2.0;

    double scale = std::min(availW / bbox.width(), availH / bbox.height());
    double offsetX = marginArea + (availW - bbox.width() * scale) * 0.5;
    double offsetY = marginArea + (availH - bbox.height() * scale) * 0.5;

    auto mapPt = [&](const QPointF& pt) -> QPointF {
        return QPointF(offsetX + (pt.x() - bbox.left()) * scale,
                       offsetY + (pt.y() - bbox.top()) * scale);
    };

    // 元のコマ枠のシルエット（破線）
    QPolygonF scaledOrig;
    for (const auto& pt : m_origPoly) {
        scaledOrig << mapPt(pt);
    }
    QPen dashPen(QColor(100, 100, 110), 1.0, Qt::DashLine);
    painter.setPen(dashPen);
    painter.setBrush(QColor(60, 60, 65));
    painter.drawPolygon(scaledOrig);

    // 分割後の各コマのプレビュー
    double totalDim = (m_orientation == Qt::Vertical) ? bbox.width() : bbox.height();
    double totalMargin = (m_count - 1) * m_margin;
    double pieceDim = (totalDim > totalMargin) ? (totalDim - totalMargin) / m_count : 1.0;

    QFont font = painter.font();
    font.setPointSize(9);
    font.setBold(true);
    painter.setFont(font);

    for (int i = 0; i < m_count; ++i) {
        QRectF pieceDocRect;
        if (m_orientation == Qt::Vertical) {
            double left = bbox.left() + i * (pieceDim + m_margin);
            pieceDocRect = QRectF(left, bbox.top(), pieceDim, bbox.height());
        } else {
            double top = bbox.top() + i * (pieceDim + m_margin);
            pieceDocRect = QRectF(bbox.left(), top, bbox.width(), pieceDim);
        }

        QRectF pieceScreenRect(mapPt(pieceDocRect.topLeft()), mapPt(pieceDocRect.bottomRight()));

        // コマの塗りつぶし & 枠線
        painter.setPen(QPen(QColor(230, 230, 230), 1.5));
        painter.setBrush(QColor(245, 245, 248));
        painter.drawRect(pieceScreenRect);

        // コマ番号
        painter.setPen(QColor(50, 50, 60));
        painter.drawText(pieceScreenRect, Qt::AlignCenter, QString::number(i + 1));
    }
}

// ----------------------------------------------------------------------------
// SplitPanelDialog
// ----------------------------------------------------------------------------
SplitPanelDialog::SplitPanelDialog(const std::shared_ptr<GroupLayer>& targetPanel,
                                   float defaultMarginH,
                                   float defaultMarginV,
                                   QWidget* parent)
    : QDialog(parent)
    , m_targetPanel(targetPanel)
    , m_defaultMarginH(defaultMarginH)
    , m_defaultMarginV(defaultMarginV)
{
    setWindowTitle(tr("コマ枠の分割"));
    setMinimumWidth(380);
    setupUI();
    onParametersChanged();
}

void SplitPanelDialog::setupUI() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    QString targetName = m_targetPanel ? m_targetPanel->name() : tr("コマ枠");
    auto labelTarget = new QLabel(tr("対象コマ: <b>%1</b>").arg(targetName), this);
    mainLayout->addWidget(labelTarget);

    auto contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(12);

    // 左側: 設定グループ
    auto settingsLayout = new QVBoxLayout();
    settingsLayout->setSpacing(8);

    // 分割方向
    auto dirGroup = new QGroupBox(tr("分割方向"), this);
    auto dirLayout = new QVBoxLayout(dirGroup);
    m_radioVertical = new QRadioButton(tr("縦に分割 (左右)"), dirGroup);
    m_radioHorizontal = new QRadioButton(tr("横に分割 (上下)"), dirGroup);
    m_radioVertical->setChecked(true);
    dirLayout->addWidget(m_radioVertical);
    dirLayout->addWidget(m_radioHorizontal);
    settingsLayout->addWidget(dirGroup);

    // 分割数・間隔
    auto formGroup = new QGroupBox(tr("分割設定"), this);
    auto formLayout = new QFormLayout(formGroup);
    formLayout->setSpacing(8);

    m_countSpin = new QSpinBox(formGroup);
    m_countSpin->setRange(2, 10);
    m_countSpin->setValue(2);
    formLayout->addRow(tr("分割数:"), m_countSpin);

    m_marginSpin = new QDoubleSpinBox(formGroup);
    m_marginSpin->setRange(0.0, 200.0);
    m_marginSpin->setValue(m_defaultMarginH);
    m_marginSpin->setSingleStep(1.0);
    m_marginSpin->setSuffix(" px");
    formLayout->addRow(tr("コマの間隔:"), m_marginSpin);

    settingsLayout->addWidget(formGroup);
    settingsLayout->addStretch();
    contentLayout->addLayout(settingsLayout);

    // 右側: プレビュー
    auto previewLayout = new QVBoxLayout();
    auto previewLabel = new QLabel(tr("プレビュー:"), this);
    previewLayout->addWidget(previewLabel);
    m_previewWidget = new SplitPanelPreviewWidget(this);
    previewLayout->addWidget(m_previewWidget);
    previewLayout->addStretch();
    contentLayout->addLayout(previewLayout);

    mainLayout->addLayout(contentLayout);

    // ボタン
    auto btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    auto okBtn = new QPushButton(tr("分割(&S)"), this);
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(okBtn);

    auto cancelBtn = new QPushButton(tr("キャンセル"), this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    mainLayout->addLayout(btnLayout);

    // シグナル
    connect(m_radioVertical, &QRadioButton::toggled, this, &SplitPanelDialog::onOrientationToggled);
    connect(m_countSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &SplitPanelDialog::onParametersChanged);
    connect(m_marginSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SplitPanelDialog::onParametersChanged);
}

void SplitPanelDialog::onOrientationToggled() {
    if (m_radioVertical->isChecked()) {
        m_marginSpin->setValue(m_defaultMarginH);
    } else {
        m_marginSpin->setValue(m_defaultMarginV);
    }
    onParametersChanged();
}

void SplitPanelDialog::onParametersChanged() {
    if (!m_previewWidget || !m_targetPanel) return;
    m_previewWidget->updatePreview(m_targetPanel->panelPolygon(),
                                   orientation(),
                                   splitCount(),
                                   margin());
}

Qt::Orientation SplitPanelDialog::orientation() const {
    return (m_radioHorizontal && m_radioHorizontal->isChecked()) ? Qt::Horizontal : Qt::Vertical;
}

int SplitPanelDialog::splitCount() const {
    return m_countSpin ? m_countSpin->value() : 2;
}

float SplitPanelDialog::margin() const {
    return m_marginSpin ? static_cast<float>(m_marginSpin->value()) : 20.0f;
}

} // namespace Kaburagi
