#include "PageManagerWidget.hpp"
#include "NewProjectDialog.hpp"
#include "BatchExportDialog.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDrag>
#include <QMimeData>
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QStyle>
#include <QPainter>

namespace Kaburagi {

// ----------------------------------------------------------------------------
// PageCardWidget
// ----------------------------------------------------------------------------
PageCardWidget::PageCardWidget(int pageIndex, const PageItem& item, const QImage& thumbnail, bool isCurrent, QWidget* parent)
    : QWidget(parent), m_pageIndex(pageIndex), m_isCurrent(isCurrent) {
    setAcceptDrops(true);
    setCursor(Qt::PointingHandCursor);

    auto lay = new QVBoxLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(4);

    m_thumbLabel = new QLabel(this);
    m_thumbLabel->setAlignment(Qt::AlignCenter);
    QPixmap pix = QPixmap::fromImage(thumbnail);
    m_thumbLabel->setPixmap(pix);
    lay->addWidget(m_thumbLabel, 1, Qt::AlignCenter);

    QString titleText = item.title.isEmpty() ? QString("%1p").arg(pageIndex + 1) : item.title;
    if (item.isSpread) {
        titleText += tr(" [見開き]");
    }

    m_titleLabel = new QLabel(titleText, this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet("font-size: 11px; font-weight: bold; color: #333333;");
    lay->addWidget(m_titleLabel);

    setCurrent(isCurrent);
}

void PageCardWidget::setCurrent(bool current) {
    m_isCurrent = current;
    if (m_isCurrent) {
        setStyleSheet(
            "PageCardWidget { background: #e3f2fd; border: 2px solid #1976d2; border-radius: 4px; }"
        );
    } else {
        setStyleSheet(
            "PageCardWidget { background: #ffffff; border: 1px solid #c0c0c0; border-radius: 4px; }"
            "PageCardWidget:hover { background: #f5f5f5; border: 1px solid #999999; }"
        );
    }
}

void PageCardWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragStartPos = event->pos();
        emit clicked(m_pageIndex);
    }
    QWidget::mousePressEvent(event);
}

void PageCardWidget::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        if ((event->pos() - m_dragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
            auto drag = new QDrag(this);
            auto mimeData = new QMimeData();
            mimeData->setData("application/x-kaburagi-page-index", QByteArray::number(m_pageIndex));
            drag->setMimeData(mimeData);

            if (m_thumbLabel && !m_thumbLabel->pixmap().isNull()) {
                drag->setPixmap(m_thumbLabel->pixmap().scaled(60, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                drag->setHotSpot(QPoint(30, 40));
            }
            drag->exec(Qt::MoveAction);
        }
    }
    QWidget::mouseMoveEvent(event);
}

void PageCardWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit doubleClicked(m_pageIndex);
    }
    QWidget::mouseDoubleClickEvent(event);
}

void PageCardWidget::contextMenuEvent(QContextMenuEvent* event) {
    emit contextMenuRequested(m_pageIndex, event->globalPos());
}

void PageCardWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasFormat("application/x-kaburagi-page-index")) {
        event->acceptProposedAction();
    }
}

void PageCardWidget::dropEvent(QDropEvent* event) {
    if (event->mimeData()->hasFormat("application/x-kaburagi-page-index")) {
        int fromIdx = event->mimeData()->data("application/x-kaburagi-page-index").toInt();
        if (fromIdx != m_pageIndex) {
            emit pageMoveRequested(fromIdx, m_pageIndex);
        }
        event->acceptProposedAction();
    }
}

// ----------------------------------------------------------------------------
// PageManagerWidget
// ----------------------------------------------------------------------------
PageManagerWidget::PageManagerWidget(QWidget* parent)
    : QWidget(parent) {
    setupUI();
}

