#include "AdjustmentSettingsWidget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>

namespace Kaburagi {

AdjustmentSettingsWidget::AdjustmentSettingsWidget(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

void AdjustmentSettingsWidget::setupUi() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    m_stack = new QStackedWidget(this);

    // =========================================================================
    // 1. 明るさ・コントラスト ページ (index 0)
    // =========================================================================
    m_brightContrastPage = new QWidget(this);
    auto bcLayout = new QVBoxLayout(m_brightContrastPage);
    bcLayout->setContentsMargins(0, 0, 0, 0);
    bcLayout->setSpacing(6);

    auto bcGroup = new QGroupBox("明るさ・コントラスト", m_brightContrastPage);
    auto bcGroupLayout = new QVBoxLayout(bcGroup);
    bcGroupLayout->setContentsMargins(4, 6, 4, 4);
    bcGroupLayout->setSpacing(4);

    m_brightnessSpin = new SpinScale("明るさ", -100.0, 100.0, 1.0, 0, "%", bcGroup);
    m_brightnessSpin->setValue(0.0);
    m_contrastSpin = new SpinScale("コントラスト", -100.0, 100.0, 1.0, 0, "%", bcGroup);
    m_contrastSpin->setValue(0.0);

    bcGroupLayout->addWidget(m_brightnessSpin);
    bcGroupLayout->addWidget(m_contrastSpin);
    bcLayout->addWidget(bcGroup);
    bcLayout->addStretch(1);
    m_stack->addWidget(m_brightContrastPage);

    connect(m_brightnessSpin, &SpinScale::valueChanged, this, [this](double val) {
        if (m_updatingUi || !m_layer) return;
        m_layer->setBrightnessContrast(static_cast<float>(val / 100.0), static_cast<float>(m_contrastSpin->value() / 100.0));
        emit parametersChanged();
    });
    connect(m_contrastSpin, &SpinScale::valueChanged, this, [this](double val) {
        if (m_updatingUi || !m_layer) return;
        m_layer->setBrightnessContrast(static_cast<float>(m_brightnessSpin->value() / 100.0), static_cast<float>(val / 100.0));
        emit parametersChanged();
    });

    // =========================================================================
    // 2. 色相・彩度 ページ (index 1)
    // =========================================================================
    m_hueSatPage = new QWidget(this);
    auto hsLayout = new QVBoxLayout(m_hueSatPage);
    hsLayout->setContentsMargins(0, 0, 0, 0);
    hsLayout->setSpacing(6);

    auto hsGroup = new QGroupBox("色相・彩度・明度", m_hueSatPage);
    auto hsGroupLayout = new QVBoxLayout(hsGroup);
    hsGroupLayout->setContentsMargins(4, 6, 4, 4);
    hsGroupLayout->setSpacing(4);

    m_hueSpin = new SpinScale("色相", -180.0, 180.0, 1.0, 0, "°", hsGroup);
    m_hueSpin->setValue(0.0);
    m_saturationSpin = new SpinScale("彩度", 0.0, 200.0, 1.0, 0, "%", hsGroup);
    m_saturationSpin->setValue(100.0);
    m_lightnessSpin = new SpinScale("明度", -100.0, 100.0, 1.0, 0, "%", hsGroup);
    m_lightnessSpin->setValue(0.0);

    hsGroupLayout->addWidget(m_hueSpin);
    hsGroupLayout->addWidget(m_saturationSpin);
    hsGroupLayout->addWidget(m_lightnessSpin);
    hsLayout->addWidget(hsGroup);
    hsLayout->addStretch(1);
    m_stack->addWidget(m_hueSatPage);

    auto updateHueSat = [this]() {
        if (m_updatingUi || !m_layer) return;
        float h = static_cast<float>(m_hueSpin->value());
        float s = static_cast<float>(m_saturationSpin->value() / 100.0);
        float l = static_cast<float>(m_lightnessSpin->value() / 100.0);
        m_layer->setHueSaturation(h, s, l);
        emit parametersChanged();
    };
    connect(m_hueSpin, &SpinScale::valueChanged, this, updateHueSat);
    connect(m_saturationSpin, &SpinScale::valueChanged, this, updateHueSat);
    connect(m_lightnessSpin, &SpinScale::valueChanged, this, updateHueSat);

    // =========================================================================
    // 3. トーンカーブ ページ (index 2)
    // =========================================================================
    m_curvePage = new QWidget(this);
    auto tcLayout = new QVBoxLayout(m_curvePage);
    tcLayout->setContentsMargins(0, 0, 0, 0);
    tcLayout->setSpacing(4);

    auto tcTopLayout = new QHBoxLayout();
    tcTopLayout->setContentsMargins(0, 0, 0, 0);
    tcTopLayout->setSpacing(4);

    m_curveChannelCombo = new QComboBox(m_curvePage);
    m_curveChannelCombo->addItem("RGB (マスター)", static_cast<int>(AdjustmentChannel::RGB));
    m_curveChannelCombo->addItem("赤 (Red)", static_cast<int>(AdjustmentChannel::Red));
    m_curveChannelCombo->addItem("緑 (Green)", static_cast<int>(AdjustmentChannel::Green));
    m_curveChannelCombo->addItem("青 (Blue)", static_cast<int>(AdjustmentChannel::Blue));
    tcTopLayout->addWidget(new QLabel("チャンネル:", m_curvePage));
    tcTopLayout->addWidget(m_curveChannelCombo, 1);
    tcLayout->addLayout(tcTopLayout);

    auto tcInterpLayout = new QHBoxLayout();
    tcInterpLayout->setContentsMargins(0, 0, 0, 0);
    tcInterpLayout->setSpacing(4);
    m_curveInterpCombo = new QComboBox(m_curvePage);
    m_curveInterpCombo->addItem("スプライン曲線 (滑らか)", static_cast<int>(CurveInterpolationMode::Spline));
    m_curveInterpCombo->addItem("直線 (折れ線)", static_cast<int>(CurveInterpolationMode::Linear));
    tcInterpLayout->addWidget(new QLabel("補間方式:", m_curvePage));
    tcInterpLayout->addWidget(m_curveInterpCombo, 1);
    tcLayout->addLayout(tcInterpLayout);

    m_curveWidget = new ToneCurveWidget(m_curvePage);
    tcLayout->addWidget(m_curveWidget, 1);

    m_btnResetCurve = new QPushButton("カーブをリセット", m_curvePage);
    tcLayout->addWidget(m_btnResetCurve);
    m_stack->addWidget(m_curvePage);

    connect(m_curveChannelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_updatingUi) return;
        m_curveWidget->setCurrentChannel(static_cast<AdjustmentChannel>(idx));
        updateCurveHistogram();
    });
    connect(m_curveInterpCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_updatingUi) return;
        auto mode = static_cast<CurveInterpolationMode>(idx);
        m_curveWidget->setInterpolationMode(mode);
        if (m_layer) {
            m_layer->setCurveInterpolationMode(mode);
            emit parametersChanged();
        }
    });
    connect(m_curveWidget, &ToneCurveWidget::curveChanged, this, [this](AdjustmentChannel ch, const std::vector<QPointF>& pts) {
        if (m_updatingUi || !m_layer) return;
        m_layer->setCurvePoints(ch, pts);
        emit parametersChanged();
    });
    connect(m_btnResetCurve, &QPushButton::clicked, this, [this]() {
        if (!m_layer) return;
        m_curveWidget->resetCurrentCurve();
        m_layer->resetCurve(m_curveWidget->currentChannel());
        emit parametersChanged();
    });

    // =========================================================================
    // 4. レベル補正 ページ (index 3)
    // =========================================================================
    m_levelsPage = new QWidget(this);
    auto levLayout = new QVBoxLayout(m_levelsPage);
    levLayout->setContentsMargins(0, 0, 0, 0);
    levLayout->setSpacing(4);

    auto levTopLayout = new QHBoxLayout();
    levTopLayout->setContentsMargins(0, 0, 0, 0);
    levTopLayout->setSpacing(4);

    m_levelsChannelCombo = new QComboBox(m_levelsPage);
    m_levelsChannelCombo->addItem("RGB (マスター)", static_cast<int>(AdjustmentChannel::RGB));
    m_levelsChannelCombo->addItem("赤 (Red)", static_cast<int>(AdjustmentChannel::Red));
    m_levelsChannelCombo->addItem("緑 (Green)", static_cast<int>(AdjustmentChannel::Green));
    m_levelsChannelCombo->addItem("青 (Blue)", static_cast<int>(AdjustmentChannel::Blue));
    levTopLayout->addWidget(new QLabel("チャンネル:", m_levelsPage));
    levTopLayout->addWidget(m_levelsChannelCombo, 1);
    levLayout->addLayout(levTopLayout);

    m_levelsWidget = new LevelsWidget(m_levelsPage);
    levLayout->addWidget(m_levelsWidget);

    // 入力値スピンボックス
    auto inNumLayout = new QHBoxLayout();
    inNumLayout->setContentsMargins(0, 0, 0, 0);
    inNumLayout->setSpacing(4);
    m_inShadowSpin = new QSpinBox(m_levelsPage);
    m_inShadowSpin->setRange(0, 255);
    m_inGammaSpin = new QDoubleSpinBox(m_levelsPage);
    m_inGammaSpin->setRange(0.10, 9.99);
    m_inGammaSpin->setSingleStep(0.05);
    m_inGammaSpin->setValue(1.00);
    m_inHighlightSpin = new QSpinBox(m_levelsPage);
    m_inHighlightSpin->setRange(0, 255);
    m_inHighlightSpin->setValue(255);

    inNumLayout->addWidget(new QLabel("入力:", m_levelsPage));
    inNumLayout->addWidget(m_inShadowSpin);
    inNumLayout->addWidget(m_inGammaSpin);
    inNumLayout->addWidget(m_inHighlightSpin);
    levLayout->addLayout(inNumLayout);

    // 出力値スピンボックス
    auto outNumLayout = new QHBoxLayout();
    outNumLayout->setContentsMargins(0, 0, 0, 0);
    outNumLayout->setSpacing(4);
    m_outShadowSpin = new QSpinBox(m_levelsPage);
    m_outShadowSpin->setRange(0, 255);
    m_outHighlightSpin = new QSpinBox(m_levelsPage);
    m_outHighlightSpin->setRange(0, 255);
    m_outHighlightSpin->setValue(255);

    outNumLayout->addWidget(new QLabel("出力:", m_levelsPage));
    outNumLayout->addWidget(m_outShadowSpin);
    outNumLayout->addWidget(m_outHighlightSpin);
    levLayout->addLayout(outNumLayout);

    m_btnResetLevels = new QPushButton("レベルをリセット", m_levelsPage);
    levLayout->addWidget(m_btnResetLevels);
    m_stack->addWidget(m_levelsPage);

    connect(m_levelsChannelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_updatingUi) return;
        m_levelsWidget->setCurrentChannel(static_cast<AdjustmentChannel>(idx));
        syncFromLayer();
        updateLevelsHistogram();
    });

    auto onLevelsWidgetChanged = [this](AdjustmentChannel ch, const LevelParams& p) {
        if (m_updatingUi || !m_layer) return;
        m_updatingUi = true;
        m_inShadowSpin->setValue(p.inShadow);
        m_inGammaSpin->setValue(p.inGamma);
        m_inHighlightSpin->setValue(p.inHighlight);
        m_outShadowSpin->setValue(p.outShadow);
        m_outHighlightSpin->setValue(p.outHighlight);
        m_updatingUi = false;

        m_layer->setLevelParams(ch, p);
        emit parametersChanged();
    };
    connect(m_levelsWidget, &LevelsWidget::levelsChanged, this, onLevelsWidgetChanged);

    auto onSpinChanged = [this]() {
        if (m_updatingUi || !m_layer) return;
        LevelParams p;
        p.inShadow = m_inShadowSpin->value();
        p.inGamma = static_cast<float>(m_inGammaSpin->value());
        p.inHighlight = m_inHighlightSpin->value();
        p.outShadow = m_outShadowSpin->value();
        p.outHighlight = m_outHighlightSpin->value();

        auto ch = static_cast<AdjustmentChannel>(m_levelsChannelCombo->currentIndex());
        m_levelsWidget->setLevelParams(ch, p);
        m_layer->setLevelParams(ch, p);
        emit parametersChanged();
    };
    connect(m_inShadowSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, onSpinChanged);
    connect(m_inGammaSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onSpinChanged);
    connect(m_inHighlightSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, onSpinChanged);
    connect(m_outShadowSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, onSpinChanged);
    connect(m_outHighlightSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, onSpinChanged);

    connect(m_btnResetLevels, &QPushButton::clicked, this, [this]() {
        if (!m_layer) return;
        auto ch = static_cast<AdjustmentChannel>(m_levelsChannelCombo->currentIndex());
        m_levelsWidget->resetCurrentLevels();
        m_layer->resetLevels(ch);
        syncFromLayer();
        emit parametersChanged();
    });

    // =========================================================================
    // 5. 階調の反転 ページ (index 4)
    // =========================================================================
    m_invertPage = new QWidget(this);
    auto invLayout = new QVBoxLayout(m_invertPage);
    invLayout->addWidget(new QLabel("下層のRGB階調を反転します (ネガポジ反転)。\n不透明度で反転効果の強度を調整できます。", m_invertPage));
    invLayout->addStretch(1);
    m_stack->addWidget(m_invertPage);

    // =========================================================================
    // 6. 白黒化 ページ (index 5)
    // =========================================================================
    m_grayscalePage = new QWidget(this);
    auto grayLayout = new QVBoxLayout(m_grayscalePage);
    grayLayout->addWidget(new QLabel("下層を輝度加重平均により白黒（モノクロ）化します。\n不透明度でモノクロ化の強度を調整できます。", m_grayscalePage));
    grayLayout->addStretch(1);
    m_stack->addWidget(m_grayscalePage);

    mainLayout->addWidget(m_stack);
}

