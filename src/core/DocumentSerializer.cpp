#include "DocumentSerializer.hpp"
#include "Document.hpp"
#include "Layer.hpp"
#include "VectorLayer.hpp"
#include "AdjustmentLayer.hpp"
#include "TextLayer.hpp"
#include "GroupLayer.hpp"
#include "MemoryPool.hpp"
#include <QFile>
#include <QDataStream>
#include <QByteArray>
#include <QBuffer>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace Kaburagi {

static bool writePsdFile(const QImage& sourceImage, const QString& filePath, double dpi, QString* outErrorMessage) {
    QImage img = sourceImage.convertToFormat(QImage::Format_ARGB32);
    int w = img.width();
    int h = img.height();

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (outErrorMessage) *outErrorMessage = file.errorString();
        return false;
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::BigEndian);

    // 1. Signature "8BPS"
    stream.writeRawData("8BPS", 4);
    // Version = 1
    stream << static_cast<quint16>(1);
    // 6 bytes reserved
    stream.writeRawData("\0\0\0\0\0\0", 6);
    // Channels = 4 (R, G, B, A)
    stream << static_cast<quint16>(4);
    // Height & Width
    stream << static_cast<quint32>(h);
    stream << static_cast<quint32>(w);
    // Depth = 8
    stream << static_cast<quint16>(8);
    // Color Mode = 3 (RGB)
    stream << static_cast<quint16>(3);

    // 2. Color Mode Data: length 0
    stream << static_cast<quint32>(0);

    // 3. Image Resources (ResolutionInfo DPI)
    QByteArray resBlock;
    {
        QDataStream resStream(&resBlock, QIODevice::WriteOnly);
        resStream.setByteOrder(QDataStream::BigEndian);
        resStream.writeRawData("8BIM", 4);
        resStream << static_cast<quint16>(0x03ED); // ResolutionInfo ID
        resStream << static_cast<quint16>(0);      // Empty Pascal string + pad
        resStream << static_cast<quint32>(16);     // Data size

        quint32 fixedDpi = static_cast<quint32>(dpi * 65536.0);
        resStream << fixedDpi;
        resStream << static_cast<quint16>(1); // hResUnit: 1 = pixels per inch
        resStream << static_cast<quint16>(1); // widthUnit: 1 = inches
        resStream << fixedDpi;
        resStream << static_cast<quint16>(1); // vResUnit: 1 = pixels per inch
        resStream << static_cast<quint16>(1); // heightUnit: 1 = inches
    }
    stream << static_cast<quint32>(resBlock.size());
    stream.writeRawData(resBlock.constData(), resBlock.size());

    // 4. Layer and Mask Information: length 0
    stream << static_cast<quint32>(0);

    // 5. Image Data: compression = 0 (raw)
    stream << static_cast<quint16>(0);

    // Planar channel data: R plane, G plane, B plane, A plane
    qint64 totalPixels = static_cast<qint64>(w) * h;
    QByteArray rPlane(totalPixels, Qt::Uninitialized);
    QByteArray gPlane(totalPixels, Qt::Uninitialized);
    QByteArray bPlane(totalPixels, Qt::Uninitialized);
    QByteArray aPlane(totalPixels, Qt::Uninitialized);

    char* rPtr = rPlane.data();
    char* gPtr = gPlane.data();
    char* bPtr = bPlane.data();
    char* aPtr = aPlane.data();

    for (int y = 0; y < h; ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        qint64 rowOffset = static_cast<qint64>(y) * w;
        for (int x = 0; x < w; ++x) {
            QRgb px = line[x];
            rPtr[rowOffset + x] = static_cast<char>(qRed(px));
            gPtr[rowOffset + x] = static_cast<char>(qGreen(px));
            bPtr[rowOffset + x] = static_cast<char>(qBlue(px));
            aPtr[rowOffset + x] = static_cast<char>(qAlpha(px));
        }
    }

    stream.writeRawData(rPlane.constData(), rPlane.size());
    stream.writeRawData(gPlane.constData(), gPlane.size());
    stream.writeRawData(bPlane.constData(), bPlane.size());
    stream.writeRawData(aPlane.constData(), aPlane.size());

    file.close();
    return true;
}

