#include <QApplication>
#include <cassert>
#include <iostream>
#include "Document.hpp"
#include "CanvasWidget.hpp"
#include "SmudgeSettingsWidget.hpp"
#include "ToolBoxWidget.hpp"

using namespace Kaburagi;

class TestCanvasWidget : public CanvasWidget {
public:
    using CanvasWidget::CanvasWidget;

    void simulateStroke(const QPointF& from, const QPointF& to, int steps = 15) {
        strokeBegin(from, 1.0f);
        for (int i = 1; i <= steps; ++i) {
            float t = static_cast<float>(i) / steps;
            QPointF p = from + (to - from) * t;
            strokeMove(p, 1.0f);
        }
        strokeEnd();
    }
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    std::cout << "[TEST] Starting Smudge (Finger) tests..." << std::endl;

    // 1. Test SmudgeSettingsWidget
    {
        SmudgeSettingsWidget widget;
        assert(widget.smudgeSize() == 30.0f);
        assert(widget.minSizeRatio() == 0.0f);
        assert(widget.smudgeDensity() == 1.0f);
        assert(widget.rate() == 50);
        assert(widget.isPressureSizeEnabled() == true);
        assert(widget.isPressureDensityEnabled() == false);

        float receivedSize = 0.0f;
        int receivedRate = 0;
        QObject::connect(&widget, &SmudgeSettingsWidget::smudgeSizeChanged, [&](float s) { receivedSize = s; });
        QObject::connect(&widget, &SmudgeSettingsWidget::rateChanged, [&](int r) { receivedRate = r; });

        widget.setSmudgeSize(25.0f);
        assert(widget.smudgeSize() == 25.0f);
        assert(receivedSize == 25.0f);

        widget.setRate(80);
        assert(widget.rate() == 80);
        assert(receivedRate == 80);

        SmudgeConfig cfg;
        cfg.size = 18.0f;
        cfg.minSizeRatio = 0.3f;
        cfg.density = 0.8f;
        cfg.rate = 65;
        cfg.pressureSize = false;
        cfg.pressureDensity = true;
        widget.setConfig(cfg);

        SmudgeConfig readCfg = widget.config();
        assert(readCfg.size == 18.0f);
        assert(std::abs(readCfg.minSizeRatio - 0.3f) < 0.01f);
        assert(std::abs(readCfg.density - 0.8f) < 0.01f);
        assert(readCfg.rate == 65);
        assert(readCfg.pressureSize == false);
        assert(readCfg.pressureDensity == true);

        std::cout << "[PASS] SmudgeSettingsWidget properties and getters/setters passed!" << std::endl;
    }

    // 2. Test ToolBoxWidget label for ToolType::Blend
    {
        ToolBoxWidget toolBox;
        // Search child buttons to verify label is "指先"
        bool foundFingerButton = false;
        const auto buttons = toolBox.findChildren<SaiToolButton*>();
        for (auto btn : buttons) {
            if (btn->tool() == ToolType::Blend) {
                assert(btn->toolTip() == "指先");
                foundFingerButton = true;
                break;
            }
        }
        assert(foundFingerButton);
        std::cout << "[PASS] ToolBoxWidget ToolType::Blend label is '指先' passed!" << std::endl;
    }

