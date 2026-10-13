#include "VectorLineSettingsWidget.hpp"

namespace Kaburagi {

VectorLineSettingsWidget::VectorLineSettingsWidget(QWidget* parent)
    : QWidget(parent) {
    setupUI();
}

void VectorLineSettingsWidget::setupUI() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    auto box = new QGroupBox("線設定", this);
    box->setStyleSheet("QGroupBox { font-size: 11px; font-weight: bold; }");
    auto boxLayout = new QVBoxLayout(box);
    boxLayout->setContentsMargins(6, 6, 6, 6);
    boxLayout->setSpacing(6);

    m_sizeScale = new SpinScale("サイズ", 0.5, 500.0, 0.5, 2, "", box);
    m_sizeScale->setValue(4.0);
    boxLayout->addWidget(m_sizeScale);

    connect(m_sizeScale, &SpinScale::valueChanged, this, [this](double val) {
        emit sizeChanged(val);
    });

    m_densityScale = new SpinScale("濃度", 1.0, 100.0, 1.0, 1, "%", box);
    m_densityScale->setValue(100.0);
    boxLayout->addWidget(m_densityScale);

    connect(m_densityScale, &SpinScale::valueChanged, this, [this](double val) {
        emit densityChanged(val / 100.0);
    });

    m_closePathCheckBox = new QCheckBox("パスを閉じる", box);
    m_closePathCheckBox->setStyleSheet("font-size: 11px;");
    m_closePathCheckBox->setChecked(false);
    boxLayout->addWidget(m_closePathCheckBox);

    connect(m_closePathCheckBox, &QCheckBox::toggled, this, &VectorLineSettingsWidget::closePathChanged);

    mainLayout->addWidget(box);
    mainLayout->addStretch(1);
}

void VectorLineSettingsWidget::setLineSize(double size) {
    if (m_sizeScale && m_sizeScale->value() != size) {
        m_sizeScale->setValue(size);
    }
}

void VectorLineSettingsWidget::setLineDensity(double density) {
    if (m_densityScale) {
        double val = density * 100.0;
        if (m_densityScale->value() != val) {
            m_densityScale->setValue(val);
        }
    }
}

void VectorLineSettingsWidget::setClosePath(bool closed) {
    if (m_closePathCheckBox && m_closePathCheckBox->isChecked() != closed) {
        m_closePathCheckBox->setChecked(closed);
    }
}

} // namespace Kaburagi
