#pragma once

#include <QWidget>
#include <QGridLayout>
#include <QPushButton>
#include <QCheckBox>
#include "SpinScaleWidget.hpp"

namespace Kaburagi {

struct BlurConfig {
    float size = 30.0f;
    float minSizeRatio = 0.0f;
    float density = 1.0f;
    bool pressureSize = true;
    bool pressureDensity = false;
    int colorMix = 50;
    int colorStretch = 50;
    int hardness = 0;
    int falloff = 100;
    int minPressure = 0;
};

class BlurSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit BlurSettingsWidget(QWidget* parent = nullptr);
    ~BlurSettingsWidget() override = default;

    float blurSize() const { return static_cast<float>(m_sizeScale->value()); }
    float minSizeRatio() const { return static_cast<float>(m_minSizeScale->value()) / 100.0f; }
    float blurDensity() const { return static_cast<float>(m_densityScale->value()) / 100.0f; }
    bool isPressureSizeEnabled() const { return m_pressureSizeCheck ? m_pressureSizeCheck->isChecked() : true; }
    bool isPressureDensityEnabled() const { return m_pressureDensityCheck ? m_pressureDensityCheck->isChecked() : false; }
    int colorMix() const { return static_cast<int>(m_colorMixScale->value()); }
    int colorStretch() const { return static_cast<int>(m_colorStretchScale->value()); }
    int hardness() const { return static_cast<int>(m_hardnessScale->value()); }
    int falloff() const { return static_cast<int>(m_falloffScale->value()); }
    int minPressure() const { return static_cast<int>(m_minPressureScale->value()); }

    BlurConfig config() const {
        BlurConfig cfg;
        cfg.size = blurSize();
        cfg.minSizeRatio = minSizeRatio();
        cfg.density = blurDensity();
        cfg.pressureSize = isPressureSizeEnabled();
        cfg.pressureDensity = isPressureDensityEnabled();
        cfg.colorMix = colorMix();
        cfg.colorStretch = colorStretch();
        cfg.hardness = hardness();
        cfg.falloff = falloff();
        cfg.minPressure = minPressure();
        return cfg;
    }

    void setConfig(const BlurConfig& cfg);
    void setBlurSize(float size);
    void setMinSizeRatio(float ratio);
    void setBlurDensity(float density);
    void setPressureSizeEnabled(bool enabled);
    void setPressureDensityEnabled(bool enabled);
    void setColorMix(int mix);
    void setColorStretch(int stretch);
    void setHardness(int hardness);
    void setFalloff(int falloff);
    void setMinPressure(int minPressure);

signals:
    void blurSizeChanged(float size);
    void minSizeChanged(float ratio);
    void densityChanged(float density);
    void pressureSizeChanged(bool enabled);
    void pressureDensityChanged(bool enabled);
    void colorMixChanged(int mix);
    void colorStretchChanged(int stretch);
    void hardnessChanged(int hardness);
    void falloffChanged(int falloff);
    void minPressureChanged(int minPressure);

private:
    void createSizePresetButton(float size, int row, int col, QGridLayout* grid);

    SpinScale* m_sizeScale = nullptr;
    SpinScale* m_minSizeScale = nullptr;
    SpinScale* m_densityScale = nullptr;
    SpinScale* m_hardnessScale = nullptr;
    SpinScale* m_falloffScale = nullptr;
    SpinScale* m_minPressureScale = nullptr;
    QCheckBox* m_pressureSizeCheck = nullptr;
    QCheckBox* m_pressureDensityCheck = nullptr;
    SpinScale* m_colorMixScale = nullptr;
    SpinScale* m_colorStretchScale = nullptr;
};

} // namespace Kaburagi
