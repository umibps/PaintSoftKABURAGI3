#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QGroupBox>
#include "SpinScaleWidget.hpp"

namespace Kaburagi {

class VectorChangeLineSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit VectorChangeLineSettingsWidget(QWidget* parent = nullptr);
    ~VectorChangeLineSettingsWidget() override = default;

    double lineSize() const { return m_sizeScale ? m_sizeScale->value() : 4.0; }
    void setLineSize(double size);

    double lineDensity() const { return m_densityScale ? m_densityScale->value() / 100.0 : 1.0; }
    void setLineDensity(double density);

    int lineHardness() const { return m_hardnessScale ? static_cast<int>(m_hardnessScale->value()) : 100; }
    void setLineHardness(int hardness);

    int lineFalloff() const { return m_falloffScale ? static_cast<int>(m_falloffScale->value()) : 0; }
    void setLineFalloff(int falloff);

signals:
    void sizeChanged(double size);
    void densityChanged(double density);
    void hardnessChanged(int hardness);
    void falloffChanged(int falloff);

private:
    void setupUI();

    SpinScale* m_sizeScale = nullptr;
    SpinScale* m_densityScale = nullptr;
    SpinScale* m_hardnessScale = nullptr;
    SpinScale* m_falloffScale = nullptr;
};

} // namespace Kaburagi
