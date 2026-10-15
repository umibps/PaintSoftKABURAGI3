#include <QApplication>
#include <QScrollBar>
#include <QSettings>
#include <QMenu>
#include <cassert>
#include <iostream>
#include "Document.hpp"
#include "LayerPanelWidget.hpp"
#include "CanvasWidget.hpp"

using namespace Kaburagi;

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    std::cout << "[TEST] Starting LayerPanelWidget tests..." << std::endl;

    Document doc(800, 600, 350.0);
    LayerPanelWidget panel;
    panel.setDocument(&doc);
    panel.resize(300, 400);
    panel.show();

    // Doc has 2 layers: "背景" (index 0) and "レイヤー 1" (index 1)
    auto listWidget = panel.findChild<LayerListWidget*>();
    assert(listWidget != nullptr);
    assert(listWidget->count() == 2);

    // List item 0 is top layer ("レイヤー 1"), item 1 is bottom ("背景")
    auto item0 = listWidget->item(0);
    assert(item0 != nullptr);
    assert(item0->checkState() == Qt::Checked);

    auto layer1 = doc.getLayer(1);
    assert(layer1 != nullptr);
    assert(layer1->isVisible() == true);

    // Simulate clicking on the eye icon (at x=10, y inside item 0)
    QRect r0 = listWidget->visualItemRect(item0);
    QPoint eyePos(r0.left() + 10, r0.center().y());

    QMouseEvent clickEvent(QEvent::MouseButtonPress, eyePos, eyePos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    app.sendEvent(listWidget->viewport(), &clickEvent);

    // Verify visibility toggled to false
    assert(layer1->isVisible() == false);
    assert(item0->checkState() == Qt::Unchecked);
    std::cout << "[PASS] Layer visibility toggled to false on eye click!" << std::endl;

    // Simulate clicking again on the eye icon
    QMouseEvent clickEvent2(QEvent::MouseButtonPress, eyePos, eyePos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    app.sendEvent(listWidget->viewport(), &clickEvent2);

    // Verify visibility toggled back to true
    assert(layer1->isVisible() == true);
    assert(item0->checkState() == Qt::Checked);
    std::cout << "[PASS] Layer visibility toggled to true on second eye click!" << std::endl;

    // Simulate clicking layer body (x=80)
    // Should NOT toggle visibility
    QPoint bodyPos(r0.left() + 80, r0.center().y());
    QMouseEvent clickBody(QEvent::MouseButtonPress, bodyPos, bodyPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    app.sendEvent(listWidget->viewport(), &clickBody);
    assert(layer1->isVisible() == true);
    std::cout << "[PASS] Clicking layer body did not toggle visibility!" << std::endl;

    // Double clicking eye icon should NOT open editor
    QMouseEvent dblClickEye(QEvent::MouseButtonDblClick, eyePos, eyePos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    app.sendEvent(listWidget->viewport(), &dblClickEye);
    assert(listWidget->state() != QAbstractItemView::EditingState);
    std::cout << "[PASS] Double clicking eye icon does not trigger rename editor!" << std::endl;

    // --- Test Scroll Bar Position (Right vs Left) ---
    std::cout << "[TEST] Testing scroll bar position (Left / Right)..." << std::endl;

    // Add many layers to make vertical scrollbar active
    for (int i = 2; i <= 25; ++i) {
        doc.addRasterLayer();
    }
    app.processEvents();

    assert(listWidget->count() == 26);
    assert(listWidget->verticalScrollBar() != nullptr);

    // Default or Right: scrollbar on the right
    panel.setScrollBarPosition(false);
    app.processEvents();
    assert(!listWidget->isScrollBarOnLeft());
    assert(listWidget->layoutDirection() == Qt::LeftToRight);
    if (listWidget->verticalScrollBar()->isVisible()) {
        assert(listWidget->verticalScrollBar()->geometry().x() > listWidget->viewport()->geometry().x());
        std::cout << "[PASS] Scrollbar is on the right side!" << std::endl;
    }

    // Switch to Left: scrollbar on the left
    panel.setScrollBarPosition(true);
    app.processEvents();
    assert(listWidget->isScrollBarOnLeft());
    assert(listWidget->layoutDirection() == Qt::RightToLeft);
    if (listWidget->verticalScrollBar()->isVisible()) {
        assert(listWidget->verticalScrollBar()->geometry().x() < listWidget->viewport()->geometry().x());
        std::cout << "[PASS] Scrollbar is on the left side!" << std::endl;
    }

    // Check clicking eye icon when scrollbar is on the left
    auto topItem = listWidget->item(0);
    assert(topItem != nullptr);
    int topLayerIdx = static_cast<int>(doc.layers().size()) - 1;
    auto topLayer = doc.getLayer(topLayerIdx);
    assert(topLayer != nullptr);
    assert(topLayer->isVisible() == true);

    QRect rTop = listWidget->visualItemRect(topItem);
    QPoint eyePosLeft(rTop.left() + 10, rTop.center().y());
    QMouseEvent clickEyeLeft(QEvent::MouseButtonPress, eyePosLeft, eyePosLeft, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    app.sendEvent(listWidget->viewport(), &clickEyeLeft);
    assert(topLayer->isVisible() == false);
    std::cout << "[PASS] Eye click works correctly with scrollbar on the left!" << std::endl;

    // Toggle back
    QMouseEvent clickEyeLeft2(QEvent::MouseButtonPress, eyePosLeft, eyePosLeft, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    app.sendEvent(listWidget->viewport(), &clickEyeLeft2);
    assert(topLayer->isVisible() == true);
    std::cout << "[PASS] Second eye click works correctly with scrollbar on the left!" << std::endl;

    // Test PreferencesDialog integration & updateScrollBarPosition
    {
        QSettings settings;
        // Test setting to Left in settings
        settings.setValue("Preferences/layerScrollBarPosition", 1);
        panel.updateScrollBarPosition();
        app.processEvents();
        assert(listWidget->isScrollBarOnLeft());

        // Test setting to Right in settings
        settings.setValue("Preferences/layerScrollBarPosition", 0);
        panel.updateScrollBarPosition();
        app.processEvents();
        assert(!listWidget->isScrollBarOnLeft());
        std::cout << "[PASS] PreferencesDialog integration and updateScrollBarPosition work!" << std::endl;
    }

    // --- Test Context Menu Items on Right-Click ---
    std::cout << "[TEST] Testing Layer Context Menu items on right-click..." << std::endl;
    {
        QRect r = listWidget->visualItemRect(topItem);
        QPoint centerPt = r.center();
        QMenu* ctxMenu = panel.createContextMenu(centerPt);
        assert(ctxMenu != nullptr);

        QList<QAction*> actions = ctxMenu->actions();
        QStringList actionTexts;
        for (auto act : actions) {
            actionTexts << act->text();
        }

        assert(actionTexts.contains("新規通常レイヤー"));
        assert(actionTexts.contains("新規ベクトルレイヤー"));
        assert(actionTexts.contains("新規レイヤーグループ"));
        assert(actionTexts.contains("レイヤーを複製"));
        assert(actionTexts.contains("下のレイヤーと結合"));
        assert(actionTexts.contains("レイヤーを削除"));
        assert(actionTexts.contains("不透明部分を選択範囲にする"));
        assert(actionTexts.contains("不透明部分を選択範囲に加える"));

        QAction* actNewRaster = nullptr;
        for (auto act : actions) {
            if (act->text() == "新規通常レイヤー") actNewRaster = act;
        }
        assert(actNewRaster != nullptr);
        int oldCount = static_cast<int>(doc.layers().size());
        actNewRaster->trigger();
        assert(static_cast<int>(doc.layers().size()) == oldCount + 1);

        delete ctxMenu;
        std::cout << "[PASS] Context menu contains all requested actions and functions properly!" << std::endl;
    }

    // --- Test scrollToLayer ---
    std::cout << "[TEST] Testing scrollToLayer centering..." << std::endl;
    {
        panel.scrollToLayer(5, true);
        app.processEvents();
        std::cout << "[PASS] scrollToLayer executed successfully!" << std::endl;
    }

    // --- Test LayerSelectTool & Shift+Ctrl+Click on CanvasWidget ---
    std::cout << "[TEST] Testing CanvasWidget Layer Selection..." << std::endl;
    {
        Document testDoc(400, 400, 350.0);
        // testDoc starts with:
        // index 0: "背景" (white, 0xFFFFFFFF)
        // index 1: "レイヤー 1" (transparent)

        auto l2 = testDoc.addRasterLayer("レイヤー 2"); // index 2
        auto l1 = testDoc.getLayer(1);

        // Draw red pixel on Layer 1 at (100, 100)
        l1->tileManager().setPixel(100, 100, 0xFFFF0000);

        // Draw blue pixel on Layer 2 at (100, 100) and (200, 200)
        l2->tileManager().setPixel(100, 100, 0xFF0000FF);
        l2->tileManager().setPixel(200, 200, 0xFF0000FF);

        CanvasWidget canvas;
        canvas.setDocument(&testDoc);
        canvas.resize(400, 400);
        canvas.show();
        app.processEvents();

        int selectedLayerIndex = -1;
        QObject::connect(&canvas, &CanvasWidget::layerSelectedFromCanvas, [&](int idx) {
            selectedLayerIndex = idx;
        });

        // 1. ToolType::LayerSelect active: click at (100, 100)
        // Both Layer 2 and Layer 1 have drawing, but Layer 2 is topmost -> selects Layer 2 (index 2)
        canvas.setCurrentTool(ToolType::LayerSelect);
        testDoc.setActiveLayerIndex(0); // reset active to background
        selectedLayerIndex = -1;

        QMouseEvent clickEv1(QEvent::MouseButtonPress, QPointF(100, 100), QPointF(100, 100), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        app.sendEvent(&canvas, &clickEv1);
        app.processEvents();

        assert(selectedLayerIndex == 2);
        assert(testDoc.activeLayerIndex() == 2);
        std::cout << "[PASS] LayerSelectTool selects topmost drawing layer (Layer 2) at overlapping pixel!" << std::endl;

        // 2. Hide Layer 2 -> clicking at (100, 100) should now select Layer 1 (index 1)
        l2->setVisible(false);
        testDoc.setActiveLayerIndex(0);
        selectedLayerIndex = -1;

        app.sendEvent(&canvas, &clickEv1);
        app.processEvents();

        assert(selectedLayerIndex == 1);
        assert(testDoc.activeLayerIndex() == 1);
        std::cout << "[PASS] LayerSelectTool skips hidden Layer 2 and selects visible Layer 1!" << std::endl;

        // Re-enable Layer 2
        l2->setVisible(true);

        // 3. Click at empty area (300, 300) -> selects background layer (index 0)
        selectedLayerIndex = -1;
        QMouseEvent clickEvEmpty(QEvent::MouseButtonPress, QPointF(300, 300), QPointF(300, 300), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        app.sendEvent(&canvas, &clickEvEmpty);
        app.processEvents();

        assert(selectedLayerIndex == 0);
        assert(testDoc.activeLayerIndex() == 0);
        std::cout << "[PASS] LayerSelectTool selects background layer on empty area!" << std::endl;

        // 4. Test Shift+Ctrl+Click on normal layer with another tool active (e.g. Pen)
        canvas.setCurrentTool(ToolType::Pen);
        testDoc.setActiveLayerIndex(1); // Layer 1 is a normal/raster layer
        selectedLayerIndex = -1;

        QMouseEvent clickCtrlShift(QEvent::MouseButtonPress, QPointF(200, 200), QPointF(200, 200),
                                   Qt::LeftButton, Qt::LeftButton, Qt::ControlModifier | Qt::ShiftModifier);
        app.sendEvent(&canvas, &clickCtrlShift);
        app.processEvents();

        assert(selectedLayerIndex == 2);
        assert(testDoc.activeLayerIndex() == 2);
        std::cout << "[PASS] Shift+Ctrl+Click on normal layer triggers layer selection to Layer 2!" << std::endl;

        // 5. Test Shift+Click straight line drawing on normal layer (GIMP style)
        canvas.setCurrentTool(ToolType::Pen);
        canvas.setCurrentColor(QColor(255, 0, 0, 255));
        canvas.setBrushSize(5.0f);
        testDoc.setActiveLayerIndex(1); // Normal raster layer

        // First click (without Shift) creates a point/stroke and sets last stroke end pos
        QMouseEvent clickNormal(QEvent::MouseButtonPress, QPointF(50, 50), QPointF(50, 50),
                                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        app.sendEvent(&canvas, &clickNormal);
        QMouseEvent releaseNormal(QEvent::MouseButtonRelease, QPointF(50, 50), QPointF(50, 50),
                                  Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        app.sendEvent(&canvas, &releaseNormal);
        app.processEvents();

        assert(canvas.hasLastStrokeEndPos());
        assert(std::abs(canvas.lastStrokeEndPos().x() - 50.0) < 1.0);
        assert(std::abs(canvas.lastStrokeEndPos().y() - 50.0) < 1.0);

        // Second click with ShiftModifier triggers straight line from (50, 50) to (150, 50)
        QMouseEvent clickShift(QEvent::MouseButtonPress, QPointF(150, 50), QPointF(150, 50),
                               Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
        app.sendEvent(&canvas, &clickShift);
        app.processEvents();

        assert(canvas.hasLastStrokeEndPos());
        assert(std::abs(canvas.lastStrokeEndPos().x() - 150.0) < 1.0);
        assert(std::abs(canvas.lastStrokeEndPos().y() - 50.0) < 1.0);

        // Check that pixels along the straight line are painted
        QColor midPixel = testDoc.samplePixel(100, 50);
        assert(midPixel.alpha() > 0);
        std::cout << "[PASS] Shift+Click straight line drawing painted line from (50, 50) to (150, 50)!" << std::endl;

        // Third click with ShiftModifier continues from (150, 50) to (150, 150)
        QMouseEvent clickShift2(QEvent::MouseButtonPress, QPointF(150, 150), QPointF(150, 150),
                                Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
        app.sendEvent(&canvas, &clickShift2);
        app.processEvents();

        assert(std::abs(canvas.lastStrokeEndPos().x() - 150.0) < 1.0);
        assert(std::abs(canvas.lastStrokeEndPos().y() - 150.0) < 1.0);
        QColor midPixel2 = testDoc.samplePixel(150, 100);
        assert(midPixel2.alpha() > 0);
        std::cout << "[PASS] Continuous Shift+Click straight line drawing works!" << std::endl;

        // 6. Test Touch Action Preferences and canvas response
        std::cout << "[TEST] Testing Touch Action Preferences (Draw, PanZoomRotate, None)..." << std::endl;

        // Verify PreferencesDialog persistence
        PreferencesDialog::setTouchAction(PreferencesDialog::TouchAction::Draw);
        assert(PreferencesDialog::touchAction() == PreferencesDialog::TouchAction::Draw);

        PreferencesDialog::setTouchAction(PreferencesDialog::TouchAction::PanZoomRotate);
        assert(PreferencesDialog::touchAction() == PreferencesDialog::TouchAction::PanZoomRotate);

        PreferencesDialog::setTouchAction(PreferencesDialog::TouchAction::None);
        assert(PreferencesDialog::touchAction() == PreferencesDialog::TouchAction::None);
        std::cout << "[PASS] PreferencesDialog touchAction getters and setters work!" << std::endl;

        // Test CanvasWidget with TouchAction::None
        canvas.setTouchAction(CanvasWidget::TouchAction::None);
        assert(canvas.touchAction() == CanvasWidget::TouchAction::None);

        // Synthesized mouse event should be blocked when TouchAction::None
        QPointF testPt(200, 200);
        QMouseEvent synthClickNone(QEvent::MouseButtonPress, testPt, testPt, testPt,
                                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier, Qt::MouseEventSynthesizedByQt);
        app.sendEvent(&canvas, &synthClickNone);
        app.processEvents();
        assert(!canvas.isDrawing());
        std::cout << "[PASS] TouchAction::None blocks synthesized mouse press (no drawing)!" << std::endl;

        // Test CanvasWidget with TouchAction::PanZoomRotate
        canvas.setTouchAction(CanvasWidget::TouchAction::PanZoomRotate);
        assert(canvas.touchAction() == CanvasWidget::TouchAction::PanZoomRotate);

        QMouseEvent synthClickPan(QEvent::MouseButtonPress, testPt, testPt, testPt,
                                  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier, Qt::MouseEventSynthesizedByQt);
        app.sendEvent(&canvas, &synthClickPan);
        app.processEvents();
        assert(!canvas.isDrawing());
        std::cout << "[PASS] TouchAction::PanZoomRotate blocks synthesized mouse press (no drawing)!" << std::endl;

        // Test CanvasWidget with TouchAction::Draw (default)
        canvas.setTouchAction(CanvasWidget::TouchAction::Draw);
        assert(canvas.touchAction() == CanvasWidget::TouchAction::Draw);

        QMouseEvent synthClickDraw(QEvent::MouseButtonPress, testPt, testPt, testPt,
                                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier, Qt::MouseEventSynthesizedByQt);
        app.sendEvent(&canvas, &synthClickDraw);
        app.processEvents();
        assert(canvas.isDrawing());
        QMouseEvent synthReleaseDraw(QEvent::MouseButtonRelease, testPt, testPt, testPt,
                                     Qt::LeftButton, Qt::NoButton, Qt::NoModifier, Qt::MouseEventSynthesizedByQt);
        app.sendEvent(&canvas, &synthReleaseDraw);
        app.processEvents();
        assert(!canvas.isDrawing());
        std::cout << "[PASS] TouchAction::Draw allows synthesized mouse press and draws with brush!" << std::endl;

        // Restore TouchAction setting
        PreferencesDialog::setTouchAction(PreferencesDialog::TouchAction::Draw);
    }

    std::cout << "[ALL LAYER PANEL TESTS PASSED SUCCESSFULLY!]" << std::endl;
    return 0;
}
