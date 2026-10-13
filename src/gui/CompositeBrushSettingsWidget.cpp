#include "CompositeBrushSettingsWidget.hpp"
#include "ScatterSettingsWidget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>

namespace Kaburagi {

CompositeBrushSettingsWidget::CompositeBrushSettingsWidget(QWidget* parent)
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

    // 0.5 対象 (キャンバス / ひとつ下のレイヤー)
    auto refLayout = new QHBoxLayout();
    auto refLabel = new QLabel("対象:", this);
    refLabel->setStyleSheet("font-size: 11px;");
    refLayout->addWidget(refLabel);

    m_canvasRadio = new QRadioButton("キャンバス", this);
    m_layerBelowRadio = new QRadioButton("ひとつ下のレイヤー", this);
    m_canvasRadio->setStyleSheet("font-size: 11px;");
    m_layerBelowRadio->setStyleSheet("font-size: 11px;");
    m_canvasRadio->setChecked(true);

    m_refGroup = new QButtonGroup(this);
    m_refGroup->addButton(m_canvasRadio, static_cast<int>(CompositeBrushReference::Canvas));
    m_refGroup->addButton(m_layerBelowRadio, static_cast<int>(CompositeBrushReference::LayerBelow));

    refLayout->addWidget(m_canvasRadio);
    refLayout->addWidget(m_layerBelowRadio);
    mainLayout->addLayout(refLayout);

    connect(m_refGroup, &QButtonGroup::idClicked, this, [this](int id) {
        emit referenceModeChanged(static_cast<CompositeBrushReference>(id));
    });

    // 1. サイズ
    m_sizeScale = new SpinScale("サイズ", 0.5, 500.0, 0.5, 2, "", this);
    m_sizeScale->setValue(20.0);
    mainLayout->addWidget(m_sizeScale);

    connect(m_sizeScale, &SpinScale::valueChanged, this, [this](double val) {
        float fVal = static_cast<float>(val);
        emit compositeBrushSizeChanged(fVal);
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

    // 11. チェックボックス群 (筆圧サイズ, 筆圧濃度, アンチエイリアス)
    auto checkLayout = new QVBoxLayout();
    checkLayout->setSpacing(2);

    m_pressureSizeCheck = new QCheckBox("筆圧に対するサイズ変化", this);
    m_pressureSizeCheck->setChecked(true);
    m_pressureSizeCheck->setStyleSheet("font-size: 11px;");
    connect(m_pressureSizeCheck, &QCheckBox::toggled, this, &CompositeBrushSettingsWidget::pressureSizeChanged);
    checkLayout->addWidget(m_pressureSizeCheck);

    m_pressureDensityCheck = new QCheckBox("筆圧に対する濃度変化", this);
    m_pressureDensityCheck->setChecked(true);
    m_pressureDensityCheck->setStyleSheet("font-size: 11px;");
    connect(m_pressureDensityCheck, &QCheckBox::toggled, this, &CompositeBrushSettingsWidget::pressureDensityChanged);
    checkLayout->addWidget(m_pressureDensityCheck);

    m_antialiasCheck = new QCheckBox("アンチエイリアス", this);
    m_antialiasCheck->setChecked(true);
    m_antialiasCheck->setStyleSheet("font-size: 11px;");
    connect(m_antialiasCheck, &QCheckBox::toggled, this, &CompositeBrushSettingsWidget::antialiasChanged);
    checkLayout->addWidget(m_antialiasCheck);

    mainLayout->addLayout(checkLayout);

    // 散布設定
    m_scatterWidget = new ScatterSettingsWidget(this);
    mainLayout->addWidget(m_scatterWidget);

    connect(m_scatterWidget, &ScatterSettingsWidget::scatterChanged, this, &CompositeBrushSettingsWidget::scatterChanged);
    connect(m_scatterWidget, &ScatterSettingsWidget::scatterAmountChanged, this, &CompositeBrushSettingsWidget::scatterAmountChanged);
    connect(m_scatterWidget, &ScatterSettingsWidget::scatterRangeChanged, this, &CompositeBrushSettingsWidget::scatterRangeChanged);
    connect(m_scatterWidget, &ScatterSettingsWidget::scatterSizeRandomChanged, this, &CompositeBrushSettingsWidget::scatterSizeRandomChanged);
    connect(m_scatterWidget, &ScatterSettingsWidget::scatterScaleUpChanged, this, &CompositeBrushSettingsWidget::scatterScaleUpChanged);

    // 12. サイズプリセットグリッド
    auto presetGroup = new QGroupBox("サイズプリセット", this);
    presetGroup->setStyleSheet("font-size: 11px; margin-top: 6px;");
    auto presetGrid = new QGridLayout(presetGroup);
    presetGrid->setContentsMargins(4, 4, 4, 4);
    presetGrid->setSpacing(2);

    const float presets[4][4] = {
        { 3.0f,  5.0f,  7.0f,  10.0f },
        { 14.0f, 20.0f, 30.0f, 40.0f },
        { 50.0f, 60.0f, 80.0f, 100.0f },
        { 150.0f, 200.0f, 300.0f, 500.0f }
    };

    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            createSizePresetButton(presets[r][c], r, c, presetGrid);
        }
    }

