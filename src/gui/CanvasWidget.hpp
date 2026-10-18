#pragma once

#include <QWidget>
#include <QTransform>
#include <QTimer>
#include <unordered_map>
#include <optional>
#include "Document.hpp"
#include "ToolBoxWidget.hpp"
#include "StrokeSmoother.hpp"
#include "TextLayer.hpp"
#include "RectSelectSettingsWidget.hpp"
#include "EllipseSelectSettingsWidget.hpp"
#include "LassoSelectSettingsWidget.hpp"
#include "ColorSelectSettingsWidget.hpp"
#include "BucketSettingsWidget.hpp"
#include "GradientSettingsWidget.hpp"
#include "CompositeBrushSettingsWidget.hpp"
#include "VectorLayer.hpp"
#include "TransformSettingsWidget.hpp"
#include "core/BlendModes.hpp"
#include "PreferencesDialog.hpp"

namespace Kaburagi {

class TextSettingsWidget;
class PanelSettingsWidget;
class ThreeDSettingsWidget;

class CanvasWidget : public QWidget {
    Q_OBJECT
public:
    explicit CanvasWidget(QWidget* parent = nullptr);
    ~CanvasWidget() override;

    void setTextSettingsWidget(TextSettingsWidget* settings) { m_textSettings = settings; }
    int selectedBalloonIndex() const { return m_selectedBalloonIndex; }

    void setPanelSettingsWidget(PanelSettingsWidget* settings) { m_panelSettings = settings; }
    void applyPanelSettingsToSelected();

    void setThreeDSettingsWidget(ThreeDSettingsWidget* settings) { m_threeDSettings = settings; }

    void setDocument(Document* doc);
    Document* document() const { return m_document; }

    // ビュー変換
    double zoom() const { return m_zoom; }
    void setZoom(double z);
    double rotationAngle() const { return m_rotationAngle; }
    void setRotationAngle(double angleDeg);
    void toggleFlipHorizontally();
    void resetTransform();
    void fitToView();
    void setViewCenter(const QPointF& docCenter);
    void panByScreenDelta(double dx, double dy);
    QPointF panOffset() const { return m_panOffset; }

    QPointF viewToDoc(const QPointF& viewPos) const;
    QPointF docToView(const QPointF& docPos) const;

    // ツール・ブラシプロパティ
    void setCurrentTool(ToolType tool) {
        if (m_currentTool != tool) {
            if (m_airbrushTimer && m_airbrushTimer->isActive()) {
                m_airbrushTimer->stop();
            }
            if (m_currentTool == ToolType::Transform && m_isTransforming) {
                applyTransform();
            }
            m_currentTool = tool;
            if (m_currentTool == ToolType::Transform && !m_isTransforming) {
                startTransform();
            }
            updateCursorForActiveLayer();
            updateAntsTimer();
            update();
        }
    }
    void setCurrentColor(const QColor& color) { m_currentColor = color; }
    QColor currentColor() const { return m_currentColor; }
    void setSecondaryColor(const QColor& color) { m_secondaryColor = color; }
    QColor secondaryColor() const { return m_secondaryColor; }
    void setBrushSize(float size) {
        m_brushSize = size;
        if (m_showBrushOutline) update();
    }
    void setMinSizeRatio(float ratio) { m_minSizeRatio = ratio; }
    void setBrushDensity(float density) { m_brushDensity = density; }
    void setSmoothLevel(int level) { m_smoother.setSmoothLevel(level); }
    void setAntialias(bool enabled) { m_antialias = enabled; }
    bool isAntialias() const { return m_antialias; }
    void setPressureSizeEnabled(bool enabled) { m_pressureSizeEnabled = enabled; }
    bool isPressureSizeEnabled() const { return m_pressureSizeEnabled; }
    void setPressureDensityEnabled(bool enabled) { m_pressureDensityEnabled = enabled; }
    bool isPressureDensityEnabled() const { return m_pressureDensityEnabled; }
    void setBrushHardness(int h) { m_brushHardness = h; }
    int brushHardness() const { return m_brushHardness; }
    void setBrushFalloff(int f) { m_brushFalloff = f; }
    int brushFalloff() const { return m_brushFalloff; }
    void setBrushMinPressure(int p) { m_brushMinPressure = p; }
    int brushMinPressure() const { return m_brushMinPressure; }
    void setBrushBlendMode(BlendMode mode) { m_brushBlendMode = mode; }
    BlendMode brushBlendMode() const { return m_brushBlendMode; }
    bool isDrawing() const { return m_isDrawing; }

    // 右クリックスポイト設定 (通常レイヤー用ツール使用時)
    void setRightClickEyedropperEnabled(bool enabled) { m_rightClickEyedropperEnabled = enabled; }
    bool isRightClickEyedropperEnabled() const { return m_rightClickEyedropperEnabled; }
    bool isRasterDrawingTool(ToolType tool) const;
    bool isRasterBrushTool(ToolType tool) const;
    void drawStraightLine(const QPointF& startPos, const QPointF& endPos);

    // タッチ操作設定
    using TouchAction = PreferencesDialog::TouchAction;
    void setTouchAction(TouchAction action) { m_touchAction = action; }
    TouchAction touchAction() const { return m_touchAction; }

    // ブラシ散布設定
    void setBrushScatter(bool s) { m_brushScatter = s; }
    bool isBrushScatter() const { return m_brushScatter; }
    void setBrushScatterAmount(int a) { m_brushScatterAmount = std::clamp(a, 1, 20); }
    int brushScatterAmount() const { return m_brushScatterAmount; }
    void setBrushScatterRange(int r) { m_brushScatterRange = std::clamp(r, 1, 500); }
    int brushScatterRange() const { return m_brushScatterRange; }
    void setBrushScatterSizeRandom(int sr) { m_brushScatterSizeRandom = std::clamp(sr, 0, 100); }
    int brushScatterSizeRandom() const { return m_brushScatterSizeRandom; }
    void setBrushScatterScaleUp(bool su) { m_brushScatterScaleUp = su; }
    bool isBrushScatterScaleUp() const { return m_brushScatterScaleUp; }

    // 矩形選択ツール設定
    void setSelectionMode(SelectionMode mode) { m_selectionMode = mode; }
    SelectionMode selectionMode() const { return m_selectionMode; }
    void setSelectionAlignToWindow(bool align) { m_selectionAlignToWindow = align; }
    bool isSelectionAlignToWindow() const { return m_selectionAlignToWindow; }

    // 楕円選択ツール設定
    void setEllipseSelectMode(SelectionMode mode) { m_ellipseSelectMode = mode; }
    SelectionMode ellipseSelectMode() const { return m_ellipseSelectMode; }
    void setEllipseSelectMethod(EllipseSelectMethod method) { m_ellipseSelectMethod = method; }
    EllipseSelectMethod ellipseSelectMethod() const { return m_ellipseSelectMethod; }

    // 投げ縄ツール設定
    void setLassoSelectMode(SelectionMode mode) { m_lassoSelectMode = mode; }
    SelectionMode lassoSelectMode() const { return m_lassoSelectMode; }

