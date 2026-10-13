#pragma once

#include <QImage>
#include <QPointF>
#include <QPainterPath>
#include <vector>
#include <array>
#include "AdjustmentLayer.hpp"

namespace Kaburagi {

class FilterProcessor {
public:
    // 1. ガウスぼかし (半径 1 ~ 100)
    static QImage applyGaussianBlur(const QImage& src, int radius);

    // 2. 焦点ぼかし (ボケ半径 1~50, フォーカス範囲 0~1, ぼかし推移幅 0~1, 中心点 (0~1, 0~1), チルトシフトフラグ)
    static QImage applyFocusBlur(const QImage& src, int blurRadius,
                                float focusRadiusRatio, float featherRatio,
                                const QPointF& centerNorm, bool isTiltShift = false);

    // 3. 明るさ - コントラスト (-100 ~ +100)
    static QImage applyBrightnessContrast(const QImage& src, float brightness, float contrast);

    // 4. 色相 / 彩度 / 明度 (色相: -180 ~ +180, 彩度: -100 ~ +100, 明度: -100 ~ +100)
    static QImage applyHueSaturationLightness(const QImage& src, float hue, float saturation, float lightness);

    // 5. トーンカーブ
    static QImage applyToneCurve(const QImage& src,
                                const std::array<std::vector<QPointF>, 4>& curves,
                                CurveInterpolationMode mode);

    // 6. モーションぼかし (移動距離 1~100px, 角度 -180°~180°)
    static QImage applyMotionBlur(const QImage& src, int distance, float angleDeg);

    // 7. グラデーションマップ (グラデーションカラーストップ, 反転フラグ, 不透明度 0~1)
    static QImage applyGradientMap(const QImage& src,
                                   const std::vector<QPair<float, QColor>>& stops,
                                   bool reverse = false, float opacity = 1.0f);

    // 選択範囲がある場合のブレンド合成
    static void blendWithSelection(QImage& dst, const QImage& orig,
                                  const QPainterPath& selectionPath, int width, int height);
    static void blendWithSelectionMask(QImage& dst, const QImage& orig,
                                      const class Layer* maskLayer, int width, int height);
};

} // namespace Kaburagi
