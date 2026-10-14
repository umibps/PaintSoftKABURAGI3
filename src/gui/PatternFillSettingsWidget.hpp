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
#include <QListWidget>
#include <QPushButton>
#include <QMap>
#include <QImage>
#include "SpinScaleWidget.hpp"
#include "BucketSettingsWidget.hpp"
#include "BlendModes.hpp"

namespace Kaburagi {

class PatternFillSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit PatternFillSettingsWidget(QWidget* parent = nullptr);
    ~PatternFillSettingsWidget() override = default;

    BlendMode blendMode() const;
    void setBlendMode(BlendMode mode);

    float density() const { return m_density; }
    void setDensity(float density);

    float patternScale() const { return m_patternScale; }
    void setPatternScale(float scale);

    float patternAngle() const { return m_patternAngle; }
    void setPatternAngle(float angle);

    BucketReference referenceMode() const { return m_refMode; }
    void setReferenceMode(BucketReference mode);

    int expandPixels() const { return m_expandPixels; }
    void setExpandPixels(int pixels);

    bool isNarrowAreaFill() const { return m_narrowAreaCheck ? m_narrowAreaCheck->isChecked() : true; }
    void setNarrowAreaFill(bool enable);

    int tolerance() const { return m_tolerance; }
    void setTolerance(int tol);

    QImage currentPatternImage() const;
    QString currentPatternName() const { return m_selectedPatternName; }
    void setSelectedPatternByName(const QString& name);

public slots:
    void reloadPatterns();
    void importFromClipboard();
    void importFromFile();

signals:
    void blendModeChanged(BlendMode mode);
    void densityChanged(float density);
    void patternScaleChanged(float scale);
    void patternAngleChanged(float angle);
    void patternImageChanged(const QImage& img, const QString& name);
    void referenceModeChanged(BucketReference mode);
    void expandPixelsChanged(int pixels);
    void narrowAreaFillChanged(bool enable);
    void toleranceChanged(int tol);

private:
    void setupUI();

    BucketReference m_refMode = BucketReference::Canvas;
    int m_expandPixels = 1;
    int m_tolerance = 30;
    float m_density = 1.0f;
    float m_patternScale = 1.0f;
    float m_patternAngle = 0.0f;

    QComboBox* m_blendCombo = nullptr;
    SpinScale* m_densityScale = nullptr;
    SpinScale* m_scaleScale = nullptr;
    SpinScale* m_angleScale = nullptr;

    QListWidget* m_patternListWidget = nullptr;
    QPushButton* m_btnReload = nullptr;
    QPushButton* m_btnClipboard = nullptr;
    QPushButton* m_btnFile = nullptr;

    QButtonGroup* m_refGroup = nullptr;
    QRadioButton* m_radioActiveLayer = nullptr;
    QRadioButton* m_radioCanvas = nullptr;

    SpinScale* m_expandScale = nullptr;
    QCheckBox* m_narrowAreaCheck = nullptr;
    SpinScale* m_toleranceScale = nullptr;

    QMap<QString, QImage> m_patternImages;
    QString m_selectedPatternName;
    QImage m_clipboardImage;
};

} // namespace Kaburagi
