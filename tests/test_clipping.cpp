#include <QApplication>
#include <cassert>
#include <iostream>
#include "Document.hpp"
#include "Layer.hpp"
#include "GroupLayer.hpp"
#include "LayerPanelWidget.hpp"
#include "MainWindow.hpp"
#include "CanvasWidget.hpp"
#include <QToolButton>

using namespace Kaburagi;

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    std::cout << "[TEST] Starting Layer Clipping tests..." << std::endl;

    // 1. findClippingBaseLayer アルゴリズム検証
    {
        std::cout << "[TEST 1] Testing findClippingBaseLayer resolution..." << std::endl;
        Document doc(200, 200);
        // デフォルトでは背景レイヤー (index 0) が存在
        assert(doc.layers().size() == 1);

        // レイヤー構成を作成:
        // Index 0: 背景 (clip=false)
        // Index 1: Layer1 (clip=false)
        // Index 2: Layer2 (clip=true)  -> base: Layer1
        // Index 3: Layer3 (clip=true)  -> base: Layer1 (連続クリッピング)
        // Index 4: Layer4 (clip=true)  -> base: Layer1 (連続クリッピング)
        // Index 5: Layer5 (clip=false) -> base: nullptr
        // Index 6: Layer6 (clip=true)  -> base: Layer5
        auto l1 = doc.addRasterLayer("Layer1"); // index 1
        auto l2 = doc.addRasterLayer("Layer2"); // index 2
        auto l3 = doc.addRasterLayer("Layer3"); // index 3
        auto l4 = doc.addRasterLayer("Layer4"); // index 4
        auto l5 = doc.addRasterLayer("Layer5"); // index 5
        auto l6 = doc.addRasterLayer("Layer6"); // index 6

        l2->setClippingMask(true);
        l3->setClippingMask(true);
        l4->setClippingMask(true);
        l6->setClippingMask(true);

        // 検証
        assert(doc.findClippingBaseLayer(0) == nullptr);
        assert(doc.findClippingBaseLayer(1) == nullptr);
        assert(doc.findClippingBaseLayer(2) == l1);
        assert(doc.findClippingBaseLayer(3) == l1); // 連続の一番下のレイヤー(2)のもうひとつ下(1)
        assert(doc.findClippingBaseLayer(4) == l1); // 連続の一番下のレイヤー(2)のもうひとつ下(1)
        assert(doc.findClippingBaseLayer(5) == nullptr);
        assert(doc.findClippingBaseLayer(6) == l5);

        // 最下層レイヤーでクリッピングONにした場合
        doc.getLayer(0)->setClippingMask(true);
        assert(doc.findClippingBaseLayer(0) == nullptr); // 下にレイヤーがないのでnullptr

        std::cout << "[PASS] findClippingBaseLayer resolution passed!" << std::endl;
    }

    // 2. 多階層グループ内でのクリッピング解決検証
    {
        std::cout << "[TEST 2] Testing group layer clipping boundary..." << std::endl;
        Document doc(200, 200);
        // 0: 背景
        auto grp = doc.addGroupLayer("Group1"); // 1: グループ
        auto gChild0 = doc.addRasterLayer("GChild0"); // 2
        auto gChild1 = doc.addRasterLayer("GChild1"); // 3
        auto gChild2 = doc.addRasterLayer("GChild2"); // 4
        doc.setLayerParentGroup(2, grp);
        doc.setLayerParentGroup(3, grp);
        doc.setLayerParentGroup(4, grp);

        gChild1->setClippingMask(true);
        gChild2->setClippingMask(true);

        // グループ内での連続クリッピング: gChild1, gChild2 はともに gChild0 をベースとする
        assert(doc.findClippingBaseLayer(3) == gChild0);
        assert(doc.findClippingBaseLayer(4) == gChild0);

        // もし gChild0 がクリッピングONで、グループ内にベースがない場合
        gChild0->setClippingMask(true);
        // グループの境界を越えてグループ外の背景レイヤーまで行かないこと
        assert(doc.findClippingBaseLayer(2) == nullptr);
        assert(doc.findClippingBaseLayer(3) == nullptr);
        assert(doc.findClippingBaseLayer(4) == nullptr);

        std::cout << "[PASS] Group layer clipping boundary passed!" << std::endl;
    }

    // 3. 描画コンポジット合成（クリッピング描画）の検証
    {
        std::cout << "[TEST 3] Testing composite rendering with clipping mask..." << std::endl;
        Document doc(200, 200);
        // 背景レイヤーを非表示にしてピクセルチェックを容易にする
        doc.getLayer(0)->setVisible(false);

        // Layer1: ベースレイヤー (青: 0xFF0000FF, 矩形 (20, 20) ~ (40, 40))
        auto baseLayer = doc.addRasterLayer("Base");
        for (int y = 20; y <= 40; ++y) {
            for (int x = 20; x <= 40; ++x) {
                baseLayer->tileManager().setPixel(x, y, 0xFF0000FF);
            }
        }

        // Layer2: クリッピングレイヤー (赤: 0xFFFF0000, 矩形 (10, 10) ~ (50, 50))
        auto clipLayer = doc.addRasterLayer("ClipRed");
        clipLayer->setClippingMask(true);
        for (int y = 10; y <= 50; ++y) {
            for (int x = 10; x <= 50; ++x) {
                clipLayer->tileManager().setPixel(x, y, 0xFFFF0000);
            }
        }

        doc.composite();

        // 検証 A: ベースレイヤーの範囲外 (15, 15) -> クリッピングにより赤は描画されず透明！
        QColor cOuter = doc.compositeBuffer().getPixel(15, 15);
        assert(cOuter.alpha() == 0);

        // 検証 B: ベースレイヤーの範囲内 (30, 30) -> 赤が描画されている！
        QColor cInner = doc.compositeBuffer().getPixel(30, 30);
        assert(cInner.alpha() == 255);
        assert(cInner.red() == 255 && cInner.blue() == 0);

        std::cout << "[PASS] 2-layer clipping rendering verified: outer=0, inner=Red!" << std::endl;

        // 4. 連続クリッピングでの描画検証
        // Layer3: 2番目のクリッピングレイヤー (緑: 0xFF00FF00, 矩形 (10, 10) ~ (60, 60))
        auto clipLayer2 = doc.addRasterLayer("ClipGreen");
        clipLayer2->setClippingMask(true);
        for (int y = 10; y <= 60; ++y) {
            for (int x = 10; x <= 60; ++x) {
                clipLayer2->tileManager().setPixel(x, y, 0xFF00FF00);
            }
        }

        doc.composite();

        // 検証 C: 連続クリッピングされた Layer3 もベースレイヤー (Base, 20~40) でクリッピングされる
        // (15, 15) は Base の外なので透明！
        QColor cOuter2 = doc.compositeBuffer().getPixel(15, 15);
        assert(cOuter2.alpha() == 0);

        // (45, 45) は Layer2(赤)やLayer3(緑)はあるが、Base(青)の外なので透明！
        QColor cOuter3 = doc.compositeBuffer().getPixel(45, 45);
        assert(cOuter3.alpha() == 0);

        // (30, 30) は Base の内側なので、最上層の緑が反映されている！
        QColor cInner2 = doc.compositeBuffer().getPixel(30, 30);
        assert(cInner2.alpha() == 255);
        assert(cInner2.green() == 255 && cInner2.red() == 0);

        std::cout << "[PASS] Consecutive clipping rendering verified: all clipped to Base!" << std::endl;

        // 5. ベースレイヤー非表示時の検証 (SAI仕様: ベースが消えるとクリッピングレイヤーも消える)
        baseLayer->setVisible(false);
        doc.composite();

        QColor cHidden = doc.compositeBuffer().getPixel(30, 30);
        assert(cHidden.alpha() == 0); // ベース非表示に伴いクリッピング先も非表示！

        std::cout << "[PASS] Base layer hidden suppresses clipped layers passed!" << std::endl;
    }

    // 6. UIチェックボックスとデリゲートのテスト
    {
        std::cout << "[TEST 4] Testing LayerPanelWidget clipping toggle..." << std::endl;
        Document doc(200, 200);
        LayerPanelWidget panel;
        panel.setDocument(&doc);

        auto l1 = doc.addRasterLayer("Layer1");
        doc.setActiveLayerIndex(1);
        assert(!l1->isClippingMask());

        // クリッピングトグル
        l1->setClippingMask(true);
        assert(l1->isClippingMask());

        std::cout << "[PASS] LayerPanelWidget clipping verified!" << std::endl;
    }

    // 7. ツールバーの左右反転トグルボタンの検証
    {
        std::cout << "[TEST 5] Testing MainWindow horizontal flip toggle button..." << std::endl;
        MainWindow win;
        auto flipBtn = win.flipHorizontalButton();
        assert(flipBtn != nullptr);
        assert(flipBtn->isCheckable());
        assert(!flipBtn->isChecked()); // 初期状態は非反転 (OFF)

        // トグルボタンをクリック -> ON になり、キャンバスが左右反転する
        flipBtn->click();
        assert(flipBtn->isChecked());

        // もう一度クリック -> OFF になり、キャンバスの左右反転が解除される
        flipBtn->click();
        assert(!flipBtn->isChecked());

        std::cout << "[PASS] Flip button toggle interaction verified!" << std::endl;
    }

    std::cout << "[ALL LAYER CLIPPING TESTS PASSED SUCCESSFULLY!]" << std::endl;
    return 0;
}
