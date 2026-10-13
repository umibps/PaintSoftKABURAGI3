#pragma once

#include <QDialog>
#include <QColor>

class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QPushButton;

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

private slots:
    void onPresetChanged(int index);
    void onBgModeChanged(int index);
    void onPickBgColor();
    void onPickBg2Color();
    void accept() override;

private:
    void loadSettings();
    void saveSettings();
    void updateColorButtons();

    QComboBox* m_presetCombo = nullptr;
    QSpinBox* m_widthSpin = nullptr;
    QSpinBox* m_heightSpin = nullptr;
    QDoubleSpinBox* m_dpiSpin = nullptr;
    QComboBox* m_bgModeCombo = nullptr;
    QPushButton* m_bgColorBtn = nullptr;
    QPushButton* m_bg2ColorBtn = nullptr;

    QColor m_customBgColor = Qt::white;
    QColor m_secondaryBgColor = Qt::white;
};

} // namespace Kaburagi
