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
    GradientMapDialog(Document* doc, CanvasWidget* canvas, QWidget* parent = nullptr);

protected:
    void applyFilterToImage(const QImage& src, QImage& out) override;
    QString filterName() const override { return "グラデーションマップ"; }

private:
    QComboBox* m_comboPreset = nullptr;
    QCheckBox* m_chkReverse = nullptr;
    QSlider* m_sliderOpacity = nullptr;
    QSpinBox* m_spinOpacity = nullptr;

    std::vector<std::vector<QPair<float, QColor>>> m_presets;
};

} // namespace Kaburagi

