#pragma once

#include <QString>
#include <QImage>
#include <memory>

namespace Kaburagi {

class Document;

class DocumentSerializer {
public:
    static constexpr quint32 KAB_MAGIC = 0x4B414255; // 'KABU'
    static constexpr quint32 KAB_VERSION = 5;

    static bool saveToFile(const Document* document, const QString& filePath, QString* outErrorMessage = nullptr);
    static Document* loadFromFile(const QString& filePath, QObject* parent = nullptr, QString* outErrorMessage = nullptr);

    // .kab ファイルから埋め込みサムネイルを高速抽出
    static QImage extractThumbnail(const QString& filePath, QString* outErrorMessage = nullptr);

    // 画像ファイルエクスポート (PNG / JPEG / BMP / TIFF / WebP / PSD)
    static bool exportImage(const Document* document, const QString& filePath, QString* outErrorMessage = nullptr);
};

} // namespace Kaburagi
