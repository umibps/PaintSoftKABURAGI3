#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QGroupBox>

namespace Kaburagi {

class VectorPressureSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit VectorPressureSettingsWidget(QWidget* parent = nullptr);
    ~VectorPressureSettingsWidget() override = default;

private:
    void setupUI();
};

} // namespace Kaburagi
