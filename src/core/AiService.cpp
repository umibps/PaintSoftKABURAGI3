#include "AiService.hpp"
#include <QBuffer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QUrl>
#include <QUrlQuery>
#include <QTimer>
#include <QProcess>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QDebug>
#include <QWebSocket>
#include <QUuid>
#include <QRegularExpression>

namespace Kaburagi {

AiService::AiService(QObject* parent)
    : QObject(parent)
    , m_netManager(new QNetworkAccessManager(this))
{
}

AiService::~AiService() {
    cancel();
    stopWebUiProgressPolling();
    disconnectComfyWebSocket();
    if (m_serverHealthTimer) {
        m_serverHealthTimer->stop();
    }
    if (m_comfyPollTimer) {
        m_comfyPollTimer->stop();
    }
}

bool AiService::isBusy() const {
    if (m_currentReply != nullptr && m_currentReply->isRunning()) return true;
    if (m_waitingServerLaunch) return true;
    if (m_comfyPollTimer != nullptr && m_comfyPollTimer->isActive()) return true;
    if (m_comfyWebSocket != nullptr && m_comfyWebSocket->state() == QAbstractSocket::ConnectedState) return true;
    if (m_embeddedProcess != nullptr && m_embeddedProcess->state() != QProcess::NotRunning) return true;
    return false;
}

QString AiService::imageToBase64Png(const QImage& img) {
    if (img.isNull()) return QString();
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    return QString::fromLatin1(byteArray.toBase64());
}

QImage AiService::base64ToImage(const QString& b64) {
    if (b64.isEmpty()) return QImage();
    QString cleanB64 = b64;
    int commaIdx = cleanB64.indexOf(',');
    if (commaIdx >= 0) {
        cleanB64 = cleanB64.mid(commaIdx + 1);
    }
    QByteArray data = QByteArray::fromBase64(cleanB64.toLatin1());
    QImage img;
    img.loadFromData(data, "PNG");
    return img;
}

QImage AiService::upscaleImage(const QImage& source, const QSize& targetSize, const QString& method) {
    if (source.isNull() || targetSize.isEmpty()) return source;
    if (source.size() == targetSize) return source;

    QString m = method.toLower().trimmed();
    if (m.contains("nearest") || m.contains("ニアレスト")) {
        return source.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    } else {
        // バイキュービック・Lanczos・Bilinear・AIアップスケーラー（RealESRGAN等）の平滑リサンプリング
        return source.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
}

QJsonObject AiService::buildWebUiTxt2ImgJson(const AiGenerateParams& params) {
    QJsonObject json;
    json["prompt"] = params.prompt;
    json["negative_prompt"] = params.negativePrompt;
    json["steps"] = params.steps;
    json["cfg_scale"] = params.cfgScale;
    json["width"] = params.width > 0 ? params.width : 512;
    json["height"] = params.height > 0 ? params.height : 512;
    json["batch_size"] = std::clamp(params.batchCount, 1, 8);
    json["n_iter"] = 1;
    json["seed"] = static_cast<qint64>(params.seed);
    json["sampler_name"] = params.sampler.isEmpty() ? "Euler a" : params.sampler;
    json["tiling"] = (params.mode == AiMode::SeamlessTexture) || params.tiling;

    if (params.enableControlNet && !params.controlNetImage.isNull()) {
        QJsonObject alwaysOn;
        QJsonObject cnObj;
        QJsonArray argsArr;
        QJsonObject unit;

        unit["enabled"] = true;
        unit["weight"] = params.controlNetWeight;
        unit["image"] = imageToBase64Png(params.controlNetImage);

        switch (params.controlNetType) {
        case ControlNetType::Lineart:
            unit["module"] = "lineart_standard (from white bg & black line)";
            unit["model"] = "control_v11p_sd15_lineart";
            break;
        case ControlNetType::Depth:
            unit["module"] = "depth";
            unit["model"] = "control_v11f1p_sd15_depth";
            break;
        case ControlNetType::Openpose:
            unit["module"] = "openpose_full";
            unit["model"] = "control_v11p_sd15_openpose";
            break;
        case ControlNetType::Scribble:
            unit["module"] = "scribble_xdog";
            unit["model"] = "control_v11p_sd15_scribble";
            break;
        case ControlNetType::Canny:
            unit["module"] = "canny";
            unit["model"] = "control_v11p_sd15_canny";
            break;
        }

        argsArr.append(unit);
        cnObj["args"] = argsArr;
        alwaysOn["controlnet"] = cnObj;
        json["alwayson_scripts"] = alwaysOn;
    }

    return json;
}

QJsonObject AiService::buildWebUiImg2ImgJson(const AiGenerateParams& params) {
    QJsonObject json = buildWebUiTxt2ImgJson(params);

    if (!params.initImage.isNull()) {
        QJsonArray initImages;
        initImages.append(imageToBase64Png(params.initImage));
        json["init_images"] = initImages;
    }

    json["denoising_strength"] = params.denoisingStrength;

    if (!params.maskImage.isNull() && params.mode == AiMode::Inpaint) {
        json["mask"] = imageToBase64Png(params.maskImage);
        json["mask_blur"] = 4;
        json["inpainting_fill"] = 1; // 1: original
        json["inpaint_full_res"] = true;
        json["inpaint_full_res_padding"] = 32;
        json["inpainting_mask_invert"] = 0; // 0: inpaint masked area
    }

    return json;
}

QJsonObject AiService::buildComfyUiPromptJson(const AiGenerateParams& params, const QString& uploadedInitImage) {
    QJsonObject prompt;

    // Node 4: CheckpointLoaderSimple
    QJsonObject node4;
    node4["class_type"] = "CheckpointLoaderSimple";
    QJsonObject node4Inputs;
    QString ckpt = params.modelName.trimmed();
    if (ckpt.contains('/') || ckpt.contains('\\')) {
        ckpt = QFileInfo(ckpt).fileName();
    }
    if (ckpt.isEmpty()) {
        ckpt = "v1-5-pruned-emaonly-fp16.safetensors";
    }
    node4Inputs["ckpt_name"] = ckpt;
    node4["inputs"] = node4Inputs;
    prompt["4"] = node4;

    // Node 6: Positive CLIPTextEncode
    QJsonObject node6;
    node6["class_type"] = "CLIPTextEncode";
    QJsonObject node6Inputs;
    node6Inputs["text"] = params.prompt;
    QJsonArray clipLink;
    clipLink.append("4");
    clipLink.append(1);
    node6Inputs["clip"] = clipLink;
    node6["inputs"] = node6Inputs;
    prompt["6"] = node6;

    // Node 7: Negative CLIPTextEncode
    QJsonObject node7;
    node7["class_type"] = "CLIPTextEncode";
    QJsonObject node7Inputs;
    node7Inputs["text"] = params.negativePrompt;
    node7Inputs["clip"] = clipLink;
    node7["inputs"] = node7Inputs;
    prompt["7"] = node7;

    // Latent source
    QString latentNodeId = "5";
    int latentOutIndex = 0;

    bool isImg2Img = (params.mode == AiMode::RoughToClean || params.mode == AiMode::Inpaint || params.mode == AiMode::Colorization) && !params.initImage.isNull();

    if (isImg2Img && !uploadedInitImage.isEmpty()) {
        // Node 10: LoadImage
        QJsonObject node10;
        node10["class_type"] = "LoadImage";
        QJsonObject node10Inputs;
        node10Inputs["image"] = uploadedInitImage;
        node10["inputs"] = node10Inputs;
        prompt["10"] = node10;

        // Node 11: VAEEncode
        QJsonObject node11;
        node11["class_type"] = "VAEEncode";
        QJsonObject node11Inputs;
        QJsonArray imgLink; imgLink.append("10"); imgLink.append(0);
        QJsonArray vaeLink; vaeLink.append("4"); vaeLink.append(2);
        node11Inputs["pixels"] = imgLink;
        node11Inputs["vae"] = vaeLink;
        node11["inputs"] = node11Inputs;
        prompt["11"] = node11;

        latentNodeId = "11";
    } else {
        // Node 5: EmptyLatentImage
        QJsonObject node5;
        node5["class_type"] = "EmptyLatentImage";
        QJsonObject node5Inputs;
        node5Inputs["width"] = params.width > 0 ? params.width : 512;
        node5Inputs["height"] = params.height > 0 ? params.height : 512;
        node5Inputs["batch_size"] = std::clamp(params.batchCount, 1, 8);
        node5["inputs"] = node5Inputs;
        prompt["5"] = node5;
    }

    // Node 3: KSampler
    QJsonObject node3;
    node3["class_type"] = "KSampler";
    QJsonObject node3Inputs;
    node3Inputs["seed"] = static_cast<qint64>(params.seed >= 0 ? params.seed : 123456789);
    node3Inputs["steps"] = params.steps;
    node3Inputs["cfg"] = params.cfgScale;
    node3Inputs["sampler_name"] = "euler";
    node3Inputs["scheduler"] = "normal";
    node3Inputs["denoise"] = isImg2Img ? params.denoisingStrength : 1.0;

    QJsonArray modelLink; modelLink.append("4"); modelLink.append(0);
    QJsonArray posLink; posLink.append("6"); posLink.append(0);
    QJsonArray negLink; negLink.append("7"); negLink.append(0);
    QJsonArray latLink; latLink.append(latentNodeId); latLink.append(latentOutIndex);

    node3Inputs["model"] = modelLink;
    node3Inputs["positive"] = posLink;
    node3Inputs["negative"] = negLink;
    node3Inputs["latent_image"] = latLink;
    node3["inputs"] = node3Inputs;
    prompt["3"] = node3;

    // Node 8: VAEDecode
    QJsonObject node8;
    node8["class_type"] = "VAEDecode";
    QJsonObject node8Inputs;
    QJsonArray sampleLink; sampleLink.append("3"); sampleLink.append(0);
    QJsonArray vaeLink; vaeLink.append("4"); vaeLink.append(2);
    node8Inputs["samples"] = sampleLink;
    node8Inputs["vae"] = vaeLink;
    node8["inputs"] = node8Inputs;
    prompt["8"] = node8;

    // Node 9: SaveImage
    QJsonObject node9;
    node9["class_type"] = "SaveImage";
    QJsonObject node9Inputs;
    node9Inputs["filename_prefix"] = "PaintSoftKABURAGI";
    QJsonArray imgInLink; imgInLink.append("8"); imgInLink.append(0);
    node9Inputs["images"] = imgInLink;
    node9["inputs"] = node9Inputs;
    prompt["9"] = node9;

    QJsonObject root;
    root["prompt"] = prompt;
    root["client_id"] = "PaintSoftKABURAGI";
    return root;
}

QJsonObject AiService::buildOpenAiJson(const AiGenerateParams& params) {
    QJsonObject json;
    json["prompt"] = params.prompt;
    json["model"] = params.modelName.isEmpty() ? "dall-e-3" : params.modelName;
    json["n"] = std::clamp(params.batchCount, 1, 4);
    json["size"] = QString("%1x%2").arg(params.width).arg(params.height);
    json["response_format"] = "b64_json";
    return json;
}

QList<QImage> AiService::parseWebUiResponse(const QByteArray& responseData, QString* outError) {
    QList<QImage> result;
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (doc.isNull() || !doc.isObject()) {
        if (outError) *outError = "Invalid JSON response from WebUI";
        return result;
    }

    QJsonObject obj = doc.object();
    if (obj.contains("error")) {
        if (outError) *outError = obj["error"].toString();
        return result;
    }
    if (obj.contains("detail")) {
        if (outError) *outError = obj["detail"].toString();
        return result;
    }

    if (obj.contains("images") && obj["images"].isArray()) {
        QJsonArray arr = obj["images"].toArray();
        for (const auto& val : arr) {
            QString b64 = val.toString();
            QImage img = base64ToImage(b64);
            if (!img.isNull()) {
                result.append(img);
            }
        }
    }

    return result;
}

QList<QImage> AiService::parseOpenAiResponse(const QByteArray& responseData, QString* outError) {
    QList<QImage> result;
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (doc.isNull() || !doc.isObject()) {
        if (outError) *outError = "Invalid JSON response from OpenAI";
        return result;
    }

    QJsonObject obj = doc.object();
    if (obj.contains("error")) {
        QJsonObject errObj = obj["error"].toObject();
        if (outError) *outError = errObj["message"].toString();
        return result;
    }

    if (obj.contains("data") && obj["data"].isArray()) {
        QJsonArray arr = obj["data"].toArray();
        for (const auto& val : arr) {
            QJsonObject item = val.toObject();
            if (item.contains("b64_json")) {
                QString b64 = item["b64_json"].toString();
                QImage img = base64ToImage(b64);
                if (!img.isNull()) {
                    result.append(img);
                }
            }
        }
    }

    return result;
}

QJsonObject AiService::paramsToJson(const AiGenerateParams& params) {
    QJsonObject json;
    json["backend"] = static_cast<int>(params.backend);
    json["serverUrl"] = params.serverUrl;
    json["apiKey"] = params.apiKey;
    json["modelName"] = params.modelName;

    json["mode"] = static_cast<int>(params.mode);
    json["prompt"] = params.prompt;
    json["negativePrompt"] = params.negativePrompt;

    json["batchCount"] = params.batchCount;
    json["steps"] = params.steps;
    json["cfgScale"] = static_cast<double>(params.cfgScale);
    json["denoisingStrength"] = static_cast<double>(params.denoisingStrength);
    json["seed"] = static_cast<qint64>(params.seed);
    json["width"] = params.width;
    json["height"] = params.height;
    json["sampler"] = params.sampler;
    json["tiling"] = params.tiling;

    json["enableControlNet"] = params.enableControlNet;
    json["controlNetType"] = static_cast<int>(params.controlNetType);
    json["controlNetWeight"] = static_cast<double>(params.controlNetWeight);

    json["computeDevice"] = params.computeDevice;
    json["upscaleMethod"] = params.upscaleMethod;

    return json;
}

AiGenerateParams AiService::jsonToParams(const QJsonObject& json) {
    AiGenerateParams params;
    if (json.contains("backend")) params.backend = static_cast<ApiBackend>(json["backend"].toInt());
    if (json.contains("serverUrl")) params.serverUrl = json["serverUrl"].toString();
    if (json.contains("apiKey")) params.apiKey = json["apiKey"].toString();
    if (json.contains("modelName")) params.modelName = json["modelName"].toString();

    if (json.contains("mode")) params.mode = static_cast<AiMode>(json["mode"].toInt());
    if (json.contains("prompt")) params.prompt = json["prompt"].toString();
    if (json.contains("negativePrompt")) params.negativePrompt = json["negativePrompt"].toString();

    if (json.contains("batchCount")) params.batchCount = json["batchCount"].toInt(1);
    if (json.contains("steps")) params.steps = json["steps"].toInt(20);
    if (json.contains("cfgScale")) params.cfgScale = static_cast<float>(json["cfgScale"].toDouble(7.0));
    if (json.contains("denoisingStrength")) params.denoisingStrength = static_cast<float>(json["denoisingStrength"].toDouble(0.55));
    if (json.contains("seed")) params.seed = json["seed"].toInteger(-1);
    if (json.contains("width")) params.width = json["width"].toInt(512);
    if (json.contains("height")) params.height = json["height"].toInt(512);
    if (json.contains("sampler")) params.sampler = json["sampler"].toString("Euler a");
    if (json.contains("tiling")) params.tiling = json["tiling"].toBool(false);

    if (json.contains("enableControlNet")) params.enableControlNet = json["enableControlNet"].toBool(false);
    if (json.contains("controlNetType")) params.controlNetType = static_cast<ControlNetType>(json["controlNetType"].toInt(0));
    if (json.contains("controlNetWeight")) params.controlNetWeight = static_cast<float>(json["controlNetWeight"].toDouble(1.0));

    if (json.contains("computeDevice")) params.computeDevice = json["computeDevice"].toString("cuda");
    if (json.contains("upscaleMethod")) params.upscaleMethod = json["upscaleMethod"].toString("bicubic");

    return params;
}

void AiService::fetchModels(ApiBackend backend, const QString& serverUrl, const QString& apiKey) {
    Q_UNUSED(apiKey);
    if (backend == ApiBackend::CloudOpenAI) {
        QStringList models = {"dall-e-3", "dall-e-2"};
        emit modelsFetched(models);
        return;
    }

    if (backend == ApiBackend::CloudStability) {
        QStringList models = {"core", "ultra", "sd3-large", "sd3-medium"};
        emit modelsFetched(models);
        return;
    }

    if (backend == ApiBackend::EmbeddedEngine) {
        // 組み込みエンジンはモデル保存フォルダ内のファイルをモデルとする
        return;
    }

    if (m_modelsReply) {
        QNetworkReply* oldReply = m_modelsReply;
        m_modelsReply = nullptr;
        disconnect(oldReply, nullptr, this, nullptr);
        oldReply->abort();
        oldReply->deleteLater();
    }

    QString urlStr = serverUrl.trimmed();
    if (urlStr.endsWith('/')) urlStr.chop(1);

    QUrl url;
    if (backend == ApiBackend::LocalComfyUI) {
        url = QUrl(urlStr + "/object_info/CheckpointLoaderSimple");
    } else {
        url = QUrl(urlStr + "/sdapi/v1/sd-models");
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    m_modelsReply = m_netManager->get(request);
    if (m_modelsReply) {
        connect(m_modelsReply, &QNetworkReply::finished, this, &AiService::onModelsReplyFinished);
    }
}

void AiService::onModelsReplyFinished() {
    if (!m_modelsReply) return;
    QNetworkReply* reply = m_modelsReply;
    m_modelsReply = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit modelsFetchFailed(QString("モデル取得失敗: %1").arg(reply->errorString()));
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QStringList models;

    if (m_currentParams.backend == ApiBackend::LocalComfyUI) {
        // ComfyUI object_info format
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("CheckpointLoaderSimple")) {
                QJsonObject node = obj["CheckpointLoaderSimple"].toObject();
                QJsonObject inObj = node["input"].toObject()["required"].toObject();
                if (inObj.contains("ckpt_name")) {
                    QJsonArray ckptArr = inObj["ckpt_name"].toArray()[0].toArray();
                    for (const auto& v : ckptArr) {
                        models.append(v.toString());
                    }
                }
            }
        }
    } else {
        // WebUI format
        if (doc.isArray()) {
            QJsonArray arr = doc.array();
            for (const auto& val : arr) {
                QJsonObject obj = val.toObject();
                QString title = obj["title"].toString();
                if (!title.isEmpty()) {
                    models.append(title);
                } else {
                    QString modelName = obj["model_name"].toString();
                    if (!modelName.isEmpty()) models.append(modelName);
                }
            }
        }
    }

    if (!models.isEmpty()) {
        emit modelsFetched(models);
    }
}

void AiService::generate(const AiGenerateParams& params) {
    if (isBusy()) {
        emit generationFailed(tr("現在別の生成タスクが実行中です。"));
        return;
    }

    m_currentParams = params;
    m_waitingServerLaunch = false;
    executeGenerate(params);
}

void AiService::executeGenerate(const AiGenerateParams& params) {
    m_currentParams = params;
    emit generationProgress(5, tr("生成準備中..."));

    if (params.backend == ApiBackend::EmbeddedEngine) {
        executeEmbeddedInference(params);
        return;
    }

    if (params.backend == ApiBackend::LocalComfyUI) {
        executeComfyUiGenerate(params);
        return;
    }

    QString urlStr = params.serverUrl.trimmed();
    if (urlStr.endsWith('/')) urlStr.chop(1);

    if (params.backend == ApiBackend::LocalWebUI) {
        bool isImg2Img = (params.mode == AiMode::RoughToClean ||
                          params.mode == AiMode::Inpaint ||
                          params.mode == AiMode::Colorization) && !params.initImage.isNull();

        QUrl url(urlStr + (isImg2Img ? "/sdapi/v1/img2img" : "/sdapi/v1/txt2img"));
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QJsonObject jsonBody = isImg2Img ? buildWebUiImg2ImgJson(params) : buildWebUiTxt2ImgJson(params);
        QByteArray data = QJsonDocument(jsonBody).toJson(QJsonDocument::Compact);

        m_currentReply = m_netManager->post(request, data);
        connect(m_currentReply, &QNetworkReply::finished, this, &AiService::onGenerateReplyFinished);
        emit generationProgress(15, tr("ローカルAIサーバーへリクエスト送信中..."));
        startWebUiProgressPolling();
    } else if (params.backend == ApiBackend::CloudOpenAI) {
        QUrl url("https://api.openai.com/v1/images/generations");
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", QString("Bearer %1").arg(params.apiKey.trimmed()).toUtf8());

        QJsonObject jsonBody = buildOpenAiJson(params);
        QByteArray data = QJsonDocument(jsonBody).toJson(QJsonDocument::Compact);

        m_currentReply = m_netManager->post(request, data);
        connect(m_currentReply, &QNetworkReply::finished, this, &AiService::onGenerateReplyFinished);
        emit generationProgress(15, tr("OpenAIクラウドへリクエスト送信中..."));
    } else {
        emit generationFailed(tr("未対応のバックエンドです。"));
    }
}

void AiService::executeComfyUiGenerate(const AiGenerateParams& params) {
    QString urlStr = params.serverUrl.trimmed();
    if (urlStr.endsWith('/')) urlStr.chop(1);

    connectComfyWebSocket(urlStr);

    bool isImg2Img = (params.mode == AiMode::RoughToClean ||
                      params.mode == AiMode::Inpaint ||
                      params.mode == AiMode::Colorization) && !params.initImage.isNull();

    if (isImg2Img) {
        emit generationProgress(10, tr("初期画像をComfyUIへアップロード中..."));
        QUrl uploadUrl(urlStr + "/upload/image");
        QNetworkRequest req(uploadUrl);

        auto multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

        QHttpPart imagePart;
        imagePart.setHeader(QNetworkRequest::ContentDispositionHeader,
            QVariant("form-data; name=\"image\"; filename=\"kaburagi_init.png\""));
        imagePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("image/png"));

        QByteArray imgBytes;
        QBuffer buffer(&imgBytes);
        buffer.open(QIODevice::WriteOnly);
        params.initImage.save(&buffer, "PNG");
        imagePart.setBody(imgBytes);
        multiPart->append(imagePart);

        QHttpPart overwritePart;
        overwritePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"overwrite\""));
        overwritePart.setBody("true");
        multiPart->append(overwritePart);

        m_currentReply = m_netManager->post(req, multiPart);
        multiPart->setParent(m_currentReply);

        connect(m_currentReply, &QNetworkReply::finished, this, [this, params, urlStr]() {
            if (!m_currentReply) return;
            QNetworkReply* reply = m_currentReply;
            m_currentReply = nullptr;
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                if (reply->error() == QNetworkReply::ConnectionRefusedError && params.autoLaunchServer && !m_waitingServerLaunch) {
                    startServerLaunchAndRetry(ApiBackend::LocalComfyUI, params.comfyUiLaunchPath);
                    return;
                }
                emit generationFailed(QString("ComfyUIへの画像アップロード失敗: %1").arg(reply->errorString()));
                return;
            }

            QJsonObject upJson = QJsonDocument::fromJson(reply->readAll()).object();
            QString uploadedName = upJson["name"].toString();
            if (uploadedName.isEmpty()) uploadedName = "kaburagi_init.png";

            // prompt POST
            QUrl pUrl(urlStr + "/prompt");
            QNetworkRequest pReq(pUrl);
            pReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

            QJsonObject pBody;
            pBody["prompt"] = buildComfyUiPromptJson(params, uploadedName);
            if (!m_comfyClientId.isEmpty()) {
                pBody["client_id"] = m_comfyClientId;
            }
            m_currentReply = m_netManager->post(pReq, QJsonDocument(pBody).toJson(QJsonDocument::Compact));
            connect(m_currentReply, &QNetworkReply::finished, this, &AiService::onGenerateReplyFinished);
            emit generationProgress(20, tr("ComfyUIワークフローをキューに追加中..."));
        });
        return;
    }

    // txt2img
    QUrl url(urlStr + "/prompt");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject jsonBody;
    jsonBody["prompt"] = buildComfyUiPromptJson(params);
    if (!m_comfyClientId.isEmpty()) {
        jsonBody["client_id"] = m_comfyClientId;
    }
    QByteArray data = QJsonDocument(jsonBody).toJson(QJsonDocument::Compact);

    m_currentReply = m_netManager->post(request, data);
    connect(m_currentReply, &QNetworkReply::finished, this, &AiService::onGenerateReplyFinished);
    emit generationProgress(15, tr("ComfyUIワークフローをキューに追加中..."));
}

