#include "VectorPressureSettingsWidget.hpp"

namespace Kaburagi {

VectorPressureSettingsWidget::VectorPressureSettingsWidget(QWidget* parent)
    : QWidget(parent) {
    setupUI();
}

void VectorPressureSettingsWidget::setupUI() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    auto group = new QGroupBox("筆圧ツール", this);
    auto groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(6, 8, 6, 8);
    groupLayout->setSpacing(6);

    auto infoLabel = new QLabel(
        "制御点の筆圧を調整します\n\n"
        "制御点上でドラッグを開始して、\n"
        "・左方向で筆圧減少\n"
        "・右方向で筆圧増加\n"
        "となります", group);
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: #444444; font-size: 11px;");
    groupLayout->addWidget(infoLabel);

    mainLayout->addWidget(group);
    mainLayout->addStretch();
}

} // namespace Kaburagi
