#pragma once

#include <QWidget>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QGroupBox>
#include "SpinScaleWidget.hpp"

namespace Kaburagi {

class VectorPolygonSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit VectorPolygonSettingsWidget(QWidget* parent = nullptr);
    ~VectorPolygonSettingsWidget() override = default;

    int sides() const { return m_sidesScale ? static_cast<int>(m_sidesScale->value()) : 5; }
    void setSides(int s);

    bool isRegular() const { return m_regularCheckBox ? m_regularCheckBox->isChecked() : false; }
    void setRegular(bool reg);

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
    void sidesChanged(int sides);
    void regularChanged(bool regular);
    void sizeChanged(double size);
    void densityChanged(double density);
    void hardnessChanged(int hardness);
    void falloffChanged(int falloff);
    void filledChanged(bool filled);

private:
    void setupUI();

    SpinScale* m_sidesScale = nullptr;
    QCheckBox* m_regularCheckBox = nullptr;
    SpinScale* m_sizeScale = nullptr;
    SpinScale* m_densityScale = nullptr;
    SpinScale* m_hardnessScale = nullptr;
    SpinScale* m_falloffScale = nullptr;
    QCheckBox* m_fillCheckBox = nullptr;
};

} // namespace Kaburagi
