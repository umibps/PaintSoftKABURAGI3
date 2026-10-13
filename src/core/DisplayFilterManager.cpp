#include "DisplayFilterManager.hpp"
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QCoreApplication>
#include <algorithm>
#include <cmath>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace Kaburagi {

DisplayFilterManager& DisplayFilterManager::instance() {
    static DisplayFilterManager mgr;
    return mgr;
}

DisplayFilterManager::DisplayFilterManager() {
    QSettings settings;
    int typeInt = settings.value("display/filterType", static_cast<int>(DisplayFilterType::None)).toInt();
    m_filterType = static_cast<DisplayFilterType>(std::clamp(typeInt, 0, 3));
    m_monitorProfilePath = settings.value("display/monitorProfilePath", QString()).toString();
    m_proofingProfilePath = settings.value("display/iccProfilePath", QString()).toString();

    if (!m_monitorProfilePath.isEmpty() || !m_proofingProfilePath.isEmpty()) {
        updateTransform();
    }
}

void DisplayFilterManager::setFilterType(DisplayFilterType type) {
    if (m_filterType == type) return;
    m_filterType = type;

    QSettings settings;
    settings.setValue("display/filterType", static_cast<int>(m_filterType));

    emit filterChanged();
}

void DisplayFilterManager::setMonitorProfilePath(const QString& path) {
    if (m_monitorProfilePath == path && m_hasMonitorTransform) return;
    m_monitorProfilePath = path;

    QSettings settings;
    settings.setValue("display/monitorProfilePath", m_monitorProfilePath);

    updateTransform();
    emit filterChanged();
}

void DisplayFilterManager::setProofingProfilePath(const QString& path) {
    if (m_proofingProfilePath == path && m_hasProofingTransform) return;
    m_proofingProfilePath = path;

    QSettings settings;
    settings.setValue("display/iccProfilePath", m_proofingProfilePath);

    updateTransform();
    emit filterChanged();
}

void DisplayFilterManager::updateTransform() {
    m_hasProofingTransform = false;
    m_hasMonitorTransform = false;

    QColorSpace srgb(QColorSpace::SRgb);

    // 1. 校正プロファイル
    m_proofingColorSpace = QColorSpace();
    if (!m_proofingProfilePath.isEmpty() && QFile::exists(m_proofingProfilePath)) {
        QFile file(m_proofingProfilePath);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            m_proofingColorSpace = QColorSpace::fromIccProfile(data);
        }
    }

    // 2. モニタープロファイル
    m_monitorColorSpace = QColorSpace();
    if (!m_monitorProfilePath.isEmpty() && QFile::exists(m_monitorProfilePath)) {
        QFile file(m_monitorProfilePath);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            m_monitorColorSpace = QColorSpace::fromIccProfile(data);
        }
    }

    // パイプライン構築: sRGB -> 校正プロファイル -> モニタープロファイル
    QColorSpace currentCs = srgb;

    if (m_proofingColorSpace.isValid()) {
        m_transformToProofing = currentCs.transformationToColorSpace(m_proofingColorSpace);
        m_hasProofingTransform = true;
        currentCs = m_proofingColorSpace;
    }

    if (m_monitorColorSpace.isValid()) {
        m_transformToMonitor = currentCs.transformationToColorSpace(m_monitorColorSpace);
        m_hasMonitorTransform = true;
    }
}

