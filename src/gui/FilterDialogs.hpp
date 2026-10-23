#pragma once

#include <QDialog>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <vector>
#include <memory>
#include "Document.hpp"
#include "CanvasWidget.hpp"
#include "FilterProcessor.hpp"
#include "ToneCurveWidget.hpp"

namespace Kaburagi {

/**
 * @brief フィルター設定ダイアログ基底クラス (プレビュー・ピン留めレイヤー一括適用・Undo対応)
 */
class FilterBaseDialog : public QDialog {
    Q_OBJECT
public:
    FilterBaseDialog(Document* doc, CanvasWidget* canvas, const QString& title, QWidget* parent = nullptr);
    ~FilterBaseDialog() override;

    bool hasTargets() const { return !m_targets.empty(); }

public slots:
    void accept() override;
    void reject() override;
    void updatePreview();

protected:
    virtual void applyFilterToImage(const QImage& src, QImage& out) = 0;
    virtual QString filterName() const = 0;

    void setupBaseUi(QWidget* controlsWidget);

    Document* m_document = nullptr;
    CanvasWidget* m_canvas = nullptr;

    struct TargetLayerInfo {
        int layerIndex;
        std::shared_ptr<Layer> layer;
        TileManager originalTiles;
        QImage originalImage;
    };
    std::vector<TargetLayerInfo> m_targets;

    QCheckBox* m_chkPreview = nullptr;
    QDialogButtonBox* m_buttonBox = nullptr;
    bool m_isApplying = false;
};

/**
 * @brief ガウスぼかし設定ダイアログ
 */
class GaussianBlurDialog : public FilterBaseDialog {
    Q_OBJECT
public:
    GaussianBlurDialog(Document* doc, CanvasWidget* canvas, QWidget* parent = nullptr);
    void accept() override;

    int radius() const;
    void setRadius(int r);

protected:
    void applyFilterToImage(const QImage& src, QImage& out) override;
    QString filterName() const override { return "ガウスぼかし"; }

private:
    QSlider* m_sliderRadius = nullptr;
    QSpinBox* m_spinRadius = nullptr;
};

/**
 * @brief 焦点ぼかし設定ダイアログ
 */
class FocusBlurDialog : public FilterBaseDialog {
    Q_OBJECT
public:
    FocusBlurDialog(Document* doc, CanvasWidget* canvas, QWidget* parent = nullptr);
    void accept() override;

    int blurRadius() const;
    void setBlurRadius(int r);
    int focusRange() const;
    void setFocusRange(int r);
    int feather() const;
    void setFeather(int f);

protected:
    void applyFilterToImage(const QImage& src, QImage& out) override;
    QString filterName() const override { return "焦点ぼかし"; }

private:
    QSlider* m_sliderBlurRadius = nullptr;
    QSpinBox* m_spinBlurRadius = nullptr;
    QSlider* m_sliderFocusRange = nullptr;
    QSpinBox* m_spinFocusRange = nullptr;
    QSlider* m_sliderFeather = nullptr;
    QSpinBox* m_spinFeather = nullptr;
    QComboBox* m_comboType = nullptr; // 0: 放射状 (円形), 1: 水平 (チルトシフト)
    QSlider* m_sliderCenterX = nullptr;
    QSlider* m_sliderCenterY = nullptr;
};

/**
 * @brief 明るさ - コントラスト設定ダイアログ
 */
class BrightnessContrastDialog : public FilterBaseDialog {
    Q_OBJECT
public:
    BrightnessContrastDialog(Document* doc, CanvasWidget* canvas, QWidget* parent = nullptr);
    void accept() override;

    int brightness() const;
    void setBrightness(int b);
    int contrast() const;
    void setContrast(int c);

protected:
    void applyFilterToImage(const QImage& src, QImage& out) override;
    QString filterName() const override { return "明るさ・コントラスト"; }

private:
    QSlider* m_sliderBrightness = nullptr;
    QSpinBox* m_spinBrightness = nullptr;
    QSlider* m_sliderContrast = nullptr;
    QSpinBox* m_spinContrast = nullptr;
};

/**
 * @brief 色相 / 彩度 / 明度設定ダイアログ
 */
class HueSaturationDialog : public FilterBaseDialog {
    Q_OBJECT
public:
    HueSaturationDialog(Document* doc, CanvasWidget* canvas, QWidget* parent = nullptr);
    void accept() override;

