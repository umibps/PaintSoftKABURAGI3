#include "VectorColorChangeSettingsWidget.hpp"

namespace Kaburagi {

VectorColorChangeSettingsWidget::VectorColorChangeSettingsWidget(QWidget* parent)
    : QWidget(parent) {
    setupUI();
}

void VectorColorChangeSettingsWidget::setupUI() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    auto group = new QGroupBox("線色変更", this);
    auto groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(6, 8, 6, 8);
    groupLayout->setSpacing(6);

    auto infoLabel = new QLabel("キャンバス上のストロークを左クリックすると、\n対象のストロークの色を描画色に変更します。", group);
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: #555555; font-size: 11px;");
    groupLayout->addWidget(infoLabel);

    mainLayout->addWidget(group);
    mainLayout->addStretch();
}

} // namespace Kaburagi
