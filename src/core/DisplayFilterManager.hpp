#pragma once

#include <QObject>
#include <QColor>
#include <QImage>
#include <QString>
#include <QColorSpace>
#include <QColorTransform>
#include <vector>

namespace Kaburagi {

enum class DisplayFilterType {
    None = 0,
    Grayscale = 1,
    GrayscaleYIQ = 2,
    IccProfile = 3
};

struct IccProfileInfo {
    QString name;        // プロファイル名 (Description または ファイル名)
    QString path;        // フルパス
    QString colorSpace;  // RGB, CMYK, etc.
    QString deviceClass; // Display, Output, etc.
};

/**
 * @brief 表示フィルター管理クラス (グレースケール、YIQ、ICCプロファイル変換)
 */
class DisplayFilterManager : public QObject {
    Q_OBJECT
public:
    static DisplayFilterManager& instance();

    DisplayFilterType filterType() const { return m_filterType; }
    void setFilterType(DisplayFilterType type);

    QString monitorProfilePath() const { return m_monitorProfilePath; }
    void setMonitorProfilePath(const QString& path);

    QString proofingProfilePath() const { return m_proofingProfilePath; }
    void setProofingProfilePath(const QString& path);

    // 互換用 (proofingProfilePath と同等)
    QString iccProfilePath() const { return proofingProfilePath(); }
    void setIccProfilePath(const QString& path) { setProofingProfilePath(path); }

    // 単一カラーへの表示フィルター適用
    QColor applyFilter(const QColor& color) const;

    // 画像への表示フィルター適用
    QImage applyFilter(const QImage& src) const;

    // システムにインストールされているICC/ICMプロファイル一覧の取得
    static std::vector<IccProfileInfo> scanSystemIccProfiles();

signals:
    void filterChanged();

private:
    DisplayFilterManager();
    ~DisplayFilterManager() override = default;

    void updateTransform();

    DisplayFilterType m_filterType = DisplayFilterType::None;
    QString m_monitorProfilePath;
    QString m_proofingProfilePath;
    QColorSpace m_monitorColorSpace;
    QColorSpace m_proofingColorSpace;
    QColorTransform m_transformToProofing;
    QColorTransform m_transformToMonitor;
    bool m_hasProofingTransform = false;
    bool m_hasMonitorTransform = false;
};

} // namespace Kaburagi
