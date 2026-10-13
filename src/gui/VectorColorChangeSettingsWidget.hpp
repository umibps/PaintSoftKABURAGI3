#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QGroupBox>

namespace Kaburagi {

class VectorColorChangeSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit VectorColorChangeSettingsWidget(QWidget* parent = nullptr);
    ~VectorColorChangeSettingsWidget() override = default;

private:
    void setupUI();
};

} // namespace Kaburagi
