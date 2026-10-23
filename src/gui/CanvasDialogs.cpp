#include "CanvasDialogs.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <cmath>

namespace Kaburagi {

// ==========================================
// ChangeCanvasSizeDialog
// ==========================================

ChangeCanvasSizeDialog::ChangeCanvasSizeDialog(int currentWidth, int currentHeight, QWidget* parent)
    : QDialog(parent), m_currentWidth(currentWidth), m_currentHeight(currentHeight) {
    setWindowTitle(tr("Change Canvas Size"));
    setModal(true);
    resize(360, 320);

    auto mainLayout = new QVBoxLayout(this);

    // 1. 現在のサイズ表示
    auto curGroup = new QGroupBox(tr("Current Size"), this);
    auto curLayout = new QHBoxLayout(curGroup);
    auto lblCurrent = new QLabel(QString("%1 x %2 px").arg(m_currentWidth).arg(m_currentHeight), curGroup);
    lblCurrent->setStyleSheet("font-weight: bold; font-size: 13px;");
    curLayout->addWidget(lblCurrent);
    mainLayout->addWidget(curGroup);

    // 2. 新しいサイズ設定
    auto newGroup = new QGroupBox(tr("New Size"), this);
    auto formLayout = new QFormLayout(newGroup);

    m_widthSpin = new QSpinBox(newGroup);
    m_widthSpin->setRange(1, 32000);
    m_widthSpin->setValue(m_currentWidth);
    m_widthSpin->setSuffix(" px");
    m_widthSpin->setMinimumWidth(95);
    m_widthSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_heightSpin = new QSpinBox(newGroup);
    m_heightSpin->setRange(1, 32000);
    m_heightSpin->setValue(m_currentHeight);
    m_heightSpin->setSuffix(" px");
    m_heightSpin->setMinimumWidth(95);
    m_heightSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    formLayout->addRow(tr("Width:"), m_widthSpin);
    formLayout->addRow(tr("Height:"), m_heightSpin);
    mainLayout->addWidget(newGroup);

    // 3. 基準位置 (3x3 グリッド)
    auto anchorGroup = new QGroupBox(tr("Anchor (Placement)"), this);
    auto anchorLayout = new QGridLayout(anchorGroup);
    anchorLayout->setAlignment(Qt::AlignCenter);

    m_anchorGroup = new QButtonGroup(this);
    m_anchorGroup->setExclusive(true);

    const QString symbols[3][3] = {
        { "↖", "↑", "↗" },
        { "←", "●", "→" },
        { "↙", "↓", "↘" }
    };
    const QString tips[3][3] = {
        { tr("Top-Left"), tr("Top"), tr("Top-Right") },
        { tr("Left"), tr("Center"), tr("Right") },
        { tr("Bottom-Left"), tr("Bottom"), tr("Bottom-Right") }
    };

    int id = 0;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            auto btn = new QToolButton(anchorGroup);
            btn->setText(symbols[r][c]);
            btn->setToolTip(tips[r][c]);
            btn->setCheckable(true);
            btn->setFixedSize(36, 36);
            m_anchorButtons[r][c] = btn;
            m_anchorGroup->addButton(btn, id++);
            anchorLayout->addWidget(btn, r, c);
        }
    }
    // デフォルトは中央
    m_anchorButtons[1][1]->setChecked(true);
    mainLayout->addWidget(anchorGroup);

    // 4. ダイアログボタン
    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

int ChangeCanvasSizeDialog::newWidth() const {
    return m_widthSpin ? m_widthSpin->value() : m_currentWidth;
}

int ChangeCanvasSizeDialog::newHeight() const {
    return m_heightSpin ? m_heightSpin->value() : m_currentHeight;
}

ChangeCanvasSizeDialog::Anchor ChangeCanvasSizeDialog::anchor() const {
    int checkedId = m_anchorGroup ? m_anchorGroup->checkedId() : 4;
    switch (checkedId) {
        case 0: return Anchor::TopLeft;
        case 1: return Anchor::Top;
        case 2: return Anchor::TopRight;
        case 3: return Anchor::Left;
        case 4: return Anchor::Center;
        case 5: return Anchor::Right;
        case 6: return Anchor::BottomLeft;
        case 7: return Anchor::Bottom;
        case 8: return Anchor::BottomRight;
        default: return Anchor::Center;
    }
}

