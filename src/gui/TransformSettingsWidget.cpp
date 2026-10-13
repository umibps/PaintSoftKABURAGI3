#include "TransformSettingsWidget.hpp"

namespace Kaburagi {

TransformSettingsWidget::TransformSettingsWidget(QWidget* parent)
    : QWidget(parent)
{
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    // タイトル & モード切り替え
    auto titleLabel = new QLabel("変形ツール", this);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 11px; padding: 2px; color: #333333;");
    mainLayout->addWidget(titleLabel);

    auto modeGroup = new QGroupBox("変形モード", this);
    auto modeLayout = new QVBoxLayout(modeGroup);
    modeLayout->setContentsMargins(4, 6, 4, 4);
    modeLayout->setSpacing(4);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem("統合変形 (拡大縮小/回転/自由変形)", static_cast<int>(TransformMode::Unified));
    m_modeCombo->addItem("ワープ変形", static_cast<int>(TransformMode::Warp));
    m_modeCombo->addItem("ケージ変形", static_cast<int>(TransformMode::Cage));
    modeLayout->addWidget(m_modeCombo);

    // 反転ボタン（左右反転、上下反転）
    auto flipLayout = new QHBoxLayout();
    flipLayout->setContentsMargins(0, 2, 0, 0);
    flipLayout->setSpacing(4);
    m_btnFlipH = new QPushButton("左右反転", this);
    m_btnFlipV = new QPushButton("上下反転", this);
    flipLayout->addWidget(m_btnFlipH);
    flipLayout->addWidget(m_btnFlipV);
    modeLayout->addLayout(flipLayout);

    mainLayout->addWidget(modeGroup);

    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        updateModeVisibility();
        emit transformModeChanged(transformMode());
    });
    connect(m_btnFlipH, &QPushButton::clicked, this, &TransformSettingsWidget::flipHorizontalRequested);
    connect(m_btnFlipV, &QPushButton::clicked, this, &TransformSettingsWidget::flipVerticalRequested);

    // --- 1. 統合変形 パネル ---
    m_unifiedPanel = new QWidget(this);
    auto unifiedLayout = new QVBoxLayout(m_unifiedPanel);
    unifiedLayout->setContentsMargins(0, 0, 0, 0);
    unifiedLayout->setSpacing(4);

    auto unifiedOptGroup = new QGroupBox("統合変形設定", m_unifiedPanel);
    auto optLayout = new QVBoxLayout(unifiedOptGroup);
    optLayout->setContentsMargins(4, 6, 4, 4);
    optLayout->setSpacing(4);

    m_keepAspectCheck = new QCheckBox("縦横比を固定 (Shift)", unifiedOptGroup);
    m_fromCenterCheck = new QCheckBox("中心基準で変形 (Alt)", unifiedOptGroup);
    optLayout->addWidget(m_keepAspectCheck);
    optLayout->addWidget(m_fromCenterCheck);
    unifiedLayout->addWidget(unifiedOptGroup);

    connect(m_keepAspectCheck, &QCheckBox::toggled, this, &TransformSettingsWidget::keepAspectChanged);
    connect(m_fromCenterCheck, &QCheckBox::toggled, this, &TransformSettingsWidget::fromCenterChanged);

    mainLayout->addWidget(m_unifiedPanel);

    // --- 2. ワープ変形 パネル ---
    m_warpPanel = new QWidget(this);
    auto warpLayout = new QVBoxLayout(m_warpPanel);
    warpLayout->setContentsMargins(0, 0, 0, 0);
    warpLayout->setSpacing(4);

    auto warpOptGroup = new QGroupBox("ワープ変形設定", m_warpPanel);
    auto warpOptLayout = new QVBoxLayout(warpOptGroup);
    warpOptLayout->setContentsMargins(4, 6, 4, 4);
    warpOptLayout->setSpacing(4);

    warpOptLayout->addWidget(new QLabel("ワープ方式:", warpOptGroup));
    m_warpSubModeCombo = new QComboBox(warpOptGroup);
    m_warpSubModeCombo->addItem("ブラシワープ (押し出し/歪み)", static_cast<int>(WarpSubMode::Brush));
    m_warpSubModeCombo->addItem("メッシュワープ (格子点変形)", static_cast<int>(WarpSubMode::Mesh));
    warpOptLayout->addWidget(m_warpSubModeCombo);

    // ブラシワープ用コントロール
    m_warpBrushControls = new QWidget(warpOptGroup);
    auto wbLayout = new QVBoxLayout(m_warpBrushControls);
    wbLayout->setContentsMargins(0, 4, 0, 0);
    wbLayout->setSpacing(4);

    m_warpSizeSpin = new SpinScale("ブラシサイズ", 5.0, 500.0, 1.0, 1, "px", m_warpBrushControls);
    m_warpSizeSpin->setValue(60.0);
    m_warpStrengthSpin = new SpinScale("強さ", 1.0, 100.0, 1.0, 0, "%", m_warpBrushControls);
    m_warpStrengthSpin->setValue(50.0);
    wbLayout->addWidget(m_warpSizeSpin);
    wbLayout->addWidget(m_warpStrengthSpin);
    warpOptLayout->addWidget(m_warpBrushControls);

    // メッシュワープ用コントロール
    m_warpMeshControls = new QWidget(warpOptGroup);
    auto wmLayout = new QHBoxLayout(m_warpMeshControls);
    wmLayout->setContentsMargins(0, 4, 0, 0);
    wmLayout->setSpacing(4);
    wmLayout->addWidget(new QLabel("格子分割数:", m_warpMeshControls));
    m_warpMeshDivCombo = new QComboBox(m_warpMeshControls);
    m_warpMeshDivCombo->addItem("3 x 3", 3);
    m_warpMeshDivCombo->addItem("4 x 4", 4);
    m_warpMeshDivCombo->addItem("5 x 5", 5);
    m_warpMeshDivCombo->setCurrentIndex(1); // 4x4
    wmLayout->addWidget(m_warpMeshDivCombo);
    warpOptLayout->addWidget(m_warpMeshControls);

    warpLayout->addWidget(warpOptGroup);
    mainLayout->addWidget(m_warpPanel);

    connect(m_warpSubModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        bool isBrush = (warpSubMode() == WarpSubMode::Brush);
        m_warpBrushControls->setVisible(isBrush);
        m_warpMeshControls->setVisible(!isBrush);
        emit warpSubModeChanged(warpSubMode());
    });
    connect(m_warpSizeSpin, &SpinScale::valueChanged, this, [this](double val) {
        emit warpBrushSizeChanged(static_cast<float>(val));
    });
    connect(m_warpStrengthSpin, &SpinScale::valueChanged, this, [this](double val) {
        emit warpBrushStrengthChanged(static_cast<float>(val));
    });
    connect(m_warpMeshDivCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        emit warpMeshDivisionsChanged(warpMeshDivisions());
    });

    // --- 3. ケージ変形 パネル ---
    m_cagePanel = new QWidget(this);
    auto cageLayout = new QVBoxLayout(m_cagePanel);
    cageLayout->setContentsMargins(0, 0, 0, 0);
    cageLayout->setSpacing(4);

    auto cageOptGroup = new QGroupBox("ケージ変形設定", m_cagePanel);
    auto cageOptLayout = new QVBoxLayout(cageOptGroup);
    cageOptLayout->setContentsMargins(4, 6, 4, 4);
    cageOptLayout->setSpacing(4);

    cageOptLayout->addWidget(new QLabel("ケージ生成方式:", cageOptGroup));
    m_cageSubModeCombo = new QComboBox(cageOptGroup);
    m_cageSubModeCombo->addItem("自動バウンディングボックス (推奨)", static_cast<int>(CageSubMode::AutoBox));
    m_cageSubModeCombo->addItem("カスタム多角形ケージ", static_cast<int>(CageSubMode::CustomPolygon));
    cageOptLayout->addWidget(m_cageSubModeCombo);

    m_btnResetCage = new QPushButton("ケージを初期化", cageOptGroup);
    cageOptLayout->addWidget(m_btnResetCage);
    cageLayout->addWidget(cageOptGroup);
    mainLayout->addWidget(m_cagePanel);

    connect(m_cageSubModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        emit cageSubModeChanged(cageSubMode());
    });
    connect(m_btnResetCage, &QPushButton::clicked, this, &TransformSettingsWidget::resetCageRequested);

    // --- 4. 補間アルゴリズム ---
    auto interpGroup = new QGroupBox("補間方法", this);
    auto interpLayout = new QVBoxLayout(interpGroup);
    interpLayout->setContentsMargins(4, 6, 4, 4);

    m_interpolationCombo = new QComboBox(this);
    m_interpolationCombo->addItem("バイリニア (標準)", static_cast<int>(InterpolationMode::Bilinear));
    m_interpolationCombo->addItem("バイキュービック (高品質)", static_cast<int>(InterpolationMode::Bicubic));
    m_interpolationCombo->addItem("最近傍 (ニアレストネイバー)", static_cast<int>(InterpolationMode::NearestNeighbor));
    interpLayout->addWidget(m_interpolationCombo);
    mainLayout->addWidget(interpGroup);

    connect(m_interpolationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        emit interpolationChanged(interpolationMode());
    });

    mainLayout->addStretch(1);

    // --- 5. 確定 / キャンセル ボタン ---
    auto btnLayout = new QHBoxLayout();
    btnLayout->setContentsMargins(0, 4, 0, 0);
    btnLayout->setSpacing(4);

    m_btnApply = new QPushButton("変形確定 (Enter)", this);
    m_btnApply->setStyleSheet(
        "QPushButton { background: #3377dd; color: white; font-weight: bold; padding: 4px 8px; border-radius: 3px; }"
        "QPushButton:hover { background: #2266cc; }"
        "QPushButton:pressed { background: #1155bb; }"
    );
    m_btnCancel = new QPushButton("キャンセル (Esc)", this);
    m_btnCancel->setStyleSheet(
        "QPushButton { background: #e0e0e0; color: #333333; padding: 4px 8px; border: 1px solid #b0b0b0; border-radius: 3px; }"
        "QPushButton:hover { background: #d0d0d0; }"
        "QPushButton:pressed { background: #c0c0c0; }"
    );
    btnLayout->addWidget(m_btnApply);
    btnLayout->addWidget(m_btnCancel);
    mainLayout->addLayout(btnLayout);

    connect(m_btnApply, &QPushButton::clicked, this, &TransformSettingsWidget::applyRequested);
    connect(m_btnCancel, &QPushButton::clicked, this, &TransformSettingsWidget::cancelRequested);

    updateModeVisibility();
}

