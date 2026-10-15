#pragma once

#include <QObject>
#include <QImage>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QByteArray>
#include <cstdint>

class QNetworkAccessManager;
class QNetworkReply;
class QProcess;
class QTimer;
class QWebSocket;

namespace Kaburagi {

enum class ApiBackend {
    LocalWebUI = 0,     // Stable Diffusion WebUI / Forge (127.0.0.1:7860 など)
    LocalComfyUI = 1,   // ComfyUI (127.0.0.1:8188 など)
    EmbeddedEngine = 2, // 組み込み推論エンジン (sd.exe / stable-diffusion.cpp 等、サーバー不要)
    CloudOpenAI = 3,    // OpenAI DALL-E / Edits API
    CloudStability = 4  // Stability AI
};

enum class AiMode {
    RoughToClean = 0,     // ラフ・アタリからの清書補助 (img2img)
    Inpaint = 1,          // 選択範囲インペイント (img2img with mask)
    Colorization = 2,     // 自動着色・カラーラフ生成 (img2img / ControlNet)
    SeamlessTexture = 3,  // テクスチャ・シームレスパターン生成 (txt2img tiling)
    BackgroundAndNew = 4, // 背景・新規生成 (txt2img)
    SketchToLineart = 5   // ラフから線画生成 (Sketch2Lineart / 白紙ベース+ControlNet)
};

enum class ControlNetType {
    Lineart = 0,       // 線画
    Depth = 1,         // 深度マップ
    Openpose = 2,      // 人体ポーズ
    Scribble = 3,      // ラフ・落書き
    Canny = 4,         // エッジ輪郭
    ReferenceOnly = 5  // リファレンス (reference_only)
};

struct GgmlDevice {
    QString id;
    QString description;
};

struct AiGenerateParams {
    ApiBackend backend = ApiBackend::LocalComfyUI;
    QString serverUrl = "http://127.0.0.1:8188";
    QString apiKey;
    QString modelName;

    AiMode mode = AiMode::RoughToClean;
    QString prompt;
    QString negativePrompt;

    int batchCount = 1;
    int steps = 20;
    float cfgScale = 7.0f;
    float denoisingStrength = 0.55f;
    int64_t seed = -1;
    int width = 512;
    int height = 512;
    QString sampler = "Euler a";
    QString scheduler = "normal";
    bool removeBackground = false;
    bool tiling = false;

    QImage initImage;
    QImage maskImage; // 白 (255) = 再生成領域, 黒 (0) = 保持領域

    bool enableControlNet = false;
    ControlNetType controlNetType = ControlNetType::Lineart;
    float controlNetWeight = 1.0f;
    QImage controlNetImage;

    bool enableIpAdapter = false;
    QString ipAdapterType = "ip-adapter_clip_sd15";
    float ipAdapterWeight = 0.8f;
    QImage ipAdapterImage;

    bool autoLaunchServer = true;
    QString comfyUiLaunchPath;
    QString webUiLaunchPath;
    QString embeddedEnginePath;
    QString modelDirectory;

    QString computeDevice = "cuda";     // "cuda", "rocm", "openvino", "cpu"
    QString upscaleMethod = "bicubic";  // "bicubic", "bilinear", "nearest", "lanczos", または AI モデル名
};

/**
 * @brief 画像生成AI通信サービスクラス
 * ローカルAPI (ComfyUI / WebUI / Forge)、組み込み推論エンジン、および クラウドAPI (OpenAI)
 * との非同期通信・データフォーマット変換・自動起動・推論管理を担当
 */
class AiService : public QObject {
    Q_OBJECT

public:
    explicit AiService(QObject* parent = nullptr);
    ~AiService() override;

    bool isBusy() const;

    // モデル一覧の取得
    void fetchModels(ApiBackend backend, const QString& serverUrl, const QString& apiKey = QString());

    // 生成リクエストの送信
    void generate(const AiGenerateParams& params);

    // キャンセル
    void cancel();

    // 画像アップスケールユーティリティ
    static QImage upscaleImage(const QImage& source, const QSize& targetSize, const QString& method);

    // 背景透過ユーティリティ（外周・4隅からFloodFillし、境界を滑らかにフェザリング透過）
    static QImage removeBackground(const QImage& source, int tolerance = 25);