void PageManagerWidget::setupUI() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. 上部ツールバー
    m_toolBar = new QToolBar(this);
    m_toolBar->setIconSize(QSize(18, 18));
    m_toolBar->setStyleSheet("QToolBar { background: #f0f0f0; border-bottom: 1px solid #d0d0d0; padding: 2px; }");

    auto actAdd = m_toolBar->addAction(tr("ページ追加"));
    actAdd->setToolTip(tr("末尾に新規ページを追加"));
    connect(actAdd, &QAction::triggered, this, &PageManagerWidget::onAddPage);

    auto actInsert = m_toolBar->addAction(tr("挿入"));
    actInsert->setToolTip(tr("選択位置の前に新規ページを挿入"));
    connect(actInsert, &QAction::triggered, this, &PageManagerWidget::onInsertPage);

    auto actDelete = m_toolBar->addAction(tr("削除"));
    actDelete->setToolTip(tr("選択中のページを削除"));
    connect(actDelete, &QAction::triggered, this, &PageManagerWidget::onDeletePage);

    m_toolBar->addSeparator();

    auto actRealign = m_toolBar->addAction(tr("整列"));
    actRealign->setToolTip(tr("ページファイル名を連番に整列 (page0001.kab...)"));
    connect(actRealign, &QAction::triggered, this, &PageManagerWidget::onRealignFileNames);

    auto actExport = m_toolBar->addAction(tr("一括書き出し"));
    actExport->setToolTip(tr("全ページまたは指定ページを連番画像/PDFへ一括書き出し"));
    connect(actExport, &QAction::triggered, this, &PageManagerWidget::requestBatchExport);

    mainLayout->addWidget(m_toolBar);

    // 2. ページ一覧スクロール領域
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_gridContainer = new QWidget();
    m_gridLayout = new QGridLayout(m_gridContainer);
    m_gridLayout->setContentsMargins(8, 8, 8, 8);
    m_gridLayout->setSpacing(8);

    m_scrollArea->setWidget(m_gridContainer);
    mainLayout->addWidget(m_scrollArea);

    buildGridLayout();
}

void PageManagerWidget::setProject(std::shared_ptr<PageProject> project) {
    m_project = project;
    m_selectedPageIndex = -1;
    refreshPageList();
}

void PageManagerWidget::setCurrentPagePath(const QString& fullPath) {
    m_currentOpenPath = fullPath;
    if (!m_project) return;

    for (size_t i = 0; i < m_cardWidgets.size(); ++i) {
        if (m_cardWidgets[i]) {
            bool isCur = (m_project->pageFullPath(static_cast<int>(i)) == fullPath);
            m_cardWidgets[i]->setCurrent(isCur);
        }
    }
}

void PageManagerWidget::refreshPageList() {
    buildGridLayout();
}

