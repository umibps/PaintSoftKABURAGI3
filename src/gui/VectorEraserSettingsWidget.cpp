#include "VectorEraserSettingsWidget.hpp"
#include <QLabel>

namespace Kaburagi {

VectorEraserSettingsWidget::VectorEraserSettingsWidget(QWidget* parent)
    : QWidget(parent) {
    setupUI();
}

void VectorEraserSettingsWidget::setupUI() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    // 1. 消しゴムサイズ
    m_sizeScale = new SpinScale("サイズ", 0.1, 500.0, 1.0, 1, "px", this);
    m_sizeScale->setValue(10.0);
    connect(m_sizeScale, &SpinScale::valueChanged, this, &VectorEraserSettingsWidget::eraserSizeChanged);
    mainLayout->addWidget(m_sizeScale);

    // 2. ベクター消去モード
    auto box = new QGroupBox("ベクター消去", this);
    box->setStyleSheet("QGroupBox { font-size: 11px; font-weight: bold; }");
    auto boxLayout = new QVBoxLayout(box);
    boxLayout->setContentsMargins(6, 6, 6, 6);
    boxLayout->setSpacing(4);

    m_modeGroup = new QButtonGroup(this);

    m_radioIntersection = new QRadioButton("交点まで消去", box);
    m_radioIntersection->setStyleSheet("font-size: 11px;");
    m_radioIntersection->setToolTip("他の線と交差するポイントまでの余分な線を一括消去します。");

    m_radioWholeLine = new QRadioButton("線全体を消去", box);
    m_radioWholeLine->setStyleSheet("font-size: 11px;");
    m_radioWholeLine->setToolTip("触れたストローク全体を丸ごと消去します。");

    m_radioTouchedArea = new QRadioButton("触れた部分を消去", box);
    m_radioTouchedArea->setStyleSheet("font-size: 11px;");
    m_radioTouchedArea->setToolTip("触れた部分を消去して線を分割します。");

    m_modeGroup->addButton(m_radioIntersection, static_cast<int>(VectorEraserMode::Intersection));
    m_modeGroup->addButton(m_radioWholeLine, static_cast<int>(VectorEraserMode::WholeLine));
    m_modeGroup->addButton(m_radioTouchedArea, static_cast<int>(VectorEraserMode::TouchedArea));

    boxLayout->addWidget(m_radioIntersection);
    boxLayout->addWidget(m_radioWholeLine);
    boxLayout->addWidget(m_radioTouchedArea);

    mainLayout->addWidget(box);
    mainLayout->addStretch(1);

    m_radioIntersection->setChecked(true);

    connect(m_modeGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_mode = static_cast<VectorEraserMode>(id);
        emit eraserModeChanged(m_mode);
    });
}

void VectorEraserSettingsWidget::setEraserMode(VectorEraserMode mode) {
    m_mode = mode;
    if (m_modeGroup) {
        auto btn = m_modeGroup->button(static_cast<int>(mode));
        if (btn) btn->setChecked(true);
    }
}

} // namespace Kaburagi
