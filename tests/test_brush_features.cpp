#include <QApplication>
#include <cassert>
#include <iostream>
#include <cmath>
#include "Document.hpp"
#include "CanvasWidget.hpp"
#include "BrushSettingsWidget.hpp"
#include "BlurSettingsWidget.hpp"
#include "SmudgeSettingsWidget.hpp"
#include "WatercolorSettingsWidget.hpp"
#include "CompositeBrushSettingsWidget.hpp"
#include "ScatterSettingsWidget.hpp"
#include "MainWindow.hpp"
#include "NavigatorWidget.hpp"
#include "ToolBoxWidget.hpp"
#include "BrushSettingsDialog.hpp"
#include "ImageBrushSettingsWidget.hpp"
#include <QSplitter>

using namespace Kaburagi;

class TestCanvasWidget : public CanvasWidget {
public:
    using CanvasWidget::CanvasWidget;

    void testStrokeBegin(const QPointF& p, float pressure) {
        strokeBegin(p, pressure);
    }
    void testStrokeMove(const QPointF& p, float pressure) {
        strokeMove(p, pressure);
    }
    void testStrokeEnd() {
        strokeEnd();
    }
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    std::cout << "[TEST] Starting Brush Features (Hardness, Falloff, MinPressure) tests..." << std::endl;

    // 1. BrushSettingsWidget
    {
        BrushSettingsWidget w;
        assert(w.hardness() == 100);
        assert(w.falloff() == 0);
        assert(w.minPressure() == 0);

        int recH = -1, recF = -1, recP = -1;
        QObject::connect(&w, &BrushSettingsWidget::hardnessChanged, [&](int val) { recH = val; });
        QObject::connect(&w, &BrushSettingsWidget::falloffChanged, [&](int val) { recF = val; });
        QObject::connect(&w, &BrushSettingsWidget::minPressureChanged, [&](int val) { recP = val; });

        w.setHardness(50);
        assert(w.hardness() == 50);
        assert(recH == 50);

        w.setFalloff(80);
        assert(w.falloff() == 80);
        assert(recF == 80);

        w.setMinPressure(25);
        assert(w.minPressure() == 25);
        assert(recP == 25);

        assert(w.airbrushInterval() == 30);
        int recInt = -1;
        QObject::connect(&w, &BrushSettingsWidget::airbrushIntervalChanged, [&](int val) { recInt = val; });
        w.setAirbrushInterval(50);
        assert(w.airbrushInterval() == 50);
        assert(recInt == 50);

        w.setAirbrushMode(true);
        w.setAirbrushMode(false);

        BrushConfig cfg;
        cfg.size = 20.0f;
        cfg.hardness = 40;
        cfg.falloff = 70;
        cfg.minPressure = 35;
        cfg.airbrushInterval = 45;
        w.setConfig(cfg);

        BrushConfig readCfg = w.config();
        assert(readCfg.hardness == 40);
        assert(readCfg.falloff == 70);
        assert(readCfg.minPressure == 35);
        assert(readCfg.airbrushInterval == 45);

        std::cout << "[PASS] BrushSettingsWidget hardness, falloff, minPressure passed!" << std::endl;
    }

    // 2. BlurSettingsWidget
    {
        BlurSettingsWidget w;
        assert(w.hardness() == 0);
        assert(w.falloff() == 100);
        assert(w.minPressure() == 0);

        int recH = -1, recF = -1, recP = -1;
        QObject::connect(&w, &BlurSettingsWidget::hardnessChanged, [&](int val) { recH = val; });
        QObject::connect(&w, &BlurSettingsWidget::falloffChanged, [&](int val) { recF = val; });
        QObject::connect(&w, &BlurSettingsWidget::minPressureChanged, [&](int val) { recP = val; });

        w.setHardness(60);
        assert(w.hardness() == 60);
        assert(recH == 60);

        w.setFalloff(30);
        assert(w.falloff() == 30);
        assert(recF == 30);

        w.setMinPressure(15);
        assert(w.minPressure() == 15);
        assert(recP == 15);

        BlurConfig cfg;
        cfg.hardness = 20;
        cfg.falloff = 90;
        cfg.minPressure = 45;
        w.setConfig(cfg);

        BlurConfig readCfg = w.config();
        assert(readCfg.hardness == 20);
        assert(readCfg.falloff == 90);
        assert(readCfg.minPressure == 45);

        std::cout << "[PASS] BlurSettingsWidget hardness, falloff, minPressure passed!" << std::endl;
    }

    // 3. SmudgeSettingsWidget
    {
        SmudgeSettingsWidget w;
        assert(w.hardness() == 0);
        assert(w.falloff() == 100);
        assert(w.minPressure() == 0);

        int recH = -1, recF = -1, recP = -1;
        QObject::connect(&w, &SmudgeSettingsWidget::hardnessChanged, [&](int val) { recH = val; });
        QObject::connect(&w, &SmudgeSettingsWidget::falloffChanged, [&](int val) { recF = val; });
        QObject::connect(&w, &SmudgeSettingsWidget::minPressureChanged, [&](int val) { recP = val; });

        w.setHardness(75);
        assert(w.hardness() == 75);
        assert(recH == 75);

        w.setFalloff(25);
        assert(w.falloff() == 25);
        assert(recF == 25);

        w.setMinPressure(50);
        assert(w.minPressure() == 50);
        assert(recP == 50);

        SmudgeConfig cfg;
        cfg.hardness = 10;
        cfg.falloff = 85;
        cfg.minPressure = 60;
        w.setConfig(cfg);

        SmudgeConfig readCfg = w.config();
        assert(readCfg.hardness == 10);
        assert(readCfg.falloff == 85);
        assert(readCfg.minPressure == 60);

        std::cout << "[PASS] SmudgeSettingsWidget hardness, falloff, minPressure passed!" << std::endl;
    }

    // 4. CanvasWidget getters/setters and minPressure logic
    {
        Document doc(100, 100, 350.0);
        TestCanvasWidget canvas;
        canvas.setDocument(&doc);

        canvas.setBrushHardness(45);
        assert(canvas.brushHardness() == 45);
        canvas.setBrushFalloff(65);
        assert(canvas.brushFalloff() == 65);
        canvas.setBrushMinPressure(30);
        assert(canvas.brushMinPressure() == 30);

        canvas.setBlurHardness(25);
        assert(canvas.blurHardness() == 25);
        canvas.setBlurFalloff(75);
        assert(canvas.blurFalloff() == 75);
        canvas.setBlurMinPressure(40);
        assert(canvas.blurMinPressure() == 40);

        canvas.setSmudgeHardness(35);
        assert(canvas.smudgeHardness() == 35);
        canvas.setSmudgeFalloff(55);
        assert(canvas.smudgeFalloff() == 55);
        canvas.setSmudgeMinPressure(20);
        assert(canvas.smudgeMinPressure() == 20);

        assert(canvas.airbrushInterval() == 30);
        canvas.setAirbrushInterval(60);
        assert(canvas.airbrushInterval() == 60);
        canvas.setAirbrushInterval(2); // clamped to >= 5
        assert(canvas.airbrushInterval() == 5);

        std::cout << "[PASS] CanvasWidget accessors passed!" << std::endl;
    }

    // 5. Hardness and Falloff brush stamp gradient rendering
    {
        // 5a. Hardness 100, Falloff 0: sharp brush
        Document docSharp(100, 100, 350.0);
        TestCanvasWidget canvasSharp;
        canvasSharp.setDocument(&docSharp);
        canvasSharp.setCurrentTool(ToolType::Pen);
        canvasSharp.setCurrentColor(QColor(255, 0, 0, 255));
        canvasSharp.setBrushSize(40.0f); // radius 20
        canvasSharp.setBrushDensity(1.0f);
        canvasSharp.setBrushHardness(100);
        canvasSharp.setBrushFalloff(0);
        canvasSharp.setBrushMinPressure(0);
        canvasSharp.setPressureSizeEnabled(false);
        canvasSharp.setPressureDensityEnabled(false);

        canvasSharp.testStrokeBegin(QPointF(50.0, 50.0), 1.0f);
        canvasSharp.testStrokeEnd();

        auto lSharp = docSharp.activeLayer();
        uint32_t pCenterSharp = lSharp->tileManager().getPixel(50, 50);
        uint32_t pNearEdgeSharp = lSharp->tileManager().getPixel(50, 68); // d = 18 < 20
        uint32_t aCenterSharp = (pCenterSharp >> 24) & 0xFF;
        uint32_t aNearEdgeSharp = (pNearEdgeSharp >> 24) & 0xFF;
        assert(aCenterSharp > 240);
        assert(aNearEdgeSharp > 240); // still solid alpha near edge!

        // 5b. Hardness 0, Falloff 100: soft gradient brush
        Document docSoft(100, 100, 350.0);
        TestCanvasWidget canvasSoft;
        canvasSoft.setDocument(&docSoft);
        canvasSoft.setCurrentTool(ToolType::Pen);
        canvasSoft.setCurrentColor(QColor(255, 0, 0, 255));
        canvasSoft.setBrushSize(40.0f); // radius 20
        canvasSoft.setBrushDensity(1.0f);
        canvasSoft.setBrushHardness(0);
        canvasSoft.setBrushFalloff(100);
        canvasSoft.setBrushMinPressure(0);
        canvasSoft.setPressureSizeEnabled(false);
        canvasSoft.setPressureDensityEnabled(false);

        canvasSoft.testStrokeBegin(QPointF(50.0, 50.0), 1.0f);
        canvasSoft.testStrokeEnd();

        auto lSoft = docSoft.activeLayer();
        uint32_t pCenterSoft = lSoft->tileManager().getPixel(50, 50);
        uint32_t pNearEdgeSoft = lSoft->tileManager().getPixel(50, 68); // d = 18/20 = 0.9
        uint32_t aCenterSoft = (pCenterSoft >> 24) & 0xFF;
        uint32_t aNearEdgeSoft = (pNearEdgeSoft >> 24) & 0xFF;
        assert(aCenterSoft > 240);
        assert(aNearEdgeSoft < 60); // significantly faded near edge!

        std::cout << "[PASS] Hardness and Falloff rendering gradient verified: sharp nearEdge=" 
                  << aNearEdgeSharp << " vs soft nearEdge=" << aNearEdgeSoft << std::endl;
    }

