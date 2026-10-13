#pragma once

#include <QWidget>
#include <QRadioButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include "RectSelectSettingsWidget.hpp"
#include "SpinScaleWidget.hpp"

namespace Kaburagi {

enum class ColorSelectReference {
    ActiveLayer, // アクティブなレイヤー
    Canvas       // キャンバス (全レイヤー合成結果)
};

class ColorSelectSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit ColorSelectSettingsWidget(QWidget* parent = nullptr);
    ~ColorSelectSettingsWidget() override = default;

    // 1. 選択モード
    SelectionMode selectionMode() const { return m_mode; }
    void setSelectionMode(SelectionMode mode);

    // 2. 参照先 (アクティブレイヤー / キャンバス)
    ColorSelectReference referenceMode() const { return m_refMode; }
    void setReferenceMode(ColorSelectReference mode);

    // 3. 色の誤差 (Tolerance: 0 ～ 255)
    int tolerance() const { return m_tolerance; }
    void setTolerance(int tol);

    // 4. 透明部分の選択
    bool isSelectTransparent() const { return m_selectTransparentCheck ? m_selectTransparentCheck->isChecked() : true; }
    void setSelectTransparent(bool enable);

signals:
    void modeChanged(SelectionMode mode);
    void referenceModeChanged(ColorSelectReference mode);
    void toleranceChanged(int tol);
    void selectTransparentChanged(bool enable);

private:
    SelectionMode m_mode = SelectionMode::New;
    ColorSelectReference m_refMode = ColorSelectReference::ActiveLayer;
    int m_tolerance = 20;

    QButtonGroup* m_modeGroup = nullptr;
    QRadioButton* m_radioNew = nullptr;
    QRadioButton* m_radioAdd = nullptr;
    QRadioButton* m_radioSubtract = nullptr;

    QButtonGroup* m_refGroup = nullptr;
    QRadioButton* m_radioActiveLayer = nullptr;
    QRadioButton* m_radioCanvas = nullptr;

    SpinScale* m_toleranceScale = nullptr;
    QCheckBox* m_selectTransparentCheck = nullptr;
};

} // namespace Kaburagi
