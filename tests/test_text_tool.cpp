#include <QApplication>
#include <cassert>
#include <iostream>
#include <QTemporaryFile>
#include <QDir>
#include "core/Document.hpp"
#include "core/TextLayer.hpp"
#include "core/DocumentSerializer.hpp"
#include "gui/MainWindow.hpp"
#include "gui/CanvasWidget.hpp"
#include "gui/TextSettingsWidget.hpp"
#include "gui/ToolBoxWidget.hpp"
#include "gui/LayerPanelWidget.hpp"

using namespace Kaburagi;

int main(int argc, char* argv[]) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::cout << "[TEST] Starting Text Layer & Tool tests..." << std::endl;

    QApplication app(argc, argv);
    std::cout << "[TEST] QApplication initialized" << std::endl;

    // 1. MainWindow のメニューアクション検証
    MainWindow win;
    auto doc = new Document(400, 300, 350.0);
    win.setDocumentForTest(doc);

    std::cout << "[TEST] Testing Layer Menu 'New Text Layer'..." << std::endl;
    QAction* newTextAct = win.newTextLayerAction();
    assert(newTextAct != nullptr);
    assert(newTextAct->text().contains("新規テキストレイヤー"));
    assert(newTextAct->isEnabled());

    size_t initialLayerCount = doc->layers().size();

    // 新規テキストレイヤー実行
    win.onNewTextLayer();
    assert(doc->layers().size() == initialLayerCount + 1);
    auto addedLayer = doc->activeLayer();
    assert(addedLayer != nullptr);
    assert(addedLayer->type() == LayerType::Text);
    std::cout << "[PASS] New Text Layer created via action!" << std::endl;

    // 2. TextSettingsWidget のUIとTextBalloonItemの連携検証 (拡張吹き出しプロパティ)
    std::cout << "[TEST] Testing TextSettingsWidget configuration..." << std::endl;
    TextSettingsWidget textSettings;
    TextBalloonItem srcItem;
    srcItem.font = QFont("Arial", 24, QFont::Bold, false);
    srcItem.horizontalAlign = Qt::AlignHCenter;
    srcItem.verticalAlign = Qt::AlignVCenter;
    srcItem.text = "テスト本文";
    srcItem.shape = BalloonShape::Spike;
    srcItem.spikeCount = 20;
    srcItem.spikeDepth = 25.0f;
    srcItem.spikePhase = 45.0f;
    srcItem.hasTail = true;
    srcItem.tailBaseWidth = 30.0f;
    srcItem.tailCurvature = 50.0f;
    srcItem.textColor = QColor(10, 20, 30);
    srcItem.direction = WritingDirection::Vertical;
    srcItem.text = "吾輩《わがはい》は猫である。名前はまだ無い。";
    srcItem.frameWidth = 4.0f;
    srcItem.frameColor = QColor(255, 0, 0);
    srcItem.fillColor = QColor(255, 255, 200);
    srcItem.hasOutline = true;
    srcItem.outlineWidth = 5.0f;
    srcItem.outlineColor = QColor(0, 0, 255);

    textSettings.setFromItem(srcItem);
    assert(textSettings.currentFont().family() == "Arial");
    assert(textSettings.fontSize() == 24);
    assert(textSettings.isBold() == true);
    assert(textSettings.isItalic() == false);
    assert(textSettings.textColor() == QColor(10, 20, 30));
    assert(textSettings.direction() == WritingDirection::Vertical);
    assert(textSettings.horizontalAlignment() == Qt::AlignHCenter);
    assert(textSettings.verticalAlignment() == Qt::AlignVCenter);
    assert(textSettings.text() == "吾輩《わがはい》は猫である。名前はまだ無い。");
    assert(textSettings.balloonShape() == BalloonShape::Spike);
    assert(textSettings.spikeCount() == 20);
    assert(textSettings.spikeDepth() == 25.0f);
    assert(textSettings.spikePhase() == 45.0f);
    assert(textSettings.hasTail() == true);
    assert(textSettings.tailBaseWidth() == 30.0f);
    assert(textSettings.tailCurvature() == 50.0f);
    assert(textSettings.frameWidth() == 4.0f);
    assert(textSettings.frameColor() == QColor(255, 0, 0));
    assert(textSettings.fillColor() == QColor(255, 255, 200));
    assert(textSettings.hasOutline() == true);
    assert(textSettings.outlineWidth() == 5.0f);
    assert(textSettings.outlineColor() == QColor(0, 0, 255));

    // タブ分離 & setToolTypeの検証
    auto tabWidget = textSettings.findChild<QTabWidget*>();
    assert(tabWidget != nullptr);
    assert(tabWidget->count() == 2);
    assert(tabWidget->tabText(0) == "テキスト");
    assert(tabWidget->tabText(1) == "吹き出し");

    textSettings.setToolType(ToolType::Text);
    assert(tabWidget->currentIndex() == 0);
    textSettings.setToolType(ToolType::TextBalloon);
    assert(tabWidget->currentIndex() == 1);

    // 書字方向切り替えテスト
    textSettings.setDirection(WritingDirection::Horizontal);
    assert(textSettings.direction() == WritingDirection::Horizontal);
    textSettings.setDirection(WritingDirection::Vertical);
    assert(textSettings.direction() == WritingDirection::Vertical);

    std::cout << "[PASS] TextSettingsWidget setFromItem & tab separation work!" << std::endl;

    // applyToItemの検証
    TextBalloonItem targetItem;
    textSettings.applyToItem(targetItem);
    assert(targetItem.font.family() == "Arial");
    assert(targetItem.font.pointSize() == 24);
    assert(targetItem.textColor == QColor(10, 20, 30));
    assert(targetItem.direction == WritingDirection::Vertical);
    assert(targetItem.shape == BalloonShape::Spike);
    assert(targetItem.spikeCount() == 20 || targetItem.spikeCount == 20);
    assert(targetItem.hasOutline == true);
    assert(targetItem.outlineWidth == 5.0f);
    assert(targetItem.tailCurvature == 50.0f);
    std::cout << "[PASS] TextSettingsWidget applies correctly to TextBalloonItem!" << std::endl;

    // 3. TextLayer特殊形状・カーブしっぽ・フチ取りのラスタライズ描画検証 (縦書き + ルビ)
    std::cout << "[TEST] Testing TextLayer rasterize and special shapes (vertical + ruby)..." << std::endl;
    auto textLayer = std::dynamic_pointer_cast<TextLayer>(addedLayer);
    assert(textLayer != nullptr);
    srcItem.rect = QRectF(50, 50, 300, 150);
    srcItem.tailTip = QPointF(380, 220);
    int idx = textLayer->addItem(srcItem);
    assert(idx == 0);
    textLayer->rasterizeAll();
    int hit = textLayer->findItemAt(QPointF(100, 100));
    assert(hit == 0);

    // モコモコ (Cloud) + 横書きルビ [世界](ワールド) も追加して描画テスト
    TextBalloonItem cloudItem = srcItem;
    cloudItem.shape = BalloonShape::Cloud;
    cloudItem.direction = WritingDirection::Horizontal;
    cloudItem.text = "ここは[世界](ワールド)の｜果て《はて》だ！";
    cloudItem.rect = QRectF(50, 200, 200, 100);
    textLayer->addItem(cloudItem);
    textLayer->rasterizeAll();
    assert(textLayer->items().size() == 2);
    std::cout << "[PASS] TextLayer item hit-test and rasterize succeed for Vertical and Horizontal Ruby!" << std::endl;

    // 4. DocumentSerializer 保存 & 読込テスト（アライメント、特殊形状、カーブしっぽ、フチ取り）
    std::cout << "[TEST] Testing DocumentSerializer serialization..." << std::endl;
    QString tempPath = QDir::tempPath() + "/test_text_tool_file.kab";
    QFile::remove(tempPath);
    std::cout << "[TEST] tempPath: " << tempPath.toStdString() << std::endl;

    QString saveErr;
    bool saveOk = DocumentSerializer::saveToFile(doc, tempPath, &saveErr);
    std::cout << "[TEST] saveOk: " << saveOk << " err: " << saveErr.toStdString() << std::endl;
    assert(saveOk);

    QString loadErr;
    Document* loadedDoc = DocumentSerializer::loadFromFile(tempPath, nullptr, &loadErr);
    std::cout << "[TEST] loadedDoc: " << (loadedDoc != nullptr) << " err: " << loadErr.toStdString() << std::endl;
    assert(loadedDoc != nullptr);
    std::cout << "[TEST] loaded layers: " << loadedDoc->layers().size() << " expected: " << doc->layers().size() << std::endl;
    assert(loadedDoc->layers().size() == doc->layers().size());

    auto loadedTextLayer = std::dynamic_pointer_cast<TextLayer>(loadedDoc->layers().back());
    assert(loadedTextLayer != nullptr);
    std::cout << "[TEST] loaded items: " << loadedTextLayer->items().size() << std::endl;
    assert(loadedTextLayer->items().size() == 2);

    const auto& loadedSpike = loadedTextLayer->items()[0];
    assert(loadedSpike.shape == BalloonShape::Spike);
    assert(loadedSpike.spikeCount == 20);
    assert(loadedSpike.spikeDepth == 25.0f);
    assert(loadedSpike.spikePhase == 45.0f);
    assert(loadedSpike.hasTail == true);
    assert(loadedSpike.tailBaseWidth == 30.0f);
    assert(loadedSpike.tailCurvature == 50.0f);
    assert(loadedSpike.hasOutline == true);
    assert(loadedSpike.outlineWidth == 5.0f);
    assert(loadedSpike.outlineColor == QColor(0, 0, 255));
    assert(loadedSpike.direction == WritingDirection::Vertical);

    const auto& loadedCloud = loadedTextLayer->items()[1];
    assert(loadedCloud.shape == BalloonShape::Cloud);
    assert(loadedCloud.direction == WritingDirection::Horizontal);

    delete loadedDoc;
    std::cout << "[PASS] DocumentSerializer correctly loaded speech balloon properties!" << std::endl;

    std::cout << "[ALL TEXT & BALLOON TESTS PASSED SUCCESSFULLY!]" << std::endl;
    return 0;
}
