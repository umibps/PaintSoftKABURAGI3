#pragma once

#include <QWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QStackedWidget>
#include "SpinScaleWidget.hpp"

namespace Kaburagi {

enum class InterpolationMode {
    Bilinear = 0,        // バイリニア (標準)
    Bicubic = 1,         // バイキュービック (高品質)
    NearestNeighbor = 2  // 最近傍 (ニアレストネイバー)
};

enum class TransformMode {
    Unified = 0,  // 統合変形 (拡大縮小・回転・シアー・パース)
    Warp = 1,     // ワープ変形 (ブラシ押し出し / メッシュ変形)
    Cage = 2      // ケージ変形 (自動バウンディングボックス / カスタム多角形)
};

enum class WarpSubMode {
    Brush = 0,    // ブラシワープ (GIMP風押し出し)
    Mesh = 1      // メッシュワープ (格子点変形)
};

enum class CageSubMode {
    AutoBox = 0,        // 自動バウンディングボックス (デフォルト)
    CustomPolygon = 1   // カスタム多角形ケージ
};

class TransformSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit TransformSettingsWidget(QWidget* parent = nullptr);
    ~TransformSettingsWidget() override = default;

    // 変形モード
    TransformMode transformMode() const;
    void setTransformMode(TransformMode mode);

    // 補間アルゴリズム
    InterpolationMode interpolationMode() const;
    void setInterpolationMode(InterpolationMode mode);

    // 統合変形オプション
    bool isKeepAspect() const;
    void setKeepAspect(bool keep);

    bool isFromCenter() const;
    void setFromCenter(bool fromCenter);

    // ワープ変形オプション
    WarpSubMode warpSubMode() const;
    void setWarpSubMode(WarpSubMode mode);
    float warpBrushSize() const;
    void setWarpBrushSize(float sz);
    float warpBrushStrength() const;
    void setWarpBrushStrength(float st);
    int warpMeshDivisions() const;
    void setWarpMeshDivisions(int div);

    // ケージ変形オプション
    CageSubMode cageSubMode() const;
    void setCageSubMode(CageSubMode mode);

signals:
    void transformModeChanged(TransformMode mode);
    void interpolationChanged(InterpolationMode mode);
    void keepAspectChanged(bool keep);
    void fromCenterChanged(bool fromCenter);
    void flipHorizontalRequested();
    void flipVerticalRequested();
    void warpSubModeChanged(WarpSubMode mode);
    void warpBrushSizeChanged(float sz);
    void warpBrushStrengthChanged(float st);
    void warpMeshDivisionsChanged(int div);
    void cageSubModeChanged(CageSubMode mode);
    void resetCageRequested();
    void applyRequested();
    void cancelRequested();

private:
    void updateModeVisibility();

    // モード選択 & 反転
    QComboBox* m_modeCombo = nullptr;
    QPushButton* m_btnFlipH = nullptr;
    QPushButton* m_btnFlipV = nullptr;

    // 統合変形パネル
    QWidget* m_unifiedPanel = nullptr;
    QCheckBox* m_keepAspectCheck = nullptr;
    QCheckBox* m_fromCenterCheck = nullptr;

    // ワープ変形パネル
    QWidget* m_warpPanel = nullptr;
    QComboBox* m_warpSubModeCombo = nullptr;
    QWidget* m_warpBrushControls = nullptr;
    SpinScale* m_warpSizeSpin = nullptr;
    SpinScale* m_warpStrengthSpin = nullptr;
    QWidget* m_warpMeshControls = nullptr;
    QComboBox* m_warpMeshDivCombo = nullptr;

    // ケージ変形パネル
    QWidget* m_cagePanel = nullptr;
    QComboBox* m_cageSubModeCombo = nullptr;
    QPushButton* m_btnResetCage = nullptr;

    // 補間 & OK/Cancel
    QComboBox* m_interpolationCombo = nullptr;
    QPushButton* m_btnApply = nullptr;
    QPushButton* m_btnCancel = nullptr;
};

} // namespace Kaburagi
