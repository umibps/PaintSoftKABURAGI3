#pragma once

#include <QWidget>
#include <QGridLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include "SpinScaleWidget.hpp"
#include "BlendModes.hpp"

namespace Kaburagi {

struct BrushConfig {
    float size = 10.0f;
    float minSizeRatio = 0.0f;
    float density = 1.0f;
    BlendMode blendMode = BlendMode::Normal;
    bool antialias = true;
    bool pressureSize = true;
    bool pressureDensity = false;
    int hardness = 100;
    int falloff = 0;
    int minPressure = 0;
    int airbrushInterval = 30;

    // 散布 (Scatter)
    bool scatter = false;
    int scatterAmount = 5;
    int scatterRange = 100;
    int scatterSizeRandom = 0;
    bool scatterScaleUp = false;
};

class ScatterSettingsWidget;

class BrushSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit BrushSettingsWidget(QWidget* parent = nullptr);
    ~BrushSettingsWidget() override = default;

    BlendMode blendMode() const;

    float brushSize() const { return static_cast<float>(m_sizeScale->value()); }
    float minSizeRatio() const { return static_cast<float>(m_minSizeScale->value()) / 100.0f; }
    float brushDensity() const { return static_cast<float>(m_densityScale->value()) / 100.0f; }
    bool isAntialias() const { return m_antialiasCheck ? m_antialiasCheck->isChecked() : true; }
    bool isPressureSizeEnabled() const { return m_pressureSizeCheck ? m_pressureSizeCheck->isChecked() : true; }
    bool isPressureDensityEnabled() const { return m_pressureDensityCheck ? m_pressureDensityCheck->isChecked() : false; }
    int hardness() const { return static_cast<int>(m_hardnessScale->value()); }
    int falloff() const { return static_cast<int>(m_falloffScale->value()); }
    int minPressure() const { return static_cast<int>(m_minPressureScale->value()); }
    int airbrushInterval() const { return m_airbrushIntervalScale ? static_cast<int>(m_airbrushIntervalScale->value()) : 30; }

    bool isScatterEnabled() const;
    bool isScatter() const { return isScatterEnabled(); }
    int scatterAmount() const;
    int scatterRange() const;
    int scatterSizeRandom() const;
    bool isScatterScaleUpEnabled() const;
    bool isScatterScaleUp() const { return isScatterScaleUpEnabled(); }

    BrushConfig config() const {
        BrushConfig cfg;
        cfg.size = brushSize();
        cfg.minSizeRatio = minSizeRatio();
        cfg.density = brushDensity();
        cfg.blendMode = blendMode();
        cfg.antialias = isAntialias();
        cfg.pressureSize = isPressureSizeEnabled();
        cfg.pressureDensity = isPressureDensityEnabled();
        cfg.hardness = hardness();
        cfg.falloff = falloff();
        cfg.minPressure = minPressure();
        cfg.airbrushInterval = airbrushInterval();
        cfg.scatter = isScatterEnabled();
        cfg.scatterAmount = scatterAmount();
        cfg.scatterRange = scatterRange();
        cfg.scatterSizeRandom = scatterSizeRandom();
        cfg.scatterScaleUp = isScatterScaleUpEnabled();
        return cfg;
    }

    void setConfig(const BrushConfig& cfg);
    void setBlendMode(BlendMode mode);
    void setBrushSize(float size);
    void setMinSizeRatio(float ratio);
    void setBrushDensity(float density);
    void setAntialias(bool enabled);
    void setPressureSizeEnabled(bool enabled);
    void setPressureDensityEnabled(bool enabled);
    void setHardness(int hardness);
    void setFalloff(int falloff);
    void setMinPressure(int minPressure);
    void setAirbrushInterval(int ms);
    void setAirbrushMode(bool enabled);
    void setEraserMode(bool enabled);

    void setScatterEnabled(bool enabled);
    void setScatterAmount(int amount);
    void setScatterRange(int range);
    void setScatterSizeRandom(int random);
    void setScatterScaleUpEnabled(bool enabled);

signals:
    void blendModeChanged(BlendMode mode);
    void brushSizeChanged(float size);
    void minSizeChanged(float ratio);
    void densityChanged(float density);
    void antialiasChanged(bool enabled);
    void pressureSizeChanged(bool enabled);
    void pressureDensityChanged(bool enabled);
    void hardnessChanged(int hardness);
    void falloffChanged(int falloff);
    void minPressureChanged(int minPressure);
    void airbrushIntervalChanged(int ms);

    void scatterChanged(bool enabled);
    void scatterAmountChanged(int amount);
    void scatterRangeChanged(int range);
    void scatterSizeRandomChanged(int random);
    void scatterScaleUpChanged(bool enabled);

private:
    void createSizePresetButton(float size, int row, int col, QGridLayout* grid);

    QComboBox* m_blendCombo = nullptr;
    SpinScale* m_sizeScale = nullptr;
    SpinScale* m_minSizeScale = nullptr;
    SpinScale* m_densityScale = nullptr;
    SpinScale* m_hardnessScale = nullptr;
    SpinScale* m_falloffScale = nullptr;
    SpinScale* m_minPressureScale = nullptr;
    SpinScale* m_airbrushIntervalScale = nullptr;
    QCheckBox* m_antialiasCheck = nullptr;
    QCheckBox* m_pressureSizeCheck = nullptr;
    QCheckBox* m_pressureDensityCheck = nullptr;
    ScatterSettingsWidget* m_scatterWidget = nullptr;
};

} // namespace Kaburagi
