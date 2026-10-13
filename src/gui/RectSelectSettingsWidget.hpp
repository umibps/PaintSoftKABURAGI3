#pragma once

#include <QWidget>
#include <QRadioButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QLabel>

namespace Kaburagi {

enum class SelectionMode {
    New,       // 通常選択
    Add,       // 追加選択
    Subtract   // 範囲削除
};

class RectSelectSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit RectSelectSettingsWidget(QWidget* parent = nullptr);
    ~RectSelectSettingsWidget() override = default;

    SelectionMode selectionMode() const { return m_mode; }
    void setSelectionMode(SelectionMode mode);

    bool isAlignToWindow() const { return m_alignToWindowCheck ? m_alignToWindowCheck->isChecked() : false; }
    void setAlignToWindow(bool align);

signals:
    void modeChanged(SelectionMode mode);
    void alignToWindowChanged(bool align);

private:
    SelectionMode m_mode = SelectionMode::New;

    QButtonGroup* m_modeGroup = nullptr;
    QRadioButton* m_radioNew = nullptr;
    QRadioButton* m_radioAdd = nullptr;
    QRadioButton* m_radioSubtract = nullptr;

    QCheckBox* m_alignToWindowCheck = nullptr;
};

} // namespace Kaburagi
