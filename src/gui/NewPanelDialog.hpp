#pragma once

#include <QDialog>
#include <QPolygonF>
#include <QColor>
#include <QList>
#include <QString>

class QRadioButton;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QPushButton;
class QLabel;

namespace Kaburagi {

struct PanelSpec {
    QPolygonF polygon;
    QString name;
};

class PanelPreviewWidget;

class NewPanelDialog : public QDialog {
    Q_OBJECT
public:
    explicit NewPanelDialog(int canvasWidth, int canvasHeight,
                            float defaultBorderWidth = 3.0f,
                            const QColor& defaultBorderColor = Qt::black,
                            float defaultMarginX = 20.0f,
                            float defaultMarginY = 40.0f,
                            QWidget* parent = nullptr);
    ~NewPanelDialog() override = default;

    // 設定・生成結果の取得
    QList<PanelSpec> panels() const;
    bool drawBorder() const;
    float borderWidth() const;
    QColor borderColor() const;
    float marginX() const;
    float marginY() const;

private slots:
    void onModeToggled();
    void onTemplateChanged(int index);
    void onParamChanged();
    void onChooseBorderColor();

private:
    void setupUI();
    void updatePreview();
    void updateColorButtonStyle();
    QList<PanelSpec> calculatePanels() const;

    int m_canvasWidth = 1600;
    int m_canvasHeight = 1200;

    // 作成モード
    QRadioButton* m_templateRadio = nullptr;
    QRadioButton* m_gridRadio = nullptr;

    // テンプレート
    QComboBox* m_templateCombo = nullptr;
    QLabel* m_templateDescLabel = nullptr;

    // グリッド
    QSpinBox* m_colsSpin = nullptr;
    QSpinBox* m_rowsSpin = nullptr;

    // 余白 (マージン)
    QSpinBox* m_padXSpin = nullptr;
    QSpinBox* m_padYSpin = nullptr;

    // コマ間隔
    QDoubleSpinBox* m_marginXSpin = nullptr;
    QDoubleSpinBox* m_marginYSpin = nullptr;

    // 枠線
    QCheckBox* m_drawBorderCheck = nullptr;
    QDoubleSpinBox* m_borderWidthSpin = nullptr;
    QPushButton* m_borderColorBtn = nullptr;
    QColor m_borderColor = Qt::black;

    // プレビュー
    PanelPreviewWidget* m_previewWidget = nullptr;
};

} // namespace Kaburagi
