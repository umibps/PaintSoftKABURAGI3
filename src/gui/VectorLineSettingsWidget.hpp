#pragma once

#include <QWidget>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QGroupBox>
#include "SpinScaleWidget.hpp"

namespace Kaburagi {

class VectorLineSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit VectorLineSettingsWidget(QWidget* parent = nullptr);
    ~VectorLineSettingsWidget() override = default;

    double lineSize() const { return m_sizeScale ? m_sizeScale->value() : 4.0; }
    void setLineSize(double size);

    double lineDensity() const { return m_densityScale ? m_densityScale->value() / 100.0 : 1.0; }
    void setLineDensity(double density);

    bool isClosePath() const { return m_closePathCheckBox ? m_closePathCheckBox->isChecked() : false; }
    void setClosePath(bool closed);

signals:
    void sizeChanged(double size);
    void densityChanged(double density);
    void closePathChanged(bool closed);

private:
    void setupUI();

    SpinScale* m_sizeScale = nullptr;
    SpinScale* m_densityScale = nullptr;
    QCheckBox* m_closePathCheckBox = nullptr;
};

} // namespace Kaburagi
