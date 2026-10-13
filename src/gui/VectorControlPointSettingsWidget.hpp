#pragma once

#include <QWidget>
#include <QRadioButton>
#include <QButtonGroup>
#include <QVBoxLayout>
#include <QGroupBox>
#include "VectorLayer.hpp"

namespace Kaburagi {

class VectorControlPointSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit VectorControlPointSettingsWidget(QWidget* parent = nullptr);
    ~VectorControlPointSettingsWidget() override = default;

    VectorControlPointMode mode() const { return m_mode; }
    void setMode(VectorControlPointMode mode);

    VectorControlPointMode controlPointMode() const { return m_mode; }
    void setControlPointMode(VectorControlPointMode mode) { setMode(mode); }

signals:
    void modeChanged(VectorControlPointMode mode);

private:
    void setupUI();

    VectorControlPointMode m_mode = VectorControlPointMode::MovePoint;

    QButtonGroup* m_modeGroup = nullptr;
    QRadioButton* m_radioMoveOrAddPoint = nullptr;
    QRadioButton* m_radioMovePoint = nullptr;
    QRadioButton* m_radioAddPoint = nullptr;
    QRadioButton* m_radioDeletePoint = nullptr;
    QRadioButton* m_radioMoveStroke = nullptr;
    QRadioButton* m_radioDuplicateMoveStroke = nullptr;
    QRadioButton* m_radioRotateStroke = nullptr;
    QRadioButton* m_radioConnectStrokes = nullptr;
    QRadioButton* m_radioDeleteStroke = nullptr;
};

} // namespace Kaburagi
