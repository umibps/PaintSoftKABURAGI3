#pragma once

#include <QWidget>
#include <QScrollBar>
#include <QGridLayout>
#include "CanvasWidget.hpp"

namespace Kaburagi {

class CanvasContainer : public QWidget {
    Q_OBJECT
public:
    explicit CanvasContainer(CanvasWidget* canvas, QWidget* parent = nullptr);
    ~CanvasContainer() override = default;

    CanvasWidget* canvas() const { return m_canvas; }

public slots:
    void updateScrollBars();

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onHScrollBarValueChanged(int value);
    void onVScrollBarValueChanged(int value);

private:
    CanvasWidget* m_canvas = nullptr;
    QScrollBar* m_hScrollBar = nullptr;
    QScrollBar* m_vScrollBar = nullptr;
    QWidget* m_cornerWidget = nullptr;

    bool m_isUpdating = false;
};

} // namespace Kaburagi
