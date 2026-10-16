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
#include "SplitPanelDialog.hpp"
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

    // 2. 4-koma with title (index 1) -> 5 panels (1 title + 4 panels)
    tplCombo->setCurrentIndex(1);
    auto p5_title = dlg.panels();
    assert(p5_title.size() == 5);
    assert(p5_title[0].name == "タイトル");
    assert(p5_title[1].name == "コマ 1");
    assert(p5_title[4].name == "コマ 4");
    assert(p5_title[0].polygon.boundingRect().top() < p5_title[1].polygon.boundingRect().top());

    // 3. 2-column 4-koma (index 2) -> 8 panels
    tplCombo->setCurrentIndex(2);
    auto p8 = dlg.panels();
    assert(p8.size() == 8);

    // 4. 2-column 4-koma with title (index 3) -> 10 panels
    tplCombo->setCurrentIndex(3);
    auto p10_title = dlg.panels();
    assert(p10_title.size() == 10);
    assert(p10_title[0].name == "タイトル 1");
    assert(p10_title[1].name == "コマ 1");
    assert(p10_title[5].name == "タイトル 2");
    assert(p10_title[6].name == "コマ 5");

    // 5. 3-tier (index 4) -> 3 panels
    tplCombo->setCurrentIndex(4);
    auto p3 = dlg.panels();
    assert(p3.size() == 3);

    // 6. Standard 5 panels (index 8) -> 5 panels
    tplCombo->setCurrentIndex(8);
    auto p5 = dlg.panels();
    assert(p5.size() == 5);

    // 7. Single panel (index 12) -> 1 panel
    tplCombo->setCurrentIndex(12);
    auto p1 = dlg.panels();
    assert(p1.size() == 1);

    // 8. Grid mode: 2 cols x 3 rows -> 6 panels
    gridRadio->setChecked(true);
    colsSpin->setValue(2);
    rowsSpin->setValue(3);
    auto pGrid = dlg.panels();
    assert(pGrid.size() == 6);

    // 9. Test persistence (saveSettings & loadSettings via accept())
    dlg.accept();

    // Create a new dialog instance; it should load the saved settings (Grid mode, 2 cols, 3 rows)
    NewPanelDialog dlg2(800, 600);
    auto gridRadio2 = dlg2.findChild<QRadioButton*>("gridRadio");
    auto colsSpin2 = dlg2.findChild<QSpinBox*>("colsSpin");
    auto rowsSpin2 = dlg2.findChild<QSpinBox*>("rowsSpin");
    assert(gridRadio2 && gridRadio2->isChecked());
    assert(colsSpin2 && colsSpin2->value() == 2);
    assert(rowsSpin2 && rowsSpin2->value() == 3);

    // Switch back to template mode, choose Yonkoma with title (index 1), and save
    auto tplRadio2 = dlg2.findChild<QRadioButton*>("templateRadio");
    auto tplCombo2 = dlg2.findChild<QComboBox*>("templateCombo");
    assert(tplRadio2 && tplCombo2);
    tplRadio2->setChecked(true);
    tplCombo2->setCurrentIndex(1);
    dlg2.accept();

    // Verify 3rd instance restores template index 1
    NewPanelDialog dlg3(800, 600);
    auto tplRadio3 = dlg3.findChild<QRadioButton*>("templateRadio");
    auto tplCombo3 = dlg3.findChild<QComboBox*>("templateCombo");
    assert(tplRadio3 && tplRadio3->isChecked());
    assert(tplCombo3 && tplCombo3->currentIndex() == 1);

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

