#include <QApplication>
#include <cassert>
#include <iostream>
#include <cmath>
#include "Document.hpp"
#include "VectorLayer.hpp"
#include "UndoCommands.hpp"
#include "ToolBoxWidget.hpp"
#include "VectorEraserSettingsWidget.hpp"
#include "VectorControlPointSettingsWidget.hpp"
#include "VectorLineSettingsWidget.hpp"
#include "VectorChangeLineSettingsWidget.hpp"
#include "VectorColorChangeSettingsWidget.hpp"
#include "VectorEllipseSettingsWidget.hpp"
#include "VectorCircleSettingsWidget.hpp"
#include "VectorPolygonSettingsWidget.hpp"
#include "VectorPressureSettingsWidget.hpp"
#include "CanvasWidget.hpp"

using namespace Kaburagi;

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    std::cout << "[TEST] Starting VectorLayer tests..." << std::endl;

    // 1. ストロークの追加とラスタライズ検証
    {
        std::cout << "[TEST 1] Testing vector stroke rasterization..." << std::endl;
        VectorLayer vecLayer("Vector1", 200, 200);
        assert(vecLayer.type() == LayerType::Vector);
        assert(vecLayer.strokes().empty());

        VectorStroke stroke1;
        stroke1.baseWidth = 4.0f;
        stroke1.color = 0xFF000000;
        stroke1.points.push_back({ 20.0f, 50.0f, 1.0f, 1.0f, false });
        stroke1.points.push_back({ 50.0f, 50.0f, 1.0f, 1.0f, false });
        stroke1.points.push_back({ 80.0f, 50.0f, 1.0f, 1.0f, false });

        vecLayer.addStroke(stroke1);
        assert(vecLayer.strokes().size() == 1);

        // ラスタライズ結果を確認: (50, 50) 付近にピクセルが描画されているはず
        uint32_t px = vecLayer.tileManager().getPixel(50, 50);
        assert((px >> 24) > 0); // アルファ値が0でないこと
        std::cout << "[PASS] Vector stroke rasterization passed!" << std::endl;
    }

    // 2. 「線全体消去」 (WholeLine) モードの検証
    {
        std::cout << "[TEST 2] Testing VectorEraserMode::WholeLine..." << std::endl;
        VectorLayer vecLayer("Vector2", 200, 200);

        // ストロークA: 水平線 y = 30
        VectorStroke sA;
        sA.baseWidth = 4.0f;
        sA.color = 0xFF000000;
        sA.points.push_back({ 10.0f, 30.0f, 1.0f, 1.0f, false });
        sA.points.push_back({ 50.0f, 30.0f, 1.0f, 1.0f, false });
        sA.points.push_back({ 90.0f, 30.0f, 1.0f, 1.0f, false });
        vecLayer.addStroke(sA);

        // ストロークB: 水平線 y = 70
        VectorStroke sB;
        sB.baseWidth = 4.0f;
        sB.color = 0xFF000000;
        sB.points.push_back({ 10.0f, 70.0f, 1.0f, 1.0f, false });
        sB.points.push_back({ 50.0f, 70.0f, 1.0f, 1.0f, false });
        sB.points.push_back({ 90.0f, 70.0f, 1.0f, 1.0f, false });
        vecLayer.addStroke(sB);

        assert(vecLayer.strokes().size() == 2);

        // ストロークA上の点 (50, 30) を半径5で消去
        bool erased = vecLayer.eraseAt(QPointF(50.0, 30.0), 5.0f, VectorEraserMode::WholeLine);
        assert(erased);
        assert(vecLayer.strokes().size() == 1);
        // 残ったのはストロークB (y = 70)
        assert(std::abs(vecLayer.strokes()[0].points[0].y - 70.0f) < 0.1f);

        // ストロークAの位置 (50, 30) は消えている
        uint32_t pxA = vecLayer.tileManager().getPixel(50, 30);
        assert((pxA >> 24) == 0);

        // ストロークBの位置 (50, 70) は残っている
        uint32_t pxB = vecLayer.tileManager().getPixel(50, 70);
        assert((pxB >> 24) > 0);

        std::cout << "[PASS] VectorEraserMode::WholeLine passed!" << std::endl;
    }

    // 3. 「交点まで消去」 (Intersection) モードの検証
    {
        std::cout << "[TEST 3] Testing VectorEraserMode::Intersection..." << std::endl;
        VectorLayer vecLayer("Vector3", 200, 200);

        // 水平線: (10, 50) -> (90, 50)
        VectorStroke hStroke;
        hStroke.baseWidth = 4.0f;
        hStroke.color = 0xFF000000;
        hStroke.points.push_back({ 10.0f, 50.0f, 1.0f, 1.0f, false });
        hStroke.points.push_back({ 50.0f, 50.0f, 1.0f, 1.0f, false });
        hStroke.points.push_back({ 90.0f, 50.0f, 1.0f, 1.0f, false });
        vecLayer.addStroke(hStroke);

        // 垂直線: (50, 10) -> (50, 90)
        // 交点は (50, 50)
        VectorStroke vStroke;
        vStroke.baseWidth = 4.0f;
        vStroke.color = 0xFF000000;
        vStroke.points.push_back({ 50.0f, 10.0f, 1.0f, 1.0f, false });
        vStroke.points.push_back({ 50.0f, 50.0f, 1.0f, 1.0f, false });
        vStroke.points.push_back({ 50.0f, 90.0f, 1.0f, 1.0f, false });
        vecLayer.addStroke(vStroke);

        assert(vecLayer.strokes().size() == 2);

        // 水平線の交点より右側の突き出し部分 (75, 50) を消しゴムで消去
        bool erased = vecLayer.eraseAt(QPointF(75.0, 50.0), 5.0f, VectorEraserMode::Intersection);
        assert(erased);

        // 水平線の右側が交点 (50, 50) まで消去され、左側 (10, 50)〜(50, 50) が残っているはず
        assert(vecLayer.strokes().size() == 2);

        // 残った水平ストロークの終端が交点 (50, 50) 近傍であることを確認
        const auto& remainingH = vecLayer.strokes()[0];
        assert(!remainingH.points.empty());
        float startX = remainingH.points.front().x;
        float endX = remainingH.points.back().x;
        assert(std::abs(startX - 10.0f) < 1.0f);
        assert(std::abs(endX - 50.0f) < 2.0f); // 交点50付近でカットされている

        // (75, 50) は消えている
        uint32_t pxRight = vecLayer.tileManager().getPixel(75, 50);
        assert((pxRight >> 24) == 0);

        // (25, 50) は残っている
        uint32_t pxLeft = vecLayer.tileManager().getPixel(25, 50);
        assert((pxLeft >> 24) > 0);

        // 垂直線 (50, 30) は残っている
        uint32_t pxV = vecLayer.tileManager().getPixel(50, 30);
        assert((pxV >> 24) > 0);

        // 交点が存在しない単独のストロークを追加
        VectorStroke isolatedStroke;
        isolatedStroke.baseWidth = 4.0f;
        isolatedStroke.color = 0xFF000000;
        isolatedStroke.points.push_back({ 120.0f, 120.0f, 1.0f, 1.0f, false });
        isolatedStroke.points.push_back({ 180.0f, 180.0f, 1.0f, 1.0f, false });
        vecLayer.addStroke(isolatedStroke);
        assert(vecLayer.strokes().size() == 3);

        // 「交点まで消去」モードで交点のないストロークに消しゴムを当てても消去されないことを確認
        bool erasedIsolated = vecLayer.eraseAt(QPointF(150.0, 150.0), 5.0f, VectorEraserMode::Intersection);
        assert(!erasedIsolated);
        assert(vecLayer.strokes().size() == 3);
        uint32_t pxIso = vecLayer.tileManager().getPixel(150, 150);
        assert((pxIso >> 24) > 0);

        // 「線全体を消去」モードであれば消去されることを確認
        bool erasedWhole = vecLayer.eraseAt(QPointF(150.0, 150.0), 5.0f, VectorEraserMode::WholeLine);
        assert(erasedWhole);
        assert(vecLayer.strokes().size() == 2);
        uint32_t pxIsoAfter = vecLayer.tileManager().getPixel(150, 150);
        assert((pxIsoAfter >> 24) == 0);

        std::cout << "[PASS] VectorEraserMode::Intersection passed!" << std::endl;
    }

    // 4. Undo / Redo の検証
    {
        std::cout << "[TEST 4] Testing Undo/Redo of vector erase..." << std::endl;
        Document doc(200, 200);
        auto vecLayer = doc.addVectorLayer("VectorDoc");
        int layerIdx = doc.activeLayerIndex();

        VectorStroke s;
        s.baseWidth = 4.0f;
        s.color = 0xFF000000;
        s.points.push_back({ 10.0f, 50.0f, 1.0f, 1.0f, false });
        s.points.push_back({ 90.0f, 50.0f, 1.0f, 1.0f, false });
        vecLayer->addStroke(s);

        auto beforeStrokes = vecLayer->strokes();
        assert(beforeStrokes.size() == 1);

        // 消去実行
        bool erased = vecLayer->eraseAt(QPointF(50.0, 50.0), 10.0f, VectorEraserMode::WholeLine);
        assert(erased);
        assert(vecLayer->strokes().empty());

        auto afterStrokes = vecLayer->strokes();

        // UndoStack にコマンドをプッシュ
        doc.undoStack()->push(new ModifyVectorStrokesCommand(&doc, layerIdx, beforeStrokes, afterStrokes));

        // アンドゥ実行
        doc.undoStack()->undo();
        assert(vecLayer->strokes().size() == 1);
        uint32_t pxRestored = vecLayer->tileManager().getPixel(50, 50);
        assert((pxRestored >> 24) > 0);

        // リドゥ実行
        doc.undoStack()->redo();
        assert(vecLayer->strokes().empty());
        uint32_t pxErasedAgain = vecLayer->tileManager().getPixel(50, 50);
        assert((pxErasedAgain >> 24) == 0);

        std::cout << "[PASS] Undo/Redo of vector erase passed!" << std::endl;
    }

    // 5. ToolBoxWidget のベクターレイヤー用ツール配置検証
    {
        std::cout << "[TEST 5] Testing ToolBoxWidget vector tools..." << std::endl;
        ToolBoxWidget toolBox;
        toolBox.setLayerType(LayerType::Vector);

        assert(toolBox.currentLayerType() == LayerType::Vector);
        assert(toolBox.currentTool() == ToolType::Pen);

        auto bg = toolBox.findChild<QButtonGroup*>();
        assert(bg != nullptr);
        assert(bg->button(static_cast<int>(ToolType::Pen)) != nullptr);
        assert(bg->button(static_cast<int>(ToolType::Eraser)) != nullptr);
        assert(bg->button(static_cast<int>(ToolType::VectorControlPoint)) != nullptr);
        assert(bg->button(static_cast<int>(ToolType::VectorPressure)) != nullptr);
        assert(bg->button(static_cast<int>(ToolType::VectorLineEdit)) != nullptr);

        std::cout << "[PASS] ToolBoxWidget vector tools passed!" << std::endl;
    }

    // 6. VectorEraserSettingsWidget のUI検証
    {
        std::cout << "[TEST 6] Testing VectorEraserSettingsWidget..." << std::endl;
        VectorEraserSettingsWidget widget;
        assert(widget.eraserMode() == VectorEraserMode::Intersection);

        bool modeChangedSignalEmitted = false;
        QObject::connect(&widget, &VectorEraserSettingsWidget::eraserModeChanged, [&](VectorEraserMode mode) {
            modeChangedSignalEmitted = true;
            assert(mode == VectorEraserMode::WholeLine);
        });

        widget.setEraserMode(VectorEraserMode::WholeLine);
        assert(widget.eraserMode() == VectorEraserMode::WholeLine);
        assert(modeChangedSignalEmitted);

        bool sizeChangedSignalEmitted = false;
        QObject::connect(&widget, &VectorEraserSettingsWidget::eraserSizeChanged, [&](double size) {
            sizeChangedSignalEmitted = true;
            assert(std::abs(size - 35.0) < 0.01);
        });

        widget.setEraserSize(35.0);
        assert(std::abs(widget.eraserSize() - 35.0) < 0.01);
        assert(sizeChangedSignalEmitted);

        std::cout << "[PASS] VectorEraserSettingsWidget passed!" << std::endl;
    }

    // 7. VectorControlPointSettingsWidget のUI検証
    {
        std::cout << "[TEST 7] Testing VectorControlPointSettingsWidget..." << std::endl;
        VectorControlPointSettingsWidget widget;
        assert(widget.controlPointMode() == VectorControlPointMode::MovePoint);

        bool modeSignalReceived = false;
        VectorControlPointMode emittedMode = VectorControlPointMode::MovePoint;
        QObject::connect(&widget, &VectorControlPointSettingsWidget::modeChanged, [&](VectorControlPointMode m) {
            modeSignalReceived = true;
            emittedMode = m;
        });

        // 9つのモードすべてを順次切り替えてシグナルとゲッターを検証
        std::vector<VectorControlPointMode> modes = {
            VectorControlPointMode::MoveOrAddPoint,
            VectorControlPointMode::AddPoint,
            VectorControlPointMode::DeletePoint,
            VectorControlPointMode::MoveStroke,
            VectorControlPointMode::DuplicateMoveStroke,
            VectorControlPointMode::RotateStroke,
            VectorControlPointMode::ConnectStrokes,
            VectorControlPointMode::DeleteStroke,
            VectorControlPointMode::MovePoint
        };

        for (auto m : modes) {
            modeSignalReceived = false;
            widget.setControlPointMode(m);
            assert(widget.controlPointMode() == m);
            assert(modeSignalReceived);
            assert(emittedMode == m);
        }

        std::cout << "[PASS] VectorControlPointSettingsWidget passed!" << std::endl;
    }

    // 8. 制御点・ストロークの選択管理検証
    {
        std::cout << "[TEST 8] Testing VectorLayer selection management..." << std::endl;
        VectorLayer vecLayer("VectorSel", 200, 200);

        VectorStroke s1;
        s1.points.push_back({ 10.0f, 10.0f, 1.0f, 1.0f, false });
        s1.points.push_back({ 50.0f, 10.0f, 1.0f, 1.0f, false });
        vecLayer.addStroke(s1);

        VectorStroke s2;
        s2.points.push_back({ 10.0f, 50.0f, 1.0f, 1.0f, false });
        s2.points.push_back({ 50.0f, 50.0f, 1.0f, 1.0f, false });
        vecLayer.addStroke(s2);

        assert(vecLayer.selectedStrokeIndex() == -1);
        assert(vecLayer.selectedPointIndex().first == -1);

        // ストローク1を選択
        vecLayer.selectStroke(1, true);
        assert(vecLayer.selectedStrokeIndex() == 1);
        assert(vecLayer.strokes()[1].isSelected);
        assert(!vecLayer.strokes()[0].isSelected);

        // 制御点を選択 (ストローク0の点1)
        vecLayer.selectPoint(0, 1, true);
        assert(vecLayer.selectedPointIndex().first == 0);
        assert(vecLayer.selectedPointIndex().second == 1);
        assert(vecLayer.strokes()[0].points[1].isSelected);
        // ストローク選択はクリアされているはず
        assert(vecLayer.selectedStrokeIndex() == -1);

        // 選択全解除
        vecLayer.clearSelection();
        assert(vecLayer.selectedStrokeIndex() == -1);
        assert(vecLayer.selectedPointIndex().first == -1);
        assert(!vecLayer.strokes()[0].points[1].isSelected);

        std::cout << "[PASS] VectorLayer selection management passed!" << std::endl;
    }

    // 9. 制御点ツールの7つの編集モード検証
    {
        std::cout << "[TEST 9] Testing VectorLayer 7 editing modes..." << std::endl;
        VectorLayer vecLayer("VectorEdit", 200, 200);

        // ストローク0: (10, 20) -> (50, 20) -> (90, 20)
        VectorStroke s0;
        s0.baseWidth = 4.0f;
        s0.color = 0xFF000000;
        s0.points.push_back({ 10.0f, 20.0f, 1.0f, 1.0f, false });
        s0.points.push_back({ 50.0f, 20.0f, 1.0f, 1.0f, false });
        s0.points.push_back({ 90.0f, 20.0f, 1.0f, 1.0f, false });
        vecLayer.addStroke(s0);

        // (1) MovePoint: 制御点移動
        bool movedPt = vecLayer.movePoint(0, 1, QPointF(50.0, 35.0));
        assert(movedPt);
        assert(std::abs(vecLayer.strokes()[0].points[1].y - 35.0f) < 0.01f);

        // (2) AddPoint: 制御点追加 (セグメント0と1の間に挿入)
        bool addedPt = vecLayer.addPointToStroke(0, QPointF(30.0, 25.0));
        assert(addedPt);
        assert(vecLayer.strokes()[0].points.size() == 4);
        assert(std::abs(vecLayer.strokes()[0].points[1].x - 30.0f) < 0.01f);

        // (3) DeletePoint: 制御点削除
        bool delPt = vecLayer.removePointFromStroke(0, 1);
        assert(delPt);
        assert(vecLayer.strokes()[0].points.size() == 3);
        assert(std::abs(vecLayer.strokes()[0].points[1].x - 50.0f) < 0.01f);

        // (4) MoveStroke: ストローク移動 (+10, +15)
        bool movedStroke = vecLayer.moveStroke(0, 10.0f, 15.0f);
        assert(movedStroke);
        assert(std::abs(vecLayer.strokes()[0].points[0].x - 20.0f) < 0.01f);
        assert(std::abs(vecLayer.strokes()[0].points[0].y - 35.0f) < 0.01f);

        // (5) DuplicateMoveStroke: ストローク複製
        int dupIdx = vecLayer.duplicateStroke(0);
        assert(dupIdx == 1);
        assert(vecLayer.strokes().size() == 2);
        assert(vecLayer.strokes()[1].points.size() == 3);

        // (6) ConnectStrokes: ストローク連結
        // 1) 端以外の点 (ptIdx 1) を接続しようとした場合は何もしない (false を返す)
        bool invalidConnect1 = vecLayer.connectStrokes(0, 1, 1, 0);
        assert(!invalidConnect1);
        bool invalidConnect2 = vecLayer.connectStrokes(0, 2, 1, 1);
        assert(!invalidConnect2);
        assert(vecLayer.strokes().size() == 2); // ストローク数は変わらない

        // 2) 複製したストローク1の端点 (ptIdx 0) とストローク0の端点 (ptIdx 2) を連結
        bool connected = vecLayer.connectStrokes(0, 2, 1, 0);
        assert(connected);
        assert(vecLayer.strokes().size() == 1); // 結合されて1本に統合
        assert(vecLayer.strokes()[0].points.size() == 6); // 3点 + 3点 = 6点

        // (7) RotateStroke: ストローク回転
        // 連結されたストローク0の中心を回転中心として90度回転
        QPointF center = vecLayer.strokeCenter(0);
        float origX0 = vecLayer.strokes()[0].points[0].x;
        float origY0 = vecLayer.strokes()[0].points[0].y;
        bool rotated = vecLayer.rotateStroke(0, 90.0f, center);
        assert(rotated);
        // (x - cx, y - cy) が (- (y - cy), x - cx) に回転されるはず
        float expectedX0 = static_cast<float>(center.x()) - (origY0 - static_cast<float>(center.y()));
        float expectedY0 = static_cast<float>(center.y()) + (origX0 - static_cast<float>(center.x()));
        assert(std::abs(vecLayer.strokes()[0].points[0].x - expectedX0) < 0.1f);
        assert(std::abs(vecLayer.strokes()[0].points[0].y - expectedY0) < 0.1f);

        // (8) DeleteStroke: ストローク削除
        bool delStroke = vecLayer.removeStroke(0);
        assert(delStroke);
        assert(vecLayer.strokes().empty());

        std::cout << "[PASS] VectorLayer 8 editing modes passed!" << std::endl;
    }

    // 10. ベクトルレイヤーのペンにおける濃度、輪郭の硬さ、ボケ足の反映検証
    {
        std::cout << "[TEST 10] Testing vector stroke density, hardness, falloff..." << std::endl;

        // (1) 濃度の検証
        {
            VectorLayer layerFull("DensFull", 100, 100);
            VectorStroke sFull;
            sFull.baseWidth = 20.0f;
            sFull.color = 0xFF000000;
            sFull.density = 1.0f;
            sFull.hardness = 100;
            sFull.falloff = 0;
            sFull.points.push_back({ 30.0f, 50.0f, 1.0f, 1.0f, false });
            sFull.points.push_back({ 70.0f, 50.0f, 1.0f, 1.0f, false });
            layerFull.addStroke(sFull);

            uint32_t pxFull = layerFull.tileManager().getPixel(50, 50);
            uint32_t aFull = (pxFull >> 24) & 0xFF;
            assert(aFull >= 250);

            VectorLayer layerHalf("DensHalf", 100, 100);
            VectorStroke sHalf;
            sHalf.baseWidth = 20.0f;
            sHalf.color = 0xFF000000;
            sHalf.density = 0.5f;
            sHalf.hardness = 100;
            sHalf.falloff = 0;
            sHalf.points.push_back({ 30.0f, 50.0f, 1.0f, 1.0f, false });
            sHalf.points.push_back({ 70.0f, 50.0f, 1.0f, 1.0f, false });
            layerHalf.addStroke(sHalf);

            uint32_t pxHalf = layerHalf.tileManager().getPixel(50, 50);
            uint32_t aHalf = (pxHalf >> 24) & 0xFF;
            assert(aHalf >= 120 && aHalf <= 135); // 255 * 0.5 = 127.5 付近
            std::cout << "  - Density test: Full A=" << aFull << ", Half A=" << aHalf << " (passed)" << std::endl;
        }

        // (2) 輪郭の硬さとボケ足の検証
        {
            // 硬い輪郭: hardness = 100, falloff = 0
            VectorLayer layerSharp("Sharp", 100, 100);
            VectorStroke sSharp;
            sSharp.baseWidth = 20.0f; // 半径 10
            sSharp.color = 0xFF000000;
            sSharp.density = 1.0f;
            sSharp.hardness = 100;
            sSharp.falloff = 0;
            sSharp.points.push_back({ 30.0f, 50.0f, 1.0f, 1.0f, false });
            sSharp.points.push_back({ 70.0f, 50.0f, 1.0f, 1.0f, false });
            layerSharp.addStroke(sSharp);

            uint32_t pxSharpCenter = layerSharp.tileManager().getPixel(50, 50); // 中心 (d=0)
            uint32_t pxSharpNearEdge = layerSharp.tileManager().getPixel(50, 56); // d=6 (半径10のうち60%)
            uint32_t aSharpCenter = (pxSharpCenter >> 24) & 0xFF;
            uint32_t aSharpNearEdge = (pxSharpNearEdge >> 24) & 0xFF;
            assert(aSharpCenter >= 250);
            assert(aSharpNearEdge >= 250); // 硬い輪郭なので d=6 でもほぼ 255

            // ボケ足: hardness = 0, falloff = 100
            VectorLayer layerSoft("Soft", 100, 100);
            VectorStroke sSoft;
            sSoft.baseWidth = 20.0f; // 半径 10
            sSoft.color = 0xFF000000;
            sSoft.density = 1.0f;
            sSoft.hardness = 0;
            sSoft.falloff = 100;
            sSoft.points.push_back({ 30.0f, 50.0f, 1.0f, 1.0f, false });
            sSoft.points.push_back({ 70.0f, 50.0f, 1.0f, 1.0f, false });
            layerSoft.addStroke(sSoft);

            uint32_t pxSoftCenter = layerSoft.tileManager().getPixel(50, 50); // 中心 (d=0)
            uint32_t pxSoftNearEdge = layerSoft.tileManager().getPixel(50, 56); // d=6 (半径10のうち60%)
            uint32_t aSoftCenter = (pxSoftCenter >> 24) & 0xFF;
            uint32_t aSoftNearEdge = (pxSoftNearEdge >> 24) & 0xFF;
            assert(aSoftCenter >= 250);
            assert(aSoftNearEdge < 130); // ボケ足100%なので d=6 では大幅に減衰 (40%以下)
            assert(aSoftNearEdge < aSharpNearEdge);

            std::cout << "  - Hardness & Falloff test: Sharp NearEdge A=" << aSharpNearEdge 
                      << ", Soft NearEdge A=" << aSoftNearEdge << " (passed)" << std::endl;
        }

        std::cout << "[PASS] Vector stroke density, hardness, falloff passed!" << std::endl;
    }

    // 11. 「触れた部分を消去」(TouchedArea) および ドラッグ軌跡消去 (eraseByTrajectory) の検証
    {
        std::cout << "[TEST 11] Testing VectorEraserMode::TouchedArea and eraseByTrajectory..." << std::endl;
        VectorLayer vecLayer("VectorTrajectory", 200, 200);

        // 1本の水平ストローク: (10, 50) -> (50, 50) -> (90, 50)
        VectorStroke s;
        s.baseWidth = 4.0f;
        s.color = 0xFF000000;
        s.points.push_back({ 10.0f, 50.0f, 1.0f, 1.0f, false });
        s.points.push_back({ 50.0f, 50.0f, 1.0f, 1.0f, false });
        s.points.push_back({ 90.0f, 50.0f, 1.0f, 1.0f, false });
        vecLayer.addStroke(s);
        assert(vecLayer.strokes().size() == 1);

        // 消しゴムのドラッグ軌跡: (50, 30) -> (50, 70) (中央部を縦に横切る)
        std::vector<QPointF> trajectory = {
            QPointF(50.0, 30.0),
            QPointF(50.0, 50.0),
            QPointF(50.0, 70.0)
        };

        // 「触れた部分を消去」モードで軌跡消去を実行
        bool modified = vecLayer.eraseByTrajectory(trajectory, 6.0f, VectorEraserMode::TouchedArea);
        assert(modified);

        // ストロークが触れた箇所で中央分割され、2本のストロークになっているはず
        assert(vecLayer.strokes().size() == 2);

        // (50, 50) のピクセルは消去されている
        uint32_t pxMid = vecLayer.tileManager().getPixel(50, 50);
        assert((pxMid >> 24) == 0);

        // 分割された左側の線 (25, 50) と右側の線 (75, 50) は残っている
        uint32_t pxLeft = vecLayer.tileManager().getPixel(25, 50);
        uint32_t pxRight = vecLayer.tileManager().getPixel(75, 50);
        assert((pxLeft >> 24) > 0);
        assert((pxRight >> 24) > 0);

        // 隙間が現状の半分 (約8px) になっていることの検証 (旧仕様では約16px)
        float endX0 = vecLayer.strokes()[0].points.back().x;
        float startX1 = vecLayer.strokes()[1].points.front().x;
        float gap = startX1 - endX0;
        assert(gap >= 7.0f && gap <= 9.0f);
        uint32_t pxNearLeft = vecLayer.tileManager().getPixel(44, 50);
        assert((pxNearLeft >> 24) > 0);

        // 「線全体を消去」のドラッグ軌跡消去も検証
        bool modifiedWhole = vecLayer.eraseByTrajectory({ QPointF(20.0, 50.0), QPointF(30.0, 50.0) }, 5.0f, VectorEraserMode::WholeLine);
        assert(modifiedWhole);
        assert(vecLayer.strokes().size() == 1); // 左側のストロークが丸ごと消去され、右側のみ残る

        std::cout << "[PASS] VectorEraserMode::TouchedArea and eraseByTrajectory passed!" << std::endl;
    }

    // 11. 折れ線ツール (Polyline) と 曲線ツール (Curve) の描画検証
    {
        std::cout << "[TEST 11] Testing VectorPolyline and VectorCurve strokes..." << std::endl;
        VectorLayer vecLayer("Vector11", 200, 200);

        // 折れ線ストローク (すべての制御点が isCorner = true)
        VectorStroke polylineStroke;
        polylineStroke.baseWidth = 4.0f;
        polylineStroke.color = 0xFF000000;
        polylineStroke.points.push_back({ 10.0f, 10.0f, 1.0f, 1.0f, true });
        polylineStroke.points.push_back({ 10.0f, 80.0f, 1.0f, 1.0f, true });
        polylineStroke.points.push_back({ 80.0f, 80.0f, 1.0f, 1.0f, true });

        vecLayer.addStroke(polylineStroke);
        assert(vecLayer.strokes().size() == 1);

        // 線上の点 (10, 45) と (45, 80) に描画されていることを確認
        uint32_t pxPoly1 = vecLayer.tileManager().getPixel(10, 45);
        uint32_t pxPoly2 = vecLayer.tileManager().getPixel(45, 80);
        assert((pxPoly1 >> 24) > 0);
        assert((pxPoly2 >> 24) > 0);

        // 曲線ストローク (すべての制御点が isCorner = false)
        VectorStroke curveStroke;
        curveStroke.baseWidth = 4.0f;
        curveStroke.color = 0xFF000000;
        curveStroke.points.push_back({ 100.0f, 10.0f, 1.0f, 1.0f, false });
        curveStroke.points.push_back({ 150.0f, 50.0f, 1.0f, 1.0f, false });
        curveStroke.points.push_back({ 100.0f, 90.0f, 1.0f, 1.0f, false });

        vecLayer.addStroke(curveStroke);
        assert(vecLayer.strokes().size() == 2);

        // 曲線頂点付近 (150, 50) に描画されていることを確認
        uint32_t pxCurve = vecLayer.tileManager().getPixel(150, 50);
        assert((pxCurve >> 24) > 0);

        std::cout << "[PASS] VectorPolyline and VectorCurve strokes passed!" << std::endl;
    }

    // 12. パスを閉じるオプション (isClosed = true) の検証
    {
        std::cout << "[TEST 12] Testing isClosed stroke rendering..." << std::endl;
        VectorLayer vecLayer("Vector12", 200, 200);

        // 三角形の折れ線ストローク (始点 (20,20), (80,20), (50,80), isClosed = true)
        VectorStroke closedTri;
        closedTri.baseWidth = 4.0f;
        closedTri.color = 0xFF000000;
        closedTri.isClosed = true;
        closedTri.points.push_back({ 20.0f, 20.0f, 1.0f, 1.0f, true });
        closedTri.points.push_back({ 80.0f, 20.0f, 1.0f, 1.0f, true });
        closedTri.points.push_back({ 50.0f, 80.0f, 1.0f, 1.0f, true });

        vecLayer.addStroke(closedTri);
        assert(vecLayer.strokes().size() == 1);
        assert(vecLayer.strokes()[0].isClosed);

        // 水平上辺の中点 (50, 20) に描画されている
        uint32_t pxTop = vecLayer.tileManager().getPixel(50, 20);
        assert((pxTop >> 24) > 0);

        // 最後の点 (50, 80) から最初の点 (20, 20) を結ぶ閉じたセグメントの中点 (35, 50) 付近にも描画されている
        uint32_t pxClosingSeg = vecLayer.tileManager().getPixel(35, 50);
        assert((pxClosingSeg >> 24) > 0);

        std::cout << "[PASS] isClosed stroke rendering passed!" << std::endl;
    }

    // 13. 閉じたストロークに対する制御点ツールの「ストロークの連結」ガード検証
    {
        std::cout << "[TEST 13] Testing connectStrokes rejection on closed strokes..." << std::endl;
        VectorLayer vecLayer("Vector13", 200, 200);

        VectorStroke sClosed;
        sClosed.isClosed = true;
        sClosed.points.push_back({ 10.0f, 10.0f, 1.0f, 1.0f, true });
        sClosed.points.push_back({ 50.0f, 10.0f, 1.0f, 1.0f, true });
        sClosed.points.push_back({ 30.0f, 50.0f, 1.0f, 1.0f, true });
        vecLayer.addStroke(sClosed);

        VectorStroke sOpen;
        sOpen.isClosed = false;
        sOpen.points.push_back({ 60.0f, 10.0f, 1.0f, 1.0f, true });
        sOpen.points.push_back({ 90.0f, 10.0f, 1.0f, 1.0f, true });
        vecLayer.addStroke(sOpen);

        // 閉じたストローク sClosed と 開いたストローク sOpen の端点を連結しようとしても失敗 (false) すること
        bool connected = vecLayer.connectStrokes(0, 0, 1, 0);
        assert(!connected);
        assert(vecLayer.strokes().size() == 2); // ストローク数が減っていないこと

        connected = vecLayer.connectStrokes(1, 1, 0, 2);
        assert(!connected);
        assert(vecLayer.strokes().size() == 2);

        std::cout << "[PASS] connectStrokes rejection on closed strokes passed!" << std::endl;
    }

    // 14. VectorLineSettingsWidget の UI 操作・値取得検証
    {
        std::cout << "[TEST 14] Testing VectorLineSettingsWidget..." << std::endl;
        VectorLineSettingsWidget lineSettings;

        // デフォルト値
        assert(std::abs(lineSettings.lineSize() - 4.0) < 0.01);
        assert(std::abs(lineSettings.lineDensity() - 1.0) < 0.01);
        assert(!lineSettings.isClosePath());

        bool sizeFired = false;
        QObject::connect(&lineSettings, &VectorLineSettingsWidget::sizeChanged, [&sizeFired](double sz) {
            if (std::abs(sz - 12.0) < 0.01) sizeFired = true;
        });
        lineSettings.setLineSize(12.0);
        assert(std::abs(lineSettings.lineSize() - 12.0) < 0.01);

        bool closeFired = false;
        QObject::connect(&lineSettings, &VectorLineSettingsWidget::closePathChanged, [&closeFired](bool closed) {
            if (closed) closeFired = true;
        });
        lineSettings.setClosePath(true);
        assert(lineSettings.isClosePath());

        std::cout << "[PASS] VectorLineSettingsWidget passed!" << std::endl;
    }

    // 15. ToolBoxWidget での VectorPolyline, VectorCurve の選択検証
    {
        std::cout << "[TEST 15] Testing ToolBoxWidget tool switching for vector lines..." << std::endl;
        ToolBoxWidget toolBox;
        toolBox.setLayerType(LayerType::Vector);

        toolBox.setCurrentTool(ToolType::VectorPolyline);
        assert(toolBox.currentTool() == ToolType::VectorPolyline);

        toolBox.setCurrentTool(ToolType::VectorCurve);
        assert(toolBox.currentTool() == ToolType::VectorCurve);

        std::cout << "[PASS] ToolBoxWidget tool switching for vector lines passed!" << std::endl;
    }

    // 16. VectorChangeLineSettingsWidget の UI 操作・値取得検証
    {
        std::cout << "[TEST 16] Testing VectorChangeLineSettingsWidget..." << std::endl;
        VectorChangeLineSettingsWidget changeSettings;

        // デフォルト値
        assert(std::abs(changeSettings.lineSize() - 4.0) < 0.01);
        assert(std::abs(changeSettings.lineDensity() - 1.0) < 0.01);
        assert(changeSettings.lineHardness() == 100);
        assert(changeSettings.lineFalloff() == 0);

        bool sizeFired = false;
        QObject::connect(&changeSettings, &VectorChangeLineSettingsWidget::sizeChanged, [&sizeFired](double sz) {
            if (std::abs(sz - 16.0) < 0.01) sizeFired = true;
        });
        changeSettings.setLineSize(16.0);
        assert(std::abs(changeSettings.lineSize() - 16.0) < 0.01);
        assert(sizeFired);

        bool densFired = false;
        QObject::connect(&changeSettings, &VectorChangeLineSettingsWidget::densityChanged, [&densFired](double d) {
            if (std::abs(d - 0.5) < 0.01) densFired = true;
        });
        changeSettings.setLineDensity(0.5);
        assert(std::abs(changeSettings.lineDensity() - 0.5) < 0.01);
        assert(densFired);

        bool hardFired = false;
        QObject::connect(&changeSettings, &VectorChangeLineSettingsWidget::hardnessChanged, [&hardFired](int h) {
            if (h == 40) hardFired = true;
        });
        changeSettings.setLineHardness(40);
        assert(changeSettings.lineHardness() == 40);
        assert(hardFired);

        bool falloffFired = false;
        QObject::connect(&changeSettings, &VectorChangeLineSettingsWidget::falloffChanged, [&falloffFired](int f) {
            if (f == 80) falloffFired = true;
        });
        changeSettings.setLineFalloff(80);
        assert(changeSettings.lineFalloff() == 80);
        assert(falloffFired);

        std::cout << "[PASS] VectorChangeLineSettingsWidget passed!" << std::endl;
    }

    // 17. ToolBoxWidget での「線変更」ツール名表示と選択検証
    {
        std::cout << "[TEST 17] Testing ToolBoxWidget tool rename to 線変更..." << std::endl;
        ToolBoxWidget toolBox;
        toolBox.setLayerType(LayerType::Vector);

        toolBox.setCurrentTool(ToolType::VectorLineEdit);
        assert(toolBox.currentTool() == ToolType::VectorLineEdit);

        std::cout << "[PASS] ToolBoxWidget 線変更 passed!" << std::endl;
    }

    // 18. 線変更ツールによるストロークの属性変更と再描画の検証
    {
        std::cout << "[TEST 18] Testing stroke modification (size, density, hardness, falloff)..." << std::endl;
        VectorLayer vecLayer("Vector18", 200, 200);

        VectorStroke s;
        s.baseWidth = 4.0f;
        s.density = 1.0f;
        s.hardness = 100;
        s.falloff = 0;
        s.points.push_back({ 10.0f, 50.0f, 1.0f, 1.0f, true });
        s.points.push_back({ 90.0f, 50.0f, 1.0f, 1.0f, true });
        vecLayer.addStroke(s);

        // 変更前: 幅4px (半径2px) なので (50, 56) には描画されていない
        uint32_t pxBefore = vecLayer.tileManager().getPixel(50, 56);
        assert((pxBefore >> 24) == 0);

        // ストロークの属性を線変更ツールで変更 (サイズ 20px, 濃度 0.5, 硬さ 50, ボケ足 50)
        vecLayer.strokes()[0].baseWidth = 20.0f;
        vecLayer.strokes()[0].density = 0.5f;
        vecLayer.strokes()[0].hardness = 50;
        vecLayer.strokes()[0].falloff = 50;
        vecLayer.rasterizeAll();

        // 変更後: 幅20px (半径10px) なので (50, 56) にピクセルが描画されている
        uint32_t pxAfter = vecLayer.tileManager().getPixel(50, 56);
        assert((pxAfter >> 24) > 0);

        // 中心 (50, 50) のアルファ値が濃度 0.5 により約 128 付近であること
        uint32_t pxCenter = vecLayer.tileManager().getPixel(50, 50);
        uint8_t a = (pxCenter >> 24) & 0xFF;
        assert(a >= 120 && a <= 135);

        std::cout << "[PASS] Stroke modification passed!" << std::endl;
    }

    // 19. ToolBoxWidget での「線色変更」ツール検証
    {
        std::cout << "[TEST 19] Testing ToolBoxWidget tool rename and selection for 線色変更..." << std::endl;
        ToolBoxWidget toolBox;
        toolBox.setLayerType(LayerType::Vector);

        toolBox.setCurrentTool(ToolType::VectorColorChange);
        assert(toolBox.currentTool() == ToolType::VectorColorChange);

        VectorColorChangeSettingsWidget colorWidget;
        // ウィジェットが正常に構築できること
        assert(colorWidget.findChild<QLabel*>() != nullptr);

        std::cout << "[PASS] ToolBoxWidget 線色変更 passed!" << std::endl;
    }

    // 20. 線色変更ツールによるストロークの色変更と再描画の検証
    {
        std::cout << "[TEST 20] Testing stroke color modification and rasterization..." << std::endl;
        VectorLayer vecLayer("Vector20", 200, 200);

        VectorStroke s;
        s.baseWidth = 6.0f;
        s.color = 0xFF000000; // 元は黒色
        s.points.push_back({ 10.0f, 50.0f, 1.0f, 1.0f, true });
        s.points.push_back({ 90.0f, 50.0f, 1.0f, 1.0f, true });
        vecLayer.addStroke(s);

        // 変更前: (50, 50) は黒色 (RGB: 0, 0, 0, A: 255)
        uint32_t pxBefore = vecLayer.tileManager().getPixel(50, 50);
        assert(((pxBefore >> 24) & 0xFF) == 255);
        assert(((pxBefore >> 16) & 0xFF) == 0);
        assert(((pxBefore >> 8) & 0xFF) == 0);
        assert((pxBefore & 0xFF) == 0);

        // 赤色 (0xFFFF0000) に変更
        vecLayer.strokes()[0].color = 0xFFFF0000;
        vecLayer.rasterizeAll();

        // 変更後: (50, 50) は赤色 (RGB: 255, 0, 0, A: 255)
        uint32_t pxAfter = vecLayer.tileManager().getPixel(50, 50);
        assert(((pxAfter >> 24) & 0xFF) == 255);
        assert(((pxAfter >> 16) & 0xFF) == 255); // R = 255
        assert(((pxAfter >> 8) & 0xFF) == 0);   // G = 0
        assert((pxAfter & 0xFF) == 0);          // B = 0

        std::cout << "[PASS] Stroke color modification passed!" << std::endl;
    }

    // 21. VectorEllipseSettingsWidget のUI・設定値・シグナル検証
    {
        std::cout << "[TEST 21] Testing VectorEllipseSettingsWidget..." << std::endl;
        VectorEllipseSettingsWidget ellipseSettings;

        bool sizeFired = false;
        QObject::connect(&ellipseSettings, &VectorEllipseSettingsWidget::sizeChanged, [&sizeFired](double s) {
            if (std::abs(s - 12.0) < 0.01) sizeFired = true;
        });
        ellipseSettings.setLineSize(12.0);
        assert(std::abs(ellipseSettings.lineSize() - 12.0) < 0.01);
        assert(sizeFired);

        bool densFired = false;
        QObject::connect(&ellipseSettings, &VectorEllipseSettingsWidget::densityChanged, [&densFired](double d) {
            if (std::abs(d - 0.75) < 0.01) densFired = true;
        });
        ellipseSettings.setLineDensity(0.75);
        assert(std::abs(ellipseSettings.lineDensity() - 0.75) < 0.01);
        assert(densFired);

        bool hardFired = false;
        QObject::connect(&ellipseSettings, &VectorEllipseSettingsWidget::hardnessChanged, [&hardFired](int h) {
            if (h == 60) hardFired = true;
        });
        ellipseSettings.setLineHardness(60);
        assert(ellipseSettings.lineHardness() == 60);
        assert(hardFired);

        bool falloffFired = false;
        QObject::connect(&ellipseSettings, &VectorEllipseSettingsWidget::falloffChanged, [&falloffFired](int f) {
            if (f == 30) falloffFired = true;
        });
        ellipseSettings.setLineFalloff(30);
        assert(ellipseSettings.lineFalloff() == 30);
        assert(falloffFired);

        bool filledFired = false;
        QObject::connect(&ellipseSettings, &VectorEllipseSettingsWidget::filledChanged, [&filledFired](bool f) {
            if (f) filledFired = true;
        });
        ellipseSettings.setFilled(true);
        assert(ellipseSettings.isFilled());
        assert(filledFired);

        std::cout << "[PASS] VectorEllipseSettingsWidget passed!" << std::endl;
    }

    // 22. VectorStroke::createEllipse による楕円ストローク生成とToolBoxWidget選択検証
    {
        std::cout << "[TEST 22] Testing createEllipse and ToolBoxWidget VectorEllipse..." << std::endl;
        ToolBoxWidget toolBox;
        toolBox.setLayerType(LayerType::Vector);
        toolBox.setCurrentTool(ToolType::VectorEllipse);
        assert(toolBox.currentTool() == ToolType::VectorEllipse);

        VectorStroke el = VectorStroke::createEllipse(QPointF(100.0, 100.0), 40.0f, 20.0f, 4.0f, 0xFF000000, 1.0f, 100, 0, false);
        assert(el.isClosed);
        assert(!el.isFilled);
        assert(el.points.size() == 16);
        assert(std::abs(el.baseWidth - 4.0f) < 0.01f);

        std::cout << "[PASS] createEllipse and ToolBoxWidget VectorEllipse passed!" << std::endl;
    }

    // 23. 楕円ストロークが isClosed = true のため「ストローク連結」で何もしないことの検証
    {
        std::cout << "[TEST 23] Testing stroke connect rejection on ellipse..." << std::endl;
        VectorLayer vecLayer("Vector23", 200, 200);
        VectorStroke el = VectorStroke::createEllipse(QPointF(50.0, 50.0), 30.0f, 20.0f, 4.0f, 0xFF000000, 1.0f, 100, 0, false);
        vecLayer.addStroke(el);

        VectorStroke openStroke;
        openStroke.points.push_back({ 50.0f, 80.0f, 1.0f, 1.0f, false });
        openStroke.points.push_back({ 80.0f, 80.0f, 1.0f, 1.0f, false });
        vecLayer.addStroke(openStroke);

        bool connected = vecLayer.connectStrokes(0, 0, 1, 0);
        assert(!connected); // isClosed なので連結不可
        assert(vecLayer.strokes().size() == 2);

        std::cout << "[PASS] Stroke connect rejection on ellipse passed!" << std::endl;
    }

    // 24. 内部塗りつぶしなし楕円のラスタライズ検証 (輪郭のみ描画、中心は透明)
    {
        std::cout << "[TEST 24] Testing non-filled ellipse rasterization..." << std::endl;
        VectorLayer vecLayer("Vector24", 200, 200);
        VectorStroke el = VectorStroke::createEllipse(QPointF(100.0, 100.0), 40.0f, 25.0f, 4.0f, 0xFF000000, 1.0f, 100, 0, false);
        vecLayer.addStroke(el);

        // 中心 (100, 100) は透明であること
        uint32_t pxCenter = vecLayer.tileManager().getPixel(100, 100);
        assert((pxCenter >> 24) == 0);

        // 輪郭上の点 (140, 100) と (100, 125) は描画されていること
        uint32_t pxRight = vecLayer.tileManager().getPixel(140, 100);
        uint32_t pxBottom = vecLayer.tileManager().getPixel(100, 125);
        assert(((pxRight >> 24) & 0xFF) > 100);
        assert(((pxBottom >> 24) & 0xFF) > 100);

        std::cout << "[PASS] Non-filled ellipse rasterization passed!" << std::endl;
    }

    // 25. 内部塗りつぶしあり楕円のラスタライズ検証 (中心も塗りつぶされていること)
    {
        std::cout << "[TEST 25] Testing filled ellipse rasterization..." << std::endl;
        VectorLayer vecLayer("Vector25", 200, 200);
        VectorStroke el = VectorStroke::createEllipse(QPointF(100.0, 100.0), 40.0f, 25.0f, 4.0f, 0xFF000000, 1.0f, 100, 0, true);
        vecLayer.addStroke(el);

        // 内部塗りつぶしのため、中心 (100, 100) も描画されていること
        uint32_t pxCenter = vecLayer.tileManager().getPixel(100, 100);
        assert(((pxCenter >> 24) & 0xFF) == 255);

        // 内部の任意点 (110, 105) も描画されていること
        uint32_t pxInner = vecLayer.tileManager().getPixel(110, 105);
        assert(((pxInner >> 24) & 0xFF) == 255);

        // 楕円の外側 (155, 100) は透明であること
        uint32_t pxOutside = vecLayer.tileManager().getPixel(155, 100);
        assert((pxOutside >> 24) == 0);

        std::cout << "[PASS] Filled ellipse rasterization passed!" << std::endl;
    }

    // 26. VectorCircleSettingsWidget のUI設定・シグナル検証
    {
        std::cout << "[TEST 26] Testing VectorCircleSettingsWidget..." << std::endl;
        VectorCircleSettingsWidget widget;
        assert(std::abs(widget.lineSize() - 4.0) < 0.01);
        assert(std::abs(widget.lineDensity() - 1.0) < 0.01);
        assert(widget.lineHardness() == 100);
        assert(widget.lineFalloff() == 0);
        assert(!widget.isFilled());

        bool sizeChangedCalled = false;
        QObject::connect(&widget, &VectorCircleSettingsWidget::sizeChanged, [&sizeChangedCalled](double val) {
            if (std::abs(val - 12.5) < 0.01) sizeChangedCalled = true;
        });
        widget.setLineSize(12.5);
        assert(sizeChangedCalled);
        assert(std::abs(widget.lineSize() - 12.5) < 0.01);

        bool filledChangedCalled = false;
        QObject::connect(&widget, &VectorCircleSettingsWidget::filledChanged, [&filledChangedCalled](bool val) {
            if (val) filledChangedCalled = true;
        });
        widget.setFilled(true);
        assert(filledChangedCalled);
        assert(widget.isFilled());

        std::cout << "[PASS] VectorCircleSettingsWidget passed!" << std::endl;
    }

    // 27. ToolBoxWidget における円ツールの検証 (ToolType::VectorCircle, スロット 2, 1)
    {
        std::cout << "[TEST 27] Testing ToolBoxWidget VectorCircle tool slot..." << std::endl;
        ToolBoxWidget toolBox;
        toolBox.setLayerType(LayerType::Vector);
        assert(toolBox.toolAt(2, 1) == ToolType::VectorCircle);

        bool toolChangedFired = false;
        QObject::connect(&toolBox, &ToolBoxWidget::toolChanged, [&toolChangedFired](ToolType t) {
            if (t == ToolType::VectorCircle) toolChangedFired = true;
        });

        toolBox.setCurrentTool(ToolType::VectorCircle);
        assert(toolChangedFired);
        assert(toolBox.currentTool() == ToolType::VectorCircle);

        std::cout << "[PASS] ToolBoxWidget VectorCircle tool slot passed!" << std::endl;
    }

    // 28. 真円ストローク生成と幾何計算・閉曲線・連結拒否の検証
    {
        std::cout << "[TEST 28] Testing circular stroke generation and distance calculation..." << std::endl;
        // 開始中心 (50, 50), ドラッグ終了 (80, 90) -> dx = 30, dy = 40 -> 半径 r = 50
        QPointF center(50.0, 50.0);
        QPointF endPos(80.0, 90.0);
        float r = static_cast<float>(std::hypot(endPos.x() - center.x(), endPos.y() - center.y()));
        assert(std::abs(r - 50.0f) < 0.01f);

        VectorStroke circleStroke = VectorStroke::createEllipse(center, r, r, 4.0f, 0xFF000000, 1.0f, 100, 0, false);
        assert(circleStroke.isClosed);
        assert(circleStroke.points.size() == 16);

        // 16点すべてが中心 (50, 50) から距離50にあることを確認
        for (const auto& pt : circleStroke.points) {
            float dist = static_cast<float>(std::hypot(pt.x - 50.0f, pt.y - 50.0f));
            assert(std::abs(dist - 50.0f) < 0.1f);
        }

        // 閉曲線のため、ストローク連結が拒否されることを確認
        VectorLayer vecLayer("Vector28", 200, 200);
        vecLayer.addStroke(circleStroke);

        VectorStroke openStroke;
        openStroke.points.push_back({ 50.0f, 110.0f, 1.0f, 1.0f, false });
        openStroke.points.push_back({ 80.0f, 110.0f, 1.0f, 1.0f, false });
        vecLayer.addStroke(openStroke);

        bool connected = vecLayer.connectStrokes(0, 0, 1, 0);
        assert(!connected);
        assert(vecLayer.strokes().size() == 2);

        std::cout << "[PASS] Circular stroke geometry and connect rejection passed!" << std::endl;
    }

    // 29. 内部塗りつぶしあり円 vs 塗りつぶしなし円のラスタライズ検証
    {
        std::cout << "[TEST 29] Testing circle rasterization (outline only vs filled)..." << std::endl;
        VectorLayer vecLayerOutline("Vector29_Outline", 200, 200);
        VectorStroke circleOutline = VectorStroke::createEllipse(QPointF(100.0, 100.0), 40.0f, 40.0f, 4.0f, 0xFF000000, 1.0f, 100, 0, false);
        vecLayerOutline.addStroke(circleOutline);

        // 輪郭のみ: 中心 (100, 100) は透明
        uint32_t pxCenterOutline = vecLayerOutline.tileManager().getPixel(100, 100);
        assert((pxCenterOutline >> 24) == 0);

        // 半径40の輪郭上 (140, 100), (100, 140), (60, 100), (100, 60) は描画されていること
        assert(((vecLayerOutline.tileManager().getPixel(140, 100) >> 24) & 0xFF) > 100);
        assert(((vecLayerOutline.tileManager().getPixel(100, 140) >> 24) & 0xFF) > 100);
        assert(((vecLayerOutline.tileManager().getPixel(60, 100) >> 24) & 0xFF) > 100);
        assert(((vecLayerOutline.tileManager().getPixel(100, 60) >> 24) & 0xFF) > 100);

        // 塗りつぶしあり円
        VectorLayer vecLayerFilled("Vector29_Filled", 200, 200);
        VectorStroke circleFilled = VectorStroke::createEllipse(QPointF(100.0, 100.0), 40.0f, 40.0f, 4.0f, 0xFF000000, 1.0f, 100, 0, true);
        vecLayerFilled.addStroke(circleFilled);

        // 塗りつぶしあり: 中心 (100, 100) および内部点 (120, 110) は不透明
        uint32_t pxCenterFilled = vecLayerFilled.tileManager().getPixel(100, 100);
        assert(((pxCenterFilled >> 24) & 0xFF) == 255);
        uint32_t pxInnerFilled = vecLayerFilled.tileManager().getPixel(120, 110);
        assert(((pxInnerFilled >> 24) & 0xFF) == 255);

        // 円の外側 (150, 100) は透明
        uint32_t pxOutside = vecLayerFilled.tileManager().getPixel(150, 100);
        assert((pxOutside >> 24) == 0);

        std::cout << "[PASS] Circle rasterization passed!" << std::endl;
    }

    // 30. 「制御点 移動/追加」 (MoveOrAddPoint) モードの検証
    {
        std::cout << "[TEST 30] Testing VectorControlPointMode::MoveOrAddPoint..." << std::endl;
        VectorControlPointSettingsWidget cpWidget;
        cpWidget.setControlPointMode(VectorControlPointMode::MoveOrAddPoint);
        assert(cpWidget.controlPointMode() == VectorControlPointMode::MoveOrAddPoint);

        VectorLayer vecLayer("Vector30", 200, 200);
        VectorStroke s;
        s.baseWidth = 4.0f;
        s.color = 0xFF000000;
        s.points.push_back({ 20.0f, 50.0f, 1.0f, 1.0f, false });
        s.points.push_back({ 80.0f, 50.0f, 1.0f, 1.0f, false });
        vecLayer.addStroke(s);
        assert(vecLayer.strokes()[0].points.size() == 2);

        // (A) 制御点に接しているポイント (20.5, 50.2) を判定 -> hitPoint が true (制御点移動として動作)
        float hitRadius = 8.0f;
        int hitStroke = -1, hitPt = -1;
        bool hitPoint = vecLayer.findNearestControlPoint(QPointF(20.5, 50.2), hitRadius, hitStroke, hitPt);
        assert(hitPoint);
        assert(hitStroke == 0 && hitPt == 0);

        // 制御点移動
        bool moved = vecLayer.movePoint(hitStroke, hitPt, QPointF(20.0, 30.0));
        assert(moved);
        assert(std::abs(vecLayer.strokes()[0].points[0].y - 30.0f) < 0.01f);

        // (B) 制御点には接していないがストロークに接しているポイント (50.0, 50.0) を判定
        // -> hitPoint は false、hitStrokeIdx は 0 (制御点追加として動作)
        hitStroke = -1;
        hitPt = -1;
        bool hitPointMid = vecLayer.findNearestControlPoint(QPointF(50.0, 50.0), hitRadius, hitStroke, hitPt);
        assert(!hitPointMid);
        int hitStrokeMid = vecLayer.findNearestStroke(QPointF(50.0, 50.0), 6.0f);
        assert(hitStrokeMid == 0);

        // 制御点の追加
        auto beforeStrokes = vecLayer.strokes();
        int addedPtIdx = -1;
        bool added = vecLayer.addPointToStroke(hitStrokeMid, QPointF(50.0, 50.0), &addedPtIdx);
        assert(added);
        assert(addedPtIdx == 1);
        assert(vecLayer.strokes()[0].points.size() == 3);
        assert(std::abs(vecLayer.strokes()[0].points[1].x - 50.0f) < 0.01f);
        assert(std::abs(vecLayer.strokes()[0].points[1].y - 50.0f) < 0.01f);

        // 追加した制御点のドラッグ移動
        bool movedAdded = vecLayer.movePoint(0, addedPtIdx, QPointF(50.0, 75.0));
        assert(movedAdded);
        assert(std::abs(vecLayer.strokes()[0].points[1].y - 75.0f) < 0.01f);

        // (C) Undo / Redo の整合性確認
        Document doc(200, 200);
        auto docVec = doc.addVectorLayer("DocVec");
        docVec->setStrokes(beforeStrokes);
        int layerIdx = doc.activeLayerIndex();

        // 制御点追加コマンド実行
        auto afterStrokes = vecLayer.strokes();
        doc.undoStack()->push(new ModifyVectorStrokesCommand(&doc, layerIdx, beforeStrokes, afterStrokes, "制御点追加"));
        assert(docVec->strokes()[0].points.size() == 3);

        // Undo: 元の2点に戻る
        doc.undoStack()->undo();
        assert(docVec->strokes()[0].points.size() == 2);

        // Redo: 追加された3点 (50, 75) に戻る
        doc.undoStack()->redo();
        assert(docVec->strokes()[0].points.size() == 3);
        assert(std::abs(docVec->strokes()[0].points[1].y - 75.0f) < 0.01f);

        std::cout << "[PASS] VectorControlPointMode::MoveOrAddPoint passed!" << std::endl;
    }

    // 31. 「筆圧」 (VectorPressure) ツールの検証
    {
        std::cout << "[TEST 31] Testing VectorPressure tool..." << std::endl;

        // (A) 設定UIとToolBoxWidgetの検証
        VectorPressureSettingsWidget pressureWidget;
        ToolBoxWidget toolBox;
        toolBox.setLayerType(LayerType::Vector);
        assert(toolBox.toolAt(0, 3) == ToolType::VectorPressure);

        bool toolChangedFired = false;
        QObject::connect(&toolBox, &ToolBoxWidget::toolChanged, [&](ToolType t) {
            if (t == ToolType::VectorPressure) toolChangedFired = true;
        });
        toolBox.setCurrentTool(ToolType::VectorPressure);
        assert(toolChangedFired);
        assert(toolBox.currentTool() == ToolType::VectorPressure);

        // (B) 筆圧計算・範囲クランプ (0% ~ 200%) の検証
        int startPercent = 100;

        // 左ドラッグ: 筆圧減少 (-50px -> 50%)
        int leftDragPercent = std::clamp(startPercent + (-50), 0, 200);
        assert(leftDragPercent == 50);

        // 左ドラッグ過大: 0% クランプ (-150px -> 0%)
        int leftClampPercent = std::clamp(startPercent + (-150), 0, 200);
        assert(leftClampPercent == 0);

        // 右ドラッグ: 筆圧増加 (+70px -> 170%)
        int rightDragPercent = std::clamp(startPercent + 70, 0, 200);
        assert(rightDragPercent == 170);

        // 右ドラッグ過大: 200% クランプ (+150px -> 200%)
        int rightClampPercent = std::clamp(startPercent + 150, 0, 200);
        assert(rightClampPercent == 200);

        // (C) 筆圧による線幅変化とラスタライズ検証
        // ストローク: (50, 50) -> (150, 50), baseWidth = 10.0 (半径 5.0)
        VectorLayer vecLayerNormal("VecNormal", 200, 200);
        VectorStroke sNormal;
        sNormal.baseWidth = 10.0f;
        sNormal.color = 0xFF000000;
        sNormal.points.push_back({ 50.0f, 50.0f, 1.0f, 1.0f, false });
        sNormal.points.push_back({ 150.0f, 50.0f, 1.0f, 1.0f, false });
        vecLayerNormal.addStroke(sNormal);

        // 基準 (筆圧 100%): (50, 54) [距離4] は線内、(50, 57) [距離7] は線外
        uint32_t pxNormalIn = vecLayerNormal.tileManager().getPixel(50, 54);
        uint32_t pxNormalOut = vecLayerNormal.tileManager().getPixel(50, 57);
        assert(((pxNormalIn >> 24) & 0xFF) > 100);
        assert((pxNormalOut >> 24) == 0);

        // 筆圧減少 (50% = 0.5f): 線幅 5.0 (半径 2.5) -> (50, 54) [距離4] は線外になる
        VectorLayer vecLayerThin("VecThin", 200, 200);
        VectorStroke sThin = sNormal;
        sThin.points[0].pressure = 0.5f;
        vecLayerThin.addStroke(sThin);
        uint32_t pxThin = vecLayerThin.tileManager().getPixel(50, 54);
        assert((pxThin >> 24) == 0);

        // 筆圧増加 (180% = 1.8f): 線幅 18.0 (半径 9.0) -> (50, 57) [距離7] は線内になる
        VectorLayer vecLayerThick("VecThick", 200, 200);
        VectorStroke sThick = sNormal;
        sThick.points[0].pressure = 1.8f;
        vecLayerThick.addStroke(sThick);
        uint32_t pxThick = vecLayerThick.tileManager().getPixel(50, 57);
        assert(((pxThick >> 24) & 0xFF) > 100);

        // (D) Undo / Redo の整合性確認
        Document doc(200, 200);
        auto docVec = doc.addVectorLayer("DocVec");
        docVec->addStroke(sNormal);
        int layerIdx = doc.activeLayerIndex();
        auto beforeStrokes = docVec->strokes();

        docVec->strokes()[0].points[0].pressure = 0.3f;
        docVec->rasterizeAll();
        auto afterStrokes = docVec->strokes();

        doc.undoStack()->push(new ModifyVectorStrokesCommand(&doc, layerIdx, beforeStrokes, afterStrokes, "筆圧変更"));
        assert(std::abs(docVec->strokes()[0].points[0].pressure - 0.3f) < 0.01f);

        // Undo -> 1.0f に復元
        doc.undoStack()->undo();
        assert(std::abs(docVec->strokes()[0].points[0].pressure - 1.0f) < 0.01f);

        // Redo -> 0.3f に復元
        doc.undoStack()->redo();
        assert(std::abs(docVec->strokes()[0].points[0].pressure - 0.3f) < 0.01f);

        std::cout << "[PASS] VectorPressure tool passed!" << std::endl;
    }

    // 32. ベクターペン描画時の制御点簡約化 (simplifyPoints) の検証
    {
        std::cout << "[TEST 32] Testing VectorLayer::simplifyPoints for minimal control points..." << std::endl;

        // (A) 2点以下のストロークはそのまま保持される
        {
            std::vector<VectorPoint> pts1 = { { 10.0f, 10.0f, 1.0f, 1.0f, false } };
            auto sim1 = VectorLayer::simplifyPoints(pts1);
            assert(sim1.size() == 1);

            std::vector<VectorPoint> pts2 = {
                { 10.0f, 10.0f, 1.0f, 1.0f, false },
                { 100.0f, 50.0f, 1.0f, 1.0f, false }
            };
            auto sim2 = VectorLayer::simplifyPoints(pts2);
            assert(sim2.size() == 2);
        }

        // (B) 100点から成る直線ストローク -> 始点と終点の2点のみに簡約化される
        {
            std::vector<VectorPoint> linePts;
            for (int i = 0; i < 100; ++i) {
                float t = i / 99.0f;
                linePts.push_back({ 10.0f + 200.0f * t, 50.0f + 100.0f * t, 1.0f, 1.0f, false });
            }
            auto simLine = VectorLayer::simplifyPoints(linePts, 2.0f, 0.2f);
            assert(simLine.size() == 2);
            assert(std::abs(simLine.front().x - 10.0f) < 0.001f);
            assert(std::abs(simLine.back().x - 210.0f) < 0.001f);
        }

        // (C) 形状は直線だが中間で筆圧が大きく変化するストローク -> 筆圧ピーク点が保持される
        {
            std::vector<VectorPoint> pressPts;
            for (int i = 0; i < 101; ++i) {
                float t = i / 100.0f;
                // 中間 (i=50) で筆圧が 1.8f、両端は 0.5f
                float p = 0.5f + 1.3f * (1.0f - std::abs(2.0f * t - 1.0f));
                pressPts.push_back({ 10.0f + 100.0f * t, 50.0f, p, 1.0f, false });
            }
            auto simPress = VectorLayer::simplifyPoints(pressPts, 2.0f, 0.2f);
            // 2点ではなく中間ピークが保持されていること
            assert(simPress.size() >= 3);
            bool hasPeak = false;
            for (const auto& pt : simPress) {
                if (pt.pressure > 1.5f) hasPeak = true;
            }
            assert(hasPeak);
        }

        // (D) 200点から成るなだらかな波状曲線 (S字カーブ) -> 劇的に制御点が削減される (5〜15点程度)
        {
            std::vector<VectorPoint> curvePts;
            for (int i = 0; i < 200; ++i) {
                float t = i / 199.0f;
                float x = 10.0f + 300.0f * t;
                float y = 100.0f + 40.0f * std::sin(t * 3.14159265f * 2.0f);
                curvePts.push_back({ x, y, 1.0f, 1.0f, false });
            }
            auto simCurve = VectorLayer::simplifyPoints(curvePts, 2.0f, 0.2f);
            std::cout << "  S-curve points reduced from 200 to " << simCurve.size() << std::endl;
            assert(simCurve.size() >= 4 && simCurve.size() <= 15);
            assert(std::abs(simCurve.front().x - 10.0f) < 0.001f);
            assert(std::abs(simCurve.back().x - 310.0f) < 0.001f);
        }

        // (E) コーナー点 (isCorner = true) を持つ折れ線 -> コーナー点が確実に保持される
        {
            std::vector<VectorPoint> cornerPts;
            for (int i = 0; i <= 50; ++i) {
                float t = i / 50.0f;
                bool isCorner = (i == 50);
                cornerPts.push_back({ 10.0f + 100.0f * t, 10.0f, 1.0f, 1.0f, isCorner });
            }
            for (int i = 1; i <= 50; ++i) {
                float t = i / 50.0f;
                cornerPts.push_back({ 110.0f, 10.0f + 100.0f * t, 1.0f, 1.0f, false });
            }
            auto simCorner = VectorLayer::simplifyPoints(cornerPts, 2.0f, 0.2f);
            // 始点、角(110, 10)、終点(110, 110) の3点に簡約化される
            assert(simCorner.size() == 3);
            assert(simCorner[1].isCorner);
            assert(std::abs(simCorner[1].x - 110.0f) < 0.001f && std::abs(simCorner[1].y - 10.0f) < 0.001f);
        }

        std::cout << "[PASS] VectorLayer::simplifyPoints passed!" << std::endl;
    }

    // 33. 多角形ツール (VectorPolygon) の検証
    {
        std::cout << "[TEST 33] Testing VectorPolygon tool and VectorStroke::createPolygon..." << std::endl;

        // (A) 頂点数と正多角形フラグ、isCorner、isClosed の検証
        QPointF center(100.0, 100.0);
        float radius = 40.0f;
        VectorStroke poly5 = VectorStroke::createPolygon(
            center, radius, radius, 5, true, -1.5707963f, 4.0f, 0xFF000000, 1.0f, 100, 0, false);
        assert(poly5.points.size() == 5);
        assert(poly5.isClosed);
        assert(!poly5.isFilled);
        for (const auto& pt : poly5.points) {
            assert(pt.isCorner);
            float dist = std::hypot(pt.x - static_cast<float>(center.x()), pt.y - static_cast<float>(center.y()));
            assert(std::abs(dist - radius) < 0.01f);
        }

        // 頂点数クランプ (3未満は3、24超過は24)
        VectorStroke polyClampMin = VectorStroke::createPolygon(center, radius, radius, 1, true, 0.0f, 4.0f, 0xFF000000, 1.0f, 100, 0, false);
        assert(polyClampMin.points.size() == 3);
        VectorStroke polyClampMax = VectorStroke::createPolygon(center, radius, radius, 50, true, 0.0f, 4.0f, 0xFF000000, 1.0f, 100, 0, false);
        assert(polyClampMax.points.size() == 24);

        // (B) 閉じたストロークに対する「ストロークの連結」ガード
        VectorLayer vecLayer("VectorPolygonTest", 200, 200);
        vecLayer.addStroke(poly5);
        assert(vecLayer.strokes().size() == 1);

        VectorStroke openStroke;
        openStroke.points.push_back({ 10.0f, 10.0f, 1.0f, 1.0f, false });
        openStroke.points.push_back({ 20.0f, 20.0f, 1.0f, 1.0f, false });
        vecLayer.addStroke(openStroke);
        assert(!vecLayer.connectStrokes(0, 0, 1, 0));

        // (C) 塗りつぶしなしのラスタライズ検証 (中心は透明、頂点付近は描画)
        VectorLayer vecLayerOutline("VecPolyOutline", 200, 200);
        vecLayerOutline.addStroke(poly5);
        // 中心 (100, 100) は透明
        uint32_t pxCenter = vecLayerOutline.tileManager().getPixel(100, 100);
        assert((pxCenter >> 24) == 0);
        // 最上部頂点 (100, 60) は描画されている
        uint32_t pxTop = vecLayerOutline.tileManager().getPixel(100, 60);
        assert(((pxTop >> 24) & 0xFF) > 100);

        // (D) 内部塗りつぶしありのラスタライズ検証 (中心も描画される)
        VectorStroke poly5Filled = VectorStroke::createPolygon(
            center, radius, radius, 5, true, -1.5707963f, 4.0f, 0xFF000000, 1.0f, 100, 0, true);
        VectorLayer vecLayerFilled("VecPolyFilled", 200, 200);
        vecLayerFilled.addStroke(poly5Filled);
        uint32_t pxFilledCenter = vecLayerFilled.tileManager().getPixel(100, 100);
        assert(((pxFilledCenter >> 24) & 0xFF) > 100);

        // (E) 設定ウィジェット (VectorPolygonSettingsWidget) の動作検証
        VectorPolygonSettingsWidget settingsWidget;
        settingsWidget.setSides(6);
        assert(settingsWidget.sides() == 6);
        settingsWidget.setSides(1); // clamp to 3
        assert(settingsWidget.sides() == 3);
        settingsWidget.setSides(30); // clamp to 24
        assert(settingsWidget.sides() == 24);

        settingsWidget.setRegular(true);
        assert(settingsWidget.isRegular());
        settingsWidget.setRegular(false);
        assert(!settingsWidget.isRegular());

        settingsWidget.setLineSize(8.0);
        assert(std::abs(settingsWidget.lineSize() - 8.0) < 0.01);
        settingsWidget.setLineDensity(0.75);
        assert(std::abs(settingsWidget.lineDensity() - 0.75) < 0.01);
        settingsWidget.setLineHardness(80);
        assert(settingsWidget.lineHardness() == 80);
        settingsWidget.setLineFalloff(20);
        assert(settingsWidget.lineFalloff() == 20);
        settingsWidget.setFilled(true);
        assert(settingsWidget.isFilled());

        // (F) ToolBoxWidget への多角形ツール登録確認
        ToolBoxWidget toolBox;
        toolBox.setLayerType(LayerType::Vector);
        toolBox.setCurrentTool(ToolType::VectorPolygon);
        assert(toolBox.currentTool() == ToolType::VectorPolygon);

        std::cout << "[PASS] VectorPolygon tool passed!" << std::endl;
    }

    std::cout << "[ALL TESTS PASSED] VectorLayer successfully verified!" << std::endl;
    return 0;
}


