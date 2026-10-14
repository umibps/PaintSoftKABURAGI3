#include <QApplication>
#include <QMouseEvent>
#include <cassert>
#include <iostream>
#include "CanvasWidget.hpp"
#include "Document.hpp"
#include "Layer.hpp"
#include "PreferencesDialog.hpp"
#include "ReferenceImageWindow.hpp"

using namespace Kaburagi;

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    std::cout << "=== RUNNING UI IMPROVEMENTS TESTS ===" << std::endl;

    // 1. isRasterDrawingTool check
    {
        std::cout << "[TEST] 1. Checking isRasterDrawingTool..." << std::endl;
        CanvasWidget canvas;

        assert(canvas.isRasterDrawingTool(ToolType::Pen) == true);
        assert(canvas.isRasterDrawingTool(ToolType::Airbrush) == true);
        assert(canvas.isRasterDrawingTool(ToolType::Watercolor) == true);
        assert(canvas.isRasterDrawingTool(ToolType::Marker) == true);
        assert(canvas.isRasterDrawingTool(ToolType::Eraser) == true);
        assert(canvas.isRasterDrawingTool(ToolType::Bucket) == true);
        assert(canvas.isRasterDrawingTool(ToolType::Blur) == true);
        assert(canvas.isRasterDrawingTool(ToolType::Blend) == true);
        assert(canvas.isRasterDrawingTool(ToolType::Gradient) == true);
        assert(canvas.isRasterDrawingTool(ToolType::CompositeBrush) == true);

        assert(canvas.isRasterDrawingTool(ToolType::Pipette) == false);
        assert(canvas.isRasterDrawingTool(ToolType::RectSelect) == false);
        assert(canvas.isRasterDrawingTool(ToolType::EllipseSelect) == false);
        assert(canvas.isRasterDrawingTool(ToolType::LassoSelect) == false);
        assert(canvas.isRasterDrawingTool(ToolType::MagicWand) == false);
        assert(canvas.isRasterDrawingTool(ToolType::ColorSelect) == false);
        assert(canvas.isRasterDrawingTool(ToolType::Transform) == false);
        assert(canvas.isRasterDrawingTool(ToolType::Text) == false);
        assert(canvas.isRasterDrawingTool(ToolType::TextBalloon) == false);
        assert(canvas.isRasterDrawingTool(ToolType::ThreeD) == false);
        assert(canvas.isRasterDrawingTool(ToolType::VectorControlPoint) == false);
        std::cout << "  -> PASSED!" << std::endl;
    }

    // 2. Right-click Eyedropper on CanvasWidget
    {
        std::cout << "[TEST] 2. Checking Right-Click Eyedropper behavior..." << std::endl;
        Document doc(100, 100);
        QImage img(100, 100, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        img.setPixelColor(10, 10, QColor(255, 0, 0));
        img.setPixelColor(20, 20, QColor(0, 255, 0));
        doc.addRasterLayerWithImage(img, "RasterLayer");
        doc.composite();

        CanvasWidget canvas;
        canvas.setDocument(&doc);
        canvas.resize(200, 200);

        QColor pickedColor;
        QObject::connect(&canvas, &CanvasWidget::colorPicked, [&](const QColor& c) {
            pickedColor = c;
        });

        // 2a. With Pen tool and enabled
        canvas.setCurrentTool(ToolType::Pen);
        canvas.setRightClickEyedropperEnabled(true);

        QPointF docPt(10, 10);
        QPointF viewPt = canvas.docToView(docPt);

        QMouseEvent pressEvent(QEvent::MouseButtonPress, viewPt, viewPt, Qt::RightButton, Qt::RightButton, Qt::NoModifier);
        QApplication::sendEvent(&canvas, &pressEvent);

        assert(pickedColor == QColor(255, 0, 0));

        // 2b. Dragging with right button to (20, 20)
        QPointF docPt2(20, 20);
        QPointF viewPt2 = canvas.docToView(docPt2);

        QMouseEvent moveEvent(QEvent::MouseMove, viewPt2, viewPt2, Qt::RightButton, Qt::RightButton, Qt::NoModifier);
        QApplication::sendEvent(&canvas, &moveEvent);

        assert(pickedColor == QColor(0, 255, 0));

        // Release event
        QMouseEvent releaseEvent(QEvent::MouseButtonRelease, viewPt2, viewPt2, Qt::RightButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&canvas, &releaseEvent);

        // 2c. Disabled setting
        canvas.setRightClickEyedropperEnabled(false);
        pickedColor = QColor(); // reset

        QMouseEvent pressEventDisabled(QEvent::MouseButtonPress, viewPt, viewPt, Qt::RightButton, Qt::RightButton, Qt::NoModifier);
        QApplication::sendEvent(&canvas, &pressEventDisabled);

        assert(!pickedColor.isValid());

        std::cout << "  -> PASSED!" << std::endl;
    }

    // 3. ReferenceImageWindow Drag & Drop acceptance
    {
        std::cout << "[TEST] 3. Checking ReferenceImageWindow Drag & Drop..." << std::endl;
        ReferenceImageWindow refWindow;
        assert(refWindow.acceptDrops() == true);
        std::cout << "  -> PASSED!" << std::endl;
    }

    std::cout << "[ALL UI IMPROVEMENTS TESTS PASSED SUCCESSFULLY!]" << std::endl;
    return 0;
}