void testLinkedPanelVertices() {
    std::cout << "[TEST] 9. Linked Panel Vertices Editing..." << std::endl;

    Document doc(120, 120);

    // 2x2 コマ作成
    // makePoly: topLeft, topRight, bottomRight, bottomLeft
    auto makePoly = [](double x, double y, double w, double h) {
        QPolygonF p;
        p << QPointF(x, y) << QPointF(x + w, y) << QPointF(x + w, y + h) << QPointF(x, y + h);
        return p;
    };

    // コマ1 (右上: x=60, y=20, w=40, h=40) -> bottomLeft = (60, 60)
    // コマ2 (左上: x=10, y=20, w=40, h=40) -> bottomRight = (50, 60)
    // コマ3 (右下: x=60, y=70, w=40, h=40) -> topLeft = (60, 70)
    // コマ4 (左下: x=10, y=70, w=40, h=40) -> topRight = (50, 70)
    std::vector<Document::PanelCreationSpec> specs = {
        {makePoly(60, 20, 40, 40), 1.0f, Qt::black, true, "コマ 1"},
        {makePoly(10, 20, 40, 40), 1.0f, Qt::black, true, "コマ 2"},
        {makePoly(60, 70, 40, 40), 1.0f, Qt::black, true, "コマ 3"},
        {makePoly(10, 70, 40, 40), 1.0f, Qt::black, true, "コマ 4"}
    };
    auto panels = doc.addPanelFolders(specs);
    assert(panels.size() == 4);

    PanelSettingsWidget settings;
    assert(settings.linkAdjacentPanels() == true);
    settings.setLinkAdjacentPanels(false);
    assert(settings.linkAdjacentPanels() == false);
    settings.setLinkAdjacentPanels(true);

    // コマ1の bottomLeft (index 3) を基準に隣接頂点を検出
    auto p1 = panels[0];
    QPointF p0 = p1->panelPolygon()[3]; // (60, 60)
    assert(p0 == QPointF(60, 60));

    float mx = 10.0f;
    float my = 10.0f;
    float maxDx = std::max(50.0f, mx * 1.8f);
    float maxDy = std::max(50.0f, my * 1.8f);

    struct Found {
        std::shared_ptr<GroupLayer> panel;
        int idx;
        QPointF pt;
    };
    std::vector<Found> linked;

    for (const auto& layer : doc.layers()) {
        auto grp = std::dynamic_pointer_cast<GroupLayer>(layer);
        if (!grp || !grp->isPanelFolder() || grp == p1) continue;
        const auto& otherPoly = grp->panelPolygon();
        for (int k = 0; k < otherPoly.size(); ++k) {
            QPointF q = otherPoly[k];
            float dx = std::abs(q.x() - p0.x());
            float dy = std::abs(q.y() - p0.y());
            if (dx <= maxDx && dy <= maxDy) {
                linked.push_back({grp, k, q});
            }
        }
    }

    // コマ2の bottomRight(index 2), コマ3の topLeft(index 0), コマ4の topRight(index 1) の3つが検出される
    assert(linked.size() == 3);

    // +5, +8 移動
    QPointF delta(5, 8);
    std::vector<PanelPolygonSnapshot> snapshots;
    QPolygonF oldP1 = p1->panelPolygon();
    QPolygonF newP1 = oldP1;
    newP1[3] += delta;
    p1->setPanelPolygon(newP1);
    snapshots.push_back({p1, oldP1, newP1});

    for (const auto& l : linked) {
        QPolygonF oldPoly = l.panel->panelPolygon();
        QPolygonF newPoly = oldPoly;
        newPoly[l.idx] += delta;
        l.panel->setPanelPolygon(newPoly);
        snapshots.push_back({l.panel, oldPoly, newPoly});
    }

    // 移動後の間隔検証
    // コマ1とコマ2の水平間隔
    double distH_after = p1->panelPolygon()[3].x() - panels[1]->panelPolygon()[2].x();
    assert(std::abs(distH_after - 10.0) < 1e-4);

    // コマ1とコマ3の垂直間隔
    double distV_after = panels[2]->panelPolygon()[0].y() - p1->panelPolygon()[3].y();
    assert(std::abs(distV_after - 10.0) < 1e-4);

    // コマ4の対角位置関係も維持
    assert(panels[3]->panelPolygon()[1] == QPointF(55, 78));

    // ModifyPanelPolygonsCommand による Undo / Redo 検証
    doc.undoStack()->push(new ModifyPanelPolygonsCommand(&doc, snapshots));
    assert(doc.undoStack()->canUndo() == true);

    // Undo 実行
    doc.undoStack()->undo();
    assert(p1->panelPolygon()[3] == QPointF(60, 60));
    assert(panels[1]->panelPolygon()[2] == QPointF(50, 60));
    assert(panels[2]->panelPolygon()[0] == QPointF(60, 70));
    assert(panels[3]->panelPolygon()[1] == QPointF(50, 70));

    // Redo 実行
    doc.undoStack()->redo();
    assert(p1->panelPolygon()[3] == QPointF(65, 68));
    assert(panels[1]->panelPolygon()[2] == QPointF(55, 68));
    assert(panels[2]->panelPolygon()[0] == QPointF(65, 78));
    assert(panels[3]->panelPolygon()[1] == QPointF(55, 78));

    std::cout << "  -> PASSED!" << std::endl;
}

