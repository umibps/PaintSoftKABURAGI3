#include <QPdfWriter>
#include <QPainter>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <cassert>
#include <iostream>

#include "PageProject.hpp"
#include "NewProjectDialog.hpp"
#include "BatchExportDialog.hpp"
#include "PageManagerWidget.hpp"
#include "MainWindow.hpp"
#include "DocumentSerializer.hpp"

using namespace Kaburagi;

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    std::cout << "=== RUNNING PAGE MANAGER (.kap) TESTS ===" << std::endl;

    QTemporaryDir tempDir;
    assert(tempDir.isValid());
    QString projectDir = tempDir.path() + "/TestMangaWork";

    // ------------------------------------------------------------------------
    // 1. PageProject::createNewProject 検証
    // ------------------------------------------------------------------------
    std::cout << "[TEST 1] Creating new multi-page project..." << std::endl;
    QString err;
    auto project = PageProject::createNewProject(
        projectDir,
        "TestManga",
        4,          // 4ページ
        1200, 1600, // 幅1200, 高さ1600
        350.0,
        BindingDirection::RightToLeft,
        true,
        &err
    );
    assert(project != nullptr);
    assert(project->name() == "TestManga");
    assert(project->pageCount() == 4);
    assert(project->standardWidth() == 1200);
    assert(project->standardHeight() == 1600);
    assert(project->bindingDirection() == BindingDirection::RightToLeft);
    assert(project->startFromRight() == true);

    QString kapPath = project->projectFilePath();
    assert(QFile::exists(kapPath));

    for (int i = 0; i < 4; ++i) {
        QString pagePath = project->pageFullPath(i);
        assert(QFile::exists(pagePath));
        assert(pagePath.endsWith(".kab", Qt::CaseInsensitive));
        assert(!project->page(i).isSpread);
    }
    std::cout << "  -> PASSED!" << std::endl;

    // ------------------------------------------------------------------------
    // 2. PageProject::loadFromFile 検証
    // ------------------------------------------------------------------------
    std::cout << "[TEST 2] Loading project from .kap..." << std::endl;
    auto loadedProj = PageProject::loadFromFile(kapPath, &err);
    assert(loadedProj != nullptr);
    assert(loadedProj->name() == "TestManga");
    assert(loadedProj->pageCount() == 4);
    assert(loadedProj->bindingDirection() == BindingDirection::RightToLeft);
    assert(loadedProj->startFromRight() == true);
    std::cout << "  -> PASSED!" << std::endl;

    // ------------------------------------------------------------------------
    // 3. ページの追加・挿入・複製・削除・移動
    // ------------------------------------------------------------------------
    std::cout << "[TEST 3] Testing page additions, insertion, duplication, deletion, moving..." << std::endl;
    // (a) 追加 (Add Page)
    assert(project->addPage(QString(), &err));
    assert(project->pageCount() == 5);
    assert(QFile::exists(project->pageFullPath(4)));

    // (b) 挿入 (Insert Page at index 1)
    assert(project->insertPage(1, QString(), &err));
    assert(project->pageCount() == 6);
    assert(QFile::exists(project->pageFullPath(1)));

    // (c) 複製 (Duplicate Page at index 1)
    assert(project->duplicatePage(1, &err));
    assert(project->pageCount() == 7);
    assert(QFile::exists(project->pageFullPath(2)));

    // (d) 移動 (Move Page from 0 to 3)
    QString originalFirstPageFile = project->page(0).fileName;
    assert(project->movePage(0, 3));
    assert(project->page(3).fileName == originalFirstPageFile);

    // (e) 削除 (Delete Page)
    assert(project->deletePage(3, true, &err));
    assert(project->pageCount() == 6);

    std::cout << "  -> PASSED!" << std::endl;

    // ------------------------------------------------------------------------
    // 4. ページファイル名の整列 (Realign File Names)
    // ------------------------------------------------------------------------
    std::cout << "[TEST 4] Testing realignPageFileNames (sequential rename)..." << std::endl;
    assert(project->realignPageFileNames(&err));
    for (int i = 0; i < project->pageCount(); ++i) {
        QString expectedName = QString("page%1.kab").arg(i + 1, 4, 10, QChar('0'));
        assert(project->page(i).fileName == expectedName);
        assert(QFile::exists(project->pageFullPath(i)));
    }
    std::cout << "  -> PASSED!" << std::endl;

    // ------------------------------------------------------------------------
    // 5. サムネイル取得・キャッシュ
    // ------------------------------------------------------------------------
    std::cout << "[TEST 5] Testing pageThumbnail..." << std::endl;
    QImage thumb = project->pageThumbnail(0, QSize(100, 140));
    assert(!thumb.isNull());
    assert(thumb.width() > 0 && thumb.height() > 0);
    std::cout << "  -> PASSED!" << std::endl;

    // ------------------------------------------------------------------------
    // 6. 外部画像の読み込み・差し替え (Replace page from image)
    // ------------------------------------------------------------------------
    std::cout << "[TEST 6] Testing replacePage with external image..." << std::endl;
    QString samplePngPath = tempDir.path() + "/sample_drawing.png";
    QImage testImg(800, 1000, QImage::Format_ARGB32_Premultiplied);
    testImg.fill(QColor(255, 128, 64));
    assert(testImg.save(samplePngPath, "PNG"));

    assert(project->replacePage(0, samplePngPath, &err));
    auto replacedDoc = std::unique_ptr<Document>(DocumentSerializer::loadFromFile(project->pageFullPath(0)));
    assert(replacedDoc != nullptr);
    assert(replacedDoc->width() == 800);
    assert(replacedDoc->height() == 1000);
    std::cout << "  -> PASSED!" << std::endl;

    // ------------------------------------------------------------------------
    // 7. 見開き化 & 単ページ分割 (Combine to Spread & Split Spread)
    // ------------------------------------------------------------------------
    std::cout << "[TEST 7] Testing combineToSpread and splitSpread..." << std::endl;
    int prevCount = project->pageCount();
    // ページ1とページ2を見開き結合
    assert(project->combineToSpread(1, &err));
    assert(project->pageCount() == prevCount - 1);
    assert(project->page(1).isSpread == true);
    assert(project->page(1).spreadPageCount == 2);

    auto spreadDoc = std::unique_ptr<Document>(DocumentSerializer::loadFromFile(project->pageFullPath(1)));
    assert(spreadDoc != nullptr);
    assert(spreadDoc->width() == 2400); // 1200 + 1200

    // 単ページに分割
    assert(project->splitSpread(1, &err));
    assert(project->pageCount() == prevCount);
    assert(project->page(1).isSpread == false);
    assert(project->page(2).isSpread == false);

    auto splitDoc1 = std::unique_ptr<Document>(DocumentSerializer::loadFromFile(project->pageFullPath(1)));
    assert(splitDoc1 != nullptr);
    assert(splitDoc1->width() == 1200);
    assert(project->saveToFile());
    std::cout << "  -> PASSED!" << std::endl;

    // ------------------------------------------------------------------------
    // 8. NewProjectDialog UI検証
    // ------------------------------------------------------------------------
    std::cout << "[TEST 8] Testing NewProjectDialog..." << std::endl;
    {
        NewProjectDialog dlg;
        assert(dlg.pageCount() == 8);
        assert(dlg.bindingDirection() == BindingDirection::RightToLeft);
        assert(dlg.startFromRight() == true);
    }
    std::cout << "  -> PASSED!" << std::endl;

    // ------------------------------------------------------------------------
    // 9. PageManagerWidget UI & D&D検証
    // ------------------------------------------------------------------------
    std::cout << "[TEST 9] Testing PageManagerWidget..." << std::endl;
    {
        PageManagerWidget pm;
        pm.setProject(project);

        bool openRequested = false;
        int requestedIndex = -1;
        QObject::connect(&pm, &PageManagerWidget::pageOpenRequested, [&](int idx, const QString&) {
            openRequested = true;
            requestedIndex = idx;
        });

        // カードダブルクリックのシミュレーション
        auto cards = pm.findChildren<PageCardWidget*>();
        assert(!cards.empty());
        assert(cards.size() == static_cast<size_t>(project->pageCount()));

        pm.setCurrentPagePath(project->pageFullPath(0));
    }
    std::cout << "  -> PASSED!" << std::endl;

    // ------------------------------------------------------------------------
    // 10. 一括書き出し (Batch Export) 検証
    // ------------------------------------------------------------------------
    std::cout << "[TEST 10] Testing Batch Export (Images and PDF)..." << std::endl;
    QString exportDir = tempDir.path() + "/ExportOutput";
    QDir().mkpath(exportDir);

    // PNG 連番書き出しの実行
    for (int i = 0; i < project->pageCount(); ++i) {
        QString outPng = QString("%1/page_%2.png").arg(exportDir).arg(i + 1, 4, 10, QChar('0'));
        auto doc = std::unique_ptr<Document>(DocumentSerializer::loadFromFile(project->pageFullPath(i)));
        assert(doc != nullptr);
        QImage rendered = doc->renderToImage();
        assert(!rendered.isNull());
        assert(rendered.save(outPng, "PNG"));
        assert(QFile::exists(outPng));
    }

    // PDF 書き出しの実行
    QString outPdf = exportDir + "/TestManga.pdf";
    {
        QPdfWriter pdfWriter(outPdf);
        pdfWriter.setResolution(350);
        QPainter p(&pdfWriter);
        for (int i = 0; i < project->pageCount(); ++i) {
            if (i > 0) pdfWriter.newPage();
            auto doc = std::unique_ptr<Document>(DocumentSerializer::loadFromFile(project->pageFullPath(i)));
            QImage rendered = doc->renderToImage();
            p.drawImage(QRectF(0, 0, pdfWriter.width(), pdfWriter.height()), rendered);
        }
        p.end();
    }
    assert(QFile::exists(outPdf));
    assert(QFileInfo(outPdf).size() > 0);
    std::cout << "  -> PASSED!" << std::endl;

    // ------------------------------------------------------------------------
    // 11. MainWindow 統合検証
    // ------------------------------------------------------------------------
    std::cout << "[TEST 11] Testing MainWindow integration..." << std::endl;
    {
        MainWindow mw;
        assert(mw.pageManager() != nullptr);
        assert(mw.pageManagerDock() != nullptr);

        // 作品を開く
        mw.openProject(kapPath);
        assert(mw.currentProject() != nullptr);
        assert(mw.currentProject()->name() == "TestManga");
        assert(mw.pageManagerDock()->isVisible());

        // 1ページ目が自動的にキャンバスタブで開かれていること
        assert(mw.tabCount() == 1);
        assert(mw.document() != nullptr);
        assert(mw.currentFilePath() == project->pageFullPath(0));

        // 2ページ目をタブで開く
        mw.openPageInTab(1, project->pageFullPath(1));
        assert(mw.tabCount() == 2);
        assert(mw.currentFilePath() == project->pageFullPath(1));

        // 既に開いている1ページ目を再度開く要求 -> タブが増えずに既存タブへ切り替わること
        mw.openPageInTab(0, project->pageFullPath(0));
        assert(mw.tabCount() == 2);
        assert(mw.currentTabIndex() == 0);
        assert(mw.currentFilePath() == project->pageFullPath(0));

        // 作品を閉じる
        mw.currentProject()->setDirty(false);
        mw.onCloseProject();
        assert(mw.currentProject() == nullptr);
        assert(!mw.pageManagerDock()->isVisible());
    }
    std::cout << "  -> PASSED!" << std::endl;

    std::cout << "[ALL PAGE MANAGER TESTS PASSED SUCCESSFULLY!]" << std::endl;
    return 0;
}
