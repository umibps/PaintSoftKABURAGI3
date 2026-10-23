#include "PageProject.hpp"
#include "Document.hpp"
#include "DocumentSerializer.hpp"
#include "Layer.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPainter>
#include <algorithm>
#include <cmath>

namespace Kaburagi {

PageProject::PageProject() = default;

void PageProject::setProjectFilePath(const QString& path) {
    m_projectFilePath = QFileInfo(path).absoluteFilePath();
}

QString PageProject::projectDir() const {
    if (m_projectFilePath.isEmpty()) return QString();
    return QFileInfo(m_projectFilePath).absolutePath();
}

QString PageProject::pageFullPath(int index) const {
    if (index < 0 || index >= static_cast<int>(m_pages.size())) return QString();
    return QDir(projectDir()).filePath(m_pages[index].fileName);
}

void PageProject::invalidateThumbnail(int index) {
    if (index >= 0 && index < static_cast<int>(m_pages.size())) {
        m_pages[index].thumbnailCache = QImage();
    }
}

QImage PageProject::pageThumbnail(int index, const QSize& targetSize) const {
    if (index < 0 || index >= static_cast<int>(m_pages.size())) {
        return QImage();
    }

    const auto& item = m_pages[index];
    if (!item.thumbnailCache.isNull()) {
        return item.thumbnailCache;
    }

    QString fullPath = pageFullPath(index);
    QImage thumb;

    if (QFile::exists(fullPath)) {
        if (fullPath.endsWith(".kab", Qt::CaseInsensitive)) {
            thumb = DocumentSerializer::extractThumbnail(fullPath);
            if (thumb.isNull()) {
                // サムネイル抽出不可の場合はDocumentを直接ロードして合成
                auto doc = std::unique_ptr<Document>(DocumentSerializer::loadFromFile(fullPath));
                if (doc) {
                    thumb = doc->renderToImage();
                }
            }
        } else {
            thumb.load(fullPath);
        }
    }

    if (thumb.isNull()) {
        // プレースホルダーサムネイル (白背景＋枠線＋ページ番号)
        int w = item.isSpread ? targetSize.width() * 2 : targetSize.width();
        int h = targetSize.height();
        thumb = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
        thumb.fill(QColor(245, 245, 245));

        QPainter p(&thumb);
        p.setPen(QColor(180, 180, 180));
        p.drawRect(0, 0, w - 1, h - 1);
        p.setPen(QColor(100, 100, 100));
        p.setFont(QFont("sans-serif", 12, QFont::Bold));
        p.drawText(QRect(0, 0, w, h), Qt::AlignCenter, item.title.isEmpty() ? QString("%1p").arg(index + 1) : item.title);
    } else {
        int targetW = item.isSpread ? targetSize.width() * 2 : targetSize.width();
        thumb = thumb.scaled(targetW, targetSize.height(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    item.thumbnailCache = thumb;
    return thumb;
}

QString PageProject::generateUniquePageFileName(const QString& basePrefix) const {
    QDir dir(projectDir());
    int num = 1;
    while (true) {
        QString candidate = QString("%1%2.kab").arg(basePrefix).arg(num, 4, 10, QChar('0'));
        bool existsInPages = false;
        for (const auto& p : m_pages) {
            if (p.fileName.compare(candidate, Qt::CaseInsensitive) == 0) {
                existsInPages = true;
                break;
            }
        }
        if (!existsInPages && !dir.exists(candidate)) {
            return candidate;
        }
        num++;
    }
}

void PageProject::updateDefaultTitles() {
    int currentP = 1;
    for (size_t i = 0; i < m_pages.size(); ++i) {
        if (m_pages[i].isSpread) {
            m_pages[i].title = QString("%1-%2p").arg(currentP).arg(currentP + 1);
            currentP += 2;
        } else {
            m_pages[i].title = QString("%1p").arg(currentP);
            currentP += 1;
        }
    }
}

bool PageProject::addPage(const QString& sourceFile, QString* outError) {
    return insertPage(pageCount(), sourceFile, outError);
}

bool PageProject::insertPage(int index, const QString& sourceFile, QString* outError) {
    if (index < 0) index = 0;
    if (index > pageCount()) index = pageCount();

    QString newFileName = generateUniquePageFileName("page");
    QString targetPath = QDir(projectDir()).filePath(newFileName);

    if (sourceFile.isEmpty()) {
        // 新規白紙ドキュメントを作成して保存
        auto doc = std::make_unique<Document>(m_standardWidth, m_standardHeight, m_standardDpi, true, Qt::white);
        if (!DocumentSerializer::saveToFile(doc.get(), targetPath, outError)) {
            return false;
        }
    } else {
        if (sourceFile.endsWith(".kab", Qt::CaseInsensitive)) {
            if (!QFile::copy(sourceFile, targetPath)) {
                if (outError) *outError = QString("Failed to copy .kab file to %1").arg(targetPath);
                return false;
            }
        } else {
            // 外部画像ファイル (PNG, JPG等) をロードして.kabとして変換保存
            QImage img(sourceFile);
            if (img.isNull()) {
                if (outError) *outError = QString("Failed to load image: %1").arg(sourceFile);
                return false;
            }
            auto doc = std::make_unique<Document>(img.width(), img.height(), m_standardDpi, false);
            auto layer = doc->addRasterLayer("Layer 1");
            if (layer) {
                auto& tileMgr = layer->tileManager();
                for (int y = 0; y < img.height(); ++y) {
                    const QRgb* scan = reinterpret_cast<const QRgb*>(img.constScanLine(y));
                    for (int x = 0; x < img.width(); ++x) {
                        tileMgr.setPixel(x, y, scan[x]);
                    }
                }
            }
            if (!DocumentSerializer::saveToFile(doc.get(), targetPath, outError)) {
                return false;
            }
        }
    }

    PageItem item;
    item.fileName = newFileName;
    item.isSpread = false;
    item.spreadPageCount = 1;

    m_pages.insert(m_pages.begin() + index, item);
    updateDefaultTitles();
    m_isDirty = true;
    return true;
}

bool PageProject::duplicatePage(int index, QString* outError) {
    if (index < 0 || index >= pageCount()) {
        if (outError) *outError = "Invalid page index";
        return false;
    }

    QString srcPath = pageFullPath(index);
    QString newFileName = generateUniquePageFileName("page");
    QString targetPath = QDir(projectDir()).filePath(newFileName);

    if (!QFile::copy(srcPath, targetPath)) {
        if (outError) *outError = QString("Failed to copy file to %1").arg(targetPath);
        return false;
    }

    PageItem item = m_pages[index];
    item.fileName = newFileName;
    item.thumbnailCache = QImage();

    m_pages.insert(m_pages.begin() + index + 1, item);
    updateDefaultTitles();
    m_isDirty = true;
    return true;
}

bool PageProject::deletePage(int index, bool deleteFromDisk, QString* outError) {
    if (index < 0 || index >= pageCount()) {
        if (outError) *outError = "Invalid page index";
        return false;
    }

    if (deleteFromDisk) {
        QString path = pageFullPath(index);
        if (QFile::exists(path)) {
            QFile::remove(path);
        }
    }

    m_pages.erase(m_pages.begin() + index);
    updateDefaultTitles();
    m_isDirty = true;
    return true;
}

bool PageProject::movePage(int fromIndex, int toIndex) {
    if (fromIndex < 0 || fromIndex >= pageCount() || toIndex < 0 || toIndex >= pageCount() || fromIndex == toIndex) {
        return false;
    }

    PageItem item = m_pages[fromIndex];
    m_pages.erase(m_pages.begin() + fromIndex);
    m_pages.insert(m_pages.begin() + toIndex, item);

    updateDefaultTitles();
    m_isDirty = true;
    return true;
}

bool PageProject::replacePage(int index, const QString& sourceFile, QString* outError) {
    if (index < 0 || index >= pageCount()) {
        if (outError) *outError = "Invalid page index";
        return false;
    }

    QString targetPath = pageFullPath(index);

    if (sourceFile.endsWith(".kab", Qt::CaseInsensitive)) {
        if (QFile::exists(targetPath)) QFile::remove(targetPath);
        if (!QFile::copy(sourceFile, targetPath)) {
            if (outError) *outError = QString("Failed to copy file to %1").arg(targetPath);
            return false;
        }
    } else {
        QImage img(sourceFile);
        if (img.isNull()) {
            if (outError) *outError = QString("Failed to load image: %1").arg(sourceFile);
            return false;
        }
        auto doc = std::make_unique<Document>(img.width(), img.height(), m_standardDpi, false);
        auto layer = doc->addRasterLayer("Layer 1");
        if (layer) {
            auto& tileMgr = layer->tileManager();
            for (int y = 0; y < img.height(); ++y) {
                const QRgb* scan = reinterpret_cast<const QRgb*>(img.constScanLine(y));
                for (int x = 0; x < img.width(); ++x) {
                    tileMgr.setPixel(x, y, scan[x]);
                }
            }
        }
        if (QFile::exists(targetPath)) QFile::remove(targetPath);
        if (!DocumentSerializer::saveToFile(doc.get(), targetPath, outError)) {
            return false;
        }
    }

    invalidateThumbnail(index);
    m_isDirty = true;
    return true;
}

bool PageProject::realignPageFileNames(QString* outError) {
    QDir dir(projectDir());

    // ステップ 1: 一時的なプレフィックス付きファイル名にリネーム（ファイル名の重複衝突を防ぐ）
    std::vector<QString> tempNames;
    tempNames.reserve(m_pages.size());

    for (size_t i = 0; i < m_pages.size(); ++i) {
        QString currentName = m_pages[i].fileName;
        QString tempName = QString("__tmp_realign_%1_%2").arg(i).arg(currentName);
        tempNames.push_back(tempName);

        if (dir.exists(currentName)) {
            if (!dir.rename(currentName, tempName)) {
                if (outError) *outError = QString("Failed to rename %1 to %2").arg(currentName, tempName);
                return false;
            }
        }
    }

    // ステップ 2: 連番（page0001.kab, page0002.kab ...）の正規ファイル名にリネーム
    for (size_t i = 0; i < m_pages.size(); ++i) {
        QString targetName = QString("page%1.kab").arg(i + 1, 4, 10, QChar('0'));
        QString tempName = tempNames[i];

        if (dir.exists(tempName)) {
            if (dir.exists(targetName)) {
                dir.remove(targetName);
            }
            if (!dir.rename(tempName, targetName)) {
                if (outError) *outError = QString("Failed to rename %1 to %2").arg(tempName, targetName);
                return false;
            }
        }
        m_pages[i].fileName = targetName;
        invalidateThumbnail(static_cast<int>(i));
    }

    updateDefaultTitles();
    m_isDirty = true;
    return saveToFile(QString(), outError);
}

bool PageProject::combineToSpread(int firstPageIndex, QString* outError) {
    if (firstPageIndex < 0 || firstPageIndex + 1 >= pageCount()) {
        if (outError) *outError = "Invalid page pair for spread combination";
        return false;
    }

    if (m_pages[firstPageIndex].isSpread || m_pages[firstPageIndex + 1].isSpread) {
        if (outError) *outError = "One of the pages is already a spread";
        return false;
    }

    QString path1 = pageFullPath(firstPageIndex);
    QString path2 = pageFullPath(firstPageIndex + 1);

    auto doc1 = std::unique_ptr<Document>(DocumentSerializer::loadFromFile(path1, nullptr, outError));
    if (!doc1) return false;

    auto doc2 = std::unique_ptr<Document>(DocumentSerializer::loadFromFile(path2, nullptr, outError));
    if (!doc2) return false;

    int spreadW = doc1->width() + doc2->width();
    int spreadH = std::max(doc1->height(), doc2->height());

    auto spreadDoc = std::make_unique<Document>(spreadW, spreadH, doc1->dpi(), true, Qt::white);
    auto spreadLayer = spreadDoc->addRasterLayer("Spread Content");

    QImage img1 = doc1->renderToImage();
    QImage img2 = doc2->renderToImage();

    // 綴じ方向に応じた配置:
    // RightToLeft (右綴じ): 前のページ(doc1)が右側、後のページ(doc2)が左側
    // LeftToRight (左綴じ): 前のページ(doc1)が左側、後のページ(doc2)が右側
    int x1 = (m_bindingDirection == BindingDirection::RightToLeft) ? doc2->width() : 0;
    int x2 = (m_bindingDirection == BindingDirection::RightToLeft) ? 0 : doc1->width();

    if (spreadLayer) {
        auto& tileMgr = spreadLayer->tileManager();
        // doc1を描画
        for (int y = 0; y < img1.height() && y < spreadH; ++y) {
            const QRgb* scan = reinterpret_cast<const QRgb*>(img1.constScanLine(y));
            for (int x = 0; x < img1.width() && (x + x1) < spreadW; ++x) {
                tileMgr.setPixel(x + x1, y, scan[x]);
            }
        }
        // doc2を描画
        for (int y = 0; y < img2.height() && y < spreadH; ++y) {
            const QRgb* scan = reinterpret_cast<const QRgb*>(img2.constScanLine(y));
            for (int x = 0; x < img2.width() && (x + x2) < spreadW; ++x) {
                tileMgr.setPixel(x + x2, y, scan[x]);
            }
        }
    }

    // 見開きドキュメントを先頭ページのファイルへ保存
    if (!DocumentSerializer::saveToFile(spreadDoc.get(), path1, outError)) {
        return false;
    }

    // 2枚目のファイルを削除
    if (QFile::exists(path2)) {
        QFile::remove(path2);
    }

    m_pages[firstPageIndex].isSpread = true;
    m_pages[firstPageIndex].spreadPageCount = 2;
    invalidateThumbnail(firstPageIndex);

    // 2枚目のエントリを削除
    m_pages.erase(m_pages.begin() + firstPageIndex + 1);

    updateDefaultTitles();
    m_isDirty = true;
    return true;
}

bool PageProject::splitSpread(int spreadIndex, QString* outError) {
    if (spreadIndex < 0 || spreadIndex >= pageCount()) {
        if (outError) *outError = "Invalid spread index";
        return false;
    }

    if (!m_pages[spreadIndex].isSpread) {
        if (outError) *outError = "The selected page is not a spread";
        return false;
    }

    QString spreadPath = pageFullPath(spreadIndex);
    auto spreadDoc = std::unique_ptr<Document>(DocumentSerializer::loadFromFile(spreadPath, nullptr, outError));
    if (!spreadDoc) return false;

    int halfW = spreadDoc->width() / 2;
    int h = spreadDoc->height();
    QImage fullImg = spreadDoc->renderToImage();

    QImage leftImg = fullImg.copy(0, 0, halfW, h);
    QImage rightImg = fullImg.copy(halfW, 0, halfW, h);

    // 綴じ方向に応じた分割:
    // RightToLeft (右綴じ): 前のページ(1枚目)は右半分、後のページ(2枚目)は左半分
    // LeftToRight (左綴じ): 前のページ(1枚目)は左半分、後のページ(2枚目)は右半分
    QImage firstImg = (m_bindingDirection == BindingDirection::RightToLeft) ? rightImg : leftImg;
    QImage secondImg = (m_bindingDirection == BindingDirection::RightToLeft) ? leftImg : rightImg;

    // 1枚目ドキュメント保存
    auto doc1 = std::make_unique<Document>(halfW, h, spreadDoc->dpi(), false);
    auto l1 = doc1->addRasterLayer("Layer 1");
    if (l1) {
        auto& tm = l1->tileManager();
        for (int y = 0; y < h; ++y) {
            const QRgb* scan = reinterpret_cast<const QRgb*>(firstImg.constScanLine(y));
            for (int x = 0; x < halfW; ++x) tm.setPixel(x, y, scan[x]);
        }
    }
    if (!DocumentSerializer::saveToFile(doc1.get(), spreadPath, outError)) return false;

    // 2枚目ドキュメント保存
    QString newFileName = generateUniquePageFileName("page");
    QString secondPath = QDir(projectDir()).filePath(newFileName);
    auto doc2 = std::make_unique<Document>(halfW, h, spreadDoc->dpi(), false);
    auto l2 = doc2->addRasterLayer("Layer 1");
    if (l2) {
        auto& tm = l2->tileManager();
        for (int y = 0; y < h; ++y) {
            const QRgb* scan = reinterpret_cast<const QRgb*>(secondImg.constScanLine(y));
            for (int x = 0; x < halfW; ++x) tm.setPixel(x, y, scan[x]);
        }
    }
    if (!DocumentSerializer::saveToFile(doc2.get(), secondPath, outError)) return false;

    m_pages[spreadIndex].isSpread = false;
    m_pages[spreadIndex].spreadPageCount = 1;
    invalidateThumbnail(spreadIndex);

    PageItem item2;
    item2.fileName = newFileName;
    item2.isSpread = false;
    item2.spreadPageCount = 1;
    m_pages.insert(m_pages.begin() + spreadIndex + 1, item2);

    updateDefaultTitles();
    m_isDirty = true;
    return true;
}

bool PageProject::saveToFile(const QString& filePath, QString* outError) {
    QString savePath = filePath.isEmpty() ? m_projectFilePath : filePath;
    if (savePath.isEmpty()) {
        if (outError) *outError = "Project file path is empty";
        return false;
    }

    QJsonObject root;
    root["format"] = "KABURAGI_PAGE_PROJECT";
    root["version"] = 1;
    root["name"] = m_name;
    root["standardWidth"] = m_standardWidth;
    root["standardHeight"] = m_standardHeight;
    root["standardDpi"] = m_standardDpi;
    root["bindingDirection"] = (m_bindingDirection == BindingDirection::RightToLeft) ? "RightToLeft" : "LeftToRight";
    root["startFromRight"] = m_startFromRight;

    QJsonArray pageArr;
    for (const auto& item : m_pages) {
        QJsonObject pObj;
        pObj["fileName"] = item.fileName;
        pObj["title"] = item.title;
        pObj["isSpread"] = item.isSpread;
        pObj["spreadPageCount"] = item.spreadPageCount;
        pageArr.append(pObj);
    }
    root["pages"] = pageArr;

    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (outError) *outError = QString("Cannot open file for writing: %1").arg(savePath);
        return false;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    m_projectFilePath = QFileInfo(savePath).absoluteFilePath();
    m_isDirty = false;
    return true;
}

std::shared_ptr<PageProject> PageProject::loadFromFile(const QString& filePath, QString* outError) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (outError) *outError = QString("Cannot open file for reading: %1").arg(filePath);
        return nullptr;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        if (outError) *outError = "Invalid JSON structure in .kap file";
        return nullptr;
    }

    QJsonObject root = doc.object();
    if (root.value("format").toString() != "KABURAGI_PAGE_PROJECT") {
        if (outError) *outError = "Unrecognized project format. Expected KABURAGI_PAGE_PROJECT.";
        return nullptr;
    }

    auto proj = std::make_shared<PageProject>();
    proj->setProjectFilePath(filePath);
    proj->m_name = root.value("name").toString("Untitled");
    proj->m_standardWidth = root.value("standardWidth").toInt(2480);
    proj->m_standardHeight = root.value("standardHeight").toInt(3508);
    proj->m_standardDpi = root.value("standardDpi").toDouble(350.0);

    QString bDir = root.value("bindingDirection").toString("RightToLeft");
    proj->m_bindingDirection = (bDir == "LeftToRight") ? BindingDirection::LeftToRight : BindingDirection::RightToLeft;
    proj->m_startFromRight = root.value("startFromRight").toBool(true);

    QJsonArray pageArr = root.value("pages").toArray();
    for (const auto& val : pageArr) {
        QJsonObject pObj = val.toObject();
        PageItem item;
        item.fileName = pObj.value("fileName").toString();
        item.title = pObj.value("title").toString();
        item.isSpread = pObj.value("isSpread").toBool(false);
        item.spreadPageCount = pObj.value("spreadPageCount").toInt(item.isSpread ? 2 : 1);
        proj->m_pages.push_back(item);
    }

    proj->m_isDirty = false;
    return proj;
}

std::shared_ptr<PageProject> PageProject::createNewProject(
    const QString& dirPath,
    const QString& projectName,
    int pageCount,
    int width,
    int height,
    double dpi,
    BindingDirection direction,
    bool startFromRight,
    QString* outError
) {
    QDir dir(dirPath);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            if (outError) *outError = QString("Failed to create directory: %1").arg(dirPath);
            return nullptr;
        }
    }

    auto proj = std::make_shared<PageProject>();
    QString kapPath = dir.filePath(projectName + ".kap");
    proj->setProjectFilePath(kapPath);
    proj->setName(projectName);
    proj->setStandardWidth(width);
    proj->setStandardHeight(height);
    proj->setStandardDpi(dpi);
    proj->setBindingDirection(direction);
    proj->setStartFromRight(startFromRight);

    for (int i = 0; i < pageCount; ++i) {
        if (!proj->addPage(QString(), outError)) {
            return nullptr;
        }
    }

    if (!proj->saveToFile(kapPath, outError)) {
        return nullptr;
    }

    return proj;
}

} // namespace Kaburagi
