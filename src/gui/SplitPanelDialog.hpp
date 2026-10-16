#pragma once

#include <QDialog>
#include <QRadioButton>
#include <QButtonGroup>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPainter>
#include <memory>
#include "GroupLayer.hpp"

namespace Kaburagi {

class SplitPanelPreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit SplitPanelPreviewWidget(QWidget* parent = nullptr);

    void updatePreview(const QPolygonF& origPoly, Qt::Orientation orientation, int count, float margin);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPolygonF m_origPoly;
    Qt::Orientation m_orientation = Qt::Vertical;
    int m_count = 2;
    float m_margin = 20.0f;
};

class SplitPanelDialog : public QDialog {
    Q_OBJECT
public:
    SplitPanelDialog(const std::shared_ptr<GroupLayer>& targetPanel,
                     float defaultMarginH = 20.0f,
                     float defaultMarginV = 40.0f,
                     QWidget* parent = nullptr);

    Qt::Orientation orientation() const;
    int splitCount() const;
    float margin() const;

private slots:
    void onParametersChanged();
    void onOrientationToggled();

private:
    void setupUI();

    std::shared_ptr<GroupLayer> m_targetPanel;
    float m_defaultMarginH;
    float m_defaultMarginV;

    QRadioButton* m_radioVertical = nullptr;
    QRadioButton* m_radioHorizontal = nullptr;
    QSpinBox* m_countSpin = nullptr;
    QDoubleSpinBox* m_marginSpin = nullptr;
    SplitPanelPreviewWidget* m_previewWidget = nullptr;
};

} // namespace Kaburagi
