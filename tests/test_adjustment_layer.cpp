#include <cassert>
#include <iostream>
#include <QApplication>
#include <QTemporaryDir>
#include "Document.hpp"
#include "AdjustmentLayer.hpp"
#include "ToneCurveWidget.hpp"
#include "LevelsWidget.hpp"
#include "AdjustmentSettingsWidget.hpp"
#include "DocumentSerializer.hpp"

using namespace Kaburagi;

void testBrightnessContrast() {
    std::cout << "[Test] BrightnessContrast..." << std::endl;
    auto doc = std::make_unique<Document>(64, 64);
    auto baseLayer = doc->addRasterLayer("Base");

    // Fill with mid-gray (128, 128, 128, 255)
    uint32_t grayPx = 0xFF808080;
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            baseLayer->tileManager().setPixel(x, y, grayPx);
        }
    }

    auto adj = doc->addAdjustmentLayer(AdjustmentType::BrightnessContrast, "BrightAdj");
    adj->setBrightnessContrast(0.2f, 0.0f); // Brightness +20%
    doc->composite();

    uint32_t resPx = doc->compositeBuffer().getPixel(10, 10);
    uint8_t r = (resPx >> 16) & 0xFF;
    assert(r > 128); // Should be brighter than 128
    std::cout << "  Passed. (r = " << (int)r << ")" << std::endl;
}

void testHueSaturation() {
    std::cout << "[Test] HueSaturation..." << std::endl;
    auto doc = std::make_unique<Document>(64, 64);
    auto baseLayer = doc->addRasterLayer("Base");

    // Fill with pure Red (255, 0, 0, 255)
    uint32_t redPx = 0xFFFF0000;
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            baseLayer->tileManager().setPixel(x, y, redPx);
        }
    }

    auto adj = doc->addAdjustmentLayer(AdjustmentType::HueSaturation, "HueAdj");
    adj->setHueSaturation(120.0f, 1.0f, 0.0f); // Rotate Red (+120 deg) -> Green
    doc->composite();

    uint32_t resPx = doc->compositeBuffer().getPixel(10, 10);
    uint8_t r = (resPx >> 16) & 0xFF;
    uint8_t g = (resPx >> 8) & 0xFF;
    uint8_t b = resPx & 0xFF;
    assert(g > 200 && r < 50 && b < 50); // Should be green!
    std::cout << "  Passed. (r=" << (int)r << ", g=" << (int)g << ", b=" << (int)b << ")" << std::endl;
}

void testToneCurve() {
    std::cout << "[Test] ToneCurve LUT & rendering..." << std::endl;
    // 1. Check default LUT (identity)
    std::vector<QPointF> defaultPts = { QPointF(0, 0), QPointF(255, 255) };
    auto lutId = AdjustmentLayer::calculateCurveLUT(defaultPts, CurveInterpolationMode::Spline);
    assert(lutId[0] == 0 && lutId[128] == 128 && lutId[255] == 255);

    // 2. Custom curve (inverting: (0, 255) -> (255, 0))
    std::vector<QPointF> invertPts = { QPointF(0, 255), QPointF(255, 0) };
    auto lutInv = AdjustmentLayer::calculateCurveLUT(invertPts, CurveInterpolationMode::Linear);
    assert(lutInv[0] == 255 && lutInv[255] == 0);

    // 3. Document application
    auto doc = std::make_unique<Document>(64, 64);
    auto baseLayer = doc->addRasterLayer("Base");
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            baseLayer->tileManager().setPixel(x, y, 0xFF4080C0);
        }
    }

    auto adj = doc->addAdjustmentLayer(AdjustmentType::ToneCurve, "CurveAdj");
    adj->setCurvePoints(AdjustmentChannel::RGB, invertPts);
    doc->composite();

    uint32_t resPx = doc->compositeBuffer().getPixel(10, 10);
    uint8_t r = (resPx >> 16) & 0xFF;
    // Original was 0x40 = 64, inverted should be 255 - 64 = 191
    assert(std::abs(static_cast<int>(r) - 191) <= 1);
    std::cout << "  Passed. (inverted r = " << (int)r << ")" << std::endl;
}

