#pragma once

#include <QDialog>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QButtonGroup>
#include <QToolButton>

namespace Kaburagi {

/**
 * @brief キャンバスサイズ変更ダイアログ
 * 画像の内容は拡大縮小せず、キャンバスの枠（幅・高さ）を拡張/縮小する。
 * 3x3の基準位置（アンカー）に従って既存レイヤーの配置位置を決定。
 */
class ChangeCanvasSizeDialog : public QDialog {
    Q_OBJECT
public:
    enum class Anchor {
        TopLeft, Top, TopRight,
        Left, Center, Right,
        BottomLeft, Bottom, BottomRight
    };

    ChangeCanvasSizeDialog(int currentWidth, int currentHeight, QWidget* parent = nullptr);
    ~ChangeCanvasSizeDialog() override = default;

    int newWidth() const;
    int newHeight() const;
    Anchor anchor() const;
    void getOffsets(int& outOffsetX, int& outOffsetY) const;

private:
    int m_currentWidth;
    int m_currentHeight;

    QSpinBox* m_widthSpin = nullptr;
    QSpinBox* m_heightSpin = nullptr;
    QButtonGroup* m_anchorGroup = nullptr;
    QToolButton* m_anchorButtons[3][3]{};
};

/**
 * @brief キャンバスの拡大・縮小ダイアログ
 * キャンバス全体の解像度（画像サイズ）をリサンプル（拡大/縮小）する。
 * アスペクト比固定フラグおよび補間アルゴリズム（バイリニア/最近傍）の指定が可能。
 */
class ScaleCanvasDialog : public QDialog {
    Q_OBJECT
public:
    ScaleCanvasDialog(int currentWidth, int currentHeight, QWidget* parent = nullptr);
    ~ScaleCanvasDialog() override = default;

    int newWidth() const;
    int newHeight() const;
    Qt::TransformationMode interpolationMode() const;

private slots:
    void onPixelWidthChanged(int w);
    void onPixelHeightChanged(int h);
    void onPercentWidthChanged(double pw);
    void onPercentHeightChanged(double ph);

private:
    int m_currentWidth;
    int m_currentHeight;
    bool m_updating = false;

    QSpinBox* m_widthSpin = nullptr;
    QSpinBox* m_heightSpin = nullptr;
    QDoubleSpinBox* m_percentWSpin = nullptr;
    QDoubleSpinBox* m_percentHSpin = nullptr;
    QCheckBox* m_keepAspectCheck = nullptr;
    QComboBox* m_interpolationCombo = nullptr;
};

} // namespace Kaburagi
