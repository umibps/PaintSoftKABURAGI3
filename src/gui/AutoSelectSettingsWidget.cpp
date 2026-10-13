#include "AutoSelectSettingsWidget.hpp"

namespace Kaburagi {

AutoSelectSettingsWidget::AutoSelectSettingsWidget(QWidget* parent)
    : QWidget(parent) {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    // 1. 選択モード
    auto modeTitle = new QLabel("選択モード", this);
    modeTitle->setStyleSheet("font-weight: bold; font-size: 11px; color: #333333;");
    mainLayout->addWidget(modeTitle);

    m_modeGroup = new QButtonGroup(this);
    m_radioNew = new QRadioButton("通常選択 (上書き)", this);
    m_radioAdd = new QRadioButton("追加選択 (+Shift)", this);
    m_radioSubtract = new QRadioButton("範囲削除 (+Alt)", this);

    m_radioNew->setChecked(true);

    m_modeGroup->addButton(m_radioNew, static_cast<int>(SelectionMode::New));
    m_modeGroup->addButton(m_radioAdd, static_cast<int>(SelectionMode::Add));
    m_modeGroup->addButton(m_radioSubtract, static_cast<int>(SelectionMode::Subtract));

    mainLayout->addWidget(m_radioNew);
    mainLayout->addWidget(m_radioAdd);
    mainLayout->addWidget(m_radioSubtract);

    connect(m_modeGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_mode = static_cast<SelectionMode>(id);
        emit modeChanged(m_mode);
    });

    // 区切り線 1
    auto line1 = new QFrame(this);
    line1->setFrameShape(QFrame::HLine);
    line1->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line1);

    // 2. 参照先レイヤーの選択
    auto refTitle = new QLabel("参照先レイヤー", this);
    refTitle->setStyleSheet("font-weight: bold; font-size: 11px; color: #333333;");
    mainLayout->addWidget(refTitle);

    m_refGroup = new QButtonGroup(this);
    m_radioCanvas = new QRadioButton("キャンバス (全レイヤー)", this);
    m_radioActiveLayer = new QRadioButton("アクティブなレイヤー", this);

    m_radioCanvas->setChecked(true);

    m_refGroup->addButton(m_radioCanvas, static_cast<int>(BucketReference::Canvas));
    m_refGroup->addButton(m_radioActiveLayer, static_cast<int>(BucketReference::ActiveLayer));

    mainLayout->addWidget(m_radioCanvas);
    mainLayout->addWidget(m_radioActiveLayer);

    connect(m_refGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_refMode = static_cast<BucketReference>(id);
        emit referenceModeChanged(m_refMode);
    });

    // 区切り線 2
    auto line2 = new QFrame(this);
    line2->setFrameShape(QFrame::HLine);
    line2->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line2);

    // 3. 領域拡縮 (-10 ～ +10 px)
    m_expandScale = new SpinScale("領域拡縮", -10.0, 10.0, 1.0, 0, " px", this);
    m_expandScale->setValue(m_expandPixels);
    mainLayout->addWidget(m_expandScale);

    connect(m_expandScale, &SpinScale::valueChanged, this, [this](double val) {
        m_expandPixels = static_cast<int>(std::round(val));
        emit expandPixelsChanged(m_expandPixels);
    });

    // 区切り線 3
    auto line3 = new QFrame(this);
    line3->setFrameShape(QFrame::HLine);
    line3->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line3);

    // 4. 細い領域にしみこむ (鋭角選択漏れ防止オプション)
    m_narrowAreaCheck = new QCheckBox("細い領域にしみこむ", this);
    m_narrowAreaCheck->setChecked(true);
    m_narrowAreaCheck->setToolTip("三角形の頂点のような鋭角な先端や、細い隙間への選択漏れを防ぎます (CLIP STUDIO PAINT同等機能)");
    mainLayout->addWidget(m_narrowAreaCheck);

    connect(m_narrowAreaCheck, &QCheckBox::toggled, this, &AutoSelectSettingsWidget::narrowAreaFillChanged);

    // 区切り線 4
    auto line4 = new QFrame(this);
    line4->setFrameShape(QFrame::HLine);
    line4->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line4);

    // 5. 色の誤差 (Tolerance: 0 ～ 100)
    m_toleranceScale = new SpinScale("色の誤差", 0.0, 100.0, 1.0, 0, "", this);
    m_toleranceScale->setValue(m_tolerance);
    mainLayout->addWidget(m_toleranceScale);

    connect(m_toleranceScale, &SpinScale::valueChanged, this, [this](double val) {
        m_tolerance = static_cast<int>(std::round(val));
        emit toleranceChanged(m_tolerance);
    });

    mainLayout->addStretch(1);
}

void AutoSelectSettingsWidget::setSelectionMode(SelectionMode mode) {
    m_mode = mode;
    if (m_mode == SelectionMode::New && m_radioNew) m_radioNew->setChecked(true);
    else if (m_mode == SelectionMode::Add && m_radioAdd) m_radioAdd->setChecked(true);
    else if (m_mode == SelectionMode::Subtract && m_radioSubtract) m_radioSubtract->setChecked(true);
}

void AutoSelectSettingsWidget::setReferenceMode(BucketReference mode) {
    m_refMode = mode;
    if (m_refMode == BucketReference::Canvas && m_radioCanvas) {
        m_radioCanvas->setChecked(true);
    } else if (m_refMode == BucketReference::ActiveLayer && m_radioActiveLayer) {
        m_radioActiveLayer->setChecked(true);
    }
}

void AutoSelectSettingsWidget::setExpandPixels(int pixels) {
    m_expandPixels = std::clamp(pixels, -10, 10);
    if (m_expandScale) m_expandScale->setValue(m_expandPixels);
}

void AutoSelectSettingsWidget::setNarrowAreaFill(bool enable) {
    if (m_narrowAreaCheck) {
        m_narrowAreaCheck->setChecked(enable);
    }
}

void AutoSelectSettingsWidget::setTolerance(int tol) {
    m_tolerance = std::clamp(tol, 0, 100);
    if (m_toleranceScale) m_toleranceScale->setValue(m_tolerance);
}

} // namespace Kaburagi
