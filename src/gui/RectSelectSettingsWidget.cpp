#include "RectSelectSettingsWidget.hpp"
#include <QFrame>

namespace Kaburagi {

RectSelectSettingsWidget::RectSelectSettingsWidget(QWidget* parent)
    : QWidget(parent) {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    // タイトル
    auto titleLabel = new QLabel("選択モード", this);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 11px; color: #333333;");
    mainLayout->addWidget(titleLabel);

    // ラジオボタン
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

    // 区切り線
    auto line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line);

    // 平行基準切り替えチェックボックス
    m_alignToWindowCheck = new QCheckBox("表示ウィンドウの軸に平行", this);
    m_alignToWindowCheck->setChecked(false);
    m_alignToWindowCheck->setToolTip("チェックなし: 矩形の辺はキャンバスのX軸/Y軸に平行\nチェックあり: 矩形の辺は表示ウィンドウのX軸/Y軸に平行");
    mainLayout->addWidget(m_alignToWindowCheck);

    connect(m_alignToWindowCheck, &QCheckBox::toggled, this, &RectSelectSettingsWidget::alignToWindowChanged);

    mainLayout->addStretch(1);
}

void RectSelectSettingsWidget::setSelectionMode(SelectionMode mode) {
    m_mode = mode;
    if (m_mode == SelectionMode::New && m_radioNew) m_radioNew->setChecked(true);
    else if (m_mode == SelectionMode::Add && m_radioAdd) m_radioAdd->setChecked(true);
    else if (m_mode == SelectionMode::Subtract && m_radioSubtract) m_radioSubtract->setChecked(true);
}

void RectSelectSettingsWidget::setAlignToWindow(bool align) {
    if (m_alignToWindowCheck) {
        m_alignToWindowCheck->setChecked(align);
    }
}

} // namespace Kaburagi