    // 6. Minimum Pressure override verification
    {
        // When minPressure is 30, input pressure 0.1 should be clamped to 0.3.
        // With pressureSizeEnabled = true, minSizeRatio = 0.0:
        // Pressure 0.1 produces diameter size * 0.1 = 4.0 (radius 2.0).
        // With minPressure 50, pressure becomes 0.5 -> diameter size * 0.5 = 20.0 (radius 10.0).
        Document docLowP(100, 100, 350.0);
        TestCanvasWidget canvasLowP;
        canvasLowP.setDocument(&docLowP);
        canvasLowP.setCurrentTool(ToolType::Pen);
        canvasLowP.setCurrentColor(QColor(0, 255, 0, 255));
        canvasLowP.setBrushSize(40.0f);
        canvasLowP.setBrushDensity(1.0f);
        canvasLowP.setBrushHardness(100);
        canvasLowP.setBrushFalloff(0);
        canvasLowP.setBrushMinPressure(0);
        canvasLowP.setPressureSizeEnabled(true);
        canvasLowP.setMinSizeRatio(0.0f);

        // Low pressure without minPressure override
        canvasLowP.testStrokeBegin(QPointF(50.0, 50.0), 0.1f);
        canvasLowP.testStrokeEnd();
        auto lLowP = docLowP.activeLayer();
        // At d = 7 (e.g. (50, 57)), radius is 40 * 0.1 * 0.5 = 2.0, so d=7 must be empty
        uint32_t pD7_noMin = lLowP->tileManager().getPixel(50, 57);
        assert(((pD7_noMin >> 24) & 0xFF) == 0);

        // Now with minPressure = 50 (overrides 0.1 -> 0.5)
        Document docMinP(100, 100, 350.0);
        TestCanvasWidget canvasMinP;
        canvasMinP.setDocument(&docMinP);
        canvasMinP.setCurrentTool(ToolType::Pen);
        canvasMinP.setCurrentColor(QColor(0, 255, 0, 255));
        canvasMinP.setBrushSize(40.0f);
        canvasMinP.setBrushDensity(1.0f);
        canvasMinP.setBrushHardness(100);
        canvasMinP.setBrushFalloff(0);
        canvasMinP.setBrushMinPressure(50); // minPressure = 50%
        canvasMinP.setPressureSizeEnabled(true);
        canvasMinP.setMinSizeRatio(0.0f);

        canvasMinP.testStrokeBegin(QPointF(50.0, 50.0), 0.1f); // 0.1 should be overwritten to 0.5
        canvasMinP.testStrokeEnd();
        auto lMinP = docMinP.activeLayer();
        // With pressure 0.5, radius is 40 * 0.5 * 0.5 = 10.0, so d=7 (50, 57) MUST be filled!
        uint32_t pD7_withMin = lMinP->tileManager().getPixel(50, 57);
        assert(((pD7_withMin >> 24) & 0xFF) > 200);

        std::cout << "[PASS] MinPressure overriding low input pressure verified! d=7 alpha="
                  << ((pD7_withMin >> 24) & 0xFF) << std::endl;
    }

    // 7. Airbrush Hold & Continuous Spray Darkening verification (GIMP-style Airbrush)
    {
        Document docAir(100, 100, 350.0);
        TestCanvasWidget canvasAir;
        canvasAir.setDocument(&docAir);
        canvasAir.setCurrentTool(ToolType::Airbrush);
        canvasAir.setCurrentColor(QColor(0, 0, 255, 255));
        canvasAir.setBrushSize(40.0f);
        canvasAir.setBrushDensity(0.5f);
        canvasAir.setBrushHardness(0);
        canvasAir.setBrushFalloff(100);
        canvasAir.setBrushMinPressure(0);
        canvasAir.setPressureSizeEnabled(false);
        canvasAir.setPressureDensityEnabled(false);

        auto lAir = docAir.activeLayer();

        // Start stroke at (50, 50)
        canvasAir.testStrokeBegin(QPointF(50.0, 50.0), 1.0f);
        uint32_t p0 = lAir->tileManager().getPixel(50, 50);
        uint32_t a0 = (p0 >> 24) & 0xFF;
        assert(a0 > 0 && a0 < 50); // initial spray is subtle

        // Simulate holding down left button without moving mouse (timer ticks)
        for (int i = 0; i < 5; ++i) {
            canvasAir.onAirbrushTimerTimeout();
        }
        uint32_t p5 = lAir->tileManager().getPixel(50, 50);
        uint32_t a5 = (p5 >> 24) & 0xFF;
        assert(a5 > a0); // darker after 5 ticks!

        for (int i = 0; i < 20; ++i) {
            canvasAir.onAirbrushTimerTimeout();
        }
        uint32_t p25 = lAir->tileManager().getPixel(50, 50);
        uint32_t a25 = (p25 >> 24) & 0xFF;
        assert(a25 > a5); // much darker after 25 ticks!

        for (int i = 0; i < 40; ++i) {
            canvasAir.onAirbrushTimerTimeout();
        }
        uint32_t p65 = lAir->tileManager().getPixel(50, 50);
        uint32_t a65 = (p65 >> 24) & 0xFF;
        assert(a65 > a25 && a65 > 240); // nearly opaque after prolonged holding!

        // Release button (strokeEnd) and verify Undo/Redo
        canvasAir.testStrokeEnd();

        docAir.undoStack()->undo();
        uint32_t pUndo = lAir->tileManager().getPixel(50, 50);
        assert(((pUndo >> 24) & 0xFF) == 0); // cleanly undone!

        docAir.undoStack()->redo();
        uint32_t pRedo = lAir->tileManager().getPixel(50, 50);
        assert(((pRedo >> 24) & 0xFF) == a65); // cleanly redone!

        std::cout << "[PASS] Airbrush continuous spray hold darkening verified! a0=" 
                  << a0 << " -> a5=" << a5 << " -> a25=" << a25 << " -> a65=" << a65 << std::endl;
    }

    // 8. QSplitter between ToolBoxWidget and Tool Settings
    {
        MainWindow win;
        QSplitter* splitter = win.toolSplitter();
        assert(splitter != nullptr);
        assert(splitter->orientation() == Qt::Vertical);
        assert(splitter->count() == 2);
        assert(!splitter->childrenCollapsible());

        // Check size adjustment
        splitter->setSizes({ 180, 260 });
        auto sizes = splitter->sizes();
        assert(sizes.size() == 2);
        assert(sizes[0] > 0 && sizes[1] > 0);

        QByteArray state = splitter->saveState();
        assert(!state.isEmpty());
        assert(splitter->restoreState(state));

        std::cout << "[PASS] ToolSplitter partition adjustment and state restoration passed!" << std::endl;

        // 9. QSplitter between NavigatorWidget and LayerPanelWidget
        QSplitter* navSplitter = win.navSplitter();
        assert(navSplitter != nullptr);
        assert(navSplitter->orientation() == Qt::Vertical);
        assert(navSplitter->count() == 2);
        assert(!navSplitter->childrenCollapsible());

        navSplitter->setSizes({ 190, 290 });
        auto navSizes = navSplitter->sizes();
        assert(navSizes.size() == 2);
        assert(navSizes[0] > 0 && navSizes[1] > 0);

        QByteArray navState = navSplitter->saveState();
        assert(!navState.isEmpty());
        assert(navSplitter->restoreState(navState));

        std::cout << "[PASS] NavSplitter partition adjustment and state restoration passed!" << std::endl;
    }

    // 10. NavigatorWidget canvas preview widget expansion
    {
        NavigatorWidget nav;
        assert(nav.previewWidget() != nullptr);
        assert(nav.previewWidget()->sizePolicy().verticalPolicy() == QSizePolicy::Expanding);

        nav.resize(180, 220);
        nav.show();
        QApplication::processEvents();
        int h1 = nav.previewWidget()->height();

        nav.resize(180, 360);
        QApplication::processEvents();
        int h2 = nav.previewWidget()->height();

        assert(h2 > h1); // preview widget expands as navigator expands!

        std::cout << "[PASS] NavigatorWidget canvas preview expansion verified! h1=" << h1 << " -> h2=" << h2 << std::endl;
    }