    mainLayout->addWidget(presetGroup);
    mainLayout->addStretch();
}

void CompositeBrushSettingsWidget::createSizePresetButton(float size, int row, int col, QGridLayout* grid) {
    QString label = (size < 10.0f) ? QString::number(size, 'f', 1) : QString::number(static_cast<int>(size));
    auto btn = new QPushButton(label, this);
    btn->setFixedHeight(20);
    btn->setStyleSheet("font-size: 10px; padding: 0px;");
    connect(btn, &QPushButton::clicked, this, [this, size]() {
        setCompositeBrushSize(size);
    });
    grid->addWidget(btn, row, col);
}

BlendMode CompositeBrushSettingsWidget::blendMode() const {
    if (!m_blendCombo) return BlendMode::Normal;
    return static_cast<BlendMode>(m_blendCombo->currentData().toInt());
}

void CompositeBrushSettingsWidget::setBlendMode(BlendMode mode) {
    if (!m_blendCombo) return;
    int idx = m_blendCombo->findData(static_cast<int>(mode));
    if (idx >= 0 && idx != m_blendCombo->currentIndex()) {
        m_blendCombo->setCurrentIndex(idx);
    }
}

CompositeBrushReference CompositeBrushSettingsWidget::referenceMode() const {
    if (m_layerBelowRadio && m_layerBelowRadio->isChecked()) {
        return CompositeBrushReference::LayerBelow;
    }
    return CompositeBrushReference::Canvas;
}

void CompositeBrushSettingsWidget::setReferenceMode(CompositeBrushReference ref) {
    if (ref == CompositeBrushReference::LayerBelow) {
        if (m_layerBelowRadio) m_layerBelowRadio->setChecked(true);
    } else {
        if (m_canvasRadio) m_canvasRadio->setChecked(true);
    }
}

void CompositeBrushSettingsWidget::setConfig(const CompositeBrushConfig& cfg) {
    setBlendMode(cfg.blendMode);
    setReferenceMode(cfg.reference);
    setCompositeBrushSize(cfg.size);
    setMinSizeRatio(cfg.minSizeRatio);
    setCompositeBrushDensity(cfg.density);
    setHardness(cfg.hardness);
    setFalloff(cfg.falloff);
    setMinPressure(cfg.minPressure);
    setStrength(cfg.strength);
    setWater(cfg.water);
    setColorStretch(cfg.colorStretch);
    setBlurPressure(cfg.blurPressure);
    setPressureSizeEnabled(cfg.pressureSize);
    setPressureDensityEnabled(cfg.pressureDensity);
    setAntialias(cfg.antialias);
    setScatterEnabled(cfg.scatter);
    setScatterAmount(cfg.scatterAmount);
    setScatterRange(cfg.scatterRange);
    setScatterSizeRandom(cfg.scatterSizeRandom);
    setScatterScaleUpEnabled(cfg.scatterScaleUp);
}

