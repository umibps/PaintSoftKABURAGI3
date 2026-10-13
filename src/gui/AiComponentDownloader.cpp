#include "AiComponentDownloader.hpp"
#include "PreferencesDialog.hpp"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QEventLoop>
#include <QMessageBox>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QDebug>

namespace Kaburagi {

AiComponentDownloader::AiComponentDownloader(QObject* parent)
    : QObject(parent)
    , m_netManager(new QNetworkAccessManager(this))
{
}

AiComponentDownloader::~AiComponentDownloader() {
    cancel();
}

ComponentInfo AiComponentDownloader::getComponentInfo(ComponentType type) {
    ComponentInfo info;
    info.type = type;

    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appData.isEmpty()) {
        appData = QDir::homePath() + "/.paintsoft_kaburagi";
    }

    switch (type) {
    case ComponentType::EmbeddedSdExe:
        info.name = "組み込み画像生成エンジン (sd.exe)";
        info.fileName = "sd.exe";
        info.targetDirectory = appData + "/tools";
        // デフォルトフォールバックURL（検証済み・安定動作バージョン）
        info.downloadUrl = "https://github.com/leejet/stable-diffusion.cpp/releases/download/master-956-1b0ba10/sd-master-1b0ba10-bin-win-vulkan-x64.zip";
        info.estimatedSizeBytes = 30 * 1024 * 1024; // 約30MB
        info.description = "Stable Diffusion をローカルサーバー不要で直接高速実行するための軽量推論エンジンです（Vulkan/DirectX対応）。";
        break;

    case ComponentType::UpscaleModelAnime6B:
        info.name = "AIアップスケーラー (R-ESRGAN 4x+ Anime6B)";
        info.fileName = "RealESRGAN_x4plus_anime_6B.pth";
        {
            QString mDir = PreferencesDialog::aiModelDirectory();
            if (!mDir.isEmpty()) {
                info.targetDirectory = mDir + "/upscale_models";
            } else {
                info.targetDirectory = appData + "/models/ESRGAN";
            }
        }
        info.downloadUrl = "https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.2.4/RealESRGAN_x4plus_anime_6B.pth";
        info.estimatedSizeBytes = 18 * 1024 * 1024; // 約18MB
        info.description = "イラスト・アニメ調の画像をくっきり滑らかに4倍高解像度化するAIアップスケールモデルです。";
        break;

    case ComponentType::UpscaleModelRealEsrgan:
        info.name = "AIアップスケーラー (RealESRGAN_x4plus)";
        info.fileName = "RealESRGAN_x4plus.pth";
        {
            QString mDir = PreferencesDialog::aiModelDirectory();
            if (!mDir.isEmpty()) {
                info.targetDirectory = mDir + "/upscale_models";
            } else {
                info.targetDirectory = appData + "/models/ESRGAN";
            }
        }
        info.downloadUrl = "https://github.com/xinntao/Real-ESRGAN/releases/download/v0.1.0/RealESRGAN_x4plus.pth";
        info.estimatedSizeBytes = 67 * 1024 * 1024; // 約67MB
        info.description = "実写・汎用の画像を鮮明に4倍高解像度化するAIアップスケールモデルです。";
        break;

    case ComponentType::ControlNetLineart:
        info.name = "ControlNet 線画抽出モデル (Lineart)";
        info.fileName = "control_v11p_sd15_lineart.pth";
        {
            QString cDir = PreferencesDialog::aiControlNetDirectory();
            if (!cDir.isEmpty()) {
                info.targetDirectory = cDir;
            } else {
                info.targetDirectory = appData + "/models/ControlNet";
            }
        }
        info.downloadUrl = "https://huggingface.co/lllyasviel/ControlNet-v1-1/resolve/main/control_v11p_sd15_lineart.pth";
        info.estimatedSizeBytes = 1450ULL * 1024 * 1024; // 約1.45GB
        info.description = "キャンバスの線画から構図を保って清書・着色するためのモデルです。";
        break;

    case ComponentType::BaseSdModel:
        info.name = "標準画像生成モデル (SD 1.5 Base)";
        info.fileName = "v1-5-pruned-emaonly.safetensors";
        {
            QString mDir = PreferencesDialog::aiModelDirectory();
            if (!mDir.isEmpty()) {
                info.targetDirectory = mDir;
            } else {
                info.targetDirectory = appData + "/models";
            }
        }
        info.downloadUrl = "https://huggingface.co/runwayml/stable-diffusion-v1-5/resolve/main/v1-5-pruned-emaonly.safetensors";
        info.estimatedSizeBytes = 2100ULL * 1024 * 1024; // 約2.1GB
        info.description = "標準的な画像生成を行うためのベースチェックポイントモデルです。";
        break;
    }