void AiService::executeEmbeddedInference(const AiGenerateParams& params) {
    QString exePath = params.embeddedEnginePath;
    if (exePath.isEmpty() || !QFile::exists(exePath)) {
        emit generationFailed(tr("組み込み推論エンジンが見つかりません。環境設定で sd.exe (stable-diffusion.cpp) の実行ファイルパスを指定するか、ComfyUI / WebUI をご利用ください。"));
        return;
    }

    QString modelDir = params.modelDirectory;
    QString modelPath = params.modelName;
    if (!modelDir.isEmpty() && !QDir::isAbsolutePath(modelPath)) {
        modelPath = QDir(modelDir).filePath(modelPath);
    }

    QString tempOutput = QDir::temp().filePath(QString("kaburagi_embed_%1.png").arg(QDateTime::currentMSecsSinceEpoch()));
    m_embeddedOutputPath = tempOutput;

    QStringList args;
    args << "-m" << modelPath;
    args << "-p" << params.prompt;
    if (!params.negativePrompt.isEmpty()) {
        args << "-n" << params.negativePrompt;
    }
    args << "--steps" << QString::number(params.steps);
    args << "--cfg-scale" << QString::number(params.cfgScale);
    args << "-W" << QString::number(params.width > 0 ? params.width : 512);
    args << "-H" << QString::number(params.height > 0 ? params.height : 512);
    args << "-b" << QString::number(std::clamp(params.batchCount, 1, 8));
    if (params.seed >= 0) {
        args << "-s" << QString::number(params.seed);
    }
    args << "-o" << tempOutput;

    emit generationProgress(20, tr("組み込み推論エンジン(sd.exe)を実行中..."));

    if (m_embeddedProcess) {
        m_embeddedProcess->kill();
        m_embeddedProcess->deleteLater();
    }
    m_embeddedProcess = new QProcess(this);
    connect(m_embeddedProcess, &QProcess::readyReadStandardOutput, this, &AiService::onEmbeddedProcessReadyRead);
    connect(m_embeddedProcess, &QProcess::readyReadStandardError, this, &AiService::onEmbeddedProcessReadyRead);
    connect(m_embeddedProcess, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus) {
        onEmbeddedProcessFinished(exitCode);
    });

    m_embeddedProcess->start(exePath, args);
}

