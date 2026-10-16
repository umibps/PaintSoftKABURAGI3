#include <QApplication>
#include <QPolygonF>
#include <QDir>
#include <QFile>
#include <iostream>
#include <cassert>
#include <cstdio>
#include "Document.hpp"
#include "GroupLayer.hpp"
#include "DocumentSerializer.hpp"
#include "PanelSettingsWidget.hpp"
#include "ToolBoxWidget.hpp"
#include "NewPanelDialog.hpp"
#include <QComboBox>
#include <QRadioButton>
#include <QSpinBox>

using namespace Kaburagi;

void testPanelFolderCreation() {
    std::cout << "[TEST] 1. Panel Folder Creation..." << std::endl;
    Document doc(800, 600, 72.0, true, Qt::transparent);

    QPolygonF poly;
    poly << QPointF(50, 50) << QPointF(400, 50) << QPointF(400, 300) << QPointF(50, 300);

    auto panel = doc.addPanelFolder(poly, 4.0f, Qt::black, "コマ 1");
    assert(panel != nullptr);
    assert(panel->isPanelFolder());
    assert(panel->panelPolygon().size() == 4);
    assert(std::abs(panel->panelBorderWidth() - 4.0f) < 0.001f);
    assert(panel->panelBorderColor() == Qt::black);
    assert(panel->drawPanelBorder() == true);

    // 子レイヤー（ラスターレイヤー）が自動配置されているか
    assert(panel->children().size() >= 1);
    auto child = panel->children()[0].lock();
    assert(child != nullptr);
    assert(child->parentGroup() == panel);

    // レイヤー順序: panel (上位) が child (直下) にあることを検証（UI上でフォルダーの下に子レイヤーが来る）
    int panelIdx = -1;
    int childIdx = -1;
    for (size_t i = 0; i < doc.layers().size(); ++i) {
        if (doc.layers()[i] == panel) panelIdx = static_cast<int>(i);
        if (doc.layers()[i] == child) childIdx = static_cast<int>(i);
    }
    assert(panelIdx > childIdx);
    assert(panelIdx == childIdx + 1);

    // findPanelFolderAt のテスト
    auto found = doc.findPanelFolderAt(QPointF(100, 100));
    assert(found == panel);

    auto notFound = doc.findPanelFolderAt(QPointF(500, 500));
    assert(notFound == nullptr);

    std::cout << "  -> PASSED!" << std::endl;
}

void testPanelClippingAndBorder() {
    std::cout << "[TEST] 2. Panel Clipping & Border Drawing..." << std::endl;
    Document doc(400, 400, 72.0, false, Qt::transparent);

    // 枠 (100, 100) - (300, 300)
    QPolygonF poly;
    poly << QPointF(100, 100) << QPointF(300, 100) << QPointF(300, 300) << QPointF(100, 300);

    auto panel = doc.addPanelFolder(poly, 4.0f, QColor(0, 0, 0, 255));
    assert(panel->children().size() >= 1);
    auto innerLayer = panel->children()[0].lock();
    assert(innerLayer != nullptr);

    // コマ枠の内側 (150, 150) と外側 (50, 50) の両方に赤色を塗る
    innerLayer->tileManager().clear(0xFFFF0000); // ARGB: 赤

    // 合成実行
    doc.composite(QRect(0, 0, 400, 400));

    // 1. コマ枠外 (50, 50) はマスクされ透明 (A=0) になっているか
    QColor cOutside = doc.samplePixel(50, 50);
    assert(cOutside.alpha() == 0 || (cOutside.red() == 255 && cOutside.green() == 255 && cOutside.blue() == 255)); // 白キャンバス背景なし

    // 2. コマ枠内 (150, 150) は赤色 (R=255) が保持されているか
    QColor cInside = doc.samplePixel(150, 150);
    assert(cInside.red() > 200 && cInside.green() < 50 && cInside.blue() < 50);

    // 3. 枠線位置 (100, 100) 付近に黒線 (R=0, G=0, B=0) が描画されているか
    QColor cBorder = doc.samplePixel(100, 100);
    assert(cBorder.red() < 50 && cBorder.green() < 50 && cBorder.blue() < 50);

    std::cout << "  -> PASSED!" << std::endl;
}

