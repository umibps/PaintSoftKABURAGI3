#include "BucketSettingsWidget.hpp"

namespace Kaburagi {

BucketSettingsWidget::BucketSettingsWidget(QWidget* parent)
    : QWidget(parent) {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
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

    auto line0 = new QFrame(this);
    line0->setFrameShape(QFrame::HLine);
    line0->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line0);

    // 1. 参照先の選択
    auto refTitle = new QLabel("参照先レイヤー", this);
    refTitle->setStyleSheet("font-weight: bold; font-size: 11px; color: #333333;");
    mainLayout->addWidget(refTitle);

    m_refGroup = new QButtonGroup(this);
    m_radioCanvas = new QRadioButton("キャンバス (全レイヤー)", this);
    m_radioActiveLayer = new QRadioButton("アクティブなレイヤー", this);

    m_radioCanvas->setChecked(true); // デフォルトでキャンバス参照

    m_refGroup->addButton(m_radioCanvas, static_cast<int>(BucketReference::Canvas));
    m_refGroup->addButton(m_radioActiveLayer, static_cast<int>(BucketReference::ActiveLayer));

    mainLayout->addWidget(m_radioCanvas);
    mainLayout->addWidget(m_radioActiveLayer);

    connect(m_refGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_refMode = static_cast<BucketReference>(id);
        emit referenceModeChanged(m_refMode);
    });

    // 区切り線 1
    auto line1 = new QFrame(this);
    line1->setFrameShape(QFrame::HLine);
    line1->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line1);

    // 2. 塗りつぶし領域の拡大/縮小 (-10 ～ +10)
    m_expandScale = new SpinScale("領域拡縮", -10.0, 10.0, 1.0, 0, " px", this);
    m_expandScale->setValue(m_expandPixels);
    mainLayout->addWidget(m_expandScale);

    connect(m_expandScale, &SpinScale::valueChanged, this, [this](double val) {
        m_expandPixels = static_cast<int>(std::round(val));
        emit expandPixelsChanged(m_expandPixels);
    });

    // 区切り線 2
    auto line2 = new QFrame(this);
    line2->setFrameShape(QFrame::HLine);
    line2->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line2);

    // 3. 細い領域にしみこむ (鋭角塗り残し防止オプション)
    m_narrowAreaCheck = new QCheckBox("細い領域にしみこむ", this);
    m_narrowAreaCheck->setChecked(true);
    m_narrowAreaCheck->setToolTip("三角形の頂点のような鋭角な先端や、細い隙間への塗り残しを防ぎます (CLIP STUDIO PAINT同等機能)");
    mainLayout->addWidget(m_narrowAreaCheck);

    connect(m_narrowAreaCheck, &QCheckBox::toggled, this, &BucketSettingsWidget::narrowAreaFillChanged);

    // 区切り線 3
    auto line3 = new QFrame(this);
    line3->setFrameShape(QFrame::HLine);
    line3->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line3);

    // 4. 色の誤差 (Tolerance: 0 ～ 100)
    m_toleranceScale = new SpinScale("色の誤差", 0.0, 100.0, 1.0, 0, "", this);
    m_toleranceScale->setValue(m_tolerance);
    mainLayout->addWidget(m_toleranceScale);

    connect(m_toleranceScale, &SpinScale::valueChanged, this, [this](double val) {
        m_tolerance = static_cast<int>(std::round(val));
        emit toleranceChanged(m_tolerance);
    });

    mainLayout->addStretch(1);
}

void BucketSettingsWidget::setReferenceMode(BucketReference mode) {
    m_refMode = mode;
    if (m_refMode == BucketReference::Canvas && m_radioCanvas) {
        m_radioCanvas->setChecked(true);
    } else if (m_refMode == BucketReference::ActiveLayer && m_radioActiveLayer) {
        m_radioActiveLayer->setChecked(true);
    }
}

void BucketSettingsWidget::setExpandPixels(int pixels) {
    m_expandPixels = std::clamp(pixels, -10, 10);
    if (m_expandScale) m_expandScale->setValue(m_expandPixels);
}

void BucketSettingsWidget::setNarrowAreaFill(bool enable) {
    if (m_narrowAreaCheck) {
        m_narrowAreaCheck->setChecked(enable);
    }
}

void BucketSettingsWidget::setTolerance(int tol) {
    m_tolerance = std::clamp(tol, 0, 100);
    if (m_toleranceScale) m_toleranceScale->setValue(m_tolerance);
}

BlendMode BucketSettingsWidget::blendMode() const {
    if (!m_blendCombo || m_blendCombo->currentIndex() < 0) return BlendMode::Normal;
    return static_cast<BlendMode>(m_blendCombo->currentData().toInt());
}

void BucketSettingsWidget::setBlendMode(BlendMode mode) {
    if (!m_blendCombo) return;
    int idx = m_blendCombo->findData(static_cast<int>(mode));
    if (idx >= 0) {
        m_blendCombo->setCurrentIndex(idx);
    }
}

} // namespace Kaburagi
