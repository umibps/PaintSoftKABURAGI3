#include <QApplication>
#include <QScrollBar>
#include <QSettings>
#include <QMenu>
#include <cassert>
#include <iostream>
#include "Document.hpp"
#include "LayerPanelWidget.hpp"
#include "CanvasWidget.hpp"
#include "SpinScaleWidget.hpp"

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

    // 7. Test Group Expand / Collapse on Left-Click of "[▼ グループ]"
    std::cout << "[TEST] Testing Group Expand/Collapse on Left-Click of '[▼ グループ]'..." << std::endl;
    {
        Document groupDoc(400, 400, 350.0);
        LayerPanelWidget gPanel;
        gPanel.setDocument(&groupDoc);
        gPanel.resize(300, 400);
        gPanel.show();
        app.processEvents();

        auto gListWidget = gPanel.findChild<LayerListWidget*>();
        assert(gListWidget != nullptr);

        // Add a group layer and 2 child layers
        auto grp = groupDoc.addGroupLayer("グループ 1");
        assert(grp != nullptr);
        auto child1 = groupDoc.addRasterLayer("子レイヤー 1");
        auto child2 = groupDoc.addRasterLayer("子レイヤー 2");

        auto findIdx = [&](const LayerPtr& l) {
            const auto& layers = groupDoc.layers();
            for (size_t i = 0; i < layers.size(); ++i) {
                if (layers[i] == l) return static_cast<int>(i);
            }
            return -1;
        };

        groupDoc.setLayerParentGroup(findIdx(child1), grp);
        groupDoc.setLayerParentGroup(findIdx(child2), grp);
        app.processEvents();

        // Check initial state: grp is expanded
        assert(grp->isExpanded() == true);

        // Find the group row in gListWidget
        int grpDocIdx = findIdx(grp);
        int grpRow = static_cast<int>(groupDoc.layers().size()) - 1 - grpDocIdx;
        auto grpItem = gListWidget->item(grpRow);
        assert(grpItem != nullptr);
        assert(grpItem->text().contains("[▼ グループ]"));

        int child1DocIdx = findIdx(child1);
        int child1Row = static_cast<int>(groupDoc.layers().size()) - 1 - child1DocIdx;
        int child2DocIdx = findIdx(child2);
        int child2Row = static_cast<int>(groupDoc.layers().size()) - 1 - child2DocIdx;

        // Both children should initially be visible (not row hidden)
        assert(gListWidget->isRowHidden(child1Row) == false);
        assert(gListWidget->isRowHidden(child2Row) == false);

        // Compute location of "[▼ グループ]" in visualItemRect
        QRect rGrp = gListWidget->visualItemRect(grpItem);
        QFontMetrics fm(gListWidget->font());
        int prefixStart = rGrp.left() + 68; // no indent for root group
        int prefixWidth = fm.horizontalAdvance("[▼ グループ]");
        QPoint clickFoldPos(prefixStart + prefixWidth / 2, rGrp.center().y());

        // Test hover cursor over "[▼ グループ]"
        QMouseEvent hoverEv(QEvent::MouseMove, clickFoldPos, clickFoldPos, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        app.sendEvent(gListWidget->viewport(), &hoverEv);
        assert(gListWidget->cursor().shape() == Qt::PointingHandCursor);
        std::cout << "[PASS] Hovering over '[▼ グループ]' shows PointingHandCursor!" << std::endl;

        // 1st click on "[▼ グループ]": should collapse group
        QMouseEvent clickCollapse(QEvent::MouseButtonPress, clickFoldPos, clickFoldPos,
                                 Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        app.sendEvent(gListWidget->viewport(), &clickCollapse);
        app.processEvents();

        assert(grp->isExpanded() == false);
        grpItem = gListWidget->item(grpRow);
        assert(grpItem->text().contains("[▶ グループ]"));
        assert(gListWidget->isRowHidden(child1Row) == true);
        assert(gListWidget->isRowHidden(child2Row) == true);
        std::cout << "[PASS] Left-click on '[▼ グループ]' collapsed the group and hid child layers!" << std::endl;

        // 2nd click on "[▶ グループ]": should expand group
        int prefixWidthCollapsed = fm.horizontalAdvance("[▶ グループ]");
        QPoint clickExpandPos(prefixStart + prefixWidthCollapsed / 2, rGrp.center().y());
        QMouseEvent clickExpand(QEvent::MouseButtonPress, clickExpandPos, clickExpandPos,
                               Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        app.sendEvent(gListWidget->viewport(), &clickExpand);
        app.processEvents();

        assert(grp->isExpanded() == true);
        grpItem = gListWidget->item(grpRow);
        assert(grpItem->text().contains("[▼ グループ]"));
        assert(gListWidget->isRowHidden(child1Row) == false);
        assert(gListWidget->isRowHidden(child2Row) == false);
        std::cout << "[PASS] Left-click on '[▶ グループ]' expanded the group and restored child layers!" << std::endl;

        // Click on group name text (far to the right of "[▼ グループ]"): should NOT collapse
        QPoint clickNamePos(prefixStart + prefixWidth + 50, rGrp.center().y());
        QMouseEvent clickName(QEvent::MouseButtonPress, clickNamePos, clickNamePos,
                              Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        app.sendEvent(gListWidget->viewport(), &clickName);
        app.processEvents();

        assert(grp->isExpanded() == true);
        std::cout << "[PASS] Left-click on group name did not toggle group expand/collapse!" << std::endl;
    }

    // --- Test Drag & Drop Auto-Scroll ---
    {
        std::cout << "[TEST] Testing drag & drop auto-scrolling..." << std::endl;
        Document scrollDoc(800, 600, 350.0);
        LayerPanelWidget scrollPanel;
        scrollPanel.setDocument(&scrollDoc);
        scrollPanel.resize(300, 200);
        scrollPanel.show();

        // Add enough layers to require vertical scrolling
        for (int i = 0; i < 30; ++i) {
            scrollDoc.addRasterLayer();
        }
        app.processEvents();

        auto sListWidget = scrollPanel.findChild<LayerListWidget*>();
        assert(sListWidget != nullptr);
        QScrollBar* vbar = sListWidget->verticalScrollBar();
        assert(vbar != nullptr);
        assert(vbar->maximum() > 0);

        // Position scrollbar in the middle
        int midVal = vbar->maximum() / 2;
        vbar->setValue(midVal);
        app.processEvents();
        assert(vbar->value() == midVal);

        // Start auto-scroll with active drag
        sListWidget->setDragStartRow(5);
        sListWidget->startAutoScroll();

        // 1. Cursor above viewport (upper outside) -> should scroll up
        QPoint aboveViewport = sListWidget->viewport()->mapToGlobal(QPoint(50, -20));
        sListWidget->setMockCursorPos(aboveViewport);
        sListWidget->onAutoScrollTimeout();
        app.processEvents();
        int afterScrollUp = vbar->value();
        if (afterScrollUp >= midVal) {
            std::cerr << "FAILED: auto-scroll up did not decrease scroll value! midVal=" << midVal << ", afterScrollUp=" << afterScrollUp << std::endl;
            return 1;
        }
        std::cout << "[PASS] Auto-scroll scrolled UP when cursor is above viewport (from "
                  << midVal << " to " << afterScrollUp << ")!" << std::endl;

        // 2. Cursor below viewport (lower outside) -> should scroll down
        int vh = sListWidget->viewport()->height();
        QPoint belowViewport = sListWidget->viewport()->mapToGlobal(QPoint(50, vh + 30));
        sListWidget->setMockCursorPos(belowViewport);
        sListWidget->onAutoScrollTimeout();
        app.processEvents();
        int afterScrollDown = vbar->value();
        if (afterScrollDown <= afterScrollUp) {
            std::cerr << "FAILED: auto-scroll down did not increase scroll value! afterScrollUp=" << afterScrollUp << ", afterScrollDown=" << afterScrollDown << std::endl;
            return 1;
        }
        std::cout << "[PASS] Auto-scroll scrolled DOWN when cursor is below viewport (from "
                  << afterScrollUp << " to " << afterScrollDown << ")!" << std::endl;

        // 3. Stop auto-scroll
        sListWidget->stopAutoScroll();
        sListWidget->setDragStartRow(-1);
        sListWidget->setMockCursorPos(std::nullopt);

        // Drop slot checking when cursor is above viewport
        sListWidget->updateDropSlotFromPos(QPoint(50, -10));
        if (sListWidget->dropSlot() != 0) {
            std::cerr << "FAILED: dropSlot should be 0 when above viewport, got " << sListWidget->dropSlot() << std::endl;
            return 1;
        }
        std::cout << "[PASS] Drop slot is set to top slot (0) when above viewport!" << std::endl;

        // Drop slot checking when cursor is below viewport
        sListWidget->updateDropSlotFromPos(QPoint(50, vh + 50));
        if (sListWidget->dropSlot() > sListWidget->getBackgroundRow()) {
            std::cerr << "FAILED: dropSlot should be clamped to background row, got " << sListWidget->dropSlot() << std::endl;
            return 1;
        }
        std::cout << "[PASS] Drop slot is clamped to valid row range when below viewport!" << std::endl;
    }

    {
        std::cout << "[TEST] Testing Layer Item Opacity display under Layer Name..." << std::endl;
        Document testDoc(200, 200);
        LayerPanelWidget panel;
        panel.setDocument(&testDoc);

        // Document has 背景 (0) and レイヤー 1 (1)
        // In layer list widget:
        // row 0: レイヤー 1
        // row 1: 背景
        auto listWidget = panel.findChild<QListWidget*>();
        assert(listWidget != nullptr);
        assert(listWidget->count() == 2);

        auto item0 = listWidget->item(0);
        assert(item0 != nullptr);
        QString text0 = item0->text();
        assert(text0.contains("\n"));
        QStringList lines0 = text0.split('\n');
        assert(lines0.size() == 2);
        assert(lines0[0].contains("レイヤー 1"));
        assert(lines0[1].contains("不透明度: 100%"));

        // Change opacity via SpinScale widget
        auto opacityScale = panel.findChild<SpinScale*>();
        assert(opacityScale != nullptr);
        opacityScale->setValue(42.0);
        assert(std::abs(testDoc.activeLayer()->opacity() - 0.42f) < 0.01f);
        QString updatedText = listWidget->item(0)->text();
        QStringList updatedLines = updatedText.split('\n');
        assert(updatedLines.size() == 2);
        assert(updatedLines[0].contains("レイヤー 1"));
        assert(updatedLines[1].contains("不透明度: 42%"));

        std::cout << "[PASS] Layer item displays opacity on second row and updates in real-time!" << std::endl;
    }

    std::cout << "[ALL LAYER PANEL TESTS PASSED SUCCESSFULLY!]" << std::endl;
    return 0;
}