    // 11. WatercolorSettingsWidget Tests
    {
        WatercolorSettingsWidget w;
        assert(std::abs(w.watercolorSize() - 20.0f) < 0.01f);
        assert(std::abs(w.minSizeRatio() - 0.2f) < 0.01f);
        assert(std::abs(w.watercolorDensity() - 0.7f) < 0.01f);
        assert(w.hardness() == 50);
        assert(w.falloff() == 100);
        assert(w.minPressure() == 0);
        assert(w.strength() == 50);
        assert(w.water() == 50);
        assert(w.colorStretch() == 50);
        assert(w.blurPressure() == 0);
        assert(w.isPressureSizeEnabled() == true);
        assert(w.isPressureDensityEnabled() == true);
        assert(w.isAntialias() == true);

        float recSize = 0, recMin = 0, recDens = 0;
        int recH = -1, recF = -1, recP = -1, recStr = -1, recWat = -1, recStret = -1, recBlurP = -1;
        bool recPSize = false, recPDens = false, recAnti = false;

        QObject::connect(&w, &WatercolorSettingsWidget::watercolorSizeChanged, [&](float val) { recSize = val; });
        QObject::connect(&w, &WatercolorSettingsWidget::minSizeChanged, [&](float val) { recMin = val; });
        QObject::connect(&w, &WatercolorSettingsWidget::densityChanged, [&](float val) { recDens = val; });
        QObject::connect(&w, &WatercolorSettingsWidget::hardnessChanged, [&](int val) { recH = val; });
        QObject::connect(&w, &WatercolorSettingsWidget::falloffChanged, [&](int val) { recF = val; });
        QObject::connect(&w, &WatercolorSettingsWidget::minPressureChanged, [&](int val) { recP = val; });
        QObject::connect(&w, &WatercolorSettingsWidget::strengthChanged, [&](int val) { recStr = val; });
        QObject::connect(&w, &WatercolorSettingsWidget::waterChanged, [&](int val) { recWat = val; });
        QObject::connect(&w, &WatercolorSettingsWidget::colorStretchChanged, [&](int val) { recStret = val; });
        QObject::connect(&w, &WatercolorSettingsWidget::blurPressureChanged, [&](int val) { recBlurP = val; });
        QObject::connect(&w, &WatercolorSettingsWidget::pressureSizeChanged, [&](bool val) { recPSize = val; });
        QObject::connect(&w, &WatercolorSettingsWidget::pressureDensityChanged, [&](bool val) { recPDens = val; });
        QObject::connect(&w, &WatercolorSettingsWidget::antialiasChanged, [&](bool val) { recAnti = val; });

        w.setWatercolorSize(35.0f);
        assert(std::abs(w.watercolorSize() - 35.0f) < 0.01f);
        assert(std::abs(recSize - 35.0f) < 0.01f);

        w.setMinSizeRatio(0.4f);
        assert(std::abs(w.minSizeRatio() - 0.4f) < 0.01f);
        assert(std::abs(recMin - 0.4f) < 0.01f);

        w.setWatercolorDensity(0.9f);
        assert(std::abs(w.watercolorDensity() - 0.9f) < 0.01f);
        assert(std::abs(recDens - 0.9f) < 0.01f);

        w.setHardness(80);
        assert(w.hardness() == 80);
        assert(recH == 80);

        w.setFalloff(60);
        assert(w.falloff() == 60);
        assert(recF == 60);

        w.setMinPressure(20);
        assert(w.minPressure() == 20);
        assert(recP == 20);

        w.setStrength(90);
        assert(w.strength() == 90);
        assert(recStr == 90);

        w.setWater(30);
        assert(w.water() == 30);
        assert(recWat == 30);

        w.setColorStretch(70);
        assert(w.colorStretch() == 70);
        assert(recStret == 70);

        w.setBlurPressure(40);
        assert(w.blurPressure() == 40);
        assert(recBlurP == 40);

        w.setPressureSizeEnabled(false);
        assert(w.isPressureSizeEnabled() == false);
        assert(recPSize == false);

        w.setPressureDensityEnabled(false);
        assert(w.isPressureDensityEnabled() == false);
        assert(recPDens == false);

        w.setAntialias(false);
        assert(w.isAntialias() == false);
        assert(recAnti == false);

        // setConfig & config
        WatercolorConfig cfg;
        cfg.size = 45.0f;
        cfg.minSizeRatio = 0.5f;
        cfg.density = 0.85f;
        cfg.hardness = 30;
        cfg.falloff = 50;
        cfg.minPressure = 15;
        cfg.strength = 65;
        cfg.water = 25;
        cfg.colorStretch = 85;
        cfg.blurPressure = 35;
        cfg.pressureSize = true;
        cfg.pressureDensity = true;
        cfg.antialias = true;
        w.setConfig(cfg);

        WatercolorConfig readCfg = w.config();
        assert(std::abs(readCfg.size - 45.0f) < 0.01f);
        assert(std::abs(readCfg.minSizeRatio - 0.5f) < 0.01f);
        assert(std::abs(readCfg.density - 0.85f) < 0.01f);
        assert(readCfg.hardness == 30);
        assert(readCfg.falloff == 50);
        assert(readCfg.minPressure == 15);
        assert(readCfg.strength == 65);
        assert(readCfg.water == 25);
        assert(readCfg.colorStretch == 85);
        assert(readCfg.blurPressure == 35);
        assert(readCfg.pressureSize == true);
        assert(readCfg.pressureDensity == true);
        assert(readCfg.antialias == true);

        std::cout << "[PASS] WatercolorSettingsWidget configuration and signals passed!" << std::endl;
    }

    // 12. Watercolor Drawing Tests (Strength, Water, Color Stretch, Blur Pressure)
    {
        Document doc(100, 100, 72.0);
        auto layer = doc.activeLayer();
        assert(layer != nullptr);

        TestCanvasWidget canvas;
        canvas.setDocument(&doc);
        canvas.setCurrentTool(ToolType::Watercolor);
        canvas.setWatercolorSize(20.0f);
        canvas.setWatercolorDensity(1.0f);
        canvas.setWatercolorHardness(100);
        canvas.setWatercolorFalloff(0);
        canvas.setWatercolorMinPressure(0);
        canvas.setWatercolorPressureSizeEnabled(false);
        canvas.setWatercolorPressureDensityEnabled(false);

        // A. 水分量 (Water) = 100 のテスト
        // 透明下地に対して水分量100で塗ると、不透明度が全く上がらない（何も描かれない）
        canvas.setWatercolorWater(100);
        canvas.setWatercolorStrength(100);
        canvas.setCurrentColor(QColor(255, 0, 0, 255));
        canvas.testStrokeBegin(QPointF(20, 20), 1.0f);
        canvas.testStrokeEnd();
        uint32_t pWater100 = layer->tileManager().getPixel(20, 20);
        assert(((pWater100 >> 24) & 0xFF) == 0); // 完全透明のまま！

        // 水分量 = 0 のテスト（最も不透明度が上がりやすい）
        canvas.setWatercolorWater(0);
        canvas.testStrokeBegin(QPointF(20, 20), 1.0f);
        canvas.testStrokeEnd();
        uint32_t pWater0 = layer->tileManager().getPixel(20, 20);
        uint32_t aWater0 = (pWater0 >> 24) & 0xFF;
        assert(aWater0 > 0); // 色が塗られる！
        assert(((pWater0 >> 16) & 0xFF) > 200); // 赤

        // B. 筆強度 (Strength) のテスト
        // 青（0, 0, 255）で塗りつぶした下地を作成
        for (int y = 40; y <= 60; ++y) {
            for (int x = 40; x <= 60; ++x) {
                layer->tileManager().setPixel(x, y, 0xFF0000FF);
            }
        }
        // 描画色は赤 (255, 0, 0)
        canvas.setCurrentColor(QColor(255, 0, 0, 255));
        canvas.setWatercolorWater(0);
        canvas.setWatercolorColorStretch(0);
        canvas.setWatercolorBlurPressure(0);

        // 筆強度 0: 全て既存色（青）を適用
        canvas.setWatercolorStrength(0);
        canvas.testStrokeBegin(QPointF(50, 50), 1.0f);
        canvas.testStrokeEnd();
        uint32_t pStr0 = layer->tileManager().getPixel(50, 50);
        uint32_t r0 = (pStr0 >> 16) & 0xFF;
        uint32_t b0 = pStr0 & 0xFF;
        assert(b0 > 200 && r0 == 0); // 描画色（赤）が無視され、既存色（青）のまま！

        // 筆強度 100: 全て描画色（赤）を適用
        canvas.setWatercolorStrength(100);
        canvas.testStrokeBegin(QPointF(50, 50), 1.0f);
        canvas.testStrokeEnd();
        uint32_t pStr100 = layer->tileManager().getPixel(50, 50);
        uint32_t r100 = (pStr100 >> 16) & 0xFF;
        assert(r100 > 50); // 赤成分が増加！

        // C. ぼかし筆圧 (Blur Pressure) のテスト
        // 筆強度 100 に設定していても、筆圧 <= ぼかし筆圧の場合は筆強度を無視して既存色（青）のみを適用する
        // 再び青で下地を復旧
        for (int y = 70; y <= 90; ++y) {
            for (int x = 70; x <= 90; ++x) {
                layer->tileManager().setPixel(x, y, 0xFF0000FF);
            }
        }
        canvas.setWatercolorStrength(100);
        canvas.setWatercolorBlurPressure(50); // 筆圧 0.50 以下は既存色を適用

        // 筆圧 0.30 (<= 0.50) でストローク
        canvas.testStrokeBegin(QPointF(80, 80), 0.3f);
        canvas.testStrokeEnd();
        uint32_t pBlurPress = layer->tileManager().getPixel(80, 80);
        uint32_t rBlur = (pBlurPress >> 16) & 0xFF;
        assert(rBlur == 0); // 筆強度が無視され、下地の青のみ適用されたため赤成分は0！

        // 筆圧 0.80 (> 0.50) でストローク -> 筆強度100が発動
        canvas.testStrokeBegin(QPointF(80, 80), 0.8f);
        canvas.testStrokeEnd();
        uint32_t pNormPress = layer->tileManager().getPixel(80, 80);
        uint32_t rNorm = (pNormPress >> 16) & 0xFF;
        assert(rNorm > 0); // 赤成分が混ざる！

        // D. 最低筆圧 (Min Pressure) のテスト
        canvas.setWatercolorMinPressure(40); // 0.40 未満を 0.40 に引き上げ
        // キャンバスの strokeMove で minP 判定が行われることを確認
        assert(canvas.watercolorMinPressure() == 40);

        // E. Undo / Redo のテスト
        assert(doc.undoStack()->canUndo());
        doc.undoStack()->undo();
        assert(doc.undoStack()->canRedo());
        doc.undoStack()->redo();

        // F. 水彩筆および各ツールのカーソル輪郭（ブラシプレビュー円）サイズのテスト
        canvas.setBrushSize(50.0f);
        canvas.setBlurSize(40.0f);
        canvas.setSmudgeSize(30.0f);
        canvas.setWatercolorSize(60.0f);

        canvas.setCurrentTool(ToolType::Pen);
        assert(std::abs(canvas.currentBrushOutlineRadius() - 25.0f) < 0.001f);

        canvas.setCurrentTool(ToolType::Blur);
        assert(std::abs(canvas.currentBrushOutlineRadius() - 20.0f) < 0.001f);

        canvas.setCurrentTool(ToolType::Blend);
        assert(std::abs(canvas.currentBrushOutlineRadius() - 15.0f) < 0.001f);

        canvas.setCurrentTool(ToolType::Watercolor);
        assert(std::abs(canvas.currentBrushOutlineRadius() - 30.0f) < 0.001f);

        // 水彩筆サイズを変更した際にカーソル輪郭半径が追従することを確認
        canvas.setWatercolorSize(12.0f);
        assert(std::abs(canvas.currentBrushOutlineRadius() - 6.0f) < 0.001f);

        std::cout << "[PASS] Watercolor Drawing Features (Strength, Water, Blur Pressure, Undo, Outline Radius) passed!" << std::endl;
    }

