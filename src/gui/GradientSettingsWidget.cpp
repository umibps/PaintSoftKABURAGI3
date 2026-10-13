#include "GradientSettingsWidget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>

namespace Kaburagi {

GradientSettingsWidget::GradientSettingsWidget(QWidget* parent)
    : QWidget(parent) {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(6);

    setMinimumWidth(0);

    // 0. 合成モード
    auto modeLayout = new QHBoxLayout();
    auto modeLabel = new QLabel("合成モード:", this);
    modeLabel->setStyleSheet("font-size: 11px;");
    m_blendCombo = new QComboBox(this);
    m_blendCombo->setStyleSheet("QComboBox { font-size: 11px; }");
    m_blendCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_blendCombo->setMinimumContentsLength(4);
    m_blendCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
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

    // 1. カラーモード (描画色→背景色, 描画色→透明)
    auto colorModeBox = new QGroupBox("グラデーション色", this);
    colorModeBox->setStyleSheet("QGroupBox { font-size: 11px; font-weight: bold; padding-top: 10px; }");
    auto colorModeLayout = new QVBoxLayout(colorModeBox);
    colorModeLayout->setContentsMargins(4, 4, 4, 4);

    m_colorModeCombo = new QComboBox(colorModeBox);
    m_colorModeCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_colorModeCombo->setMinimumContentsLength(4);
    m_colorModeCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_colorModeCombo->addItem("描画色 → 背景色", static_cast<int>(GradientColorMode::FgToBg));
    m_colorModeCombo->addItem("描画色 → 透明", static_cast<int>(GradientColorMode::FgToTransparent));
    m_colorModeCombo->setStyleSheet("QComboBox { font-size: 11px; height: 22px; }");
    colorModeLayout->addWidget(m_colorModeCombo);

    connect(m_colorModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        emit colorModeChanged(colorMode());
    });
    mainLayout->addWidget(colorModeBox);

    // 2. 形状 (線形, 放射状, 双線形, 円錐状)
    auto shapeBox = new QGroupBox("形状 (Shape)", this);
    shapeBox->setStyleSheet("QGroupBox { font-size: 11px; font-weight: bold; padding-top: 10px; }");
    auto shapeLayout = new QVBoxLayout(shapeBox);
    shapeLayout->setContentsMargins(4, 4, 4, 4);

    m_shapeCombo = new QComboBox(shapeBox);
    m_shapeCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_shapeCombo->setMinimumContentsLength(4);
    m_shapeCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_shapeCombo->addItem("線形 (Linear)", static_cast<int>(GradientShape::Linear));
    m_shapeCombo->addItem("放射状 (Radial)", static_cast<int>(GradientShape::Radial));
    m_shapeCombo->addItem("双線形 (Bi-linear)", static_cast<int>(GradientShape::BiLinear));
    m_shapeCombo->addItem("円錐状 (Conical)", static_cast<int>(GradientShape::Conical));
    m_shapeCombo->setStyleSheet("QComboBox { font-size: 11px; height: 22px; }");
    shapeLayout->addWidget(m_shapeCombo);