void AiService::onEmbeddedProcessFinished(int exitCode) {
    if (exitCode == 0 && QFile::exists(m_embeddedOutputPath)) {
        QImage result(m_embeddedOutputPath);
        QFile::remove(m_embeddedOutputPath);
        if (!result.isNull()) {
            emit generationProgress(100, tr("生成完了！"));
            emit generationFinished({result});
            return;
        }
    }
    emit generationFailed(tr("組み込み推論エンジンの実行に失敗しました (終了コード: %1)").arg(exitCode));
}

void AiService::startServerLaunchAndRetry(ApiBackend backend, const QString& launchPath) {
    if (launchPath.isEmpty() || !QFile::exists(launchPath)) {
        QString msg = (backend == ApiBackend::LocalComfyUI)
            ? tr("ComfyUIが起動していません。環境設定でComfyUIの起動ファイル(run_nvidia_gpu.bat等)を設定してください。")
            : tr("Web UIが起動していません。環境設定でWeb UIの起動ファイル(webui-user.bat等)を設定してください。");
        emit generationFailed(msg);
        return;
    }

    QString serverName = (backend == ApiBackend::LocalComfyUI) ? "ComfyUI" : "Web UI";
    emit serverAutoLaunched(serverName);
    emit generationProgress(10, tr("%1 を自動起動しています... (数秒〜数十秒お待ちください)").arg(serverName));

    QFileInfo fi(launchPath);
    QString baseDir = fi.absolutePath();

    if (backend == ApiBackend::LocalComfyUI) {
        // ComfyUI: ブラウザを立ち上げないよう --disable-auto-launch を指定
        QString pyEmbed = QDir(baseDir).filePath("python_embeded/python.exe");
        QString mainPy = QDir(baseDir).filePath("ComfyUI/main.py");
        if (!QFile::exists(mainPy)) {
            mainPy = QDir(baseDir).filePath("main.py");
        }

        QStringList args = {"-s", mainPy, "--disable-auto-launch"};
        if (m_currentParams.computeDevice == "cpu") {
            args << "--cpu";
        } else if (m_currentParams.computeDevice == "openvino" || m_currentParams.computeDevice == "rocm") {
            args << "--directml";
        }

        if (QFile::exists(pyEmbed) && QFile::exists(mainPy)) {
            QProcess::startDetached(pyEmbed, args, baseDir);
        } else {
            QStringList batArgs = {"--disable-auto-launch"};
            if (m_currentParams.computeDevice == "cpu") batArgs << "--cpu";
            QProcess::startDetached(launchPath, batArgs, baseDir);
        }
    } else {
        // Web UI: ブラウザ自動オープンを防ぐため --nowebui または 引数付き実行
        QStringList webArgs = {"--nowebui"};
        if (m_currentParams.computeDevice == "cpu") {
            webArgs << "--use-cpu" << "all";
        }
        QProcess::startDetached(launchPath, webArgs, baseDir);
    }

    m_waitingServerLaunch = true;
    m_serverHealthAttempts = 0;
    m_serverHealthCheckUrl = m_currentParams.serverUrl;

    if (!m_serverHealthTimer) {
        m_serverHealthTimer = new QTimer(this);
        connect(m_serverHealthTimer, &QTimer::timeout, this, &AiService::onServerHealthCheckTimer);
    }
    m_serverHealthTimer->start(2500);
}

