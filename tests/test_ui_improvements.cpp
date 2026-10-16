#include <QApplication>
#include <QMouseEvent>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <cassert>
#include <iostream>
#include "CanvasWidget.hpp"
#include "Document.hpp"
#include "Layer.hpp"
#include "PreferencesDialog.hpp"
#include "ReferenceImageWindow.hpp"
#include "PatternFillSettingsWidget.hpp"
#include "NewCanvasDialog.hpp"

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

    // 4. Bucket Tool Density
    {
        std::cout << "[TEST] 4. Checking Bucket Tool Density..." << std::endl;

        // 4a. BucketSettingsWidget
        BucketSettingsWidget bucketWidget;
        float emittedDensity = 0.0f;
        QObject::connect(&bucketWidget, &BucketSettingsWidget::densityChanged, [&](float d) {
            emittedDensity = d;
        });

        bucketWidget.setDensity(0.5f);
        assert(std::abs(bucketWidget.density() - 0.5f) < 0.01f);

        // 4b. Fill bucket with 50% density
        Document doc(50, 50);
        auto layer = doc.addRasterLayer("FillLayer");
        doc.composite();

        CanvasWidget canvas;
        canvas.setDocument(&doc);
        canvas.setCurrentTool(ToolType::Bucket);
        canvas.setCurrentColor(QColor(0, 0, 0, 255)); // opaque black
        canvas.setBucketDensity(0.5f);
        canvas.setBucketReference(BucketReference::ActiveLayer);
        canvas.setBucketTolerance(10);
        canvas.setBucketExpandPixels(0);

        canvas.fillBucket(QPointF(25, 25));

        // Sample filled pixel
        QColor filledColor = doc.samplePixel(25, 25);
        // Alpha should be ~128 (approx 50% of 255)
        std::cout << "  [INFO] Filled color with 50% density: RGBA("
                  << filledColor.red() << ", "
                  << filledColor.green() << ", "
                  << filledColor.blue() << ", "
                  << filledColor.alpha() << ")" << std::endl;
        assert(filledColor.alpha() >= 120 && filledColor.alpha() <= 135);

        // 4c. Fill bucket with 100% density
        auto layer2 = doc.addRasterLayer("FillLayer2");
        doc.setActiveLayerIndex(doc.activeLayerIndex());
        doc.composite();

        canvas.setBucketDensity(1.0f);
        canvas.fillBucket(QPointF(25, 25));

        QColor filledColor100 = doc.samplePixel(25, 25);
        std::cout << "  [INFO] Filled color with 100% density: RGBA("
                  << filledColor100.red() << ", "
                  << filledColor100.green() << ", "
                  << filledColor100.blue() << ", "
                  << filledColor100.alpha() << ")" << std::endl;
        assert(filledColor100.alpha() == 255);

        std::cout << "  -> PASSED!" << std::endl;
    }

    // 5. Pattern Fill Tool
    {
        std::cout << "[TEST] 5. Checking Pattern Fill Tool..." << std::endl;

        // 5a. PatternFillSettingsWidget
        PatternFillSettingsWidget patWidget;
        patWidget.setDensity(0.8f);
        assert(std::abs(patWidget.density() - 0.8f) < 0.01f);
        patWidget.setPatternScale(2.0f);
        assert(std::abs(patWidget.patternScale() - 2.0f) < 0.01f);
        patWidget.setPatternAngle(45.0f);
        assert(std::abs(patWidget.patternAngle() - 45.0f) < 0.01f);

        // 5b. CanvasWidget fillPattern with a 2x2 custom pattern
        // Pattern: [Red, Blue]
        //          [Blue, Red]
        QImage pattern(2, 2, QImage::Format_ARGB32);
        pattern.setPixelColor(0, 0, QColor(255, 0, 0, 255));
        pattern.setPixelColor(1, 0, QColor(0, 0, 255, 255));
        pattern.setPixelColor(0, 1, QColor(0, 0, 255, 255));
        pattern.setPixelColor(1, 1, QColor(255, 0, 0, 255));

        Document doc(10, 10);
        auto layer = doc.addRasterLayer("PatLayer");
        doc.composite();

        CanvasWidget canvas;
        canvas.setDocument(&doc);
        canvas.setCurrentTool(ToolType::PatternFill);
        canvas.setPatternFillImage(pattern);
        canvas.setPatternFillScale(1.0f);
        canvas.setPatternFillAngle(0.0f);
        canvas.setPatternFillDensity(1.0f);
        canvas.setPatternFillReference(BucketReference::ActiveLayer);
        canvas.setPatternFillTolerance(10);
        canvas.setPatternFillExpandPixels(0);

        canvas.fillPattern(QPointF(5, 5));

        // Check tiled pixels
        QColor c00 = doc.samplePixel(0, 0);
        QColor c10 = doc.samplePixel(1, 0);
        QColor c01 = doc.samplePixel(0, 1);
        QColor c11 = doc.samplePixel(1, 1);

        assert(c00 == QColor(255, 0, 0, 255));
        assert(c10 == QColor(0, 0, 255, 255));
        assert(c01 == QColor(0, 0, 255, 255));
        assert(c11 == QColor(255, 0, 0, 255));

        // Check repeat at (4, 4) -> should be Red (4 % 2 = 0)
        QColor c44 = doc.samplePixel(4, 4);
        assert(c44 == QColor(255, 0, 0, 255));
        // (5, 4) -> should be Blue (5 % 2 = 1)
        QColor c54 = doc.samplePixel(5, 4);
        assert(c54 == QColor(0, 0, 255, 255));

        std::cout << "  [INFO] Tiled coordinates (0,0), (1,0), (4,4), (5,4) matched pattern colors perfectly!" << std::endl;
        std::cout << "  -> PASSED!" << std::endl;
    }

    // 6. NewCanvasDialog keep aspect ratio check
    {
        std::cout << "[TEST] 6. Checking NewCanvasDialog Keep Aspect Ratio..." << std::endl;
        NewCanvasDialog dlg;
        auto widthSpin = dlg.findChild<QSpinBox*>("widthSpin");
        auto heightSpin = dlg.findChild<QSpinBox*>("heightSpin");
        auto keepAspectCheck = dlg.findChild<QCheckBox*>("keepAspectCheck");

        assert(widthSpin != nullptr);
        assert(heightSpin != nullptr);
        assert(keepAspectCheck != nullptr);

        // Initially set to 1600 x 1200
        widthSpin->setValue(1600);
        heightSpin->setValue(1200);

        // Turn ON keep aspect
        keepAspectCheck->setChecked(true);
        assert(dlg.isKeepAspect() == true);

        // Change width to 800 -> height should automatically become 600
        widthSpin->setValue(800);
        assert(heightSpin->value() == 600);
        assert(dlg.canvasWidth() == 800);
        assert(dlg.canvasHeight() == 600);

        // Change height to 900 -> width should automatically become 1200
        heightSpin->setValue(900);
        assert(widthSpin->value() == 1200);

        // Turn OFF keep aspect
        keepAspectCheck->setChecked(false);
        assert(dlg.isKeepAspect() == false);

        // Change width to 500 -> height should stay 900
        widthSpin->setValue(500);
        assert(heightSpin->value() == 900);

        // Turn ON keep aspect at 500 x 900 (aspect ratio 5/9)
        keepAspectCheck->setChecked(true);
        // Change width to 1000 -> height should become 1800
        widthSpin->setValue(1000);
        assert(heightSpin->value() == 1800);

        // Check swap width and height button
        auto swapSizeBtn = dlg.findChild<QPushButton*>("swapSizeBtn");
        assert(swapSizeBtn != nullptr);
        swapSizeBtn->click();
        assert(widthSpin->value() == 1800);
        assert(heightSpin->value() == 1000);
        assert(dlg.canvasWidth() == 1800);
        assert(dlg.canvasHeight() == 1000);

        // Aspect ratio is now inverted (1800 / 1000 = 1.8)
        // Change height to 500 -> width should become 900
        heightSpin->setValue(500);
        assert(widthSpin->value() == 900);

        std::cout << "  -> PASSED!" << std::endl;
    }

    std::cout << "[ALL UI IMPROVEMENTS TESTS PASSED SUCCESSFULLY!]" << std::endl;
    return 0;
}