void AdjustmentSettingsWidget::setAdjustmentLayer(AdjustmentLayer* layer) {
    m_layer = layer;
    if (m_layer) {
        setAdjustmentType(m_layer->adjustmentType());
        syncFromLayer();
    }
}

void AdjustmentSettingsWidget::setAdjustmentType(AdjustmentType type) {
    int idx = static_cast<int>(type);
    if (idx >= 0 && idx < m_stack->count()) {
        m_stack->setCurrentIndex(idx);
    }
    if (m_layer && m_layer->adjustmentType() != type) {
        m_layer->setAdjustmentType(type);
        syncFromLayer();
        emit parametersChanged();
    }
}

AdjustmentType AdjustmentSettingsWidget::adjustmentType() const {
    return static_cast<AdjustmentType>(m_stack->currentIndex());
}

void AdjustmentSettingsWidget::syncFromLayer() {
    if (!m_layer) return;
    m_updatingUi = true;

    // 明るさ・コントラスト
    m_brightnessSpin->setValue(m_layer->brightness() * 100.0);
    m_contrastSpin->setValue(m_layer->contrast() * 100.0);

    // 色相・彩度
    m_hueSpin->setValue(m_layer->hueShift());
    m_saturationSpin->setValue(m_layer->saturation() * 100.0);
    m_lightnessSpin->setValue(m_layer->lightness() * 100.0);

    // トーンカーブ
    for (int i = 0; i < 4; ++i) {
        auto ch = static_cast<AdjustmentChannel>(i);
        m_curveWidget->setCurvePoints(ch, m_layer->curvePoints(ch));
    }
    m_curveWidget->setInterpolationMode(m_layer->curveInterpolationMode());
    m_curveInterpCombo->setCurrentIndex(static_cast<int>(m_layer->curveInterpolationMode()));

    // レベル補正
    for (int i = 0; i < 4; ++i) {
        auto ch = static_cast<AdjustmentChannel>(i);
        m_levelsWidget->setLevelParams(ch, m_layer->levelParams(ch));
    }
    auto ch = static_cast<AdjustmentChannel>(m_levelsChannelCombo->currentIndex());
    const auto& lp = m_layer->levelParams(ch);
    m_inShadowSpin->setValue(lp.inShadow);
    m_inGammaSpin->setValue(lp.inGamma);
    m_inHighlightSpin->setValue(lp.inHighlight);
    m_outShadowSpin->setValue(lp.outShadow);
    m_outHighlightSpin->setValue(lp.outHighlight);

    m_updatingUi = false;
}