void AiService::onServerHealthCheckTimer() {
    m_serverHealthAttempts++;
    if (m_serverHealthAttempts > 36) { // 最大90秒待機
        m_serverHealthTimer->stop();
        m_waitingServerLaunch = false;
        emit generationFailed(tr("AIサーバーの起動待機がタイムアウトしました。コマンド画面等でサーバーの状態をご確認ください。"));
        return;
    }

    emit generationProgress(10 + std::min(m_serverHealthAttempts * 2, 40),
        tr("サーバー起動待機中... (%1秒経過)").arg(m_serverHealthAttempts * 2));

    checkServerHealth();
}

void AiService::checkServerHealth() {
    QString urlStr = m_serverHealthCheckUrl.trimmed();
    if (urlStr.endsWith('/')) urlStr.chop(1);

    QUrl url;
    if (m_currentParams.backend == ApiBackend::LocalComfyUI) {
        url = QUrl(urlStr + "/system_stats");
    } else {
        url = QUrl(urlStr + "/sdapi/v1/options");
    }

    QNetworkRequest req(url);
    QNetworkReply* reply = m_netManager->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            if (m_serverHealthTimer) m_serverHealthTimer->stop();
            m_waitingServerLaunch = false;
            emit generationProgress(30, tr("サーバー接続完了！画像生成を開始します..."));
            executeGenerate(m_currentParams);
        }
    });
}