void PageManagerWidget::buildGridLayout() {
    // 既存のカードウィジェットを破棄
    m_cardWidgets.clear();
    QLayoutItem* child;
    while ((child = m_gridLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            delete child->widget();
        }
        delete child;
    }

    if (!m_project || m_project->pageCount() == 0) {
        auto emptyLabel = new QLabel(tr("作品が開かれていません。\n「ページ管理」メニューから新規作成または開いてください。"), m_gridContainer);
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet("color: #777777; font-size: 12px; padding: 30px;");
        m_gridLayout->addWidget(emptyLabel, 0, 0, 1, 2);
        return;
    }

    bool isRtl = (m_project->bindingDirection() == BindingDirection::RightToLeft);
    bool startFromRight = m_project->startFromRight();

    int totalPages = m_project->pageCount();
    m_cardWidgets.resize(totalPages, nullptr);

    int currentRow = 0;
    int pageIdx = 0;

    // 先頭行 (表紙 / 1p)
    if (pageIdx < totalPages) {
        const auto& item = m_project->page(pageIdx);
        QImage thumb = m_project->pageThumbnail(pageIdx, QSize(100, 140));
        bool isCur = (m_project->pageFullPath(pageIdx) == m_currentOpenPath);

        auto card = new PageCardWidget(pageIdx, item, thumb, isCur, m_gridContainer);
        connect(card, &PageCardWidget::clicked, this, &PageManagerWidget::onCardClicked);
        connect(card, &PageCardWidget::doubleClicked, this, &PageManagerWidget::onCardDoubleClicked);
        connect(card, &PageCardWidget::contextMenuRequested, this, &PageManagerWidget::onCardContextMenu);
        connect(card, &PageCardWidget::pageMoveRequested, this, &PageManagerWidget::onPageMoved);
        m_cardWidgets[pageIdx] = card;

        if (item.isSpread) {
            // 見開き表紙
            m_gridLayout->addWidget(card, currentRow, 0, 1, 2);
            pageIdx++;
            currentRow++;
        } else {
            if (isRtl) {
                // 右綴じ: 表紙(1p)は右側 (col = 1)
                if (startFromRight) {
                    m_gridLayout->addWidget(card, currentRow, 1);
                    // 左側は空欄プレースホルダー
                    auto spacer = new QWidget(m_gridContainer);
                    spacer->setFixedSize(110, 160);
                    m_gridLayout->addWidget(spacer, currentRow, 0);
                    pageIdx++;
                    currentRow++;
                } else {
                    // 左側から開始
                    m_gridLayout->addWidget(card, currentRow, 0);
                    pageIdx++;
                    // 次のページが同じ行の右側に入る可能性があるため改行しない
                }
            } else {
                // 左綴じ: 表紙(1p)は左側 (col = 0)
                if (!startFromRight) {
                    m_gridLayout->addWidget(card, currentRow, 0);
                    auto spacer = new QWidget(m_gridContainer);
                    spacer->setFixedSize(110, 160);
                    m_gridLayout->addWidget(spacer, currentRow, 1);
                    pageIdx++;
                    currentRow++;
                } else {
                    m_gridLayout->addWidget(card, currentRow, 1);
                    pageIdx++;
                }
            }
        }
    }

    // 以降のページ (見開きペアで2列ずつ配置)
    while (pageIdx < totalPages) {
        const auto& item1 = m_project->page(pageIdx);
        if (item1.isSpread) {
            // 見開きページは2列を結合して中央配置
            QImage thumb = m_project->pageThumbnail(pageIdx, QSize(100, 140));
            bool isCur = (m_project->pageFullPath(pageIdx) == m_currentOpenPath);
            auto card = new PageCardWidget(pageIdx, item1, thumb, isCur, m_gridContainer);
            connect(card, &PageCardWidget::clicked, this, &PageManagerWidget::onCardClicked);
            connect(card, &PageCardWidget::doubleClicked, this, &PageManagerWidget::onCardDoubleClicked);
            connect(card, &PageCardWidget::contextMenuRequested, this, &PageManagerWidget::onCardContextMenu);
            connect(card, &PageCardWidget::pageMoveRequested, this, &PageManagerWidget::onPageMoved);
            m_cardWidgets[pageIdx] = card;

            m_gridLayout->addWidget(card, currentRow, 0, 1, 2);
            pageIdx++;
            currentRow++;
            continue;
        }

        // 単ページ
        QImage thumb1 = m_project->pageThumbnail(pageIdx, QSize(100, 140));
        bool isCur1 = (m_project->pageFullPath(pageIdx) == m_currentOpenPath);
        auto card1 = new PageCardWidget(pageIdx, item1, thumb1, isCur1, m_gridContainer);
        connect(card1, &PageCardWidget::clicked, this, &PageManagerWidget::onCardClicked);
        connect(card1, &PageCardWidget::doubleClicked, this, &PageManagerWidget::onCardDoubleClicked);
        connect(card1, &PageCardWidget::contextMenuRequested, this, &PageManagerWidget::onCardContextMenu);
        connect(card1, &PageCardWidget::pageMoveRequested, this, &PageManagerWidget::onPageMoved);
        m_cardWidgets[pageIdx] = card1;

        if (pageIdx + 1 < totalPages && !m_project->page(pageIdx + 1).isSpread) {
            // ペアとなる2枚目
            int nextIdx = pageIdx + 1;
            const auto& item2 = m_project->page(nextIdx);
            QImage thumb2 = m_project->pageThumbnail(nextIdx, QSize(100, 140));
            bool isCur2 = (m_project->pageFullPath(nextIdx) == m_currentOpenPath);
            auto card2 = new PageCardWidget(nextIdx, item2, thumb2, isCur2, m_gridContainer);
            connect(card2, &PageCardWidget::clicked, this, &PageManagerWidget::onCardClicked);
            connect(card2, &PageCardWidget::doubleClicked, this, &PageManagerWidget::onCardDoubleClicked);
            connect(card2, &PageCardWidget::contextMenuRequested, this, &PageManagerWidget::onCardContextMenu);
            connect(card2, &PageCardWidget::pageMoveRequested, this, &PageManagerWidget::onPageMoved);
            m_cardWidgets[nextIdx] = card2;

            if (isRtl) {
                // 右綴じ: pageIdx(若い番号)が右側 (col = 1), nextIdx(後の番号)が左側 (col = 0)
                m_gridLayout->addWidget(card1, currentRow, 1);
                m_gridLayout->addWidget(card2, currentRow, 0);
            } else {
                // 左綴じ: pageIdx(若い番号)が左側 (col = 0), nextIdx(後の番号)が右側 (col = 1)
                m_gridLayout->addWidget(card1, currentRow, 0);
                m_gridLayout->addWidget(card2, currentRow, 1);
            }
            pageIdx += 2;
            currentRow++;
        } else {
            // 単独ページ (最終ページ等)
            if (isRtl) {
                m_gridLayout->addWidget(card1, currentRow, 1);
            } else {
                m_gridLayout->addWidget(card1, currentRow, 0);
            }
            pageIdx++;
            currentRow++;
        }
    }

    // 縦スクロール用に下部に伸縮スペーサーを追加
    m_gridLayout->setRowStretch(currentRow, 1);
}