bool DocumentSerializer::saveToFile(const Document* document, const QString& filePath, QString* outErrorMessage) {
    if (!document) {
        if (outErrorMessage) *outErrorMessage = "ドキュメントが無効です。";
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (outErrorMessage) *outErrorMessage = QString("ファイルを開けませんでした: %1").arg(file.errorString());
        return false;
    }

    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_5);

    // 1. ヘッダー
    stream << static_cast<quint32>(KAB_MAGIC);
    stream << static_cast<quint32>(KAB_VERSION);

    // 2. 埋め込みサムネイル (PNG 256x256)
    QImage thumb = document->compositeBuffer().createThumbnail(256, 256);
    QByteArray thumbData;
    {
        QBuffer buf(&thumbData);
        buf.open(QIODevice::WriteOnly);
        thumb.save(&buf, "PNG");
    }
    stream << thumbData;

    // 3. ドキュメント基本情報
    stream << static_cast<qint32>(document->width());
    stream << static_cast<qint32>(document->height());
    stream << static_cast<double>(document->dpi());
    stream << static_cast<qint32>(document->activeLayerIndex());

    const auto& layers = document->layers();
    qint32 layerCount = static_cast<qint32>(layers.size());
    stream << layerCount;

    // 背景色と選択範囲
    stream << document->primaryBackgroundColor();
    stream << document->secondaryBackgroundColor();
    stream << document->isSecondaryBgColorActive();
    stream << document->selection();
    // Version 6: 非表示ベースレイヤーへのクリッピングマスク有効化フラグ
    stream << document->isClipToHiddenBaseLayerEnabled();

    // 4. 各レイヤーのシリアライズ
    for (qint32 i = 0; i < layerCount; ++i) {
        const auto& layer = layers[i];

        // 親グループのインデックスを特定
        qint32 parentGroupIdx = -1;
        auto parentGrp = layer->parentGroup();
        if (parentGrp) {
            for (qint32 j = 0; j < layerCount; ++j) {
                if (layers[j] == parentGrp) {
                    parentGroupIdx = j;
                    break;
                }
            }
        }

        // レイヤー共通属性
        stream << static_cast<qint32>(layer->type());
        stream << layer->name();
        stream << layer->isVisible();
        stream << layer->opacity();
        stream << static_cast<qint32>(layer->blendMode());
        stream << layer->isClippingMask();
        stream << layer->isAlphaLocked();
        stream << layer->isLocked();
        stream << parentGroupIdx;

        // Version 5: AI生成パラメータ (JSON)
        QByteArray aiJsonData = layer->hasAiParameters() ? QJsonDocument(layer->aiParameters()).toJson(QJsonDocument::Compact) : QByteArray();
        stream << aiJsonData;

        // レイヤー種別固有データ
        switch (layer->type()) {
        case LayerType::Raster: {
            const auto& tm = layer->tileManager();
            int tilesX = tm.tilesX();
            int tilesY = tm.tilesY();

            // 有効タイル数カウント
            qint32 validTileCount = 0;
            for (int ty = 0; ty < tilesY; ++ty) {
                for (int tx = 0; tx < tilesX; ++tx) {
                    if (tm.getTile(tx, ty) != nullptr) {
                        validTileCount++;
                    }
                }
            }

            stream << validTileCount;

            // 各タイルの座標と圧縮ピクセルデータ
            for (int ty = 0; ty < tilesY; ++ty) {
                for (int tx = 0; tx < tilesX; ++tx) {
                    const Tile* tile = tm.getTile(tx, ty);
                    if (tile != nullptr) {
                        stream << static_cast<qint32>(tx);
                        stream << static_cast<qint32>(ty);

                        const uint8_t* rawData = tile->readBuffer();
                        QByteArray compressed = qCompress(reinterpret_cast<const uchar*>(rawData),
                                                           static_cast<qsizetype>(TileMemoryPool::TILE_BYTE_SIZE));
                        stream << compressed;
                    }
                }
            }
            break;
        }
        case LayerType::Vector: {
            auto vecLayer = std::dynamic_pointer_cast<VectorLayer>(layer);
            const auto& strokes = vecLayer ? vecLayer->strokes() : std::vector<VectorStroke>();
            qint32 strokeCount = static_cast<qint32>(strokes.size());
            stream << strokeCount;

            for (const auto& stroke : strokes) {
                stream << stroke.baseWidth;
                stream << stroke.color;
                stream << stroke.density;
                stream << static_cast<qint32>(stroke.hardness);
                stream << static_cast<qint32>(stroke.falloff);
                stream << stroke.isClosed;
                stream << stroke.isFilled;

                qint32 ptCount = static_cast<qint32>(stroke.points.size());
                stream << ptCount;
                for (const auto& pt : stroke.points) {
                    stream << pt.x;
                    stream << pt.y;
                    stream << pt.pressure;
                    stream << pt.opacity;
                    stream << pt.isCorner;
                }
            }
            break;
        }
        case LayerType::Adjustment: {
            auto adjLayer = std::dynamic_pointer_cast<AdjustmentLayer>(layer);
            if (adjLayer) {
                stream << static_cast<qint32>(adjLayer->adjustmentType());
                stream << adjLayer->brightness();
                stream << adjLayer->contrast();
                stream << adjLayer->hueShift();
                stream << adjLayer->saturation();
                stream << adjLayer->lightness();
                stream << static_cast<qint32>(adjLayer->curveInterpolationMode());
                for (int ch = 0; ch < 4; ++ch) {
                    auto ach = static_cast<AdjustmentChannel>(ch);
                    const auto& pts = adjLayer->curvePoints(ach);
                    stream << static_cast<qint32>(pts.size());
                    for (const auto& pt : pts) {
                        stream << pt.x() << pt.y();
                    }
                    const auto& lp = adjLayer->levelParams(ach);
                    stream << lp.inShadow << lp.inGamma << lp.inHighlight << lp.outShadow << lp.outHighlight;
                }
            } else {
                stream << static_cast<qint32>(AdjustmentType::BrightnessContrast);
                stream << 0.0f << 0.0f << 0.0f << 1.0f << 0.0f;
                stream << static_cast<qint32>(0);
                for (int ch = 0; ch < 4; ++ch) {
                    stream << static_cast<qint32>(2) << 0.0 << 0.0 << 255.0 << 255.0;
                    stream << 0 << 1.0f << 255 << 0 << 255;
                }
            }
            break;
        }
        case LayerType::Group: {
            auto grpLayer = std::dynamic_pointer_cast<GroupLayer>(layer);
            if (grpLayer) {
                stream << grpLayer->isExpanded();
                stream << grpLayer->isPassThrough();
                stream << grpLayer->isPanelFolder();
                stream << grpLayer->panelPolygon();
                stream << grpLayer->panelBorderWidth();
                stream << grpLayer->panelBorderColor();
                stream << grpLayer->drawPanelBorder();
            } else {
                stream << true << false << false << QPolygonF() << 3.0f << QColor(Qt::black) << true;
            }
            break;
        }
        case LayerType::Text: {
            auto txtLayer = std::dynamic_pointer_cast<TextLayer>(layer);
            const auto& items = txtLayer ? txtLayer->items() : std::vector<TextBalloonItem>();
            qint32 itemCount = static_cast<qint32>(items.size());
            stream << itemCount;

            for (const auto& item : items) {
                stream << item.id;
                stream << item.rect;
                stream << item.text;
                stream << item.font;
                stream << item.textColor;
                stream << item.frameColor;
                stream << item.fillColor;
                stream << item.frameWidth;
                stream << static_cast<qint32>(item.shape);
                stream << item.hasTail;
                stream << item.tailTip;
                stream << static_cast<qint32>(item.horizontalAlign);
                stream << static_cast<qint32>(item.verticalAlign);
                stream << item.tailBaseWidth;
                stream << item.tailCurvature;
                stream << static_cast<qint32>(item.spikeCount);
                stream << item.spikeDepth;
                stream << item.spikePhase;
                stream << item.hasOutline;
                stream << item.outlineWidth;
                stream << item.outlineColor;
            }
            break;
        }
        }
    }

    file.close();
    return true;
}