void AiService::onGenerateReplyFinished() {
    if (!m_currentReply) return;
    QNetworkReply* reply = m_currentReply;
    m_currentReply = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        stopWebUiProgressPolling();
        // ConnectionRefused 検出時の自動起動判定
        if (reply->error() == QNetworkReply::ConnectionRefusedError && m_currentParams.autoLaunchServer && !m_waitingServerLaunch) {
            if (m_currentParams.backend == ApiBackend::LocalComfyUI) {
                startServerLaunchAndRetry(ApiBackend::LocalComfyUI, m_currentParams.comfyUiLaunchPath);
                return;
            } else if (m_currentParams.backend == ApiBackend::LocalWebUI) {
                startServerLaunchAndRetry(ApiBackend::LocalWebUI, m_currentParams.webUiLaunchPath);
                return;
            }
        }

        emit generationFailed(QString("生成エラー: %1").arg(reply->errorString()));
        return;
    }

    QByteArray data = reply->readAll();
    QString err;
    QList<QImage> results;

    if (m_currentParams.backend == ApiBackend::LocalComfyUI) {
        QJsonObject promptRes = QJsonDocument::fromJson(data).object();
        QString promptId = promptRes["prompt_id"].toString();
        if (!promptId.isEmpty()) {
            pollComfyUiHistory(promptId);
            return;
        } else {
            disconnectComfyWebSocket();
            QString errorMsg;
            if (promptRes.contains("error")) {
                QJsonObject errObj = promptRes["error"].toObject();
                errorMsg = errObj["message"].toString();
            }
            if (promptRes.contains("node_errors")) {
                QJsonObject nodeErrors = promptRes["node_errors"].toObject();
                for (auto it = nodeErrors.begin(); it != nodeErrors.end(); ++it) {
                    QJsonObject nObj = it.value().toObject();
                    QJsonArray errArr = nObj["errors"].toArray();
                    for (const auto& ev : errArr) {
                        if (!errorMsg.isEmpty()) errorMsg += "\n";
                        errorMsg += QString("・ノード %1 (%2): %3").arg(it.key(), nObj["class_type"].toString(), ev.toObject()["message"].toString());
                    }
                }
            }
            if (errorMsg.isEmpty()) {
                errorMsg = QString::fromUtf8(data).trimmed();
                if (errorMsg.length() > 200) errorMsg = errorMsg.left(200) + "...";
            }
            emit generationFailed(tr("ComfyUIエラー:\n%1").arg(errorMsg.isEmpty() ? tr("prompt_idを取得できませんでした") : errorMsg));
            return;
        }
    } else if (m_currentParams.backend == ApiBackend::LocalWebUI) {
        stopWebUiProgressPolling();
        results = parseWebUiResponse(data, &err);
    } else if (m_currentParams.backend == ApiBackend::CloudOpenAI) {
        results = parseOpenAiResponse(data, &err);
    }

    if (!err.isEmpty()) {
        emit generationFailed(QString("APIエラー: %1").arg(err));
        return;
    }

    if (results.isEmpty()) {
        emit generationFailed(tr("画像を生成できませんでした (レスポンスが空です)。"));
        return;
    }

    emit generationProgress(100, tr("生成完了！"));
    emit generationFinished(results);
}

