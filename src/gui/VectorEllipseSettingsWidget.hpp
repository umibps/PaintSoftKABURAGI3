#pragma once

#include <QWidget>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QGroupBox>
#include "SpinScaleWidget.hpp"

namespace Kaburagi {

class VectorEllipseSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit VectorEllipseSettingsWidget(QWidget* parent = nullptr);
    ~VectorEllipseSettingsWidget() override = default;

    double lineSize() const { return m_sizeScale ? m_sizeScale->value() : 4.0; }
    void setLineSize(double size);

    double lineDensity() const { return m_densityScale ? m_densityScale->value() / 100.0 : 1.0; }
    void setLineDensity(double density);

    int lineHardness() const { return m_hardnessScale ? static_cast<int>(m_hardnessScale->value()) : 100; }
    void setLineHardness(int hardness);

    int lineFalloff() const { return m_falloffScale ? static_cast<int>(m_falloffScale->value()) : 0; }
    void setLineFalloff(int falloff);

    bool isFilled() const { return m_fillCheckBox ? m_fillCheckBox->isChecked() : false; }
    void setFilled(bool fill);

signals:
    void sizeChanged(double size);
    void densityChanged(double density);
    void hardnessChanged(int hardness);
    void falloffChanged(int falloff);
    void filledChanged(bool filled);

private:
    void setupUI();

    SpinScale* m_sizeScale = nullptr;
    SpinScale* m_densityScale = nullptr;
    SpinScale* m_hardnessScale = nullptr;
    SpinScale* m_falloffScale = nullptr;
    QCheckBox* m_fillCheckBox = nullptr;
};

} // namespace Kaburagi
