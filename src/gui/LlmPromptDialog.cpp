#include "LlmPromptDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QLineEdit>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QSettings>
#include <QMessageBox>

namespace Kaburagi {

LlmPromptDialog::LlmPromptDialog(QWidget* parent)
    : QDialog(parent)
    , m_llmService(new LlmPromptService(this))
{
    setWindowTitle(tr("LLM プロンプト生成アシスタント"));
    resize(620, 680);
    setModal(true);

    setupUi();

    connect(m_llmService, &LlmPromptService::modelsFetched, this, &LlmPromptDialog::onModelsFetched);
    connect(m_llmService, &LlmPromptService::modelsFetchFailed, this, &LlmPromptDialog::onModelsFetchFailed);
    connect(m_llmService, &LlmPromptService::promptGenerationProgress, this, &LlmPromptDialog::onPromptGenerationProgress);
    connect(m_llmService, &LlmPromptService::promptGenerated, this, &LlmPromptDialog::onPromptGenerated);
    connect(m_llmService, &LlmPromptService::promptGenerationFailed, this, &LlmPromptDialog::onPromptGenerationFailed);

    loadSettings();
    m_initialized = true;
    onFetchModelsClicked();
}

LlmPromptDialog::~LlmPromptDialog() {
    saveSettings();
}

void LlmPromptDialog::setCurrentPrompt(const QString& prompt, const QString& negativePrompt, const QImage& lastImage) {
    m_existingPrompt = prompt;
    m_existingNegative = negativePrompt;
    m_lastImage = lastImage;

    if (m_editRefinePrevPrompt) {
        m_editRefinePrevPrompt->setPlainText(prompt);
    }
    if (m_lblRefineImagePreview) {
        if (!lastImage.isNull()) {
            m_lblRefineImagePreview->setPixmap(QPixmap::fromImage(lastImage.scaled(90, 90, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            m_lblRefineImagePreview->setText(QString());
        } else {
            m_lblRefineImagePreview->setPixmap(QPixmap());
            m_lblRefineImagePreview->setText(tr("(画像なし)"));
        }
    }

    // 前回画像やプロンプトがあれば「修正・改善」モードをデフォルト推奨
    if (!lastImage.isNull() || !prompt.trimmed().isEmpty()) {
        if (m_radioModeRefine) {
            m_radioModeRefine->setChecked(true);
            onModeChanged();
        }
    }
}

QString LlmPromptDialog::generatedPrompt() const {
    return m_editResultPrompt ? m_editResultPrompt->toPlainText().trimmed() : QString();
}

QString LlmPromptDialog::generatedNegativePrompt() const {
    return m_editResultNegative ? m_editResultNegative->toPlainText().trimmed() : QString();
}

QString LlmPromptDialog::lastUserInput() const {
    if (isRefineMode()) {
        return m_editRefineRequest ? m_editRefineRequest->toPlainText().trimmed() : QString();
    }
    return m_editIdea ? m_editIdea->toPlainText().trimmed() : QString();
}

bool LlmPromptDialog::shouldAppend() const {
    return m_comboApplyMode && m_comboApplyMode->currentIndex() == 1;
}

bool LlmPromptDialog::isRefineMode() const {
    return m_radioModeRefine && m_radioModeRefine->isChecked();
}

void LlmPromptDialog::onModeChanged() {
    bool refine = isRefineMode();
    if (m_widgetCreateMode) m_widgetCreateMode->setVisible(!refine);
    if (m_widgetRefineMode) m_widgetRefineMode->setVisible(refine);

    if (m_btnGenerate) {
        m_btnGenerate->setText(refine ? tr("🔄 LLM で改善プロンプトを生成") : tr("✨ LLM でプロンプトを考案・最適化"));
    }
}

void LlmPromptDialog::setupUi() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // 1. モード選択
    auto grpMode = new QGroupBox(tr("作業モード"), this);
    auto layMode = new QHBoxLayout(grpMode);
    m_radioModeCreate = new QRadioButton(tr("✨ 新規プロンプト考案"), grpMode);
    m_radioModeRefine = new QRadioButton(tr("🔄 前回の画像・プロンプトから修正・改善"), grpMode);
    m_radioModeCreate->setChecked(true);
    layMode->addWidget(m_radioModeCreate);
    layMode->addWidget(m_radioModeRefine);
    mainLayout->addWidget(grpMode);

    connect(m_radioModeCreate, &QRadioButton::toggled, this, &LlmPromptDialog::onModeChanged);
    connect(m_radioModeRefine, &QRadioButton::toggled, this, &LlmPromptDialog::onModeChanged);

    // 2. LLM バックエンド接続設定
    auto grpBackend = new QGroupBox(tr("LLM 接続設定"), this);
    auto layBackend = new QVBoxLayout(grpBackend);

    auto gridRadio = new QGridLayout();
    m_radioOllama = new QRadioButton(tr("Ollama (ローカル)"), grpBackend);
    m_radioLmStudio = new QRadioButton(tr("LM Studio (ローカル)"), grpBackend);
    m_radioLlamaCpp = new QRadioButton(tr("llama.cpp (ローカル)"), grpBackend);
    m_radioCloud = new QRadioButton(tr("クラウド / OpenAI互換"), grpBackend);
    m_radioOllama->setChecked(true);

    gridRadio->addWidget(m_radioOllama, 0, 0);
    gridRadio->addWidget(m_radioLmStudio, 0, 1);
    gridRadio->addWidget(m_radioLlamaCpp, 1, 0);
    gridRadio->addWidget(m_radioCloud, 1, 1);
    layBackend->addLayout(gridRadio);

    connect(m_radioOllama, &QRadioButton::toggled, this, &LlmPromptDialog::onBackendChanged);
    connect(m_radioLmStudio, &QRadioButton::toggled, this, &LlmPromptDialog::onBackendChanged);
    connect(m_radioLlamaCpp, &QRadioButton::toggled, this, &LlmPromptDialog::onBackendChanged);
    connect(m_radioCloud, &QRadioButton::toggled, this, &LlmPromptDialog::onBackendChanged);

    auto formConn = new QFormLayout();
    m_editServerUrl = new QLineEdit("http://127.0.0.1:11434", grpBackend);
    m_editApiKey = new QLineEdit(grpBackend);
    m_editApiKey->setEchoMode(QLineEdit::Password);
    m_editApiKey->setPlaceholderText(tr("sk-... (クラウド利用時のみ)"));

    auto layModel = new QHBoxLayout();
    m_comboModel = new QComboBox(grpBackend);
    m_comboModel->setEditable(true);
    m_comboModel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_btnFetchModels = new QPushButton(tr("モデル取得"), grpBackend);
    m_btnFetchModels->setMaximumWidth(80);
    connect(m_btnFetchModels, &QPushButton::clicked, this, &LlmPromptDialog::onFetchModelsClicked);
    layModel->addWidget(m_comboModel);
    layModel->addWidget(m_btnFetchModels);

    formConn->addRow(tr("サーバーURL:"), m_editServerUrl);
    formConn->addRow(tr("APIキー:"), m_editApiKey);
    formConn->addRow(tr("使用モデル:"), layModel);
    layBackend->addLayout(formConn);
    mainLayout->addWidget(grpBackend);

    // 3-1. Mode 1: 新規プロンプト考案
    m_widgetCreateMode = new QWidget(this);
    auto layCreate = new QVBoxLayout(m_widgetCreateMode);
    layCreate->setContentsMargins(0, 0, 0, 0);
    auto grpIdea = new QGroupBox(tr("描きたい内容・アイデア (日本語可)"), m_widgetCreateMode);
    auto layIdea = new QVBoxLayout(grpIdea);
    m_editIdea = new QPlainTextEdit(grpIdea);
    m_editIdea->setPlaceholderText(tr("例: 夕暮れの教室、窓辺に立つ制服の女子高生、少し物憂げな表情、エモーショナルな逆光、高品質なアニメ塗り"));
    m_editIdea->setMaximumHeight(70);
    layIdea->addWidget(m_editIdea);
    layCreate->addWidget(grpIdea);
    mainLayout->addWidget(m_widgetCreateMode);

    // 3-2. Mode 2: 前回の画像・プロンプトから修正・改善
    m_widgetRefineMode = new QWidget(this);
    auto layRefine = new QVBoxLayout(m_widgetRefineMode);
    layRefine->setContentsMargins(0, 0, 0, 0);
    auto grpRefine = new QGroupBox(tr("前回の画像・プロンプトからの改善・修正指示"), m_widgetRefineMode);
    auto layRefineBox = new QVBoxLayout(grpRefine);

    auto layPrevInfo = new QHBoxLayout();
    m_lblRefineImagePreview = new QLabel(tr("(画像なし)"), grpRefine);
    m_lblRefineImagePreview->setFixedSize(94, 94);
    m_lblRefineImagePreview->setAlignment(Qt::AlignCenter);
    m_lblRefineImagePreview->setStyleSheet("QLabel { border: 1px solid #b0bec5; border-radius: 4px; background-color: #f5f5f5; color: #78909c; }");
    layPrevInfo->addWidget(m_lblRefineImagePreview);

    auto layPrevPromptCol = new QVBoxLayout();
    layPrevPromptCol->addWidget(new QLabel(tr("前回のプロンプト:"), grpRefine));
    m_editRefinePrevPrompt = new QPlainTextEdit(grpRefine);
    m_editRefinePrevPrompt->setReadOnly(true);
    m_editRefinePrevPrompt->setMaximumHeight(70);
    m_editRefinePrevPrompt->setPlaceholderText(tr("(前回のプロンプトなし)"));
    layPrevPromptCol->addWidget(m_editRefinePrevPrompt);
    layPrevInfo->addLayout(layPrevPromptCol);
    layRefineBox->addLayout(layPrevInfo);

    layRefineBox->addWidget(new QLabel(tr("修正・改善したい点、追加・変更したい要素 (日本語可):"), grpRefine));
    m_editRefineRequest = new QPlainTextEdit(grpRefine);
    m_editRefineRequest->setPlaceholderText(tr("例: もっと笑顔にして、髪を銀髪ロングに変えて、背景をサイバーパンクの夜景にしたい"));
    m_editRefineRequest->setMaximumHeight(65);
    layRefineBox->addWidget(m_editRefineRequest);

    layRefine->addWidget(grpRefine);
    mainLayout->addWidget(m_widgetRefineMode);
    m_widgetRefineMode->setVisible(false);

    // 画風・適用オプション
    auto formOptions = new QFormLayout();
    m_comboStyle = new QComboBox(this);
    m_comboStyle->addItem(tr("指定なし（自由）"));
    m_comboStyle->addItem(tr("アニメ・美麗イラスト調 (Anime / Masterpiece)"));
    m_comboStyle->addItem(tr("美麗デジタルアート (Digital Painting)"));
    m_comboStyle->addItem(tr("フォトリアル・実写調 (Photorealistic)"));
    m_comboStyle->addItem(tr("ファンタジー・水彩調 (Watercolor / Fantasy)"));
    m_comboStyle->addItem(tr("サイバーパンク・SF調 (Cyberpunk / Sci-Fi)"));
    m_comboStyle->addItem(tr("レトロコミック・アメコミ調 (Retro Comic)"));
    m_comboStyle->addItem(tr("ドット絵・ピクセルアート (Pixel Art)"));

    m_comboApplyMode = new QComboBox(this);
    m_comboApplyMode->addItem(tr("現在のプロンプトを上書き"));
    m_comboApplyMode->addItem(tr("現在のプロンプトの末尾に追加"));

    formOptions->addRow(tr("画風・スタイル:"), m_comboStyle);
    formOptions->addRow(tr("適用方法:"), m_comboApplyMode);
    mainLayout->addLayout(formOptions);

    // 4. アクション & ステータス
    auto layAction = new QVBoxLayout();
    m_btnGenerate = new QPushButton(tr("✨ LLM でプロンプトを考案・最適化"), this);
    m_btnGenerate->setStyleSheet("QPushButton { font-weight: bold; background-color: #6a1b9a; color: white; padding: 8px; font-size: 13px; } QPushButton:hover { background-color: #7b1fa2; }");
    connect(m_btnGenerate, &QPushButton::clicked, this, &LlmPromptDialog::onGenerateClicked);
    layAction->addWidget(m_btnGenerate);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setVisible(false);
    m_lblStatus = new QLabel(tr("準備完了"), this);
    m_lblStatus->setAlignment(Qt::AlignCenter);

    layAction->addWidget(m_progressBar);
    layAction->addWidget(m_lblStatus);
    mainLayout->addLayout(layAction);

    // 5. 生成結果プレビュー
    auto grpResult = new QGroupBox(tr("生成結果 (英語タグ)"), this);
    auto layResult = new QVBoxLayout(grpResult);

    layResult->addWidget(new QLabel(tr("プロンプト (Positive):"), grpResult));
    m_editResultPrompt = new QPlainTextEdit(grpResult);
    m_editResultPrompt->setMaximumHeight(75);
    layResult->addWidget(m_editResultPrompt);

    layResult->addWidget(new QLabel(tr("ネガティブプロンプト (Negative):"), grpResult));
    m_editResultNegative = new QPlainTextEdit(grpResult);
    m_editResultNegative->setMaximumHeight(45);
    layResult->addWidget(m_editResultNegative);

    mainLayout->addWidget(grpResult);

    // 6. 下部ダイアログボタン
    auto layBottom = new QHBoxLayout();
    auto btnCancel = new QPushButton(tr("閉じる"), this);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    m_btnApply = new QPushButton(tr("✔ プロンプト欄に適用"), this);
    m_btnApply->setStyleSheet("QPushButton { font-weight: bold; background-color: #2e7d32; color: white; padding: 6px 16px; } QPushButton:hover { background-color: #388e3c; }");
    m_btnApply->setEnabled(false);
    connect(m_btnApply, &QPushButton::clicked, this, &LlmPromptDialog::onApplyClicked);

    layBottom->addStretch();
    layBottom->addWidget(btnCancel);
    layBottom->addWidget(m_btnApply);
    mainLayout->addLayout(layBottom);

    onBackendChanged();
}

void LlmPromptDialog::onBackendChanged() {
    if (m_radioOllama->isChecked()) {
        m_editServerUrl->setText(LlmPromptService::defaultUrlForBackend(LlmBackend::Ollama));
        m_editApiKey->setEnabled(false);
    } else if (m_radioLmStudio->isChecked()) {
        m_editServerUrl->setText(LlmPromptService::defaultUrlForBackend(LlmBackend::LmStudio));
        m_editApiKey->setEnabled(false);
    } else if (m_radioLlamaCpp->isChecked()) {
        m_editServerUrl->setText(LlmPromptService::defaultUrlForBackend(LlmBackend::LlamaCpp));
        m_editApiKey->setEnabled(false);
    } else if (m_radioCloud->isChecked()) {
        m_editServerUrl->setText(LlmPromptService::defaultUrlForBackend(LlmBackend::CloudOpenAI));
        m_editApiKey->setEnabled(true);
    }

    if (m_initialized) {
        onFetchModelsClicked();
    }
}

LlmConfig LlmPromptDialog::currentConfig() const {
    LlmConfig cfg;
    if (m_radioOllama->isChecked()) cfg.backend = LlmBackend::Ollama;
    else if (m_radioLmStudio->isChecked()) cfg.backend = LlmBackend::LmStudio;
    else if (m_radioLlamaCpp->isChecked()) cfg.backend = LlmBackend::LlamaCpp;
    else cfg.backend = LlmBackend::CloudOpenAI;

    cfg.serverUrl = m_editServerUrl->text().trimmed();
    cfg.apiKey = m_editApiKey->text().trimmed();
    cfg.modelName = m_comboModel->currentText().trimmed();
    return cfg;
}

void LlmPromptDialog::onFetchModelsClicked() {
    LlmConfig cfg = currentConfig();
    if (cfg.backend == LlmBackend::CloudOpenAI && cfg.apiKey.isEmpty()) {
        m_lblStatus->setText(tr("※ APIキーを入力後、「モデル取得」をクリックしてください。"));
        return;
    }
    m_lblStatus->setText(tr("モデル一覧を取得中..."));
    m_btnFetchModels->setEnabled(false);
    m_llmService->fetchModels(cfg.backend, cfg.serverUrl, cfg.apiKey);
}

void LlmPromptDialog::onModelsFetched(const QStringList& models) {
    m_btnFetchModels->setEnabled(true);
    QString current = m_comboModel->currentText();
    m_comboModel->clear();
    for (const auto& m : models) {
        m_comboModel->addItem(m);
    }
    if (!current.isEmpty()) {
        int idx = m_comboModel->findText(current);
        if (idx >= 0) {
            m_comboModel->setCurrentIndex(idx);
        } else {
            m_comboModel->setCurrentText(current);
        }
    } else if (m_comboModel->count() > 0) {
        m_comboModel->setCurrentIndex(0);
    }

    m_lblStatus->setText(tr("モデル一覧を更新しました (%1 件)").arg(models.size()));
}

void LlmPromptDialog::onModelsFetchFailed(const QString& errorMessage) {
    m_btnFetchModels->setEnabled(true);
    LlmConfig cfg = currentConfig();
    QString backendName = (cfg.backend == LlmBackend::Ollama) ? "Ollama" :
                          (cfg.backend == LlmBackend::LmStudio) ? "LM Studio" :
                          (cfg.backend == LlmBackend::LlamaCpp) ? "llama.cpp" : "クラウド";
    m_lblStatus->setText(tr("モデル取得失敗 (%1: %2)。サーバーが起動しているか確認してください。").arg(backendName, errorMessage));
}

void LlmPromptDialog::onGenerateClicked() {
    bool refine = isRefineMode();
    QString idea = m_editIdea ? m_editIdea->toPlainText().trimmed() : QString();
    QString req = m_editRefineRequest ? m_editRefineRequest->toPlainText().trimmed() : QString();

    if (refine) {
        if (req.isEmpty()) {
            QMessageBox::information(this, tr("入力確認"), tr("修正・改善したい要望や変更点を入力してください。"));
            return;
        }
    } else {
        if (idea.isEmpty()) {
            QMessageBox::information(this, tr("入力確認"), tr("描きたい内容やアイデアを入力してください。"));
            return;
        }
    }

    LlmConfig cfg = currentConfig();
    if (cfg.modelName.isEmpty()) {
        // デフォルトモデルの案内
        if (cfg.backend == LlmBackend::CloudOpenAI) cfg.modelName = "gpt-4o-mini";
        else cfg.modelName = "llama3";
        m_comboModel->setCurrentText(cfg.modelName);
    }

    m_btnGenerate->setEnabled(false);
    m_progressBar->setVisible(true);
    m_progressBar->setValue(5);
    m_lblStatus->setText(refine ? tr("LLM が前回のプロンプトを改善中...") : tr("LLM がプロンプトを考案中..."));
    m_editResultPrompt->clear();
    m_editResultNegative->clear();

    QString style = m_comboStyle->currentText();
    if (refine) {
        m_llmService->refinePrompt(cfg, m_existingPrompt, m_existingNegative, req, style);
    } else {
        m_llmService->generatePrompt(cfg, idea, style, m_existingPrompt);
    }
}

void LlmPromptDialog::onPromptGenerationProgress(int percent, const QString& statusMessage, const QString& currentContent) {
    if (!m_progressBar->isVisible()) {
        m_progressBar->setVisible(true);
    }
    m_progressBar->setValue(percent);
    m_lblStatus->setText(statusMessage);

    // ストリーミング中のテキストをプレビュー欄に表示
    if (!currentContent.isEmpty()) {
        m_editResultPrompt->setPlainText(currentContent);
    }
}

void LlmPromptDialog::onPromptGenerated(const LlmPromptResult& result) {
    m_btnGenerate->setEnabled(true);
    m_progressBar->setValue(100);
    m_progressBar->setVisible(false);
    m_lblStatus->setText(tr("プロンプト生成が完了しました！"));

    m_editResultPrompt->setPlainText(result.prompt);
    m_editResultNegative->setPlainText(result.negativePrompt);
    m_btnApply->setEnabled(!result.prompt.isEmpty());
}

void LlmPromptDialog::onPromptGenerationFailed(const QString& errorMessage) {
    m_btnGenerate->setEnabled(true);
    m_progressBar->setVisible(false);
    m_lblStatus->setText(tr("生成失敗: %1").arg(errorMessage));
    QMessageBox::warning(this, tr("プロンプト生成エラー"), tr("LLM との通信でエラーが発生しました:\n%1").arg(errorMessage));
}

void LlmPromptDialog::onApplyClicked() {
    saveSettings();
    accept();
}

void LlmPromptDialog::loadSettings() {
    QSettings settings("KaburagiPaint", "PaintSoftKABURAGI");
    settings.beginGroup("LlmPrompt");
    int backend = settings.value("backend", 0).toInt();
    if (backend == 0) m_radioOllama->setChecked(true);
    else if (backend == 1) m_radioLmStudio->setChecked(true);
    else if (backend == 2) m_radioLlamaCpp->setChecked(true);
    else m_radioCloud->setChecked(true);

    QString url = settings.value("serverUrl", "").toString();
    if (!url.isEmpty()) m_editServerUrl->setText(url);

    m_editApiKey->setText(settings.value("apiKey", "").toString());
    QString model = settings.value("modelName", "").toString();
    if (!model.isEmpty()) m_comboModel->setCurrentText(model);

    int styleIdx = settings.value("styleIndex", 1).toInt();
    if (styleIdx >= 0 && styleIdx < m_comboStyle->count()) {
        m_comboStyle->setCurrentIndex(styleIdx);
    }
    settings.endGroup();
}

void LlmPromptDialog::saveSettings() {
    QSettings settings("KaburagiPaint", "PaintSoftKABURAGI");
    settings.beginGroup("LlmPrompt");
    int backend = 0;
    if (m_radioLmStudio->isChecked()) backend = 1;
    else if (m_radioLlamaCpp->isChecked()) backend = 2;
    else if (m_radioCloud->isChecked()) backend = 3;

    settings.setValue("backend", backend);
    settings.setValue("serverUrl", m_editServerUrl->text().trimmed());
    settings.setValue("apiKey", m_editApiKey->text().trimmed());
    settings.setValue("modelName", m_comboModel->currentText().trimmed());
    settings.setValue("styleIndex", m_comboStyle->currentIndex());
    settings.endGroup();
}

} // namespace Kaburagi
