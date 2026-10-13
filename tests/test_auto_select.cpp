#include <QApplication>
#include <cassert>
#include <iostream>
#include "Document.hpp"
#include "CanvasWidget.hpp"
#include "AutoSelectSettingsWidget.hpp"

using namespace Kaburagi;

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    std::cout << "[TEST] Starting AutoSelect tests..." << std::endl;

    // 1. Test AutoSelectSettingsWidget
    {
        AutoSelectSettingsWidget widget;
        assert(widget.selectionMode() == SelectionMode::New);
        assert(widget.referenceMode() == BucketReference::Canvas);
        assert(widget.tolerance() == 30);
        assert(widget.expandPixels() == 0);
        assert(widget.isNarrowAreaFill() == true);

        SelectionMode receivedMode = SelectionMode::New;
        QObject::connect(&widget, &AutoSelectSettingsWidget::modeChanged, [&](SelectionMode m) {
            receivedMode = m;
        });

        widget.setSelectionMode(SelectionMode::Add);
        assert(widget.selectionMode() == SelectionMode::Add);

        widget.setReferenceMode(BucketReference::ActiveLayer);
        assert(widget.referenceMode() == BucketReference::ActiveLayer);

        widget.setTolerance(50);
        assert(widget.tolerance() == 50);

        widget.setExpandPixels(3);
        assert(widget.expandPixels() == 3);

        widget.setNarrowAreaFill(false);
        assert(widget.isNarrowAreaFill() == false);

        std::cout << "[PASS] AutoSelectSettingsWidget properties and getters/setters passed!" << std::endl;
    }

    // 2. Test CanvasWidget autoSelect functionality
    {
        Document doc(200, 200, 350.0);
        CanvasWidget canvas;
        canvas.setDocument(&doc);

        auto activeL = doc.activeLayer();
        assert(activeL != nullptr);

        // Draw a solid red square at [20, 20] to [60, 60] (41x41 pixels)
        for (int y = 20; y <= 60; ++y) {
            for (int x = 20; x <= 60; ++x) {
                activeL->tileManager().setPixel(x, y, 0xFFFF0000); // Red
            }
        }

        // Draw a solid blue square at [100, 20] to [140, 60] (41x41 pixels)
        for (int y = 20; y <= 60; ++y) {
            for (int x = 100; x <= 140; ++x) {
                activeL->tileManager().setPixel(x, y, 0xFF0000FF); // Blue
            }
        }

        doc.composite();

        canvas.setAutoSelectMode(SelectionMode::New);
        canvas.setAutoSelectReference(BucketReference::Canvas);
        canvas.setAutoSelectTolerance(10);
        canvas.setAutoSelectExpandPixels(0);
        canvas.setAutoSelectNarrowAreaFill(false);

        // Initially no selection
        assert(!doc.hasSelection());

        // Click inside the red square at (40, 40)
        canvas.autoSelect(QPointF(40, 40));

        assert(doc.hasSelection());
        assert(doc.isPixelSelected(40, 40));
        assert(doc.isPixelSelected(20, 20));
        assert(doc.isPixelSelected(60, 60));
        // Outside the red square should NOT be selected
        assert(!doc.isPixelSelected(19, 19));
        assert(!doc.isPixelSelected(61, 61));
        assert(!doc.isPixelSelected(120, 40)); // Blue square is NOT selected

        std::cout << "[PASS] New AutoSelect on red square passed!" << std::endl;

        // 3. Test Add mode: Click inside blue square with Add mode
        canvas.setAutoSelectMode(SelectionMode::Add);
        canvas.autoSelect(QPointF(120, 40));

        // Both red square and blue square should now be selected
        assert(doc.hasSelection());
        assert(doc.isPixelSelected(40, 40));  // Red still selected
        assert(doc.isPixelSelected(120, 40)); // Blue now selected
        assert(doc.isPixelSelected(100, 20));
        assert(doc.isPixelSelected(140, 60));
        assert(!doc.isPixelSelected(80, 40)); // Gap between them is NOT selected

        std::cout << "[PASS] Add AutoSelect on blue square passed!" << std::endl;

        // 4. Test Subtract mode: Subtract the red square
        canvas.setAutoSelectMode(SelectionMode::Subtract);
        canvas.autoSelect(QPointF(40, 40));

        // Red square should now be deselected, blue remains selected
        assert(doc.hasSelection());
        assert(!doc.isPixelSelected(40, 40)); // Red is deselected
        assert(doc.isPixelSelected(120, 40)); // Blue is still selected

        std::cout << "[PASS] Subtract AutoSelect on red square passed!" << std::endl;

        // 5. Test Undo/Redo
        auto undoStack = doc.undoStack();
        assert(undoStack != nullptr);
        assert(undoStack->canUndo());

        // Undo Subtract -> red is selected again
        undoStack->undo();
        assert(doc.isPixelSelected(40, 40));
        assert(doc.isPixelSelected(120, 40));

        // Undo Add -> only red is selected
        undoStack->undo();
        assert(doc.isPixelSelected(40, 40));
        assert(!doc.isPixelSelected(120, 40));

        // Undo New -> no selection
        undoStack->undo();
        assert(!doc.hasSelection());

        // Redo New -> red selected
        undoStack->redo();
        assert(doc.hasSelection());
        assert(doc.isPixelSelected(40, 40));
        assert(!doc.isPixelSelected(120, 40));

        std::cout << "[PASS] AutoSelect Undo/Redo passed!" << std::endl;

        // 6. Test Expand pixels
        canvas.setAutoSelectMode(SelectionMode::New);
        canvas.setAutoSelectExpandPixels(2);
        canvas.autoSelect(QPointF(40, 40));

        // Because of expand = 2, pixel (19, 19) should now be selected
        assert(doc.isPixelSelected(19, 19));
        assert(doc.isPixelSelected(62, 62));

        std::cout << "[PASS] AutoSelect Expand pixels passed!" << std::endl;
    }

    std::cout << "[ALL AUTO SELECT TESTS PASSED SUCCESSFULLY!]" << std::endl;
    return 0;
}
