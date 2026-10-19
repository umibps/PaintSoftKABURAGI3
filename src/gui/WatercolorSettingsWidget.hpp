#pragma once

#include <QWidget>
#include <QGridLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include "SpinScaleWidget.hpp"
#include "BlendModes.hpp"

namespace Kaburagi {

struct WatercolorConfig {
    float size = 20.0f;
    float minSizeRatio = 0.2f;
    float density = 0.7f;
    BlendMode blendMode = BlendMode::Normal;
    int hardness = 50;
    int falloff = 100;
    int minPressure = 0;
    int strength = 50;       // 筆強度 0〜100 (描画色と既にレイヤーに存在する色の比率)
    int water = 50;          // 水分量 0〜100 (不透明度がどれ程上がるか。0が最も上がりやすく、100は上がらない)
    int colorStretch = 50;   // 色延び 0〜100 (どれほどブラシ移動前の色を反映するか)
    int blurPressure = 0;    // ぼかし筆圧 0〜100 (設定筆圧以下なら筆強度を無視して既存色を適用)
    bool pressureSize = true;
    bool pressureDensity = true;
    bool antialias = true;

    // 散布 (Scatter)
    bool scatter = false;
    int scatterAmount = 5;
    int scatterRange = 100;
    int scatterSizeRandom = 0;
    bool scatterScaleUp = false;
};

class ScatterSettingsWidget;

class WatercolorSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit WatercolorSettingsWidget(QWidget* parent = nullptr);
    ~WatercolorSettingsWidget() override = default;

    BlendMode blendMode() const;

    float watercolorSize() const { return static_cast<float>(m_sizeScale->value()); }
    float minSizeRatio() const { return static_cast<float>(m_minSizeScale->value()) / 100.0f; }
    float watercolorDensity() const { return static_cast<float>(m_densityScale->value()) / 100.0f; }
    int hardness() const { return static_cast<int>(m_hardnessScale->value()); }
    int falloff() const { return static_cast<int>(m_falloffScale->value()); }
    int minPressure() const { return static_cast<int>(m_minPressureScale->value()); }
    int strength() const { return static_cast<int>(m_strengthScale->value()); }
    int water() const { return static_cast<int>(m_waterScale->value()); }
    int colorStretch() const { return static_cast<int>(m_colorStretchScale->value()); }
    int blurPressure() const { return static_cast<int>(m_blurPressureScale->value()); }
    bool isPressureSizeEnabled() const { return m_pressureSizeCheck ? m_pressureSizeCheck->isChecked() : true; }
    bool isPressureDensityEnabled() const { return m_pressureDensityCheck ? m_pressureDensityCheck->isChecked() : true; }
    bool isAntialias() const { return m_antialiasCheck ? m_antialiasCheck->isChecked() : true; }
    bool antialias() const { return isAntialias(); }

    bool isScatterEnabled() const;
    bool isScatter() const { return isScatterEnabled(); }
    int scatterAmount() const;
    int scatterRange() const;
    int scatterSizeRandom() const;
    bool isScatterScaleUpEnabled() const;
    bool isScatterScaleUp() const { return isScatterScaleUpEnabled(); }

    WatercolorConfig config() const {
        WatercolorConfig cfg;
        cfg.size = watercolorSize();
        cfg.minSizeRatio = minSizeRatio();
        cfg.density = watercolorDensity();
        cfg.blendMode = blendMode();
        cfg.hardness = hardness();
        cfg.falloff = falloff();
        cfg.minPressure = minPressure();
        cfg.strength = strength();
        cfg.water = water();
        cfg.colorStretch = colorStretch();
        cfg.blurPressure = blurPressure();
        cfg.pressureSize = isPressureSizeEnabled();
        cfg.pressureDensity = isPressureDensityEnabled();
        cfg.antialias = isAntialias();
        cfg.scatter = isScatterEnabled();
        cfg.scatterAmount = scatterAmount();
        cfg.scatterRange = scatterRange();
        cfg.scatterSizeRandom = scatterSizeRandom();
        cfg.scatterScaleUp = isScatterScaleUpEnabled();
        return cfg;
    }

    void setConfig(const WatercolorConfig& cfg);
    void setBlendMode(BlendMode mode);
    void setWatercolorSize(float size);
    float sizeMultiplier() const;
    void setSizeMultiplier(float multiplier);
    void setMinSizeRatio(float ratio);
    void setWatercolorDensity(float density);
    void setHardness(int hardness);
    void setFalloff(int falloff);
    void setMinPressure(int minPressure);
    void setStrength(int strength);
    void setWater(int water);
    void setColorStretch(int stretch);
    void setBlurPressure(int pressure);
    void setPressureSizeEnabled(bool enabled);
    void setPressureDensityEnabled(bool enabled);
    void setAntialias(bool enabled);

    void setScatterEnabled(bool enabled);
    void setScatterAmount(int amount);
    void setScatterRange(int range);
    void setScatterSizeRandom(int random);
    void setScatterScaleUpEnabled(bool enabled);

signals:
    void blendModeChanged(BlendMode mode);
    void watercolorSizeChanged(float size);
    void sizeChanged(float size);
    void sizeMultiplierChanged(float multiplier);
    void minSizeChanged(float ratio);
    void densityChanged(float density);
    void hardnessChanged(int hardness);
    void falloffChanged(int falloff);
    void minPressureChanged(int minPressure);
    void strengthChanged(int strength);
    void waterChanged(int water);
    void colorStretchChanged(int stretch);
    void blurPressureChanged(int pressure);
    void pressureSizeChanged(bool enabled);
    void pressureDensityChanged(bool enabled);
    void antialiasChanged(bool enabled);

    void scatterChanged(bool enabled);
    void scatterAmountChanged(int amount);
    void scatterRangeChanged(int range);
    void scatterSizeRandomChanged(int random);
    void scatterScaleUpChanged(bool enabled);

private:
    void createSizePresetButton(float size, int row, int col, QGridLayout* grid);
    void applySizeMultiplier(int index);

    QComboBox* m_blendCombo = nullptr;
    QComboBox* m_sizeMultiplierCombo = nullptr;
    SpinScale* m_sizeScale = nullptr;
    SpinScale* m_minSizeScale = nullptr;
    SpinScale* m_densityScale = nullptr;
    SpinScale* m_hardnessScale = nullptr;
    SpinScale* m_falloffScale = nullptr;
    SpinScale* m_minPressureScale = nullptr;
    SpinScale* m_strengthScale = nullptr;
    SpinScale* m_waterScale = nullptr;
    SpinScale* m_colorStretchScale = nullptr;
    SpinScale* m_blurPressureScale = nullptr;

    QCheckBox* m_pressureSizeCheck = nullptr;
    QCheckBox* m_pressureDensityCheck = nullptr;
    QCheckBox* m_antialiasCheck = nullptr;
    ScatterSettingsWidget* m_scatterWidget = nullptr;
};

} // namespace Kaburagi
