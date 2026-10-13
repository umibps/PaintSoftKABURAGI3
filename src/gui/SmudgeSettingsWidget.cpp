#include "SmudgeSettingsWidget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>

namespace Kaburagi {

SmudgeSettingsWidget::SmudgeSettingsWidget(QWidget* parent)
    : QWidget(parent) {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(6);

    // 1. サイズ (0.5〜500.0)
    m_sizeScale = new SpinScale("サイズ", 0.5, 500.0, 0.5, 2, "", this);
    m_sizeScale->setValue(30.0);
    mainLayout->addWidget(m_sizeScale);

    connect(m_sizeScale, &SpinScale::valueChanged, this, [this](double val) {
        emit smudgeSizeChanged(static_cast<float>(val));
    });

    // 2. 最小サイズ (0〜100%)
    m_minSizeScale = new SpinScale("最小サイズ", 0.0, 100.0, 1.0, 1, "%", this);
    m_minSizeScale->setValue(0.0);
    mainLayout->addWidget(m_minSizeScale);

    connect(m_minSizeScale, &SpinScale::valueChanged, this, [this](double val) {
        emit minSizeChanged(static_cast<float>(val / 100.0));
    });

    // 3. 濃度 (1〜100%)
    m_densityScale = new SpinScale("濃度", 1.0, 100.0, 1.0, 1, "%", this);
    m_densityScale->setValue(100.0);
    mainLayout->addWidget(m_densityScale);

    connect(m_densityScale, &SpinScale::valueChanged, this, [this](double val) {
        emit densityChanged(static_cast<float>(val / 100.0));
    });

    // 4. 割合 (Rate: 1〜100%, GIMP基準)
    m_rateScale = new SpinScale("割合", 1.0, 100.0, 1.0, 0, "%", this);
    m_rateScale->setValue(50.0);
    m_rateScale->setToolTip("指先で色を引きずって伸ばす強さ (低いとすぐ馴染み、高いと長く強く伸びます)");
    mainLayout->addWidget(m_rateScale);

    connect(m_rateScale, &SpinScale::valueChanged, this, [this](double val) {
        emit rateChanged(static_cast<int>(val));
    });

    // 5. 輪郭の硬さ (0〜100)
    m_hardnessScale = new SpinScale("輪郭の硬さ", 0.0, 100.0, 1.0, 0, "", this);
    m_hardnessScale->setValue(0.0);
    mainLayout->addWidget(m_hardnessScale);

    connect(m_hardnessScale, &SpinScale::valueChanged, this, [this](double val) {
        emit hardnessChanged(static_cast<int>(val));
    });

    // 6. ボケ足 (0〜100)
    m_falloffScale = new SpinScale("ボケ足", 0.0, 100.0, 1.0, 0, "", this);
    m_falloffScale->setValue(100.0);
    mainLayout->addWidget(m_falloffScale);

    connect(m_falloffScale, &SpinScale::valueChanged, this, [this](double val) {
        emit falloffChanged(static_cast<int>(val));
    });

    // 7. 最低筆圧 (0〜100)
    m_minPressureScale = new SpinScale("最低筆圧", 0.0, 100.0, 1.0, 0, "", this);
    m_minPressureScale->setValue(0.0);
    mainLayout->addWidget(m_minPressureScale);

    connect(m_minPressureScale, &SpinScale::valueChanged, this, [this](double val) {
        emit minPressureChanged(static_cast<int>(val));
    });

    setMinimumWidth(0);

    // 筆圧影響項目 (サイズ / 濃度)
    m_pressureSizeCheck = new QCheckBox("筆圧サイズ", this);
    m_pressureSizeCheck->setChecked(true);
    m_pressureSizeCheck->setStyleSheet("QCheckBox { font-size: 11px; }");
    m_pressureSizeCheck->setToolTip("ペンタブの筆圧に応じて指先のサイズを変化させる");

    m_pressureDensityCheck = new QCheckBox("筆圧濃度", this);
    m_pressureDensityCheck->setChecked(false);
    m_pressureDensityCheck->setStyleSheet("QCheckBox { font-size: 11px; }");
    m_pressureDensityCheck->setToolTip("ペンタブの筆圧に応じて指先の濃淡・押し込み強度を変化させる");

    mainLayout->addWidget(m_pressureSizeCheck);
    mainLayout->addWidget(m_pressureDensityCheck);

    connect(m_pressureSizeCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_minSizeScale->setEnabled(checked);
        emit pressureSizeChanged(checked);
    });
    connect(m_pressureDensityCheck, &QCheckBox::toggled, this, &SmudgeSettingsWidget::pressureDensityChanged);

