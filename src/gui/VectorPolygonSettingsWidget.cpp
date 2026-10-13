#include "VectorPolygonSettingsWidget.hpp"
#include <algorithm>

namespace Kaburagi {

VectorPolygonSettingsWidget::VectorPolygonSettingsWidget(QWidget* parent)
    : QWidget(parent) {
    setupUI();
}

void VectorPolygonSettingsWidget::setupUI() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    auto group = new QGroupBox("多角形", this);
    auto groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(6, 8, 6, 8);
    groupLayout->setSpacing(6);

    // 頂点数 (3 ~ 24, デフォルト 5)
    m_sidesScale = new SpinScale("頂点数", 3.0, 24.0, 1.0, 0, "", group);
    m_sidesScale->setValue(5.0);
    groupLayout->addWidget(m_sidesScale);

    // 正N角形とする
    m_regularCheckBox = new QCheckBox("正N角形とする", group);
    m_regularCheckBox->setChecked(false);
    groupLayout->addWidget(m_regularCheckBox);

    // サイズ (0.5 ~ 500.0, デフォルト 4.0)
    m_sizeScale = new SpinScale("サイズ", 0.5, 500.0, 0.5, 2, "", group);
    m_sizeScale->setValue(4.0);
    groupLayout->addWidget(m_sizeScale);

    // 濃度 (1 ~ 100%, デフォルト 100%)
    m_densityScale = new SpinScale("濃度", 1.0, 100.0, 1.0, 1, "%", group);
    m_densityScale->setValue(100.0);
    groupLayout->addWidget(m_densityScale);

    // 輪郭の硬さ (0 ~ 100, デフォルト 100)
    m_hardnessScale = new SpinScale("輪郭の硬さ", 0.0, 100.0, 1.0, 0, "", group);
    m_hardnessScale->setValue(100.0);
    groupLayout->addWidget(m_hardnessScale);

    // ボケ足 (0 ~ 100, デフォルト 0)
    m_falloffScale = new SpinScale("ボケ足", 0.0, 100.0, 1.0, 0, "", group);
    m_falloffScale->setValue(0.0);
    groupLayout->addWidget(m_falloffScale);

    // 内部を塗りつぶす
    m_fillCheckBox = new QCheckBox("内部を塗りつぶす", group);
    m_fillCheckBox->setChecked(false);
    groupLayout->addWidget(m_fillCheckBox);

    mainLayout->addWidget(group);
    mainLayout->addStretch();

    // シグナル接続
    connect(m_sidesScale, &SpinScale::valueChanged, this, [this](double val) {
        emit sidesChanged(static_cast<int>(val));
    });
    connect(m_regularCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        emit regularChanged(checked);
    });
    connect(m_sizeScale, &SpinScale::valueChanged, this, [this](double val) {
        emit sizeChanged(val);
    });
    connect(m_densityScale, &SpinScale::valueChanged, this, [this](double val) {
        emit densityChanged(val / 100.0);
    });
    connect(m_hardnessScale, &SpinScale::valueChanged, this, [this](double val) {
        emit hardnessChanged(static_cast<int>(val));
    });
    connect(m_falloffScale, &SpinScale::valueChanged, this, [this](double val) {
        emit falloffChanged(static_cast<int>(val));
    });
    connect(m_fillCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        emit filledChanged(checked);
    });
}

void VectorPolygonSettingsWidget::setSides(int s) {
    int clamped = std::clamp(s, 3, 24);
    if (m_sidesScale && static_cast<int>(m_sidesScale->value()) != clamped) {
        m_sidesScale->setValue(static_cast<double>(clamped));
    }
}

void VectorPolygonSettingsWidget::setRegular(bool reg) {
    if (m_regularCheckBox && m_regularCheckBox->isChecked() != reg) {
        m_regularCheckBox->setChecked(reg);
    }
}

void VectorPolygonSettingsWidget::setLineSize(double size) {
    if (m_sizeScale && m_sizeScale->value() != size) {
        m_sizeScale->setValue(size);
    }
}

void VectorPolygonSettingsWidget::setLineDensity(double density) {
    if (m_densityScale) {
        double val = density * 100.0;
        if (m_densityScale->value() != val) {
            m_densityScale->setValue(val);
        }
    }
}

void VectorPolygonSettingsWidget::setLineHardness(int hardness) {
    if (m_hardnessScale && static_cast<int>(m_hardnessScale->value()) != hardness) {
        m_hardnessScale->setValue(static_cast<double>(hardness));
    }
}

void VectorPolygonSettingsWidget::setLineFalloff(int falloff) {
    if (m_falloffScale && static_cast<int>(m_falloffScale->value()) != falloff) {
        m_falloffScale->setValue(static_cast<double>(falloff));
    }
}

void VectorPolygonSettingsWidget::setFilled(bool fill) {
    if (m_fillCheckBox && m_fillCheckBox->isChecked() != fill) {
        m_fillCheckBox->setChecked(fill);
    }
}

} // namespace Kaburagi
