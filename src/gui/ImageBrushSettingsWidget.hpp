#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QMap>
#include <QImage>
#include "ToolBoxWidget.hpp"
#include "BlendModes.hpp"
#include "SpinScaleWidget.hpp"
#include "ScatterSettingsWidget.hpp"

namespace Kaburagi {

struct ImageBrushConfig {
    float size = 40.0f;
    float minSizeRatio = 0.0f;
    int spacing = 20; // 1〜500%
    float density = 1.0f;
    BlendMode blendMode = BlendMode::Normal;
    int minPressure = 0;
    bool pressureSize = true;
    bool pressureDensity = false;
    bool scatter = false;
    int scatterAmount = 5;
    int scatterRange = 100;
    int scatterSizeRandom = 0;
    bool scatterScaleUp = false;

    QString currentImageName;
    QString selectedImageId;
    bool flipH = false;
    bool flipV = false;
    int angle = 0;
    bool followCursor = false;
    bool randomRotate = false;
    int randomRotateAmount = 360;
    bool randomSize = false;
    int randomSizeAmount = 0;
    bool randomSizeScaleUp = false;
};

class ImageBrushSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit ImageBrushSettingsWidget(QWidget* parent = nullptr);
    ~ImageBrushSettingsWidget() override = default;

    BlendMode blendMode() const;
    void setBlendMode(BlendMode mode);
    float brushSize() const;
    void setBrushSize(float sz);
    float minSizeRatio() const;
    void setMinSizeRatio(float r);
    int spacing() const;
    void setSpacing(int sp);
    float density() const;
    void setDensity(float d);
    int minPressure() const;
    void setMinPressure(int p);
    bool isPressureSizeEnabled() const;
    void setPressureSizeEnabled(bool en);
    bool isPressureDensityEnabled() const;
    void setPressureDensityEnabled(bool en);

    bool isScatterEnabled() const;
    bool isScatter() const { return isScatterEnabled(); }
    int scatterAmount() const;
    int scatterRange() const;
    int scatterSizeRandom() const;
    bool isScatterScaleUpEnabled() const;
    bool isScatterScaleUp() const { return isScatterScaleUpEnabled(); }

    bool isFlipH() const;
    bool isFlipV() const;
    int angle() const;
    bool isFollowCursor() const;
    bool isRandomRotate() const;
    int randomRotateAmount() const;
    bool isRandomSize() const;
    int randomSizeAmount() const;
    bool isRandomSizeScaleUp() const;

    QString currentImageName() const;
    QString selectedImageId() const { return currentImageName(); }
    QImage currentBrushImage() const;
    QImage currentImage() const { return currentBrushImage(); }

    ImageBrushConfig config() const;
    void setConfig(const ImageBrushConfig& cfg);

    // フォルダの再読み込み
    void reloadBrushImages();
    // クリップボードからの画像取り込み（あれば追加）
    bool importFromClipboard(bool selectIt = true);

signals:
    void blendModeChanged(BlendMode mode);
    void brushSizeChanged(float size);
    void minSizeChanged(float ratio);
    void spacingChanged(int spacing);
    void densityChanged(float density);
    void minPressureChanged(int pressure);
    void pressureSizeChanged(bool enabled);
    void pressureDensityChanged(bool enabled);

    void scatterChanged(bool enabled);
    void scatterAmountChanged(int amount);
    void scatterRangeChanged(int range);
    void scatterSizeRandomChanged(int random);
    void scatterScaleUpChanged(bool enabled);

    void imageSelected(const QString& name, const QImage& image);
    void brushImageChanged(const QImage& image, const QString& name);
    void flipHChanged(bool flip);
    void flipVChanged(bool flip);
    void angleChanged(int angle);
    void followCursorChanged(bool enabled);
    void randomRotateChanged(bool enabled);
    void randomRotateAmountChanged(int amount);
    void randomSizeChanged(bool enabled);
    void randomSizeAmountChanged(int amount);
    void randomSizeScaleUpChanged(bool enabled);

private:
    void setupUI();
    void updateRandomRotateVisibility();
    void updateRandomSizeVisibility();
    void selectImageByName(const QString& name);

    // UIパーツ
    QComboBox* m_blendCombo = nullptr;
    SpinScale* m_sizeScale = nullptr;
    SpinScale* m_minSizeScale = nullptr;
    SpinScale* m_spacingScale = nullptr;
    SpinScale* m_densityScale = nullptr;
    SpinScale* m_minPressureScale = nullptr;
    QCheckBox* m_pressureSizeCheck = nullptr;
    QCheckBox* m_pressureDensityCheck = nullptr;
    ScatterSettingsWidget* m_scatterWidget = nullptr;

    // 画像選択関連
    QPushButton* m_btnReload = nullptr;
    QPushButton* m_btnClipboard = nullptr;
    QListWidget* m_imageListWidget = nullptr;

    QMap<QString, QImage> m_brushImages; // 識別名 -> QImage
    QString m_selectedImageName;
    QImage m_clipboardImage; // 一時的なクリップボード画像

    // 変形・回転関連
    QCheckBox* m_flipHCheck = nullptr;
    QCheckBox* m_flipVCheck = nullptr;
    SpinScale* m_angleScale = nullptr;
    QCheckBox* m_followCursorCheck = nullptr;
    QCheckBox* m_randomRotateCheck = nullptr;
    QWidget* m_randomRotateContainer = nullptr;
    SpinScale* m_randomRotateAmountScale = nullptr;

    QCheckBox* m_randomSizeCheck = nullptr;
    QWidget* m_randomSizeContainer = nullptr;
    SpinScale* m_randomSizeAmountScale = nullptr;
    QCheckBox* m_randomSizeScaleUpCheck = nullptr;
};

} // namespace Kaburagi