    // 5. SAI/GIMP風 ブラシサイズプリセットパレット
    auto presetBox = new QGroupBox("サイズ一覧", this);
    presetBox->setStyleSheet("QGroupBox { font-size: 11px; font-weight: bold; margin-top: 6px; padding-top: 10px; }");
    auto presetGrid = new QGridLayout(presetBox);
    presetGrid->setSpacing(2);
    presetGrid->setContentsMargins(2, 4, 2, 2);

    const std::vector<float> presets = {
        0.7f, 1.0f, 1.5f, 2.0f, 3.0f,
        4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
        10.0f, 12.0f, 14.0f, 16.0f, 20.0f,
        25.0f, 30.0f, 40.0f, 60.0f, 100.0f
    };

    int cols = 5;
    for (size_t i = 0; i < presets.size(); ++i) {
        int r = static_cast<int>(i / cols);
        int c = static_cast<int>(i % cols);
        createSizePresetButton(presets[i], r, c, presetGrid);
    }
    mainLayout->addWidget(presetBox);
    mainLayout->addStretch();
}

void SmudgeSettingsWidget::createSizePresetButton(float size, int row, int col, QGridLayout* grid) {
    QString label = (size == static_cast<int>(size)) ? QString::number(static_cast<int>(size)) : QString::number(size, 'f', 1);
    QPushButton* btn = new QPushButton(label, this);
    btn->setFixedSize(26, 22);
    btn->setStyleSheet(
        "QPushButton { background: #fafafa; border: 1px solid #c0c0c0; font-size: 10px; border-radius: 2px; }"
        "QPushButton:hover { background: #e0e8ff; border-color: #3060b0; }"
        "QPushButton:pressed { background: #c8d8f8; }"
    );
    connect(btn, &QPushButton::clicked, this, [this, size]() {
        setSmudgeSize(size);
    });
    grid->addWidget(btn, row, col);
}

void SmudgeSettingsWidget::setConfig(const SmudgeConfig& cfg) {
    setSmudgeSize(cfg.size);
    setMinSizeRatio(cfg.minSizeRatio);
    setSmudgeDensity(cfg.density);
    setRate(cfg.rate);
    setPressureSizeEnabled(cfg.pressureSize);
    setPressureDensityEnabled(cfg.pressureDensity);
    setHardness(cfg.hardness);
    setFalloff(cfg.falloff);
    setMinPressure(cfg.minPressure);
}

void SmudgeSettingsWidget::setSmudgeSize(float size) {
    if (m_sizeScale) {
        m_sizeScale->setValue(size);
    }
}

void SmudgeSettingsWidget::setMinSizeRatio(float ratio) {
    if (m_minSizeScale) {
        m_minSizeScale->setValue(ratio * 100.0);
    }
}

void SmudgeSettingsWidget::setSmudgeDensity(float density) {
    if (m_densityScale) {
        m_densityScale->setValue(density * 100.0);
    }
}

void SmudgeSettingsWidget::setRate(int rate) {
    if (m_rateScale) {
        m_rateScale->setValue(rate);
    }
}

void SmudgeSettingsWidget::setPressureSizeEnabled(bool enabled) {
    if (m_pressureSizeCheck) {
        m_pressureSizeCheck->setChecked(enabled);
        if (m_minSizeScale) {
            m_minSizeScale->setEnabled(enabled);
        }
    }
}

void SmudgeSettingsWidget::setPressureDensityEnabled(bool enabled) {
    if (m_pressureDensityCheck) {
        m_pressureDensityCheck->setChecked(enabled);
    }
}

void SmudgeSettingsWidget::setHardness(int hardness) {
    if (m_hardnessScale) {
        m_hardnessScale->setValue(hardness);
    }
}

void SmudgeSettingsWidget::setFalloff(int falloff) {
    if (m_falloffScale) {
        m_falloffScale->setValue(falloff);
    }
}

void SmudgeSettingsWidget::setMinPressure(int minPressure) {
    if (m_minPressureScale) {
        m_minPressureScale->setValue(minPressure);
    }
}

} // namespace Kaburagi