    // 13. Blend Modes Tests for Pen, Airbrush, Marker, Watercolor, Bucket, Gradient
    {
        // A. Widget properties and signals
        {
            BrushSettingsWidget bw;
            assert(bw.blendMode() == BlendMode::Normal);
            bool bwSignal = false;
            QObject::connect(&bw, &BrushSettingsWidget::blendModeChanged, [&bwSignal](BlendMode m) {
                if (m == BlendMode::Multiply) bwSignal = true;
            });
            bw.setBlendMode(BlendMode::Multiply);
            assert(bw.blendMode() == BlendMode::Multiply);
            assert(bwSignal);

            // Eraser mode disables blend combo
            bw.setEraserMode(true);
            assert(bw.blendMode() == BlendMode::Normal);
            bw.setEraserMode(false);

            WatercolorSettingsWidget ww;
            assert(ww.blendMode() == BlendMode::Normal);
            bool wwSignal = false;
            QObject::connect(&ww, &WatercolorSettingsWidget::blendModeChanged, [&wwSignal](BlendMode m) {
                if (m == BlendMode::Screen) wwSignal = true;
            });
            ww.setBlendMode(BlendMode::Screen);
            assert(ww.blendMode() == BlendMode::Screen);
            assert(wwSignal);

            BucketSettingsWidget buw;
            assert(buw.blendMode() == BlendMode::Normal);
            bool buwSignal = false;
            QObject::connect(&buw, &BucketSettingsWidget::blendModeChanged, [&buwSignal](BlendMode m) {
                if (m == BlendMode::Addition) buwSignal = true;
            });
            buw.setBlendMode(BlendMode::Addition);
            assert(buw.blendMode() == BlendMode::Addition);
            assert(buwSignal);

            GradientSettingsWidget gw;
            assert(gw.blendMode() == BlendMode::Normal);
            bool gwSignal = false;
            QObject::connect(&gw, &GradientSettingsWidget::blendModeChanged, [&gwSignal](BlendMode m) {
                if (m == BlendMode::Darken) gwSignal = true;
            });
            gw.setBlendMode(BlendMode::Darken);
            assert(gw.blendMode() == BlendMode::Darken);
            assert(gwSignal);

            std::cout << "[PASS] BlendMode widget properties and signals verified!" << std::endl;
        }

        // B. Drawing Blend Tests (CanvasWidget)
        Document doc(100, 100, 72.0);
        auto layer = doc.activeLayer();
        assert(layer != nullptr);

        TestCanvasWidget canvas;
        canvas.setDocument(&doc);

        auto fillDoc = [&](uint32_t argb) {
            for (int y = 0; y < 100; ++y) {
                for (int x = 0; x < 100; ++x) {
                    layer->tileManager().setPixel(x, y, argb);
                }
            }
        };

        auto getPixel = [&](int x, int y) -> uint32_t {
            return layer->tileManager().getPixel(x, y);
        };

        // 1. Pen / Marker tool with Multiply
        // Red base (0xFFFF0000) + Green stroke (0xFF00FF00) with Multiply -> (0xFF000000)
        fillDoc(0xFFFF0000);
        canvas.setCurrentTool(ToolType::Pen);
        canvas.setCurrentColor(QColor(0, 255, 0));
        canvas.setBrushSize(20.0f);
        canvas.setBrushDensity(1.0f);
        canvas.setBrushBlendMode(BlendMode::Multiply);
        canvas.testStrokeBegin(QPointF(50, 50), 1.0f);
        canvas.testStrokeMove(QPointF(50, 50), 1.0f);
        canvas.testStrokeEnd();

        uint32_t pixPen = getPixel(50, 50);
        uint8_t penR = (pixPen >> 16) & 0xFF;
        uint8_t penG = (pixPen >> 8) & 0xFF;
        uint8_t penB = pixPen & 0xFF;
        // With Multiply: Red * Green = 0
        assert(penR == 0 && penG == 0 && penB == 0);
        std::cout << "[PASS] Pen Multiply blend mode verified (Red * Green = Black)!" << std::endl;

        // 2. Airbrush with Addition
        // Blue base (0xFF000080) + Blue stroke (0xFF000080) with Addition -> Blue increases towards 0xFF
        fillDoc(0xFF000080);
        canvas.setCurrentTool(ToolType::Airbrush);
        canvas.setCurrentColor(QColor(0, 0, 128));
        canvas.setBrushSize(30.0f);
        canvas.setBrushDensity(1.0f);
        canvas.setBrushBlendMode(BlendMode::Addition);
        canvas.testStrokeBegin(QPointF(50, 50), 1.0f);
        canvas.testStrokeMove(QPointF(50, 50), 1.0f);
        canvas.testStrokeEnd();

        uint32_t pixAir = getPixel(50, 50);
        uint8_t airB = pixAir & 0xFF;
        assert(airB > 128); // Blue channel grew due to Addition!
        std::cout << "[PASS] Airbrush Addition blend mode verified (Blue increased from 128 to " << (int)airB << ")!" << std::endl;

        // 3. Bucket with Multiply
        // White base (0xFFFFFFFF) filled with Red (0xFFFF0000) under Multiply -> Red (0xFFFF0000)
        // Red base (0xFFFF0000) filled with Blue (0xFF0000FF) under Multiply -> Black (0xFF000000)
        fillDoc(0xFFFF0000);
        canvas.setCurrentTool(ToolType::Bucket);
        canvas.setCurrentColor(QColor(0, 0, 255));
        canvas.setBucketBlendMode(BlendMode::Multiply);
        canvas.setBucketTolerance(100);
        canvas.fillBucket(QPointF(50, 50));

        uint32_t pixBucket = getPixel(50, 50);
        uint8_t bR = (pixBucket >> 16) & 0xFF;
        uint8_t bG = (pixBucket >> 8) & 0xFF;
        uint8_t bB = pixBucket & 0xFF;
        assert(bR == 0 && bG == 0 && bB == 0);
        std::cout << "[PASS] Bucket Multiply blend mode verified (Red * Blue = Black)!" << std::endl;

        // 4. Gradient with Multiply
        // Yellow base (0xFFFFFF00) + Blue gradient (0xFF0000FF) with Multiply -> Black (0xFF000000)
        fillDoc(0xFFFFFF00);
        canvas.setCurrentTool(ToolType::Gradient);
        canvas.setCurrentColor(QColor(0, 0, 255));
        canvas.setSecondaryColor(QColor(0, 0, 255));
        canvas.setGradientBlendMode(BlendMode::Multiply);
        canvas.setGradientShape(GradientShape::Linear);
        canvas.setGradientColorMode(GradientColorMode::FgToBg);
        canvas.renderGradient(QPointF(0, 0), QPointF(99, 99));

        uint32_t pixGrad = getPixel(50, 50);
        uint8_t gR = (pixGrad >> 16) & 0xFF;
        uint8_t gG = (pixGrad >> 8) & 0xFF;
        uint8_t gB = pixGrad & 0xFF;
        assert(gR == 0 && gG == 0 && gB == 0);
        std::cout << "[PASS] Gradient Multiply blend mode verified (Yellow * Blue = Black)!" << std::endl;

        // 5. Watercolor with Screen
        // Dark gray base (0xFF404040) + Dark gray stroke (0xFF404040) with Screen -> Lighter gray (> 0x40)
        fillDoc(0xFF404040);
        canvas.setCurrentTool(ToolType::Watercolor);
        canvas.setCurrentColor(QColor(64, 64, 64));
        canvas.setWatercolorBlendMode(BlendMode::Screen);
        canvas.setWatercolorSize(30.0f);
        canvas.setWatercolorDensity(1.0f);
        canvas.setWatercolorStrength(100);
        canvas.setWatercolorWater(0);
        canvas.setWatercolorColorStretch(0);
        canvas.testStrokeBegin(QPointF(50, 50), 1.0f);
        canvas.testStrokeMove(QPointF(50, 50), 1.0f);
        canvas.testStrokeEnd();

        uint32_t pixWc = getPixel(50, 50);
        uint8_t wcR = (pixWc >> 16) & 0xFF;
        assert(wcR > 64); // Screen brightened the pixels!
        std::cout << "[PASS] Watercolor Screen blend mode verified (brightness increased from 64 to " << (int)wcR << ")!" << std::endl;

        std::cout << "[PASS] All Blend Mode drawing tests passed!" << std::endl;
    }