    // 色域選択ツール設定
    void setColorSelectMode(SelectionMode mode) { m_colorSelectMode = mode; }
    SelectionMode colorSelectMode() const { return m_colorSelectMode; }
    void setColorSelectReference(ColorSelectReference ref) { m_colorSelectReference = ref; }
    ColorSelectReference colorSelectReference() const { return m_colorSelectReference; }
    void setColorSelectTolerance(int tol) { m_colorSelectTolerance = tol; }
    int colorSelectTolerance() const { return m_colorSelectTolerance; }
    void setColorSelectTransparent(bool enable) { m_colorSelectTransparent = enable; }
    bool isColorSelectTransparent() const { return m_colorSelectTransparent; }
    void executeColorSelection(const QPointF& docPos);

    // バケツツール設定
    void setBucketBlendMode(BlendMode mode) { m_bucketBlendMode = mode; }
    BlendMode bucketBlendMode() const { return m_bucketBlendMode; }
    void setBucketDensity(float density) { m_bucketDensity = std::clamp(density, 0.0f, 1.0f); }
    float bucketDensity() const { return m_bucketDensity; }
    void setBucketReference(BucketReference ref) { m_bucketReference = ref; }
    BucketReference bucketReference() const { return m_bucketReference; }
    void setBucketExpandPixels(int px) { m_bucketExpandPixels = px; }
    int bucketExpandPixels() const { return m_bucketExpandPixels; }
    void setBucketNarrowAreaFill(bool enable) { m_bucketNarrowAreaFill = enable; }
    bool bucketNarrowAreaFill() const { return m_bucketNarrowAreaFill; }
    void setBucketTolerance(int tol) { m_bucketTolerance = tol; }
    int bucketTolerance() const { return m_bucketTolerance; }
    void fillBucket(const QPointF& docPos);

    // パターン塗りつぶしツール設定
    void setPatternFillBlendMode(BlendMode mode) { m_patternFillBlendMode = mode; }
    BlendMode patternFillBlendMode() const { return m_patternFillBlendMode; }
    void setPatternFillDensity(float density) { m_patternFillDensity = std::clamp(density, 0.0f, 1.0f); }
    float patternFillDensity() const { return m_patternFillDensity; }
    void setPatternFillScale(float scale) { m_patternFillScale = std::clamp(scale, 0.1f, 5.0f); }
    float patternFillScale() const { return m_patternFillScale; }
    void setPatternFillAngle(float angle) { m_patternFillAngle = angle; }
    float patternFillAngle() const { return m_patternFillAngle; }
    void setPatternFillImage(const QImage& img) { m_patternFillImage = img; }
    QImage patternFillImage() const { return m_patternFillImage; }
    void setPatternFillReference(BucketReference ref) { m_patternFillReference = ref; }
    BucketReference patternFillReference() const { return m_patternFillReference; }
    void setPatternFillExpandPixels(int px) { m_patternFillExpandPixels = px; }
    int patternFillExpandPixels() const { return m_patternFillExpandPixels; }
    void setPatternFillNarrowAreaFill(bool enable) { m_patternFillNarrowAreaFill = enable; }
    bool patternFillNarrowAreaFill() const { return m_patternFillNarrowAreaFill; }
    void setPatternFillTolerance(int tol) { m_patternFillTolerance = tol; }
    int patternFillTolerance() const { return m_patternFillTolerance; }
    void fillPattern(const QPointF& docPos);

    // 自動選択ツール設定
    void setAutoSelectMode(SelectionMode mode) { m_autoSelectMode = mode; }
    SelectionMode autoSelectMode() const { return m_autoSelectMode; }
    void setAutoSelectReference(BucketReference ref) { m_autoSelectReference = ref; }
    BucketReference autoSelectReference() const { return m_autoSelectReference; }
    void setAutoSelectExpandPixels(int px) { m_autoSelectExpandPixels = px; }
    int autoSelectExpandPixels() const { return m_autoSelectExpandPixels; }
    void setAutoSelectNarrowAreaFill(bool enable) { m_autoSelectNarrowAreaFill = enable; }
    bool autoSelectNarrowAreaFill() const { return m_autoSelectNarrowAreaFill; }
    void setAutoSelectTolerance(int tol) { m_autoSelectTolerance = tol; }
    int autoSelectTolerance() const { return m_autoSelectTolerance; }
    void autoSelect(const QPointF& docPos);

    // ぼかしツール設定
    void setBlurSize(float size) { m_blurSize = size; update(); }
    float blurSize() const { return m_blurSize; }
    void setBlurMinSizeRatio(float ratio) { m_blurMinSizeRatio = ratio; }
    float blurMinSizeRatio() const { return m_blurMinSizeRatio; }
    void setBlurDensity(float density) { m_blurDensity = density; }
    float blurDensity() const { return m_blurDensity; }
    void setBlurPressureSizeEnabled(bool enabled) { m_blurPressureSizeEnabled = enabled; }
    bool isBlurPressureSizeEnabled() const { return m_blurPressureSizeEnabled; }
    void setBlurPressureDensityEnabled(bool enabled) { m_blurPressureDensityEnabled = enabled; }
    bool isBlurPressureDensityEnabled() const { return m_blurPressureDensityEnabled; }
    void setBlurColorMix(int mix) { m_blurColorMix = mix; }
    int blurColorMix() const { return m_blurColorMix; }
    void setBlurColorStretch(int stretch) { m_blurColorStretch = stretch; }
    int blurColorStretch() const { return m_blurColorStretch; }
    void setBlurHardness(int h) { m_blurHardness = h; }
    int blurHardness() const { return m_blurHardness; }
    void setBlurFalloff(int f) { m_blurFalloff = f; }
    int blurFalloff() const { return m_blurFalloff; }
    void setBlurMinPressure(int p) { m_blurMinPressure = p; }
    int blurMinPressure() const { return m_blurMinPressure; }

    // 指先ツール設定
    void setSmudgeSize(float size) { m_smudgeSize = size; update(); }
    float smudgeSize() const { return m_smudgeSize; }
    void setSmudgeMinSizeRatio(float ratio) { m_smudgeMinSizeRatio = ratio; }
    float smudgeMinSizeRatio() const { return m_smudgeMinSizeRatio; }
    void setSmudgeDensity(float density) { m_smudgeDensity = density; }
    float smudgeDensity() const { return m_smudgeDensity; }
    void setSmudgeRate(int rate) { m_smudgeRate = rate; }
    int smudgeRate() const { return m_smudgeRate; }
    void setSmudgePressureSizeEnabled(bool enabled) { m_smudgePressureSizeEnabled = enabled; }
    bool isSmudgePressureSizeEnabled() const { return m_smudgePressureSizeEnabled; }
    void setSmudgePressureDensityEnabled(bool enabled) { m_smudgePressureDensityEnabled = enabled; }
    bool isSmudgePressureDensityEnabled() const { return m_smudgePressureDensityEnabled; }
    void setSmudgeHardness(int h) { m_smudgeHardness = h; }
    int smudgeHardness() const { return m_smudgeHardness; }
    void setSmudgeFalloff(int f) { m_smudgeFalloff = f; }
    int smudgeFalloff() const { return m_smudgeFalloff; }
    void setSmudgeMinPressure(int p) { m_smudgeMinPressure = p; }
    int smudgeMinPressure() const { return m_smudgeMinPressure; }

