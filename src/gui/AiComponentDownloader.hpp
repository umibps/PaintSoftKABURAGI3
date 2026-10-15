#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>

#include "AiService.hpp"

class QWidget;

namespace Kaburagi {

enum class ComponentType {
    EmbeddedSdExe,         // 組み込み推論エンジン (sd.exe / sd-cli.exe)
    UpscaleModelAnime6B,   // R-ESRGAN 4x+ Anime6B
    UpscaleModelRealEsrgan,// RealESRGAN_x4plus
    ControlNetLineart,     // ControlNet lineart
    ControlNetDepth,       // ControlNet depth
    ControlNetOpenpose,    // ControlNet openpose
    ControlNetScribble,    // ControlNet scribble
    ControlNetCanny,       // ControlNet canny
    BaseSdModel            // 標準ベースモデル (v1-5-pruned-emaonly.safetensors)
};

struct ComponentInfo {
    ComponentType type;
    QString name;
    QString fileName;
    QString targetDirectory;
    QString downloadUrl;
    qint64 estimatedSizeBytes = 0;
    QString description;
};

/**
 * @brief 画像生成に必要なコンポーネント（sd.exe、アップスケールモデル等）の自動確認・ダウンロードマネージャー
 */
class AiComponentDownloader : public QObject {
    Q_OBJECT

public:
    explicit AiComponentDownloader(QObject* parent = nullptr);
    ~AiComponentDownloader() override;

    static ComponentInfo getComponentInfo(ComponentType type);
    static QString getLocalFilePath(ComponentType type);
    static bool isComponentInstalled(ComponentType type);
    static QString resolveLatestDownloadUrl(ComponentType type);
    static ComponentType fromControlNetType(ControlNetType cnType);
    static bool hasComponentForControlNetType(ControlNetType cnType);

    // プログレスダイアログを表示してモーダルダウンロードを実行 (成功で true)
    // customTargetDir が指定されている場合、そのディレクトリに保存
    bool downloadComponentWithDialog(ComponentType type, QWidget* parentWidget = nullptr, const QString& customTargetDir = QString());

    // 非同期ダウンロード API
    void startDownload(ComponentType type, const QString& customTargetDir = QString());
    void cancel();

    QString installedFilePath() const { return m_finalFilePath; }
    bool isUserCancelled() const { return m_cancelled; }
    QString errorMessage() const { return m_errorMessage; }

signals:
    void progressChanged(qint64 bytesReceived, qint64 bytesTotal);
    void finished(bool success, const QString& localPath, const QString& errorMessage);

private slots:
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();
    void onReadyRead();

private:
    bool extractZipArchive(const QString& zipPath, const QString& targetDir);

private:
    QNetworkAccessManager* m_netManager = nullptr;
    QNetworkReply* m_currentReply = nullptr;
    QFile* m_outputFile = nullptr;
    ComponentInfo m_currentInfo;
    QString m_tempFilePath;
    QString m_finalFilePath;
    bool m_cancelled = false;
    bool m_succeeded = false;
    QString m_errorMessage;
};

} // namespace Kaburagi