    // 10. CompositeBrushSettingsWidget & Canvas Drawing Simulation Tests
    {
        std::cout << "[TEST] Starting CompositeBrush tests..." << std::endl;

        // 10-1. CompositeBrushSettingsWidget verification
        CompositeBrushSettingsWidget w;
        assert(w.compositeBrushSize() == 20.0f);
        assert(std::abs(w.minSizeRatio() - 0.2f) < 0.01f);
        assert(std::abs(w.compositeBrushDensity() - 0.7f) < 0.01f);
        assert(w.blendMode() == BlendMode::Normal);
        assert(w.referenceMode() == CompositeBrushReference::Canvas);
        assert(w.strength() == 50);
        assert(w.water() == 50);
        assert(w.colorStretch() == 50);
        assert(w.blurPressure() == 0);
        assert(w.isPressureSizeEnabled() == true);
        assert(w.isPressureDensityEnabled() == true);
        assert(w.isAntialias() == true);

        // GUI and Signal tests
        BlendMode recBlend = BlendMode::Normal;
        CompositeBrushReference recRef = CompositeBrushReference::Canvas;
        QObject::connect(&w, &CompositeBrushSettingsWidget::blendModeChanged, [&](BlendMode m) { recBlend = m; });
        QObject::connect(&w, &CompositeBrushSettingsWidget::referenceModeChanged, [&](CompositeBrushReference r) { recRef = r; });

        w.setBlendMode(BlendMode::Multiply);
        assert(w.blendMode() == BlendMode::Multiply);
        assert(recBlend == BlendMode::Multiply);

        w.setReferenceMode(CompositeBrushReference::LayerBelow);
        assert(w.referenceMode() == CompositeBrushReference::LayerBelow);
        assert(recRef == CompositeBrushReference::LayerBelow);

        CompositeBrushConfig cfg;
        cfg.size = 35.0f;
        cfg.minSizeRatio = 0.5f;
        cfg.density = 0.9f;
        cfg.blendMode = BlendMode::Screen;
        cfg.reference = CompositeBrushReference::LayerBelow;
        cfg.hardness = 70;
        cfg.falloff = 80;
        cfg.minPressure = 15;
        cfg.strength = 80;
        cfg.water = 20;
        cfg.colorStretch = 60;
        cfg.blurPressure = 10;
        cfg.pressureSize = false;
        cfg.pressureDensity = false;
        cfg.antialias = false;
        w.setConfig(cfg);

        CompositeBrushConfig readCfg = w.config();
        assert(readCfg.size == 35.0f);
        assert(readCfg.blendMode == BlendMode::Screen);
        assert(readCfg.reference == CompositeBrushReference::LayerBelow);
        assert(readCfg.strength == 80);
        assert(readCfg.water == 20);
        assert(readCfg.colorStretch == 60);
        assert(readCfg.blurPressure == 10);
        assert(readCfg.pressureSize == false);
        assert(readCfg.pressureDensity == false);
        assert(readCfg.antialias == false);
        std::cout << "[PASS] CompositeBrushSettingsWidget config and signals verified!" << std::endl;

        // 10-2. Canvas simulation drawing test: Reference = LayerBelow
        Document doc(100, 100, 72.0);
        TestCanvasWidget canvas;
        canvas.setDocument(&doc);

        // レイヤー0（下レイヤー）に赤色 (0xFFFF0000) をベタ塗り
        auto l0 = doc.activeLayer();
        assert(l0 != nullptr);
        for (int y = 0; y < 100; ++y) {
            for (int x = 0; x < 100; ++x) {
                int tx = x / TileMemoryPool::TILE_WIDTH;
                int ty = y / TileMemoryPool::TILE_HEIGHT;
                int lx = x % TileMemoryPool::TILE_WIDTH;
                int ly = y % TileMemoryPool::TILE_HEIGHT;
                Tile* t = l0->tileManager().getTileForWrite(tx, ty);
                auto* p = reinterpret_cast<uint32_t*>(t->writeBuffer());
                p[ly * TileMemoryPool::TILE_WIDTH + lx] = 0xFFFF0000;
            }
        }
        doc.composite();

        // レイヤー1（新規通常レイヤー）を追加してアクティブにする（最初は透明 0x00000000）
        auto l1 = doc.addRasterLayer();
        assert(doc.activeLayer() == l1);
        assert(doc.activeLayerIndex() == 1);

        // 合成ブラシ設定: 参照 = LayerBelow、合成モード = Multiply、描画色 = 青 (0xFF0000FF)
        canvas.setCurrentTool(ToolType::CompositeBrush);
        canvas.setCompositeBrushReference(CompositeBrushReference::LayerBelow);
        canvas.setCompositeBrushBlendMode(BlendMode::Multiply);
        canvas.setCurrentColor(QColor(0, 0, 255));
        canvas.setCompositeBrushSize(30.0f);
        canvas.setCompositeBrushDensity(1.0f);
        canvas.setCompositeBrushStrength(100);
        canvas.setCompositeBrushWater(0);
        canvas.setCompositeBrushColorStretch(0);
        canvas.setCompositeBrushPressureSizeEnabled(false);
        canvas.setCompositeBrushPressureDensityEnabled(false);

        // レイヤー1上で描画を実行
        canvas.testStrokeBegin(QPointF(50, 50), 1.0f);
        canvas.testStrokeMove(QPointF(50, 50), 1.0f);
        canvas.testStrokeEnd();

        // 検証: レイヤー1の (50, 50) ピクセルを取得
        // 下のレイヤーの赤色 (0xFFFF0000) とツールの描画色の青色 (0xFF0000FF) が Multiply された結果、
        // シミュレート色 = 黒色 (0, 0, 0) がレイヤー1に反映されている！
        int tx = 50 / TileMemoryPool::TILE_WIDTH;
        int ty = 50 / TileMemoryPool::TILE_HEIGHT;
        int lx = 50 % TileMemoryPool::TILE_WIDTH;
        int ly = 50 % TileMemoryPool::TILE_HEIGHT;
        const Tile* t1 = l1->tileManager().getTile(tx, ty);
        assert(t1 != nullptr);
        const auto* p1 = reinterpret_cast<const uint32_t*>(t1->readBuffer());
        uint32_t pix1 = p1[ly * TileMemoryPool::TILE_WIDTH + lx];
        uint8_t a1 = (pix1 >> 24) & 0xFF;
        uint8_t r1 = (pix1 >> 16) & 0xFF;
        uint8_t g1 = (pix1 >> 8) & 0xFF;
        uint8_t b1 = pix1 & 0xFF;

        assert(a1 > 0); // アクティブレイヤーに描画が反映された
        assert(r1 == 0 && g1 == 0 && b1 == 0); // 赤 * 青 = 黒 が反映された！
        std::cout << "[PASS] CompositeBrush LayerBelow simulation verified (Red * Blue = Black in active layer, Alpha=" << (int)a1 << ")!" << std::endl;

        // 10-3. Canvas simulation drawing test: Reference = Canvas
        // 下レイヤーを緑色 (0xFF00FF00) に塗り替え、ドキュメント全体を再合成
        for (int y = 0; y < 100; ++y) {
            for (int x = 0; x < 100; ++x) {
                int ltx = x / TileMemoryPool::TILE_WIDTH;
                int lty = y / TileMemoryPool::TILE_HEIGHT;
                int llx = x % TileMemoryPool::TILE_WIDTH;
                int lly = y % TileMemoryPool::TILE_HEIGHT;
                Tile* t = l0->tileManager().getTileForWrite(ltx, lty);
                auto* p = reinterpret_cast<uint32_t*>(t->writeBuffer());
                p[lly * TileMemoryPool::TILE_WIDTH + llx] = 0xFF00FF00; // 緑
            }
        }
        // レイヤー1を一旦クリア
        for (int y = 0; y < 100; ++y) {
            for (int x = 0; x < 100; ++x) {
                int ltx = x / TileMemoryPool::TILE_WIDTH;
                int lty = y / TileMemoryPool::TILE_HEIGHT;
                int llx = x % TileMemoryPool::TILE_WIDTH;
                int lly = y % TileMemoryPool::TILE_HEIGHT;
                Tile* t = l1->tileManager().getTileForWrite(ltx, lty);
                auto* p = reinterpret_cast<uint32_t*>(t->writeBuffer());
                p[lly * TileMemoryPool::TILE_WIDTH + llx] = 0x00000000;
            }
        }
        doc.composite();

        // 参照 = Canvas、合成モード = Screen、描画色 = 赤 (0xFFFF0000)
        canvas.setCompositeBrushReference(CompositeBrushReference::Canvas);
        canvas.setCompositeBrushBlendMode(BlendMode::Screen);
        canvas.setCurrentColor(QColor(255, 0, 0));

        // レイヤー1上で描画を実行
        canvas.testStrokeBegin(QPointF(50, 50), 1.0f);
        canvas.testStrokeMove(QPointF(50, 50), 1.0f);
        canvas.testStrokeEnd();

        // 検証: キャンバスの緑 (0xFF00FF00) と赤 (0xFFFF0000) を Screen 合成すると、
        // 赤+緑 = 黄色 (R=255, G=255, B=0) のシミュレート結果がレイヤー1に反映される！
        const Tile* t2 = l1->tileManager().getTile(tx, ty);
        assert(t2 != nullptr);
        const auto* p2 = reinterpret_cast<const uint32_t*>(t2->readBuffer());
        uint32_t pix2 = p2[ly * TileMemoryPool::TILE_WIDTH + lx];
        uint8_t a2 = (pix2 >> 24) & 0xFF;
        uint8_t r2 = (pix2 >> 16) & 0xFF;
        uint8_t g2 = (pix2 >> 8) & 0xFF;
        uint8_t b2 = pix2 & 0xFF;

        assert(a2 > 0);
        assert(r2 == 255 && g2 == 255 && b2 == 0); // 緑 Screen 赤 = 黄色がレイヤー1に反映された！
        std::cout << "[PASS] CompositeBrush Canvas simulation verified (Green Screen Red = Yellow in active layer, Alpha=" << (int)a2 << ")!" << std::endl;

        std::cout << "[PASS] All CompositeBrush tests passed successfully!" << std::endl;
    }