    int hue() const;
    void setHue(int h);
    int saturation() const;
    void setSaturation(int s);
    int lightness() const;
    void setLightness(int l);

protected:
    void applyFilterToImage(const QImage& src, QImage& out) override;
    QString filterName() const override { return "色相・彩度・明度"; }

private:
    QSlider* m_sliderHue = nullptr;
    QSpinBox* m_spinHue = nullptr;
    QSlider* m_sliderSat = nullptr;
    QSpinBox* m_spinSat = nullptr;
    QSlider* m_sliderLight = nullptr;
    QSpinBox* m_spinLight = nullptr;
};

/**
 * @brief トーンカーブ設定ダイアログ
 */
class ToneCurveDialog : public FilterBaseDialog {
    Q_OBJECT
public:
    ToneCurveDialog(Document* doc, CanvasWidget* canvas, QWidget* parent = nullptr);
    void accept() override;

    int channelIndex() const;
    void setChannelIndex(int idx);

protected:
    void applyFilterToImage(const QImage& src, QImage& out) override;
    QString filterName() const override { return "トーンカーブ"; }

private:
    ToneCurveWidget* m_curveWidget = nullptr;
    QComboBox* m_channelCombo = nullptr;
    std::array<std::vector<QPointF>, 4> m_curves;
};

/**
 * @brief モーションぼかし設定ダイアログ
 */
class MotionBlurDialog : public FilterBaseDialog {
    Q_OBJECT
public:
    MotionBlurDialog(Document* doc, CanvasWidget* canvas, QWidget* parent = nullptr);
    void accept() override;

    int distance() const;
    void setDistance(int d);
    int angle() const;
    void setAngle(int a);

protected:
    void applyFilterToImage(const QImage& src, QImage& out) override;
    QString filterName() const override { return "モーションぼかし"; }

private:
    QSlider* m_sliderDistance = nullptr;
    QSpinBox* m_spinDistance = nullptr;
    QSlider* m_sliderAngle = nullptr;
    QSpinBox* m_spinAngle = nullptr;
};

/**
 * @brief グラデーションマップ設定ダイアログ
 */
class GradientMapDialog : public FilterBaseDialog {
    Q_OBJECT
public:
    enum class GradientMode {
        ForegroundBackground = 0, // 描画色⇔背景色
        CustomTwoColors = 1,      // 選択2色
        Preset = 2                // プリセット
    };

    GradientMapDialog(Document* doc, CanvasWidget* canvas, QWidget* parent = nullptr);
    void accept() override;

    GradientMode mode() const;
    void setMode(GradientMode mode);

    int modeIndex() const;
    void setModeIndex(int idx);

    QColor color1() const { return m_color1; }
    void setColor1(const QColor& c);

    QColor color2() const { return m_color2; }
    void setColor2(const QColor& c);

protected:
    void applyFilterToImage(const QImage& src, QImage& out) override;
    QString filterName() const override { return "グラデーションマップ"; }

private slots:
    void onColor1Clicked();
    void onColor2Clicked();

private:
    void updateColorButtonStyles();
    void updateModeVisibility();

    QComboBox* m_comboPreset = nullptr;
    QLabel* m_lblColorSelect = nullptr;
    QWidget* m_colorSelectWidget = nullptr;
    QPushButton* m_btnColor1 = nullptr;
    QPushButton* m_btnColor2 = nullptr;

    QCheckBox* m_chkReverse = nullptr;
    QSlider* m_sliderOpacity = nullptr;
    QSpinBox* m_spinOpacity = nullptr;

    QColor m_color1 = QColor(0, 0, 0);
    QColor m_color2 = QColor(255, 255, 255);

    std::vector<std::vector<QPair<float, QColor>>> m_presets;
};

/**
 * @brief 色を透明度に (Color to Alpha) 設定ダイアログ (GIMP互換)
 */
class ColorToAlphaDialog : public FilterBaseDialog {
    Q_OBJECT
public:
    ColorToAlphaDialog(Document* doc, CanvasWidget* canvas, QWidget* parent = nullptr);
    void accept() override;

    QColor targetColor() const { return m_targetColor; }
    void setTargetColor(const QColor& color);

    float transparencyThreshold() const;
    float opacityThreshold() const;

protected:
    void applyFilterToImage(const QImage& src, QImage& out) override;
    QString filterName() const override { return tr("Color to Alpha"); }

private slots:
    void onPickColorClicked();
    void onFromFgColorClicked();

private:
    QColor m_targetColor = Qt::white;
    QPushButton* m_btnColor = nullptr;
    QPushButton* m_btnFromFg = nullptr;
    QSlider* m_sliderTransThreshold = nullptr;
    QDoubleSpinBox* m_spinTransThreshold = nullptr;
    QSlider* m_sliderOpacityThreshold = nullptr;
    QDoubleSpinBox* m_spinOpacityThreshold = nullptr;

    void updateColorButtonStyle();
};

} // namespace Kaburagi

