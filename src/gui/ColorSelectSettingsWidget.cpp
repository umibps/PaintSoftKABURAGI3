#include "ColorSelectSettingsWidget.hpp"

namespace Kaburagi {

ColorSelectSettingsWidget::ColorSelectSettingsWidget(QWidget* parent)
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

    // 区切り線1
    auto line1 = new QFrame(this);
    line1->setFrameShape(QFrame::HLine);
    line1->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line1);

    // 2. 参照先 (アクティブレイヤー / キャンバス)
    auto refTitle = new QLabel("参照先", this);
    refTitle->setStyleSheet("font-weight: bold; font-size: 11px; color: #333333;");
    mainLayout->addWidget(refTitle);

    m_refGroup = new QButtonGroup(this);
    m_radioActiveLayer = new QRadioButton("アクティブなレイヤー", this);
    m_radioCanvas = new QRadioButton("キャンバス (全レイヤー合成)", this);
    m_radioActiveLayer->setChecked(true);

    m_refGroup->addButton(m_radioActiveLayer, static_cast<int>(ColorSelectReference::ActiveLayer));
    m_refGroup->addButton(m_radioCanvas, static_cast<int>(ColorSelectReference::Canvas));

    mainLayout->addWidget(m_radioActiveLayer);
    mainLayout->addWidget(m_radioCanvas);

    connect(m_refGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_refMode = static_cast<ColorSelectReference>(id);
        emit referenceModeChanged(m_refMode);
    });

    // 区切り線2
    auto line2 = new QFrame(this);
    line2->setFrameShape(QFrame::HLine);
    line2->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line2);

    // 3. 色の誤差 (Tolerance: 0 ～ 255)
    auto tolTitle = new QLabel("色の許容誤差 (Tolerance)", this);
    tolTitle->setStyleSheet("font-weight: bold; font-size: 11px; color: #333333;");
    mainLayout->addWidget(tolTitle);

    m_toleranceScale = new SpinScale(tr("色の許容誤差"), 0.0, 255.0, 1.0, 0, "", this);
    m_toleranceScale->setValue(20.0);
    mainLayout->addWidget(m_toleranceScale);

    connect(m_toleranceScale, &SpinScale::valueChanged, this, [this](double val) {
        m_tolerance = static_cast<int>(val);
        emit toleranceChanged(m_tolerance);
    });

    // 4. 透明部分の選択
    m_selectTransparentCheck = new QCheckBox("透明領域も同色として選択", this);
    m_selectTransparentCheck->setChecked(true);
    mainLayout->addWidget(m_selectTransparentCheck);

    connect(m_selectTransparentCheck, &QCheckBox::toggled, this, &ColorSelectSettingsWidget::selectTransparentChanged);

    // ヘルプ
    auto helpLabel = new QLabel("クリックしたピクセルと同系色の領域を、\n画像全体から一括で選択します。", this);
    helpLabel->setStyleSheet("font-size: 11px; color: #666666; padding: 4px; background: #f5f5f5; border-radius: 4px;");
    mainLayout->addWidget(helpLabel);

    mainLayout->addStretch(1);
}

void ColorSelectSettingsWidget::setSelectionMode(SelectionMode mode) {
    m_mode = mode;
    if (m_mode == SelectionMode::New && m_radioNew) m_radioNew->setChecked(true);
    else if (m_mode == SelectionMode::Add && m_radioAdd) m_radioAdd->setChecked(true);
    else if (m_mode == SelectionMode::Subtract && m_radioSubtract) m_radioSubtract->setChecked(true);
}

void ColorSelectSettingsWidget::setReferenceMode(ColorSelectReference mode) {
    m_refMode = mode;
    if (m_refMode == ColorSelectReference::ActiveLayer && m_radioActiveLayer) m_radioActiveLayer->setChecked(true);
    else if (m_refMode == ColorSelectReference::Canvas && m_radioCanvas) m_radioCanvas->setChecked(true);
}

void ColorSelectSettingsWidget::setTolerance(int tol) {
    m_tolerance = tol;
    if (m_toleranceScale) {
        m_toleranceScale->setValue(tol);
    }
}

void ColorSelectSettingsWidget::setSelectTransparent(bool enable) {
    if (m_selectTransparentCheck) {
        m_selectTransparentCheck->setChecked(enable);
    }
}

} // namespace Kaburagi
