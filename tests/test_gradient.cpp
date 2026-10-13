#include <QApplication>
#include <cassert>
#include <iostream>
#include "Document.hpp"
#include "CanvasWidget.hpp"
#include "GradientSettingsWidget.hpp"
#include "ToolBoxWidget.hpp"

using namespace Kaburagi;

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    std::cout << "[TEST] Starting Gradient tests..." << std::endl;

    // 1. GradientSettingsWidget properties and getters/setters
    {
        GradientSettingsWidget widget;
        assert(widget.shape() == GradientShape::Linear);
        assert(widget.colorMode() == GradientColorMode::FgToBg);
        assert(widget.repeatMode() == GradientRepeatMode::None);
        assert(widget.opacity() == 1.0f);
        assert(widget.isReverse() == false);
        assert(widget.isDither() == false);

        GradientShape receivedShape = GradientShape::Linear;
        GradientColorMode receivedColorMode = GradientColorMode::FgToBg;
        GradientRepeatMode receivedRepeatMode = GradientRepeatMode::None;
        float receivedOpacity = 0.0f;
        bool receivedReverse = false;
        bool receivedDither = false;

        QObject::connect(&widget, &GradientSettingsWidget::shapeChanged, [&](GradientShape s) { receivedShape = s; });
        QObject::connect(&widget, &GradientSettingsWidget::colorModeChanged, [&](GradientColorMode m) { receivedColorMode = m; });
        QObject::connect(&widget, &GradientSettingsWidget::repeatModeChanged, [&](GradientRepeatMode r) { receivedRepeatMode = r; });
        QObject::connect(&widget, &GradientSettingsWidget::opacityChanged, [&](float op) { receivedOpacity = op; });
        QObject::connect(&widget, &GradientSettingsWidget::reverseChanged, [&](bool rev) { receivedReverse = rev; });
        QObject::connect(&widget, &GradientSettingsWidget::ditherChanged, [&](bool d) { receivedDither = d; });

        widget.setShape(GradientShape::Radial);
        assert(widget.shape() == GradientShape::Radial);
        assert(receivedShape == GradientShape::Radial);

        widget.setColorMode(GradientColorMode::FgToTransparent);
        assert(widget.colorMode() == GradientColorMode::FgToTransparent);
        assert(receivedColorMode == GradientColorMode::FgToTransparent);

        widget.setRepeatMode(GradientRepeatMode::Sawtooth);
        assert(widget.repeatMode() == GradientRepeatMode::Sawtooth);
        assert(receivedRepeatMode == GradientRepeatMode::Sawtooth);

        widget.setOpacity(0.75f);
        assert(std::abs(widget.opacity() - 0.75f) < 0.01f);
        assert(std::abs(receivedOpacity - 0.75f) < 0.01f);

        widget.setReverse(true);
        assert(widget.isReverse() == true);
        assert(receivedReverse == true);

        widget.setDither(true);
        assert(widget.isDither() == true);
        assert(receivedDither == true);

        // setConfig test
        GradientConfig cfg;
        cfg.shape = GradientShape::BiLinear;
        cfg.colorMode = GradientColorMode::FgToBg;
        cfg.repeatMode = GradientRepeatMode::Triangular;
        cfg.opacity = 0.5f;
        cfg.reverse = false;
        cfg.dither = false;
        widget.setConfig(cfg);
        assert(widget.shape() == GradientShape::BiLinear);
        assert(widget.repeatMode() == GradientRepeatMode::Triangular);
        assert(std::abs(widget.opacity() - 0.5f) < 0.01f);

        std::cout << "[PASS] GradientSettingsWidget properties and getters/setters passed!" << std::endl;
    }

    // 2. Linear Gradient Rendering & Undo/Redo
    {
        Document doc(100, 100);
        CanvasWidget canvas;
        canvas.setDocument(&doc);
        canvas.setCurrentTool(ToolType::Gradient);

        // レイヤー初期状態 (白で塗りつぶし)
        auto activeL = doc.activeLayer();
        assert(activeL != nullptr);
        Tile* t = activeL->tileManager().getTileForWrite(0, 0);
        uint32_t* p = reinterpret_cast<uint32_t*>(t->writeBuffer());
        for (int i = 0; i < TileMemoryPool::TILE_WIDTH * TileMemoryPool::TILE_HEIGHT; ++i) {
            p[i] = 0xFFFFFFFF; // 白
        }

        // 赤 (Fg) -> 青 (Bg)
        canvas.setCurrentColor(QColor(255, 0, 0, 255));
        canvas.setSecondaryColor(QColor(0, 0, 255, 255));
        canvas.setGradientShape(GradientShape::Linear);
        canvas.setGradientColorMode(GradientColorMode::FgToBg);
        canvas.setGradientRepeatMode(GradientRepeatMode::None);
        canvas.setGradientOpacity(1.0f);
        canvas.setGradientReverse(false);
        canvas.setGradientDither(false);

        // 始点 (0, 50) -> 終点 (100, 50)
        canvas.renderGradient(QPointF(0, 50), QPointF(100, 50));

        uint32_t colStart = activeL->tileManager().getPixel(0, 50);
        uint32_t colMid = activeL->tileManager().getPixel(50, 50);
        uint32_t colEnd = activeL->tileManager().getPixel(99, 50);

        int rStart = (colStart >> 16) & 0xFF;
        int bStart = colStart & 0xFF;
        int rMid = (colMid >> 16) & 0xFF;
        int bMid = colMid & 0xFF;
        int rEnd = (colEnd >> 16) & 0xFF;
        int bEnd = colEnd & 0xFF;

        assert(rStart >= 250 && bStart <= 5);
        assert(rMid >= 100 && rMid <= 150 && bMid >= 100 && bMid <= 150);
        assert(bEnd >= 240 && rEnd <= 10);

        // Undo
        assert(doc.undoStack()->canUndo());
        doc.undoStack()->undo();
        uint32_t colUndone = activeL->tileManager().getPixel(50, 50);
        assert(colUndone == 0xFFFFFFFF);

        // Redo
        assert(doc.undoStack()->canRedo());
        doc.undoStack()->redo();
        uint32_t colRedone = activeL->tileManager().getPixel(50, 50);
        assert(colRedone == colMid);

        std::cout << "[PASS] Linear gradient rendering and Undo/Redo passed!" << std::endl;
    }

    // 3. Radial Gradient & Reverse
    {
        Document doc(100, 100);
        CanvasWidget canvas;
        canvas.setDocument(&doc);
        canvas.setCurrentTool(ToolType::Gradient);

        auto activeL = doc.activeLayer();
        Tile* t = activeL->tileManager().getTileForWrite(0, 0);
        uint32_t* p = reinterpret_cast<uint32_t*>(t->writeBuffer());
        for (int i = 0; i < TileMemoryPool::TILE_WIDTH * TileMemoryPool::TILE_HEIGHT; ++i) {
            p[i] = 0x00000000; // 完全透明
        }

        // 赤 (Fg) -> 青 (Bg), 中心(50, 50), 半径50(終点 (100, 50))
        canvas.setCurrentColor(QColor(255, 0, 0, 255));
        canvas.setSecondaryColor(QColor(0, 0, 255, 255));
        canvas.setGradientShape(GradientShape::Radial);
        canvas.setGradientColorMode(GradientColorMode::FgToBg);
        canvas.setGradientReverse(false);

        canvas.renderGradient(QPointF(50, 50), QPointF(100, 50));

        uint32_t colCenter = activeL->tileManager().getPixel(50, 50);
        uint32_t colEdge = activeL->tileManager().getPixel(99, 50);

        int rCenter = (colCenter >> 16) & 0xFF;
        int bCenter = colCenter & 0xFF;
        int rEdge = (colEdge >> 16) & 0xFF;
        int bEdge = colEdge & 0xFF;

        assert(rCenter >= 250 && bCenter <= 5);
        assert(bEdge >= 240 && rEdge <= 10);

        // Reverse test
        canvas.setGradientReverse(true);
        canvas.renderGradient(QPointF(50, 50), QPointF(100, 50));
        colCenter = activeL->tileManager().getPixel(50, 50);
        colEdge = activeL->tileManager().getPixel(99, 50);

        rCenter = (colCenter >> 16) & 0xFF;
        bCenter = colCenter & 0xFF;
        rEdge = (colEdge >> 16) & 0xFF;
        bEdge = colEdge & 0xFF;

        assert(bCenter >= 250 && rCenter <= 5);
        assert(rEdge >= 240 && bEdge <= 10);

        std::cout << "[PASS] Radial gradient rendering and Reverse mode passed!" << std::endl;
    }

    // 4. FgToTransparent Mode
    {
        Document doc(100, 100);
        CanvasWidget canvas;
        canvas.setDocument(&doc);
        canvas.setCurrentTool(ToolType::Gradient);

        auto activeL = doc.activeLayer();
        Tile* t = activeL->tileManager().getTileForWrite(0, 0);
        uint32_t* p = reinterpret_cast<uint32_t*>(t->writeBuffer());
        for (int i = 0; i < TileMemoryPool::TILE_WIDTH * TileMemoryPool::TILE_HEIGHT; ++i) {
            p[i] = 0x00000000; // 完全透明
        }

        canvas.setCurrentColor(QColor(0, 255, 0, 255)); // 緑
        canvas.setGradientShape(GradientShape::Linear);
        canvas.setGradientColorMode(GradientColorMode::FgToTransparent);
        canvas.setGradientReverse(false);

        canvas.renderGradient(QPointF(0, 50), QPointF(100, 50));

        uint32_t p0 = activeL->tileManager().getPixel(0, 50);
        uint32_t pMid = activeL->tileManager().getPixel(50, 50);
        uint32_t pEnd = activeL->tileManager().getPixel(100, 50);

        int a0 = (p0 >> 24) & 0xFF;
        int aMid = (pMid >> 24) & 0xFF;
        int aEnd = (pEnd >> 24) & 0xFF;

        assert(a0 >= 250);
        assert(aMid >= 100 && aMid <= 150);
        assert(aEnd <= 2);

        std::cout << "[PASS] FgToTransparent color mode passed!" << std::endl;
    }

    // 5. Selection Masking restriction
    {
        Document doc(100, 100);
        CanvasWidget canvas;
        canvas.setDocument(&doc);
        canvas.setCurrentTool(ToolType::Gradient);

        auto activeL = doc.activeLayer();
        Tile* t = activeL->tileManager().getTileForWrite(0, 0);
        uint32_t* p = reinterpret_cast<uint32_t*>(t->writeBuffer());
        for (int i = 0; i < TileMemoryPool::TILE_WIDTH * TileMemoryPool::TILE_HEIGHT; ++i) {
            p[i] = 0xFFFFFFFF; // 白
        }

        // 矩形選択範囲 (20, 20, 40, 40)
        QPainterPath sel;
        sel.addRect(20, 20, 40, 40);
        doc.setSelection(sel);

        canvas.setCurrentColor(QColor(255, 0, 0, 255));
        canvas.setSecondaryColor(QColor(0, 0, 255, 255));
        canvas.setGradientShape(GradientShape::Linear);
        canvas.setGradientColorMode(GradientColorMode::FgToBg);
        canvas.setGradientReverse(false);

        canvas.renderGradient(QPointF(0, 50), QPointF(100, 50));

        // 選択範囲外 (10, 50) は変化なし (白)
        uint32_t outside = activeL->tileManager().getPixel(10, 50);
        assert(outside == 0xFFFFFFFF);

        // 選択範囲内 (30, 50) はグラデーション色が適用されている
        uint32_t inside = activeL->tileManager().getPixel(30, 50);
        assert(inside != 0xFFFFFFFF);

        std::cout << "[PASS] Selection masking restriction passed!" << std::endl;
    }

    // 6. Delete Selected Pixels & Undo/Redo & Guards (GIMP Clear)
    {
        Document doc(100, 100);
        CanvasWidget canvas;
        canvas.setDocument(&doc);

        auto activeL = doc.activeLayer();
        assert(activeL != nullptr);
        Tile* t = activeL->tileManager().getTileForWrite(0, 0);
        uint32_t* p = reinterpret_cast<uint32_t*>(t->writeBuffer());
        for (int i = 0; i < TileMemoryPool::TILE_WIDTH * TileMemoryPool::TILE_HEIGHT; ++i) {
            p[i] = 0xFFFF0000; // 赤
        }

        // 6-1. 選択範囲がないときは実行されない (ガード)
        assert(!doc.hasSelection());
        canvas.deleteSelectedPixels();
        assert(activeL->tileManager().getPixel(30, 30) == 0xFFFF0000);

        // 6-2. 選択範囲 (20, 20, 40, 40) を設定して削除実行
        QPainterPath sel;
        sel.addRect(20, 20, 40, 40);
        doc.setSelection(sel);
        assert(doc.hasSelection());

        canvas.deleteSelectedPixels();

        // 選択範囲外 (10, 10) は赤のまま
        assert(activeL->tileManager().getPixel(10, 10) == 0xFFFF0000);
        // 選択範囲内 (30, 30) は完全に透明 (0x00000000)
        assert(activeL->tileManager().getPixel(30, 30) == 0x00000000);

        // 6-3. Undo / Redo
        assert(doc.undoStack()->canUndo());
        doc.undoStack()->undo();
        assert(activeL->tileManager().getPixel(30, 30) == 0xFFFF0000);

        assert(doc.undoStack()->canRedo());
        doc.undoStack()->redo();
        assert(activeL->tileManager().getPixel(30, 30) == 0x00000000);

        // 6-4. 透明度保護 (Alpha Lock) 下での消去 (GIMP準拠: アルファ保持＆背景色で消去)
        activeL->setAlphaLocked(true);
        canvas.setSecondaryColor(QColor(0, 0, 255)); // 背景色: 青
        // 元に戻して赤に戻す
        doc.undoStack()->undo();
        assert(activeL->tileManager().getPixel(30, 30) == 0xFFFF0000);

        canvas.deleteSelectedPixels();
        // アルファは 255 のまま、RGB が背景色 (青) になる
        uint32_t clearedWithAlphaLock = activeL->tileManager().getPixel(30, 30);
        assert(clearedWithAlphaLock == 0xFF0000FF);

        std::cout << "[PASS] Delete Selected Pixels & Undo/Redo & AlphaLock passed!" << std::endl;
    }

    std::cout << "[ALL PASS] All Gradient & Delete tests completed successfully!" << std::endl;
    return 0;
}
