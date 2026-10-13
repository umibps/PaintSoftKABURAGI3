#pragma once

#include "Layer.hpp"
#include <array>
#include <vector>
#include <QPointF>

namespace Kaburagi {

enum class AdjustmentType {
    BrightnessContrast = 0,
    HueSaturation = 1,
    ToneCurve = 2,
    Levels = 3,
    Invert = 4,
    Grayscale = 5
};

enum class AdjustmentChannel {
    RGB = 0,
    Red = 1,
    Green = 2,
    Blue = 3
};

enum class CurveInterpolationMode {
    Spline = 0,
    Linear = 1
};

struct LevelParams {
    int inShadow = 0;        // 0 ~ 255
    float inGamma = 1.0f;    // 0.10 ~ 9.99
    int inHighlight = 255;   // 0 ~ 255
    int outShadow = 0;       // 0 ~ 255
    int outHighlight = 255;  // 0 ~ 255
};

class AdjustmentLayer : public Layer {
public:
    AdjustmentLayer(const QString& name, int width, int height, AdjustmentType adjType);
    ~AdjustmentLayer() override = default;

    AdjustmentType adjustmentType() const { return m_adjType; }
    void setAdjustmentType(AdjustmentType type) { m_adjType = type; updateLUT(); }

    // 明るさ・コントラスト
    void setBrightnessContrast(float brightness, float contrast);
    float brightness() const { return m_brightness; }
    float contrast() const { return m_contrast; }

    // 色相・彩度・明度
    void setHueSaturation(float hueShift, float saturationRatio, float lightnessShift = 0.0f);
    float hueShift() const { return m_hueShift; }
    float saturation() const { return m_saturation; }
    float lightness() const { return m_lightness; }

    // トーンカーブ
    const std::vector<QPointF>& curvePoints(AdjustmentChannel ch) const {
        return m_curvePoints[static_cast<size_t>(ch)];
    }
    void setCurvePoints(AdjustmentChannel ch, const std::vector<QPointF>& points);
    CurveInterpolationMode curveInterpolationMode() const { return m_curveInterpMode; }
    void setCurveInterpolationMode(CurveInterpolationMode mode);
    void resetCurve(AdjustmentChannel ch);
    void resetAllCurves();

    // レベル補正
    const LevelParams& levelParams(AdjustmentChannel ch) const {
        return m_levelParams[static_cast<size_t>(ch)];
    }
    void setLevelParams(AdjustmentChannel ch, const LevelParams& params);
    void resetLevels(AdjustmentChannel ch);
    void resetAllLevels();

    // 合成処理
    void renderToTarget(TileManager& target, const QRect& dirtyRect, const Layer* baseLayer = nullptr) override;

    // ヒストグラム算出ユーティリティ (R, G, B, 輝度)
    static void computeHistogram(const TileManager& source, const QRect& bounds,
                                 std::array<int, 256>& histR,
                                 std::array<int, 256>& histG,
                                 std::array<int, 256>& histB,
                                 std::array<int, 256>& histLum);

    // 単一チャンネルのトーンカーブ LUT (0〜255) を算出
    static std::array<uint8_t, 256> calculateCurveLUT(const std::vector<QPointF>& points, CurveInterpolationMode mode);
    // 単一チャンネルのレベル補正 LUT (0〜255) を算出
    static std::array<uint8_t, 256> calculateLevelLUT(const LevelParams& params);

private:
    void updateLUT();

    AdjustmentType m_adjType;
    float m_brightness = 0.0f;  // -1.0 ~ 1.0
    float m_contrast = 0.0f;    // -1.0 ~ 1.0
    float m_hueShift = 0.0f;    // -180 ~ 180
    float m_saturation = 1.0f;  // 0.0 ~ 2.0
    float m_lightness = 0.0f;   // -1.0 ~ 1.0

    // トーンカーブ: 各チャンネル (RGB, R, G, B) の制御点リスト (座標 0~255)
    std::array<std::vector<QPointF>, 4> m_curvePoints;
    CurveInterpolationMode m_curveInterpMode = CurveInterpolationMode::Spline;

    // レベル補正: 各チャンネル (RGB, R, G, B) のパラメータ
    std::array<LevelParams, 4> m_levelParams;

    // 高速256階調ルックアップテーブル (R, G, B)
    std::array<uint8_t, 256> m_lutR;
    std::array<uint8_t, 256> m_lutG;
    std::array<uint8_t, 256> m_lutB;
};

} // namespace Kaburagi
