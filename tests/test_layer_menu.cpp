#include <QApplication>
#include <cassert>
#include <iostream>
#include "MainWindow.hpp"
#include "Document.hpp"
#include "Layer.hpp"
#include "VectorLayer.hpp"
#include "TextLayer.hpp"

using namespace Kaburagi;

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    std::cout << "[TEST] Starting Layer Menu actions tests..." << std::endl;

    MainWindow win;
    auto doc = new Document(400, 300, 350.0);
    win.setDocumentForTest(doc);

    // Initial state: doc has 2 layers: [0] "背景", [1] "レイヤー 1" (active: 1)
    assert(doc->layers().size() == 2);
    assert(doc->activeLayerIndex() == 1);

    // 1. 新規通常レイヤー (Ctrl+Shift+N)
    std::cout << "[TEST] Testing New Raster Layer (Ctrl+Shift+N)..." << std::endl;
    auto newRasterAct = win.newRasterLayerAction();
    assert(newRasterAct != nullptr);
    assert(newRasterAct->shortcut() == QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    assert(newRasterAct->isEnabled());

    win.onNewRasterLayer();
    assert(doc->layers().size() == 3);
    assert(doc->activeLayerIndex() == 2);
    assert(doc->activeLayer()->type() == LayerType::Raster);

    // Undo new raster layer
    doc->undoStack()->undo();
    assert(doc->layers().size() == 2);
    assert(doc->activeLayerIndex() == 1);
    doc->undoStack()->redo();
    assert(doc->layers().size() == 3);
    std::cout << "[PASS] New Raster Layer works with Undo/Redo!" << std::endl;

    // 2. 新規ベクトルレイヤー
    std::cout << "[TEST] Testing New Vector Layer..." << std::endl;
    win.onNewVectorLayer();
    assert(doc->layers().size() == 4);
    assert(doc->activeLayerIndex() == 3);
    assert(doc->activeLayer()->type() == LayerType::Vector);
    doc->undoStack()->undo();
    assert(doc->layers().size() == 3);
    std::cout << "[PASS] New Vector Layer works with Undo!" << std::endl;

    // 3. 新規レイヤーグループ
    std::cout << "[TEST] Testing New Group Layer..." << std::endl;
    win.onNewGroupLayer();
    assert(doc->layers().size() == 4);
    assert(doc->activeLayerIndex() == 3);
    assert(doc->activeLayer()->type() == LayerType::Group);
    doc->undoStack()->undo();
    assert(doc->layers().size() == 3);
    std::cout << "[PASS] New Group Layer works with Undo!" << std::endl;

    // 3.5. 可視部をレイヤーに
    std::cout << "[TEST] Testing New Layer From Visible..." << std::endl;
    auto newFromVisAct = win.newLayerFromVisibleAction();
    assert(newFromVisAct != nullptr);
    assert(newFromVisAct->isEnabled());

    win.onNewLayerFromVisible();
    assert(doc->layers().size() == 4);
    assert(doc->activeLayerIndex() == 3);
    assert(doc->activeLayer()->type() == LayerType::Raster);
    assert(doc->activeLayer()->name() == "可視部");

    // Undo new layer from visible
    doc->undoStack()->undo();
    assert(doc->layers().size() == 3);
    doc->undoStack()->redo();
    assert(doc->layers().size() == 4);
    doc->undoStack()->undo();
    assert(doc->layers().size() == 3);
    std::cout << "[PASS] New Layer From Visible works with Undo/Redo!" << std::endl;

    // 4. レイヤーを複製
    std::cout << "[TEST] Testing Duplicate Layer..." << std::endl;
    // Currently active: index 2 (Raster)
    // Put a pixel in active layer
    Tile* t = doc->activeLayer()->tileManager().getTileForWrite(0, 0);
    assert(t != nullptr);
    uint32_t* p = reinterpret_cast<uint32_t*>(t->writeBuffer());
    p[0] = 0xFFFF0000; // Red
    doc->activeLayer()->invalidateContentBounds();

    win.onDuplicateLayer();
    assert(doc->layers().size() == 4);
    assert(doc->activeLayerIndex() == 3);
    assert(doc->activeLayer()->name().contains("コピー"));
    // Verify pixels duplicated
    const Tile* tDup = doc->activeLayer()->tileManager().getTile(0, 0);
    assert(tDup != nullptr);
    const uint32_t* pDup = reinterpret_cast<const uint32_t*>(tDup->readBuffer());
    assert(pDup[0] == 0xFFFF0000);

    // Undo duplication
    doc->undoStack()->undo();
    assert(doc->layers().size() == 3);
    doc->undoStack()->redo();
    assert(doc->layers().size() == 4);
    std::cout << "[PASS] Duplicate Layer works with content deep copy and Undo/Redo!" << std::endl;

    // 5. 下のレイヤーと結合
    std::cout << "[TEST] Testing Merge Down..." << std::endl;
    // Lower layer (index 2): paint green at (1, 0)
    doc->setActiveLayerIndex(2);
    Tile* tLower = doc->activeLayer()->tileManager().getTileForWrite(0, 0);
    reinterpret_cast<uint32_t*>(tLower->writeBuffer())[1] = 0xFF00FF00; // Green
    doc->activeLayer()->invalidateContentBounds();

    // Upper layer (index 3): paint blue at (2, 0)
    doc->setActiveLayerIndex(3);
    Tile* tUpper = doc->activeLayer()->tileManager().getTileForWrite(0, 0);
    reinterpret_cast<uint32_t*>(tUpper->writeBuffer())[2] = 0xFF0000FF; // Blue
    doc->activeLayer()->invalidateContentBounds();

    win.updateLayerMenuActions();
    assert(win.mergeDownAction()->isEnabled());

    win.onMergeDown();
    assert(doc->layers().size() == 3);
    assert(doc->activeLayerIndex() == 2);
    const Tile* tMerged = doc->activeLayer()->tileManager().getTile(0, 0);
    assert(tMerged != nullptr);
    const uint32_t* pMerged = reinterpret_cast<const uint32_t*>(tMerged->readBuffer());
    assert(pMerged[1] == 0xFF00FF00); // Green from lower
    assert(pMerged[2] == 0xFF0000FF); // Blue from upper

    // Undo merge down
    doc->undoStack()->undo();
    assert(doc->layers().size() == 4);
    assert(doc->activeLayerIndex() == 3);
    // Redo merge down
    doc->undoStack()->redo();
    assert(doc->layers().size() == 3);
    assert(doc->activeLayerIndex() == 2);
    std::cout << "[PASS] Merge Down works with rendering and Undo/Redo!" << std::endl;

    // Check merge down disabled for background layer (index 0)
    doc->setActiveLayerIndex(0);
    win.updateLayerMenuActions();
    assert(!win.mergeDownAction()->isEnabled());
    std::cout << "[PASS] Merge Down disabled for layer 0!" << std::endl;

    // 6. レイヤーを削除
    std::cout << "[TEST] Testing Delete Layer..." << std::endl;
    doc->setActiveLayerIndex(2);
    win.updateLayerMenuActions();
    assert(win.deleteLayerAction()->isEnabled());

    win.onDeleteLayer();
    assert(doc->layers().size() == 2);
    doc->undoStack()->undo();
    assert(doc->layers().size() == 3);
    doc->undoStack()->redo();
    assert(doc->layers().size() == 2);

    // Background layer cannot be deleted
    doc->setActiveLayerIndex(0);
    win.updateLayerMenuActions();
    assert(!win.deleteLayerAction()->isEnabled());
    std::cout << "[PASS] Delete Layer works and protects background layer!" << std::endl;

    // 7. 不透明部分を選択範囲にする / 加える
    std::cout << "[TEST] Testing Select Opaque & Add Opaque to Selection..." << std::endl;
    doc->setActiveLayerIndex(1);
    // Draw opaque 10x10 rect at (20, 20)
    for (int y = 20; y < 30; ++y) {
        for (int x = 20; x < 30; ++x) {
            int tx = x / TileMemoryPool::TILE_WIDTH;
            int ty = y / TileMemoryPool::TILE_HEIGHT;
            Tile* tile = doc->activeLayer()->tileManager().getTileForWrite(tx, ty);
            int lx = x % TileMemoryPool::TILE_WIDTH;
            int ly = y % TileMemoryPool::TILE_HEIGHT;
            reinterpret_cast<uint32_t*>(tile->writeBuffer())[ly * TileMemoryPool::TILE_WIDTH + lx] = 0xFF123456;
        }
    }
    doc->activeLayer()->invalidateContentBounds();

    assert(!doc->hasSelection());
    win.onSelectOpaque();
    assert(doc->hasSelection());
    assert(doc->isPixelSelected(25, 25));
    assert(!doc->isPixelSelected(5, 5));

    // Draw another 10x10 rect at (50, 50) on another layer and add opaque
    auto extraLayer = doc->addRasterLayer("一時レイヤー");
    for (int y = 50; y < 60; ++y) {
        for (int x = 50; x < 60; ++x) {
            int tx = x / TileMemoryPool::TILE_WIDTH;
            int ty = y / TileMemoryPool::TILE_HEIGHT;
            Tile* tile = extraLayer->tileManager().getTileForWrite(tx, ty);
            int lx = x % TileMemoryPool::TILE_WIDTH;
            int ly = y % TileMemoryPool::TILE_HEIGHT;
            reinterpret_cast<uint32_t*>(tile->writeBuffer())[ly * TileMemoryPool::TILE_WIDTH + lx] = 0xFF789ABC;
        }
    }
    extraLayer->invalidateContentBounds();

    win.onAddOpaqueToSelection();
    assert(doc->isPixelSelected(25, 25)); // still selected
    assert(doc->isPixelSelected(55, 55)); // newly added

    // Undo add opaque
    doc->undoStack()->undo();
    assert(doc->isPixelSelected(25, 25));
    assert(!doc->isPixelSelected(55, 55));
    std::cout << "[PASS] Select Opaque & Add to Selection work with Undo/Redo!" << std::endl;

    doc->clearSelection();

    // 8. レイヤーをラスタライズ
    std::cout << "[TEST] Testing Rasterize Layer..." << std::endl;
    auto vecLayer = doc->addVectorLayer("ベクター線テスト");
    VectorStroke stroke;
    stroke.points.push_back({10, 10, 1.0f, 1.0f, false, false});
    stroke.points.push_back({40, 40, 1.0f, 1.0f, false, false});
    stroke.baseWidth = 5.0f;
    stroke.color = 0xFF00FF00;
    vecLayer->addStroke(stroke);

    win.updateLayerMenuActions();
    assert(win.rasterizeLayerAction()->isEnabled());

    win.onRasterizeLayer();
    assert(doc->activeLayer()->type() == LayerType::Raster);
    win.updateLayerMenuActions();
    assert(!win.rasterizeLayerAction()->isEnabled()); // Now raster, so disabled!

    // Undo rasterize: should become Vector layer again
    doc->undoStack()->undo();
    assert(doc->activeLayer()->type() == LayerType::Vector);
    win.updateLayerMenuActions();
    assert(win.rasterizeLayerAction()->isEnabled());

    doc->undoStack()->redo();
    assert(doc->activeLayer()->type() == LayerType::Raster);
    std::cout << "[PASS] Rasterize Layer works and disables when layer is already raster!" << std::endl;

    // 9. レイヤーを描画色で塗りつぶす
    std::cout << "[TEST] Testing Fill Layer..." << std::endl;
    assert(win.fillLayerAction() != nullptr);
    assert(win.fillLayerAction()->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_F));
    std::cout << "[PASS] Fill Layer shortcut is Ctrl+F!" << std::endl;

    // Without selection: fill whole layer with green
    win.canvasWidget()->setCurrentColor(QColor(0, 255, 0));
    win.onFillLayer();

    const Tile* tFill = doc->activeLayer()->tileManager().getTile(0, 0);
    assert(tFill != nullptr);
    const uint32_t* pFill = reinterpret_cast<const uint32_t*>(tFill->readBuffer());
    assert(pFill[0] == 0xFF00FF00); // Green
    assert(pFill[100] == 0xFF00FF00); // Green

    // Undo fill
    doc->undoStack()->undo();
    // With selection: select (10, 10, 20, 20) and fill with blue
    QPainterPath selRect;
    selRect.addRect(10, 10, 20, 20);
    doc->setSelection(selRect);

    win.canvasWidget()->setCurrentColor(QColor(0, 0, 255));
    win.onFillLayer();

    const Tile* tSelFill = doc->activeLayer()->tileManager().getTile(0, 0);
    assert(tSelFill != nullptr);
    const uint32_t* pSelFill = reinterpret_cast<const uint32_t*>(tSelFill->readBuffer());
    // (15, 15) is inside selection -> Blue (0xFF0000FF)
    assert(pSelFill[15 * TileMemoryPool::TILE_WIDTH + 15] == 0xFF0000FF);
    // (5, 5) is outside selection -> not Blue
    assert(pSelFill[5 * TileMemoryPool::TILE_WIDTH + 5] != 0xFF0000FF);

    // Test alpha lock
    doc->activeLayer()->setAlphaLocked(true);
    win.canvasWidget()->setCurrentColor(QColor(255, 0, 0));
    win.onFillLayer();
    // Inside selection: was Blue (alpha 255), so now Red (0xFFFF0000)
    assert(pSelFill[15 * TileMemoryPool::TILE_WIDTH + 15] == 0xFFFF0000);
    // Outside was transparent (alpha 0) -> still transparent
    assert(pSelFill[5 * TileMemoryPool::TILE_WIDTH + 5] == 0x00000000);

    // Undo fill with alpha lock
    doc->undoStack()->undo();
    assert(pSelFill[15 * TileMemoryPool::TILE_WIDTH + 15] == 0xFF0000FF);

    std::cout << "[PASS] Fill Layer works with/without selection and with alpha lock!" << std::endl;

    std::cout << "[ALL PASS] All Layer Menu action tests passed successfully!" << std::endl;
    return 0;
}
