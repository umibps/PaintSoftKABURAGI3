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
            QString uDir = PreferencesDialog::upscaleModelDirectory();
            if (!uDir.isEmpty()) {
                info.targetDirectory = uDir;
            } else {
                QString mDir = PreferencesDialog::aiModelDirectory();
                if (!mDir.isEmpty()) {
                    info.targetDirectory = mDir + "/upscale_models";
                } else {
                    info.targetDirectory = appData + "/models/ESRGAN";
                }
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
            QString uDir = PreferencesDialog::upscaleModelDirectory();
            if (!uDir.isEmpty()) {
                info.targetDirectory = uDir;
            } else {
                QString mDir = PreferencesDialog::aiModelDirectory();
                if (!mDir.isEmpty()) {
                    info.targetDirectory = mDir + "/upscale_models";
                } else {
                    info.targetDirectory = appData + "/models/ESRGAN";
                }
            }
        }
        info.downloadUrl = "https://github.com/xinntao/Real-ESRGAN/releases/download/v0.1.0/RealESRGAN_x4plus.pth";
        info.estimatedSizeBytes = 67 * 1024 * 1024; // 約67MB
        info.description = "実写・汎用の画像を鮮明に4倍高解像度化するAIアップスケールモデルです。";
        break;

    case ComponentType::ControlNetLineart:
    case ComponentType::ControlNetDepth:
    case ComponentType::ControlNetOpenpose:
    case ComponentType::ControlNetScribble:
    case ComponentType::ControlNetCanny:
        {
            QString cDir = PreferencesDialog::aiControlNetDirectory();
            if (!cDir.isEmpty() && QDir(cDir).exists()) {
                info.targetDirectory = cDir;
            } else {
                QString mDir = PreferencesDialog::aiModelDirectory();
                if (!mDir.isEmpty()) {
                    QDir md(mDir);
                    if (md.exists("ControlNet")) info.targetDirectory = md.filePath("ControlNet");
                    else if (md.exists("controlnet")) info.targetDirectory = md.filePath("controlnet");
                    else {
                        QDir parentDir = md;
                        if (parentDir.cdUp() && (parentDir.exists("ControlNet") || parentDir.exists("controlnet"))) {
                            info.targetDirectory = parentDir.exists("ControlNet") ? parentDir.filePath("ControlNet") : parentDir.filePath("controlnet");
                        } else {
                            info.targetDirectory = md.filePath("ControlNet");
                        }
                    }
                } else {
                    info.targetDirectory = appData + "/models/ControlNet";
                }
            }

            info.estimatedSizeBytes = 1450ULL * 1024 * 1024; // 約1.45GB

            if (type == ComponentType::ControlNetLineart) {
                info.name = "ControlNet 線画抽出モデル (Lineart)";
                info.fileName = "control_v11p_sd15_lineart.pth";
                info.downloadUrl = "https://huggingface.co/lllyasviel/ControlNet-v1-1/resolve/main/control_v11p_sd15_lineart.pth";
                info.description = "キャンバスの線画から構図を保って清書・着色するためのモデルです。";
            } else if (type == ComponentType::ControlNetDepth) {
                info.name = "ControlNet 深度推定モデル (Depth)";
                info.fileName = "control_v11f1p_sd15_depth.pth";
                info.downloadUrl = "https://huggingface.co/lllyasviel/ControlNet-v1-1/resolve/main/control_v11f1p_sd15_depth.pth";
                info.description = "立体感や奥行きを維持して画像を生成するためのモデルです。";
            } else if (type == ComponentType::ControlNetOpenpose) {
                info.name = "ControlNet 人体ポーズモデル (Openpose)";
                info.fileName = "control_v11p_sd15_openpose.pth";
                info.downloadUrl = "https://huggingface.co/lllyasviel/ControlNet-v1-1/resolve/main/control_v11p_sd15_openpose.pth";
                info.description = "キャラクターのポーズや骨格姿勢を精密に制御するためのモデルです。";
            } else if (type == ComponentType::ControlNetScribble) {
                info.name = "ControlNet ラフ・落書きモデル (Scribble)";
                info.fileName = "control_v11p_sd15_scribble.pth";
                info.downloadUrl = "https://huggingface.co/lllyasviel/ControlNet-v1-1/resolve/main/control_v11p_sd15_scribble.pth";
                info.description = "大まかな落書きや手書きスケッチからイラストを生成するためのモデルです。";
            } else if (type == ComponentType::ControlNetCanny) {
                info.name = "ControlNet エッジ抽出モデル (Canny)";
                info.fileName = "control_v11p_sd15_canny.pth";
                info.downloadUrl = "https://huggingface.co/lllyasviel/ControlNet-v1-1/resolve/main/control_v11p_sd15_canny.pth";
                info.description = "画像の輪郭線や境界を正確に維持して生成するためのモデルです。";
            }
        }
        break;

    case ComponentType::ControlNetSdxlLineart:
    case ComponentType::ControlNetSdxlDepth:
    case ComponentType::ControlNetSdxlOpenpose:
    case ComponentType::ControlNetSdxlScribble:
    case ComponentType::ControlNetSdxlCanny:
        {
            QString cDir = PreferencesDialog::aiControlNetDirectory();
            if (!cDir.isEmpty() && QDir(cDir).exists()) {
                info.targetDirectory = cDir;
            } else {
                QString mDir = PreferencesDialog::aiModelDirectory();
                if (!mDir.isEmpty()) {
                    QDir md(mDir);
                    if (md.exists("ControlNet")) info.targetDirectory = md.filePath("ControlNet");
                    else if (md.exists("controlnet")) info.targetDirectory = md.filePath("controlnet");
                    else {
                        QDir parentDir = md;
                        if (parentDir.cdUp() && (parentDir.exists("ControlNet") || parentDir.exists("controlnet"))) {
                            info.targetDirectory = parentDir.exists("ControlNet") ? parentDir.filePath("ControlNet") : parentDir.filePath("controlnet");
                        } else {
                            info.targetDirectory = md.filePath("ControlNet");
                        }
                    }
                } else {
                    info.targetDirectory = appData + "/models/ControlNet";
                }
            }

            info.estimatedSizeBytes = 2500ULL * 1024 * 1024; // 約2.5GB

            if (type == ComponentType::ControlNetSdxlLineart) {
                info.name = "ControlNet SDXL 線画抽出モデル (Anyline / Lineart)";
                info.fileName = "CN-anytest_v4-marged.safetensors";
                info.downloadUrl = "https://huggingface.co/2vXpSwA7/iroiro-lora/resolve/main/test_controlnet2/CN-anytest_v4-marged.safetensors";
                info.description = "SDXL・AnimagineXL・Illustrious等でラフや線画からイラストを清書するための高性能モデルです。";
            } else if (type == ComponentType::ControlNetSdxlDepth) {
                info.name = "ControlNet SDXL 深度推定モデル (Depth)";
                info.fileName = "controlnet-depth-sdxl-1.0.safetensors";
                info.downloadUrl = "https://huggingface.co/diffusers/controlnet-depth-sdxl-1.0/resolve/main/diffusion_pytorch_model.safetensors";
                info.description = "SDXL環境で立体感や奥行きを維持して画像を生成するためのモデルです。";
            } else if (type == ComponentType::ControlNetSdxlOpenpose) {
                info.name = "ControlNet SDXL 人体ポーズモデル (Openpose)";
                info.fileName = "controlnet-openpose-sdxl-1.0.safetensors";
                info.downloadUrl = "https://huggingface.co/thibaud/controlnet-openpose-sdxl-1.0/resolve/main/diffusion_pytorch_model.safetensors";
                info.description = "SDXL環境でキャラクターのポーズや骨格姿勢を制御するためのモデルです。";
            } else if (type == ComponentType::ControlNetSdxlScribble) {
                info.name = "ControlNet SDXL ラフ・落書きモデル (Scribble)";
                info.fileName = "controlnet-scribble-sdxl-1.0.safetensors";
                info.downloadUrl = "https://huggingface.co/xinsir/controlnet-scribble-sdxl-1.0/resolve/main/diffusion_pytorch_model.safetensors";
                info.description = "SDXL環境で手書きスケッチやアタリからイラストを生成するためのモデルです。";
            } else if (type == ComponentType::ControlNetSdxlCanny) {
                info.name = "ControlNet SDXL エッジ抽出モデル (Canny)";
                info.fileName = "controlnet-canny-sdxl-1.0.safetensors";
                info.downloadUrl = "https://huggingface.co/diffusers/controlnet-canny-sdxl-1.0/resolve/main/diffusion_pytorch_model.safetensors";
                info.description = "SDXL環境で画像の輪郭線や境界を正確に維持して生成するためのモデルです。";
            }
        }
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