void testMergePanelFolders() {
    std::cout << "[TEST] 10. Merge Adjacent Panel Folders & Undo/Redo..." << std::endl;

    Document doc(120, 120);

    auto makePoly = [](double x, double y, double w, double h) {
        QPolygonF p;
        p << QPointF(x, y) << QPointF(x + w, y) << QPointF(x + w, y + h) << QPointF(x, y + h);
        return p;
    };

    // 上下2コマ (コマ1: y=10..50, コマ2: y=60..100, 間隔 10px)
    std::vector<Document::PanelCreationSpec> specs = {
        {makePoly(10, 10, 80, 40), 1.0f, Qt::black, true, "コマ 1"},
        {makePoly(10, 60, 80, 40), 1.0f, Qt::black, true, "コマ 2"}
    };
    auto panels = doc.addPanelFolders(specs);
    std::cout << "panels returned size = " << panels.size() << std::endl;
    for (size_t i = 0; i < panels.size(); ++i) {
        std::cout << "  panels[" << i << "] = " << panels[i]->name().toStdString() << " rect=" << panels[i]->panelPolygon().boundingRect().left() << "," << panels[i]->panelPolygon().boundingRect().top() << std::endl;
    }
    assert(panels.size() == 2);
    auto p1 = panels[0];
    auto p2 = panels[1];
    std::cout << "  p1 name = " << p1->name().toStdString() << ", p2 name = " << p2->name().toStdString() << std::endl;

    auto candidates = doc.findAdjacentPanels(p1);
    std::cout << "  step 1: candidates count = " << candidates.size() << std::endl;
    for (size_t i = 0; i < candidates.size(); ++i) {
        std::cout << "    c[" << i << "]: " << candidates[i].panel->name().toStdString() << " dir=" << candidates[i].direction.toStdString() << " dist=" << candidates[i].distance << std::endl;
    }
    assert(!candidates.empty());
    assert(candidates[0].panel == p2);
    assert(candidates[0].direction == "下");
    assert(std::abs(candidates[0].distance - 10.0f) < 1e-4);

    // 結合前のレイヤー構成確認
    // m_layers: [0:背景, 1:通常レイヤー1, 2:コマ2子, 3:コマ2, 4:コマ1子, 5:コマ1] (計6枚)
    assert(doc.layers().size() == 6);

    std::cout << "  step 2: merging..." << std::endl;
    bool ok = doc.mergePanelFolders(p1, p2, 30.0f);
    assert(ok == true);

    std::cout << "  step 3: merged layers size = " << doc.layers().size() << std::endl;
    for (size_t i = 0; i < doc.layers().size(); ++i) {
        std::cout << "    layer[" << i << "]: " << doc.layers()[i]->name().toStdString() << " (group=" << (doc.layers()[i]->parentGroup() ? doc.layers()[i]->parentGroup()->name().toStdString() : "none") << ")" << std::endl;
    }
    assert(doc.layers().size() == 5);
    assert(doc.layers()[4] == p1);
    assert(doc.layers()[2]->parentGroup() == p1);
    assert(doc.layers()[3]->parentGroup() == p1);

    std::cout << "  step 4: merged polygon..." << std::endl;
    QRectF mergedRect = p1->panelPolygon().boundingRect();
    std::cout << "    mergedRect: " << mergedRect.left() << "," << mergedRect.top() << " " << mergedRect.width() << "x" << mergedRect.height() << std::endl;
    assert(std::abs(mergedRect.left() - 10.0) < 1e-4);
    assert(std::abs(mergedRect.top() - 10.0) < 1e-4);
    assert(std::abs(mergedRect.width() - 80.0) < 1e-4);
    assert(std::abs(mergedRect.height() - 90.0) < 1e-4); // 40 + 10 + 40 = 90

    std::cout << "  step 5: undo..." << std::endl;
    assert(doc.undoStack()->canUndo() == true);
    doc.undoStack()->undo();
    assert(doc.layers().size() == 6);
    assert(doc.layers()[3] == p2);
    assert(doc.layers()[2]->parentGroup() == p2); // コマ2の子レイヤーが復元
    assert(p1->panelPolygon().boundingRect().height() == 40.0); // コマ1のサイズ復元

    std::cout << "  step 6: redo..." << std::endl;
    assert(doc.undoStack()->canRedo() == true);
    doc.undoStack()->redo();
    assert(doc.layers().size() == 5);
    assert(p1->panelPolygon().boundingRect().height() == 90.0);
    assert(doc.layers()[2]->parentGroup() == p1);

    std::cout << "  -> PASSED!" << std::endl;
}