    return info;
}

QString AiComponentDownloader::resolveLatestDownloadUrl(ComponentType type) {
    if (type == ComponentType::EmbeddedSdExe) {
        QString fallback = getComponentInfo(type).downloadUrl;

        // GitHub API から動的に最新アセットを検索（タイムアウト2秒）
        QNetworkAccessManager mgr;
        QNetworkRequest req(QUrl("https://api.github.com/repos/leejet/stable-diffusion.cpp/releases/latest"));
        req.setHeader(QNetworkRequest::UserAgentHeader, "PaintSoftKABURAGI/3.0.1");

        QNetworkReply* reply = mgr.get(req);
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        timer.start(2000);
        loop.exec();

        if (reply->isFinished() && reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject root = doc.object();
            QJsonArray assets = root["assets"].toArray();

            // 1. Vulkan 版 (すべてのモダンGPU対応・約30MB)
            for (const auto& a : assets) {
                QString name = a.toObject()["name"].toString();
                if (name.contains("bin-win-vulkan-x64.zip", Qt::CaseInsensitive)) {
                    QString u = a.toObject()["browser_download_url"].toString();
                    if (!u.isEmpty()) {
                        reply->deleteLater();
                        return u;
                    }
                }
            }

            // 2. CPU 版 (約17MB)
            for (const auto& a : assets) {
                QString name = a.toObject()["name"].toString();
                if (name.contains("bin-win-cpu-x64.zip", Qt::CaseInsensitive)) {
                    QString u = a.toObject()["browser_download_url"].toString();
                    if (!u.isEmpty()) {
                        reply->deleteLater();
                        return u;
                    }
                }
            }
        }
        reply->deleteLater();
        return fallback;
    }

    return getComponentInfo(type).downloadUrl;
}

QString AiComponentDownloader::getLocalFilePath(ComponentType type) {
    ComponentInfo info = getComponentInfo(type);
    if (type == ComponentType::EmbeddedSdExe) {
        QString pref = PreferencesDialog::embeddedEnginePath();
        if (!pref.isEmpty() && QFile::exists(pref)) return pref;

        QString appDir = QCoreApplication::applicationDirPath();
        if (QFile::exists(appDir + "/tools/sd-cli.exe")) return appDir + "/tools/sd-cli.exe";
        if (QFile::exists(appDir + "/tools/sd.exe")) return appDir + "/tools/sd.exe";
        if (QFile::exists(appDir + "/sd-cli.exe")) return appDir + "/sd-cli.exe";
        if (QFile::exists(appDir + "/sd.exe")) return appDir + "/sd.exe";

        if (QFile::exists(info.targetDirectory + "/sd-cli.exe")) return info.targetDirectory + "/sd-cli.exe";
        if (QFile::exists(info.targetDirectory + "/sd.exe")) return info.targetDirectory + "/sd.exe";

        return info.targetDirectory + "/sd.exe";
    }

    QString target = info.targetDirectory + "/" + info.fileName;
    if (QFile::exists(target)) return target;

    if (type == ComponentType::UpscaleModelAnime6B) {
        QString alt1 = info.targetDirectory + "/R-ESRGAN 4x+ Anime6B.pth";
        if (QFile::exists(alt1)) return alt1;
    }

    return target;
}

bool AiComponentDownloader::isComponentInstalled(ComponentType type) {
    QString path = getLocalFilePath(type);
    if (path.isEmpty()) return false;
    QFileInfo fi(path);
    return fi.exists() && fi.size() > 0;
}

