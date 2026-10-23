#pragma once

#include <QWidget>
#include <QComboBox>
#include <QCheckBox>
#include "SpinScaleWidget.hpp"
#include "BlendModes.hpp"

namespace Kaburagi {

enum class GradientShape {
    Linear,    // 線形
    Radial,    // 放射状
    BiLinear,  // 双線形
    Conical    // 円錐状
};

enum class GradientColorMode {
    FgToBg,          // 描画色 → 背景色
    FgToTransparent  // 描画色 → 透明
};

enum class GradientRepeatMode {
    None,       // なし
    Sawtooth,   // ノコギリ波
    Triangular  // 三角波
};

struct GradientConfig {
    GradientShape shape = GradientShape::Linear;
    GradientColorMode colorMode = GradientColorMode::FgToBg;
    GradientRepeatMode repeatMode = GradientRepeatMode::None;
    BlendMode blendMode = BlendMode::Normal;
    float opacity = 1.0f; // 0.01〜1.0
    bool reverse = false;
    bool dither = true;
};

class GradientSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit GradientSettingsWidget(QWidget* parent = nullptr);
    ~GradientSettingsWidget() override = default;

    BlendMode blendMode() const;
    void setBlendMode(BlendMode mode);

    GradientShape shape() const {
        return static_cast<GradientShape>(m_shapeCombo->currentData().toInt());
    }
    GradientColorMode colorMode() const {
        return static_cast<GradientColorMode>(m_colorModeCombo->currentData().toInt());
    }
    GradientRepeatMode repeatMode() const {
        return static_cast<GradientRepeatMode>(m_repeatCombo->currentData().toInt());
    }
    float opacity() const {
        return static_cast<float>(m_opacityScale->value() / 100.0);
    }
    bool isReverse() const {
        return m_reverseCheck ? m_reverseCheck->isChecked() : false;
    }
    bool isDither() const {
        return m_ditherCheck ? m_ditherCheck->isChecked() : true;
    }

    GradientConfig config() const {
        GradientConfig cfg;
        cfg.shape = shape();
        cfg.colorMode = colorMode();
        cfg.repeatMode = repeatMode();
        cfg.blendMode = blendMode();
        cfg.opacity = opacity();
        cfg.reverse = isReverse();
        cfg.dither = isDither();
        return cfg;
    }

    void setConfig(const GradientConfig& cfg);
    void setShape(GradientShape shape);
    void setColorMode(GradientColorMode mode);
    void setRepeatMode(GradientRepeatMode mode);
    void setOpacity(float opacity);
    void setReverse(bool reverse);
    void setDither(bool dither);

    QSize sizeHint() const override {
        return QSize(160, 260);
    }
    QSize minimumSizeHint() const override {
        return QSize(120, 220);
    }

signals:
    void blendModeChanged(BlendMode mode);
    void shapeChanged(GradientShape shape);
    void colorModeChanged(GradientColorMode mode);
    void repeatModeChanged(GradientRepeatMode mode);
    void opacityChanged(float opacity);
    void reverseChanged(bool reverse);
    void ditherChanged(bool dither);

private:
    QComboBox* m_blendCombo = nullptr;
    QComboBox* m_shapeCombo = nullptr;
    QComboBox* m_colorModeCombo = nullptr;
    QComboBox* m_repeatCombo = nullptr;
    SpinScale* m_opacityScale = nullptr;
    QCheckBox* m_reverseCheck = nullptr;
    QCheckBox* m_ditherCheck = nullptr;
};

} // namespace Kaburagi