    // 線画用透明化ユーティリティ（白を透明、黒を主線に変換する「輝度を透明度に変換」処理）
    static QImage convertBrightnessToAlpha(const QImage& source, const QColor& lineColor = Qt::black);

    // 組み込み推論エンジン(sd.exe)が利用可能な計算デバイス検出
    static std::vector<GgmlDevice> detectEmbeddedDevices(const QString& embeddedExePath = QString());

    // ユーティリティ & パーサー (テストやモックにも利用可能)
    static QString imageToBase64Png(const QImage& img);
    static QImage base64ToImage(const QString& b64);
    static QJsonObject buildWebUiTxt2ImgJson(const AiGenerateParams& params);
    static QJsonObject buildWebUiImg2ImgJson(const AiGenerateParams& params);
    static QJsonObject buildComfyUiPromptJson(const AiGenerateParams& params, const QString& uploadedInitImage = QString(), const QString& uploadedControlNetImage = QString());
    static QJsonObject buildOpenAiJson(const AiGenerateParams& params);
    static QString findControlNetModelPath(ControlNetType type, bool isSdxl = false, const QString& explicitDir = QString());
    static bool isModelSdxl(const QString& modelName);
    static QList<QImage> parseWebUiResponse(const QByteArray& responseData, QString* outError = nullptr);
    static QList<QImage> parseOpenAiResponse(const QByteArray& responseData, QString* outError = nullptr);

    // パラメータと JSON の相互変換（レイヤー保持・設定復元用）
    static QJsonObject paramsToJson(const AiGenerateParams& params);
    static AiGenerateParams jsonToParams(const QJsonObject& json);

signals:
    void modelsFetched(const QStringList& models);
    void modelsFetchFailed(const QString& errorMessage);
    void generationProgress(int percent, const QString& statusMessage);
    void generationFinished(const QList<QImage>& results);
    void generationFailed(const QString& errorMessage);
    void serverAutoLaunched(const QString& serverName);

private slots:
    void onModelsReplyFinished();
    void onGenerateReplyFinished();
    void onComfyHistoryPollTimer();
    void onServerHealthCheckTimer();
    void onEmbeddedProcessFinished(int exitCode);
    void onEmbeddedProcessReadyRead();
    void onWebUiProgressTimer();
    void onComfyWebSocketTextMessageReceived(const QString& message);

private:
    void startServerLaunchAndRetry(ApiBackend backend, const QString& launchPath);
    void checkServerHealth();
    void executeGenerate(const AiGenerateParams& params);
    void executeComfyUiGenerate(const AiGenerateParams& params);
    void executeEmbeddedInference(const AiGenerateParams& params);
    void pollComfyUiHistory(const QString& promptId);
    void downloadComfyUiImages(const QStringList& imageFilenames);

    void startWebUiProgressPolling();
    void stopWebUiProgressPolling();
    void connectComfyWebSocket(const QString& serverUrl);
    void disconnectComfyWebSocket();

    QNetworkAccessManager* m_netManager = nullptr;
    QNetworkReply* m_currentReply = nullptr;
    QNetworkReply* m_modelsReply = nullptr;
    AiGenerateParams m_currentParams;

    // Web UI 進捗ポーリング管理
    QTimer* m_webUiProgressTimer = nullptr;
    QNetworkReply* m_webUiProgressReply = nullptr;

    // ComfyUI WebSocket & ポーリング管理
    QWebSocket* m_comfyWebSocket = nullptr;
    QString m_comfyClientId;
    QTimer* m_comfyPollTimer = nullptr;
    QString m_comfyPromptId;
    int m_comfyPollCount = 0;

    // サーバー自動起動・ヘルスチェック管理
    QTimer* m_serverHealthTimer = nullptr;
    int m_serverHealthAttempts = 0;
    QString m_serverHealthCheckUrl;
    bool m_waitingServerLaunch = false;

    // 組み込みエンジン (QProcess)
    QProcess* m_embeddedProcess = nullptr;
    QString m_embeddedOutputPath;
    QStringList m_embeddedGeneratedFiles;
    QStringList m_embeddedTempFiles;
    QString m_embeddedProcessLog;
};

} // namespace Kaburagi
