#include "VectorControlPointSettingsWidget.hpp"

namespace Kaburagi {

VectorControlPointSettingsWidget::VectorControlPointSettingsWidget(QWidget* parent)
    : QWidget(parent) {
    setupUI();
}

void VectorControlPointSettingsWidget::setupUI() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    auto box = new QGroupBox("処理内容", this);
    box->setStyleSheet("QGroupBox { font-size: 11px; font-weight: bold; }");
    auto boxLayout = new QVBoxLayout(box);
    boxLayout->setContentsMargins(6, 6, 6, 6);
    boxLayout->setSpacing(5);

    m_modeGroup = new QButtonGroup(this);

    m_radioMoveOrAddPoint = new QRadioButton("制御点 移動/追加", box);
    m_radioMoveOrAddPoint->setStyleSheet("font-size: 11px;");
    m_radioMoveOrAddPoint->setToolTip("制御点をドラッグして移動、またはストローク上をクリックして新しい制御点を追加します。");

    m_radioMovePoint = new QRadioButton("制御点移動", box);
    m_radioMovePoint->setStyleSheet("font-size: 11px;");
    m_radioMovePoint->setToolTip("制御点をドラッグして移動します。");

    m_radioAddPoint = new QRadioButton("制御点追加", box);
    m_radioAddPoint->setStyleSheet("font-size: 11px;");
    m_radioAddPoint->setToolTip("ストローク上をクリックして新しい制御点を追加します。");

    m_radioDeletePoint = new QRadioButton("制御点削除", box);
    m_radioDeletePoint->setStyleSheet("font-size: 11px;");
    m_radioDeletePoint->setToolTip("制御点をクリックして削除します。");

    m_radioMoveStroke = new QRadioButton("ストローク移動", box);
    m_radioMoveStroke->setStyleSheet("font-size: 11px;");
    m_radioMoveStroke->setToolTip("ストロークをドラッグして全体を移動します。");

    m_radioDuplicateMoveStroke = new QRadioButton("ストローク複製移動", box);
    m_radioDuplicateMoveStroke->setStyleSheet("font-size: 11px;");
    m_radioDuplicateMoveStroke->setToolTip("ストロークを複製してドラッグ移動します。");

    m_radioRotateStroke = new QRadioButton("ストローク回転", box);
    m_radioRotateStroke->setStyleSheet("font-size: 11px;");
    m_radioRotateStroke->setToolTip("ストロークをドラッグして制御点の中心を基準に回転します。");

    m_radioConnectStrokes = new QRadioButton("ストローク連結", box);
    m_radioConnectStrokes->setStyleSheet("font-size: 11px;");
    m_radioConnectStrokes->setToolTip("ストロークの端点をドラッグして別の線の端点と連結します。");

    m_radioDeleteStroke = new QRadioButton("ストローク削除", box);
    m_radioDeleteStroke->setStyleSheet("font-size: 11px;");
    m_radioDeleteStroke->setToolTip("ストロークをクリックして線全体を削除します。");

    m_modeGroup->addButton(m_radioMoveOrAddPoint, static_cast<int>(VectorControlPointMode::MoveOrAddPoint));
    m_modeGroup->addButton(m_radioMovePoint, static_cast<int>(VectorControlPointMode::MovePoint));
    m_modeGroup->addButton(m_radioAddPoint, static_cast<int>(VectorControlPointMode::AddPoint));
    m_modeGroup->addButton(m_radioDeletePoint, static_cast<int>(VectorControlPointMode::DeletePoint));
    m_modeGroup->addButton(m_radioMoveStroke, static_cast<int>(VectorControlPointMode::MoveStroke));
    m_modeGroup->addButton(m_radioDuplicateMoveStroke, static_cast<int>(VectorControlPointMode::DuplicateMoveStroke));
    m_modeGroup->addButton(m_radioRotateStroke, static_cast<int>(VectorControlPointMode::RotateStroke));
    m_modeGroup->addButton(m_radioConnectStrokes, static_cast<int>(VectorControlPointMode::ConnectStrokes));
    m_modeGroup->addButton(m_radioDeleteStroke, static_cast<int>(VectorControlPointMode::DeleteStroke));

    boxLayout->addWidget(m_radioMoveOrAddPoint);
    boxLayout->addWidget(m_radioMovePoint);
    boxLayout->addWidget(m_radioAddPoint);
    boxLayout->addWidget(m_radioDeletePoint);
    boxLayout->addWidget(m_radioMoveStroke);
    boxLayout->addWidget(m_radioDuplicateMoveStroke);
    boxLayout->addWidget(m_radioRotateStroke);
    boxLayout->addWidget(m_radioConnectStrokes);
    boxLayout->addWidget(m_radioDeleteStroke);

    mainLayout->addWidget(box);
    mainLayout->addStretch(1);

    m_radioMovePoint->setChecked(true);

    connect(m_modeGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_mode = static_cast<VectorControlPointMode>(id);
        emit modeChanged(m_mode);
    });
}

void VectorControlPointSettingsWidget::setMode(VectorControlPointMode mode) {
    m_mode = mode;
    auto btn = m_modeGroup->button(static_cast<int>(mode));
    if (btn) {
        btn->setChecked(true);
    }
    emit modeChanged(m_mode);
}

} // namespace Kaburagi
