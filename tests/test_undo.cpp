#include <cassert>
#include <iostream>
#include "Document.hpp"
#include "UndoCommands.hpp"
#include "VectorLayer.hpp"

using namespace Kaburagi;

int main(int argc, char** argv) {
    std::cout << "[TEST] Starting Undo/Redo unit tests..." << std::endl;

    Document doc(800, 600, 350.0);
    auto undoStack = doc.undoStack();
    assert(undoStack != nullptr);
    assert(undoStack->canUndo() == false);
    assert(undoStack->canRedo() == false);

    // 1. Layer Addition Test
    size_t initialLayerCount = doc.layers().size(); // Background + Layer 1 = 2
    assert(initialLayerCount == 2);

    auto rasterLayer = doc.addRasterLayer("テストレイヤー");
    assert(doc.layers().size() == 3);
    assert(undoStack->canUndo() == true);

    undoStack->undo();
    assert(doc.layers().size() == 2);
    assert(undoStack->canUndo() == false);
    assert(undoStack->canRedo() == true);

    undoStack->redo();
    assert(doc.layers().size() == 3);
    assert(undoStack->canUndo() == true);
    assert(undoStack->canRedo() == false);

    std::cout << "[PASS] Layer add/undo/redo passed!" << std::endl;

    // 2. Raster Stroke Drawing Test
    int activeIdx = doc.activeLayerIndex();
    auto activeL = doc.getLayer(activeIdx);
    assert(activeL != nullptr);

    TileManager preTiles = activeL->tileManager().clone();
    // Simulate painting a red pixel at (50, 50)
    activeL->tileManager().setPixel(50, 50, 0xFFFF0000);
    assert(activeL->tileManager().getPixel(50, 50) == 0xFFFF0000);

    TileManager postTiles = activeL->tileManager().clone();
    undoStack->push(new DrawStrokeCommand(&doc, activeIdx, std::move(preTiles), std::move(postTiles), QRect(45, 45, 10, 10)));

    assert(activeL->tileManager().getPixel(50, 50) == 0xFFFF0000);
    assert(undoStack->canUndo() == true);

    undoStack->undo();
    assert(activeL->tileManager().getPixel(50, 50) == 0x00000000);

    undoStack->redo();
    assert(activeL->tileManager().getPixel(50, 50) == 0xFFFF0000);

    std::cout << "[PASS] Raster stroke undo/redo passed!" << std::endl;

    // 3. Vector Stroke Test
    auto vecLayer = doc.addVectorLayer("ベクトルテスト");
    assert(doc.layers().size() == 4);

    VectorStroke stroke;
    stroke.baseWidth = 5.0f;
    stroke.color = 0xFF0000FF;
    stroke.points.push_back({ 10.0f, 10.0f, 1.0f, 1.0f, false });
    stroke.points.push_back({ 20.0f, 20.0f, 1.0f, 1.0f, false });

    vecLayer->addStroke(stroke);
    assert(vecLayer->strokes().size() == 1);

    undoStack->push(new AddVectorStrokeCommand(&doc, doc.activeLayerIndex(), stroke));
    assert(vecLayer->strokes().size() == 1);

    undoStack->undo();
    assert(vecLayer->strokes().size() == 0);

    undoStack->redo();
    assert(vecLayer->strokes().size() == 1);

    std::cout << "[PASS] Vector stroke undo/redo passed!" << std::endl;

    // 4. Thumbnail & Content Bounds Test
    auto emptyLayer = std::make_shared<Layer>("空レイヤー", 800, 600);
    assert(emptyLayer->contentBounds().isEmpty());
    QImage emptyThumb = emptyLayer->createThumbnail(36, 28);
    assert(!emptyThumb.isNull() && emptyThumb.width() == 36 && emptyThumb.height() == 28);

    // 描画のあるベクトルレイヤーの使用範囲
    QRect bounds = vecLayer->contentBounds();
    assert(!bounds.isEmpty());
    assert(bounds.left() >= 5 && bounds.right() <= 25);
    assert(bounds.top() >= 5 && bounds.bottom() <= 25);

    QImage thumb = vecLayer->createThumbnail(36, 28);
    assert(!thumb.isNull());
    assert(thumb.width() == 36);
    assert(thumb.height() == 28);
    std::cout << "[PASS] Layer content bounds & cropped thumbnail generation passed!" << std::endl;

    // 5. Selection Undo/Redo & Boolean Operation Test
    assert(doc.hasSelection() == false);
    QPainterPath p1;
    p1.addRect(10, 10, 50, 50);

    QPainterPath oldSel = doc.selection();
    undoStack->push(new SelectCommand(&doc, oldSel, p1));
    assert(doc.hasSelection() == true);
    assert(doc.selection().boundingRect() == QRectF(10, 10, 50, 50));

    // Union
    QPainterPath p2;
    p2.addRect(30, 30, 50, 50);
    QPainterPath unionSel = doc.selection().united(p2);
    undoStack->push(new SelectCommand(&doc, doc.selection(), unionSel));
    assert(doc.selection().boundingRect() == QRectF(10, 10, 70, 70));

    // Subtract
    QPainterPath p3;
    p3.addRect(10, 10, 20, 20);
    QPainterPath subSel = doc.selection().subtracted(p3);
    undoStack->push(new SelectCommand(&doc, doc.selection(), subSel));
    assert(doc.selection().contains(QPointF(15, 15)) == false);
    assert(doc.selection().contains(QPointF(40, 40)) == true);

    // Undo subtract
    undoStack->undo();
    assert(doc.selection().contains(QPointF(15, 15)) == true);

    // Undo union
    undoStack->undo();
    assert(doc.selection().boundingRect() == QRectF(10, 10, 50, 50));

    // Undo initial select
    undoStack->undo();
    assert(doc.hasSelection() == false);

    // Redo all
    undoStack->redo();
    assert(doc.hasSelection() == true);
    undoStack->redo();
    undoStack->redo();
    assert(doc.selection().contains(QPointF(15, 15)) == false);
    std::cout << "[PASS] Selection undo/redo & boolean operations passed!" << std::endl;

    // 6. Selection Masking (isPixelSelected) Test
    doc.clearSelection();
    assert(doc.hasSelection() == false);
    // When no selection exists, all pixels are considered selected
    assert(doc.isPixelSelected(0, 0) == true);
    assert(doc.isPixelSelected(100, 100) == true);

    // Set a rectangular selection [20, 20, 40, 40] -> (20..60, 20..60)
    QPainterPath rectSel;
    rectSel.addRect(20, 20, 40, 40);
    doc.setSelection(rectSel);
    assert(doc.hasSelection() == true);
    assert(doc.isPixelSelected(19, 20) == false); // outside left
    assert(doc.isPixelSelected(20, 19) == false); // outside top
    assert(doc.isPixelSelected(20, 20) == true);  // inside top-left
    assert(doc.isPixelSelected(30, 30) == true);  // inside center
    assert(doc.isPixelSelected(59, 59) == true);  // inside bottom-right
    assert(doc.isPixelSelected(60, 60) == false); // outside bottom-right
    assert(doc.isPixelSelected(100, 100) == false); // completely outside

    doc.clearSelection();
    assert(doc.hasSelection() == false);
    assert(doc.isPixelSelected(100, 100) == true);
    std::cout << "[PASS] Document isPixelSelected masking test passed!" << std::endl;

    // 7. Bucket Fill Undo/Redo Test
    int fillLayerIdx = doc.activeLayerIndex();
    auto fillLayer = doc.getLayer(fillLayerIdx);
    assert(fillLayer != nullptr);
    TileManager preFill = fillLayer->tileManager().clone();
    fillLayer->tileManager().setPixel(100, 100, 0xFF00FF00); // Green
    fillLayer->tileManager().setPixel(101, 100, 0xFF00FF00);
    TileManager postFill = fillLayer->tileManager().clone();
    undoStack->push(new FillBucketCommand(&doc, fillLayerIdx, std::move(preFill), std::move(postFill), QRect(100, 100, 2, 1)));
    assert(fillLayer->tileManager().getPixel(100, 100) == 0xFF00FF00);
    assert(undoStack->canUndo() == true);

    undoStack->undo();
    assert(fillLayer->tileManager().getPixel(100, 100) != 0xFF00FF00);

    undoStack->redo();
    assert(fillLayer->tileManager().getPixel(100, 100) == 0xFF00FF00);
    std::cout << "[PASS] FillBucketCommand undo/redo passed!" << std::endl;

    // 8. MoveLayerCommand Undo/Redo Test
    assert(doc.layers().size() >= 3);
    auto layerAt1 = doc.getLayer(1);
    auto layerAt2 = doc.getLayer(2);
    doc.moveLayer(1, 2);
    assert(doc.getLayer(2) == layerAt1);
    assert(doc.getLayer(1) == layerAt2);
    assert(undoStack->canUndo() == true);

    undoStack->undo();
    assert(doc.getLayer(1) == layerAt1);
    assert(doc.getLayer(2) == layerAt2);

    undoStack->redo();
    assert(doc.getLayer(2) == layerAt1);
    assert(doc.getLayer(1) == layerAt2);

    // Verify background guard: cannot move background (index 0)
    auto bg = doc.getLayer(0);
    doc.moveLayer(0, 1);
    assert(doc.getLayer(0) == bg);

    // Verify background guard: cannot move another layer below background
    doc.moveLayer(2, 0);
    assert(doc.getLayer(0) == bg);
    std::cout << "[PASS] MoveLayerCommand and Background guard passed!" << std::endl;

    std::cout << "[ALL TESTS PASSED SUCCESSFULLY!]" << std::endl;
    return 0;
}
