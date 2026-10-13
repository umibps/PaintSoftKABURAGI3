#include <QApplication>
#include <QTemporaryFile>
#include <cassert>
#include <iostream>
#include "Document.hpp"
#include "DocumentSerializer.hpp"
#include "VectorLayer.hpp"
#include "TextLayer.hpp"
#include "AdjustmentLayer.hpp"
#include "GroupLayer.hpp"

using namespace Kaburagi;

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    std::cout << "=== Running test_document_io ===" << std::endl;

    // 1. テスト用ドキュメント作成
    auto doc = new Document(400, 300, 300.0, true);
    assert(doc->width() == 400);
    assert(doc->height() == 300);
    assert(doc->layers().size() == 2); // 背景, レイヤー 1

    doc->setPrimaryBackgroundColor(QColor(250, 240, 230));
    doc->setSecondaryBackgroundColor(QColor(100, 150, 200));
    doc->switchBackgroundColor(); // Switch to secondary
    assert(doc->isSecondaryBgColorActive() == true);
    assert(doc->backgroundLayer()->tileManager().getPixel(0, 0) == 0xFF6496C8);

    // ラスターレイヤー (レイヤー 1) に幾何学的なピクセルを描画
    auto rLayer = doc->getLayer(1);
    assert(rLayer != nullptr);
    rLayer->tileManager().setPixel(50, 50, 0xFFFF0000);
    rLayer->tileManager().setPixel(150, 150, 0xFF00FF00);
    rLayer->tileManager().setPixel(250, 250, 0xFF0000FF);

    // ベクターレイヤーを追加し、楕円ストロークを描画
    auto vLayer = doc->addVectorLayer("ベクター 1");
    assert(vLayer != nullptr);
    VectorStroke ellipseStroke = VectorStroke::createEllipse(
        QPointF(100.0f, 100.0f), 40.0f, 30.0f, 3.0f, 0xFF000000, 1.0f, 100, 0, false
    );
    vLayer->addStroke(ellipseStroke);
    assert(vLayer->strokes().size() == 1);

    // テキストレイヤーを追加
    auto tLayer = doc->addTextLayer("テキスト 1");
    assert(tLayer != nullptr);
    TextBalloonItem item;
    item.id = "balloon_1";
    item.rect = QRectF(20, 20, 100, 50);
    item.text = "Hello Kaburagi!";
    item.textColor = Qt::blue;
    tLayer->addItem(item);
    assert(tLayer->items().size() == 1);

    // 色調補正レイヤーを追加
    auto aLayer = doc->addAdjustmentLayer(AdjustmentType::BrightnessContrast, "補正 1");
    assert(aLayer != nullptr);
    aLayer->setBrightnessContrast(0.2f, -0.1f);

    // グループレイヤーを追加し、ベクターレイヤーをその子にする
    auto gLayer = doc->addGroupLayer("グループ 1");
    assert(gLayer != nullptr);
    gLayer->addChild(vLayer);
    assert(vLayer->parentGroup() == gLayer);

    // 選択範囲を設定
    QPainterPath sel;
    sel.addRect(10, 10, 80, 80);
    doc->setSelection(sel);
    assert(doc->hasSelection());

    // 2. 一時ファイルに保存 (.kab)
    QTemporaryFile tempFile;
    tempFile.setAutoRemove(false);
    assert(tempFile.open());
    QString tempPath = tempFile.fileName() + ".kab";
    tempFile.close();

    QString errorMsg;
    bool saveOk = DocumentSerializer::saveToFile(doc, tempPath, &errorMsg);
    if (!saveOk) {
        std::cerr << "Save failed: " << errorMsg.toStdString() << std::endl;
        assert(false);
    }
    std::cout << "Successfully saved .kab to: " << tempPath.toStdString() << std::endl;

    // ファイルサイズ検証 (非空タイル圧縮により、数KB〜数十KBに収まるはず)
    QFile savedFile(tempPath);
    assert(savedFile.open(QIODevice::ReadOnly));
    qint64 fileSize = savedFile.size();
    savedFile.close();
    std::cout << "File size: " << fileSize << " bytes" << std::endl;
    assert(fileSize > 0 && fileSize < 500000);

    // 3. 埋め込みサムネイルの高速抽出テスト
    QImage thumb = DocumentSerializer::extractThumbnail(tempPath, &errorMsg);
    assert(!thumb.isNull());
    assert(thumb.width() > 0 && thumb.height() > 0);
    std::cout << "Thumbnail extracted successfully: " << thumb.width() << "x" << thumb.height() << std::endl;

    // 4. ドキュメントの読み込み
    Document* loadedDoc = DocumentSerializer::loadFromFile(tempPath, nullptr, &errorMsg);
    if (!loadedDoc) {
        std::cerr << "Load failed: " << errorMsg.toStdString() << std::endl;
        assert(false);
    }
    std::cout << "Successfully loaded .kab document!" << std::endl;

    // 5. 検証: 基本属性
    assert(loadedDoc->width() == 400);
    assert(loadedDoc->height() == 300);
    assert(loadedDoc->dpi() == 300.0);
    assert(loadedDoc->primaryBackgroundColor() == QColor(250, 240, 230));
    assert(loadedDoc->secondaryBackgroundColor() == QColor(100, 150, 200));
    assert(loadedDoc->isSecondaryBgColorActive() == true);
    assert(loadedDoc->backgroundLayer()->tileManager().getPixel(0, 0) == 0xFF6496C8);
    loadedDoc->switchBackgroundColor();
    assert(loadedDoc->isSecondaryBgColorActive() == false);
    assert(loadedDoc->backgroundLayer()->tileManager().getPixel(0, 0) == 0xFFFAF0E6);
    assert(loadedDoc->hasSelection());
    assert(loadedDoc->selection().boundingRect() == QRectF(10, 10, 80, 80));

    // レイヤー数検証: 背景, レイヤー 1, ベクター 1, テキスト 1, 補正 1, グループ 1 の計6個
    assert(loadedDoc->layers().size() == 6);

    // ラスターレイヤーのピクセル値検証
    auto loadedRLayer = loadedDoc->getLayer(1);
    assert(loadedRLayer != nullptr);
    assert(loadedRLayer->tileManager().getPixel(50, 50) == 0xFFFF0000);
    assert(loadedRLayer->tileManager().getPixel(150, 150) == 0xFF00FF00);
    assert(loadedRLayer->tileManager().getPixel(250, 250) == 0xFF0000FF);
    assert(loadedRLayer->tileManager().getPixel(0, 0) == 0x00000000); // 未描画部分は透過

    // ベクターレイヤー検証
    auto loadedVLayer = std::dynamic_pointer_cast<VectorLayer>(loadedDoc->getLayer(2));
    assert(loadedVLayer != nullptr);
    assert(loadedVLayer->strokes().size() == 1);
    assert(loadedVLayer->strokes()[0].points.size() == ellipseStroke.points.size());
    assert(loadedVLayer->strokes()[0].color == 0xFF000000);

    // テキストレイヤー検証
    auto loadedTLayer = std::dynamic_pointer_cast<TextLayer>(loadedDoc->getLayer(3));
    assert(loadedTLayer != nullptr);
    assert(loadedTLayer->items().size() == 1);
    assert(loadedTLayer->items()[0].text == "Hello Kaburagi!");
    assert(loadedTLayer->items()[0].textColor == QColor(Qt::blue));

    // 色調補正レイヤー検証
    auto loadedALayer = std::dynamic_pointer_cast<AdjustmentLayer>(loadedDoc->getLayer(4));
    assert(loadedALayer != nullptr);
    assert(loadedALayer->adjustmentType() == AdjustmentType::BrightnessContrast);
    assert(std::abs(loadedALayer->brightness() - 0.2f) < 0.001f);
    assert(std::abs(loadedALayer->contrast() - (-0.1f)) < 0.001f);

    // グループ階層検証
    auto loadedGLayer = std::dynamic_pointer_cast<GroupLayer>(loadedDoc->getLayer(5));
    assert(loadedGLayer != nullptr);
    assert(loadedVLayer->parentGroup() == loadedGLayer);

    // 6. 画像エクスポート検証 (PNG, JPEG, BMP, TIFF, WebP, PSD)
    QStringList formats = { "png", "jpg", "bmp", "tif", "webp", "psd" };
    for (const QString& fmt : formats) {
        QString outPath = tempPath + "." + fmt;
        bool expOk = DocumentSerializer::exportImage(doc, outPath, &errorMsg);
        if (!expOk) {
            std::cerr << "Export failed for " << fmt.toStdString() << ": " << errorMsg.toStdString() << std::endl;
        }
        assert(expOk);
        QFile f(outPath);
        assert(f.exists());
        assert(f.size() > 0);
        std::cout << "Exported " << fmt.toStdString() << " (" << f.size() << " bytes)" << std::endl;
        f.remove();
    }

    // 7. QImage から Document の構築検証 (ドラッグ＆ドロップ機能の基盤)
    Document* fromImgDoc = Document::fromQImage(thumb, 350.0);
    assert(fromImgDoc != nullptr);
    assert(fromImgDoc->width() == thumb.width());
    assert(fromImgDoc->height() == thumb.height());
    assert(fromImgDoc->layers().size() == 1);
    delete fromImgDoc;

    // クリーンアップ
    delete doc;
    delete loadedDoc;
    QFile::remove(tempPath);

    std::cout << "All test_document_io assertions passed successfully!" << std::endl;
    return 0;
}