QColor DisplayFilterManager::applyFilter(const QColor& color) const {
    if (!color.isValid()) return color;

    switch (m_filterType) {
    case DisplayFilterType::None:
        return color;

    case DisplayFilterType::Grayscale: {
        // ITU-R BT.709 sRGB 輝度
        int r = color.red();
        int g = color.green();
        int b = color.blue();
        int gray = std::clamp(static_cast<int>(0.2126f * r + 0.7152f * g + 0.0722f * b + 0.5f), 0, 255);
        return QColor(gray, gray, gray, color.alpha());
    }

    case DisplayFilterType::GrayscaleYIQ: {
        // NTSC YIQ (BT.601) Y成分
        int r = color.red();
        int g = color.green();
        int b = color.blue();
        int gray = std::clamp(static_cast<int>(0.299f * r + 0.587f * g + 0.114f * b + 0.5f), 0, 255);
        return QColor(gray, gray, gray, color.alpha());
    }

    case DisplayFilterType::IccProfile: {
        QColor res = color;
        if (m_hasProofingTransform) {
            res = m_transformToProofing.map(res);
        }
        if (m_hasMonitorTransform) {
            res = m_transformToMonitor.map(res);
        }
        return res;
    }
    }

    return color;
}

QImage DisplayFilterManager::applyFilter(const QImage& src) const {
    if (src.isNull()) return src;
    if (m_filterType == DisplayFilterType::None) return src;

    if (m_filterType == DisplayFilterType::IccProfile) {
        if (m_hasProofingTransform || m_hasMonitorTransform) {
            QImage out = src.copy();
            if (m_hasProofingTransform) {
                out.applyColorTransform(m_transformToProofing);
            }
            if (m_hasMonitorTransform) {
                out.applyColorTransform(m_transformToMonitor);
            }
            return out;
        }
        return src;
    }

    // グレースケールまたはグレースケール(YIQ)
    QImage orig = (src.format() == QImage::Format_ARGB32_Premultiplied || src.format() == QImage::Format_ARGB32)
                  ? src
                  : src.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    bool isPremul = (orig.format() == QImage::Format_ARGB32_Premultiplied);
    int w = orig.width();
    int h = orig.height();
    QImage result(w, h, orig.format());

    bool isYiq = (m_filterType == DisplayFilterType::GrayscaleYIQ);

    for (int y = 0; y < h; ++y) {
        const uint32_t* srcLine = reinterpret_cast<const uint32_t*>(orig.constScanLine(y));
        uint32_t* dstLine = reinterpret_cast<uint32_t*>(result.scanLine(y));

        for (int x = 0; x < w; ++x) {
            uint32_t px = srcLine[x];
            uint32_t a = (px >> 24) & 0xFF;
            if (a == 0) {
                dstLine[x] = 0;
                continue;
            }

            float r, g, b;
            if (isPremul) {
                float invA = 255.0f / a;
                r = std::min(255.0f, ((px >> 16) & 0xFF) * invA);
                g = std::min(255.0f, ((px >>  8) & 0xFF) * invA);
                b = std::min(255.0f, ( px        & 0xFF) * invA);
            } else {
                r = static_cast<float>((px >> 16) & 0xFF);
                g = static_cast<float>((px >>  8) & 0xFF);
                b = static_cast<float>( px        & 0xFF);
            }

            int grayVal = isYiq
                ? std::clamp(static_cast<int>(0.299f * r + 0.587f * g + 0.114f * b + 0.5f), 0, 255)
                : std::clamp(static_cast<int>(0.2126f * r + 0.7152f * g + 0.0722f * b + 0.5f), 0, 255);

            if (isPremul) {
                float aRatio = a / 255.0f;
                uint32_t premulGray = static_cast<uint32_t>(grayVal * aRatio + 0.5f);
                dstLine[x] = (a << 24) | (premulGray << 16) | (premulGray << 8) | premulGray;
            } else {
                dstLine[x] = (a << 24) | (grayVal << 16) | (grayVal << 8) | grayVal;
            }
        }
    }

    return result;
}

