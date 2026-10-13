#pragma once

#include <QWidget>
#include <QRadioButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include "RectSelectSettingsWidget.hpp"

namespace Kaburagi {

enum class EllipseSelectMethod {
    BoundingBox, // マウスの左ドラッグ開始＆終了位置でできた長方形に接する楕円
    FromCenter   // ベクトルレイヤーの楕円ツールと同じ方法 (中心から半径)
};

class EllipseSelectSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit EllipseSelectSettingsWidget(QWidget* parent = nullptr);
    ~EllipseSelectSettingsWidget() override = default;

    SelectionMode selectionMode() const { return m_mode; }
    void setSelectionMode(SelectionMode mode);

    EllipseSelectMethod selectMethod() const { return m_method; }
    void setSelectMethod(EllipseSelectMethod method);

    bool isAlignToWindow() const { return m_alignToWindowCheck ? m_alignToWindowCheck->isChecked() : false; }
    void setAlignToWindow(bool align);

signals:
    void modeChanged(SelectionMode mode);
    void selectMethodChanged(EllipseSelectMethod method);
    void alignToWindowChanged(bool align);

private:
    SelectionMode m_mode = SelectionMode::New;
    EllipseSelectMethod m_method = EllipseSelectMethod::BoundingBox;

    QButtonGroup* m_modeGroup = nullptr;
    QRadioButton* m_radioNew = nullptr;
    QRadioButton* m_radioAdd = nullptr;
    QRadioButton* m_radioSubtract = nullptr;

    QButtonGroup* m_methodGroup = nullptr;
    QRadioButton* m_radioBoundingBox = nullptr;
    QRadioButton* m_radioFromCenter = nullptr;

    QCheckBox* m_alignToWindowCheck = nullptr;
};

} // namespace Kaburagi