    connect(m_shapeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        emit shapeChanged(shape());
    });
    mainLayout->addWidget(shapeBox);

    // 3. 不透明度 (1〜100%)
    m_opacityScale = new SpinScale("不透明度", 1.0, 100.0, 1.0, 1, "%", this);
    m_opacityScale->setValue(100.0);
    mainLayout->addWidget(m_opacityScale);

    connect(m_opacityScale, &SpinScale::valueChanged, this, [this](double val) {
        emit opacityChanged(static_cast<float>(val / 100.0));
    });

    // 4. 反転チェックボックス
    m_reverseCheck = new QCheckBox("反転 (Reverse)", this);
    m_reverseCheck->setChecked(false);
    m_reverseCheck->setStyleSheet("QCheckBox { font-size: 11px; margin-top: 2px; }");
    m_reverseCheck->setToolTip("グラデーションの方向・色の向きを反転します");
    mainLayout->addWidget(m_reverseCheck);

    connect(m_reverseCheck, &QCheckBox::toggled, this, &GradientSettingsWidget::reverseChanged);

    // 5. 繰り返し (なし, ノコギリ波, 三角波)
    auto repeatBox = new QGroupBox("繰り返し (Repeat)", this);
    repeatBox->setStyleSheet("QGroupBox { font-size: 11px; font-weight: bold; padding-top: 10px; }");
    auto repeatLayout = new QVBoxLayout(repeatBox);
    repeatLayout->setContentsMargins(4, 4, 4, 4);

    m_repeatCombo = new QComboBox(repeatBox);
    m_repeatCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_repeatCombo->setMinimumContentsLength(4);
    m_repeatCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_repeatCombo->addItem("なし (None)", static_cast<int>(GradientRepeatMode::None));
    m_repeatCombo->addItem("ノコギリ波 (Sawtooth)", static_cast<int>(GradientRepeatMode::Sawtooth));
    m_repeatCombo->addItem("三角波 (Triangular)", static_cast<int>(GradientRepeatMode::Triangular));
    m_repeatCombo->setStyleSheet("QComboBox { font-size: 11px; height: 22px; }");
    repeatLayout->addWidget(m_repeatCombo);

    connect(m_repeatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        emit repeatModeChanged(repeatMode());
    });
    mainLayout->addWidget(repeatBox);

    // 6. ディザリング
    m_ditherCheck = new QCheckBox("ディザリング (Dithering)", this);
    m_ditherCheck->setChecked(true);
    m_ditherCheck->setStyleSheet("QCheckBox { font-size: 11px; margin-top: 2px; }");
    m_ditherCheck->setToolTip("トーンジャンプ（階調の縞模様）を低減します");
    mainLayout->addWidget(m_ditherCheck);

    connect(m_ditherCheck, &QCheckBox::toggled, this, &GradientSettingsWidget::ditherChanged);

    mainLayout->addStretch();
}

void GradientSettingsWidget::setConfig(const GradientConfig& cfg) {
    setBlendMode(cfg.blendMode);
    setShape(cfg.shape);
    setColorMode(cfg.colorMode);
    setRepeatMode(cfg.repeatMode);
    setOpacity(cfg.opacity);
    setReverse(cfg.reverse);
    setDither(cfg.dither);
}

BlendMode GradientSettingsWidget::blendMode() const {
    if (!m_blendCombo || m_blendCombo->currentIndex() < 0) return BlendMode::Normal;
    return static_cast<BlendMode>(m_blendCombo->currentData().toInt());
}

void GradientSettingsWidget::setBlendMode(BlendMode mode) {
    if (!m_blendCombo) return;
    int idx = m_blendCombo->findData(static_cast<int>(mode));
    if (idx >= 0) {
        m_blendCombo->setCurrentIndex(idx);
    }
}

void GradientSettingsWidget::setShape(GradientShape shape) {
    int idx = m_shapeCombo->findData(static_cast<int>(shape));
    if (idx >= 0) m_shapeCombo->setCurrentIndex(idx);
}

void GradientSettingsWidget::setColorMode(GradientColorMode mode) {
    int idx = m_colorModeCombo->findData(static_cast<int>(mode));
    if (idx >= 0) m_colorModeCombo->setCurrentIndex(idx);
}

void GradientSettingsWidget::setRepeatMode(GradientRepeatMode mode) {
    int idx = m_repeatCombo->findData(static_cast<int>(mode));
    if (idx >= 0) m_repeatCombo->setCurrentIndex(idx);
}

void GradientSettingsWidget::setOpacity(float opacity) {
    if (m_opacityScale) {
        m_opacityScale->setValue(opacity * 100.0);
    }
}

void GradientSettingsWidget::setReverse(bool reverse) {
    if (m_reverseCheck) {
        m_reverseCheck->setChecked(reverse);
    }
}

void GradientSettingsWidget::setDither(bool dither) {
    if (m_ditherCheck) {
        m_ditherCheck->setChecked(dither);
    }
}

} // namespace Kaburagi