void PageManagerWidget::onCardClicked(int index) {
    m_selectedPageIndex = index;
    for (int i = 0; i < static_cast<int>(m_cardWidgets.size()); ++i) {
        if (m_cardWidgets[i]) {
            bool isCur = (m_project && m_project->pageFullPath(i) == m_currentOpenPath);
            m_cardWidgets[i]->setCurrent(isCur || (i == m_selectedPageIndex));
        }
    }
}

void PageManagerWidget::onCardDoubleClicked(int index) {
    if (!m_project) return;
    QString path = m_project->pageFullPath(index);
    emit pageOpenRequested(index, path);
}

void PageManagerWidget::onCardContextMenu(int index, const QPoint& globalPos) {
    if (!m_project || index < 0 || index >= m_project->pageCount()) return;

    m_selectedPageIndex = index;
    QMenu menu(this);

    auto actOpen = menu.addAction(tr("このページを開く"));
    connect(actOpen, &QAction::triggered, this, [this, index]() {
        onCardDoubleClicked(index);
    });

    menu.addSeparator();

    auto actAdd = menu.addAction(tr("末尾にページを追加"));
    connect(actAdd, &QAction::triggered, this, &PageManagerWidget::onAddPage);

    auto actInsert = menu.addAction(tr("この位置にページを挿入"));
    connect(actInsert, &QAction::triggered, this, &PageManagerWidget::onInsertPage);

    auto actDup = menu.addAction(tr("ページを複製"));
    connect(actDup, &QAction::triggered, this, &PageManagerWidget::onDuplicatePage);

    auto actDel = menu.addAction(tr("ページを削除"));
    connect(actDel, &QAction::triggered, this, &PageManagerWidget::onDeletePage);

    menu.addSeparator();

    // 見開き化 / 単ページ化
    const auto& item = m_project->page(index);
    if (item.isSpread) {
        auto actSplit = menu.addAction(tr("単ページに分割"));
        connect(actSplit, &QAction::triggered, this, &PageManagerWidget::onSplitSpread);
    } else {
        if (index + 1 < m_project->pageCount() && !m_project->page(index + 1).isSpread) {
            auto actCombine = menu.addAction(tr("次のページと見開きに変更"));
            connect(actCombine, &QAction::triggered, this, &PageManagerWidget::onCombineToSpread);
        }
    }

    menu.addSeparator();

    auto actImport = menu.addAction(tr("外部ファイルをこのページに読み込み/差し替え..."));
    connect(actImport, &QAction::triggered, this, &PageManagerWidget::onImportPageFile);

    auto actRealign = menu.addAction(tr("ページファイル名の整列"));
    connect(actRealign, &QAction::triggered, this, &PageManagerWidget::onRealignFileNames);

    menu.addSeparator();

    auto actExport = menu.addAction(tr("一括書き出し..."));
    connect(actExport, &QAction::triggered, this, &PageManagerWidget::requestBatchExport);

    menu.exec(globalPos);
}

void PageManagerWidget::onPageMoved(int fromIndex, int toIndex) {
    if (!m_project) return;
    if (m_project->movePage(fromIndex, toIndex)) {
        refreshPageList();
        emit projectModified();
    }
}

void PageManagerWidget::onAddPage() {
    if (!m_project) return;
    QString err;
    if (m_project->addPage(QString(), &err)) {
        refreshPageList();
        emit projectModified();
    } else {
        QMessageBox::warning(this, tr("エラー"), tr("ページの追加に失敗しました:\n%1").arg(err));
    }
}

void PageManagerWidget::onInsertPage() {
    if (!m_project) return;
    int targetIdx = (m_selectedPageIndex >= 0) ? m_selectedPageIndex : m_project->pageCount();
    QString err;
    if (m_project->insertPage(targetIdx, QString(), &err)) {
        refreshPageList();
        emit projectModified();
    } else {
        QMessageBox::warning(this, tr("エラー"), tr("ページの挿入に失敗しました:\n%1").arg(err));
    }
}

