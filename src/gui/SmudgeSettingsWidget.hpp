#pragma once

#include <QWidget>
#include <QGridLayout>
#include <QPushButton>
#include <QCheckBox>
#include "SpinScaleWidget.hpp"

namespace Kaburagi {

struct SmudgeConfig {
    float size = 30.0f;
    float minSizeRatio = 0.0f;
    float density = 1.0f;
    int rate = 50; // 割合 (1〜100%)
    bool pressureSize = true;
    bool pressureDensity = false;
    int hardness = 0;
    int falloff = 100;
    int minPressure = 0;
};

class SmudgeSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit SmudgeSettingsWidget(QWidget* parent = nullptr);
    ~SmudgeSettingsWidget() override = default;

    float smudgeSize() const { return static_cast<float>(m_sizeScale->value()); }
    float minSizeRatio() const { return static_cast<float>(m_minSizeScale->value()) / 100.0f; }
    float smudgeDensity() const { return static_cast<float>(m_densityScale->value()) / 100.0f; }
    int rate() const { return static_cast<int>(m_rateScale->value()); }
    bool isPressureSizeEnabled() const { return m_pressureSizeCheck ? m_pressureSizeCheck->isChecked() : true; }
    bool isPressureDensityEnabled() const { return m_pressureDensityCheck ? m_pressureDensityCheck->isChecked() : false; }
    int hardness() const { return static_cast<int>(m_hardnessScale->value()); }
    int falloff() const { return static_cast<int>(m_falloffScale->value()); }
    int minPressure() const { return static_cast<int>(m_minPressureScale->value()); }

    SmudgeConfig config() const {
        SmudgeConfig cfg;
        cfg.size = smudgeSize();
        cfg.minSizeRatio = minSizeRatio();
        cfg.density = smudgeDensity();
        cfg.rate = rate();
        cfg.pressureSize = isPressureSizeEnabled();
        cfg.pressureDensity = isPressureDensityEnabled();
        cfg.hardness = hardness();
        cfg.falloff = falloff();
        cfg.minPressure = minPressure();
        return cfg;
    }

    void setConfig(const SmudgeConfig& cfg);
    void setSmudgeSize(float size);
    void setMinSizeRatio(float ratio);
    void setSmudgeDensity(float density);
    void setRate(int rate);
    void setPressureSizeEnabled(bool enabled);
    void setPressureDensityEnabled(bool enabled);
    void setHardness(int hardness);
    void setFalloff(int falloff);
    void setMinPressure(int minPressure);

signals:
    void smudgeSizeChanged(float size);
    void minSizeChanged(float ratio);
    void densityChanged(float density);
    void rateChanged(int rate);
    void pressureSizeChanged(bool enabled);
    void pressureDensityChanged(bool enabled);
    void hardnessChanged(int hardness);
    void falloffChanged(int falloff);
    void minPressureChanged(int minPressure);

private:
    void createSizePresetButton(float size, int row, int col, QGridLayout* grid);

    SpinScale* m_sizeScale = nullptr;
    SpinScale* m_minSizeScale = nullptr;
    SpinScale* m_densityScale = nullptr;
    SpinScale* m_rateScale = nullptr;
    SpinScale* m_hardnessScale = nullptr;
    SpinScale* m_falloffScale = nullptr;
    SpinScale* m_minPressureScale = nullptr;
    QCheckBox* m_pressureSizeCheck = nullptr;
    QCheckBox* m_pressureDensityCheck = nullptr;
};

} // namespace Kaburagi