void AdjustmentSettingsWidget::updateCurveHistogram() {
    auto curveCh = m_curveWidget->currentChannel();
    if (curveCh == AdjustmentChannel::Red) m_curveWidget->setHistogram(m_histR);
    else if (curveCh == AdjustmentChannel::Green) m_curveWidget->setHistogram(m_histG);
    else if (curveCh == AdjustmentChannel::Blue) m_curveWidget->setHistogram(m_histB);
    else m_curveWidget->setHistogram(m_histLum);
}

void AdjustmentSettingsWidget::updateLevelsHistogram() {
    auto levCh = m_levelsWidget->currentChannel();
    if (levCh == AdjustmentChannel::Red) m_levelsWidget->setHistogram(m_histR);
    else if (levCh == AdjustmentChannel::Green) m_levelsWidget->setHistogram(m_histG);
    else if (levCh == AdjustmentChannel::Blue) m_levelsWidget->setHistogram(m_histB);
    else m_levelsWidget->setHistogram(m_histLum);
}

void AdjustmentSettingsWidget::updateHistogramFromSource(const TileManager& source, const QRect& bounds) {
    AdjustmentLayer::computeHistogram(source, bounds, m_histR, m_histG, m_histB, m_histLum);
    updateCurveHistogram();
    updateLevelsHistogram();
}

} // namespace Kaburagi
