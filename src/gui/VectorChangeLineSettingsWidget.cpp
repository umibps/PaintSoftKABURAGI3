#include "VectorChangeLineSettingsWidget.hpp"

namespace Kaburagi {

VectorChangeLineSettingsWidget::VectorChangeLineSettingsWidget(QWidget* parent)
    : QWidget(parent) {
    setupUI();
}

void VectorChangeLineSettingsWidget::setupUI() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    auto box = new QGroupBox("線変更設定", this);
    box->setStyleSheet("QGroupBox { font-size: 11px; font-weight: bold; }");
    auto boxLayout = new QVBoxLayout(box);
    boxLayout->setContentsMargins(6, 6, 6, 6);
    boxLayout->setSpacing(6);

    // 1. サイズ (0.5〜500.0)
    m_sizeScale = new SpinScale("サイズ", 0.5, 500.0, 0.5, 2, "", box);
    m_sizeScale->setValue(4.0);
    boxLayout->addWidget(m_sizeScale);

    connect(m_sizeScale, &SpinScale::valueChanged, this, [this](double val) {
        emit sizeChanged(val);
    });

    // 2. 濃度 (1〜100%)
    m_densityScale = new SpinScale("濃度", 1.0, 100.0, 1.0, 1, "%", box);
    m_densityScale->setValue(100.0);
    boxLayout->addWidget(m_densityScale);

    connect(m_densityScale, &SpinScale::valueChanged, this, [this](double val) {
        emit densityChanged(val / 100.0);
    });

    // 3. 輪郭の硬さ (0〜100)
    m_hardnessScale = new SpinScale("輪郭の硬さ", 0.0, 100.0, 1.0, 0, "", box);
    m_hardnessScale->setValue(100.0);
    boxLayout->addWidget(m_hardnessScale);

    connect(m_hardnessScale, &SpinScale::valueChanged, this, [this](double val) {
        emit hardnessChanged(static_cast<int>(val));
    });

    // 4. ボケ足 (0〜100)
    m_falloffScale = new SpinScale("ボケ足", 0.0, 100.0, 1.0, 0, "", box);
    m_falloffScale->setValue(0.0);
    boxLayout->addWidget(m_falloffScale);

    connect(m_falloffScale, &SpinScale::valueChanged, this, [this](double val) {
        emit falloffChanged(static_cast<int>(val));
    });

    mainLayout->addWidget(box);
    mainLayout->addStretch(1);
}

void VectorChangeLineSettingsWidget::setLineSize(double size) {
    if (m_sizeScale && m_sizeScale->value() != size) {
        m_sizeScale->setValue(size);
    }
}

void VectorChangeLineSettingsWidget::setLineDensity(double density) {
    if (m_densityScale) {
        double val = density * 100.0;
        if (m_densityScale->value() != val) {
            m_densityScale->setValue(val);
        }
    }
}

void VectorChangeLineSettingsWidget::setLineHardness(int hardness) {
    if (m_hardnessScale && static_cast<int>(m_hardnessScale->value()) != hardness) {
        m_hardnessScale->setValue(static_cast<double>(hardness));
    }
}

void VectorChangeLineSettingsWidget::setLineFalloff(int falloff) {
    if (m_falloffScale && static_cast<int>(m_falloffScale->value()) != falloff) {
        m_falloffScale->setValue(static_cast<double>(falloff));
    }
}

} // namespace Kaburagi
