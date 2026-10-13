#include <QApplication>
#include <iostream>
#include <cassert>
#include "Document.hpp"
#include "CanvasWidget.hpp"
#include "MainWindow.hpp"
#include "VectorLayer.hpp"
#include "TextLayer.hpp"
#include "UndoCommands.hpp"

using namespace Kaburagi;

void testRasterTransformNoSelection() {
    std::cout << "[TEST] Testing Raster Transform without selection (whole layer)..." << std::endl;
    Document doc(100, 100, 350.0, false, Qt::white);
    auto layer = doc.addRasterLayer();
    doc.setActiveLayerIndex(1);

    // (20, 20)から(40, 40)に赤い四角形を描く
    for (int y = 20; y < 40; ++y) {
        for (int x = 20; x < 40; ++x) {
            Tile* t = layer->tileManager().getTileForWrite(x / TileMemoryPool::TILE_WIDTH, y / TileMemoryPool::TILE_HEIGHT);
            if (t && t->writeBuffer()) {
                uint32_t* buf = reinterpret_cast<uint32_t*>(t->writeBuffer());
                int lx = x % TileMemoryPool::TILE_WIDTH;
                int ly = y % TileMemoryPool::TILE_HEIGHT;
                buf[ly * TileMemoryPool::TILE_WIDTH + lx] = 0xFFFF0000; // Red
            }
        }
    }
    layer->invalidateContentBounds();
    doc.composite();

    CanvasWidget canvas;
    canvas.setDocument(&doc);
    canvas.setCurrentTool(ToolType::Transform);

    // 変形が自動的に開始されていることを確認
    assert(canvas.isTransforming());

    canvas.applyTransform(); // そのまま確定テスト
    assert(!canvas.isTransforming());

    // Undo/Redoテスト
    assert(doc.undoStack()->canUndo());
    doc.undoStack()->undo();
    doc.undoStack()->redo();

    std::cout << "[PASS] Raster Transform without selection passed!" << std::endl;
}

void testRasterTransformWithSelection() {
    std::cout << "[TEST] Testing Raster Transform with selection (cut and paste transform)..." << std::endl;
    Document doc(100, 100, 350.0, false, Qt::white);
    auto layer = doc.addRasterLayer();
    doc.setActiveLayerIndex(1);

    // (10, 10)から(30, 30)に青い四角形を描く
    for (int y = 10; y < 30; ++y) {
        for (int x = 10; x < 30; ++x) {
            Tile* t = layer->tileManager().getTileForWrite(x / TileMemoryPool::TILE_WIDTH, y / TileMemoryPool::TILE_HEIGHT);
            if (t && t->writeBuffer()) {
                uint32_t* buf = reinterpret_cast<uint32_t*>(t->writeBuffer());
                int lx = x % TileMemoryPool::TILE_WIDTH;
                int ly = y % TileMemoryPool::TILE_HEIGHT;
                buf[ly * TileMemoryPool::TILE_WIDTH + lx] = 0xFF0000FF; // Blue
            }
        }
    }
    layer->invalidateContentBounds();

    // (10, 10)から(30, 30)を選択範囲にする
    QPainterPath selPath;
    selPath.addRect(10, 10, 20, 20);
    doc.setSelection(selPath);
    assert(doc.hasSelection());

    CanvasWidget canvas;
    canvas.setDocument(&doc);
    canvas.setCurrentTool(ToolType::Transform);
    assert(canvas.isTransforming());

    // 確定
    canvas.applyTransform();
    assert(!canvas.isTransforming());
    assert(doc.hasSelection()); // 選択範囲パスも保持・連動

    // Undoで元に戻る
    assert(doc.undoStack()->canUndo());
    doc.undoStack()->undo();

    // 元の位置に青色ピクセルが存在することを確認
    const Tile* t = layer->tileManager().getTile(15 / TileMemoryPool::TILE_WIDTH, 15 / TileMemoryPool::TILE_HEIGHT);
    assert(t && t->readBuffer());
    const uint32_t* buf = reinterpret_cast<const uint32_t*>(t->readBuffer());
    int lx = 15 % TileMemoryPool::TILE_WIDTH;
    int ly = 15 % TileMemoryPool::TILE_HEIGHT;
    assert(buf[ly * TileMemoryPool::TILE_WIDTH + lx] == 0xFF0000FF);

    std::cout << "[PASS] Raster Transform with selection passed!" << std::endl;
}

void testRasterTransformCancel() {
    std::cout << "[TEST] Testing Raster Transform Cancel..." << std::endl;
    Document doc(100, 100, 350.0, false, Qt::white);
    auto layer = doc.addRasterLayer();
    doc.setActiveLayerIndex(1);

    // (10, 10)に緑色ピクセルを描く
    Tile* t = layer->tileManager().getTileForWrite(10 / TileMemoryPool::TILE_WIDTH, 10 / TileMemoryPool::TILE_HEIGHT);
    uint32_t* buf = reinterpret_cast<uint32_t*>(t->writeBuffer());
    int lx = 10 % TileMemoryPool::TILE_WIDTH;
    int ly = 10 % TileMemoryPool::TILE_HEIGHT;
    buf[ly * TileMemoryPool::TILE_WIDTH + lx] = 0xFF00FF00;
    layer->invalidateContentBounds();

    CanvasWidget canvas;
    canvas.setDocument(&doc);
    canvas.setCurrentTool(ToolType::Transform);
    assert(canvas.isTransforming());

    // キャンセル実行
    canvas.cancelTransform();
    assert(!canvas.isTransforming());

    // 元の緑色ピクセルが復元されていること
    const Tile* tCheck = layer->tileManager().getTile(10 / TileMemoryPool::TILE_WIDTH, 10 / TileMemoryPool::TILE_HEIGHT);
    const uint32_t* readBuf = reinterpret_cast<const uint32_t*>(tCheck->readBuffer());
    assert(readBuf[ly * TileMemoryPool::TILE_WIDTH + lx] == 0xFF00FF00);

    std::cout << "[PASS] Raster Transform Cancel restored original pixels successfully!" << std::endl;
}