void AiService::pollComfyUiHistory(const QString& promptId) {
    m_comfyPromptId = promptId;
    m_comfyPollCount = 0;
    emit generationProgress(30, tr("ComfyUIにて推論中..."));

    if (!m_comfyPollTimer) {
        m_comfyPollTimer = new QTimer(this);
        connect(m_comfyPollTimer, &QTimer::timeout, this, &AiService::onComfyHistoryPollTimer);
    }
    m_comfyPollTimer->start(1000);
}

void AiService::onComfyHistoryPollTimer() {
    m_comfyPollCount++;
    if (m_comfyPollCount > 180) { // 最大3分
        m_comfyPollTimer->stop();
        disconnectComfyWebSocket();
        emit generationFailed(tr("ComfyUIの生成待機がタイムアウトしました。"));
        return;
    }

    // WebSocket 未接続または進捗メッセージ未受信時のフォールバック表示
    if (!m_comfyWebSocket || m_comfyWebSocket->state() != QAbstractSocket::ConnectedState) {
        emit generationProgress(30 + std::min(m_comfyPollCount, 60), tr("ComfyUIにて推論中... (%1s)").arg(m_comfyPollCount));
    }

    QString urlStr = m_currentParams.serverUrl.trimmed();
    if (urlStr.endsWith('/')) urlStr.chop(1);
    QUrl histUrl(urlStr + QString("/history/%1").arg(m_comfyPromptId));

    QNetworkRequest req(histUrl);
    QNetworkReply* reply = m_netManager->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;

        QByteArray data = reply->readAll();
        QJsonObject histObj = QJsonDocument::fromJson(data).object();
        if (histObj.contains(m_comfyPromptId)) {
            QJsonObject pObj = histObj[m_comfyPromptId].toObject();
            QJsonObject outputs = pObj["outputs"].toObject();
            QStringList imageFilenames;
            for (auto it = outputs.begin(); it != outputs.end(); ++it) {
                QJsonObject nodeOut = it.value().toObject();
                if (nodeOut.contains("images")) {
                    QJsonArray imgArr = nodeOut["images"].toArray();
                    for (const auto& iv : imgArr) {
                        QJsonObject imgObj = iv.toObject();
                        QString fname = imgObj["filename"].toString();
                        if (!fname.isEmpty()) {
                            imageFilenames.append(fname);
                        }
                    }
                }
            }

            if (!imageFilenames.isEmpty()) {
                m_comfyPollTimer->stop();
                disconnectComfyWebSocket();
                downloadComfyUiImages(imageFilenames);
            }
        }
    });
}