    // 3. Test Smudge stroke dragging paint from Red area into White area
    {
        Document doc(120, 100, 350.0);
        TestCanvasWidget canvas;
        canvas.setDocument(&doc);

        auto activeL = doc.activeLayer();
        assert(activeL != nullptr);

        // Fill left half [0..49, :] solid Red (0xFFFF0000)
        // Fill right half [50..119, :] solid White (0xFFFFFFFF)
        for (int y = 0; y < 100; ++y) {
            for (int x = 0; x < 50; ++x) activeL->tileManager().setPixel(x, y, 0xFFFF0000);
            for (int x = 50; x < 120; ++x) activeL->tileManager().setPixel(x, y, 0xFFFFFFFF);
        }

        canvas.setCurrentTool(ToolType::Blend);
        canvas.setSmudgeSize(25.0f);
        canvas.setSmudgeDensity(1.0f);
        canvas.setSmudgeRate(80); // 80% smudge rate
        canvas.setSmudgePressureSizeEnabled(false);
        canvas.setSmudgePressureDensityEnabled(false);

        // Drag finger from Red (x=20) to White (x=90) along y=50
        canvas.simulateStroke(QPointF(20, 50), QPointF(90, 50), 25);

        // Check pixel at x=65, y=50 (originally pure White: 0xFFFFFFFF)
        uint32_t smudgedPixel = activeL->tileManager().getPixel(65, 50);
        uint32_t a = (smudgedPixel >> 24) & 0xFF;
        uint32_t r = (smudgedPixel >> 16) & 0xFF;
        uint32_t g = (smudgedPixel >> 8) & 0xFF;
        uint32_t b = smudgedPixel & 0xFF;

        std::cout << "[INFO] Smudged pixel at (65, 50): A=" << a << ", R=" << r << ", G=" << g << ", B=" << b << std::endl;
        assert(a == 255);
        // Red from left should be dragged into the white area, so G and B should be significantly decreased from 255
        assert(r > 200);
        assert(g < 200);
        assert(b < 200);

        // 4. Test Undo / Redo
        assert(doc.undoStack() != nullptr);
        assert(doc.undoStack()->canUndo());
        doc.undoStack()->undo();

        uint32_t undoPixel = activeL->tileManager().getPixel(65, 50);
        assert(undoPixel == 0xFFFFFFFF); // Originally White

        doc.undoStack()->redo();
        uint32_t redoPixel = activeL->tileManager().getPixel(65, 50);
        assert(redoPixel == smudgedPixel);

        std::cout << "[PASS] Smudge stroke dragging and Undo/Redo passed!" << std::endl;
    }

    // 5. Test Rate difference (Rate=10 vs Rate=100)
    {
        auto runSmudgeWithRate = [](int rate) -> uint32_t {
            Document doc(120, 100, 350.0);
            TestCanvasWidget canvas;
            canvas.setDocument(&doc);
            auto activeL = doc.activeLayer();

            for (int y = 0; y < 100; ++y) {
                for (int x = 0; x < 50; ++x) activeL->tileManager().setPixel(x, y, 0xFFFF0000);
                for (int x = 50; x < 120; ++x) activeL->tileManager().setPixel(x, y, 0xFFFFFFFF);
            }

            canvas.setCurrentTool(ToolType::Blend);
            canvas.setSmudgeSize(25.0f);
            canvas.setSmudgeDensity(1.0f);
            canvas.setSmudgeRate(rate);
            canvas.setSmudgePressureSizeEnabled(false);
            canvas.setSmudgePressureDensityEnabled(false);

            canvas.simulateStroke(QPointF(20, 50), QPointF(95, 50), 30);
            return activeL->tileManager().getPixel(80, 50);
        };

        uint32_t pixRate10 = runSmudgeWithRate(10);
        uint32_t pixRate100 = runSmudgeWithRate(100);

        uint32_t g10 = (pixRate10 >> 8) & 0xFF;
        uint32_t g100 = (pixRate100 >> 8) & 0xFF;

        std::cout << "[INFO] Rate 10 at (80, 50): G=" << g10 << std::endl;
        std::cout << "[INFO] Rate 100 at (80, 50): G=" << g100 << std::endl;

        // With Rate=100, the Red paint is dragged much farther into the White area,
        // so Green channel (which is 0 in Red and 255 in White) is much lower in Rate 100 than in Rate 10
        assert(g100 < g10);
        std::cout << "[PASS] Smudge Rate comparison test passed!" << std::endl;
    }

    std::cout << "[ALL PASS] All Smudge (Finger) tests completed successfully!" << std::endl;
    return 0;
}
