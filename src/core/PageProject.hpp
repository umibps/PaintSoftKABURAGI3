#pragma once

#include <QString>
#include <QImage>
#include <vector>
#include <memory>

namespace Kaburagi {

class Document;

enum class BindingDirection {
    RightToLeft, // 右綴じ (右開き・縦組み漫画/同人誌)
    LeftToRight  // 左綴じ (左開き・横組み書籍/イラスト集)
};

struct PageItem {
    QString fileName;       // 相対ファイル名 (例: "page0001.kab")
    QString title;          // 表示名 (例: "1p", "表紙")
    bool isSpread = false;  // 見開きページかどうか
    int spreadPageCount = 1;// ページ換算数 (通常1, 見開きなら2)

    // キャッシュ用サムネイル (必要に応じて遅延ロード)
    mutable QImage thumbnailCache;
};

/**
 * @brief 複数ページマンガ・作品管理 (.kap) クラス
 */
class PageProject {
public:
    PageProject();
    ~PageProject() = default;

    // --- プロパティ ---
    const QString& projectFilePath() const { return m_projectFilePath; }
    void setProjectFilePath(const QString& path);

    QString projectDir() const;
    const QString& name() const { return m_name; }
    void setName(const QString& name) { m_name = name; m_isDirty = true; }

    int standardWidth() const { return m_standardWidth; }
    void setStandardWidth(int w) { m_standardWidth = w; m_isDirty = true; }

    int standardHeight() const { return m_standardHeight; }
    void setStandardHeight(int h) { m_standardHeight = h; m_isDirty = true; }

    double standardDpi() const { return m_standardDpi; }
    void setStandardDpi(double dpi) { m_standardDpi = dpi; m_isDirty = true; }

    BindingDirection bindingDirection() const { return m_bindingDirection; }
    void setBindingDirection(BindingDirection dir) { m_bindingDirection = dir; m_isDirty = true; }

    bool startFromRight() const { return m_startFromRight; }
    void setStartFromRight(bool val) { m_startFromRight = val; m_isDirty = true; }

    bool isDirty() const { return m_isDirty; }
    void setDirty(bool dirty) { m_isDirty = dirty; }

    // --- ページ管理 ---
    int pageCount() const { return static_cast<int>(m_pages.size()); }
    const PageItem& page(int index) const { return m_pages.at(index); }
    PageItem& page(int index) { m_isDirty = true; return m_pages.at(index); }
    const std::vector<PageItem>& pages() const { return m_pages; }

    QString pageFullPath(int index) const;
    QImage pageThumbnail(int index, const QSize& targetSize = QSize(120, 160)) const;
    void invalidateThumbnail(int index);

    // ページ操作
    bool addPage(const QString& sourceFile = QString(), QString* outError = nullptr);
    bool insertPage(int index, const QString& sourceFile = QString(), QString* outError = nullptr);
    bool duplicatePage(int index, QString* outError = nullptr);
    bool deletePage(int index, bool deleteFromDisk = false, QString* outError = nullptr);
    bool movePage(int fromIndex, int toIndex);
    bool replacePage(int index, const QString& sourceFile, QString* outError = nullptr);

    // ファイル名の整列 (page0001.kab, page0002.kab ... へ連番リネーム)
    bool realignPageFileNames(QString* outError = nullptr);

    // 見開き化 / 単ページ化
    bool combineToSpread(int firstPageIndex, QString* outError = nullptr);
    bool splitSpread(int spreadIndex, QString* outError = nullptr);

    // 保存 / 読み込み
    bool saveToFile(const QString& filePath = QString(), QString* outError = nullptr);
    static std::shared_ptr<PageProject> loadFromFile(const QString& filePath, QString* outError = nullptr);

    // 新規作品一括生成
    static std::shared_ptr<PageProject> createNewProject(
        const QString& dirPath,
        const QString& projectName,
        int pageCount,
        int width,
        int height,
        double dpi,
        BindingDirection direction,
        bool startFromRight,
        QString* outError = nullptr
    );

private:
    QString generateUniquePageFileName(const QString& basePrefix = "page") const;
    void updateDefaultTitles();

    QString m_projectFilePath;
    QString m_name = "Untitled";
    int m_standardWidth = 2480;
    int m_standardHeight = 3508;
    double m_standardDpi = 350.0;
    BindingDirection m_bindingDirection = BindingDirection::RightToLeft;
    bool m_startFromRight = true;
    bool m_isDirty = false;

    std::vector<PageItem> m_pages;
};

} // namespace Kaburagi