Document* DocumentSerializer::loadFromFile(const QString& filePath, QObject* parent, QString* outErrorMessage) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (outErrorMessage) *outErrorMessage = QString("ファイルを開けませんでした: %1").arg(file.errorString());
        return nullptr;
    }

    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_5);

    // 1. ヘッダー検証
    quint32 magic = 0;
    stream >> magic;
    if (magic != KAB_MAGIC) {
        if (outErrorMessage) *outErrorMessage = "PaintSoft KABURAGI形式のファイル (.kab) ではありません。";
        return nullptr;
    }

    quint32 version = 0;
    stream >> version;
    if (version > KAB_VERSION) {
        if (outErrorMessage) *outErrorMessage = QString("サポートされていない新しいバージョンです (Version %1)").arg(version);
        return nullptr;
    }

    // 2. 埋め込みサムネイル読み飛ばし/取得
    QByteArray thumbData;
    stream >> thumbData;

    // 3. ドキュメント基本情報
    qint32 width = 0;
    qint32 height = 0;
    double dpi = 350.0;
    qint32 activeLayerIdx = 0;
    qint32 layerCount = 0;

    stream >> width >> height >> dpi >> activeLayerIdx >> layerCount;

    if (width <= 0 || height <= 0 || layerCount < 0) {
        if (outErrorMessage) *outErrorMessage = "ファイル内のキャンバス寸法またはレイヤー数が不正です。";
        return nullptr;
    }

    QColor priBgColor = Qt::white;
    QColor secBgColor = Qt::white;
    bool isSecActive = false;

    if (version >= 2) {
        stream >> priBgColor;
        stream >> secBgColor;
        stream >> isSecActive;
    } else {
        stream >> secBgColor;
        priBgColor = Qt::white;
        isSecActive = false;
    }

    QPainterPath selectionPath;
    stream >> selectionPath;

    bool clipToHiddenBase = true;
    if (version >= 6) {
        stream >> clipToHiddenBase;
    }

    // ドキュメント初期化 (デフォルトレイヤー作成を抑制)
    auto doc = new Document(width, height, dpi, false, priBgColor, parent);
    doc->setPrimaryBackgroundColor(priBgColor);
    doc->setSecondaryBackgroundColor(secBgColor);
    doc->setBackgroundColorActive(isSecActive);
    doc->setClipToHiddenBaseLayer(clipToHiddenBase);
    if (!selectionPath.isEmpty()) {
        doc->setSelection(selectionPath);
    }

    std::vector<qint32> parentGroupIndices;
    parentGroupIndices.reserve(layerCount);

    // 4. 各レイヤー読み込み
    for (qint32 i = 0; i < layerCount; ++i) {
        qint32 typeInt = 0;
        QString name;
        bool visible = true;
        float opacity = 1.0f;
        qint32 blendModeInt = 0;
        bool isClippingMask = false;
        bool isAlphaLocked = false;
        bool isLocked = false;
        qint32 parentGroupIdx = -1;

        stream >> typeInt >> name >> visible >> opacity >> blendModeInt >> isClippingMask >> isAlphaLocked >> isLocked >> parentGroupIdx;
        parentGroupIndices.push_back(parentGroupIdx);

        QByteArray aiJsonData;
        if (version >= 5) {
            stream >> aiJsonData;
        }

        LayerType lType = static_cast<LayerType>(typeInt);
        LayerPtr newLayer;

        switch (lType) {
        case LayerType::Raster: {
            auto rLayer = std::make_shared<Layer>(name, width, height, LayerType::Raster);
            qint32 validTileCount = 0;
            stream >> validTileCount;

            for (qint32 t = 0; t < validTileCount; ++t) {
                qint32 tx = 0;
                qint32 ty = 0;
                QByteArray compressed;
                stream >> tx >> ty >> compressed;

                QByteArray raw = qUncompress(compressed);
                if (raw.size() == static_cast<qsizetype>(TileMemoryPool::TILE_BYTE_SIZE)) {
                    Tile* tile = rLayer->tileManager().getTileForWrite(tx, ty);
                    if (tile) {
                        uint8_t* dst = tile->writeBuffer();
                        std::memcpy(dst, raw.constData(), TileMemoryPool::TILE_BYTE_SIZE);
                    }
                }
            }
            rLayer->invalidateContentBounds();
            newLayer = rLayer;
            break;
        }
        case LayerType::Vector: {
            auto vLayer = std::make_shared<VectorLayer>(name, width, height);
            qint32 strokeCount = 0;
            stream >> strokeCount;

            std::vector<VectorStroke> strokes;
            strokes.reserve(strokeCount);

            for (qint32 s = 0; s < strokeCount; ++s) {
                VectorStroke stroke;
                qint32 hardness = 100;
                qint32 falloff = 0;

                stream >> stroke.baseWidth >> stroke.color >> stroke.density >> hardness >> falloff >> stroke.isClosed >> stroke.isFilled;
                stroke.hardness = hardness;
                stroke.falloff = falloff;

                qint32 ptCount = 0;
                stream >> ptCount;
                stroke.points.reserve(ptCount);

                for (qint32 p = 0; p < ptCount; ++p) {
                    VectorPoint pt;
                    stream >> pt.x >> pt.y >> pt.pressure >> pt.opacity >> pt.isCorner;
                    stroke.points.push_back(pt);
                }
                strokes.push_back(stroke);
            }
            vLayer->setStrokes(strokes);
            vLayer->rasterizeAll();
            newLayer = vLayer;
            break;
        }
        case LayerType::Adjustment: {
            qint32 adjType = 0;
            float brightness = 0.0f;
            float contrast = 0.0f;
            float hueShift = 0.0f;
            float saturation = 1.0f;

            stream >> adjType >> brightness >> contrast >> hueShift >> saturation;

            auto aLayer = std::make_shared<AdjustmentLayer>(name, width, height, static_cast<AdjustmentType>(adjType));
            aLayer->setBrightnessContrast(brightness, contrast);
            if (version >= 3) {
                float lightness = 0.0f;
                qint32 interpMode = 0;
                stream >> lightness >> interpMode;
                aLayer->setHueSaturation(hueShift, saturation, lightness);
                aLayer->setCurveInterpolationMode(static_cast<CurveInterpolationMode>(interpMode));
                for (int ch = 0; ch < 4; ++ch) {
                    auto ach = static_cast<AdjustmentChannel>(ch);
                    qint32 ptCount = 0;
                    stream >> ptCount;
                    std::vector<QPointF> pts;
                    for (qint32 p = 0; p < ptCount; ++p) {
                        qreal px = 0.0, py = 0.0;
                        stream >> px >> py;
                        pts.emplace_back(px, py);
                    }
                    aLayer->setCurvePoints(ach, pts);

                    LevelParams lp;
                    stream >> lp.inShadow >> lp.inGamma >> lp.inHighlight >> lp.outShadow >> lp.outHighlight;
                    aLayer->setLevelParams(ach, lp);
                }
            } else {
                aLayer->setHueSaturation(hueShift, saturation);
            }
            newLayer = aLayer;
            break;
        }
        case LayerType::Group: {
            bool isExpanded = true;
            bool isPassThrough = false;
            stream >> isExpanded >> isPassThrough;

            auto gLayer = std::make_shared<GroupLayer>(name, width, height);
            gLayer->setExpanded(isExpanded);
            gLayer->setPassThrough(isPassThrough);

            if (version >= 4) {
                bool isPanel = false;
                QPolygonF poly;
                float bWidth = 3.0f;
                QColor bColor;
                bool drawBorder = true;
                stream >> isPanel >> poly >> bWidth >> bColor >> drawBorder;
                gLayer->setPanelFolder(isPanel);
                gLayer->setPanelPolygon(poly);
                gLayer->setPanelBorderWidth(bWidth);
                gLayer->setPanelBorderColor(bColor);
                gLayer->setDrawPanelBorder(drawBorder);
            }

            newLayer = gLayer;
            break;
        }
        case LayerType::Text: {
            auto tLayer = std::make_shared<TextLayer>(name, width, height);
            qint32 itemCount = 0;
            stream >> itemCount;

            for (qint32 it = 0; it < itemCount; ++it) {
                TextBalloonItem item;
                qint32 shapeInt = 0;
                stream >> item.id >> item.rect >> item.text >> item.font >> item.textColor >> item.frameColor >> item.fillColor >> item.frameWidth >> shapeInt >> item.hasTail >> item.tailTip;
                item.shape = static_cast<BalloonShape>(shapeInt);
                if (version >= 3) {
                    qint32 hAlign = 0, vAlign = 0;
                    stream >> hAlign >> vAlign;
                    item.horizontalAlign = static_cast<Qt::Alignment>(hAlign);
                    item.verticalAlign = static_cast<Qt::Alignment>(vAlign);
                } else {
                    item.horizontalAlign = Qt::AlignLeft;
                    item.verticalAlign = Qt::AlignTop;
                }
                if (version >= 4) {
                    qint32 sCount = 16;
                    stream >> item.tailBaseWidth >> item.tailCurvature >> sCount >> item.spikeDepth >> item.spikePhase;
                    stream >> item.hasOutline >> item.outlineWidth >> item.outlineColor;
                    item.spikeCount = sCount;
                }
                tLayer->addItem(item);
            }
            tLayer->rasterizeAll();
            newLayer = tLayer;
            break;
        }
        }

        if (newLayer) {
            newLayer->setVisible(visible);
            newLayer->setOpacity(opacity);
            newLayer->setBlendMode(static_cast<BlendMode>(blendModeInt));
            newLayer->setClippingMask(isClippingMask);
            newLayer->setAlphaLocked(isAlphaLocked);
            newLayer->setLocked(isLocked);

            if (!aiJsonData.isEmpty()) {
                QJsonDocument aDoc = QJsonDocument::fromJson(aiJsonData);
                if (aDoc.isObject()) {
                    newLayer->setAiParameters(aDoc.object());
                }
            }

            doc->addExistingLayer(newLayer);
        }
    }

    // 5. グループ階層構造の復元
    const auto& docLayers = doc->layers();
    for (size_t i = 0; i < parentGroupIndices.size() && i < docLayers.size(); ++i) {
        qint32 pIdx = parentGroupIndices[i];
        if (pIdx >= 0 && pIdx < static_cast<qint32>(docLayers.size()) && pIdx != static_cast<qint32>(i)) {
            auto groupLayer = std::dynamic_pointer_cast<GroupLayer>(docLayers[pIdx]);
            if (groupLayer) {
                groupLayer->addChild(docLayers[i]);
            }
        }
    }

    // 6. アクティブレイヤー復元
    doc->setActiveLayerIndex(activeLayerIdx);

    // 7. キャンバス全体の初回コンポジット
    doc->composite(QRect(0, 0, width, height));

    file.close();
    return doc;
}