    // 水彩筆ツール設定
    void setWatercolorSize(float size) { m_watercolorSize = size; update(); }
    float watercolorSize() const { return m_watercolorSize; }
    void setWatercolorMinSizeRatio(float ratio) { m_watercolorMinSizeRatio = ratio; }
    float watercolorMinSizeRatio() const { return m_watercolorMinSizeRatio; }
    void setWatercolorDensity(float density) { m_watercolorDensity = density; }
    float watercolorDensity() const { return m_watercolorDensity; }
    void setWatercolorHardness(int h) { m_watercolorHardness = h; }
    int watercolorHardness() const { return m_watercolorHardness; }
    void setWatercolorFalloff(int f) { m_watercolorFalloff = f; }
    int watercolorFalloff() const { return m_watercolorFalloff; }
    void setWatercolorMinPressure(int p) { m_watercolorMinPressure = p; }
    int watercolorMinPressure() const { return m_watercolorMinPressure; }
    void setWatercolorStrength(int s) { m_watercolorStrength = s; }
    int watercolorStrength() const { return m_watercolorStrength; }
    void setWatercolorWater(int w) { m_watercolorWater = w; }
    int watercolorWater() const { return m_watercolorWater; }
    void setWatercolorColorStretch(int stretch) { m_watercolorColorStretch = stretch; }
    int watercolorColorStretch() const { return m_watercolorColorStretch; }
    void setWatercolorBlurPressure(int p) { m_watercolorBlurPressure = p; }
    int watercolorBlurPressure() const { return m_watercolorBlurPressure; }
    void setWatercolorPressureSizeEnabled(bool enabled) { m_watercolorPressureSizeEnabled = enabled; }
    bool isWatercolorPressureSizeEnabled() const { return m_watercolorPressureSizeEnabled; }
    void setWatercolorPressureDensityEnabled(bool enabled) { m_watercolorPressureDensityEnabled = enabled; }
    bool isWatercolorPressureDensityEnabled() const { return m_watercolorPressureDensityEnabled; }
    void setWatercolorAntialias(bool enabled) { m_watercolorAntialias = enabled; }
    bool isWatercolorAntialias() const { return m_watercolorAntialias; }
    void setWatercolorBlendMode(BlendMode mode) { m_watercolorBlendMode = mode; }
    BlendMode watercolorBlendMode() const { return m_watercolorBlendMode; }
    void setWatercolorScatter(bool s) { m_watercolorScatter = s; }
    bool isWatercolorScatter() const { return m_watercolorScatter; }
    void setWatercolorScatterAmount(int a) { m_watercolorScatterAmount = std::clamp(a, 1, 20); }
    int watercolorScatterAmount() const { return m_watercolorScatterAmount; }
    void setWatercolorScatterRange(int r) { m_watercolorScatterRange = std::clamp(r, 1, 500); }
    int watercolorScatterRange() const { return m_watercolorScatterRange; }
    void setWatercolorScatterSizeRandom(int sr) { m_watercolorScatterSizeRandom = std::clamp(sr, 0, 100); }
    int watercolorScatterSizeRandom() const { return m_watercolorScatterSizeRandom; }
    void setWatercolorScatterScaleUp(bool su) { m_watercolorScatterScaleUp = su; }
    bool isWatercolorScatterScaleUp() const { return m_watercolorScatterScaleUp; }

    // 合成ブラシツール設定
    void setCompositeBrushReference(CompositeBrushReference ref) { m_compositeBrushReference = ref; }
    CompositeBrushReference compositeBrushReference() const { return m_compositeBrushReference; }
    void setCompositeBrushSize(float size) { m_compositeBrushSize = size; update(); }
    float compositeBrushSize() const { return m_compositeBrushSize; }
    void setCompositeBrushMinSizeRatio(float ratio) { m_compositeBrushMinSizeRatio = ratio; }
    float compositeBrushMinSizeRatio() const { return m_compositeBrushMinSizeRatio; }
    void setCompositeBrushDensity(float density) { m_compositeBrushDensity = density; }
    float compositeBrushDensity() const { return m_compositeBrushDensity; }
    void setCompositeBrushHardness(int h) { m_compositeBrushHardness = h; }
    int compositeBrushHardness() const { return m_compositeBrushHardness; }
    void setCompositeBrushFalloff(int f) { m_compositeBrushFalloff = f; }
    int compositeBrushFalloff() const { return m_compositeBrushFalloff; }
    void setCompositeBrushMinPressure(int p) { m_compositeBrushMinPressure = p; }
    int compositeBrushMinPressure() const { return m_compositeBrushMinPressure; }
    void setCompositeBrushStrength(int s) { m_compositeBrushStrength = s; }
    int compositeBrushStrength() const { return m_compositeBrushStrength; }
    void setCompositeBrushWater(int w) { m_compositeBrushWater = w; }
    int compositeBrushWater() const { return m_compositeBrushWater; }
    void setCompositeBrushColorStretch(int stretch) { m_compositeBrushColorStretch = stretch; }
    int compositeBrushColorStretch() const { return m_compositeBrushColorStretch; }
    void setCompositeBrushBlurPressure(int p) { m_compositeBrushBlurPressure = p; }
    int compositeBrushBlurPressure() const { return m_compositeBrushBlurPressure; }
    void setCompositeBrushPressureSizeEnabled(bool enabled) { m_compositeBrushPressureSizeEnabled = enabled; }
    bool isCompositeBrushPressureSizeEnabled() const { return m_compositeBrushPressureSizeEnabled; }
    void setCompositeBrushPressureDensityEnabled(bool enabled) { m_compositeBrushPressureDensityEnabled = enabled; }
    bool isCompositeBrushPressureDensityEnabled() const { return m_compositeBrushPressureDensityEnabled; }
    void setCompositeBrushAntialias(bool enabled) { m_compositeBrushAntialias = enabled; }
    bool isCompositeBrushAntialias() const { return m_compositeBrushAntialias; }
    void setCompositeBrushBlendMode(BlendMode mode) { m_compositeBrushBlendMode = mode; }
    BlendMode compositeBrushBlendMode() const { return m_compositeBrushBlendMode; }
    void setCompositeBrushScatter(bool s) { m_compositeBrushScatter = s; }
    bool isCompositeBrushScatter() const { return m_compositeBrushScatter; }
    void setCompositeBrushScatterAmount(int a) { m_compositeBrushScatterAmount = std::clamp(a, 1, 20); }
    int compositeBrushScatterAmount() const { return m_compositeBrushScatterAmount; }
    void setCompositeBrushScatterRange(int r) { m_compositeBrushScatterRange = std::clamp(r, 1, 500); }
    int compositeBrushScatterRange() const { return m_compositeBrushScatterRange; }
    void setCompositeBrushScatterSizeRandom(int sr) { m_compositeBrushScatterSizeRandom = std::clamp(sr, 0, 100); }
    int compositeBrushScatterSizeRandom() const { return m_compositeBrushScatterSizeRandom; }
    void setCompositeBrushScatterScaleUp(bool su) { m_compositeBrushScatterScaleUp = su; }
    bool isCompositeBrushScatterScaleUp() const { return m_compositeBrushScatterScaleUp; }