void testVectorTransform() {
    std::cout << "[TEST] Testing Vector Layer Transform..." << std::endl;
    Document doc(100, 100, 350.0, false, Qt::white);
    auto vecLayer = doc.addVectorLayer();
    doc.setActiveLayerIndex(1);

    VectorStroke stroke;
    stroke.baseWidth = 4.0f;
    stroke.color = 0xFF000000;
    stroke.points.push_back({10.0f, 10.0f, 1.0f, 1.0f, false, false});
    stroke.points.push_back({50.0f, 50.0f, 1.0f, 1.0f, false, false});
    vecLayer->addStroke(stroke);

    CanvasWidget canvas;
    canvas.setDocument(&doc);
    canvas.setCurrentTool(ToolType::Transform);
    assert(canvas.isTransforming());

    canvas.applyTransform();
    assert(!canvas.isTransforming());

    // Undo
    assert(doc.undoStack()->canUndo());
    doc.undoStack()->undo();
    assert(vecLayer->strokes().size() == 1);
    assert(vecLayer->strokes()[0].points[0].x == 10.0f);

    std::cout << "[PASS] Vector Layer Transform and Undo passed!" << std::endl;
}

void testTextLayerTransformSkip() {
    std::cout << "[TEST] Testing Text Layer Transform Skip (does nothing)..." << std::endl;
    Document doc(100, 100, 350.0, false, Qt::white);
    auto textLayer = doc.addTextLayer();
    doc.setActiveLayerIndex(1);

    CanvasWidget canvas;
    canvas.setDocument(&doc);
    canvas.setCurrentTool(ToolType::Transform);

    // テキストレイヤーでは変形セッションが開始されないこと
    assert(!canvas.isTransforming());

    std::cout << "[PASS] Text Layer Transform correctly skipped!" << std::endl;
}

void testWarpAndCageModesAndFlips() {
    std::cout << "[TEST] Testing Warp, Cage modes and Flip H/V..." << std::endl;
    Document doc(100, 100, 350.0, false, Qt::white);
    auto layer = doc.addRasterLayer();
    doc.setActiveLayerIndex(1);

    // 四角形ピクセルを描く
    for (int y = 20; y < 40; ++y) {
        for (int x = 20; x < 40; ++x) {
            Tile* t = layer->tileManager().getTileForWrite(x / TileMemoryPool::TILE_WIDTH, y / TileMemoryPool::TILE_HEIGHT);
            if (t && t->writeBuffer()) {
                uint32_t* buf = reinterpret_cast<uint32_t*>(t->writeBuffer());
                buf[(y % TileMemoryPool::TILE_HEIGHT) * TileMemoryPool::TILE_WIDTH + (x % TileMemoryPool::TILE_WIDTH)] = 0xFFFF0000;
            }
        }
    }
    layer->invalidateContentBounds();
    doc.composite();

    CanvasWidget canvas;
    canvas.setDocument(&doc);

    // 1. メッシュワープと反転
    canvas.setTransformMode(TransformMode::Warp);
    canvas.setWarpSubMode(WarpSubMode::Mesh);
    canvas.setCurrentTool(ToolType::Transform);
    assert(canvas.isTransforming());
    assert(canvas.transformMode() == TransformMode::Warp);
    assert(canvas.warpSubMode() == WarpSubMode::Mesh);

    // 左右反転 & 上下反転
    canvas.flipTransformHorizontal();
    canvas.flipTransformVertical();

    canvas.applyTransform();
    assert(!canvas.isTransforming());
    assert(doc.undoStack()->canUndo());

    // 2. ケージ変形 (AutoBox)
    canvas.setTransformMode(TransformMode::Cage);
    canvas.setCageSubMode(CageSubMode::AutoBox);
    canvas.setCurrentTool(ToolType::Transform);
    assert(canvas.isTransforming());
    assert(canvas.transformMode() == TransformMode::Cage);
    assert(canvas.cageSubMode() == CageSubMode::AutoBox);

    canvas.flipTransformHorizontal();
    canvas.applyTransform();
    assert(!canvas.isTransforming());
    assert(doc.undoStack()->canUndo());

    std::cout << "[PASS] Warp, Cage modes and Flip H/V passed!" << std::endl;
}

void testTransformShortcutCtrlT() {
    std::cout << "[TEST] Testing Transform Shortcut Ctrl+T in MainWindow..." << std::endl;
    MainWindow win;
    auto actions = win.findChildren<QAction*>();
    bool foundCtrlT = false;
    for (auto act : actions) {
        if (act->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_T)) {
            foundCtrlT = true;
            assert(act->text().contains("変形"));
            break;
        }
    }
    assert(foundCtrlT);
    std::cout << "[PASS] Transform Shortcut Ctrl+T verified!" << std::endl;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    testRasterTransformNoSelection();
    testRasterTransformWithSelection();
    testRasterTransformCancel();
    testVectorTransform();
    testTextLayerTransformSkip();
    testWarpAndCageModesAndFlips();
    testTransformShortcutCtrlT();

    std::cout << "[ALL PASS] All Transform Tool tests passed successfully!" << std::endl;
    return 0;
}
