#pragma once

#include <QWidget>
#include <QRadioButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QComboBox>
#include "SpinScaleWidget.hpp"
#include "BlendModes.hpp"

namespace Kaburagi {

enum class BucketReference {
    ActiveLayer, // アクティブなレイヤー
    Canvas       // キャンバス (全レイヤー)
};

class BucketSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit BucketSettingsWidget(QWidget* parent = nullptr);
    ~BucketSettingsWidget() override = default;

    BlendMode blendMode() const;
    void setBlendMode(BlendMode mode);

    BucketReference referenceMode() const { return m_refMode; }
    void setReferenceMode(BucketReference mode);

    int expandPixels() const { return m_expandPixels; }
    void setExpandPixels(int pixels);

    bool isNarrowAreaFill() const { return m_narrowAreaCheck ? m_narrowAreaCheck->isChecked() : true; }
    void setNarrowAreaFill(bool enable);

    int tolerance() const { return m_tolerance; }
    void setTolerance(int tol);

    float density() const { return m_density; }
    void setDensity(float density);

signals:
    void blendModeChanged(BlendMode mode);
    void densityChanged(float density);
    void referenceModeChanged(BucketReference mode);
    void expandPixelsChanged(int pixels);
    void narrowAreaFillChanged(bool enable);
    void toleranceChanged(int tol);

private:
    BucketReference m_refMode = BucketReference::Canvas;
    int m_expandPixels = 1;
    int m_tolerance = 30;
    float m_density = 1.0f;

    QComboBox* m_blendCombo = nullptr;
    SpinScale* m_densityScale = nullptr;
    QButtonGroup* m_refGroup = nullptr;
    QRadioButton* m_radioActiveLayer = nullptr;
    QRadioButton* m_radioCanvas = nullptr;

    SpinScale* m_expandScale = nullptr;
    QCheckBox* m_narrowAreaCheck = nullptr;
    SpinScale* m_toleranceScale = nullptr;
};

} // namespace Kaburagi