void AiService::downloadComfyUiImages(const QStringList& imageFilenames) {
    emit generationProgress(95, tr("生成画像をダウンロード中..."));

    auto collectedImages = std::make_shared<QList<QImage>>();
    auto remainingCount = std::make_shared<int>(imageFilenames.size());

    QString urlStr = m_currentParams.serverUrl.trimmed();
    if (urlStr.endsWith('/')) urlStr.chop(1);

    for (const auto& fname : imageFilenames) {
        QUrl viewUrl(urlStr + QString("/view?filename=%1&type=output").arg(fname));
        QNetworkRequest req(viewUrl);
        QNetworkReply* reply = m_netManager->get(req);

        connect(reply, &QNetworkReply::finished, this, [this, reply, collectedImages, remainingCount]() {
            reply->deleteLater();
            (*remainingCount)--;
            if (reply->error() == QNetworkReply::NoError) {
                QImage img;
                img.loadFromData(reply->readAll());
                if (!img.isNull()) {
                    collectedImages->append(img);
                }
            }

            if (*remainingCount == 0) {
                disconnectComfyWebSocket();
                if (collectedImages->isEmpty()) {
                    emit generationFailed(tr("ComfyUIからの画像取得に失敗しました。"));
                } else {
                    emit generationProgress(100, tr("生成完了！"));
                    emit generationFinished(*collectedImages);
                }
            }
        });
    }
}

void AiService::cancel() {
    stopWebUiProgressPolling();
    disconnectComfyWebSocket();
    if (m_serverHealthTimer) m_serverHealthTimer->stop();
    if (m_comfyPollTimer) m_comfyPollTimer->stop();
    m_waitingServerLaunch = false;

    if (m_embeddedProcess && m_embeddedProcess->state() != QProcess::NotRunning) {
        m_embeddedProcess->kill();
    }

    if (m_currentReply) {
        QNetworkReply* reply = m_currentReply;
        m_currentReply = nullptr;
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();

        // WebUIの場合、interrupt APIを送信
        if (m_currentParams.backend == ApiBackend::LocalWebUI) {
            QString urlStr = m_currentParams.serverUrl.trimmed();
            if (urlStr.endsWith('/')) urlStr.chop(1);
            QUrl url(urlStr + "/sdapi/v1/interrupt");
            QNetworkRequest request(url);
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            m_netManager->post(request, QByteArray("{}"));
        } else if (m_currentParams.backend == ApiBackend::LocalComfyUI) {
            // ComfyUIの場合、interrupt APIを送信
            QString urlStr = m_currentParams.serverUrl.trimmed();
            if (urlStr.endsWith('/')) urlStr.chop(1);
            QUrl url(urlStr + "/interrupt");
            QNetworkRequest request(url);
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            m_netManager->post(request, QByteArray("{}"));
        }
        emit generationProgress(0, tr("生成をキャンセルしました。"));
    }
}

void AiService::startWebUiProgressPolling() {
    stopWebUiProgressPolling();
    if (!m_webUiProgressTimer) {
        m_webUiProgressTimer = new QTimer(this);
        connect(m_webUiProgressTimer, &QTimer::timeout, this, &AiService::onWebUiProgressTimer);
    }
    m_webUiProgressTimer->start(500);
}

void AiService::stopWebUiProgressPolling() {
    if (m_webUiProgressTimer) {
        m_webUiProgressTimer->stop();
    }
    if (m_webUiProgressReply) {
        QNetworkReply* r = m_webUiProgressReply;
        m_webUiProgressReply = nullptr;
        disconnect(r, nullptr, this, nullptr);
        r->abort();
        r->deleteLater();
    }
}