ComponentType AiComponentDownloader::fromControlNetType(ControlNetType cnType, bool isSdxl) {
    if (isSdxl) {
        switch (cnType) {
        case ControlNetType::Lineart: return ComponentType::ControlNetSdxlLineart;
        case ControlNetType::Depth: return ComponentType::ControlNetSdxlDepth;
        case ControlNetType::Openpose: return ComponentType::ControlNetSdxlOpenpose;
        case ControlNetType::Scribble: return ComponentType::ControlNetSdxlScribble;
        case ControlNetType::Canny: return ComponentType::ControlNetSdxlCanny;
        default: return ComponentType::ControlNetSdxlLineart;
        }
    } else {
        switch (cnType) {
        case ControlNetType::Lineart: return ComponentType::ControlNetLineart;
        case ControlNetType::Depth: return ComponentType::ControlNetDepth;
        case ControlNetType::Openpose: return ComponentType::ControlNetOpenpose;
        case ControlNetType::Scribble: return ComponentType::ControlNetScribble;
        case ControlNetType::Canny: return ComponentType::ControlNetCanny;
        default: return ComponentType::ControlNetLineart;
        }
    }
}

bool AiComponentDownloader::hasComponentForControlNetType(ControlNetType cnType, bool isSdxl) {
    Q_UNUSED(isSdxl);
    switch (cnType) {
    case ControlNetType::Lineart:
    case ControlNetType::Depth:
    case ControlNetType::Openpose:
    case ControlNetType::Scribble:
    case ControlNetType::Canny:
        return true;
    default:
        return false;
    }
}