void testSplitPanelFolderEquallyAndUndo() {
    std::cout << "[TEST] 11. Split Panel Folder Equally (Vertical, Horizontal, N-split) & Undo/Redo..." << std::endl;
    Document doc(800, 800, 72.0, false, Qt::transparent);

    // 1. 縦に2分割のテスト (幅400, 高400, 間隔20px)
    QPolygonF poly;
    poly << QPointF(100, 100) << QPointF(500, 100) << QPointF(500, 500) << QPointF(100, 500);
    auto p1 = doc.addPanelFolder(poly, 3.0f, Qt::black, "コマ 1");
    assert(p1 != nullptr);

    bool splitV = doc.splitPanelFolderEqually(p1, Qt::Vertical, 2, 20.0f);
    assert(splitV);

    // 分割後のコマ枠数: 2
    std::vector<std::shared_ptr<GroupLayer>> panelsV;
    for (const auto& l : doc.layers()) {
        auto g = std::dynamic_pointer_cast<GroupLayer>(l);
        if (g && g->isPanelFolder()) panelsV.push_back(g);
    }
    assert(panelsV.size() == 2);

    // 左コマ (p1): X=100..290 (幅190)
    // 右コマ: X=310..500 (幅190)
    QRectF rLeft = p1->panelPolygon().boundingRect();
    QRectF rRight = panelsV[1]->panelPolygon().boundingRect();
    assert(std::abs(rLeft.left() - 100.0) < 1.0);
    assert(std::abs(rLeft.width() - 190.0) < 1.0);
    assert(std::abs(rRight.left() - 310.0) < 1.0);
    assert(std::abs(rRight.width() - 190.0) < 1.0);
    assert(std::abs(rRight.left() - rLeft.right() - 20.0) < 1.0); // 間隔20px

    // Undo のテスト
    assert(doc.undoStack()->canUndo());
    doc.undoStack()->undo();
    assert(p1->panelPolygon().boundingRect().width() == 400.0);
    int pCountAfterUndo = 0;
    for (const auto& l : doc.layers()) {
        auto g = std::dynamic_pointer_cast<GroupLayer>(l);
        if (g && g->isPanelFolder()) pCountAfterUndo++;
    }
    assert(pCountAfterUndo == 1);

    // Redo のテスト
    assert(doc.undoStack()->canRedo());
    doc.undoStack()->redo();
    assert(std::abs(p1->panelPolygon().boundingRect().width() - 190.0) < 1.0);

    // 2. 横に3分割のテスト (高さ400, 間隔30px)
    // p1 (幅190, 高400) を上下に3分割
    bool splitH = doc.splitPanelFolderEqually(p1, Qt::Horizontal, 3, 30.0f);
    assert(splitH);

    // p1 を3分割したので、全体で 2 - 1 + 3 = 4 コマになる
    std::vector<std::shared_ptr<GroupLayer>> panelsH;
    for (const auto& l : doc.layers()) {
        auto g = std::dynamic_pointer_cast<GroupLayer>(l);
        if (g && g->isPanelFolder()) panelsH.push_back(g);
    }
    assert(panelsH.size() == 4);

    // p1 の高さ: (400 - 2*30) / 3 = 340 / 3 ≈ 113.33px
    QRectF rH1 = p1->panelPolygon().boundingRect();
    assert(std::abs(rH1.top() - 100.0) < 1.0);
    assert(std::abs(rH1.height() - (340.0 / 3.0)) < 1.0);

    // 3分割を Undo
    doc.undoStack()->undo();
    assert(std::abs(p1->panelPolygon().boundingRect().height() - 400.0) < 1.0);

    // 3. SplitPanelDialog のテスト
    SplitPanelDialog dlg(p1, 15.0f, 25.0f);
    assert(dlg.orientation() == Qt::Vertical);
    assert(dlg.splitCount() == 2);
    assert(std::abs(dlg.margin() - 15.0f) < 0.001f);

    // 4. PanelSettingsWidget のシグナル接続テスト
    PanelSettingsWidget settingsWidget;
    bool sigV = false, sigH = false, sigCustom = false;
    QObject::connect(&settingsWidget, &PanelSettingsWidget::splitPanelVerticalRequested, [&sigV]() { sigV = true; });
    QObject::connect(&settingsWidget, &PanelSettingsWidget::splitPanelHorizontalRequested, [&sigH]() { sigH = true; });
    QObject::connect(&settingsWidget, &PanelSettingsWidget::splitPanelCustomRequested, [&sigCustom]() { sigCustom = true; });

    emit settingsWidget.splitPanelVerticalRequested();
    emit settingsWidget.splitPanelHorizontalRequested();
    emit settingsWidget.splitPanelCustomRequested();
    assert(sigV && sigH && sigCustom);

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
    testLinkedPanelVertices();
    testMergePanelFolders();
    testSplitPanelFolderEquallyAndUndo();

    std::cout << "[ALL PANEL & FRAME TESTS PASSED SUCCESSFULLY!]" << std::endl;
    return 0;
}