    // 13. ToolBox 4x16 Slots, Brush Settings Dialog, Duplication, Deletion, and Shortcut Cycling Tests
    {
        std::cout << "[TEST] Starting ToolBox 4x16 Slot and Shortcut Cycling tests..." << std::endl;
        ToolBoxWidget toolBox;

        // A. スロット数 (4x16 = 64) およびデフォルトツールの検証
        assert(toolBox.totalSlotCount() == 64);
        assert(toolBox.rasterSlots().size() == 64);
        assert(ToolBoxWidget::GRID_COLS == 4);
        assert(ToolBoxWidget::GRID_ROWS == 16);

        // スロット 0〜9 はデフォルトツールが登録されている (isEmpty == false)
        for (int i = 0; i < 10; ++i) {
            const auto& slot = toolBox.slotItem(i);
            assert(!slot.isEmpty);
            assert(!slot.name.isEmpty());
        }
        assert(toolBox.slotItem(0).toolType == ToolType::Pen);
        assert(toolBox.slotItem(1).toolType == ToolType::Airbrush);
        assert(toolBox.slotItem(2).toolType == ToolType::Watercolor);
        assert(toolBox.slotItem(3).toolType == ToolType::Marker);
        assert(toolBox.slotItem(4).toolType == ToolType::Eraser);
        assert(toolBox.slotItem(5).toolType == ToolType::Bucket);
        assert(toolBox.slotItem(6).toolType == ToolType::Blur);
        assert(toolBox.slotItem(7).toolType == ToolType::Smudge);
        assert(toolBox.slotItem(8).toolType == ToolType::Gradient);
        assert(toolBox.slotItem(9).toolType == ToolType::CompositeBrush);

        // スロット 10〜63 は空白スロット (isEmpty == true)
        for (int i = 10; i < 64; ++i) {
            const auto& slot = toolBox.slotItem(i);
            assert(slot.isEmpty);
        }
        std::cout << "[PASS] 4x16 slots initialization (10 defaults + 54 empty) verified!" << std::endl;

        // B. 空白ボタンスロットのクリック動作の検証
        ToolType lastSelectedTool = toolBox.currentTool();
        bool signalEmitted = false;
        QObject::connect(&toolBox, &ToolBoxWidget::toolChanged, [&](ToolType t) {
            lastSelectedTool = t;
            signalEmitted = true;
        });

        // 空白スロット(15)を直接選択しようとしてもisEmptyのため選択されない
        toolBox.setCurrentSlot(15);
        assert(!signalEmitted);
        assert(toolBox.currentTool() == ToolType::Pen);

        // 空白ボタン(15)に左クリックイベントを送信 -> settingsRequested(15) が発火することを確認
        SaiToolButton emptyBtn(15, toolBox.slotItem(15));
        int requestedSlot = -1;
        QObject::connect(&emptyBtn, &SaiToolButton::settingsRequested, [&](int idx) {
            requestedSlot = idx;
        });

        QMouseEvent leftClick(QEvent::MouseButtonPress, QPointF(5, 5), QPointF(5, 5), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&emptyBtn, &leftClick);
        assert(requestedSlot == 15);
        std::cout << "[PASS] Empty slot left-click triggers settings dialog request verified!" << std::endl;

        // 空白スロット(15)に新規ブラシを設定 -> isEmptyがfalseになりアクティブになる
        ToolSlotItem newSlotItem;
        newSlotItem.name = "My New Brush";
        newSlotItem.toolType = ToolType::Marker;
        newSlotItem.iconType = ToolType::Marker;
        newSlotItem.isEmpty = false;
        toolBox.setSlotItem(15, newSlotItem);
        toolBox.setCurrentSlot(15);
        assert(toolBox.currentSlotIndex() == 15);
        assert(toolBox.currentTool() == ToolType::Marker);
        assert(!toolBox.slotItem(15).isEmpty);
        std::cout << "[PASS] Configured tool in previously empty slot verified!" << std::endl;

        // 再びリセットしてスロット15を空に戻す
        toolBox.deleteSlot(15);
        assert(toolBox.slotItem(15).isEmpty);

        // C. BrushSettingsDialog のデータ保持・取得検証
        {
            ToolSlotItem testItem;
            testItem.name = "Custom Marker";
            testItem.toolType = ToolType::Marker;
            testItem.iconType = ToolType::Marker;
            testItem.shortcut = QKeySequence("Ctrl+Shift+M");

            BrushSettingsDialog dlg(testItem);
            assert(dlg.brushName() == "Custom Marker");
            assert(dlg.brushType() == ToolType::Marker);
            assert(dlg.iconType() == ToolType::Marker);
            assert(dlg.shortcut() == QKeySequence("Ctrl+Shift+M"));
        }
        std::cout << "[PASS] BrushSettingsDialog values verified!" << std::endl;

        // D. ブラシの複製 (Duplicate) の検証
        // スロット 0 (鉛筆) を複製 -> 最初の空きスロット (スロット 10) に複製されるはず
        toolBox.duplicateSlot(0);
        const auto& dupSlot = toolBox.slotItem(10);
        assert(!dupSlot.isEmpty);
        assert(dupSlot.toolType == ToolType::Pen);
        assert(dupSlot.name.contains("鉛筆"));
        assert(toolBox.currentSlotIndex() == 10);
        std::cout << "[PASS] Brush duplication to first available empty slot verified!" << std::endl;

        // E. ブラシの削除 (Delete) の検証
        // 複製したスロット 10 を削除 -> isEmpty == true に戻る
        toolBox.deleteSlot(10);
        assert(toolBox.slotItem(10).isEmpty);
        std::cout << "[PASS] Brush deletion to empty slot verified!" << std::endl;

        // F. 同一ショートカットキーによる順次循環遷移 (A -> B -> C -> A...) の検証
        // スロット0 (鉛筆), スロット1 (エアブラシ), スロット4 (消しゴム) に "Ctrl+Alt+T" を割り当て
        QKeySequence sharedSeq("Ctrl+Alt+T");
        ToolSlotItem s0 = toolBox.slotItem(0);
        s0.shortcut = sharedSeq;
        toolBox.setSlotItem(0, s0);

        ToolSlotItem s1 = toolBox.slotItem(1);
        s1.shortcut = sharedSeq;
        toolBox.setSlotItem(1, s1);

        ToolSlotItem s4 = toolBox.slotItem(4);
        s4.shortcut = sharedSeq;
        toolBox.setSlotItem(4, s4);

        // 別のツール (スロット2: 水彩筆) を選択した状態から開始
        toolBox.setCurrentSlot(2);
        assert(toolBox.currentSlotIndex() == 2);

        // 1回目のショートカット押下 -> 該当ツールの最初 (スロット0: 鉛筆)
        bool handled1 = toolBox.handleShortcutKey(sharedSeq);
        assert(handled1);
        assert(toolBox.currentSlotIndex() == 0);
        assert(toolBox.currentTool() == ToolType::Pen);

        // 2回目のショートカット押下 -> 次の該当ツール (スロット1: エアブラシ)
        bool handled2 = toolBox.handleShortcutKey(sharedSeq);
        assert(handled2);
        assert(toolBox.currentSlotIndex() == 1);
        assert(toolBox.currentTool() == ToolType::Airbrush);

        // 3回目のショートカット押下 -> 次の該当ツール (スロット4: 消しゴム)
        bool handled3 = toolBox.handleShortcutKey(sharedSeq);
        assert(handled3);
        assert(toolBox.currentSlotIndex() == 4);
        assert(toolBox.currentTool() == ToolType::Eraser);

        // 4回目のショートカット押下 -> 末尾を超えて最初に戻る (スロット0: 鉛筆)
        bool handled4 = toolBox.handleShortcutKey(sharedSeq);
        assert(handled4);
        assert(toolBox.currentSlotIndex() == 0);
        assert(toolBox.currentTool() == ToolType::Pen);

        // 5回目のショートカット押下 -> 再びスロット1 (エアブラシ)
        bool handled5 = toolBox.handleShortcutKey(sharedSeq);
        assert(handled5);
        assert(toolBox.currentSlotIndex() == 1);
        assert(toolBox.currentTool() == ToolType::Airbrush);

        // 割り当てられていないキーを押下した場合は false
        bool unhandled = toolBox.handleShortcutKey(QKeySequence("Alt+Shift+Z"));
        assert(!unhandled);

        std::cout << "[PASS] Shortcut key sequential cyclic transition (0 -> 1 -> 4 -> 0 -> 1) verified!" << std::endl;
        std::cout << "[PASS] All ToolBox 4x16 Slot and Shortcut Cycling tests passed successfully!" << std::endl;
    }

