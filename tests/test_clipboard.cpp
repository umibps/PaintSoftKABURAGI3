#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QPainterPath>
#include <iostream>
#include <cassert>
#include "../src/gui/MainWindow.hpp"
#include "../src/core/Document.hpp"
#include "../src/core/Layer.hpp"

using namespace Kaburagi;

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    std::cout << "[TEST] Starting Clipboard (Cut, Copy, Paste) tests..." << std::endl;

    MainWindow window;
    window.show();

    Document* doc = window.document();
    assert(doc != nullptr);

    // 1. 選択範囲がない場合、コピー、切り取り、可視部をコピーが無効であること
    doc->clearSelection();
    window.updateEditActions();
    assert(window.copyAction()->isEnabled() == false);
    assert(window.copyVisibleAction()->isEnabled() == false);
    assert(window.cutAction()->isEnabled() == false);
    std::cout << "[PASS] Copy, Copy Visible, and Cut disabled when no selection verified!" << std::endl;

    // 2. 選択範囲を設定するとコピー、切り取り、可視部をコピーが有効になること
    QPainterPath selPath;
    selPath.addRect(10, 10, 20, 20);
    doc->setSelection(selPath);
    window.updateEditActions();
    assert(window.copyAction()->isEnabled() == true);
    assert(window.copyVisibleAction()->isEnabled() == true);
    assert(window.cutAction()->isEnabled() == true);
    std::cout << "[PASS] Copy, Copy Visible, and Cut enabled when selection exists verified!" << std::endl;

    // 3. コピーのテスト
    auto activeL = doc->activeLayer();
    assert(activeL != nullptr);

    // (10, 10) 〜 (30, 30) に赤色ピクセル (0xFFFF0000) を書き込む
    for (int y = 10; y < 30; ++y) {
        for (int x = 10; x < 30; ++x) {
            int tx = x / TileMemoryPool::TILE_WIDTH;
            int ty = y / TileMemoryPool::TILE_HEIGHT;
            Tile* t = activeL->tileManager().getTileForWrite(tx, ty);
            if (t) {
                int lx = x % TileMemoryPool::TILE_WIDTH;
                int ly = y % TileMemoryPool::TILE_HEIGHT;
                reinterpret_cast<uint32_t*>(t->writeBuffer())[ly * TileMemoryPool::TILE_WIDTH + lx] = 0xFFFF0000;
            }
        }
    }
    activeL->invalidateContentBounds();
    doc->composite();

    window.onCopy();
    QImage copiedImg = QGuiApplication::clipboard()->image();
    assert(!copiedImg.isNull());
    assert(copiedImg.width() == 20);
    assert(copiedImg.height() == 20);
    // コピーされたピクセルが赤色
    QColor cPixel = copiedImg.pixelColor(5, 5);
    assert(cPixel.red() == 255 && cPixel.green() == 0 && cPixel.blue() == 0 && cPixel.alpha() == 255);
    std::cout << "[PASS] Copy selection to clipboard image verified!" << std::endl;

    // 4. 切り取りのテスト (コピー＋選択範囲消去＋Undo)
    window.onCut();
    QImage cutImg = QGuiApplication::clipboard()->image();
    assert(!cutImg.isNull());
    assert(cutImg.width() == 20 && cutImg.height() == 20);

    // アクティブレイヤーの (15, 15) は透明になっていること
    int tx15 = 15 / TileMemoryPool::TILE_WIDTH;
    int ty15 = 15 / TileMemoryPool::TILE_HEIGHT;
    const Tile* tAfterCut = activeL->tileManager().getTile(tx15, ty15);
    uint32_t pAfterCut = reinterpret_cast<const uint32_t*>(tAfterCut->readBuffer())[(15 % TileMemoryPool::TILE_HEIGHT) * TileMemoryPool::TILE_WIDTH + (15 % TileMemoryPool::TILE_WIDTH)];
    assert(pAfterCut == 0x00000000);
    std::cout << "[PASS] Cut cleared selected pixels to transparent verified!" << std::endl;

    // Undo で復元されること
    assert(doc->undoStack()->canUndo());
    doc->undoStack()->undo();
    const Tile* tAfterUndo = activeL->tileManager().getTile(tx15, ty15);
    uint32_t pAfterUndo = reinterpret_cast<const uint32_t*>(tAfterUndo->readBuffer())[(15 % TileMemoryPool::TILE_HEIGHT) * TileMemoryPool::TILE_WIDTH + (15 % TileMemoryPool::TILE_WIDTH)];
    assert(pAfterUndo == 0xFFFF0000);
    std::cout << "[PASS] Undo restored cut pixels verified!" << std::endl;

    // 4-2. 可視部をコピーのテスト (複数レイヤーの合成結果コピー)
    // 現在 activeL (レイヤー1) の (10, 10) 〜 (30, 30) は赤色 (0xFFFF0000)
    // 上層に新規レイヤー (レイヤー2) を追加し、(10, 10) 〜 (20, 30) (左半分) に緑色 (0xFF00FF00) を書き込む
    auto layer2 = doc->addRasterLayer("レイヤー 2");
    assert(layer2 != nullptr);
    for (int y = 10; y < 30; ++y) {
        for (int x = 10; x < 20; ++x) {
            int tx = x / TileMemoryPool::TILE_WIDTH;
            int ty = y / TileMemoryPool::TILE_HEIGHT;
            Tile* t = layer2->tileManager().getTileForWrite(tx, ty);
            if (t) {
                int lx = x % TileMemoryPool::TILE_WIDTH;
                int ly = y % TileMemoryPool::TILE_HEIGHT;
                reinterpret_cast<uint32_t*>(t->writeBuffer())[ly * TileMemoryPool::TILE_WIDTH + lx] = 0xFF00FF00; // 緑
            }
        }
    }
    layer2->invalidateContentBounds();
    doc->composite();

    // 選択範囲 (10, 10, 20, 20) を設定
    doc->setSelection(selPath);
    window.updateEditActions();

    // 通常の「コピー」を実行した場合: アクティブレイヤー(レイヤー2)のみなので、左半分(5, 5)は緑、右半分(15, 5)は透明(0)
    window.onCopy();
    QImage singleLayerCopy = QGuiApplication::clipboard()->image();
    assert(singleLayerCopy.pixelColor(5, 5).green() == 255);
    assert(singleLayerCopy.pixelColor(15, 5).alpha() == 0);

    // 「可視部をコピー」を実行した場合: 表示レイヤー合成結果なので、左半分(5, 5)は緑、右半分(15, 5)は赤(255)
    window.onCopyVisible();
    QImage visibleCopy = QGuiApplication::clipboard()->image();
    assert(!visibleCopy.isNull());
    assert(visibleCopy.width() == 20 && visibleCopy.height() == 20);
    assert(visibleCopy.pixelColor(5, 5).green() == 255); // レイヤー2の緑
    assert(visibleCopy.pixelColor(15, 5).red() == 255);   // 下層レイヤー1の赤
    std::cout << "[PASS] Copy Visible composite merged layers verified!" << std::endl;

    // 5. 貼り付け（キャンバスがある場合：新規通常レイヤーに貼り付け）
    // クリップボードに 40x50 の緑色画像をセット
    QImage pasteSource(40, 50, QImage::Format_ARGB32);
    pasteSource.fill(QColor(0, 255, 0, 255));
    QGuiApplication::clipboard()->setImage(pasteSource);

    doc->clearSelection();
    size_t layerCountBefore = doc->layers().size();
    window.onPaste();
    size_t layerCountAfter = doc->layers().size();
    assert(layerCountAfter == layerCountBefore + 1);

    auto pastedLayer = doc->activeLayer();
    assert(pastedLayer != nullptr);
    assert(pastedLayer->type() == LayerType::Raster);

    // キャンバス中央付近に緑色が描画されていること
    int centerX = doc->width() / 2;
    int centerY = doc->height() / 2;
    int ctx = centerX / TileMemoryPool::TILE_WIDTH;
    int cty = centerY / TileMemoryPool::TILE_HEIGHT;
    const Tile* pTile = pastedLayer->tileManager().getTile(ctx, cty);
    assert(pTile != nullptr);
    uint32_t pPix = reinterpret_cast<const uint32_t*>(pTile->readBuffer())[(centerY % TileMemoryPool::TILE_HEIGHT) * TileMemoryPool::TILE_WIDTH + (centerX % TileMemoryPool::TILE_WIDTH)];
    assert(pPix == 0xFF00FF00); // 緑色
    std::cout << "[PASS] Paste to new raster layer when canvas exists verified!" << std::endl;

    // Undo で貼り付けレイヤーが削除され、Redo で復元されること
    assert(doc->undoStack()->canUndo());
    doc->undoStack()->undo();
    assert(doc->layers().size() == layerCountBefore);
    assert(doc->undoStack()->canRedo());
    doc->undoStack()->redo();
    assert(doc->layers().size() == layerCountAfter);
    std::cout << "[PASS] Paste Undo and Redo verified!" << std::endl;

    // 6. 貼り付け（キャンバスがない場合：貼り付け画像サイズのキャンバスを作成）
    window.clearDocumentForTest();
    assert(window.document() == nullptr);

    // 80x60 の青色画像をクリップボードにセット
    QImage canvasSource(80, 60, QImage::Format_ARGB32);
    canvasSource.fill(QColor(0, 0, 255, 255));
    QGuiApplication::clipboard()->setImage(canvasSource);

    window.onPaste();
    Document* newDoc = window.document();
    assert(newDoc != nullptr);
    assert(newDoc->width() == 80);
    assert(newDoc->height() == 60);

    // 新規キャンバスの画像が青色であること
    QColor sampled = newDoc->samplePixel(40, 30);
    assert(sampled.blue() > 200);
    std::cout << "[PASS] Paste when no canvas creates document with image size verified!" << std::endl;

    std::cout << "\n[ALL CLIPBOARD TESTS PASSED SUCCESSFULLY!]" << std::endl;
    return 0;
}