void CompositeBrushSettingsWidget::setCompositeBrushSize(float size) {
    if (m_sizeScale) m_sizeScale->setValue(size);
}

void CompositeBrushSettingsWidget::setMinSizeRatio(float ratio) {
    if (m_minSizeScale) m_minSizeScale->setValue(ratio * 100.0f);
}

void CompositeBrushSettingsWidget::setCompositeBrushDensity(float density) {
    if (m_densityScale) m_densityScale->setValue(density * 100.0f);
}

void CompositeBrushSettingsWidget::setHardness(int hardness) {
    if (m_hardnessScale) m_hardnessScale->setValue(hardness);
}

void CompositeBrushSettingsWidget::setFalloff(int falloff) {
    if (m_falloffScale) m_falloffScale->setValue(falloff);
}

void CompositeBrushSettingsWidget::setMinPressure(int minPressure) {
    if (m_minPressureScale) m_minPressureScale->setValue(minPressure);
}

void CompositeBrushSettingsWidget::setStrength(int strength) {
    if (m_strengthScale) m_strengthScale->setValue(strength);
}

void CompositeBrushSettingsWidget::setWater(int water) {
    if (m_waterScale) m_waterScale->setValue(water);
}

void CompositeBrushSettingsWidget::setColorStretch(int stretch) {
    if (m_colorStretchScale) m_colorStretchScale->setValue(stretch);
}

void CompositeBrushSettingsWidget::setBlurPressure(int pressure) {
    if (m_blurPressureScale) m_blurPressureScale->setValue(pressure);
}

void CompositeBrushSettingsWidget::setPressureSizeEnabled(bool enabled) {
    if (m_pressureSizeCheck) m_pressureSizeCheck->setChecked(enabled);
}

void CompositeBrushSettingsWidget::setPressureDensityEnabled(bool enabled) {
    if (m_pressureDensityCheck) m_pressureDensityCheck->setChecked(enabled);
}

void CompositeBrushSettingsWidget::setAntialias(bool enabled) {
    if (m_antialiasCheck) m_antialiasCheck->setChecked(enabled);
}

bool CompositeBrushSettingsWidget::isScatterEnabled() const {
    return m_scatterWidget ? m_scatterWidget->isScatterEnabled() : false;
}

int CompositeBrushSettingsWidget::scatterAmount() const {
    return m_scatterWidget ? m_scatterWidget->scatterAmount() : 5;
}

int CompositeBrushSettingsWidget::scatterRange() const {
    return m_scatterWidget ? m_scatterWidget->scatterRange() : 100;
}

int CompositeBrushSettingsWidget::scatterSizeRandom() const {
    return m_scatterWidget ? m_scatterWidget->scatterSizeRandom() : 0;
}

bool CompositeBrushSettingsWidget::isScatterScaleUpEnabled() const {
    return m_scatterWidget ? m_scatterWidget->isScatterScaleUpEnabled() : false;
}

void CompositeBrushSettingsWidget::setScatterEnabled(bool enabled) {
    if (m_scatterWidget) m_scatterWidget->setScatterEnabled(enabled);
}

void CompositeBrushSettingsWidget::setScatterAmount(int amount) {
    if (m_scatterWidget) m_scatterWidget->setScatterAmount(amount);
}

void CompositeBrushSettingsWidget::setScatterRange(int range) {
    if (m_scatterWidget) m_scatterWidget->setScatterRange(range);
}

void CompositeBrushSettingsWidget::setScatterSizeRandom(int random) {
    if (m_scatterWidget) m_scatterWidget->setScatterSizeRandom(random);
}

void CompositeBrushSettingsWidget::setScatterScaleUpEnabled(bool enabled) {
    if (m_scatterWidget) m_scatterWidget->setScatterScaleUpEnabled(enabled);
}

} // namespace Kaburagi
