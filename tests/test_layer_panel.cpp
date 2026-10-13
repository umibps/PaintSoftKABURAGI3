#include <QApplication>
#include <QScrollBar>
#include <QSettings>
#include <QMenu>
#include <cassert>
#include <iostream>
#include "Document.hpp"
#include "LayerPanelWidget.hpp"

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

    std::cout << "[ALL LAYER PANEL TESTS PASSED SUCCESSFULLY!]" << std::endl;
    return 0;
}