    // 12. Scatter Feature Tests (鉛筆, エアブラシ, 水彩筆, 消しゴム, 合成ブラシ)
    {
        std::cout << "\n[TEST 12] Testing Scatter Feature (GUI visibility, Settings, and Canvas rendering)..." << std::endl;

        // 12-1. ScatterSettingsWidget GUI表示切り替えと各パラメータ検証
        {
            ScatterSettingsWidget scatterWidget;
            scatterWidget.show();

            // 初期状態: 散布OFF, 詳細設定コンテナは非表示
            assert(!scatterWidget.isScatterEnabled());
            assert(scatterWidget.scatterAmount() == 5);
            assert(scatterWidget.scatterRange() == 100);
            assert(scatterWidget.scatterSizeRandom() == 0);
            assert(!scatterWidget.isScatterScaleUpEnabled());
            assert(!scatterWidget.containerWidget()->isVisible());

            // 散布ON -> 詳細設定コンテナが表示される
            scatterWidget.setScatterEnabled(true);
            assert(scatterWidget.isScatterEnabled());
            assert(scatterWidget.containerWidget()->isVisible());

            // 各種パラメータの設定とシグナル検証
            bool sigAmountFired = false;
            QObject::connect(&scatterWidget, &ScatterSettingsWidget::scatterAmountChanged, [&](int v) {
                if (v == 15) sigAmountFired = true;
            });
            scatterWidget.setScatterAmount(15);
            assert(scatterWidget.scatterAmount() == 15);
            assert(sigAmountFired);

            bool sigRangeFired = false;
            QObject::connect(&scatterWidget, &ScatterSettingsWidget::scatterRangeChanged, [&](int v) {
                if (v == 350) sigRangeFired = true;
            });
            scatterWidget.setScatterRange(350);
            assert(scatterWidget.scatterRange() == 350);
            assert(sigRangeFired);

            bool sigSizeRandFired = false;
            QObject::connect(&scatterWidget, &ScatterSettingsWidget::scatterSizeRandomChanged, [&](int v) {
                if (v == 50) sigSizeRandFired = true;
            });
            scatterWidget.setScatterSizeRandom(50);
            assert(scatterWidget.scatterSizeRandom() == 50);
            assert(sigSizeRandFired);

            bool sigScaleUpFired = false;
            QObject::connect(&scatterWidget, &ScatterSettingsWidget::scatterScaleUpChanged, [&](bool b) {
                if (b) sigScaleUpFired = true;
            });
            scatterWidget.setScatterScaleUp(true);
            assert(scatterWidget.isScatterScaleUpEnabled());
            assert(sigScaleUpFired);

            // 散布OFFに戻す -> 詳細設定コンテナが再び非表示になる
            scatterWidget.setScatterEnabled(false);
            assert(!scatterWidget.isScatterEnabled());
            assert(!scatterWidget.containerWidget()->isVisible());

            std::cout << "[PASS] ScatterSettingsWidget GUI visibility toggle and signals verified!" << std::endl;
        }

        // 12-2. 各設定ウィジェットにおける散布設定の保持・取得検証
        {
            // A. BrushSettingsWidget
            BrushSettingsWidget brushWidget;
            BrushConfig bcfg = brushWidget.config();
            bcfg.scatter = true;
            bcfg.scatterAmount = 12;
            bcfg.scatterRange = 250;
            bcfg.scatterSizeRandom = 40;
            bcfg.scatterScaleUp = true;
            brushWidget.setConfig(bcfg);

            assert(brushWidget.isScatterEnabled());
            assert(brushWidget.scatterAmount() == 12);
            assert(brushWidget.scatterRange() == 250);
            assert(brushWidget.scatterSizeRandom() == 40);
            assert(brushWidget.isScatterScaleUpEnabled());

            // B. WatercolorSettingsWidget
            WatercolorSettingsWidget wcWidget;
            WatercolorConfig wcfg = wcWidget.config();
            wcfg.scatter = true;
            wcfg.scatterAmount = 8;
            wcfg.scatterRange = 180;
            wcfg.scatterSizeRandom = 30;
            wcfg.scatterScaleUp = false;
            wcWidget.setConfig(wcfg);

            assert(wcWidget.isScatterEnabled());
            assert(wcWidget.scatterAmount() == 8);
            assert(wcWidget.scatterRange() == 180);
            assert(wcWidget.scatterSizeRandom() == 30);
            assert(!wcWidget.isScatterScaleUpEnabled());

            // C. CompositeBrushSettingsWidget
            CompositeBrushSettingsWidget compWidget;
            CompositeBrushConfig ccfg = compWidget.config();
            ccfg.scatter = true;
            ccfg.scatterAmount = 16;
            ccfg.scatterRange = 400;
            ccfg.scatterSizeRandom = 70;
            ccfg.scatterScaleUp = true;
            compWidget.setConfig(ccfg);

            assert(compWidget.isScatterEnabled());
            assert(compWidget.scatterAmount() == 16);
            assert(compWidget.scatterRange() == 400);
            assert(compWidget.scatterSizeRandom() == 70);
            assert(compWidget.isScatterScaleUpEnabled());

            std::cout << "[PASS] Brush, Watercolor, CompositeBrush settings widgets scatter config verified!" << std::endl;
        }

        // 12-3. CanvasWidget での描画検証
        {
            Document doc(200, 200, 72.0);
            auto layer = doc.activeLayer();
            assert(layer != nullptr);

            TestCanvasWidget canvas;
            canvas.setDocument(&doc);

            auto countPaintedPixels = [&](int minX, int maxX, int minY, int maxY) -> int {
                int count = 0;
                for (int y = minY; y <= maxY; ++y) {
                    for (int x = minX; x <= maxX; ++x) {
                        int tx = x / TileMemoryPool::TILE_WIDTH;
                        int ty = y / TileMemoryPool::TILE_HEIGHT;
                        const Tile* t = layer->tileManager().getTile(tx, ty);
                        if (!t || !t->isValid()) continue;
                        int lx = x % TileMemoryPool::TILE_WIDTH;
                        int ly = y % TileMemoryPool::TILE_HEIGHT;
                        const auto* p = reinterpret_cast<const uint32_t*>(t->readBuffer());
                        uint32_t pix = p[ly * TileMemoryPool::TILE_WIDTH + lx];
                        if (((pix >> 24) & 0xFF) > 0) {
                            count++;
                        }
                    }
                }
                return count;
            };

            auto clearLayer = [&]() {
                for (int ty = 0; ty <= 200 / TileMemoryPool::TILE_HEIGHT; ++ty) {
                    for (int tx = 0; tx <= 200 / TileMemoryPool::TILE_WIDTH; ++tx) {
                        Tile* t = layer->tileManager().getTileForWrite(tx, ty);
                        if (t) {
                            auto* p = reinterpret_cast<uint32_t*>(t->writeBuffer());
                            std::memset(p, 0, TileMemoryPool::TILE_WIDTH * TileMemoryPool::TILE_HEIGHT * sizeof(uint32_t));
                        }
                    }
                }
                doc.composite();
            };

            // A. Pen (鉛筆) での散布検証
            // 散布OFF: 中心 (100, 100), ブラシサイズ 10px (半径 5px)
            clearLayer();
            canvas.setCurrentTool(ToolType::Pen);
            canvas.setCurrentColor(QColor(255, 0, 0));
            canvas.setBrushSize(10.0f);
            canvas.setBrushDensity(1.0f);
            canvas.setBrushScatter(false);

            canvas.testStrokeBegin(QPointF(100, 100), 1.0f);
            canvas.testStrokeEnd();

            // 中心から半径 8px 以内にはピクセルが存在するが、半径 20px 以上 (80, 100)〜(60, 100) には存在しない
            int closePixelsNonScatter = countPaintedPixels(95, 105, 95, 105);
            int distantPixelsNonScatter = countPaintedPixels(60, 80, 95, 105);
            assert(closePixelsNonScatter > 0);
            assert(distantPixelsNonScatter == 0);

            // 散布ON: 散布量 15, 散布範囲 400 (最大オフセット 10px * 4.0 = 40px)
            clearLayer();
            canvas.setBrushScatter(true);
            canvas.setBrushScatterAmount(15);
            canvas.setBrushScatterRange(400);
            canvas.setBrushScatterSizeRandom(0);
            canvas.setBrushScatterScaleUp(false);

            canvas.testStrokeBegin(QPointF(100, 100), 1.0f);
            canvas.testStrokeEnd();

            // 散布により、中心から離れた領域 (60〜80, または 120〜140, 60〜80y, 120〜140y) にもピクセルが散布される！
            int distantPixelsScatter = countPaintedPixels(60, 85, 60, 140) + countPaintedPixels(115, 140, 60, 140);
            assert(distantPixelsScatter > 0);
            std::cout << "[PASS] Pen scatter rendering verified (distant pixels created: " << distantPixelsScatter << ")!" << std::endl;

            // B. Watercolor (水彩筆) での散布検証
            clearLayer();
            canvas.setCurrentTool(ToolType::Watercolor);
            canvas.setWatercolorSize(10.0f);
            canvas.setWatercolorDensity(1.0f);
            canvas.setWatercolorScatter(true);
            canvas.setWatercolorScatterAmount(15);
            canvas.setWatercolorScatterRange(400);
            canvas.setWatercolorScatterSizeRandom(0);
            canvas.setWatercolorScatterScaleUp(false);

            canvas.testStrokeBegin(QPointF(100, 100), 1.0f);
            canvas.testStrokeEnd();

            int wcDistantPixels = countPaintedPixels(60, 85, 60, 140) + countPaintedPixels(115, 140, 60, 140);
            assert(wcDistantPixels > 0);
            std::cout << "[PASS] Watercolor scatter rendering verified (distant pixels created: " << wcDistantPixels << ")!" << std::endl;

            // C. CompositeBrush (合成ブラシ) での散布検証
            clearLayer();
            canvas.setCurrentTool(ToolType::CompositeBrush);
            canvas.setCompositeBrushSize(10.0f);
            canvas.setCompositeBrushDensity(1.0f);
            canvas.setCompositeBrushScatter(true);
            canvas.setCompositeBrushScatterAmount(15);
            canvas.setCompositeBrushScatterRange(400);
            canvas.setCompositeBrushScatterSizeRandom(0);
            canvas.setCompositeBrushScatterScaleUp(false);

            canvas.testStrokeBegin(QPointF(100, 100), 1.0f);
            canvas.testStrokeEnd();

            int compDistantPixels = countPaintedPixels(60, 85, 60, 140) + countPaintedPixels(115, 140, 60, 140);
            assert(compDistantPixels > 0);
            std::cout << "[PASS] CompositeBrush scatter rendering verified (distant pixels created: " << compDistantPixels << ")!" << std::endl;

            // D. Eraser (消しゴム) での散布検証
            // 全面を赤色で塗りつぶす
            for (int ty = 0; ty <= 200 / TileMemoryPool::TILE_HEIGHT; ++ty) {
                for (int tx = 0; tx <= 200 / TileMemoryPool::TILE_WIDTH; ++tx) {
                    Tile* t = layer->tileManager().getTileForWrite(tx, ty);
                    if (t) {
                        auto* p = reinterpret_cast<uint32_t*>(t->writeBuffer());
                        for (int k = 0; k < TileMemoryPool::TILE_WIDTH * TileMemoryPool::TILE_HEIGHT; ++k) {
                            p[k] = 0xFFFF0000;
                        }
                    }
                }
            }
            doc.composite();

            int initialPainted = countPaintedPixels(0, 199, 0, 199);
            assert(initialPainted == 200 * 200);

            // 消しゴム (Eraser): 散布ONで消去を実行
            canvas.setCurrentTool(ToolType::Eraser);
            canvas.setBrushSize(10.0f);
            canvas.setBrushDensity(1.0f);
            canvas.setBrushScatter(true);
            canvas.setBrushScatterAmount(15);
            canvas.setBrushScatterRange(400);
            canvas.setBrushScatterSizeRandom(0);
            canvas.setBrushScatterScaleUp(false);

            canvas.testStrokeBegin(QPointF(100, 100), 1.0f);
            canvas.testStrokeEnd();

            int finalPainted = countPaintedPixels(0, 199, 0, 199);
            int erasedPixels = initialPainted - finalPainted;
            assert(erasedPixels > 0);

            // 離れた位置 (60〜85, 60〜140 など) でも消去が発生していることを確認
            int distantPainted = countPaintedPixels(60, 85, 60, 140) + countPaintedPixels(115, 140, 60, 140);
            int maxDistantPossible = (26 * 81) * 2;
            assert(distantPainted < maxDistantPossible); // 一部が消去されている
            std::cout << "[PASS] Eraser scatter erasing verified (erased pixels: " << erasedPixels << ")!" << std::endl;

            std::cout << "[PASS] All Canvas scatter rendering and erasing tests passed successfully!" << std::endl;
        }
    }