QImage DocumentSerializer::extractThumbnail(const QString& filePath, QString* outErrorMessage) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (outErrorMessage) *outErrorMessage = file.errorString();
        return QImage();
    }

    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_5);

    quint32 magic = 0;
    stream >> magic;
    if (magic != KAB_MAGIC) {
        if (outErrorMessage) *outErrorMessage = "不正なフォーマットです。";
        return QImage();
    }

    quint32 version = 0;
    stream >> version;

    QByteArray thumbData;
    stream >> thumbData;

    return QImage::fromData(thumbData, "PNG");
}

bool DocumentSerializer::exportImage(const Document* document, const QString& filePath, QString* outErrorMessage) {
    if (!document) {
        if (outErrorMessage) *outErrorMessage = "ドキュメントが無効です。";
        return false;
    }

    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    QImage comp = document->compositeBuffer().toQImage();
    if (comp.isNull()) {
        if (outErrorMessage) *outErrorMessage = "画像の合成に失敗しました。";
        return false;
    }

    if (ext == "psd") {
        return writePsdFile(comp, filePath, document->dpi(), outErrorMessage);
    }

    const char* format = nullptr;
    int quality = -1;

    if (ext == "png") {
        format = "PNG";
    } else if (ext == "jpg" || ext == "jpeg") {
        format = "JPEG";
        quality = 95;
    } else if (ext == "bmp") {
        format = "BMP";
    } else if (ext == "tif" || ext == "tiff") {
        format = "TIFF";
    } else if (ext == "webp") {
        format = "WEBP";
        quality = 95;
    } else {
        // デフォルトはPNG
        format = "PNG";
    }

    if (!comp.save(filePath, format, quality)) {
        if (outErrorMessage) *outErrorMessage = QString("画像ファイルの保存に失敗しました: %1").arg(filePath);
        return false;
    }

    return true;
}

} // namespace Kaburagi