    // 画像ブラシ設定
    void setImageBrushImage(const QImage& img);
    QImage imageBrushImage() const { return m_imageBrushImage; }
    void updateImageBrushOutline();
    const QPainterPath& imageBrushOutlinePath() const { return m_imageBrushOutlinePath; }
    float imageBrushLastFollowAngle() const { return m_imageBrushLastFollowAngle; }
    void setImageBrushSize(float sz) { m_imageBrushSize = sz; update(); }
    float imageBrushSize() const { return m_imageBrushSize; }
    void setImageBrushMinSizeRatio(float r) { m_imageBrushMinSizeRatio = r; }
    float imageBrushMinSizeRatio() const { return m_imageBrushMinSizeRatio; }
    void setImageBrushSpacing(int sp) { m_imageBrushSpacing = std::clamp(sp, 1, 500); }
    int imageBrushSpacing() const { return m_imageBrushSpacing; }
    void setImageBrushDensity(float d) { m_imageBrushDensity = d; }
    float imageBrushDensity() const { return m_imageBrushDensity; }
    void setImageBrushMinPressure(int p) { m_imageBrushMinPressure = std::clamp(p, 0, 100); }
    int imageBrushMinPressure() const { return m_imageBrushMinPressure; }
    void setImageBrushPressureSizeEnabled(bool en) { m_imageBrushPressureSizeEnabled = en; }
    bool isImageBrushPressureSizeEnabled() const { return m_imageBrushPressureSizeEnabled; }
    void setImageBrushPressureDensityEnabled(bool en) { m_imageBrushPressureDensityEnabled = en; }
    bool isImageBrushPressureDensityEnabled() const { return m_imageBrushPressureDensityEnabled; }
    void setImageBrushBlendMode(BlendMode mode) { m_imageBrushBlendMode = mode; }
    BlendMode imageBrushBlendMode() const { return m_imageBrushBlendMode; }

    void setImageBrushScatter(bool s) { m_imageBrushScatter = s; }
    bool isImageBrushScatter() const { return m_imageBrushScatter; }
    void setImageBrushScatterAmount(int a) { m_imageBrushScatterAmount = std::clamp(a, 1, 20); }
    int imageBrushScatterAmount() const { return m_imageBrushScatterAmount; }
    void setImageBrushScatterRange(int r) { m_imageBrushScatterRange = std::clamp(r, 1, 500); }
    int imageBrushScatterRange() const { return m_imageBrushScatterRange; }
    void setImageBrushScatterSizeRandom(int sr) { m_imageBrushScatterSizeRandom = std::clamp(sr, 0, 100); }
    int imageBrushScatterSizeRandom() const { return m_imageBrushScatterSizeRandom; }
    void setImageBrushScatterScaleUp(bool su) { m_imageBrushScatterScaleUp = su; }
    bool isImageBrushScatterScaleUp() const { return m_imageBrushScatterScaleUp; }

    void setImageBrushFlipH(bool flip) { m_imageBrushFlipH = flip; update(); }
    bool isImageBrushFlipH() const { return m_imageBrushFlipH; }
    void setImageBrushFlipV(bool flip) { m_imageBrushFlipV = flip; update(); }
    bool isImageBrushFlipV() const { return m_imageBrushFlipV; }
    void setImageBrushAngle(int a) { m_imageBrushAngle = a % 360; update(); }
    int imageBrushAngle() const { return m_imageBrushAngle; }
    void setImageBrushFollowCursor(bool follow) { m_imageBrushFollowCursor = follow; update(); }
    bool isImageBrushFollowCursor() const { return m_imageBrushFollowCursor; }
    void setImageBrushRandomRotate(bool randRot) { m_imageBrushRandomRotate = randRot; }
    bool isImageBrushRandomRotate() const { return m_imageBrushRandomRotate; }
    void setImageBrushRandomRotateAmount(int amt) { m_imageBrushRandomRotateAmount = std::clamp(amt, 0, 360); }
    int imageBrushRandomRotateAmount() const { return m_imageBrushRandomRotateAmount; }
    void setImageBrushRandomSize(bool randSz) { m_imageBrushRandomSize = randSz; }
    bool isImageBrushRandomSize() const { return m_imageBrushRandomSize; }
    void setImageBrushRandomSizeAmount(int amt) { m_imageBrushRandomSizeAmount = std::clamp(amt, 0, 100); }
    int imageBrushRandomSizeAmount() const { return m_imageBrushRandomSizeAmount; }
    void setImageBrushRandomSizeScaleUp(bool su) { m_imageBrushRandomSizeScaleUp = su; }
    bool isImageBrushRandomSizeScaleUp() const { return m_imageBrushRandomSizeScaleUp; }

    float currentBrushOutlineRadius() const;

    // ベクター消しゴム設定
    void setVectorEraserMode(VectorEraserMode mode) { m_vectorEraserMode = mode; }
    VectorEraserMode vectorEraserMode() const { return m_vectorEraserMode; }
    void setVectorEraserSize(float size) { m_vectorEraserSize = size; update(); }
    float vectorEraserSize() const { return m_vectorEraserSize; }

    // ベクター制御点ツール設定
    void setVectorControlPointMode(VectorControlPointMode mode) {
        m_vectorControlPointMode = mode;
        m_isDraggingControlPoint = false;
        m_isAddedPointInMoveOrAdd = false;
        update();
    }
    VectorControlPointMode vectorControlPointMode() const { return m_vectorControlPointMode; }

    // ベクター折れ線・曲線ツール設定
    void setVectorClosePath(bool close) { m_vectorClosePath = close; update(); }
    bool isVectorClosePath() const { return m_vectorClosePath; }
    void setVectorLineSize(float size) { m_vectorLineSize = size; update(); }
    float vectorLineSize() const { return m_vectorLineSize; }
    void setVectorLineDensity(float density) { m_vectorLineDensity = density; update(); }
    float vectorLineDensity() const { return m_vectorLineDensity; }
    void finishVectorLine();
    void cancelVectorLine();

    // ベクター線変更ツール設定
    void setChangeLineSize(float sz) { m_changeLineSize = sz; }
    float changeLineSize() const { return m_changeLineSize; }
    void setChangeLineDensity(float dens) { m_changeLineDensity = dens; }
    float changeLineDensity() const { return m_changeLineDensity; }
    void setChangeLineHardness(int h) { m_changeLineHardness = h; }
    int changeLineHardness() const { return m_changeLineHardness; }
    void setChangeLineFalloff(int f) { m_changeLineFalloff = f; }
    int changeLineFalloff() const { return m_changeLineFalloff; }

    // ベクター楕円ツール設定
    void setVectorEllipseSize(float sz) { m_vectorEllipseSize = sz; update(); }
    float vectorEllipseSize() const { return m_vectorEllipseSize; }
    void setVectorEllipseDensity(float dens) { m_vectorEllipseDensity = dens; update(); }
    float vectorEllipseDensity() const { return m_vectorEllipseDensity; }
    void setVectorEllipseHardness(int h) { m_vectorEllipseHardness = h; update(); }
    int vectorEllipseHardness() const { return m_vectorEllipseHardness; }
    void setVectorEllipseFalloff(int f) { m_vectorEllipseFalloff = f; update(); }
    int vectorEllipseFalloff() const { return m_vectorEllipseFalloff; }
    void setVectorEllipseFilled(bool fill) { m_vectorEllipseFilled = fill; update(); }
    bool isVectorEllipseFilled() const { return m_vectorEllipseFilled; }

    // ベクター円ツール設定
    void setVectorCircleSize(float sz) { m_vectorCircleSize = sz; update(); }
    float vectorCircleSize() const { return m_vectorCircleSize; }
    void setVectorCircleDensity(float dens) { m_vectorCircleDensity = dens; update(); }
    float vectorCircleDensity() const { return m_vectorCircleDensity; }
    void setVectorCircleHardness(int h) { m_vectorCircleHardness = h; update(); }
    int vectorCircleHardness() const { return m_vectorCircleHardness; }
    void setVectorCircleFalloff(int f) { m_vectorCircleFalloff = f; update(); }
    int vectorCircleFalloff() const { return m_vectorCircleFalloff; }
    void setVectorCircleFilled(bool fill) { m_vectorCircleFilled = fill; update(); }
    bool isVectorCircleFilled() const { return m_vectorCircleFilled; }

