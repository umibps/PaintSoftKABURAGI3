#include "LlmPromptService.hpp"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUrl>
#include <QDebug>

namespace Kaburagi {

LlmPromptService::LlmPromptService(QObject* parent)
    : QObject(parent)
    , m_netManager(new QNetworkAccessManager(this))
{
}

LlmPromptService::~LlmPromptService() {
    cancel();
}

bool LlmPromptService::isBusy() const {
    if (m_modelsReply && m_modelsReply->isRunning()) return true;
    if (m_generateReply && m_generateReply->isRunning()) return true;
    return false;
}

void LlmPromptService::cancel() {
    if (m_modelsReply) {
        m_modelsReply->abort();
        m_modelsReply->deleteLater();
        m_modelsReply = nullptr;
    }
    if (m_generateReply) {
        m_generateReply->abort();
        m_generateReply->deleteLater();
        m_generateReply = nullptr;
    }
}

QString LlmPromptService::defaultUrlForBackend(LlmBackend backend) {
    switch (backend) {
    case LlmBackend::Ollama:
        return "http://127.0.0.1:11434";
    case LlmBackend::LmStudio:
        return "http://127.0.0.1:1234";
    case LlmBackend::LlamaCpp:
        return "http://127.0.0.1:8080";
    case LlmBackend::CloudOpenAI:
        return "https://api.openai.com/v1";
    }
    return "http://127.0.0.1:11434";
}

void LlmPromptService::fetchModels(LlmBackend backend, const QString& serverUrl, const QString& apiKey) {
    if (m_modelsReply && m_modelsReply->isRunning()) {
        m_modelsReply->abort();
    }

    m_currentFetchBackend = backend;
    QString base = serverUrl.trimmed();
    if (base.endsWith("/")) base.chop(1);

    QString endpointUrl;
    if (backend == LlmBackend::Ollama) {
        endpointUrl = base + "/api/tags";
    } else {
        if (!base.endsWith("/v1")) {
            endpointUrl = base + "/v1/models";
        } else {
            endpointUrl = base + "/models";
        }
    }

    QNetworkRequest request{QUrl(endpointUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader, "PaintSoftKABURAGI/3.0.1");
    if (!apiKey.isEmpty()) {
        request.setRawHeader("Authorization", "Bearer " + apiKey.toUtf8());
    }

    m_modelsReply = m_netManager->get(request);
    connect(m_modelsReply, &QNetworkReply::finished, this, &LlmPromptService::onFetchModelsFinished);
}

void LlmPromptService::onFetchModelsFinished() {
    if (!m_modelsReply) return;

    if (m_modelsReply->error() != QNetworkReply::NoError) {
        QString err = m_modelsReply->errorString();
        m_modelsReply->deleteLater();
        m_modelsReply = nullptr;
        emit modelsFetchFailed(err);
        return;
    }

    QByteArray data = m_modelsReply->readAll();
    m_modelsReply->deleteLater();
    m_modelsReply = nullptr;

    QStringList models = parseModelsResponse(m_currentFetchBackend, data);
    emit modelsFetched(models);
}

QStringList LlmPromptService::parseModelsResponse(LlmBackend backend, const QByteArray& responseData) {
    QStringList result;
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (!doc.isObject()) return result;

    QJsonObject root = doc.object();

    if (backend == LlmBackend::Ollama && root.contains("models")) {
        QJsonArray arr = root["models"].toArray();
        for (const auto& item : arr) {
            QString name = item.toObject()["name"].toString();
            if (!name.isEmpty()) {
                result.append(name);
            }
        }
    } else if (root.contains("data")) { // OpenAI compatible (LM Studio, llama.cpp, Cloud)
        QJsonArray arr = root["data"].toArray();
        for (const auto& item : arr) {
            QString id = item.toObject()["id"].toString();
            if (!id.isEmpty()) {
                result.append(id);
            }
        }
    }

    result.sort(Qt::CaseInsensitive);
    return result;
}

QString LlmPromptService::buildSystemPrompt(const QString& stylePreset) {
    QString prompt =
        "You are an expert prompt engineer specializing in Text-to-Image models (Stable Diffusion, SDXL, Illustrious, Animagine, Midjourney).\n"
        "Your task is to transform the user's idea or description (which may be in Japanese or English) into an exceptionally detailed, high-quality image generation prompt in English.\n\n"
        "Guidelines:\n"
        "1. Write a comma-separated list of Danbooru tags, descriptive English keywords, subject details, clothing, pose, expression, environment, lighting (e.g. volumetric lighting, rim light), composition, and aesthetic quality tags.\n"
        "2. Provide a suitable negative prompt (unwanted artifacts, worst quality, lowres, bad anatomy, deformed hands, extra limbs, etc.).\n";

    if (!stylePreset.isEmpty() && stylePreset != "指定なし（自由）") {
        prompt += QString("3. Target Style Preset: %1. Make sure to tailor tags and aesthetics specifically to this style.\n").arg(stylePreset);
    }

    prompt +=
        "\nOutput format: You MUST return a valid JSON object ONLY with exactly these keys:\n"
        "{\n"
        "  \"prompt\": \"masterpiece, best quality, ... (comma-separated tags)\",\n"
        "  \"negative_prompt\": \"worst quality, low quality, bad anatomy, ...\"\n"
        "}\n"
        "Do NOT include any markdown code blocks (e.g. ```json). Output raw JSON text only.";

    return prompt;
}

QString LlmPromptService::buildRefineSystemPrompt(const QString& stylePreset) {
    QString prompt =
        "You are an expert prompt engineer specializing in iterative refinement of Text-to-Image prompts (Stable Diffusion, SDXL, Illustrious, Animagine, Midjourney).\n"
        "Your role is to help the user improve an existing image prompt based on their feedback about the previously generated image, bringing the next generation closer to their ideal vision.\n\n"
        "Refinement Guidelines:\n"
        "1. Retain the core strengths, subjects, and artistic style of the previous prompt, unless the user specifically requested changes to them.\n"
        "2. Add, modify, or replace specific Danbooru tags, keywords, expressions, lighting, composition, colors, or poses according to the user's modification request.\n"
        "3. If the user mentions unwanted artifacts or features, add them to the negative prompt.\n"
        "4. Always output detailed, high-quality descriptive tags in English.\n";

    if (!stylePreset.isEmpty() && stylePreset != "指定なし（自由）") {
        prompt += QString("5. Target Style Preset: %1.\n").arg(stylePreset);
    }

    prompt +=
        "\nOutput format: You MUST return a valid JSON object ONLY with exactly these keys:\n"
        "{\n"
        "  \"prompt\": \"refined prompt with tags...\",\n"
        "  \"negative_prompt\": \"refined negative prompt...\"\n"
        "}\n"
        "Do NOT include any markdown code blocks (e.g. ```json). Output raw JSON text only.";

    return prompt;
}

QJsonObject LlmPromptService::buildChatRequestJson(const LlmConfig& config,
                                                   const QString& userIdea,
                                                   const QString& stylePreset,
                                                   const QString& currentPrompt) {
    QJsonObject json;
    json["model"] = config.modelName.isEmpty() ? "llama3" : config.modelName;

    QJsonArray messages;

    // System prompt
    QJsonObject sysMsg;
    sysMsg["role"] = "system";
    sysMsg["content"] = buildSystemPrompt(stylePreset);
    messages.append(sysMsg);

    // User prompt
    QString userContent = QString("User Idea / Request: %1").arg(userIdea);
    if (!currentPrompt.isEmpty()) {
        userContent += QString("\nExisting / Draft Prompt: %1\n(Please enhance or expand upon this draft if helpful)").arg(currentPrompt);
    }

    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userContent;
    messages.append(userMsg);

    json["messages"] = messages;
    json["temperature"] = 0.7;

    return json;
}

QJsonObject LlmPromptService::buildRefineRequestJson(const LlmConfig& config,
                                                    const QString& previousPrompt,
                                                    const QString& previousNegative,
                                                    const QString& modificationRequest,
                                                    const QString& stylePreset) {
    QJsonObject json;
    json["model"] = config.modelName.isEmpty() ? "llama3" : config.modelName;

    QJsonArray messages;

    // System prompt
    QJsonObject sysMsg;
    sysMsg["role"] = "system";
    sysMsg["content"] = buildRefineSystemPrompt(stylePreset);
    messages.append(sysMsg);

    // User prompt
    QString userContent = QString("User's Modification / Improvement Request:\n%1\n\nPrevious Prompt:\n%2")
                              .arg(modificationRequest, previousPrompt);
    if (!previousNegative.isEmpty()) {
        userContent += QString("\n\nPrevious Negative Prompt:\n%1").arg(previousNegative);
    }
    userContent += "\nPlease produce a refined, updated prompt that incorporates the requested changes while maintaining the desired parts.";

    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userContent;
    messages.append(userMsg);

    json["messages"] = messages;
    json["temperature"] = 0.7;

    return json;
}

void LlmPromptService::generatePrompt(const LlmConfig& config,
                                      const QString& userIdea,
                                      const QString& stylePreset,
                                      const QString& currentPrompt) {
    if (m_generateReply && m_generateReply->isRunning()) {
        m_generateReply->abort();
    }

    m_streamBuffer.clear();
    m_accumulatedContent.clear();

    QString base = config.serverUrl.trimmed();
    if (base.endsWith("/")) base.chop(1);

    QString chatEndpoint;
    if (base.endsWith("/v1")) {
        chatEndpoint = base + "/chat/completions";
    } else if (base.endsWith("/v1/chat/completions")) {
        chatEndpoint = base;
    } else {
        chatEndpoint = base + "/v1/chat/completions";
    }

    QJsonObject reqJson = buildChatRequestJson(config, userIdea, stylePreset, currentPrompt);
    reqJson["stream"] = true; // ストリーミング受信を有効化
    QByteArray body = QJsonDocument(reqJson).toJson(QJsonDocument::Compact);

    QNetworkRequest request{QUrl(chatEndpoint)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::UserAgentHeader, "PaintSoftKABURAGI/3.0.1 (LlmPromptService)");
    if (!config.apiKey.isEmpty()) {
        request.setRawHeader("Authorization", "Bearer " + config.apiKey.toUtf8());
    }

    emit promptGenerationProgress(10, tr("LLMサーバーに接続・プロンプト送信中..."), QString());

    m_generateReply = m_netManager->post(request, body);
    connect(m_generateReply, &QNetworkReply::readyRead, this, &LlmPromptService::onGenerateReadyRead);
    connect(m_generateReply, &QNetworkReply::finished, this, &LlmPromptService::onGenerateFinished);
}

void LlmPromptService::refinePrompt(const LlmConfig& config,
                                    const QString& previousPrompt,
                                    const QString& previousNegative,
                                    const QString& modificationRequest,
                                    const QString& stylePreset) {
    if (m_generateReply && m_generateReply->isRunning()) {
        m_generateReply->abort();
    }

    m_streamBuffer.clear();
    m_accumulatedContent.clear();

    QString base = config.serverUrl.trimmed();
    if (base.endsWith("/")) base.chop(1);

    QString chatEndpoint;
    if (base.endsWith("/v1")) {
        chatEndpoint = base + "/chat/completions";
    } else if (base.endsWith("/v1/chat/completions")) {
        chatEndpoint = base;
    } else {
        chatEndpoint = base + "/v1/chat/completions";
    }

    QJsonObject reqJson = buildRefineRequestJson(config, previousPrompt, previousNegative, modificationRequest, stylePreset);
    reqJson["stream"] = true; // ストリーミング受信を有効化
    QByteArray body = QJsonDocument(reqJson).toJson(QJsonDocument::Compact);

    QNetworkRequest request{QUrl(chatEndpoint)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::UserAgentHeader, "PaintSoftKABURAGI/3.0.1 (LlmPromptService)");
    if (!config.apiKey.isEmpty()) {
        request.setRawHeader("Authorization", "Bearer " + config.apiKey.toUtf8());
    }

    emit promptGenerationProgress(10, tr("LLMサーバーにプロンプト改善リクエスト送信中..."), QString());

    m_generateReply = m_netManager->post(request, body);
    connect(m_generateReply, &QNetworkReply::readyRead, this, &LlmPromptService::onGenerateReadyRead);
    connect(m_generateReply, &QNetworkReply::finished, this, &LlmPromptService::onGenerateFinished);
}

void LlmPromptService::onGenerateReadyRead() {
    if (!m_generateReply) return;

    QByteArray chunk = m_generateReply->readAll();
    m_streamBuffer.append(chunk);

    // SSE (Server-Sent Events) の各行を処理
    while (true) {
        int newlineIdx = m_streamBuffer.indexOf('\n');
        if (newlineIdx < 0) break;

        QByteArray lineBytes = m_streamBuffer.left(newlineIdx).trimmed();
        m_streamBuffer.remove(0, newlineIdx + 1);

        if (lineBytes.startsWith("data:")) {
            QByteArray dataContent = lineBytes.mid(5).trimmed();
            if (dataContent == "[DONE]") {
                continue;
            }

            QJsonDocument doc = QJsonDocument::fromJson(dataContent);
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                QJsonArray choices = obj["choices"].toArray();
                if (!choices.isEmpty()) {
                    QJsonObject choice0 = choices[0].toObject();
                    if (choice0.contains("delta")) {
                        QJsonObject delta = choice0["delta"].toObject();
                        if (delta.contains("content")) {
                            QString dStr = delta["content"].toString();
                            m_accumulatedContent.append(dStr);
                        }
                    }
                }
            }
        }
    }

    if (!m_accumulatedContent.isEmpty()) {
        // 文字数に応じたプログレスバー計算 (20%〜90%)
        int progressPercent = std::clamp(20 + static_cast<int>(m_accumulatedContent.length() / 4), 20, 92);
        emit promptGenerationProgress(progressPercent,
                                      tr("LLM が考案・生成中... (%1文字)").arg(m_accumulatedContent.length()),
                                      m_accumulatedContent);
    }
}

void LlmPromptService::onGenerateFinished() {
    if (!m_generateReply) return;

    if (m_generateReply->error() != QNetworkReply::NoError) {
        QString err = m_generateReply->errorString();
        m_generateReply->deleteLater();
        m_generateReply = nullptr;
        emit promptGenerationFailed(err);
        return;
    }

    m_generateReply->deleteLater();
    m_generateReply = nullptr;

    QString err;
    LlmPromptResult res;

    // ストリーミングで蓄積されたテキストがあればそれをパース
    if (!m_accumulatedContent.isEmpty()) {
        QString cleanContent = m_accumulatedContent.trimmed();
        // markdown code block の除去
        if (cleanContent.startsWith("```json")) cleanContent = cleanContent.mid(7);
        else if (cleanContent.startsWith("```")) cleanContent = cleanContent.mid(3);
        if (cleanContent.endsWith("```")) cleanContent.chop(3);
        cleanContent = cleanContent.trimmed();

        // JSONブロックの抽出
        int firstBrace = cleanContent.indexOf('{');
        int lastBrace = cleanContent.lastIndexOf('}');
        if (firstBrace >= 0 && lastBrace > firstBrace) {
            cleanContent = cleanContent.mid(firstBrace, lastBrace - firstBrace + 1);
        }

        QJsonDocument doc = QJsonDocument::fromJson(cleanContent.toUtf8());
        if (doc.isObject()) {
            QJsonObject root = doc.object();
            res.prompt = root.contains("prompt") ? root["prompt"].toString().trimmed() : QString();
            res.negativePrompt = root.contains("negative_prompt") ? root["negative_prompt"].toString().trimmed() : QString();
            if (res.prompt.isEmpty() && root.contains("positive")) {
                res.prompt = root["positive"].toString().trimmed();
            }
        } else {
            // JSON 以外のフリーテキストで返ってきた場合のフォールバック
            res.prompt = m_accumulatedContent.trimmed();
        }
    } else {
        // ストリーミング非対応でバッファに一括返却された場合
        res = parseChatResponseJson(m_streamBuffer, &err);
    }

    if (!err.isEmpty()) {
        emit promptGenerationFailed(err);
    } else {
        emit promptGenerationProgress(100, tr("プロンプト生成が完了しました！"), res.prompt);
        emit promptGenerated(res);
    }
}

LlmPromptResult LlmPromptService::parseChatResponseJson(const QByteArray& responseData, QString* outError) {
    LlmPromptResult result;
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (!doc.isObject()) {
        if (outError) *outError = "LLMからの応答が正しいJSON形式ではありません。";
        return result;
    }

    QJsonObject root = doc.object();
    if (root.contains("error")) {
        QJsonObject errObj = root["error"].toObject();
        QString msg = errObj.contains("message") ? errObj["message"].toString() : "APIエラーが発生しました";
        if (outError) *outError = msg;
        return result;
    }

    QString content;
    if (root.contains("choices")) {
        QJsonArray choices = root["choices"].toArray();
        if (!choices.isEmpty()) {
            QJsonObject firstChoice = choices[0].toObject();
            if (firstChoice.contains("message")) {
                content = firstChoice["message"].toObject()["content"].toString();
            }
        }
    } else if (root.contains("message")) { // Ollama native fallback
        content = root["message"].toObject()["content"].toString();
    } else if (root.contains("response")) {
        content = root["response"].toString();
    }

    if (content.isEmpty()) {
        if (outError) *outError = "LLMからの生成テキストが空でした。";
        return result;
    }

    // マークダウンコードブロック (```json ... ```) のトリミング
    QString cleanContent = content.trimmed();
    if (cleanContent.startsWith("```json", Qt::CaseInsensitive)) {
        cleanContent = cleanContent.mid(7);
    } else if (cleanContent.startsWith("```")) {
        cleanContent = cleanContent.mid(3);
    }
    if (cleanContent.endsWith("```")) {
        cleanContent.chop(3);
    }
    cleanContent = cleanContent.trimmed();

    // JSONパースを試行
    QJsonDocument contentDoc = QJsonDocument::fromJson(cleanContent.toUtf8());
    if (contentDoc.isObject()) {
        QJsonObject contentObj = contentDoc.object();
        result.prompt = contentObj["prompt"].toString().trimmed();
        result.negativePrompt = contentObj["negative_prompt"].toString().trimmed();
        return result;
    }

    // JSONブロックを中括弧から正規表現で探索
    int openBrace = cleanContent.indexOf('{');
    int closeBrace = cleanContent.lastIndexOf('}');
    if (openBrace >= 0 && closeBrace > openBrace) {
        QString jsonSub = cleanContent.mid(openBrace, closeBrace - openBrace + 1);
        QJsonDocument subDoc = QJsonDocument::fromJson(jsonSub.toUtf8());
        if (subDoc.isObject()) {
            QJsonObject contentObj = subDoc.object();
            result.prompt = contentObj["prompt"].toString().trimmed();
            result.negativePrompt = contentObj["negative_prompt"].toString().trimmed();
            return result;
        }
    }

    // フォールバック: テキストから抽出
    result.prompt = cleanContent;
    result.negativePrompt = "worst quality, low quality, bad anatomy, deformed";
    return result;
}

} // namespace Kaburagi
