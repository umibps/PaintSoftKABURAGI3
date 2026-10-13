#pragma once

#include <QWidget>
#include <QRadioButton>
#include <QButtonGroup>
#include <QVBoxLayout>
#include <QGroupBox>
#include "SpinScaleWidget.hpp"
#include "VectorLayer.hpp"

namespace Kaburagi {

class VectorEraserSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit VectorEraserSettingsWidget(QWidget* parent = nullptr);
    ~VectorEraserSettingsWidget() override = default;

    VectorEraserMode eraserMode() const { return m_mode; }
    void setEraserMode(VectorEraserMode mode);

    double eraserSize() const { return m_sizeScale->value(); }
    void setEraserSize(double size) { m_sizeScale->setValue(size); }

signals:
    void eraserModeChanged(VectorEraserMode mode);
    void eraserSizeChanged(double size);

private:
    void setupUI();

    VectorEraserMode m_mode = VectorEraserMode::Intersection;
    SpinScale* m_sizeScale = nullptr;

    QButtonGroup* m_modeGroup = nullptr;
    QRadioButton* m_radioIntersection = nullptr;
    QRadioButton* m_radioWholeLine = nullptr;
    QRadioButton* m_radioTouchedArea = nullptr;
};

} // namespace Kaburagi
