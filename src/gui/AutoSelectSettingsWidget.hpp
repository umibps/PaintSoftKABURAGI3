#pragma once

#include <QWidget>
#include <QRadioButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include "RectSelectSettingsWidget.hpp"
#include "BucketSettingsWidget.hpp"
#include "SpinScaleWidget.hpp"

namespace Kaburagi {

class AutoSelectSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit AutoSelectSettingsWidget(QWidget* parent = nullptr);
    ~AutoSelectSettingsWidget() override = default;

    // 1. 選択モード
    SelectionMode selectionMode() const { return m_mode; }
    void setSelectionMode(SelectionMode mode);

    // 2. 参照先レイヤー
    BucketReference referenceMode() const { return m_refMode; }
    void setReferenceMode(BucketReference mode);

    // 3. 領域拡縮 (-10 ～ +10 px)
    int expandPixels() const { return m_expandPixels; }
    void setExpandPixels(int pixels);

    // 4. 細い領域にしみこむ
    bool isNarrowAreaFill() const { return m_narrowAreaCheck ? m_narrowAreaCheck->isChecked() : true; }
    void setNarrowAreaFill(bool enable);

    // 5. 色の誤差 (Tolerance: 0 ～ 100)
    int tolerance() const { return m_tolerance; }
    void setTolerance(int tol);

signals:
    void modeChanged(SelectionMode mode);
    void referenceModeChanged(BucketReference mode);
    void expandPixelsChanged(int pixels);
    void narrowAreaFillChanged(bool enable);
    void toleranceChanged(int tol);

private:
    SelectionMode m_mode = SelectionMode::New;
    BucketReference m_refMode = BucketReference::Canvas;
    int m_expandPixels = 0;
    int m_tolerance = 30;

    QButtonGroup* m_modeGroup = nullptr;
    QRadioButton* m_radioNew = nullptr;
    QRadioButton* m_radioAdd = nullptr;
    QRadioButton* m_radioSubtract = nullptr;

    QButtonGroup* m_refGroup = nullptr;
    QRadioButton* m_radioCanvas = nullptr;
    QRadioButton* m_radioActiveLayer = nullptr;

    SpinScale* m_expandScale = nullptr;
    QCheckBox* m_narrowAreaCheck = nullptr;
    SpinScale* m_toleranceScale = nullptr;
};

} // namespace Kaburagi