void PageManagerWidget::onDuplicatePage() {
    if (!m_project || m_selectedPageIndex < 0) return;
    QString err;
    if (m_project->duplicatePage(m_selectedPageIndex, &err)) {
        refreshPageList();
        emit projectModified();
    } else {
        QMessageBox::warning(this, tr("エラー"), tr("ページの複製に失敗しました:\n%1").arg(err));
    }
}

void PageManagerWidget::onDeletePage() {
    if (!m_project || m_selectedPageIndex < 0) return;
    if (m_project->pageCount() <= 1) {
        QMessageBox::warning(this, tr("警告"), tr("最後の1ページは削除できません。"));
        return;
    }

    auto reply = QMessageBox::question(
        this,
        tr("ページの削除"),
        tr("ページ %1 を作品から削除しますか？\n（ファイルも完全に削除する場合は「はい」を選択してください）").arg(m_selectedPageIndex + 1),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
        QMessageBox::No
    );

    if (reply == QMessageBox::Cancel) return;
    bool deleteFromDisk = (reply == QMessageBox::Yes);

    QString err;
    if (m_project->deletePage(m_selectedPageIndex, deleteFromDisk, &err)) {
        m_selectedPageIndex = -1;
        refreshPageList();
        emit projectModified();
    } else {
        QMessageBox::warning(this, tr("エラー"), tr("ページの削除に失敗しました:\n%1").arg(err));
    }
}

void PageManagerWidget::onCombineToSpread() {
    if (!m_project || m_selectedPageIndex < 0 || m_selectedPageIndex + 1 >= m_project->pageCount()) return;

    auto reply = QMessageBox::question(
        this,
        tr("見開きに変更"),
        tr("ページ %1 とページ %2 を結合して見開きページにしますか？\n（※2枚のキャンバスが横並びに結合されます）").arg(m_selectedPageIndex + 1).arg(m_selectedPageIndex + 2),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes
    );
    if (reply != QMessageBox::Yes) return;

    QString err;
    if (m_project->combineToSpread(m_selectedPageIndex, &err)) {
        refreshPageList();
        emit projectModified();
    } else {
        QMessageBox::warning(this, tr("エラー"), tr("見開き化に失敗しました:\n%1").arg(err));
    }
}

void PageManagerWidget::onSplitSpread() {
    if (!m_project || m_selectedPageIndex < 0) return;

    auto reply = QMessageBox::question(
        this,
        tr("単ページに分割"),
        tr("この見開きページを左右の単ページに分割しますか？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes
    );
    if (reply != QMessageBox::Yes) return;

    QString err;
    if (m_project->splitSpread(m_selectedPageIndex, &err)) {
        refreshPageList();
        emit projectModified();
    } else {
        QMessageBox::warning(this, tr("エラー"), tr("単ページ分割に失敗しました:\n%1").arg(err));
    }
}

void PageManagerWidget::onImportPageFile() {
    if (!m_project || m_selectedPageIndex < 0) return;

    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("読み込むファイルを選択"),
        m_project->projectDir(),
        tr("すべての対応ファイル (*.kab *.png *.jpg *.jpeg *.bmp *.psd);;KABファイル (*.kab);;画像ファイル (*.png *.jpg *.bmp *.psd)")
    );

    if (filePath.isEmpty()) return;

    QString err;
    if (m_project->replacePage(m_selectedPageIndex, filePath, &err)) {
        refreshPageList();
        emit projectModified();
    } else {
        QMessageBox::warning(this, tr("エラー"), tr("ファイルの読み込みに失敗しました:\n%1").arg(err));
    }
}

void PageManagerWidget::onRealignFileNames() {
    if (!m_project) return;

    auto reply = QMessageBox::question(
        this,
        tr("ページファイル名の整列"),
        tr("作品フォルダ内のページファイル名を、現在のページ順序に従って連番\n(page0001.kab, page0002.kab ...) にリネームしますか？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes
    );
    if (reply != QMessageBox::Yes) return;

    QString err;
    if (m_project->realignPageFileNames(&err)) {
        refreshPageList();
        emit projectModified();
        QMessageBox::information(this, tr("完了"), tr("ページファイル名を連番に整列しました。"));
    } else {
        QMessageBox::warning(this, tr("エラー"), tr("整列に失敗しました:\n%1").arg(err));
    }
}

} // namespace Kaburagi
