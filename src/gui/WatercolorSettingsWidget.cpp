#include "WatercolorSettingsWidget.hpp"
#include "ScatterSettingsWidget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>

namespace Kaburagi {

WatercolorSettingsWidget::WatercolorSettingsWidget(QWidget* parent)
    : QWidget(parent) {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(5);

    // 0. 合成モード
    auto modeLayout = new QHBoxLayout();
    auto modeLabel = new QLabel("合成モード:", this);
    modeLabel->setStyleSheet("font-size: 11px;");
    m_blendCombo = new QComboBox(this);
    m_blendCombo->setStyleSheet("font-size: 11px;");
    for (const auto& info : getAllBlendModes()) {
        if (info.mode == BlendMode::PassThrough) continue;
        m_blendCombo->addItem(QString("%1 (%2)").arg(info.nameJa, info.nameEn), static_cast<int>(info.mode));
    }
    modeLayout->addWidget(modeLabel);
    modeLayout->addWidget(m_blendCombo, 1);
    mainLayout->addLayout(modeLayout);

    connect(m_blendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx >= 0) {
            emit blendModeChanged(blendMode());
        }
    });

    // 1. サイズ
    m_sizeScale = new SpinScale("サイズ", 0.5, 500.0, 0.5, 2, "", this);
    m_sizeScale->setValue(20.0);
    mainLayout->addWidget(m_sizeScale);

    connect(m_sizeScale, &SpinScale::valueChanged, this, [this](double val) {
        float fVal = static_cast<float>(val);
        emit watercolorSizeChanged(fVal);
        emit sizeChanged(fVal);
    });

    // 2. 最小サイズ (0〜100%)
    m_minSizeScale = new SpinScale("最小サイズ", 0.0, 100.0, 1.0, 1, "%", this);
    m_minSizeScale->setValue(20.0);
    mainLayout->addWidget(m_minSizeScale);

    connect(m_minSizeScale, &SpinScale::valueChanged, this, [this](double val) {
        emit minSizeChanged(static_cast<float>(val / 100.0));
    });

    // 3. 濃度 (1〜100%)
    m_densityScale = new SpinScale("濃度", 1.0, 100.0, 1.0, 1, "%", this);
    m_densityScale->setValue(70.0);
    mainLayout->addWidget(m_densityScale);

    connect(m_densityScale, &SpinScale::valueChanged, this, [this](double val) {
        emit densityChanged(static_cast<float>(val / 100.0));
    });

    // 4. 輪郭の硬さ (0〜100)
    m_hardnessScale = new SpinScale("輪郭の硬さ", 0.0, 100.0, 1.0, 0, "", this);
    m_hardnessScale->setValue(50.0);
    mainLayout->addWidget(m_hardnessScale);

    connect(m_hardnessScale, &SpinScale::valueChanged, this, [this](double val) {
        emit hardnessChanged(static_cast<int>(val));
    });

    // 5. ボケ足 (0〜100)
    m_falloffScale = new SpinScale("ボケ足", 0.0, 100.0, 1.0, 0, "", this);
    m_falloffScale->setValue(100.0);
    mainLayout->addWidget(m_falloffScale);

    connect(m_falloffScale, &SpinScale::valueChanged, this, [this](double val) {
        emit falloffChanged(static_cast<int>(val));
    });

    // 6. 最低筆圧 (0〜100)
    m_minPressureScale = new SpinScale("最低筆圧", 0.0, 100.0, 1.0, 0, "", this);
    m_minPressureScale->setValue(0.0);
    mainLayout->addWidget(m_minPressureScale);

    connect(m_minPressureScale, &SpinScale::valueChanged, this, [this](double val) {
        emit minPressureChanged(static_cast<int>(val));
    });

    // 7. 筆強度 (0〜100: 描画色と既にレイヤーに存在する色の比率)
    m_strengthScale = new SpinScale("筆強度", 0.0, 100.0, 1.0, 0, "", this);
    m_strengthScale->setValue(50.0);
    mainLayout->addWidget(m_strengthScale);

    connect(m_strengthScale, &SpinScale::valueChanged, this, [this](double val) {
        emit strengthChanged(static_cast<int>(val));
    });

    // 8. 水分量 (0〜100: 不透明度がどれ程上がるか。0が最も上がりやすく、100は上がらない)
    m_waterScale = new SpinScale("水分量", 0.0, 100.0, 1.0, 0, "", this);
    m_waterScale->setValue(50.0);
    mainLayout->addWidget(m_waterScale);

    connect(m_waterScale, &SpinScale::valueChanged, this, [this](double val) {
        emit waterChanged(static_cast<int>(val));
    });

    // 9. 色延び (0〜100: どれほどブラシ移動前の色を反映するか)
    m_colorStretchScale = new SpinScale("色延び", 0.0, 100.0, 1.0, 0, "", this);
    m_colorStretchScale->setValue(50.0);
    mainLayout->addWidget(m_colorStretchScale);

    connect(m_colorStretchScale, &SpinScale::valueChanged, this, [this](double val) {
        emit colorStretchChanged(static_cast<int>(val));
    });

    // 10. ぼかし筆圧 (0〜100: 設定筆圧以下なら筆強度を無視して既存色を適用)
    m_blurPressureScale = new SpinScale("ぼかし筆圧", 0.0, 100.0, 1.0, 0, "", this);
    m_blurPressureScale->setValue(0.0);
    mainLayout->addWidget(m_blurPressureScale);

    connect(m_blurPressureScale, &SpinScale::valueChanged, this, [this](double val) {
        emit blurPressureChanged(static_cast<int>(val));
    });

    // 筆圧影響項目 (サイズ / 濃度)
    auto pressLayout = new QHBoxLayout();
    m_pressureSizeCheck = new QCheckBox("筆圧サイズ", this);
    m_pressureSizeCheck->setChecked(true);
    m_pressureSizeCheck->setStyleSheet("QCheckBox { font-size: 11px; }");
    m_pressureSizeCheck->setToolTip("ペンタブの筆圧に応じて線の太さを変化させる");

    m_pressureDensityCheck = new QCheckBox("筆圧濃度", this);
    m_pressureDensityCheck->setChecked(true);
    m_pressureDensityCheck->setStyleSheet("QCheckBox { font-size: 11px; }");
    m_pressureDensityCheck->setToolTip("ペンタブの筆圧に応じてインクの濃淡を変化させる");

    pressLayout->addWidget(m_pressureSizeCheck);
    pressLayout->addWidget(m_pressureDensityCheck);
    mainLayout->addLayout(pressLayout);

    connect(m_pressureSizeCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_minSizeScale->setEnabled(checked);
        emit pressureSizeChanged(checked);
    });
    connect(m_pressureDensityCheck, &QCheckBox::toggled, this, &WatercolorSettingsWidget::pressureDensityChanged);

    // アンチエイリアス
    m_antialiasCheck = new QCheckBox("アンチエイリアス", this);
    m_antialiasCheck->setChecked(true);
    m_antialiasCheck->setStyleSheet("QCheckBox { font-size: 11px; margin-top: 2px; }");
    mainLayout->addWidget(m_antialiasCheck);

    connect(m_antialiasCheck, &QCheckBox::toggled, this, &WatercolorSettingsWidget::antialiasChanged);

    // 散布設定
    m_scatterWidget = new ScatterSettingsWidget(this);
    mainLayout->addWidget(m_scatterWidget);

    connect(m_scatterWidget, &ScatterSettingsWidget::scatterChanged, this, &WatercolorSettingsWidget::scatterChanged);
    connect(m_scatterWidget, &ScatterSettingsWidget::scatterAmountChanged, this, &WatercolorSettingsWidget::scatterAmountChanged);
    connect(m_scatterWidget, &ScatterSettingsWidget::scatterRangeChanged, this, &WatercolorSettingsWidget::scatterRangeChanged);
    connect(m_scatterWidget, &ScatterSettingsWidget::scatterSizeRandomChanged, this, &WatercolorSettingsWidget::scatterSizeRandomChanged);
    connect(m_scatterWidget, &ScatterSettingsWidget::scatterScaleUpChanged, this, &WatercolorSettingsWidget::scatterScaleUpChanged);

    // サイズプリセットパレット
    auto presetBox = new QGroupBox("サイズ一覧", this);
    presetBox->setStyleSheet("QGroupBox { font-size: 11px; font-weight: bold; margin-top: 6px; padding-top: 10px; }");
    auto presetGrid = new QGridLayout(presetBox);
    presetGrid->setSpacing(2);
    presetGrid->setContentsMargins(2, 4, 2, 2);

    const std::vector<float> presets = {
        1.0f, 2.0f, 3.0f, 4.0f, 5.0f,
        6.0f, 8.0f, 10.0f, 12.0f, 14.0f,
        16.0f, 20.0f, 25.0f, 30.0f, 40.0f,
        50.0f, 60.0f, 80.0f, 100.0f, 150.0f
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

void WatercolorSettingsWidget::createSizePresetButton(float size, int row, int col, QGridLayout* grid) {
    QString label = (size == static_cast<int>(size)) ? QString::number(static_cast<int>(size)) : QString::number(size, 'f', 1);
    QPushButton* btn = new QPushButton(label, this);
    btn->setFixedSize(26, 22);
    btn->setStyleSheet(
        "QPushButton { background: #fafafa; border: 1px solid #c0c0c0; font-size: 10px; border-radius: 2px; }"
        "QPushButton:hover { background: #e0e8ff; border-color: #3060b0; }"
    );
    grid->addWidget(btn, row, col);

    connect(btn, &QPushButton::clicked, this, [this, size]() {
        m_sizeScale->setValue(size);
    });
}

void WatercolorSettingsWidget::setConfig(const WatercolorConfig& cfg) {
    setBlendMode(cfg.blendMode);
    if (m_sizeScale) m_sizeScale->setValue(cfg.size);
    if (m_minSizeScale) m_minSizeScale->setValue(cfg.minSizeRatio * 100.0);
    if (m_densityScale) m_densityScale->setValue(cfg.density * 100.0);
    if (m_hardnessScale) m_hardnessScale->setValue(cfg.hardness);
    if (m_falloffScale) m_falloffScale->setValue(cfg.falloff);
    if (m_minPressureScale) m_minPressureScale->setValue(cfg.minPressure);
    if (m_strengthScale) m_strengthScale->setValue(cfg.strength);
    if (m_waterScale) m_waterScale->setValue(cfg.water);
    if (m_colorStretchScale) m_colorStretchScale->setValue(cfg.colorStretch);
    if (m_blurPressureScale) m_blurPressureScale->setValue(cfg.blurPressure);
    if (m_pressureSizeCheck) {
        m_pressureSizeCheck->setChecked(cfg.pressureSize);
        if (m_minSizeScale) m_minSizeScale->setEnabled(cfg.pressureSize);
    }
    if (m_pressureDensityCheck) m_pressureDensityCheck->setChecked(cfg.pressureDensity);
    if (m_antialiasCheck) m_antialiasCheck->setChecked(cfg.antialias);
    setScatterEnabled(cfg.scatter);
    setScatterAmount(cfg.scatterAmount);
    setScatterRange(cfg.scatterRange);
    setScatterSizeRandom(cfg.scatterSizeRandom);
    setScatterScaleUpEnabled(cfg.scatterScaleUp);
}

BlendMode WatercolorSettingsWidget::blendMode() const {
    if (!m_blendCombo || m_blendCombo->currentIndex() < 0) return BlendMode::Normal;
    return static_cast<BlendMode>(m_blendCombo->currentData().toInt());
}

void WatercolorSettingsWidget::setBlendMode(BlendMode mode) {
    if (!m_blendCombo) return;
    int idx = m_blendCombo->findData(static_cast<int>(mode));
    if (idx >= 0) {
        m_blendCombo->setCurrentIndex(idx);
    }
}

void WatercolorSettingsWidget::setWatercolorSize(float size) {
    if (m_sizeScale) m_sizeScale->setValue(size);
}

void WatercolorSettingsWidget::setMinSizeRatio(float ratio) {
    if (m_minSizeScale) m_minSizeScale->setValue(ratio * 100.0);
}

void WatercolorSettingsWidget::setWatercolorDensity(float density) {
    if (m_densityScale) m_densityScale->setValue(density * 100.0);
}

void WatercolorSettingsWidget::setHardness(int hardness) {
    if (m_hardnessScale) m_hardnessScale->setValue(hardness);
}

void WatercolorSettingsWidget::setFalloff(int falloff) {
    if (m_falloffScale) m_falloffScale->setValue(falloff);
}

void WatercolorSettingsWidget::setMinPressure(int minPressure) {
    if (m_minPressureScale) m_minPressureScale->setValue(minPressure);
}

void WatercolorSettingsWidget::setStrength(int strength) {
    if (m_strengthScale) m_strengthScale->setValue(strength);
}

void WatercolorSettingsWidget::setWater(int water) {
    if (m_waterScale) m_waterScale->setValue(water);
}

void WatercolorSettingsWidget::setColorStretch(int stretch) {
    if (m_colorStretchScale) m_colorStretchScale->setValue(stretch);
}

void WatercolorSettingsWidget::setBlurPressure(int pressure) {
    if (m_blurPressureScale) m_blurPressureScale->setValue(pressure);
}

void WatercolorSettingsWidget::setPressureSizeEnabled(bool enabled) {
    if (m_pressureSizeCheck) {
        m_pressureSizeCheck->setChecked(enabled);
        if (m_minSizeScale) m_minSizeScale->setEnabled(enabled);
    }
}

void WatercolorSettingsWidget::setPressureDensityEnabled(bool enabled) {
    if (m_pressureDensityCheck) m_pressureDensityCheck->setChecked(enabled);
}

void WatercolorSettingsWidget::setAntialias(bool enabled) {
    if (m_antialiasCheck) m_antialiasCheck->setChecked(enabled);
}

bool WatercolorSettingsWidget::isScatterEnabled() const {
    return m_scatterWidget ? m_scatterWidget->isScatterEnabled() : false;
}

int WatercolorSettingsWidget::scatterAmount() const {
    return m_scatterWidget ? m_scatterWidget->scatterAmount() : 5;
}

int WatercolorSettingsWidget::scatterRange() const {
    return m_scatterWidget ? m_scatterWidget->scatterRange() : 100;
}

int WatercolorSettingsWidget::scatterSizeRandom() const {
    return m_scatterWidget ? m_scatterWidget->scatterSizeRandom() : 0;
}

bool WatercolorSettingsWidget::isScatterScaleUpEnabled() const {
    return m_scatterWidget ? m_scatterWidget->isScatterScaleUpEnabled() : false;
}

void WatercolorSettingsWidget::setScatterEnabled(bool enabled) {
    if (m_scatterWidget) m_scatterWidget->setScatterEnabled(enabled);
}

void WatercolorSettingsWidget::setScatterAmount(int amount) {
    if (m_scatterWidget) m_scatterWidget->setScatterAmount(amount);
}

void WatercolorSettingsWidget::setScatterRange(int range) {
    if (m_scatterWidget) m_scatterWidget->setScatterRange(range);
}

void WatercolorSettingsWidget::setScatterSizeRandom(int random) {
    if (m_scatterWidget) m_scatterWidget->setScatterSizeRandom(random);
}

void WatercolorSettingsWidget::setScatterScaleUpEnabled(bool enabled) {
    if (m_scatterWidget) m_scatterWidget->setScatterScaleUpEnabled(enabled);
}

} // namespace Kaburagi