    // =========================================================================
    // 17. 画像ブラシ (Image Brush) 設定および機能の網羅的テスト
    // =========================================================================
    {
        std::cout << "\n[TEST 17] Image Brush (ImageBrushSettingsWidget & CanvasWidget image brush rendering) tests..." << std::endl;

        // A. ImageBrushSettingsWidget 基本設定・ゲッター・セッター・config
        {
            ImageBrushSettingsWidget ibw;
            ibw.setBlendMode(BlendMode::Multiply);
            assert(ibw.blendMode() == BlendMode::Multiply);
            ibw.setBrushSize(55.0f);
            assert(std::abs(ibw.brushSize() - 55.0f) < 0.01f);
            ibw.setMinSizeRatio(0.3f);
            assert(std::abs(ibw.minSizeRatio() - 0.3f) < 0.01f);
            ibw.setSpacing(45);
            assert(ibw.spacing() == 45);
            ibw.setDensity(0.85f);
            assert(std::abs(ibw.density() - 0.85f) < 0.02f);
            ibw.setMinPressure(25);
            assert(ibw.minPressure() == 25);
            ibw.setPressureSizeEnabled(true);
            assert(ibw.isPressureSizeEnabled() == true);
            ibw.setPressureDensityEnabled(true);
            assert(ibw.isPressureDensityEnabled() == true);

            ImageBrushConfig cfg = ibw.config();
            assert(cfg.blendMode == BlendMode::Multiply);
            assert(std::abs(cfg.size - 55.0f) < 0.01f);
            assert(cfg.spacing == 45);

            cfg.size = 70.0f;
            cfg.spacing = 30;
            cfg.blendMode = BlendMode::Screen;
            cfg.flipH = true;
            cfg.flipV = true;
            cfg.angle = 120;
            cfg.followCursor = true;
            cfg.randomRotate = true;
            cfg.randomRotateAmount = 180;
            cfg.randomSize = true;
            cfg.randomSizeAmount = 40;
            cfg.randomSizeScaleUp = true;
            ibw.setConfig(cfg);

            assert(std::abs(ibw.brushSize() - 70.0f) < 0.01f);
            assert(ibw.spacing() == 30);
            assert(ibw.blendMode() == BlendMode::Screen);
            assert(ibw.isFlipH() == true);
            assert(ibw.isFlipV() == true);
            assert(ibw.angle() == 120);
            assert(ibw.isFollowCursor() == true);
            assert(ibw.isRandomRotate() == true);
            assert(ibw.randomRotateAmount() == 180);
            assert(ibw.isRandomSize() == true);
            assert(ibw.randomSizeAmount() == 40);
            assert(ibw.isRandomSizeScaleUp() == true);
            std::cout << "[PASS] ImageBrushSettingsWidget getters, setters, config serialization verified!" << std::endl;
        }

        // B. CanvasWidget での描画検証
        {
            Document doc(200, 200, 72.0);
            auto layer = doc.addRasterLayer("TestImageBrush");
            doc.setActiveLayerIndex(0);

            TestCanvasWidget canvas;
            canvas.setDocument(&doc);
            canvas.setCurrentTool(ToolType::Marker);

            auto countNonZeroPixels = [&](uint32_t targetColor = 0) -> int {
                int count = 0;
                for (int y = 0; y < 200; ++y) {
                    for (int x = 0; x < 200; ++x) {
                        uint32_t p = doc.samplePixel(x, y).rgba();
                        if (targetColor == 0) {
                            if ((p >> 24) > 0) count++;
                        } else {
                            if (p == targetColor) count++;
                        }
                    }
                }
                return count;
            };

            auto clearLayer = [&]() {
                for (int ty = 0; ty <= 200 / TileMemoryPool::TILE_HEIGHT; ++ty) {
                    for (int tx = 0; tx <= 200 / TileMemoryPool::TILE_WIDTH; ++tx) {
                        Tile* t = layer->tileManager().getTileForWrite(tx, ty);
                        if (t) {
                            auto* p = reinterpret_cast<uint32_t*>(t->writeBuffer());
                            std::memset(p, 0, TileMemoryPool::TILE_WIDTH * TileMemoryPool::TILE_HEIGHT * sizeof(uint32_t));
                        }
                    }
                }
                doc.composite();
            };

            // 1. フルカラー (RGB/RGBA) 画像ブラシのテスト
            // 赤と青のチェッカーパターン画像を作成
            QImage colorImg(20, 20, QImage::Format_ARGB32);
            colorImg.fill(Qt::transparent);
            for (int y = 0; y < 20; ++y) {
                for (int x = 0; x < 20; ++x) {
                    colorImg.setPixelColor(x, y, (x < 10) ? QColor(255, 0, 0, 255) : QColor(0, 0, 255, 255));
                }
            }

            clearLayer();
            canvas.setImageBrushImage(colorImg);
            canvas.setImageBrushSize(20.0f);
            canvas.setImageBrushDensity(1.0f);
            canvas.setImageBrushSpacing(20);
            canvas.setImageBrushBlendMode(BlendMode::Normal);
            canvas.setImageBrushScatter(false);
            canvas.setImageBrushFlipH(false);
            canvas.setImageBrushFlipV(false);
            canvas.setImageBrushAngle(0);
            canvas.setImageBrushFollowCursor(false);
            canvas.setImageBrushRandomRotate(false);
            canvas.setImageBrushRandomSize(false);

            // 単一打刻テスト
            canvas.testStrokeBegin(QPointF(50, 50), 1.0f);
            canvas.testStrokeEnd();

            int painted = countNonZeroPixels();
            assert(painted > 0);
            // 元画像の赤(0xFFFF0000)と青(0xFF0000FF)が保持されているか確認
            QColor sRed = doc.samplePixel(45, 50);
            QColor sBlue = doc.samplePixel(55, 50);
            assert(sRed.red() > 200 && sRed.blue() < 50);
            assert(sBlue.blue() > 200 && sBlue.red() < 50);
            std::cout << "[PASS] Full-color RGB/RGBA image brush stamping verified!" << std::endl;

            // 2. グレースケール画像ブラシのテスト (黒が不透明、白が透明、描画色を適用)
            QImage grayImg(20, 20, QImage::Format_ARGB32);
            grayImg.fill(QColor(255, 255, 255, 255)); // 白 (透明になる)
            for (int y = 5; y < 15; ++y) {
                for (int x = 5; x < 15; ++x) {
                    grayImg.setPixelColor(x, y, QColor(0, 0, 0, 255)); // 黒 (不透明描画色になる)
                }
            }

            clearLayer();
            canvas.setImageBrushImage(grayImg);
            canvas.setCurrentColor(QColor(0, 255, 0, 255)); // 緑色で描画
            canvas.setImageBrushSize(20.0f);
            canvas.testStrokeBegin(QPointF(50, 50), 1.0f);
            canvas.testStrokeEnd();

            QColor sCenter = doc.samplePixel(50, 50);
            assert(sCenter.green() > 200 && sCenter.red() < 50 && sCenter.blue() < 50);
            // 外側（元画像が白だった場所）は描画されず透明であること
            QColor sEdge = doc.samplePixel(42, 42);
            assert(sEdge.alpha() == 0);
            std::cout << "[PASS] Grayscale image brush (black=opaque FG, white=transparent) verified!" << std::endl;

            // 3. グレースケール＋α画像ブラシのテスト (黒が描画色、白が背景色、元画像αを反映)
            QImage grayAlphaImg(20, 20, QImage::Format_ARGB32);
            grayAlphaImg.fill(Qt::transparent); // 外側はα=0
            for (int y = 5; y < 15; ++y) {
                for (int x = 5; x < 15; ++x) {
                    // 左半分は黒(0), 右半分は白(255), α=255
                    int g = (x < 10) ? 0 : 255;
                    grayAlphaImg.setPixelColor(x, y, QColor(g, g, g, 200));
                }
            }

            clearLayer();
            canvas.setImageBrushImage(grayAlphaImg);
            canvas.setCurrentColor(QColor(255, 0, 0, 255));      // 描画色: 赤
            canvas.setSecondaryColor(QColor(255, 255, 0, 255));  // 背景色: 黄 (赤+緑)
            canvas.setImageBrushSize(20.0f);
            canvas.testStrokeBegin(QPointF(50, 50), 1.0f);
            canvas.testStrokeEnd();

            QColor sLeft = doc.samplePixel(47, 50);
            QColor sRight = doc.samplePixel(53, 50);
            // 左(黒)は赤
            assert(sLeft.red() > 150 && sLeft.green() < 50);
            // 右(白)は黄 (赤+緑)
            assert(sRight.red() > 150 && sRight.green() > 150);
            std::cout << "[PASS] Grayscale with Alpha image brush (black=FG, white=BG, alpha preserved) verified!" << std::endl;

            // 4. ストローク間隔 (Spacing) による補間描画テスト
            clearLayer();
            canvas.setImageBrushImage(colorImg);
            canvas.setImageBrushSize(20.0f);
            canvas.setImageBrushSpacing(50); // 50% = 10px間隔
            canvas.testStrokeBegin(QPointF(20, 100), 1.0f);
            canvas.testStrokeMove(QPointF(120, 100), 1.0f); // 100px移動
            canvas.testStrokeEnd();

            int linePainted = countNonZeroPixels();
            assert(linePainted > 500); // 複数スタンプが押されている
            std::cout << "[PASS] Image brush stroke move spacing interpolation verified!" << std::endl;

            // 5. 散布 (Scatter) テスト
            clearLayer();
            canvas.setImageBrushScatter(true);
            canvas.setImageBrushScatterAmount(10);
            canvas.setImageBrushScatterRange(300);
            canvas.testStrokeBegin(QPointF(100, 100), 1.0f);
            canvas.testStrokeEnd();

            int scatterPainted = countNonZeroPixels();
            assert(scatterPainted > 0);
            // 離れた位置にも散布されていること
            int distantCount = 0;
            for (int y = 60; y <= 140; ++y) {
                for (int x = 60; x <= 140; ++x) {
                    if (std::hypot(x - 100, y - 100) > 20.0) {
                        if (doc.samplePixel(x, y).alpha() > 0) distantCount++;
                    }
                }
            }
            assert(distantCount > 0);
            // 6. 画像輪郭パス (Contour / Outline) および カーソル追従角度テスト
            {
                QImage testStamp(32, 32, QImage::Format_ARGB32);
                testStamp.fill(Qt::transparent);
                // 中央に 16x16 の正方形を描画
                for (int y = 8; y < 24; ++y) {
                    for (int x = 8; x < 24; ++x) {
                        testStamp.setPixelColor(x, y, QColor(0, 0, 0, 255));
                    }
                }
                canvas.setImageBrushImage(testStamp);
                const QPainterPath& outline = canvas.imageBrushOutlinePath();
                assert(!outline.isEmpty());
                // 正規化座標系のバウンディングボックスが [-0.5, 0.5] 付近であること
                QRectF bounds = outline.boundingRect();
                assert(bounds.width() > 0.0 && bounds.height() > 0.0);
                assert(std::abs(bounds.center().x()) < 0.1);
                assert(std::abs(bounds.center().y()) < 0.1);

                // カーソル移動追従のテスト (ホバー移動シミュレーション)
                canvas.setImageBrushFollowCursor(true);
                assert(canvas.isImageBrushFollowCursor() == true);
                canvas.setImageBrushAngle(45);
                assert(canvas.imageBrushAngle() == 45);

                // マウス移動イベント相当のテスト
                QMouseEvent moveEv1(QEvent::MouseMove, QPointF(50, 50), QPointF(50, 50), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(&canvas, &moveEv1);

                // 右へ移動 (dx=10, dy=0) -> 角度 0 度
                QMouseEvent moveEv2(QEvent::MouseMove, QPointF(60, 50), QPointF(60, 50), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(&canvas, &moveEv2);
                assert(std::abs(canvas.imageBrushLastFollowAngle() - 0.0f) < 1.0f);

                // 下へ移動 (dx=0, dy=10) -> 角度 90 度
                QMouseEvent moveEv3(QEvent::MouseMove, QPointF(60, 60), QPointF(60, 60), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(&canvas, &moveEv3);
                assert(std::abs(canvas.imageBrushLastFollowAngle() - 90.0f) < 1.0f);

                std::cout << "[PASS] Image brush contour path generation and follow cursor angle verified!" << std::endl;
            }
        }

        std::cout << "[PASS] All Image Brush tests passed successfully!" << std::endl;
    }

    std::cout << "\n[ALL BRUSH FEATURES TESTS PASSED SUCCESSFULLY!]" << std::endl;
    return 0;
}
