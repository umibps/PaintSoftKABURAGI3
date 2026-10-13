#pragma once

#include <QWidget>
#include <QRadioButton>
#include <QButtonGroup>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include "RectSelectSettingsWidget.hpp"

namespace Kaburagi {

class LassoSelectSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit LassoSelectSettingsWidget(QWidget* parent = nullptr);
    ~LassoSelectSettingsWidget() override = default;

    SelectionMode selectionMode() const { return m_mode; }
    void setSelectionMode(SelectionMode mode);

signals:
    void modeChanged(SelectionMode mode);

private:
    SelectionMode m_mode = SelectionMode::New;

    QButtonGroup* m_modeGroup = nullptr;
    QRadioButton* m_radioNew = nullptr;
    QRadioButton* m_radioAdd = nullptr;
    QRadioButton* m_radioSubtract = nullptr;
};

} // namespace Kaburagi
