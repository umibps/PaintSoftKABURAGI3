#include "EllipseSelectSettingsWidget.hpp"
#include <QFrame>

namespace Kaburagi {

EllipseSelectSettingsWidget::EllipseSelectSettingsWidget(QWidget* parent)
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

    // 2. 選択方法 (長方形に接する楕円 / ベクトルツール同等)
    auto methodTitle = new QLabel("作成方法", this);
    methodTitle->setStyleSheet("font-weight: bold; font-size: 11px; color: #333333;");
    mainLayout->addWidget(methodTitle);

    m_methodGroup = new QButtonGroup(this);
    m_radioBoundingBox = new QRadioButton("長方形に接する楕円 (外接矩形)", this);
    m_radioFromCenter = new QRadioButton("中心から作成 (ベクトルツール同等)", this);
    m_radioBoundingBox->setChecked(true);

    m_radioBoundingBox->setToolTip("ドラッグ開始位置と終了位置を結ぶ矩形に内接する楕円を作成します");
    m_radioFromCenter->setToolTip("ドラッグ開始位置を中心とし、ドラッグ距離を半径とする楕円を作成します");

    m_methodGroup->addButton(m_radioBoundingBox, static_cast<int>(EllipseSelectMethod::BoundingBox));
    m_methodGroup->addButton(m_radioFromCenter, static_cast<int>(EllipseSelectMethod::FromCenter));

    mainLayout->addWidget(m_radioBoundingBox);
    mainLayout->addWidget(m_radioFromCenter);

    connect(m_methodGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_method = static_cast<EllipseSelectMethod>(id);
        emit selectMethodChanged(m_method);
    });

    // 区切り線2
    auto line2 = new QFrame(this);
    line2->setFrameShape(QFrame::HLine);
    line2->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line2);

    // 3. 平行基準切り替えチェックボックス
    m_alignToWindowCheck = new QCheckBox("表示ウィンドウの軸に平行", this);
    m_alignToWindowCheck->setChecked(false);
    m_alignToWindowCheck->setToolTip("チェックなし: 楕円の軸はキャンバスのX軸/Y軸に平行\nチェックあり: 楕円の軸は表示ウィンドウのX軸/Y軸に平行");
    mainLayout->addWidget(m_alignToWindowCheck);

    connect(m_alignToWindowCheck, &QCheckBox::toggled, this, &EllipseSelectSettingsWidget::alignToWindowChanged);

    mainLayout->addStretch(1);
}

void EllipseSelectSettingsWidget::setSelectionMode(SelectionMode mode) {
    m_mode = mode;
    if (m_mode == SelectionMode::New && m_radioNew) m_radioNew->setChecked(true);
    else if (m_mode == SelectionMode::Add && m_radioAdd) m_radioAdd->setChecked(true);
    else if (m_mode == SelectionMode::Subtract && m_radioSubtract) m_radioSubtract->setChecked(true);
}

void EllipseSelectSettingsWidget::setSelectMethod(EllipseSelectMethod method) {
    m_method = method;
    if (m_method == EllipseSelectMethod::BoundingBox && m_radioBoundingBox) m_radioBoundingBox->setChecked(true);
    else if (m_method == EllipseSelectMethod::FromCenter && m_radioFromCenter) m_radioFromCenter->setChecked(true);
}

void EllipseSelectSettingsWidget::setAlignToWindow(bool align) {
    if (m_alignToWindowCheck) {
        m_alignToWindowCheck->setChecked(align);
    }
}

} // namespace Kaburagi
