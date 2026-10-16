#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>

class QNetworkAccessManager;
class QNetworkReply;

namespace Kaburagi {

enum class LlmBackend {
    Ollama = 0,     // http://127.0.0.1:11434
    LmStudio = 1,   // http://127.0.0.1:1234
    LlamaCpp = 2,   // http://127.0.0.1:8080
    CloudOpenAI = 3 // https://api.openai.com/v1 等
};

struct LlmConfig {
    LlmBackend backend = LlmBackend::Ollama;
    QString serverUrl = "http://127.0.0.1:11434";
    QString apiKey;
    QString modelName = "llama3";
};

struct LlmPromptResult {
    QString prompt;
    QString negativePrompt;
};

/**
 * @brief LLM（Ollama, LM Studio, llama.cpp, クラウド）を利用して
 * 画像生成プロンプトの考案・最適化・Danbooruタグ化を行う通信サービス
 */
class LlmPromptService : public QObject {
    Q_OBJECT

public:
    explicit LlmPromptService(QObject* parent = nullptr);
    ~LlmPromptService() override;

    bool isBusy() const;

    // モデル一覧の取得
    void fetchModels(LlmBackend backend, const QString& serverUrl, const QString& apiKey = QString());

    // プロンプト生成リクエストの送信
    void generatePrompt(const LlmConfig& config,
                        const QString& userIdea,
                        const QString& stylePreset = QString(),
                        const QString& currentPrompt = QString(),
                        bool isInpaintMode = false);

    // 前回の画像・プロンプトからの修正・改善リクエストの送信 (Iterative Refinement)
    void refinePrompt(const LlmConfig& config,
                      const QString& previousPrompt,
                      const QString& previousNegative,
                      const QString& modificationRequest,
                      const QString& stylePreset = QString(),
                      bool isInpaintMode = false);

    // キャンセル
    void cancel();

    // ユーティリティ (テストやフォーマット変換用)
    static QString defaultUrlForBackend(LlmBackend backend);
    static QJsonObject buildChatRequestJson(const LlmConfig& config,
                                            const QString& userIdea,
                                            const QString& stylePreset,
                                            const QString& currentPrompt,
                                            bool isInpaintMode = false);
    static QJsonObject buildRefineRequestJson(const LlmConfig& config,
                                              const QString& previousPrompt,
                                              const QString& previousNegative,
                                              const QString& modificationRequest,
                                              const QString& stylePreset,
                                              bool isInpaintMode = false);
    static QString buildSystemPrompt(const QString& stylePreset, bool isInpaintMode = false);
    static QString buildRefineSystemPrompt(const QString& stylePreset, bool isInpaintMode = false);
    static LlmPromptResult parseChatResponseJson(const QByteArray& responseData, QString* outError = nullptr);
    static QStringList parseModelsResponse(LlmBackend backend, const QByteArray& responseData);

signals:
    void modelsFetched(const QStringList& models);
    void modelsFetchFailed(const QString& errorMessage);
    void promptGenerationProgress(int percent, const QString& statusMessage, const QString& currentContent);
    void promptGenerated(const LlmPromptResult& result);
    void promptGenerationFailed(const QString& errorMessage);

private slots:
    void onFetchModelsFinished();
    void onGenerateFinished();
    void onGenerateReadyRead();

private:
    QNetworkAccessManager* m_netManager = nullptr;
    QNetworkReply* m_modelsReply = nullptr;
    QNetworkReply* m_generateReply = nullptr;
    LlmBackend m_currentFetchBackend = LlmBackend::Ollama;
    QByteArray m_streamBuffer;
    QString m_accumulatedContent;
};

} // namespace Kaburagi
