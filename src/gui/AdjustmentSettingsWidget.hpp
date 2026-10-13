#pragma once

#include <QWidget>
#include <QStackedWidget>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include "AdjustmentLayer.hpp"
#include "SpinScaleWidget.hpp"
#include "ToneCurveWidget.hpp"
#include "LevelsWidget.hpp"

namespace Kaburagi {

class AdjustmentSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit AdjustmentSettingsWidget(QWidget* parent = nullptr);
    ~AdjustmentSettingsWidget() override = default;

    void setAdjustmentLayer(AdjustmentLayer* layer);
    AdjustmentLayer* adjustmentLayer() const { return m_layer; }

    void setAdjustmentType(AdjustmentType type);
    AdjustmentType adjustmentType() const;

    void updateHistogramFromSource(const TileManager& source, const QRect& bounds);

signals:
    void parametersChanged();

private:
    void setupUi();
    void syncFromLayer();

    AdjustmentLayer* m_layer = nullptr;
    QStackedWidget* m_stack = nullptr;

    // 1. 明るさ・コントラスト
    QWidget* m_brightContrastPage = nullptr;
    SpinScale* m_brightnessSpin = nullptr;
    SpinScale* m_contrastSpin = nullptr;

    // 2. 色相・彩度
    QWidget* m_hueSatPage = nullptr;
    SpinScale* m_hueSpin = nullptr;
    SpinScale* m_saturationSpin = nullptr;
    SpinScale* m_lightnessSpin = nullptr;

    // 3. トーンカーブ
    QWidget* m_curvePage = nullptr;
    QComboBox* m_curveChannelCombo = nullptr;
    QComboBox* m_curveInterpCombo = nullptr;
    ToneCurveWidget* m_curveWidget = nullptr;
    QPushButton* m_btnResetCurve = nullptr;

    // 4. レベル補正
    QWidget* m_levelsPage = nullptr;
    QComboBox* m_levelsChannelCombo = nullptr;
    LevelsWidget* m_levelsWidget = nullptr;
    QSpinBox* m_inShadowSpin = nullptr;
    QDoubleSpinBox* m_inGammaSpin = nullptr;
    QSpinBox* m_inHighlightSpin = nullptr;
    QSpinBox* m_outShadowSpin = nullptr;
    QSpinBox* m_outHighlightSpin = nullptr;
    QPushButton* m_btnResetLevels = nullptr;

    // 5. 階調の反転
    QWidget* m_invertPage = nullptr;

    // 6. 白黒化
    QWidget* m_grayscalePage = nullptr;

    void updateCurveHistogram();
    void updateLevelsHistogram();

    std::array<int, 256> m_histR{};
    std::array<int, 256> m_histG{};
    std::array<int, 256> m_histB{};
    std::array<int, 256> m_histLum{};

    bool m_updatingUi = false;
};

} // namespace Kaburagi