std::vector<IccProfileInfo> DisplayFilterManager::scanSystemIccProfiles() {
    std::vector<IccProfileInfo> results;
    std::vector<QString> searchDirs;

#ifdef Q_OS_WIN
    // 1. Windows API GetColorDirectoryW (mscms.dll から動的ロード)
    typedef BOOL (WINAPI *GetColorDirFunc)(PCWSTR, PWSTR, PDWORD);
    HMODULE hMscms = LoadLibraryW(L"mscms.dll");
    if (hMscms) {
        auto pGetColorDir = reinterpret_cast<GetColorDirFunc>(GetProcAddress(hMscms, "GetColorDirectoryW"));
        if (pGetColorDir) {
            DWORD size = 0;
            pGetColorDir(NULL, NULL, &size);
            if (size > 0) {
                std::vector<wchar_t> buf(size + 1, 0);
                if (pGetColorDir(NULL, buf.data(), &size)) {
                    QString sysDir = QString::fromWCharArray(buf.data());
                    if (!sysDir.isEmpty()) {
                        searchDirs.push_back(sysDir);
                    }
                }
            }
        }
        FreeLibrary(hMscms);
    }

    // 既知の標準ディレクトリ (フォールバック & 追加検索)
    searchDirs.push_back("C:/Windows/System32/spool/drivers/color");

    QString commonProgramFiles = qEnvironmentVariable("COMMONPROGRAMFILES");
    if (!commonProgramFiles.isEmpty()) {
        searchDirs.push_back(commonProgramFiles + "/Adobe/Color/Profiles");
    }
    QString commonProgramFilesX86 = qEnvironmentVariable("COMMONPROGRAMFILES(X86)");
    if (!commonProgramFilesX86.isEmpty()) {
        searchDirs.push_back(commonProgramFilesX86 + "/Adobe/Color/Profiles");
    }
    QString appData = qEnvironmentVariable("APPDATA");
    if (!appData.isEmpty()) {
        searchDirs.push_back(appData + "/Adobe/Color/Profiles");
    }
#elif defined(Q_OS_MACOS)
    searchDirs.push_back(QDir::homePath() + "/Library/ColorSync/Profiles");
    searchDirs.push_back("/Library/ColorSync/Profiles");
    searchDirs.push_back("/System/Library/ColorSync/Profiles");
    searchDirs.push_back("/Library/Application Support/Adobe/Color/Profiles");
#else
    searchDirs.push_back(QDir::homePath() + "/.color/icc");
    searchDirs.push_back("/usr/share/color/icc");
    searchDirs.push_back("/usr/local/share/color/icc");
#endif

    std::vector<QString> processedPaths;

    for (const QString& dirPath : searchDirs) {
        QDir dir(dirPath);
        if (!dir.exists()) continue;

        QDirIterator it(dirPath, QStringList() << "*.icc" << "*.icm", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString filePath = it.next();
            QString normPath = QDir::fromNativeSeparators(filePath);

            // 重複チェック
            if (std::find(processedPaths.begin(), processedPaths.end(), normPath) != processedPaths.end()) {
                continue;
            }
            processedPaths.push_back(normPath);

            QFile f(normPath);
            if (f.open(QIODevice::ReadOnly)) {
                QByteArray data = f.readAll();
                f.close();

                QColorSpace cs = QColorSpace::fromIccProfile(data);
                if (cs.isValid()) {
                    IccProfileInfo info;
                    info.path = normPath;
                    QString desc = cs.description();
                    info.name = desc.isEmpty() ? QFileInfo(normPath).completeBaseName() : desc;

                    switch (cs.colorModel()) {
                    case QColorSpace::ColorModel::Rgb:
                        info.colorSpace = "RGB";
                        break;
                    case QColorSpace::ColorModel::Cmyk:
                        info.colorSpace = "CMYK";
                        break;
                    case QColorSpace::ColorModel::Gray:
                        info.colorSpace = "Gray";
                        break;
                    default:
                        info.colorSpace = "Other";
                        break;
                    }

                    results.push_back(info);
                }
            }
        }
    }

    // 名前順にソート
    std::sort(results.begin(), results.end(), [](const IccProfileInfo& a, const IccProfileInfo& b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });

    return results;
}

} // namespace Kaburagi