void TransformSettingsWidget::updateModeVisibility() {
    auto m = transformMode();
    m_unifiedPanel->setVisible(m == TransformMode::Unified);
    m_warpPanel->setVisible(m == TransformMode::Warp);
    m_cagePanel->setVisible(m == TransformMode::Cage);

    if (m == TransformMode::Warp) {
        bool isBrush = (warpSubMode() == WarpSubMode::Brush);
        m_warpBrushControls->setVisible(isBrush);
        m_warpMeshControls->setVisible(!isBrush);
    }
}

TransformMode TransformSettingsWidget::transformMode() const {
    return static_cast<TransformMode>(m_modeCombo->currentData().toInt());
}

void TransformSettingsWidget::setTransformMode(TransformMode mode) {
    int idx = m_modeCombo->findData(static_cast<int>(mode));
    if (idx >= 0) {
        m_modeCombo->setCurrentIndex(idx);
    }
    updateModeVisibility();
}

InterpolationMode TransformSettingsWidget::interpolationMode() const {
    return static_cast<InterpolationMode>(m_interpolationCombo->currentData().toInt());
}

void TransformSettingsWidget::setInterpolationMode(InterpolationMode mode) {
    int idx = m_interpolationCombo->findData(static_cast<int>(mode));
    if (idx >= 0) {
        m_interpolationCombo->setCurrentIndex(idx);
    }
}