void testLevels() {
    std::cout << "[Test] Levels LUT & rendering..." << std::endl;
    LevelParams lp;
    lp.inShadow = 50;
    lp.inHighlight = 200;
    lp.inGamma = 1.0f;
    lp.outShadow = 0;
    lp.outHighlight = 255;

    auto lut = AdjustmentLayer::calculateLevelLUT(lp);
    assert(lut[0] == 0);
    assert(lut[50] == 0);
    assert(lut[200] == 255);
    assert(lut[255] == 255);

    auto doc = std::make_unique<Document>(64, 64);
    auto baseLayer = doc->addRasterLayer("Base");
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            baseLayer->tileManager().setPixel(x, y, 0xFF323232); // 50 in RGB
        }
    }

    auto adj = doc->addAdjustmentLayer(AdjustmentType::Levels, "LevelAdj");
    adj->setLevelParams(AdjustmentChannel::RGB, lp);
    doc->composite();

    uint32_t resPx = doc->compositeBuffer().getPixel(10, 10);
    uint8_t r = (resPx >> 16) & 0xFF;
    assert(r == 0);
    std::cout << "  Passed. (levels shadow clamped to " << (int)r << ")" << std::endl;
}

void testHistogramComputation() {
    std::cout << "[Test] computeHistogram..." << std::endl;
    TileManager tm(64, 64);
    // 100 pixels of value 50
    for (int i = 0; i < 100; ++i) {
        tm.setPixel(i % 64, i / 64, 0xFF323232);
    }

    std::array<int, 256> histR, histG, histB, histLum;
    AdjustmentLayer::computeHistogram(tm, QRect(0, 0, 64, 64), histR, histG, histB, histLum);
    assert(histR[50] == 100);
    assert(histG[50] == 100);
    assert(histB[50] == 100);
    assert(histLum[50] == 100);
    std::cout << "  Passed." << std::endl;
}

void testSettingsWidgetBinding() {
    std::cout << "[Test] AdjustmentSettingsWidget binding..." << std::endl;
    auto doc = std::make_unique<Document>(64, 64);
    auto adj = doc->addAdjustmentLayer(AdjustmentType::ToneCurve, "Adj");

    AdjustmentSettingsWidget widget;
    widget.setAdjustmentLayer(adj.get());
    assert(widget.adjustmentType() == AdjustmentType::ToneCurve);

    bool signaled = false;
    QObject::connect(&widget, &AdjustmentSettingsWidget::parametersChanged, [&signaled]() {
        signaled = true;
    });

    widget.setAdjustmentType(AdjustmentType::Levels);
    assert(adj->adjustmentType() == AdjustmentType::Levels);
    assert(signaled);
    std::cout << "  Passed." << std::endl;
}

void testSerialization() {
    std::cout << "[Test] DocumentSerializer with AdjustmentLayer..." << std::endl;
    QTemporaryDir tempDir;
    QString filePath = tempDir.filePath("test_adj.kab");

    {
        auto doc = std::make_unique<Document>(64, 64);
        doc->addRasterLayer("Base");
        auto adj = doc->addAdjustmentLayer(AdjustmentType::ToneCurve, "CurveLayer");
        std::vector<QPointF> pts = { QPointF(0, 10), QPointF(128, 140), QPointF(255, 240) };
        adj->setCurvePoints(AdjustmentChannel::Red, pts);
        adj->setCurveInterpolationMode(CurveInterpolationMode::Linear);

        LevelParams lp;
        lp.inShadow = 20;
        lp.inHighlight = 230;
        adj->setLevelParams(AdjustmentChannel::Green, lp);

        bool saved = DocumentSerializer::saveToFile(doc.get(), filePath);
        assert(saved);
    }

    {
        auto doc = std::unique_ptr<Document>(DocumentSerializer::loadFromFile(filePath));
        assert(doc != nullptr);
        assert(doc->layers().size() == 2);

        auto adj = std::dynamic_pointer_cast<AdjustmentLayer>(doc->getLayer(1));
        assert(adj != nullptr);
        assert(adj->adjustmentType() == AdjustmentType::ToneCurve);
        assert(adj->curveInterpolationMode() == CurveInterpolationMode::Linear);

        const auto& loadedPts = adj->curvePoints(AdjustmentChannel::Red);
        assert(loadedPts.size() == 3);
        assert(loadedPts[1].x() == 128 && loadedPts[1].y() == 140);

        const auto& loadedLp = adj->levelParams(AdjustmentChannel::Green);
        assert(loadedLp.inShadow == 20 && loadedLp.inHighlight == 230);
    }

    std::cout << "  Passed." << std::endl;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    testBrightnessContrast();
    testHueSaturation();
    testToneCurve();
    testLevels();
    testHistogramComputation();
    testSettingsWidgetBinding();
    testSerialization();

    std::cout << "All adjustment layer tests passed successfully!" << std::endl;
    return 0;
}
