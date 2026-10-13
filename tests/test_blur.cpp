#include <QApplication>
#include <QMouseEvent>
#include <cassert>
#include <iostream>
#include "Document.hpp"
#include "CanvasWidget.hpp"
#include "BlurSettingsWidget.hpp"

using namespace Kaburagi;

class TestCanvasWidget : public CanvasWidget {
public:
    using CanvasWidget::CanvasWidget;

    void simulateStroke(const QPointF& from, const QPointF& to, int steps = 10) {
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
    std::cout << "[TEST] Starting Blur tests..." << std::endl;

    // 1. Test BlurSettingsWidget
    {
        BlurSettingsWidget widget;
        assert(widget.blurSize() == 30.0f);
        assert(widget.minSizeRatio() == 0.0f);
        assert(widget.blurDensity() == 1.0f);
        assert(widget.isPressureSizeEnabled() == true);
        assert(widget.isPressureDensityEnabled() == false);
        assert(widget.colorMix() == 50);
        assert(widget.colorStretch() == 50);

        float receivedSize = 0.0f;
        int receivedMix = 0;
        int receivedStretch = 0;
        QObject::connect(&widget, &BlurSettingsWidget::blurSizeChanged, [&](float s) { receivedSize = s; });
        QObject::connect(&widget, &BlurSettingsWidget::colorMixChanged, [&](int m) { receivedMix = m; });
        QObject::connect(&widget, &BlurSettingsWidget::colorStretchChanged, [&](int st) { receivedStretch = st; });

        widget.setBlurSize(20.0f);
        assert(widget.blurSize() == 20.0f);
        assert(receivedSize == 20.0f);

        widget.setColorMix(80);
        assert(widget.colorMix() == 80);
        assert(receivedMix == 80);

        widget.setColorStretch(60);
        assert(widget.colorStretch() == 60);
        assert(receivedStretch == 60);

        BlurConfig cfg;
        cfg.size = 15.0f;
        cfg.minSizeRatio = 0.2f;
        cfg.density = 0.7f;
        cfg.pressureSize = false;
        cfg.pressureDensity = true;
        cfg.colorMix = 30;
        cfg.colorStretch = 40;
        widget.setConfig(cfg);

        BlurConfig readCfg = widget.config();
        assert(readCfg.size == 15.0f);
        assert(std::abs(readCfg.minSizeRatio - 0.2f) < 0.01f);
        assert(std::abs(readCfg.density - 0.7f) < 0.01f);
        assert(readCfg.pressureSize == false);
        assert(readCfg.pressureDensity == true);
        assert(readCfg.colorMix == 30);
        assert(readCfg.colorStretch == 40);

        std::cout << "[PASS] BlurSettingsWidget properties and getters/setters passed!" << std::endl;
    }

    // 2. Test Blur stroke and color mixing
    {
        Document doc(100, 100, 350.0);
        TestCanvasWidget canvas;
        canvas.setDocument(&doc);

        auto activeL = doc.activeLayer();
        assert(activeL != nullptr);

        // Fill left half [0..49, 0..99] with solid Red (0xFFFF0000)
        // Fill right half [50..99, 0..99] with solid Blue (0xFF0000FF)
        for (int y = 0; y < 100; ++y) {
            for (int x = 0; x < 50; ++x) {
                activeL->tileManager().setPixel(x, y, 0xFFFF0000);
            }
            for (int x = 50; x < 100; ++x) {
                activeL->tileManager().setPixel(x, y, 0xFF0000FF);
            }
        }

        canvas.setCurrentTool(ToolType::Blur);
        canvas.setBlurSize(20.0f);
        canvas.setBlurDensity(1.0f);
        canvas.setBlurColorMix(80);
        canvas.setBlurColorStretch(50);
        canvas.setBlurPressureSizeEnabled(false);
        canvas.setBlurPressureDensityEnabled(false);

        // Apply blur stroke along the boundary (x=50, from y=20 to y=80)
        canvas.simulateStroke(QPointF(50, 20), QPointF(50, 80), 20);

        // Check pixel around boundary: x=50, y=50
        uint32_t blurredPixel = activeL->tileManager().getPixel(50, 50);
        uint32_t a = (blurredPixel >> 24) & 0xFF;
        uint32_t r = (blurredPixel >> 16) & 0xFF;
        uint32_t g = (blurredPixel >> 8) & 0xFF;
        uint32_t b = blurredPixel & 0xFF;

        std::cout << "[INFO] Blurred pixel at (50, 50): A=" << a << ", R=" << r << ", G=" << g << ", B=" << b << std::endl;
        assert(a == 255);
        // Should be mixed! Both R and B should be significantly non-zero
        assert(r > 20 && r < 240);
        assert(b > 20 && b < 240);

        // 3. Test Undo
        assert(doc.undoStack() != nullptr);
        assert(doc.undoStack()->canUndo());
        doc.undoStack()->undo();

        uint32_t undoPixel = activeL->tileManager().getPixel(50, 50);
        std::cout << "[INFO] After Undo at (50, 50): " << std::hex << undoPixel << std::dec << std::endl;
        assert(undoPixel == 0xFF0000FF); // Right half originally Blue

        // Test Redo
        doc.undoStack()->redo();
        uint32_t redoPixel = activeL->tileManager().getPixel(50, 50);
        assert(redoPixel == blurredPixel);

        std::cout << "[PASS] Blur stroke color mixing and Undo/Redo passed!" << std::endl;
    }

    // 4. Test Color Mix = 0 (no changes)
    {
        Document doc(100, 100, 350.0);
        TestCanvasWidget canvas;
        canvas.setDocument(&doc);

        auto activeL = doc.activeLayer();
        for (int y = 0; y < 100; ++y) {
            for (int x = 0; x < 50; ++x) activeL->tileManager().setPixel(x, y, 0xFFFF0000);
            for (int x = 50; x < 100; ++x) activeL->tileManager().setPixel(x, y, 0xFF0000FF);
        }

        canvas.setCurrentTool(ToolType::Blur);
        canvas.setBlurSize(20.0f);
        canvas.setBlurDensity(1.0f);
        canvas.setBlurColorMix(0); // 0% mix -> no change
        canvas.setBlurColorStretch(50);

        canvas.simulateStroke(QPointF(50, 20), QPointF(50, 80), 20);

        uint32_t pixel = activeL->tileManager().getPixel(50, 50);
        assert(pixel == 0xFF0000FF);
        std::cout << "[PASS] Blur with colorMix=0 leaves pixels intact passed!" << std::endl;
    }

    // 5. Test Color Stretch (color dragging effect)
    {
        // Compare color stretch 0 vs 100
        auto runStrokeWithStretch = [](int stretch) -> uint32_t {
            Document doc(120, 100, 350.0);
            TestCanvasWidget canvas;
            canvas.setDocument(&doc);
            auto activeL = doc.activeLayer();

            // Left half [0..49, :] solid Green (0xFF00FF00), Right half [50..119, :] solid White (0xFFFFFFFF)
            for (int y = 0; y < 100; ++y) {
                for (int x = 0; x < 50; ++x) activeL->tileManager().setPixel(x, y, 0xFF00FF00);
                for (int x = 50; x < 120; ++x) activeL->tileManager().setPixel(x, y, 0xFFFFFFFF);
            }

            canvas.setCurrentTool(ToolType::Blur);
            canvas.setBlurSize(24.0f);
            canvas.setBlurDensity(1.0f);
            canvas.setBlurColorMix(80);
            canvas.setBlurColorStretch(stretch);
            canvas.setBlurPressureSizeEnabled(false);
            canvas.setBlurPressureDensityEnabled(false);

            // Drag stroke from Green (x=20) to deep in White (x=90) along y=50
            canvas.simulateStroke(QPointF(20, 50), QPointF(90, 50), 25);

            // Sample pixel deep in right side at (x=70, y=50)
            return activeL->tileManager().getPixel(70, 50);
        };

        uint32_t pixStretch0 = runStrokeWithStretch(0);
        uint32_t pixStretch100 = runStrokeWithStretch(100);

        uint32_t r0 = (pixStretch0 >> 16) & 0xFF;
        uint32_t g0 = (pixStretch0 >> 8) & 0xFF;
        uint32_t b0 = pixStretch0 & 0xFF;

        uint32_t r100 = (pixStretch100 >> 16) & 0xFF;
        uint32_t g100 = (pixStretch100 >> 8) & 0xFF;
        uint32_t b100 = pixStretch100 & 0xFF;

        std::cout << "[INFO] Stretch 0 at (70, 50): R=" << r0 << ", G=" << g0 << ", B=" << b0 << std::endl;
        std::cout << "[INFO] Stretch 100 at (70, 50): R=" << r100 << ", G=" << g100 << ", B=" << b100 << std::endl;

        // With stretch=100, the Green color dragged from left is more strongly preserved into the white side,
        // so R and B (which are 255 in white and 0 in green) will be noticeably lower than with stretch=0.
        assert(r100 < r0);
        assert(b100 < b0);
        std::cout << "[PASS] Color stretch dragging test passed!" << std::endl;
    }

    std::cout << "[ALL PASS] All Blur tests completed successfully!" << std::endl;
    return 0;
}