void testPanelSplitting() {
    std::cout << "[TEST] 3. Panel Splitting (Horizontal, Vertical, Slanted)..." << std::endl;
    Document doc(800, 800, 72.0, false, Qt::transparent);

    // コマ (100, 100) - (500, 500)
    QPolygonF poly;
    poly << QPointF(100, 100) << QPointF(500, 100) << QPointF(500, 500) << QPointF(100, 500);
    auto panel1 = doc.addPanelFolder(poly, 3.0f, Qt::black);

    // 1. 水平分割 (Y=300 で上下分割, 間隔 marginY=40)
    QLineF hCut(QPointF(50, 300), QPointF(550, 300));
    bool splitH = doc.splitPanelFolder(panel1, hCut, 20.0f, 40.0f);
    assert(splitH);

    // 分割によりコマフォルダが2つになっているはず
    int panelCount = 0;
    for (const auto& l : doc.layers()) {
        if (l->type() == LayerType::Group) {
            auto g = std::dynamic_pointer_cast<GroupLayer>(l);
            if (g && g->isPanelFolder()) panelCount++;
        }
    }
    assert(panelCount == 2);

    // 上のコマと下のコマの間隔が約40px空いているか
    auto topPanel = doc.findPanelFolderAt(QPointF(300, 200));
    auto bottomPanel = doc.findPanelFolderAt(QPointF(300, 400));
    assert(topPanel != nullptr);
    assert(bottomPanel != nullptr);
    assert(topPanel != bottomPanel);

    // 間隔領域 (Y=300) にはコマが存在しないこと
    auto gapPanel = doc.findPanelFolderAt(QPointF(300, 300));
    assert(gapPanel == nullptr);

    // 2. 垂直分割 (上のコマを左右分割, X=300, 間隔 marginX=20)
    QLineF vCut(QPointF(300, 50), QPointF(300, 300));
    bool splitV = doc.splitPanelFolder(topPanel, vCut, 20.0f, 40.0f);
    assert(splitV);

    panelCount = 0;
    for (const auto& l : doc.layers()) {
        if (l->type() == LayerType::Group) {
            auto g = std::dynamic_pointer_cast<GroupLayer>(l);
            if (g && g->isPanelFolder()) panelCount++;
        }
    }
    assert(panelCount == 3);

    // 3. 斜め分割 (下のコマを斜めに分割)
    QLineF slantCut(QPointF(100, 350), QPointF(500, 450));
    bool splitSlant = doc.splitPanelFolder(bottomPanel, slantCut, 20.0f, 40.0f);
    assert(splitSlant);

    panelCount = 0;
    for (const auto& l : doc.layers()) {
        if (l->type() == LayerType::Group) {
            auto g = std::dynamic_pointer_cast<GroupLayer>(l);
            if (g && g->isPanelFolder()) panelCount++;
        }
    }
    assert(panelCount == 4);

    std::cout << "  -> PASSED!" << std::endl;
}

void testPanelShapeEditing() {
    std::cout << "[TEST] 4. Panel Shape Editing (Polygon deformation)..." << std::endl;
    Document doc(600, 600, 72.0, false, Qt::transparent);

    QPolygonF poly;
    poly << QPointF(100, 100) << QPointF(400, 100) << QPointF(400, 400) << QPointF(100, 400);
    auto panel = doc.addPanelFolder(poly, 3.0f, Qt::black);

    // 頂点変形（右上の頂点を (500, 50) に移動して斜めゴマにする）
    QPolygonF deformPoly = panel->panelPolygon();
    deformPoly[1] = QPointF(500, 50);
    panel->setPanelPolygon(deformPoly);

    assert(panel->panelPolygon()[1] == QPointF(500, 50));

    // 変形後の新しい領域 (450, 75) に含まれるか
    assert(panel->panelPolygon().containsPoint(QPointF(450, 75), Qt::OddEvenFill));

    std::cout << "  -> PASSED!" << std::endl;
}

