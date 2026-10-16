#include "PanelSettingsWidget.hpp"
#include "GroupLayer.hpp"
#include "ToolBoxWidget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QColorDialog>

namespace Kaburagi {

PanelSettingsWidget::PanelSettingsWidget(QWidget* parent)
    : QWidget(parent) {
    setupUI();
    setToolType(ToolType::PanelFrame);
}

void PanelSettingsWidget::setupUI() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // コマ枠線グループ（コマ枠作成ツール用）
    m_borderGroup = new QGroupBox("コマ枠線", this);
    auto borderLayout = new QFormLayout(m_borderGroup);
    borderLayout->setContentsMargins(6, 6, 6, 6);
    borderLayout->setSpacing(6);

    m_drawBorderCheck = new QCheckBox("枠線を描画する", m_borderGroup);
    m_drawBorderCheck->setChecked(true);
    borderLayout->addRow(m_drawBorderCheck);

    m_borderWidthSpin = new QDoubleSpinBox(m_borderGroup);
    m_borderWidthSpin->setRange(0.5, 50.0);
    m_borderWidthSpin->setValue(3.0);
    m_borderWidthSpin->setSingleStep(0.5);
    m_borderWidthSpin->setSuffix(" px");
    borderLayout->addRow("線の太さ:", m_borderWidthSpin);

    m_borderColorBtn = new QPushButton(m_borderGroup);
    m_borderColorBtn->setFixedHeight(24);
    updateColorButtonStyle();
    borderLayout->addRow("線の色:", m_borderColorBtn);

    mainLayout->addWidget(m_borderGroup);

    // コマ間隔グループ（コマ分割ツール用）
    m_marginGroup = new QGroupBox("コマの間隔", this);
    auto marginLayout = new QFormLayout(m_marginGroup);
    marginLayout->setContentsMargins(6, 6, 6, 6);
    marginLayout->setSpacing(6);

    m_marginHSpin = new QDoubleSpinBox(m_marginGroup);
    m_marginHSpin->setRange(0.0, 200.0);
    m_marginHSpin->setValue(20.0);
    m_marginHSpin->setSingleStep(1.0);
    m_marginHSpin->setSuffix(" px");
    marginLayout->addRow("左右の間隔:", m_marginHSpin);

    m_marginVSpin = new QDoubleSpinBox(m_marginGroup);
    m_marginVSpin->setRange(0.0, 200.0);
    m_marginVSpin->setValue(40.0);
    m_marginVSpin->setSingleStep(1.0);
    m_marginVSpin->setSuffix(" px");
    marginLayout->addRow("上下の間隔:", m_marginVSpin);

    mainLayout->addWidget(m_marginGroup);

    // タチキリスナップ（共通設定）
    m_snapToBleedCheck = new QCheckBox("キャンバス端にスナップ (タチキリ)", this);
    m_snapToBleedCheck->setChecked(true);
    mainLayout->addWidget(m_snapToBleedCheck);

    mainLayout->addStretch();

    // シグナル接続
    connect(m_drawBorderCheck, &QCheckBox::toggled, this, &PanelSettingsWidget::settingsChanged);
    connect(m_borderWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PanelSettingsWidget::settingsChanged);
    connect(m_borderColorBtn, &QPushButton::clicked, this, &PanelSettingsWidget::chooseBorderColor);
    connect(m_marginHSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PanelSettingsWidget::settingsChanged);
    connect(m_marginVSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PanelSettingsWidget::settingsChanged);
    connect(m_snapToBleedCheck, &QCheckBox::toggled, this, &PanelSettingsWidget::settingsChanged);
}

void PanelSettingsWidget::setToolType(ToolType tool) {
    if (tool == ToolType::PanelSplit) {
        if (m_borderGroup) m_borderGroup->hide();
        if (m_marginGroup) m_marginGroup->show();
    } else { // PanelFrame
        if (m_borderGroup) m_borderGroup->show();
        if (m_marginGroup) m_marginGroup->hide();
    }
}

bool PanelSettingsWidget::isBorderGroupVisible() const {
    return m_borderGroup && !m_borderGroup->isHidden();
}

bool PanelSettingsWidget::isMarginGroupVisible() const {
    return m_marginGroup && !m_marginGroup->isHidden();
}

void PanelSettingsWidget::chooseBorderColor() {
    QColor c = QColorDialog::getColor(m_borderColor, this, "コマ枠線の色を選択");
    if (c.isValid()) {
        m_borderColor = c;
        updateColorButtonStyle();
        emit settingsChanged();
    }
}

void PanelSettingsWidget::updateColorButtonStyle() {
    if (!m_borderColorBtn) return;
    QString qss = QString("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(m_borderColor.name());
    m_borderColorBtn->setStyleSheet(qss);
}

bool PanelSettingsWidget::drawBorder() const {
    return m_drawBorderCheck ? m_drawBorderCheck->isChecked() : true;
}

float PanelSettingsWidget::borderWidth() const {
    return m_borderWidthSpin ? static_cast<float>(m_borderWidthSpin->value()) : 3.0f;
}

QColor PanelSettingsWidget::borderColor() const {
    return m_borderColor;
}

float PanelSettingsWidget::marginHorizontal() const {
    return m_marginHSpin ? static_cast<float>(m_marginHSpin->value()) : 20.0f;
}

float PanelSettingsWidget::marginVertical() const {
    return m_marginVSpin ? static_cast<float>(m_marginVSpin->value()) : 40.0f;
}

bool PanelSettingsWidget::snapToBleed() const {
    return m_snapToBleedCheck ? m_snapToBleedCheck->isChecked() : true;
}

void PanelSettingsWidget::setFromPanel(const GroupLayer* panel) {
    if (!panel) return;
    blockSignals(true);
    if (m_drawBorderCheck) m_drawBorderCheck->setChecked(panel->drawPanelBorder());
    if (m_borderWidthSpin) m_borderWidthSpin->setValue(panel->panelBorderWidth());
    m_borderColor = panel->panelBorderColor();
    updateColorButtonStyle();
    blockSignals(false);
}

void PanelSettingsWidget::applyToPanel(GroupLayer* panel) const {
    if (!panel) return;
    panel->setDrawPanelBorder(drawBorder());
    panel->setPanelBorderWidth(borderWidth());
    panel->setPanelBorderColor(borderColor());
}

} // namespace Kaburagi