bool TransformSettingsWidget::isKeepAspect() const {
    return m_keepAspectCheck && m_keepAspectCheck->isChecked();
}

void TransformSettingsWidget::setKeepAspect(bool keep) {
    if (m_keepAspectCheck) {
        m_keepAspectCheck->setChecked(keep);
    }
}

bool TransformSettingsWidget::isFromCenter() const {
    return m_fromCenterCheck && m_fromCenterCheck->isChecked();
}

void TransformSettingsWidget::setFromCenter(bool fromCenter) {
    if (m_fromCenterCheck) {
        m_fromCenterCheck->setChecked(fromCenter);
    }
}

WarpSubMode TransformSettingsWidget::warpSubMode() const {
    return static_cast<WarpSubMode>(m_warpSubModeCombo->currentData().toInt());
}

void TransformSettingsWidget::setWarpSubMode(WarpSubMode mode) {
    int idx = m_warpSubModeCombo->findData(static_cast<int>(mode));
    if (idx >= 0) {
        m_warpSubModeCombo->setCurrentIndex(idx);
    }
    updateModeVisibility();
}

float TransformSettingsWidget::warpBrushSize() const {
    return static_cast<float>(m_warpSizeSpin ? m_warpSizeSpin->value() : 60.0);
}

void TransformSettingsWidget::setWarpBrushSize(float sz) {
    if (m_warpSizeSpin) m_warpSizeSpin->setValue(sz);
}

float TransformSettingsWidget::warpBrushStrength() const {
    return static_cast<float>(m_warpStrengthSpin ? m_warpStrengthSpin->value() : 50.0);
}

void TransformSettingsWidget::setWarpBrushStrength(float st) {
    if (m_warpStrengthSpin) m_warpStrengthSpin->setValue(st);
}

int TransformSettingsWidget::warpMeshDivisions() const {
    return m_warpMeshDivCombo ? m_warpMeshDivCombo->currentData().toInt() : 4;
}

void TransformSettingsWidget::setWarpMeshDivisions(int div) {
    int idx = m_warpMeshDivCombo ? m_warpMeshDivCombo->findData(div) : -1;
    if (idx >= 0) {
        m_warpMeshDivCombo->setCurrentIndex(idx);
    }
}

CageSubMode TransformSettingsWidget::cageSubMode() const {
    return static_cast<CageSubMode>(m_cageSubModeCombo->currentData().toInt());
}

void TransformSettingsWidget::setCageSubMode(CageSubMode mode) {
    int idx = m_cageSubModeCombo->findData(static_cast<int>(mode));
    if (idx >= 0) {
        m_cageSubModeCombo->setCurrentIndex(idx);
    }
}

} // namespace Kaburagi