bool AiComponentDownloader::downloadComponentWithDialog(ComponentType type, QWidget* parentWidget, const QString& customTargetDir) {
    ComponentInfo info = getComponentInfo(type);
    if (!customTargetDir.isEmpty()) {
        info.targetDirectory = customTargetDir;
    }

    auto progressDlg = new QProgressDialog(parentWidget);
    progressDlg->setWindowTitle(tr("コンポーネントのダウンロード"));
    progressDlg->setLabelText(tr("%1 をダウンロード中...\n接続中...").arg(info.name));
    progressDlg->setRange(0, 100);
    progressDlg->setValue(0);
    progressDlg->setMinimumDuration(0);
    progressDlg->setWindowModality(Qt::WindowModal);
    progressDlg->setAutoClose(false);

    QEventLoop loop;
    bool downloadSuccess = false;
    bool userCancelled = false;

    connect(this, &AiComponentDownloader::progressChanged, progressDlg, [progressDlg, info](qint64 received, qint64 total) {
        if (total > 0) {
            int pct = static_cast<int>((received * 100) / total);
            progressDlg->setValue(pct);
            double mbRec = static_cast<double>(received) / (1024.0 * 1024.0);
            double mbTot = static_cast<double>(total) / (1024.0 * 1024.0);
            progressDlg->setLabelText(tr("%1 をダウンロード中...\n%2 MB / %3 MB (%4%)")
                                          .arg(info.name)
                                          .arg(mbRec, 0, 'f', 1)
                                          .arg(mbTot, 0, 'f', 1)
                                          .arg(pct));
        } else {
            double mbRec = static_cast<double>(received) / (1024.0 * 1024.0);
            progressDlg->setLabelText(tr("%1 をダウンロード中...\n%2 MB 受信済み...")
                                          .arg(info.name)
                                          .arg(mbRec, 0, 'f', 1));
        }
    });

    auto cancelConnection = connect(progressDlg, &QProgressDialog::canceled, this, [this, &userCancelled]() {
        userCancelled = true;
        cancel();
    });

    connect(this, &AiComponentDownloader::finished, &loop, [&loop, &downloadSuccess](bool ok, const QString&, const QString&) {
        downloadSuccess = ok;
        loop.quit();
    });

    startDownload(type, customTargetDir);
    loop.exec();

    // 終了処理: キャンセル接続を解除してからダイアログを閉じる
    QObject::disconnect(cancelConnection);
    progressDlg->close();
    progressDlg->deleteLater();

    if (!downloadSuccess && !userCancelled) {
        QMessageBox::warning(parentWidget, tr("ダウンロード失敗"),
                             tr("コンポーネントのダウンロード中にエラーが発生しました:\n%1").arg(m_errorMessage));
    }

    return downloadSuccess;
}

void AiComponentDownloader::startDownload(ComponentType type, const QString& customTargetDir) {
    cancel();

    m_cancelled = false;
    m_succeeded = false;
    m_errorMessage.clear();
    m_currentInfo = getComponentInfo(type);
    if (!customTargetDir.isEmpty()) {
        m_currentInfo.targetDirectory = customTargetDir;
    }

    // 最新URLの解決
    m_currentInfo.downloadUrl = resolveLatestDownloadUrl(type);

    QDir targetDir(m_currentInfo.targetDirectory);
    if (!targetDir.exists()) {
        targetDir.mkpath(".");
    }

    m_finalFilePath = m_currentInfo.targetDirectory + "/" + m_currentInfo.fileName;
    QString ext = m_currentInfo.downloadUrl.endsWith(".zip", Qt::CaseInsensitive) ? ".zip" : ".part";
    m_tempFilePath = m_finalFilePath + ext;

    m_outputFile = new QFile(m_tempFilePath, this);
    if (!m_outputFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_errorMessage = tr("保存先ファイルを作成できませんでした: %1").arg(m_tempFilePath);
        emit finished(false, QString(), m_errorMessage);
        return;
    }

    QNetworkRequest request(QUrl(m_currentInfo.downloadUrl));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, "PaintSoftKABURAGI/3.0.1 (ComponentDownloader)");

    m_currentReply = m_netManager->get(request);
    connect(m_currentReply, &QNetworkReply::downloadProgress, this, &AiComponentDownloader::onDownloadProgress);
    connect(m_currentReply, &QNetworkReply::readyRead, this, &AiComponentDownloader::onReadyRead);
    connect(m_currentReply, &QNetworkReply::finished, this, &AiComponentDownloader::onDownloadFinished);
}

