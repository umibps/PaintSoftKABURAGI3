#include <QApplication>
#include <cassert>
#include <iostream>
#include <QTemporaryFile>
#include <QDir>
#include <QPainter>
#include <QTextEdit>
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
    srcItem.spikeRandomness = 45;
    srcItem.randomSeed = 88888;
    srcItem.customVertices = { QPointF(1, 2), QPointF(3, 4) };

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
    assert(textSettings.spikeRandomness() == 45);
    assert(textSettings.randomSeed() == 88888);
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

    // 入力支援ボタンの検証 (太字、斜体、サイズタグの挿入)
    auto textEdit = textSettings.findChild<QTextEdit*>();
    assert(textEdit != nullptr);
    textEdit->setPlainText("Hello World");
    QTextCursor cur = textEdit->textCursor();
    cur.setPosition(6);
    cur.setPosition(11, QTextCursor::KeepAnchor); // "World" を選択
    textEdit->setTextCursor(cur);

    auto btnBold = textSettings.findChild<QPushButton*>("", Qt::FindDirectChildrenOnly);
    // ボタンの動作を直接検証
    auto inlineButtons = textSettings.findChildren<QPushButton*>();
    QPushButton* btnInlineBold = nullptr;
    QPushButton* btnInlineItalic = nullptr;
    QPushButton* btnInlineSize = nullptr;
    QPushButton* btnShowTags = nullptr;
    QPushButton* btnHideTags = nullptr;
    for (auto btn : inlineButtons) {
        if (btn->text() == "B" && btn->toolTip().contains("<b>")) btnInlineBold = btn;
        else if (btn->text() == "I" && btn->toolTip().contains("<i>")) btnInlineItalic = btn;
        else if (btn->text() == "サイズ") btnInlineSize = btn;
        else if (btn->text() == "タグあり表示") btnShowTags = btn;
        else if (btn->text() == "タグ無し表示") btnHideTags = btn;
    }
    assert(btnInlineBold != nullptr);
    assert(btnInlineItalic != nullptr);
    assert(btnInlineSize != nullptr);
    assert(btnShowTags != nullptr);
    assert(btnHideTags != nullptr);

    btnInlineBold->click();
    assert(textEdit->toPlainText() == "Hello <b>World</b>");

    // "Hello" を斜体
    cur.setPosition(0);
    cur.setPosition(5, QTextCursor::KeepAnchor);
    textEdit->setTextCursor(cur);
    btnInlineItalic->click();
    assert(textEdit->toPlainText() == "<i>Hello</i> <b>World</b>");

    // タグあり / タグなし 表示切り替えの検証
    assert(textSettings.isHideTags() == false);
    btnHideTags->click();
    assert(textSettings.isHideTags() == true);
    assert(textEdit->isReadOnly() == true);
    // text() は生テキストを保持していること
    assert(textSettings.text() == "<i>Hello</i> <b>World</b>");

    // 再びタグあり表示に切り替え
    btnShowTags->click();
    assert(textSettings.isHideTags() == false);
    assert(textEdit->isReadOnly() == false);
    assert(textEdit->toPlainText() == "<i>Hello</i> <b>World</b>");

    // タグ無し表示モードでのインライン書式適用（モード遷移せず、選択されたテキストにタグが追加されること）
    textSettings.setText("テストの文章です");
    btnHideTags->click();
    assert(textSettings.isHideTags() == true);
    // "テスト" を選択 (0〜3文字目)
    cur = textEdit->textCursor();
    cur.setPosition(0);
    cur.setPosition(3, QTextCursor::KeepAnchor);
    textEdit->setTextCursor(cur);
    // サイズボタンをクリック (24pt)
    btnInlineSize->click();
    // モードが遷移せずタグ無しのままであること！
    assert(textSettings.isHideTags() == true);
    // 裏の生テキストに正しくタグが追加されていること！
    assert(textSettings.text() == "<size=24>テスト</size>の文章です");

    // 表示倍率（previewZoom）の検証
    assert(textSettings.previewZoom() == 100);
    textSettings.setPreviewZoom(50);
    assert(textSettings.previewZoom() == 50);

    // インラインサイズSpinBoxの最小幅検証 (pt見切れ防止)
    auto spinBoxes = textSettings.findChildren<QSpinBox*>();
    QSpinBox* inlineSizeSpin = nullptr;
    for (auto sb : spinBoxes) {
        if (sb->suffix() == " pt" && sb->maximum() == 300 && sb->parent() != nullptr) {
            if (sb->toolTip().contains("選択部分")) {
                inlineSizeSpin = sb;
                break;
            }
        }
    }
    assert(inlineSizeSpin != nullptr);
    assert(inlineSizeSpin->minimumWidth() >= 90);

    // テキスト編集エリアでキー入力された際にタグあり表示へ自動遷移して編集可能になることの検証
    assert(textSettings.isHideTags() == true);
    QKeyEvent keyEv(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");
    QApplication::sendEvent(textEdit, &keyEv);
    assert(textSettings.isHideTags() == false);
    assert(textEdit->isReadOnly() == false);

    // focusTextEditの検証
    textSettings.setHideTags(true);
    assert(textSettings.isHideTags() == true);
    textSettings.focusTextEdit();
    assert(textSettings.isHideTags() == false);
    assert(textEdit->isReadOnly() == false);

    // カタカナへのルビパースの検証
    auto katakanaTokens = parseFormattedTokens("テスト《てすと》の文章です", 16.0);
    assert(!katakanaTokens.empty());
    assert(katakanaTokens[0].baseText == "テスト");
    assert(katakanaTokens[0].hasRuby == true);
    assert(katakanaTokens[0].rubyText == "てすと");

    std::cout << "[PASS] TextSettingsWidget inline formatting & tag view mode work!" << std::endl;

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

    // ランダム再配置ボタンの動作検証
    QPushButton* randBtn = nullptr;
    for (auto* btn : textSettings.findChildren<QPushButton*>()) {
        if (btn->text().contains("ランダム再配置")) {
            randBtn = btn;
            break;
        }
    }
    assert(randBtn != nullptr);
    uint32_t seedBefore = textSettings.randomSeed();
    randBtn->click();
    assert(textSettings.randomSeed() != seedBefore);
    assert(textSettings.spikeRandomness() > 0);

    std::cout << "[PASS] TextSettingsWidget applies correctly to TextBalloonItem!" << std::endl;

    // 3. TextLayer特殊形状・カーブしっぽ・フチ取りのラスタライズ描画検証 (縦書き + ルビ + インライン書式)
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

    // モコモコ (Cloud) + 横書きルビ + インライン書式 (太字・文字色・サイズ変更) も追加して描画テスト
    TextBalloonItem cloudItem = srcItem;
    cloudItem.shape = BalloonShape::Cloud;
    cloudItem.direction = WritingDirection::Horizontal;
    cloudItem.text = "ここは<b><color=red>[世界](ワールド)</color></b>の<size=+6>｜果て《はて》</size>だ！";
    cloudItem.rect = QRectF(50, 200, 200, 100);
    textLayer->addItem(cloudItem);
    textLayer->rasterizeAll();
    // カタカナ縦書きの描画確認（「テスト」の縦一列揃い）
    TextBalloonItem testKatakanaItem;
    testKatakanaItem.shape = BalloonShape::Spike;
    testKatakanaItem.direction = WritingDirection::Vertical;
    testKatakanaItem.text = "テスト";
    testKatakanaItem.horizontalAlign = Qt::AlignHCenter;
    testKatakanaItem.verticalAlign = Qt::AlignVCenter;
    testKatakanaItem.rect = QRectF(20, 20, 260, 260);
    testKatakanaItem.frameColor = Qt::black;
    testKatakanaItem.fillColor = QColor(240, 240, 240);
    testKatakanaItem.textColor = Qt::blue;
    testKatakanaItem.font = QFont("Yu Gothic", 24);
    
    // TextLayer 経由で画像化
    auto katakanaLayer = std::make_shared<TextLayer>("katakana", 300, 300);
    katakanaLayer->addItem(testKatakanaItem);
    katakanaLayer->rasterizeAll();
    QImage renderedKatakana(300, 300, QImage::Format_ARGB32_Premultiplied);
    renderedKatakana.fill(Qt::transparent);
    for (int ty = 0; ty < katakanaLayer->tileManager().tilesY(); ++ty) {
        for (int tx = 0; tx < katakanaLayer->tileManager().tilesX(); ++tx) {
            const Tile* tile = katakanaLayer->tileManager().getTile(tx, ty);
            if (!tile) continue;
            const uint32_t* tileBuf = reinterpret_cast<const uint32_t*>(tile->readBuffer());
            for (int ly = 0; ly < 128; ++ly) {
                int gy = ty * 128 + ly;
                if (gy >= 300) break;
                uint32_t* line = reinterpret_cast<uint32_t*>(renderedKatakana.scanLine(gy));
                for (int lx = 0; lx < 128; ++lx) {
                    int gx = tx * 128 + lx;
                    if (gx >= 300) break;
                    line[gx] = tileBuf[ly * 128 + lx];
                }
            }
        }
    }
    // 各文字の中心Xが揃っていることを検証（テ・ス・トのX中心のズレが極小であること）
    auto getCharCenterX = [&](int minY, int maxY) -> double {
        int minX = 9999, maxX = -1;
        for (int y = minY; y <= maxY; ++y) {
            const uint32_t* line = reinterpret_cast<const uint32_t*>(renderedKatakana.constScanLine(y));
            for (int x = 0; x < 300; ++x) {
                // 青色文字ピクセル (alpha > 100, blue > 150)
                QColor c = QColor::fromRgba(line[x]);
                if (c.alpha() > 100 && c.blue() > 150 && c.red() < 100) {
                    minX = std::min(minX, x);
                    maxX = std::max(maxX, x);
                }
            }
        }
        return (minX <= maxX) ? (minX + maxX) * 0.5 : -1.0;
    };

    double centerX1 = getCharCenterX(70, 115);  // 「テ」
    double centerX2 = getCharCenterX(120, 165); // 「ス」
    double centerX3 = getCharCenterX(170, 215); // 「ト」
    assert(centerX1 > 0 && centerX2 > 0 && centerX3 > 0);
    // 3文字のX中心が2px以内の精度で一直線に揃っていることを確認
    assert(std::abs(centerX1 - centerX2) <= 2.5);
    assert(std::abs(centerX2 - centerX3) <= 2.5);
    std::cout << "[PASS] Vertical katakana characters aligned straight: X centers = "
              << centerX1 << ", " << centerX2 << ", " << centerX3 << std::endl;

    assert(textLayer->items().size() == 2);
    std::cout << "[PASS] TextLayer item hit-test and rasterize succeed for Vertical and Horizontal Ruby!" << std::endl;

    // MainWindow のキャンバスと設定ウィジェットのランダム再配置連動テスト
    auto winTextSettings = win.findChild<TextSettingsWidget*>();
    assert(winTextSettings != nullptr);
    QPushButton* winRandBtn = nullptr;
    for (auto* btn : winTextSettings->findChildren<QPushButton*>()) {
        if (btn->text().contains("ランダム再配置")) {
            winRandBtn = btn;
            break;
        }
    }
    assert(winRandBtn != nullptr);
    win.canvasWidget()->setSelectedBalloonIndex(0);
    uint32_t oldDocSeed = textLayer->items()[0].randomSeed;
    winRandBtn->click();
    assert(textLayer->items()[0].randomSeed != oldDocSeed);
    std::cout << "[PASS] Randomize button successfully updates balloon on canvas!" << std::endl;

    // 3-2. テキスト・吹き出しの頂点、辺、しっぽのハンドル判定テスト
    std::cout << "[TEST] Testing balloon handles (vertices, edges, body, tail)..." << std::endl;
    CanvasWidget canvas;
    TextBalloonItem testHandleItem;
    testHandleItem.rect = QRectF(100, 100, 200, 100);
    testHandleItem.hasTail = true;
    testHandleItem.tailTip = QPointF(50, 50);

    // 4隅の頂点 (Vertices)
    assert(canvas.hitTestBalloonHandles(testHandleItem, QPointF(100, 100), 1.0) == CanvasWidget::BalloonHandle::TopLeft);
    assert(canvas.hitTestBalloonHandles(testHandleItem, QPointF(300, 100), 1.0) == CanvasWidget::BalloonHandle::TopRight);
    assert(canvas.hitTestBalloonHandles(testHandleItem, QPointF(100, 200), 1.0) == CanvasWidget::BalloonHandle::BottomLeft);
    assert(canvas.hitTestBalloonHandles(testHandleItem, QPointF(300, 200), 1.0) == CanvasWidget::BalloonHandle::BottomRight);

    // 4辺の中央 (Edges)
    assert(canvas.hitTestBalloonHandles(testHandleItem, QPointF(200, 100), 1.0) == CanvasWidget::BalloonHandle::Top);
    assert(canvas.hitTestBalloonHandles(testHandleItem, QPointF(200, 200), 1.0) == CanvasWidget::BalloonHandle::Bottom);
    assert(canvas.hitTestBalloonHandles(testHandleItem, QPointF(100, 150), 1.0) == CanvasWidget::BalloonHandle::Left);
    assert(canvas.hitTestBalloonHandles(testHandleItem, QPointF(300, 150), 1.0) == CanvasWidget::BalloonHandle::Right);

    // しっぽ先端 (Ellipse吹き出し)
    testHandleItem.shape = BalloonShape::Ellipse;
    assert(canvas.hitTestBalloonHandles(testHandleItem, QPointF(50, 50), 1.0) == CanvasWidget::BalloonHandle::TailTip);

    // 枠なし (文字のみ) の場合は、しっぽハンドルが無効化され表示・ヒットしないこと！
    testHandleItem.shape = BalloonShape::None;
    assert(canvas.hitTestBalloonHandles(testHandleItem, QPointF(50, 50), 1.0) == CanvasWidget::BalloonHandle::None);

    // 内部 (Body)
    assert(canvas.hitTestBalloonHandles(testHandleItem, QPointF(200, 150), 1.0) == CanvasWidget::BalloonHandle::Body);

    // 範囲外 (None)
    assert(canvas.hitTestBalloonHandles(testHandleItem, QPointF(10, 10), 1.0) == CanvasWidget::BalloonHandle::None);

    // 特殊形状 (Spike / Cloud) 頂点ハンドル判定
    TextBalloonItem spikeTestItem;
    spikeTestItem.shape = BalloonShape::Spike;
    spikeTestItem.rect = QRectF(100, 100, 200, 200);
    spikeTestItem.spikeCount = 8;
    spikeTestItem.spikeDepth = 20.0f;
    spikeTestItem.spikeRandomness = 0;
    auto spikeVerts = getBalloonSpikeVertices(spikeTestItem);
    assert(spikeVerts.size() == 8);
    // 頂点直上ヒットテスト
    assert(canvas.hitTestBalloonHandles(spikeTestItem, spikeVerts[0], 1.0) == CanvasWidget::BalloonHandle::SpikeVertex);

    // ランダム凸凹度合いの変更テスト
    spikeTestItem.spikeRandomness = 60;
    auto spikeRandVerts = getBalloonSpikeVertices(spikeTestItem);
    assert(spikeRandVerts.size() == 8);
    bool diffFound = false;
    for (size_t i = 0; i < 8; ++i) {
        if (std::abs(spikeRandVerts[i].x() - spikeVerts[i].x()) > 0.01 || std::abs(spikeRandVerts[i].y() - spikeVerts[i].y()) > 0.01) {
            diffFound = true;
            break;
        }
    }
    assert(diffFound);

    // ユーザーによるカスタム頂点ドラッグ位置の反映テスト
    spikeTestItem.customVertices = spikeRandVerts;
    spikeTestItem.customVertices[0] = QPointF(500.0, 500.0);
    auto spikeCustomVerts = getBalloonSpikeVertices(spikeTestItem);
    assert(spikeCustomVerts[0] == QPointF(500.0, 500.0));
    assert(canvas.hitTestBalloonHandles(spikeTestItem, QPointF(500.0, 500.0), 1.0) == CanvasWidget::BalloonHandle::SpikeVertex);

    std::cout << "[PASS] Balloon handles (vertices, edges, body, tail, spike vertices) hit-testing succeed!" << std::endl;

    // 4. DocumentSerializer 保存 & 読込テスト（アライメント、特殊形状、カーブしっぽ、フチ取り、ランダム凸凹、カスタム頂点）
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
    assert(loadedSpike.spikeRandomness == 45);
    assert(loadedSpike.randomSeed == 88888);
    assert(loadedSpike.customVertices.size() == 2);
    assert(loadedSpike.customVertices[0] == QPointF(1, 2));
    assert(loadedSpike.customVertices[1] == QPointF(3, 4));
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
