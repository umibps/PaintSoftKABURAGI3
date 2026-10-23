#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolBar>
#include <QMenu>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <memory>
#include "PageProject.hpp"

namespace Kaburagi {

class PageCardWidget : public QWidget {
    Q_OBJECT
public:
    PageCardWidget(int pageIndex, const PageItem& item, const QImage& thumbnail, bool isCurrent, QWidget* parent = nullptr);

    int pageIndex() const { return m_pageIndex; }
    void setCurrent(bool current);

signals:
    void clicked(int pageIndex);
    void doubleClicked(int pageIndex);
    void contextMenuRequested(int pageIndex, const QPoint& globalPos);
    void pageMoveRequested(int fromIndex, int toIndex);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    int m_pageIndex = 0;
    bool m_isCurrent = false;
    QPoint m_dragStartPos;
    QLabel* m_thumbLabel = nullptr;
    QLabel* m_titleLabel = nullptr;
};

class PageManagerWidget : public QWidget {
    Q_OBJECT
public:
    explicit PageManagerWidget(QWidget* parent = nullptr);
    ~PageManagerWidget() override = default;

    void setProject(std::shared_ptr<PageProject> project);
    std::shared_ptr<PageProject> project() const { return m_project; }

    void refreshPageList();
    void setCurrentPagePath(const QString& fullPath);

signals:
    void pageOpenRequested(int index, const QString& fullPath);
    void projectModified();
    void requestNewProject();
    void requestBatchExport();

public slots:
    void onAddPage();
    void onInsertPage();
    void onDuplicatePage();
    void onDeletePage();
    void onCombineToSpread();
    void onSplitSpread();
    void onImportPageFile();
    void onRealignFileNames();

private slots:
    void onCardClicked(int index);
    void onCardDoubleClicked(int index);
    void onCardContextMenu(int index, const QPoint& globalPos);
    void onPageMoved(int fromIndex, int toIndex);

private:
    void setupUI();
    void buildGridLayout();

    std::shared_ptr<PageProject> m_project;
    int m_selectedPageIndex = -1;
    QString m_currentOpenPath;

    QToolBar* m_toolBar = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_gridContainer = nullptr;
    QGridLayout* m_gridLayout = nullptr;

    std::vector<PageCardWidget*> m_cardWidgets;
};

} // namespace Kaburagi