void testPanelSerialization() {
    std::cout << "[TEST] 5. Panel Serialization & Deserialization..." << std::endl;
    Document doc(800, 600, 72.0, false, Qt::white);

    QPolygonF poly;
    poly << QPointF(60, 80) << QPointF(500, 90) << QPointF(480, 450) << QPointF(70, 430);

    auto panel = doc.addPanelFolder(poly, 5.0f, QColor(20, 40, 80), "テストコマ");
    assert(panel != nullptr);
    panel->setDrawPanelBorder(true);

    QString tempPath = QDir::tempPath() + "/test_panel_save.kab";
    QFile::remove(tempPath);

    QString errMsg;
    bool saved = DocumentSerializer::saveToFile(&doc, tempPath, &errMsg);
    assert(saved);

    Document* loadedDoc = DocumentSerializer::loadFromFile(tempPath, nullptr, &errMsg);
    assert(loadedDoc != nullptr);

    // 復元されたコマフォルダの検証
    std::shared_ptr<GroupLayer> loadedPanel = nullptr;
    for (const auto& l : loadedDoc->layers()) {
        if (l->type() == LayerType::Group) {
            auto g = std::dynamic_pointer_cast<GroupLayer>(l);
            if (g && g->isPanelFolder()) {
                loadedPanel = g;
                break;
            }
        }
    }

    assert(loadedPanel != nullptr);
    assert(loadedPanel->isPanelFolder() == true);
    assert(loadedPanel->panelPolygon().size() == 4);
    assert(std::abs(loadedPanel->panelPolygon()[0].x() - 60.0) < 0.001);
    assert(std::abs(loadedPanel->panelPolygon()[1].x() - 500.0) < 0.001);
    assert(std::abs(loadedPanel->panelBorderWidth() - 5.0f) < 0.001f);
    assert(loadedPanel->panelBorderColor() == QColor(20, 40, 80));
    assert(loadedPanel->drawPanelBorder() == true);

    QFile::remove(tempPath);
    delete loadedDoc;
    std::cout << "  -> PASSED!" << std::endl;
}

void testPanelSettingsWidget() {
    std::cout << "[TEST] 6. PanelSettingsWidget UI Sync..." << std::endl;
    PanelSettingsWidget widget;

    assert(widget.drawBorder() == true);
    assert(std::abs(widget.borderWidth() - 3.0f) < 0.001f);
    assert(std::abs(widget.marginHorizontal() - 20.0f) < 0.001f);
    assert(std::abs(widget.marginVertical() - 40.0f) < 0.001f);

    GroupLayer panel("コマ", 400, 400);
    panel.setPanelFolder(true);
    panel.setPanelBorderWidth(8.5f);
    panel.setPanelBorderColor(QColor(10, 20, 30));
    panel.setDrawPanelBorder(false);

    widget.setFromPanel(&panel);
    assert(widget.drawBorder() == false);
    assert(std::abs(widget.borderWidth() - 8.5f) < 0.001f);
    assert(widget.borderColor() == QColor(10, 20, 30));

    // ToolType switching test (PanelFrame vs PanelSplit)
    assert(widget.isBorderGroupVisible() == true);
    assert(widget.isMarginGroupVisible() == false);

    widget.setToolType(ToolType::PanelSplit);
    assert(widget.isBorderGroupVisible() == false);
    assert(widget.isMarginGroupVisible() == true);

    widget.setToolType(ToolType::PanelFrame);
    assert(widget.isBorderGroupVisible() == true);
    assert(widget.isMarginGroupVisible() == false);

    std::cout << "  -> PASSED!" << std::endl;
}

void testNewPanelDialog() {
    std::cout << "[TEST] 7. NewPanelDialog Templates & Grid..." << std::endl;
    NewPanelDialog dlg(1600, 1200, 3.0f, Qt::black, 20.0f, 40.0f);

    auto tplCombo = dlg.findChild<QComboBox*>("templateCombo");
    auto gridRadio = dlg.findChild<QRadioButton*>("gridRadio");
    auto tplRadio = dlg.findChild<QRadioButton*>("templateRadio");
    auto colsSpin = dlg.findChild<QSpinBox*>("colsSpin");
    auto rowsSpin = dlg.findChild<QSpinBox*>("rowsSpin");

    assert(tplCombo != nullptr);
    assert(gridRadio != nullptr);
    assert(tplRadio != nullptr);
    assert(colsSpin != nullptr);
    assert(rowsSpin != nullptr);

    // 1. Default: 4-koma (1 col x 4 rows) -> 4 panels
    auto p4 = dlg.panels();
    assert(p4.size() == 4);
    assert(p4[0].name == "コマ 1");
    assert(p4[3].name == "コマ 4");
    assert(p4[0].polygon.boundingRect().top() < p4[1].polygon.boundingRect().top());
    assert(p4[1].polygon.boundingRect().top() < p4[2].polygon.boundingRect().top());
    assert(p4[2].polygon.boundingRect().top() < p4[3].polygon.boundingRect().top());

    // 2. 2-column 4-koma (index 1) -> 8 panels
    tplCombo->setCurrentIndex(1);
    auto p8 = dlg.panels();
    assert(p8.size() == 8);

    // 3. 3-tier (index 2) -> 3 panels
    tplCombo->setCurrentIndex(2);
    auto p3 = dlg.panels();
    assert(p3.size() == 3);

    // 4. Standard 5 panels (index 6) -> 5 panels
    tplCombo->setCurrentIndex(6);
    auto p5 = dlg.panels();
    assert(p5.size() == 5);

    // 5. Single panel (index 10) -> 1 panel
    tplCombo->setCurrentIndex(10);
    auto p1 = dlg.panels();
    assert(p1.size() == 1);

    // 6. Grid mode: 2 cols x 3 rows -> 6 panels
    gridRadio->setChecked(true);
    colsSpin->setValue(2);
    rowsSpin->setValue(3);
    auto pGrid = dlg.panels();
    assert(pGrid.size() == 6);

    std::cout << "  -> PASSED!" << std::endl;
}

