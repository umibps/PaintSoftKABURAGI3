#pragma once

#include <QDialog>
#include <QColor>

class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QPushButton;
class QCheckBox;

namespace Kaburagi {

class NewCanvasDialog : public QDialog {
    Q_OBJECT
public:
    explicit NewCanvasDialog(QWidget* parent = nullptr);
    ~NewCanvasDialog() override = default;

    int canvasWidth() const;
    int canvasHeight() const;
    double canvasDpi() const;
    QColor backgroundColor() const;
    QColor secondaryBackgroundColor() const;
    bool isKeepAspect() const;
    void setKeepAspect(bool keep);
    void swapDimensions();
    bool isClipToHiddenBase() const;
    void setClipToHiddenBase(bool clip);

public slots:
    void accept() override;

private slots:
    void onPresetChanged(int index);
    void onBgModeChanged(int index);
    void onPickBgColor();
    void onPickBg2Color();
    void onWidthChanged(int w);
    void onHeightChanged(int h);
    void onKeepAspectToggled(bool checked);
    void onSwapDimensions();

private:
    void loadSettings();
    void saveSettings();
    void updateColorButtons();

    QComboBox* m_presetCombo = nullptr;
    QSpinBox* m_widthSpin = nullptr;
    QSpinBox* m_heightSpin = nullptr;
    QCheckBox* m_keepAspectCheck = nullptr;
    QPushButton* m_swapSizeBtn = nullptr;
    QDoubleSpinBox* m_dpiSpin = nullptr;
    QComboBox* m_bgModeCombo = nullptr;
    QPushButton* m_bgColorBtn = nullptr;
    QPushButton* m_bg2ColorBtn = nullptr;
    QCheckBox* m_clipToHiddenBaseCheck = nullptr;

    double m_aspectRatio = 1600.0 / 1200.0;
    bool m_updatingSpin = false;

    QColor m_customBgColor = Qt::white;
    QColor m_secondaryBgColor = Qt::white;
};

} // namespace Kaburagi