void AiService::onWebUiProgressTimer() {
    if (m_webUiProgressReply) return;
    QString urlStr = m_currentParams.serverUrl.trimmed();
    if (urlStr.endsWith('/')) urlStr.chop(1);
    QUrl pUrl(urlStr + "/sdapi/v1/progress?skip_current_image=true");
    QNetworkRequest req(pUrl);
    m_webUiProgressReply = m_netManager->get(req);
    connect(m_webUiProgressReply, &QNetworkReply::finished, this, [this]() {
        if (!m_webUiProgressReply) return;
        QNetworkReply* r = m_webUiProgressReply;
        m_webUiProgressReply = nullptr;
        r->deleteLater();

        if (r->error() == QNetworkReply::NoError) {
            QByteArray data = r->readAll();
            QJsonObject root = QJsonDocument::fromJson(data).object();
            double progress = root["progress"].toDouble();
            QJsonObject state = root["state"].toObject();
            int step = state["sampling_step"].toInt();
            int totalSteps = state["sampling_steps"].toInt();
            double eta = root["eta_relative"].toDouble();

            if (progress > 0.0) {
                int percent = std::clamp(static_cast<int>(progress * 100), 10, 95);
                QString msg;
                if (totalSteps > 0) {
                    msg = tr("Web UI 推論中... (%1% - ステップ %2/%3 - 残り約 %4秒)").arg(percent).arg(step).arg(totalSteps).arg(std::max(0, qRound(eta)));
                } else {
                    msg = tr("Web UI 推論中... (%1% - 残り約 %2秒)").arg(percent).arg(std::max(0, qRound(eta)));
                }
                emit generationProgress(percent, msg);
            }
        }
    });
}

void AiService::connectComfyWebSocket(const QString& serverUrl) {
    disconnectComfyWebSocket();

    m_comfyClientId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QUrl httpUrl(serverUrl);
    QUrl wsUrl;
    wsUrl.setScheme(httpUrl.scheme().compare("https", Qt::CaseInsensitive) == 0 ? "wss" : "ws");
    wsUrl.setHost(httpUrl.host().isEmpty() ? "127.0.0.1" : httpUrl.host());
    wsUrl.setPort(httpUrl.port() > 0 ? httpUrl.port() : 8188);
    wsUrl.setPath("/ws");
    QUrlQuery query;
    query.addQueryItem("clientId", m_comfyClientId);
    wsUrl.setQuery(query);

    if (!m_comfyWebSocket) {
        m_comfyWebSocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
        connect(m_comfyWebSocket, &QWebSocket::textMessageReceived, this, &AiService::onComfyWebSocketTextMessageReceived);
    }

    m_comfyWebSocket->open(wsUrl);
}

void AiService::disconnectComfyWebSocket() {
    if (m_comfyWebSocket) {
        m_comfyWebSocket->close();
    }
}

void AiService::onComfyWebSocketTextMessageReceived(const QString& message) {
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) return;
    QJsonObject root = doc.object();
    QString type = root["type"].toString();
    QJsonObject data = root["data"].toObject();

    if (type == "status") {
        QJsonObject status = data["status"].toObject();
        QJsonObject execInfo = status["exec_info"].toObject();
        int queueRemaining = execInfo["queue_remaining"].toInt();
        if (queueRemaining > 1) {
            emit generationProgress(15, tr("ComfyUI キュー待機中 (残り %1 件)...").arg(queueRemaining));
        }
    } else if (type == "execution_start") {
        emit generationProgress(20, tr("ComfyUI 推論開始..."));
    } else if (type == "executing") {
        QString node = data["node"].toString();
        if (!node.isEmpty()) {
            emit generationProgress(25, tr("ComfyUI ノード処理中 (Node %1)...").arg(node));
        }
    } else if (type == "progress") {
        int value = data["value"].toInt();
        int max = data["max"].toInt();
        if (max > 0) {
            int percent = std::clamp(20 + static_cast<int>((static_cast<double>(value) / max) * 75.0), 20, 95);
            emit generationProgress(percent, tr("ComfyUI サンプリング中... (%1% - ステップ %2/%3)").arg(percent).arg(value).arg(max));
        }
    } else if (type == "executed") {
        if (data.contains("output")) {
            QJsonObject output = data["output"].toObject();
            if (output.contains("images")) {
                QJsonArray imgArr = output["images"].toArray();
                QStringList filenames;
                for (const auto& iv : imgArr) {
                    QString fname = iv.toObject()["filename"].toString();
                    if (!fname.isEmpty()) filenames.append(fname);
                }
                if (!filenames.isEmpty()) {
                    if (m_comfyPollTimer) m_comfyPollTimer->stop();
                    disconnectComfyWebSocket();
                    downloadComfyUiImages(filenames);
                }
            }
        }
    }
}

void AiService::onEmbeddedProcessReadyRead() {
    if (!m_embeddedProcess) return;

    QByteArray data = m_embeddedProcess->readAllStandardOutput();
    data += m_embeddedProcess->readAllStandardError();
    QString text = QString::fromUtf8(data);

    QStringList lines = text.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        if (trimmed.contains("loading model", Qt::CaseInsensitive) || trimmed.contains("load_model", Qt::CaseInsensitive)) {
            emit generationProgress(10, tr("組み込み推論: モデル読み込み中..."));
        } else if (trimmed.contains("applying", Qt::CaseInsensitive) || trimmed.contains("clip", Qt::CaseInsensitive)) {
            emit generationProgress(15, tr("組み込み推論: プロンプト解析中..."));
        }

        // Match "12/20" or "step 12/20" or "iteration 12/20"
        static QRegularExpression stepRegex(R"((?:step|sampling|it|\b)?\s*(\d+)\s*/\s*(\d+))", QRegularExpression::CaseInsensitiveOption);
        auto match = stepRegex.match(trimmed);
        if (match.hasMatch()) {
            int curStep = match.captured(1).toInt();
            int totalSteps = match.captured(2).toInt();
            if (totalSteps > 0 && curStep <= totalSteps && curStep > 0) {
                int percent = std::clamp(20 + static_cast<int>((static_cast<double>(curStep) / totalSteps) * 75.0), 20, 95);
                emit generationProgress(percent, tr("組み込み推論中... (%1% - ステップ %2/%3)").arg(percent).arg(curStep).arg(totalSteps));
                continue;
            }
        }

        // Match percentage "50%"
        static QRegularExpression percentRegex(R"((\d{1,3})\s*%)");
        auto pMatch = percentRegex.match(trimmed);
        if (pMatch.hasMatch()) {
            int p = pMatch.captured(1).toInt();
            if (p >= 0 && p <= 100) {
                int percent = std::clamp(20 + static_cast<int>(p * 0.75), 20, 95);
                emit generationProgress(percent, tr("組み込み推論中... (%1%)").arg(p));
            }
        }
    }
}

} // namespace Kaburagi
