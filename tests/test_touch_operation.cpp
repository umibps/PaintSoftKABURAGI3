#include <QApplication>
#include <cassert>
#include <iostream>
#include "CanvasWidget.hpp"
#include "PreferencesDialog.hpp"
#include "Document.hpp"
#include <QTouchEvent>
#include <QPointingDevice>

using namespace Kaburagi;

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    std::cout << "=== Running test_touch_operation ===" << std::endl;

    // 1. PreferencesDialog デフォルト値の検証
    {
        // 既存設定に依存しないようにテスト
        QSettings settings("Kaburagi", "PaintSoftKABURAGI");
        settings.remove("Preferences/touchAction");
        assert(PreferencesDialog::touchAction() == PreferencesDialog::TouchAction::PanZoomRotate);
        std::cout << "[PASS] Default touchAction is PanZoomRotate" << std::endl;
    }

    // 2. CanvasWidget の初期化と touchAction
    CanvasWidget canvas;
    assert(canvas.touchAction() == CanvasWidget::TouchAction::PanZoomRotate);
    std::cout << "[PASS] CanvasWidget initialized with PanZoomRotate" << std::endl;

    // 3. ドキュメントを作成して描画と cancelCurrentStroke() の検証
    auto doc = std::make_unique<Document>(200, 200);
    canvas.setDocument(doc.get());

    auto layer = doc->activeLayer();
    assert(layer != nullptr);

    // 初期タイルの確認 (空)
    assert(layer->tileManager().tileCount() == 0);

    // 描画開始
    canvas.testStrokeBegin(QPointF(50.0, 50.0), 1.0f);
    assert(layer->tileManager().tileCount() > 0);

    // cancelCurrentStroke() によるロールバック
    canvas.cancelCurrentStroke();
    assert(layer->tileManager().tileCount() == 0);
    std::cout << "[PASS] cancelCurrentStroke successfully rolled back raster tiles" << std::endl;

    // 4. TouchAction::Draw 状態で2本指タッチが来た時の自動ロールバック検証
    canvas.setTouchAction(CanvasWidget::TouchAction::Draw);
    canvas.testStrokeBegin(QPointF(30.0, 30.0), 1.0f);
    assert(layer->tileManager().tileCount() > 0);

    // 2本指タッチイベントを作成して sendEvent に送信
    QList<QEventPoint> touchPoints;
    touchPoints.append(QEventPoint(0, QEventPoint::State::Stationary, QPointF(30.0, 30.0), QPointF(30.0, 30.0)));
    touchPoints.append(QEventPoint(1, QEventPoint::State::Updated, QPointF(60.0, 60.0), QPointF(55.0, 55.0)));

    QTouchEvent touchEvent(QEvent::TouchUpdate, nullptr, Qt::NoModifier, touchPoints);
    bool handled = QApplication::sendEvent(&canvas, &touchEvent);
    assert(handled == true);
    // 直前のストロークが cancelCurrentStroke() されてタイルがロールバックされていること
    assert(layer->tileManager().tileCount() == 0);
    std::cout << "[PASS] Multi-touch event automatically cancels accidental stroke in Draw mode" << std::endl;

    // 5. isTouchPointerEvent の判定検証
    {
        assert(canvas.isTouchPointerEvent(&touchEvent) == true);
        QMouseEvent mouseEv(QEvent::MouseButtonPress, QPointF(10, 10), QPointF(10, 10), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        // 通常のマウスイベント（NotSynthesized, Mouseデバイス）は false
        assert(canvas.isTouchPointerEvent(&mouseEv) == false);
        std::cout << "[PASS] isTouchPointerEvent properly identifies touch vs mouse events" << std::endl;
    }

    // 6. ColorSelect ツールのペンタブレットドラッグ操作検証
    {
        canvas.setCurrentTool(ToolType::ColorSelect);
        // キャンバスの特定ピクセルに赤色を塗る
        layer->tileManager().setPixel(50, 50, 0xFFFF0000);
        layer->tileManager().setPixel(51, 50, 0xFFFF0000);
        layer->tileManager().setPixel(80, 80, 0xFF00FF00); // 別の色
        doc->composite();

        // 1回目のタップ (Press)
        QMouseEvent pressEv(QEvent::MouseButtonPress, QPointF(50, 50), QPointF(50, 50), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        bool pRet = QApplication::sendEvent(&canvas, &pressEv);
        assert(pRet == true);
        assert(doc->hasSelection());

        // ペンタブレットからの高頻度ドラッグ (1000回連続の MouseMove / Drag イベント)
        for (int i = 0; i < 1000; ++i) {
            QMouseEvent moveEv(QEvent::MouseMove, QPointF(50 + (i % 20), 50 + (i % 20)), 
                               QPointF(50 + (i % 20), 50 + (i % 20)), 
                               Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            bool ret = QApplication::sendEvent(&canvas, &moveEv);
            assert(ret == true);
        }

        // ドラッグ終了 (Release)
        QMouseEvent releaseEv(QEvent::MouseButtonRelease, QPointF(50, 50), QPointF(50, 50), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        bool rRet = QApplication::sendEvent(&canvas, &releaseEv);
        assert(rRet == true);
        assert(doc->hasSelection());
        std::cout << "[PASS] ColorSelect high-frequency drag events handled smoothly without overload" << std::endl;
    }

    std::cout << "All test_touch_operation assertions passed successfully!" << std::endl;
    return 0;
}