    // ベクター多角形ツール設定
    void setVectorPolygonSides(int s) { m_vectorPolygonSides = s; update(); }
    int vectorPolygonSides() const { return m_vectorPolygonSides; }
    void setVectorPolygonRegular(bool r) { m_vectorPolygonRegular = r; update(); }
    bool isVectorPolygonRegular() const { return m_vectorPolygonRegular; }
    void setVectorPolygonSize(float sz) { m_vectorPolygonSize = sz; update(); }
    float vectorPolygonSize() const { return m_vectorPolygonSize; }
    void setVectorPolygonDensity(float dens) { m_vectorPolygonDensity = dens; update(); }
    float vectorPolygonDensity() const { return m_vectorPolygonDensity; }
    void setVectorPolygonHardness(int h) { m_vectorPolygonHardness = h; update(); }
    int vectorPolygonHardness() const { return m_vectorPolygonHardness; }
    void setVectorPolygonFalloff(int f) { m_vectorPolygonFalloff = f; update(); }
    int vectorPolygonFalloff() const { return m_vectorPolygonFalloff; }
    void setVectorPolygonFilled(bool fill) { m_vectorPolygonFilled = fill; update(); }
    bool isVectorPolygonFilled() const { return m_vectorPolygonFilled; }

    // グラデーションツール設定
    void setGradientBlendMode(BlendMode mode) { m_gradientBlendMode = mode; }
    BlendMode gradientBlendMode() const { return m_gradientBlendMode; }
    void setGradientShape(GradientShape s) { m_gradientShape = s; }
    GradientShape gradientShape() const { return m_gradientShape; }
    void setGradientColorMode(GradientColorMode m) { m_gradientColorMode = m; }
    GradientColorMode gradientColorMode() const { return m_gradientColorMode; }
    void setGradientRepeatMode(GradientRepeatMode r) { m_gradientRepeatMode = r; }
    GradientRepeatMode gradientRepeatMode() const { return m_gradientRepeatMode; }
    void setGradientOpacity(float op) { m_gradientOpacity = op; }
    float gradientOpacity() const { return m_gradientOpacity; }
    void setGradientReverse(bool rev) { m_gradientReverse = rev; }
    bool isGradientReverse() const { return m_gradientReverse; }
    void setGradientDither(bool d) { m_gradientDither = d; }
    bool isGradientDither() const { return m_gradientDither; }
    void renderGradient(const QPointF& startPos, const QPointF& endPos);

    // GIMPスタイル 選択範囲表示・操作
    bool isShowSelection() const { return m_showSelection; }
    void setShowSelection(bool show);
    void selectAll();
    void clearSelection();
    void invertSelection();
    void deleteSelectedPixels();

    // エアブラシ ホールド連続噴霧 (GIMP Airbrush)
    void setAirbrushInterval(int ms);
    int airbrushInterval() const { return m_airbrushInterval; }
    void onAirbrushTimerTimeout();

    // 変形ツール (GIMP統合変形・ワープ変形・ケージ変形)
    void setTransformInterpolation(InterpolationMode mode) { m_transformInterpolation = mode; update(); }
    InterpolationMode transformInterpolation() const { return m_transformInterpolation; }
    void setTransformKeepAspect(bool keep) { m_transformKeepAspect = keep; }
    bool isTransformKeepAspect() const { return m_transformKeepAspect; }
    void setTransformFromCenter(bool center) { m_transformFromCenter = center; }
    bool isTransformFromCenter() const { return m_transformFromCenter; }
    bool isTransforming() const { return m_isTransforming; }

    void setTransformMode(TransformMode mode);
    TransformMode transformMode() const { return m_transformMode; }
    void setWarpSubMode(WarpSubMode mode);
    WarpSubMode warpSubMode() const { return m_warpSubMode; }
    void setWarpBrushSize(float sz) { m_warpBrushSize = sz; update(); }
    float warpBrushSize() const { return m_warpBrushSize; }
    void setWarpBrushStrength(float st) { m_warpBrushStrength = st; }
    float warpBrushStrength() const { return m_warpBrushStrength; }
    void setWarpMeshDivisions(int div);
    int warpMeshDivisions() const { return m_warpMeshDivisions; }
    void setCageSubMode(CageSubMode mode);
    CageSubMode cageSubMode() const { return m_cageSubMode; }
    void resetCage();
    void flipTransformHorizontal();
    void flipTransformVertical();

    void startTransform();
    void applyTransform();
    void cancelTransform();

    void applyTextSettingsToSelected();