void AiComponentDownloader::cancel() {
    m_cancelled = true;
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
    if (m_outputFile) {
        m_outputFile->close();
        m_outputFile->remove();
        delete m_outputFile;
        m_outputFile = nullptr;
    }
}

void AiComponentDownloader::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    if (m_cancelled) return;
    emit progressChanged(bytesReceived, bytesTotal);
}

void AiComponentDownloader::onReadyRead() {
    if (m_currentReply && m_outputFile && m_outputFile->isOpen()) {
        m_outputFile->write(m_currentReply->readAll());
    }
}

void AiComponentDownloader::onDownloadFinished() {
    if (!m_currentReply) return;

    if (m_currentReply->error() != QNetworkReply::NoError) {
        if (!m_cancelled) {
            m_errorMessage = m_currentReply->errorString();
        }
        if (m_outputFile) {
            m_outputFile->close();
            m_outputFile->remove();
            delete m_outputFile;
            m_outputFile = nullptr;
        }
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        emit finished(false, QString(), m_errorMessage);
        return;
    }

    if (m_outputFile && m_outputFile->isOpen()) {
        m_outputFile->write(m_currentReply->readAll());
        m_outputFile->flush();
        m_outputFile->close();
        delete m_outputFile;
        m_outputFile = nullptr;
    }

    m_currentReply->deleteLater();
    m_currentReply = nullptr;

    if (m_cancelled) {
        QFile::remove(m_tempFilePath);
        emit finished(false, QString(), tr("キャンセルされました"));
        return;
    }

    // ZIPアーカイブの場合、展開処理を行う
    if (m_tempFilePath.endsWith(".zip", Qt::CaseInsensitive)) {
        bool extracted = extractZipArchive(m_tempFilePath, m_currentInfo.targetDirectory);
        QFile::remove(m_tempFilePath);
        if (!extracted) {
            m_errorMessage = tr("アーカイブの展開に失敗しました。");
            emit finished(false, QString(), m_errorMessage);
            return;
        }

        // 解凍先ディレクトリ内の sd-cli.exe または sd.exe を検出
        QDir d(m_currentInfo.targetDirectory);
        if (d.exists("sd-cli.exe")) {
            m_finalFilePath = d.filePath("sd-cli.exe");
            if (!d.exists("sd.exe")) {
                QFile::copy(d.filePath("sd-cli.exe"), d.filePath("sd.exe"));
            }
        } else if (d.exists("sd.exe")) {
            m_finalFilePath = d.filePath("sd.exe");
        }
    } else {
        if (QFile::exists(m_finalFilePath)) {
            QFile::remove(m_finalFilePath);
        }
        if (!QFile::rename(m_tempFilePath, m_finalFilePath)) {
            m_errorMessage = tr("ダウンロードしたファイルを保存先にリネームできませんでした。");
            emit finished(false, QString(), m_errorMessage);
            return;
        }
    }

    m_succeeded = true;
    emit finished(true, m_finalFilePath, QString());
}

bool AiComponentDownloader::extractZipArchive(const QString& zipPath, const QString& targetDir) {
    QDir().mkpath(targetDir);

    // Windows 10/11 標準の tar コマンドによる高速展開
    QProcess proc;
    proc.start("tar", QStringList() << "-xf" << zipPath << "-C" << targetDir);
    if (proc.waitForFinished(30000) && proc.exitCode() == 0) {
        return true;
    }

    // フォールバック: PowerShell Expand-Archive
    proc.start("powershell", QStringList()
        << "-NoProfile"
        << "-Command"
        << QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force").arg(zipPath, targetDir));
    if (proc.waitForFinished(60000) && proc.exitCode() == 0) {
        return true;
    }

    return false;
}

} // namespace Kaburagi