void testPanelFoldersUndoAndHierarchy() {
    std::cout << "[TEST] 8. Panel Folders Hierarchy & Undo/Redo..." << std::endl;
    Document doc(800, 600, 72.0, true, Qt::white);
    // 初期状態: 背景(0), レイヤー 1(1)
    assert(doc.layers().size() == 2);

    QPolygonF poly1, poly2;
    poly1 << QPointF(50, 50) << QPointF(750, 50) << QPointF(750, 280) << QPointF(50, 280);
    poly2 << QPointF(50, 320) << QPointF(750, 320) << QPointF(750, 550) << QPointF(50, 550);

    Document::PanelCreationSpec spec1{poly1, 3.0f, Qt::black, true, "コマ 1"};
    Document::PanelCreationSpec spec2{poly2, 3.0f, Qt::black, true, "コマ 2"};

    auto created = doc.addPanelFolders({spec1, spec2});
    assert(created.size() == 2);
    assert(created[0]->name() == "コマ 1");
    assert(created[1]->name() == "コマ 2");

    // レイヤー総数は 2 + 4 = 6 枚
    const auto& layers = doc.layers();
    assert(layers.size() == 6);

    // インデックス順序（下位から上位へ）:
    // layers[0]: 背景
    // layers[1]: レイヤー 1 (既存)
    // layers[2]: レイヤー 1 (コマ 2 の子)
    // layers[3]: コマ 2 (GroupLayer)
    // layers[4]: レイヤー 1 (コマ 1 の子)
    // layers[5]: コマ 1 (GroupLayer)
    assert(layers[5]->name() == "コマ 1");
    assert(layers[5]->type() == LayerType::Group);
    assert(layers[4]->parentGroup() == layers[5]);

    assert(layers[3]->name() == "コマ 2");
    assert(layers[3]->type() == LayerType::Group);
    assert(layers[2]->parentGroup() == layers[3]);

    // UIのレイヤービュー（逆順走査）で:
    // 行0: コマ 1
    // 行1:   └ レイヤー 1
    // 行2: コマ 2
    // 行3:   └ レイヤー 1
    // 行4: レイヤー 1
    // 行5: 背景
    // と完全に整っていることを確認！

    // 元に戻す (Undo)
    assert(doc.undoStack()->canUndo() == true);
    doc.undoStack()->undo();
    assert(doc.layers().size() == 2);
    assert(doc.layers()[0]->name() == "背景");
    assert(doc.layers()[1]->name() == "レイヤー 1");

    // やり直し (Redo)
    assert(doc.undoStack()->canRedo() == true);
    doc.undoStack()->redo();
    assert(doc.layers().size() == 6);
    assert(doc.layers()[5]->name() == "コマ 1");
    assert(doc.layers()[4]->parentGroup() == doc.layers()[5]);
    assert(doc.layers()[3]->name() == "コマ 2");
    assert(doc.layers()[2]->parentGroup() == doc.layers()[3]);

    // もう一度 Undo
    doc.undoStack()->undo();
    assert(doc.layers().size() == 2);

    std::cout << "  -> PASSED!" << std::endl;
}

int main(int argc, char* argv[]) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    QApplication app(argc, argv);

    std::cout << "=== RUNNING PANEL & FRAME TESTS ===" << std::endl;
    testPanelFolderCreation();
    testPanelClippingAndBorder();
    testPanelSplitting();
    testPanelShapeEditing();
    testPanelSerialization();
    testPanelSettingsWidget();
    testNewPanelDialog();
    testPanelFoldersUndoAndHierarchy();

    std::cout << "[ALL PANEL & FRAME TESTS PASSED SUCCESSFULLY!]" << std::endl;
    return 0;
}