    // テスト・自動化用ヘルパー
    void testStrokeBegin(const QPointF& docPos, float pressure = 1.0f) { strokeBegin(docPos, pressure); }
    void testStrokeMove(const QPointF& docPos, float pressure = 1.0f) { strokeMove(docPos, pressure); }
    void testStrokeEnd() { strokeEnd(); }
    void cancelCurrentStroke();
    bool isTouchPointerEvent(const QEvent* event) const;
    void selectLayerAtPixel(const QPointF& docPt);
    QPointF lastStrokeEndPos() const { return m_lastStrokeEndPos; }
    bool hasLastStrokeEndPos() const { return m_hasLastStrokeEndPos; }

signals:
    void transformChanged(double zoom, double angleDeg, bool flipped, const QPointF& docCenterVisible, const QSizeF& viewportSize);
    void colorPicked(const QColor& color);
    void textItemSelected(const TextBalloonItem& item);
    void layerSelectedFromCanvas(int layerIndex);

protected:
    void paintEvent(QPaintEvent* event) override;
    void tabletEvent(QTabletEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool event(QEvent* event) override; // タッチイベント処理用

    void strokeBegin(const QPointF& docPos, float pressure);
    void strokeMove(const QPointF& docPos, float pressure);
    void strokeEnd();

private:
    LayerPtr effectiveTargetLayer() const;
    uint32_t effectiveDrawColor(uint32_t baseColor) const;

    void paintBrushSegment(const QPointF& p0, const QPointF& p1, float w0, float w1, float dens0, float dens1, uint32_t color);
    void paintBlurSegment(const QPointF& p0, const QPointF& p1, float w0, float w1, float dens0, float dens1);
    void paintSmudgeSegment(const QPointF& p0, const QPointF& p1, float w0, float w1, float dens0, float dens1);
    void paintWatercolorSegment(const QPointF& p0, const QPointF& p1, float w0, float w1, float dens0, float dens1, float p0Raw, float p1Raw);
    void paintCompositeBrushSegment(const QPointF& p0, const QPointF& p1, float w0, float w1, float dens0, float dens1, float p0Raw, float p1Raw);
    void paintImageBrushSegment(const QPointF& p0, const QPointF& p1, float w0, float w1, float dens0, float dens1, float p0Raw, float p1Raw);

    QTransform viewTransform() const;
    void notifyTransformChanged();

    Document* m_document = nullptr;

    double m_zoom = 1.0;
    double m_rotationAngle = 0.0;
    bool m_flippedHorizontally = false;
    QPointF m_panOffset{0.0, 0.0};

    // 描画・操作状態
    bool m_isDrawing = false;
    bool m_isPanning = false;
    bool m_isRotating = false;
    bool m_isSpacePressed = false;
    bool m_firstShowHandled = false;
    QPointF m_lastMouseViewPos;
    QPointF m_cursorDocPos;
    bool m_showBrushOutline = false;

    ToolType m_currentTool = ToolType::Pen;
    QColor m_currentColor = Qt::black;
    QColor m_secondaryColor = Qt::white;
    float m_brushSize = 10.0f;
    float m_minSizeRatio = 0.0f;
    float m_brushDensity = 1.0f;
    bool m_antialias = true;
    bool m_pressureSizeEnabled = true;
    bool m_pressureDensityEnabled = false;
    int m_brushHardness = 100;
    int m_brushFalloff = 0;
    int m_brushMinPressure = 0;
    BlendMode m_brushBlendMode = BlendMode::Normal;
    bool m_brushScatter = false;
    int m_brushScatterAmount = 5;
    int m_brushScatterRange = 100;
    int m_brushScatterSizeRandom = 0;
    bool m_brushScatterScaleUp = false;

    StrokeSmoother m_smoother;
    StrokeInputPoint m_lastFilteredPoint;

    // 直線描画 (Shift + 左クリック) 用
    QPointF m_lastStrokeEndPos;
    bool m_hasLastStrokeEndPos = false;

    // SAI風ベクトルストローク記録
    VectorStroke m_currentVectorStroke;
    VectorEraserMode m_vectorEraserMode = VectorEraserMode::Intersection;
    float m_vectorEraserSize = 20.0f;
    std::vector<VectorStroke> m_beforeVectorStrokes;
    bool m_vectorStrokesModified = false;
    std::vector<QPointF> m_vectorEraserTrajectory;

    // ベクター制御点ツール状態
    VectorControlPointMode m_vectorControlPointMode = VectorControlPointMode::MovePoint;
    int m_hoveredStrokeIdx = -1;
    int m_hoveredPointStrokeIdx = -1;
    int m_hoveredPointIdx = -1;

    bool m_isDraggingControlPoint = false;
    bool m_isAddedPointInMoveOrAdd = false;
    bool m_isDraggingVectorStroke = false;
    bool m_isRotatingVectorStroke = false;
    bool m_isConnectingStrokes = false;
    int m_dragStrokeIdx = -1;
    int m_dragPointIdx = -1;
    QPointF m_dragStartDocPos;
    QPointF m_dragLastDocPos;
    QPointF m_rotateStrokeCenter;
    float m_rotateStrokePrevAngle = 0.0f;
    float m_accumulatedRotateAngle = 0.0f;
    int m_connectSourceStrokeIdx = -1;
    int m_connectSourcePtIdx = -1;
    int m_connectTargetStrokeIdx = -1;
    int m_connectTargetPtIdx = -1;

    // ベクター筆圧ツール状態
    bool m_isDraggingPressure = false;
    int m_pressureStrokeIdx = -1;
    int m_pressurePointIdx = -1;
    QPointF m_pressureStartMouseViewPos;
    float m_pressureStartVal = 1.0f;
    int m_currentPressurePercent = 100;

    // 折れ線・曲線ツール状態
    bool m_vectorClosePath = false;
    float m_vectorLineSize = 4.0f;
    float m_vectorLineDensity = 1.0f;
    bool m_isCreatingVectorLine = false;
    VectorStroke m_inProgressVectorStroke;
    QPointF m_vectorLineCurrentMousePos;

    // 線変更ツール状態
    float m_changeLineSize = 4.0f;
    float m_changeLineDensity = 1.0f;
    int m_changeLineHardness = 100;
    int m_changeLineFalloff = 0;

    // 楕円ツール状態
    float m_vectorEllipseSize = 4.0f;
    float m_vectorEllipseDensity = 1.0f;
    int m_vectorEllipseHardness = 100;
    int m_vectorEllipseFalloff = 0;
    bool m_vectorEllipseFilled = false;
    bool m_isCreatingVectorEllipse = false;
    QPointF m_vectorEllipseCenter;
    QPointF m_vectorEllipseCurrentPos;

    // 円ツール状態
    float m_vectorCircleSize = 4.0f;
    float m_vectorCircleDensity = 1.0f;
    int m_vectorCircleHardness = 100;
    int m_vectorCircleFalloff = 0;
    bool m_vectorCircleFilled = false;
    bool m_isCreatingVectorCircle = false;
    QPointF m_vectorCircleCenter;
    QPointF m_vectorCircleCurrentPos;

    // 多角形ツール状態
    float m_vectorPolygonSize = 4.0f;
    float m_vectorPolygonDensity = 1.0f;
    int m_vectorPolygonHardness = 100;
    int m_vectorPolygonFalloff = 0;
    int m_vectorPolygonSides = 5;
    bool m_vectorPolygonRegular = false;
    bool m_vectorPolygonFilled = false;
    bool m_isCreatingVectorPolygon = false;
    QPointF m_vectorPolygonCenter;
    QPointF m_vectorPolygonCurrentPos;

    // 1ストローク内での濃度累積制御用バッファ (SAI/Photoshop不透明度方式)
    std::unordered_map<int, Tile> m_strokeTileSnapshots;
    std::unordered_map<int, uint8_t*> m_strokeAlphaTiles;

    // アンドゥ用ストローク前スナップショットとダーティ矩形
    TileManager m_strokePreTiles{0, 0};
    QRect m_strokeDirtyRect;

    // 吹き出し・テキスト操作用状態
    enum class BalloonHandle {
        None,
        Body,
        TailTip,
        TailCurve,
        TailBaseWidth
    };
    bool m_isCreatingBalloon = false;
    bool m_isMovingBalloon = false;
    bool m_isMovingTail = false;
    BalloonHandle m_balloonDragHandle = BalloonHandle::None;
    QPointF m_balloonStartPos;
    QPointF m_balloonCurrentPos;
    int m_selectedBalloonIndex = -1;
    QPointF m_balloonMoveOffset;
    TextBalloonItem m_balloonPreEditItem;

    std::shared_ptr<TextLayer> ensureTextLayer();
    TextSettingsWidget* m_textSettings = nullptr;

    // コマ枠・コマ分割操作状態
    enum class PanelHandle {
        None,
        Vertex,
        Edge
    };
    bool m_isCreatingPanel = false;
    QPointF m_panelCreateStartDocPos;
    QPointF m_panelCreateCurDocPos;

    bool m_isSplittingPanel = false;
    QPointF m_panelSplitStartDocPos;
    QPointF m_panelSplitCurDocPos;

    bool m_isModifyingPanel = false;
    PanelHandle m_panelDragHandle = PanelHandle::None;
    int m_panelDragIndex = -1;
    QPointF m_panelModifyStartDocPos;
    QPolygonF m_panelOrigPolygon;
    std::shared_ptr<GroupLayer> m_editingPanel;

    struct LinkedPanelVertex {
        std::shared_ptr<GroupLayer> panel;
        int vertexIndex = -1;
        QPolygonF origPolygon;
        QPointF origPos;
    };
    std::vector<LinkedPanelVertex> m_linkedPanelVertices;

    PanelSettingsWidget* m_panelSettings = nullptr;
    QPointF snapToBleed(const QPointF& pt, float threshold = 15.0f) const;
    void renderPanelOverlay(QPainter& painter);

    // 3Dレイヤー操作状態
    ThreeDSettingsWidget* m_threeDSettings = nullptr;
    bool m_isOrbiting3D = false;
    bool m_isPanning3D = false;
    bool m_isZooming3D = false;
    QPointF m_last3DMousePos;
    std::shared_ptr<ThreeDLayer> currentThreeDLayer() const;

    // グループ/レイヤー平行移動状態
    bool m_isMovingLayer = false;
    QPointF m_layerMoveStartDocPos;
    int m_accumulatedDx = 0;
    int m_accumulatedDy = 0;

    // 矩形選択ツール状態
    SelectionMode m_selectionMode = SelectionMode::New;
    bool m_selectionAlignToWindow = false;
    bool m_isSelecting = false;
    QPointF m_selectStartDocPos;
    QPointF m_selectCurDocPos;
    QPointF m_selectStartViewPos;
    QPointF m_selectCurViewPos;

    // 楕円選択ツール状態
    SelectionMode m_ellipseSelectMode = SelectionMode::New;
    EllipseSelectMethod m_ellipseSelectMethod = EllipseSelectMethod::BoundingBox;

    // 投げ縄ツール状態
    SelectionMode m_lassoSelectMode = SelectionMode::New;
    std::vector<QPointF> m_lassoPointsDoc;
    std::vector<QPointF> m_lassoPointsView;
    bool m_isLassoSelecting = false;

    // 色域選択ツール状態
    SelectionMode m_colorSelectMode = SelectionMode::New;
    ColorSelectReference m_colorSelectReference = ColorSelectReference::ActiveLayer;
    int m_colorSelectTolerance = 20;
    bool m_colorSelectTransparent = true;

    // バケツツール状態
    BlendMode m_bucketBlendMode = BlendMode::Normal;
    float m_bucketDensity = 1.0f;
    BucketReference m_bucketReference = BucketReference::Canvas;
    int m_bucketExpandPixels = 1;
    bool m_bucketNarrowAreaFill = true;
    int m_bucketTolerance = 30;

    // パターン塗りつぶしツール状態
    BlendMode m_patternFillBlendMode = BlendMode::Normal;
    float m_patternFillDensity = 1.0f;
    float m_patternFillScale = 1.0f;
    float m_patternFillAngle = 0.0f;
    QImage m_patternFillImage;
    BucketReference m_patternFillReference = BucketReference::Canvas;
    int m_patternFillExpandPixels = 1;
    bool m_patternFillNarrowAreaFill = true;
    int m_patternFillTolerance = 30;

    // 自動選択ツール状態
    SelectionMode m_autoSelectMode = SelectionMode::New;
    BucketReference m_autoSelectReference = BucketReference::Canvas;
    int m_autoSelectExpandPixels = 0;
    bool m_autoSelectNarrowAreaFill = true;
    int m_autoSelectTolerance = 30;

    // ぼかしツール状態
    float m_blurSize = 30.0f;
    float m_blurMinSizeRatio = 0.0f;
    float m_blurDensity = 1.0f;
    bool m_blurPressureSizeEnabled = true;
    bool m_blurPressureDensityEnabled = false;
    int m_blurColorMix = 50;
    int m_blurColorStretch = 50;
    int m_blurHardness = 0;
    int m_blurFalloff = 100;
    int m_blurMinPressure = 0;

    // ぼかしツールの色延び保持
    bool m_hasDragColor = false;
    float m_dragR = 0.0f;
    float m_dragG = 0.0f;
    float m_dragB = 0.0f;
    float m_dragA = 0.0f;

    // 指先ツール状態 (GIMP Smudge)
    float m_smudgeSize = 30.0f;
    float m_smudgeMinSizeRatio = 0.0f;
    float m_smudgeDensity = 1.0f;
    int m_smudgeRate = 50;
    bool m_smudgePressureSizeEnabled = true;
    bool m_smudgePressureDensityEnabled = false;
    int m_smudgeHardness = 0;
    int m_smudgeFalloff = 100;
    int m_smudgeMinPressure = 0;

    // 指先の絵の具付着状態
    bool m_hasSmudgeColor = false;
    float m_smudgeR = 0.0f;
    float m_smudgeG = 0.0f;
    float m_smudgeB = 0.0f;
    float m_smudgeA = 0.0f;

    // 水彩筆ツール設定
    BlendMode m_watercolorBlendMode = BlendMode::Normal;
    float m_watercolorSize = 20.0f;
    float m_watercolorMinSizeRatio = 0.2f;
    float m_watercolorDensity = 0.7f;
    int m_watercolorHardness = 50;
    int m_watercolorFalloff = 100;
    int m_watercolorMinPressure = 0;
    int m_watercolorStrength = 50;
    int m_watercolorWater = 50;
    int m_watercolorColorStretch = 50;
    int m_watercolorBlurPressure = 0;
    bool m_watercolorPressureSizeEnabled = true;
    bool m_watercolorPressureDensityEnabled = true;
    bool m_watercolorAntialias = true;
    bool m_watercolorScatter = false;
    int m_watercolorScatterAmount = 5;
    int m_watercolorScatterRange = 100;
    int m_watercolorScatterSizeRandom = 0;
    bool m_watercolorScatterScaleUp = false;

    // 水彩筆の絵の具付着状態 (色延び・混色用)
    bool m_hasWatercolorColor = false;
    float m_wcR = 0.0f;
    float m_wcG = 0.0f;
    float m_wcB = 0.0f;
    float m_wcA = 0.0f;

    // 合成ブラシツール設定
    CompositeBrushReference m_compositeBrushReference = CompositeBrushReference::Canvas;
    BlendMode m_compositeBrushBlendMode = BlendMode::Normal;
    float m_compositeBrushSize = 20.0f;
    float m_compositeBrushMinSizeRatio = 0.2f;
    float m_compositeBrushDensity = 0.7f;
    int m_compositeBrushHardness = 50;
    int m_compositeBrushFalloff = 100;
    int m_compositeBrushMinPressure = 0;
    int m_compositeBrushStrength = 50;
    int m_compositeBrushWater = 50;
    int m_compositeBrushColorStretch = 50;
    int m_compositeBrushBlurPressure = 0;
    bool m_compositeBrushPressureSizeEnabled = true;
    bool m_compositeBrushPressureDensityEnabled = true;
    bool m_compositeBrushAntialias = true;
    bool m_compositeBrushScatter = false;
    int m_compositeBrushScatterAmount = 5;
    int m_compositeBrushScatterRange = 100;
    int m_compositeBrushScatterSizeRandom = 0;
    bool m_compositeBrushScatterScaleUp = false;

    // 合成ブラシの絵の具付着状態 (色延び・混色用)
    bool m_hasCompositeBrushColor = false;
    float m_compWcR = 0.0f;
    float m_compWcG = 0.0f;
    float m_compWcB = 0.0f;
    float m_compWcA = 0.0f;

    // 画像ブラシツール状態
    QImage m_imageBrushImage;
    float m_imageBrushSize = 40.0f;
    float m_imageBrushMinSizeRatio = 0.0f;
    int m_imageBrushSpacing = 20; // 1〜500%
    float m_imageBrushDensity = 1.0f;
    int m_imageBrushMinPressure = 0;
    bool m_imageBrushPressureSizeEnabled = true;
    bool m_imageBrushPressureDensityEnabled = false;
    BlendMode m_imageBrushBlendMode = BlendMode::Normal;
    bool m_imageBrushScatter = false;
    int m_imageBrushScatterAmount = 5;
    int m_imageBrushScatterRange = 100;
    int m_imageBrushScatterSizeRandom = 0;
    bool m_imageBrushScatterScaleUp = false;

    bool m_imageBrushFlipH = false;
    bool m_imageBrushFlipV = false;
    int m_imageBrushAngle = 0;
    bool m_imageBrushFollowCursor = false;
    bool m_imageBrushRandomRotate = false;
    int m_imageBrushRandomRotateAmount = 360;
    bool m_imageBrushRandomSize = false;
    int m_imageBrushRandomSizeAmount = 0;
    bool m_imageBrushRandomSizeScaleUp = false;

    float m_imageBrushDistRemainder = 0.0f;
    float m_imageBrushLastFollowAngle = 0.0f;
    QPainterPath m_imageBrushOutlinePath;
    QPointF m_lastHoverDocPos;

    // グラデーションツール状態 (GIMP Gradient)
    BlendMode m_gradientBlendMode = BlendMode::Normal;
    GradientShape m_gradientShape = GradientShape::Linear;
    GradientColorMode m_gradientColorMode = GradientColorMode::FgToBg;
    GradientRepeatMode m_gradientRepeatMode = GradientRepeatMode::None;
    float m_gradientOpacity = 1.0f;
    bool m_gradientReverse = false;
    bool m_gradientDither = true;

    bool m_isDraggingGradient = false;
    QPointF m_gradientStartDocPos;
    QPointF m_gradientEndDocPos;

    struct FloodFillParams {
        BucketReference reference = BucketReference::Canvas;
        int tolerance = 30;
        int expandPixels = 0;
        bool narrowAreaFill = true;
        bool restrictToSelection = true;
    };

    struct FloodFillResult {
        bool success = false;
        std::vector<uint8_t> mask;
        int minX = 0;
        int maxX = 0;
        int minY = 0;
        int maxY = 0;
        int docW = 0;
        int docH = 0;
    };

    FloodFillResult computeFloodFillMask(const QPointF& docPos, const FloodFillParams& params);
    QPainterPath maskToPath(const FloodFillResult& res);

    // GIMPスタイル マーチングアンツ（アリの行進）状態
    bool m_showSelection = true;
    QTimer* m_antsTimer = nullptr;
    int m_antsOffset = 0;
    void updateAntsTimer();

    // GIMPスタイル 矩形選択インタラクティブ変形（端のドラッグで選択範囲変更）
    enum class RectHandle {
        None,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
        TopEdge,
        BottomEdge,
        LeftEdge,
        RightEdge,
        Inside
    };

    bool m_hasActiveRect = false;
    QRectF m_activeDocRect;
    QRectF m_activeViewRect;
    bool m_activeRectAlignToWindow = false;

    bool m_isModifyingRect = false;
    RectHandle m_hoverHandle = RectHandle::None;
    RectHandle m_dragHandle = RectHandle::None;
    QPointF m_modifyStartDocPos;
    QPointF m_modifyStartViewPos;
    QRectF m_modifyOrigDocRect;
    QRectF m_modifyOrigViewRect;
    QPainterPath m_modifyOrigSelection;

    RectHandle hitTestRectHandle(const QPointF& mouseViewPos) const;
    void updateSelectionCursor(RectHandle handle);

    void updateCursorForActiveLayer();

    // エアブラシ ホールド連続噴霧タイマー (GIMP Airbrush)
    QTimer* m_airbrushTimer = nullptr;
    int m_airbrushInterval = 30;

    // 変形ツール (統合変形) 状態
    enum class TransformHandle {
        None,
        Inside,
        TopLeft,
        TopRight,
        BottomRight,
        BottomLeft,
        TopEdge,
        RightEdge,
        BottomEdge,
        LeftEdge,
        Pivot,
        RotateOuter
    };

    bool m_isTransforming = false;
    InterpolationMode m_transformInterpolation = InterpolationMode::Bilinear;
    bool m_transformKeepAspect = false;
    bool m_transformFromCenter = false;

    // 変形ジオメトリ (ドキュメント座標系)
    QPolygonF m_transformOrigQuad;
    QPolygonF m_transformCurrentQuad;
    QPointF m_transformPivot;

    // 変形元データ
    struct TransformTargetLayer {
        int layerIndex = -1;
        std::shared_ptr<Layer> layer;
        bool isRaster = true;
        bool isVector = false;
        QImage sourceImage;
        std::optional<TileManager> preTiles;
        std::vector<VectorStroke> origStrokes;
    };
    std::vector<TransformTargetLayer> m_transformTargetLayers;

    QRect m_transformSourceRect;
    QImage m_transformSourceImage;
    bool m_transformHasSelection = false;
    QPainterPath m_transformOrigSelection;
    std::vector<VectorStroke> m_transformOrigStrokes;
    std::optional<TileManager> m_transformPreTiles;


    // インタラクション状態
    TransformHandle m_transformHoverHandle = TransformHandle::None;
    TransformHandle m_transformDragHandle = TransformHandle::None;
    QPointF m_transformDragStartDocPos;
    QPointF m_transformDragStartViewPos;
    QPolygonF m_transformDragStartQuad;
    QPointF m_transformDragStartPivot;
    double m_transformDragStartAngle = 0.0;

    TransformHandle hitTestTransformHandle(const QPointF& mouseViewPos) const;
    void updateTransformCursor(TransformHandle handle, Qt::KeyboardModifiers mods);
    void renderTransformPreview(QPainter& painter);
    void renderTransformGizmo(QPainter& painter);

    // 変形モード & サブモード
    TransformMode m_transformMode = TransformMode::Unified;
    WarpSubMode m_warpSubMode = WarpSubMode::Brush;
    CageSubMode m_cageSubMode = CageSubMode::AutoBox;

    float m_warpBrushSize = 60.0f;
    float m_warpBrushStrength = 50.0f;
    int m_warpMeshDivisions = 4;

    // メッシュワープ用状態
    std::vector<QPointF> m_meshOrigPoints;
    std::vector<QPointF> m_meshCurrentPoints;
    int m_meshDragIdx = -1;
    int m_meshHoverIdx = -1;
    void initMeshGrid();
    void renderMeshWarpPreview(QPainter& painter);
    void renderMeshWarpGizmo(QPainter& painter);

    // ブラシワープ用状態
    std::vector<float> m_warpDx;
    std::vector<float> m_warpDy;
    bool m_isWarpDragging = false;
    QPointF m_warpLastMouseDocPos;
    void initBrushWarp();
    void renderBrushWarpPreview(QPainter& painter);
    void renderBrushWarpGizmo(QPainter& painter);

    // ケージ変形用状態
    std::vector<QPointF> m_cageOrigPoints;
    std::vector<QPointF> m_cageCurrentPoints;
    bool m_isCageClosed = false;
    int m_cageDragIdx = -1;
    int m_cageHoverIdx = -1;
    void initCageAutoBox();
    void renderCageTransformPreview(QPainter& painter);
    void renderCageTransformGizmo(QPainter& painter);
    QPointF evaluateMLS(const QPointF& pt, const std::vector<QPointF>& origPts, const std::vector<QPointF>& curPts) const;
    bool m_rightClickEyedropperEnabled = true;
    bool m_isRightClickEyedropper = false;
    TouchAction m_touchAction = TouchAction::PanZoomRotate;
};

} // namespace Kaburagi