void ChangeCanvasSizeDialog::getOffsets(int& outOffsetX, int& outOffsetY) const {
    int diffW = newWidth() - m_currentWidth;
    int diffH = newHeight() - m_currentHeight;
    switch (anchor()) {
        case Anchor::TopLeft:     outOffsetX = 0;           outOffsetY = 0;           break;
        case Anchor::Top:         outOffsetX = diffW / 2;   outOffsetY = 0;           break;
        case Anchor::TopRight:    outOffsetX = diffW;       outOffsetY = 0;           break;
        case Anchor::Left:        outOffsetX = 0;           outOffsetY = diffH / 2;   break;
        case Anchor::Center:      outOffsetX = diffW / 2;   outOffsetY = diffH / 2;   break;
        case Anchor::Right:       outOffsetX = diffW;       outOffsetY = diffH / 2;   break;
        case Anchor::BottomLeft:  outOffsetX = 0;           outOffsetY = diffH;       break;
        case Anchor::Bottom:      outOffsetX = diffW / 2;   outOffsetY = diffH;       break;
        case Anchor::BottomRight: outOffsetX = diffW;       outOffsetY = diffH;       break;
    }
}


// ==========================================
// ScaleCanvasDialog
// ==========================================

ScaleCanvasDialog::ScaleCanvasDialog(int currentWidth, int currentHeight, QWidget* parent)
    : QDialog(parent), m_currentWidth(currentWidth), m_currentHeight(currentHeight) {
    setWindowTitle(tr("Scale/Resize Canvas"));
    setModal(true);
    resize(440, 320);

    auto mainLayout = new QVBoxLayout(this);

    // 1. 現在のサイズ表示
    auto curGroup = new QGroupBox(tr("Current Size"), this);
    auto curLayout = new QHBoxLayout(curGroup);
    auto lblCurrent = new QLabel(QString("%1 x %2 px").arg(m_currentWidth).arg(m_currentHeight), curGroup);
    lblCurrent->setStyleSheet("font-weight: bold; font-size: 13px;");
    curLayout->addWidget(lblCurrent);
    mainLayout->addWidget(curGroup);

    // 2. 拡大縮小サイズ設定
    auto scaleGroup = new QGroupBox(tr("New Resolution / Dimensions"), this);
    auto grid = new QGridLayout(scaleGroup);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(8);
    grid->setColumnStretch(0, 0);
    grid->setColumnStretch(1, 0);
    grid->setColumnStretch(2, 1);
    grid->setColumnStretch(3, 0);
    grid->setColumnStretch(4, 1);

    grid->addWidget(new QLabel(tr("Pixels:"), scaleGroup), 0, 0);
    grid->addWidget(new QLabel(tr("Width:"), scaleGroup), 0, 1);
    m_widthSpin = new QSpinBox(scaleGroup);
    m_widthSpin->setRange(1, 32000);
    m_widthSpin->setValue(m_currentWidth);
    m_widthSpin->setSuffix(" px");
    m_widthSpin->setMinimumWidth(95);
    m_widthSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    grid->addWidget(m_widthSpin, 0, 2);

    grid->addWidget(new QLabel(tr("Height:"), scaleGroup), 0, 3);
    m_heightSpin = new QSpinBox(scaleGroup);
    m_heightSpin->setRange(1, 32000);
    m_heightSpin->setValue(m_currentHeight);
    m_heightSpin->setSuffix(" px");
    m_heightSpin->setMinimumWidth(95);
    m_heightSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    grid->addWidget(m_heightSpin, 0, 4);

    grid->addWidget(new QLabel(tr("Percent:"), scaleGroup), 1, 0);
    grid->addWidget(new QLabel(tr("Width:"), scaleGroup), 1, 1);
    m_percentWSpin = new QDoubleSpinBox(scaleGroup);
    m_percentWSpin->setRange(1.0, 1000.0);
    m_percentWSpin->setValue(100.0);
    m_percentWSpin->setSingleStep(5.0);
    m_percentWSpin->setDecimals(1);
    m_percentWSpin->setSuffix(" %");
    m_percentWSpin->setMinimumWidth(95);
    m_percentWSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    grid->addWidget(m_percentWSpin, 1, 2);

    grid->addWidget(new QLabel(tr("Height:"), scaleGroup), 1, 3);
    m_percentHSpin = new QDoubleSpinBox(scaleGroup);
    m_percentHSpin->setRange(1.0, 1000.0);
    m_percentHSpin->setValue(100.0);
    m_percentHSpin->setSingleStep(5.0);
    m_percentHSpin->setDecimals(1);
    m_percentHSpin->setSuffix(" %");
    m_percentHSpin->setMinimumWidth(95);
    m_percentHSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    grid->addWidget(m_percentHSpin, 1, 4);

    m_keepAspectCheck = new QCheckBox(tr("Constrain Proportions (Keep Aspect Ratio)"), scaleGroup);
    m_keepAspectCheck->setChecked(true);
    grid->addWidget(m_keepAspectCheck, 2, 0, 1, 5);

    auto interpLayout = new QHBoxLayout();
    interpLayout->addWidget(new QLabel(tr("Interpolation:"), scaleGroup));
    m_interpolationCombo = new QComboBox(scaleGroup);
    m_interpolationCombo->addItem(tr("Bilinear (Smooth)"), static_cast<int>(Qt::SmoothTransformation));
    m_interpolationCombo->addItem(tr("Nearest Neighbor (Pixel Art)"), static_cast<int>(Qt::FastTransformation));
    interpLayout->addWidget(m_interpolationCombo);
    grid->addLayout(interpLayout, 3, 0, 1, 5);

    mainLayout->addWidget(scaleGroup);

    // 相互連動接続
    connect(m_widthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ScaleCanvasDialog::onPixelWidthChanged);
    connect(m_heightSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ScaleCanvasDialog::onPixelHeightChanged);
    connect(m_percentWSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ScaleCanvasDialog::onPercentWidthChanged);
    connect(m_percentHSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ScaleCanvasDialog::onPercentHeightChanged);

    // 3. ダイアログボタン
    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void ScaleCanvasDialog::onPixelWidthChanged(int w) {
    if (m_updating) return;
    m_updating = true;

    double pw = (static_cast<double>(w) / m_currentWidth) * 100.0;
    m_percentWSpin->setValue(pw);

    if (m_keepAspectCheck->isChecked()) {
        int h = std::max(1, static_cast<int>(std::round(w * (static_cast<double>(m_currentHeight) / m_currentWidth))));
        m_heightSpin->setValue(h);
        m_percentHSpin->setValue(pw);
    }

    m_updating = false;
}

void ScaleCanvasDialog::onPixelHeightChanged(int h) {
    if (m_updating) return;
    m_updating = true;

    double ph = (static_cast<double>(h) / m_currentHeight) * 100.0;
    m_percentHSpin->setValue(ph);

    if (m_keepAspectCheck->isChecked()) {
        int w = std::max(1, static_cast<int>(std::round(h * (static_cast<double>(m_currentWidth) / m_currentHeight))));
        m_widthSpin->setValue(w);
        m_percentWSpin->setValue(ph);
    }

    m_updating = false;
}

void ScaleCanvasDialog::onPercentWidthChanged(double pw) {
    if (m_updating) return;
    m_updating = true;

    int w = std::max(1, static_cast<int>(std::round(m_currentWidth * (pw / 100.0))));
    m_widthSpin->setValue(w);

    if (m_keepAspectCheck->isChecked()) {
        m_percentHSpin->setValue(pw);
        int h = std::max(1, static_cast<int>(std::round(m_currentHeight * (pw / 100.0))));
        m_heightSpin->setValue(h);
    }

    m_updating = false;
}

void ScaleCanvasDialog::onPercentHeightChanged(double ph) {
    if (m_updating) return;
    m_updating = true;

    int h = std::max(1, static_cast<int>(std::round(m_currentHeight * (ph / 100.0))));
    m_heightSpin->setValue(h);

    if (m_keepAspectCheck->isChecked()) {
        m_percentWSpin->setValue(ph);
        int w = std::max(1, static_cast<int>(std::round(m_currentWidth * (ph / 100.0))));
        m_widthSpin->setValue(w);
    }

    m_updating = false;
}

int ScaleCanvasDialog::newWidth() const {
    return m_widthSpin ? m_widthSpin->value() : m_currentWidth;
}

int ScaleCanvasDialog::newHeight() const {
    return m_heightSpin ? m_heightSpin->value() : m_currentHeight;
}

Qt::TransformationMode ScaleCanvasDialog::interpolationMode() const {
    if (!m_interpolationCombo) return Qt::SmoothTransformation;
    return static_cast<Qt::TransformationMode>(m_interpolationCombo->currentData().toInt());
}

} // namespace Kaburagi