QString AiComponentDownloader::getLocalFilePath(ComponentType type) {
    ComponentInfo info = getComponentInfo(type);
    QString appDir = QCoreApplication::applicationDirPath();

    // ControlNet モデルの場合、AiService::findControlNetModelPath を利用
    if (type == ComponentType::ControlNetLineart ||
        type == ComponentType::ControlNetDepth ||
        type == ComponentType::ControlNetOpenpose ||
        type == ComponentType::ControlNetScribble ||
        type == ComponentType::ControlNetCanny ||
        type == ComponentType::ControlNetSdxlLineart ||
        type == ComponentType::ControlNetSdxlDepth ||
        type == ComponentType::ControlNetSdxlOpenpose ||
        type == ComponentType::ControlNetSdxlScribble ||
        type == ComponentType::ControlNetSdxlCanny) {
        ControlNetType cnType = ControlNetType::Lineart;
        bool isSdxl = false;
        if (type == ComponentType::ControlNetDepth || type == ComponentType::ControlNetSdxlDepth) cnType = ControlNetType::Depth;
        else if (type == ComponentType::ControlNetOpenpose || type == ComponentType::ControlNetSdxlOpenpose) cnType = ControlNetType::Openpose;
        else if (type == ComponentType::ControlNetScribble || type == ComponentType::ControlNetSdxlScribble) cnType = ControlNetType::Scribble;
        else if (type == ComponentType::ControlNetCanny || type == ComponentType::ControlNetSdxlCanny) cnType = ControlNetType::Canny;

        if (type == ComponentType::ControlNetSdxlLineart ||
            type == ComponentType::ControlNetSdxlDepth ||
            type == ComponentType::ControlNetSdxlOpenpose ||
            type == ComponentType::ControlNetSdxlScribble ||
            type == ComponentType::ControlNetSdxlCanny) {
            isSdxl = true;
        }

        QString found = AiService::findControlNetModelPath(cnType, isSdxl);
        if (!found.isEmpty()) {
            return found;
        }
        return info.targetDirectory + "/" + info.fileName;
    }

    if (type == ComponentType::EmbeddedSdExe) {
        QString pref = PreferencesDialog::embeddedEnginePath();
        if (!pref.isEmpty() && QFile::exists(pref)) return pref;

        if (QFile::exists(appDir + "/tools/sd-cli.exe")) return appDir + "/tools/sd-cli.exe";
        if (QFile::exists(appDir + "/tools/sd.exe")) return appDir + "/tools/sd.exe";
        if (QFile::exists(appDir + "/sd-cli.exe")) return appDir + "/sd-cli.exe";
        if (QFile::exists(appDir + "/sd.exe")) return appDir + "/sd.exe";

        if (QFile::exists(info.targetDirectory + "/sd-cli.exe")) return info.targetDirectory + "/sd-cli.exe";
        if (QFile::exists(info.targetDirectory + "/sd.exe")) return info.targetDirectory + "/sd.exe";

        return info.targetDirectory + "/sd.exe";
    }

    QStringList searchDirs;
    if (!info.targetDirectory.isEmpty()) searchDirs << info.targetDirectory;

    QString uDir = PreferencesDialog::upscaleModelDirectory();
    if (!uDir.isEmpty() && !searchDirs.contains(uDir)) searchDirs << uDir;

    // アップスケールモデルの場合、ComfyUIやモデルフォルダのサブフォルダも自動探索
    if (type == ComponentType::UpscaleModelAnime6B || type == ComponentType::UpscaleModelRealEsrgan) {
        QString mDir = PreferencesDialog::aiModelDirectory();
        if (!mDir.isEmpty()) {
            QDir md(mDir);
            searchDirs << md.filePath("upscale_models");
            searchDirs << md.filePath("ESRGAN");
            searchDirs << md.filePath("Upscaler");
            QDir parentDir = md;
            if (parentDir.cdUp()) {
                searchDirs << parentDir.filePath("upscale_models");
                searchDirs << parentDir.filePath("ESRGAN");
                searchDirs << parentDir.filePath("Upscaler");
            }
        }
        QString comfyPath = PreferencesDialog::comfyUiLaunchPath();
        if (!comfyPath.isEmpty()) {
            QFileInfo cfi(comfyPath);
            QDir cDir = cfi.absoluteDir();
            searchDirs << cDir.filePath("models/upscale_models");
            searchDirs << cDir.filePath("models/ESRGAN");
            searchDirs << cDir.filePath("Data/Models/Upscaler");
            searchDirs << cDir.filePath("Data/Models/ESRGAN");
        }
        searchDirs << (appDir + "/models/ESRGAN");
        searchDirs << (appDir + "/tools/upscale_models");
    }

    QStringList candidateNames;
    candidateNames << info.fileName;
    if (type == ComponentType::UpscaleModelAnime6B) {
        candidateNames << "R-ESRGAN 4x+ Anime6B.pth";
        candidateNames << "RealESRGAN_x4plus_anime_6B.safetensors";
        candidateNames << "R-ESRGAN 4x+ Anime6B.safetensors";
        candidateNames << "Anime6B.pth";
        candidateNames << "Anime6B.safetensors";
    } else if (type == ComponentType::UpscaleModelRealEsrgan) {
        candidateNames << "RealESRGAN_x4plus.safetensors";
        candidateNames << "RealESRGAN_x4plus.pth";
    }

    for (const QString& dirPath : searchDirs) {
        if (dirPath.isEmpty()) continue;
        QDir d(dirPath);
        if (!d.exists()) continue;
        for (const QString& fn : candidateNames) {
            QString fullPath = d.filePath(fn);
            if (QFile::exists(fullPath)) {
                return fullPath;
            }
        }
    }

    return info.targetDirectory + "/" + info.fileName;
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
