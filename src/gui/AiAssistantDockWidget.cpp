#include "AiAssistantDockWidget.hpp"
#include "AiService.hpp"
#include "Document.hpp"
#include "CanvasWidget.hpp"
#include "Layer.hpp"
#include "PreferencesDialog.hpp"
#include "AiComponentDownloader.hpp"
#include "LlmPromptDialog.hpp"
#include "MainWindow.hpp"
#include "ReferenceImageWindow.hpp"
#include <QClipboard>
#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QLineEdit>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QListWidget>
#include <QScrollArea>
#include <QPainter>
#include <QMessageBox>
#include <QFileDialog>
#include <QRandomGenerator>
#include <QSettings>

namespace Kaburagi {

AiAssistantDockWidget::AiAssistantDockWidget(QWidget* parent)
    : QDockWidget(tr("AI アシスタント"), parent)
    , m_aiService(new AiService(this))
{
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setupUi();

    // AiService signals
    connect(m_aiService, &AiService::modelsFetched, this, &AiAssistantDockWidget::onModelsFetched);
    connect(m_aiService, &AiService::modelsFetchFailed, this, &AiAssistantDockWidget::onModelsFetchFailed);
    connect(m_aiService, &AiService::generationProgress, this, &AiAssistantDockWidget::onGenerationProgress);
    connect(m_aiService, &AiService::generationFinished, this, &AiAssistantDockWidget::onGenerationFinished);
    connect(m_aiService, &AiService::generationFailed, this, &AiAssistantDockWidget::onGenerationFailed);

    loadSettings();

    onBackendChanged();
    onModeChanged(m_comboMode ? m_comboMode->currentIndex() : 0);
}

AiAssistantDockWidget::~AiAssistantDockWidget() {
    saveSettings();
}

void AiAssistantDockWidget::setDocument(Document* doc) {
    m_document = doc;
}

void AiAssistantDockWidget::setCanvasWidget(CanvasWidget* canvas) {
    m_canvasWidget = canvas;
}

void AiAssistantDockWidget::setupUi() {
    auto rootWidget = new QWidget(this);
    auto rootLayout = new QVBoxLayout(rootWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto scrollArea = new QScrollArea(rootWidget);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto mainContainer = new QWidget(scrollArea);
    auto mainLayout = new QVBoxLayout(mainContainer);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // レイヤーパラメータ読み込みボタン
    m_btnLoadFromLayer = new QPushButton(tr("選択中レイヤーの生成パラメータを読込"), mainContainer);
    m_btnLoadFromLayer->setToolTip(tr("選択中のレイヤーに記憶されているAI生成パラメータ（プロンプト・モデル・シード等）を呼び出して入力欄に復元します"));
    m_btnLoadFromLayer->setStyleSheet("QPushButton { font-weight: bold; padding: 6px; background-color: #e3f2fd; border: 1px solid #90caf9; border-radius: 4px; } QPushButton:hover { background-color: #bbdefb; }");
    connect(m_btnLoadFromLayer, &QPushButton::clicked, this, &AiAssistantDockWidget::onLoadFromActiveLayerClicked);
    mainLayout->addWidget(m_btnLoadFromLayer);

    // 設定をデフォルト値にリセットボタン
    m_btnResetDefaults = new QPushButton(tr("⚙️ 設定をデフォルト値にリセット"), mainContainer);
    m_btnResetDefaults->setToolTip(tr("AIアシスタントのすべてのパラメータ設定を標準の初期値に戻します"));
    m_btnResetDefaults->setStyleSheet("QPushButton { padding: 5px; background-color: #f5f5f5; border: 1px solid #cfd8dc; border-radius: 4px; } QPushButton:hover { background-color: #eceff1; }");
    connect(m_btnResetDefaults, &QPushButton::clicked, this, &AiAssistantDockWidget::onResetDefaultsClicked);
    mainLayout->addWidget(m_btnResetDefaults);

    // 1. 接続設定グループ
    auto grpConn = new QGroupBox(tr("接続設定"), mainContainer);
    auto layConn = new QVBoxLayout(grpConn);

    auto gridRadio = new QGridLayout();
    m_radioComfyUi = new QRadioButton(tr("ローカル (ComfyUI)"), grpConn);
    m_radioWebUi = new QRadioButton(tr("ローカル (SD/Forge)"), grpConn);
    m_radioEmbedded = new QRadioButton(tr("組み込み (sd.exe/サーバー不要)"), grpConn);
    m_radioCloud = new QRadioButton(tr("クラウド (OpenAI)"), grpConn);
    m_radioComfyUi->setChecked(true);

    gridRadio->addWidget(m_radioComfyUi, 0, 0);
    gridRadio->addWidget(m_radioWebUi, 0, 1);
    gridRadio->addWidget(m_radioEmbedded, 1, 0);
    gridRadio->addWidget(m_radioCloud, 1, 1);
    layConn->addLayout(gridRadio);

    connect(m_radioComfyUi, &QRadioButton::toggled, this, &AiAssistantDockWidget::onBackendChanged);
    connect(m_radioWebUi, &QRadioButton::toggled, this, &AiAssistantDockWidget::onBackendChanged);
    connect(m_radioEmbedded, &QRadioButton::toggled, this, &AiAssistantDockWidget::onBackendChanged);
    connect(m_radioCloud, &QRadioButton::toggled, this, &AiAssistantDockWidget::onBackendChanged);

    auto formConn = new QFormLayout();
    m_editServerUrl = new QLineEdit("http://127.0.0.1:8188", grpConn);
    m_editApiKey = new QLineEdit(grpConn);
    m_editApiKey->setEchoMode(QLineEdit::Password);
    m_editApiKey->setPlaceholderText(tr("sk-... (クラウド利用時のみ)"));

    m_comboDevice = new QComboBox(grpConn);
    detectAvailableDevices();

    auto layModel = new QHBoxLayout();
    m_comboModel = new QComboBox(grpConn);
    m_comboModel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_btnFetchModels = new QPushButton(tr("更新"), grpConn);
    m_btnFetchModels->setMaximumWidth(50);
    connect(m_btnFetchModels, &QPushButton::clicked, this, &AiAssistantDockWidget::onFetchModelsClicked);
    layModel->addWidget(m_comboModel);
    layModel->addWidget(m_btnFetchModels);

    formConn->addRow(tr("サーバーURL:"), m_editServerUrl);
    formConn->addRow(tr("APIキー:"), m_editApiKey);
    formConn->addRow(tr("推論デバイス:"), m_comboDevice);
    formConn->addRow(tr("モデル:"), layModel);
    layConn->addLayout(formConn);
    mainLayout->addWidget(grpConn);

    // 2. モード選択
    auto grpMode = new QGroupBox(tr("制作モード"), mainContainer);
    auto layMode = new QVBoxLayout(grpMode);
    m_comboMode = new QComboBox(grpMode);
    m_comboMode->addItem(tr("ラフ清書 / アタリ補正 (Rough to Clean)"), static_cast<int>(AiMode::RoughToClean));
    m_comboMode->addItem(tr("選択範囲インペイント (Inpaint)"), static_cast<int>(AiMode::Inpaint));
    m_comboMode->addItem(tr("自動着彩 / カラーラフ (Colorization)"), static_cast<int>(AiMode::Colorization));
    m_comboMode->addItem(tr("テクスチャ・シームレス (Seamless Pattern)"), static_cast<int>(AiMode::SeamlessTexture));
    m_comboMode->addItem(tr("背景 / 新規生成 (Background / New)"), static_cast<int>(AiMode::BackgroundAndNew));
    connect(m_comboMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AiAssistantDockWidget::onModeChanged);
    layMode->addWidget(m_comboMode);
    mainLayout->addWidget(grpMode);

    // 3. プロンプト入力欄
    auto grpPrompt = new QGroupBox(tr("プロンプト"), mainContainer);
    auto layPrompt = new QVBoxLayout(grpPrompt);

    auto layPromptHeader = new QHBoxLayout();
    layPromptHeader->addWidget(new QLabel(tr("プロンプト:"), grpPrompt));
    layPromptHeader->addStretch();
    m_btnPromptAssist = new QPushButton(tr("🪄 LLM プロンプト支援..."), grpPrompt);
    m_btnPromptAssist->setStyleSheet("QPushButton { font-weight: bold; background-color: #6a1b9a; color: white; padding: 3px 8px; border-radius: 3px; } QPushButton:hover { background-color: #7b1fa2; }");
    connect(m_btnPromptAssist, &QPushButton::clicked, this, &AiAssistantDockWidget::onPromptAssistClicked);
    layPromptHeader->addWidget(m_btnPromptAssist);

    m_editPrompt = new QPlainTextEdit(grpPrompt);
    m_editPrompt->setPlaceholderText(tr("生成したい内容（例: 1girl, cute smile, school uniform, highly detailed...）"));
    m_editPrompt->setMaximumHeight(70);

    auto lblNeg = new QLabel(tr("ネガティブプロンプト:"), grpPrompt);
    m_editNegativePrompt = new QPlainTextEdit(grpPrompt);
    m_editNegativePrompt->setPlaceholderText(tr("除外したい要素（例: easynegative, worst quality, low quality...）"));
    m_editNegativePrompt->setMaximumHeight(50);
    m_editNegativePrompt->setPlainText("worst quality, low quality, bad anatomy, deformed");

    layPrompt->addLayout(layPromptHeader);
    layPrompt->addWidget(m_editPrompt);
    layPrompt->addWidget(lblNeg);
    layPrompt->addWidget(m_editNegativePrompt);
    mainLayout->addWidget(grpPrompt);

    // 4. ControlNet 設定
    auto grpCN = new QGroupBox(tr("ControlNet 設定"), mainContainer);
    auto layCN = new QVBoxLayout(grpCN);
    m_chkControlNet = new QCheckBox(tr("ControlNet を有効化"), grpCN);
    m_chkControlNet->setToolTip(tr("線画やポーズ、参照画像を拘束条件として指定して生成します"));
    layCN->addWidget(m_chkControlNet);

    auto formCN = new QFormLayout();
    m_comboControlNetType = new QComboBox(grpCN);
    m_comboControlNetType->addItem(tr("Lineart (線画抽出)"), static_cast<int>(ControlNetType::Lineart));
    m_comboControlNetType->addItem(tr("Depth (深度マップ)"), static_cast<int>(ControlNetType::Depth));
    m_comboControlNetType->addItem(tr("OpenPose (人体ポーズ)"), static_cast<int>(ControlNetType::Openpose));
    m_comboControlNetType->addItem(tr("Scribble (落書きアタリ)"), static_cast<int>(ControlNetType::Scribble));
    m_comboControlNetType->addItem(tr("Canny (輪郭エッジ)"), static_cast<int>(ControlNetType::Canny));
    m_comboControlNetType->addItem(tr("reference_only (リファレンス拘束)"), static_cast<int>(ControlNetType::ReferenceOnly));

    m_spinControlNetWeight = new QDoubleSpinBox(grpCN);
    m_spinControlNetWeight->setRange(0.1, 2.0);
    m_spinControlNetWeight->setSingleStep(0.1);
    m_spinControlNetWeight->setValue(1.0);

    m_comboControlNetSource = new QComboBox(grpCN);
    m_comboControlNetSource->addItem(tr("キャンバス (全体)"), static_cast<int>(AiImageSource::Canvas));
    m_comboControlNetSource->addItem(tr("キャンバス (アクティブレイヤー)"), static_cast<int>(AiImageSource::ActiveLayer));
    m_comboControlNetSource->addItem(tr("クリップボード"), static_cast<int>(AiImageSource::Clipboard));
    m_comboControlNetSource->addItem(tr("参考画像ウィンドウ"), static_cast<int>(AiImageSource::ReferenceWindow));
    m_comboControlNetSource->addItem(tr("ファイル"), static_cast<int>(AiImageSource::File));

    m_widgetControlNetFile = new QWidget(grpCN);
    auto layCnFile = new QHBoxLayout(m_widgetControlNetFile);
    layCnFile->setContentsMargins(0, 0, 0, 0);
    m_lblControlNetFile = new QLabel(tr("(未選択)"), m_widgetControlNetFile);
    m_lblControlNetFile->setStyleSheet("color: #666; font-size: 11px;");
    m_btnBrowseControlNetFile = new QPushButton(tr("参照..."), m_widgetControlNetFile);
    layCnFile->addWidget(m_lblControlNetFile, 1);
    layCnFile->addWidget(m_btnBrowseControlNetFile);
    m_widgetControlNetFile->setVisible(false);

    connect(m_comboControlNetSource, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AiAssistantDockWidget::onControlNetSourceChanged);
    connect(m_btnBrowseControlNetFile, &QPushButton::clicked, this, &AiAssistantDockWidget::onBrowseControlNetFile);

    formCN->addRow(tr("タイプ:"), m_comboControlNetType);
    formCN->addRow(tr("重み:"), m_spinControlNetWeight);
    formCN->addRow(tr("入力元:"), m_comboControlNetSource);
    formCN->addRow(tr("画像ファイル:"), m_widgetControlNetFile);
    layCN->addLayout(formCN);
    mainLayout->addWidget(grpCN);

    // 5. IP-Adapter 設定
    auto grpIP = new QGroupBox(tr("IP-Adapter 設定"), mainContainer);
    auto layIP = new QVBoxLayout(grpIP);
    m_chkIpAdapter = new QCheckBox(tr("IP-Adapter を有効化"), grpIP);
    m_chkIpAdapter->setToolTip(tr("参照画像のスタイル・画風・キャラクター特徴を転写して生成します (WebUI / Forge 対応)"));
    layIP->addWidget(m_chkIpAdapter);

    auto formIP = new QFormLayout();
    m_comboIpAdapterType = new QComboBox(grpIP);
    m_comboIpAdapterType->addItem(tr("標準 (IP-Adapter SD1.5)"), "ip-adapter_clip_sd15");
    m_comboIpAdapterType->addItem(tr("Plus (高精度 SD1.5)"), "ip-adapter_plus_sd15");
    m_comboIpAdapterType->addItem(tr("Face (顔特徴特化 SD1.5)"), "ip-adapter_face_sd15");
    m_comboIpAdapterType->addItem(tr("SDXL (IP-Adapter SDXL)"), "ip-adapter_clip_sdxl");
    m_comboIpAdapterType->addItem(tr("汎用 (ip-adapter)"), "ip-adapter");

    m_spinIpAdapterWeight = new QDoubleSpinBox(grpIP);
    m_spinIpAdapterWeight->setRange(0.05, 2.0);
    m_spinIpAdapterWeight->setSingleStep(0.05);
    m_spinIpAdapterWeight->setValue(0.80);

    m_comboIpAdapterSource = new QComboBox(grpIP);
    m_comboIpAdapterSource->addItem(tr("キャンバス (全体)"), static_cast<int>(AiImageSource::Canvas));
    m_comboIpAdapterSource->addItem(tr("キャンバス (アクティブレイヤー)"), static_cast<int>(AiImageSource::ActiveLayer));
    m_comboIpAdapterSource->addItem(tr("クリップボード"), static_cast<int>(AiImageSource::Clipboard));
    m_comboIpAdapterSource->addItem(tr("参考画像ウィンドウ"), static_cast<int>(AiImageSource::ReferenceWindow));
    m_comboIpAdapterSource->addItem(tr("ファイル"), static_cast<int>(AiImageSource::File));

    m_widgetIpAdapterFile = new QWidget(grpIP);
    auto layIpFile = new QHBoxLayout(m_widgetIpAdapterFile);
    layIpFile->setContentsMargins(0, 0, 0, 0);
    m_lblIpAdapterFile = new QLabel(tr("(未選択)"), m_widgetIpAdapterFile);
    m_lblIpAdapterFile->setStyleSheet("color: #666; font-size: 11px;");
    m_btnBrowseIpAdapterFile = new QPushButton(tr("参照..."), m_widgetIpAdapterFile);
    layIpFile->addWidget(m_lblIpAdapterFile, 1);
    layIpFile->addWidget(m_btnBrowseIpAdapterFile);
    m_widgetIpAdapterFile->setVisible(false);

    connect(m_comboIpAdapterSource, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AiAssistantDockWidget::onIpAdapterSourceChanged);
    connect(m_btnBrowseIpAdapterFile, &QPushButton::clicked, this, &AiAssistantDockWidget::onBrowseIpAdapterFile);

    formIP->addRow(tr("タイプ:"), m_comboIpAdapterType);
    formIP->addRow(tr("重み:"), m_spinIpAdapterWeight);
    formIP->addRow(tr("入力元:"), m_comboIpAdapterSource);
    formIP->addRow(tr("画像ファイル:"), m_widgetIpAdapterFile);
    layIP->addLayout(formIP);
    mainLayout->addWidget(grpIP);

    // 5. 解像度 & アップスケール設定
    auto grpRes = new QGroupBox(tr("解像度 & アップスケール"), mainContainer);
    auto formRes = new QFormLayout(grpRes);

    m_comboResolution = new QComboBox(grpRes);
    m_comboResolution->addItem(tr("512 x 512 (標準 1:1)"), QSize(512, 512));
    m_comboResolution->addItem(tr("768 x 768 (SDXL 1:1)"), QSize(768, 768));
    m_comboResolution->addItem(tr("832 x 1216 (SDXL 縦長 2:3)"), QSize(832, 1216));
    m_comboResolution->addItem(tr("1216 x 832 (SDXL 横長 3:2)"), QSize(1216, 832));
    m_comboResolution->addItem(tr("1024 x 1024 (高解像度 1:1)"), QSize(1024, 1024));
    m_comboResolution->addItem(tr("キャンバス比率追従 (Canvas Aspect)"), QSize(-1, -1));
    m_comboResolution->addItem(tr("カスタム (Custom)"), QSize(0, 0));

    auto layResCustom = new QHBoxLayout();
    m_spinWidth = new QSpinBox(grpRes);
    m_spinWidth->setRange(64, 4096);
    m_spinWidth->setSingleStep(64);
    m_spinWidth->setValue(512);
    m_spinWidth->setSuffix(" px");

    m_spinHeight = new QSpinBox(grpRes);
    m_spinHeight->setRange(64, 4096);
    m_spinHeight->setSingleStep(64);
    m_spinHeight->setValue(512);
    m_spinHeight->setSuffix(" px");

    connect(m_comboResolution, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AiAssistantDockWidget::onResolutionPresetChanged);
    connect(m_spinWidth, QOverload<int>::of(&QSpinBox::valueChanged), this, &AiAssistantDockWidget::onCustomResolutionChanged);
    connect(m_spinHeight, QOverload<int>::of(&QSpinBox::valueChanged), this, &AiAssistantDockWidget::onCustomResolutionChanged);

    layResCustom->addWidget(new QLabel(tr("W:"), grpRes));
    layResCustom->addWidget(m_spinWidth);
    layResCustom->addWidget(new QLabel(tr("H:"), grpRes));
    layResCustom->addWidget(m_spinHeight);

    m_comboUpscaleMethod = new QComboBox(grpRes);
    QStringList upscaleList = scanUpscaleModels();
    for (const auto& u : upscaleList) {
        m_comboUpscaleMethod->addItem(u);
    }

    formRes->addRow(tr("解像度プリセット:"), m_comboResolution);
    formRes->addRow(tr("生成サイズ:"), layResCustom);
    formRes->addRow(tr("アップスケール方式:"), m_comboUpscaleMethod);
    mainLayout->addWidget(grpRes);

    // 6. 生成パラメータ & 乱数設定
    auto grpParams = new QGroupBox(tr("生成パラメータ & 乱数"), mainContainer);
    auto formParams = new QFormLayout(grpParams);

    m_spinBatchCount = new QSpinBox(grpParams);
    m_spinBatchCount->setRange(1, 4);
    m_spinBatchCount->setValue(1);

    m_spinDenoising = new QDoubleSpinBox(grpParams);
    m_spinDenoising->setRange(0.05, 1.0);
    m_spinDenoising->setSingleStep(0.05);
    m_spinDenoising->setValue(0.55);

    auto laySeed = new QHBoxLayout();
    m_spinSeed = new QSpinBox(grpParams);
    m_spinSeed->setRange(-1, 2147483647);
    m_spinSeed->setValue(-1);
    m_spinSeed->setSpecialValueText(tr("ランダム (-1)"));
    m_btnRandomSeed = new QPushButton(tr("🎲"), grpParams);
    m_btnRandomSeed->setMaximumWidth(32);
    connect(m_btnRandomSeed, &QPushButton::clicked, this, &AiAssistantDockWidget::onRandomSeedClicked);
    laySeed->addWidget(m_spinSeed);
    laySeed->addWidget(m_btnRandomSeed);

    m_spinSteps = new QSpinBox(grpParams);
    m_spinSteps->setRange(10, 50);
    m_spinSteps->setValue(20);

    m_spinCfg = new QDoubleSpinBox(grpParams);
    m_spinCfg->setRange(1.0, 15.0);
    m_spinCfg->setSingleStep(0.5);
    m_spinCfg->setValue(7.0);

    formParams->addRow(tr("生成枚数:"), m_spinBatchCount);
    formParams->addRow(tr("生成強度 (Denoise):"), m_spinDenoising);
    formParams->addRow(tr("シード値:"), laySeed);
    formParams->addRow(tr("ステップ数:"), m_spinSteps);
    formParams->addRow(tr("CFG Scale:"), m_spinCfg);
    mainLayout->addWidget(grpParams);

    mainLayout->addStretch(1);
    scrollArea->setWidget(mainContainer);

    // 下部に固定表示するコンテナ（スクロール領域外で常時表示）
    auto bottomContainer = new QWidget(rootWidget);
    auto bottomLayout = new QVBoxLayout(bottomContainer);
    bottomLayout->setContentsMargins(8, 4, 8, 8);
    bottomLayout->setSpacing(6);

    // 7. 生成実行アクション
    auto grpAction = new QGroupBox(tr("生成実行"), bottomContainer);
    auto layAction = new QVBoxLayout(grpAction);

    auto layActionBtns = new QHBoxLayout();
    m_btnGenerate = new QPushButton(tr("🎨 生成開始"), grpAction);
    m_btnGenerate->setStyleSheet("QPushButton { font-weight: bold; background-color: #2e7d32; color: white; padding: 6px; } QPushButton:hover { background-color: #388e3c; }");
    m_btnCancel = new QPushButton(tr("キャンセル"), grpAction);
    m_btnCancel->setEnabled(false);
    layActionBtns->addWidget(m_btnGenerate);
    layActionBtns->addWidget(m_btnCancel);
    layAction->addLayout(layActionBtns);

    m_progressBar = new QProgressBar(grpAction);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_lblStatus = new QLabel(tr("待機中"), grpAction);
    m_lblStatus->setAlignment(Qt::AlignCenter);

    layAction->addWidget(m_progressBar);
    layAction->addWidget(m_lblStatus);
    bottomLayout->addWidget(grpAction);

    connect(m_btnGenerate, &QPushButton::clicked, this, &AiAssistantDockWidget::onGenerateClicked);
    connect(m_btnCancel, &QPushButton::clicked, this, &AiAssistantDockWidget::onCancelClicked);

    // 8. 生成結果プレビュー & キャンバス適用
    auto grpResult = new QGroupBox(tr("生成結果 & キャンバス適用"), bottomContainer);
    auto layResult = new QVBoxLayout(grpResult);

    m_listThumbnails = new QListWidget(grpResult);
    m_listThumbnails->setViewMode(QListView::IconMode);
    m_listThumbnails->setIconSize(QSize(100, 100));
    m_listThumbnails->setMovement(QListView::Static);
    m_listThumbnails->setMaximumHeight(120);
    m_listThumbnails->setSpacing(4);
    connect(m_listThumbnails, &QListWidget::currentRowChanged, this, &AiAssistantDockWidget::onThumbnailSelected);
    layResult->addWidget(m_listThumbnails);

    auto layApply = new QHBoxLayout();
    m_btnApplyNewLayer = new QPushButton(tr("＋ 新規通常レイヤー"), grpResult);
    m_btnApplyOverwrite = new QPushButton(tr("▨ 選択範囲内に上書き"), grpResult);
    m_btnApplyNewLayer->setEnabled(false);
    m_btnApplyOverwrite->setEnabled(false);

    connect(m_btnApplyNewLayer, &QPushButton::clicked, this, &AiAssistantDockWidget::onApplyNewLayerClicked);
    connect(m_btnApplyOverwrite, &QPushButton::clicked, this, &AiAssistantDockWidget::onApplyOverwriteClicked);

    layApply->addWidget(m_btnApplyNewLayer);
    layApply->addWidget(m_btnApplyOverwrite);
    layResult->addLayout(layApply);
    bottomLayout->addWidget(grpResult);

    rootLayout->addWidget(scrollArea, 1);
    rootLayout->addWidget(bottomContainer, 0);

    setWidget(rootWidget);

    // 起動時にローカルモデルフォルダを自動スキャンして一覧に設定
    refreshLocalModels();
}

void AiAssistantDockWidget::onBackendChanged() {
    auto shouldResetUrl = [](const QString& txt, int targetPort) {
        if (txt.isEmpty() || txt.startsWith('(') || txt.contains("api.openai.com")) return true;
        if (targetPort == 8188 && txt.contains(":7860")) return true;
        if (targetPort == 7860 && txt.contains(":8188")) return true;
        return false;
    };

    if (m_radioComfyUi->isChecked()) {
        m_editServerUrl->setEnabled(true);
        if (shouldResetUrl(m_editServerUrl->text(), 8188)) {
            m_editServerUrl->setText("http://127.0.0.1:8188");
        }
        m_editApiKey->setEnabled(false);
        m_chkControlNet->setEnabled(true);
        if (m_chkIpAdapter) m_chkIpAdapter->setEnabled(true);
    } else if (m_radioWebUi->isChecked()) {
        m_editServerUrl->setEnabled(true);
        if (shouldResetUrl(m_editServerUrl->text(), 7860)) {
            m_editServerUrl->setText("http://127.0.0.1:7860");
        }
        m_editApiKey->setEnabled(false);
        m_chkControlNet->setEnabled(true);
        if (m_chkIpAdapter) m_chkIpAdapter->setEnabled(true);
    } else if (m_radioEmbedded->isChecked()) {
        m_editServerUrl->setEnabled(false);
        m_editServerUrl->setText(tr("(サーバー不要 - 直接推論)"));
        m_editApiKey->setEnabled(false);
        m_chkControlNet->setEnabled(false);
        if (m_chkIpAdapter) m_chkIpAdapter->setEnabled(false);
    } else if (m_radioCloud->isChecked()) {
        m_editServerUrl->setEnabled(false);
        m_editServerUrl->setText("https://api.openai.com");
        m_editApiKey->setEnabled(true);
        m_chkControlNet->setEnabled(false);
        if (m_chkIpAdapter) m_chkIpAdapter->setEnabled(false);
    }

    onFetchModelsClicked();
}

void AiAssistantDockWidget::onModeChanged(int index) {
    auto mode = static_cast<AiMode>(m_comboMode->itemData(index).toInt());
    updateUiForMode(mode);
}

void AiAssistantDockWidget::updateUiForMode(AiMode mode) {
    switch (mode) {
    case AiMode::RoughToClean:
        m_spinDenoising->setValue(0.55);
        m_chkControlNet->setChecked(true);
        m_comboControlNetType->setCurrentIndex(0); // Lineart
        break;
    case AiMode::Inpaint:
        m_spinDenoising->setValue(0.75);
        break;
    case AiMode::Colorization:
        m_spinDenoising->setValue(0.65);
        m_chkControlNet->setChecked(true);
        m_comboControlNetType->setCurrentIndex(0); // Lineart
        break;
    case AiMode::SeamlessTexture:
        m_spinDenoising->setValue(1.0);
        m_chkControlNet->setChecked(false);
        break;
    case AiMode::BackgroundAndNew:
        m_spinDenoising->setValue(1.0);
        break;
    }
}

QStringList AiAssistantDockWidget::scanLocalModels() const {
    QStringList result;
    QString dirPath = PreferencesDialog::aiModelDirectory();
    if (dirPath.isEmpty()) return result;

    QString cleanBase = QDir::fromNativeSeparators(dirPath.trimmed());
    cleanBase = QDir::cleanPath(cleanBase);
    QDir baseDir(cleanBase);
    if (!baseDir.exists()) return result;

    QStringList filters;
    filters << "*.safetensors" << "*.ckpt" << "*.pt" << "*.bin" << "*.onnx";

    // サブディレクトリを含めて再帰検索
    QDirIterator it(cleanBase, filters, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QString relPath = baseDir.relativeFilePath(it.filePath());
        relPath = QDir::fromNativeSeparators(relPath);
        result.append(relPath);
    }

    result.sort(Qt::CaseInsensitive);
    return result;
}

QStringList AiAssistantDockWidget::scanLocalControlNetModels() const {
    QStringList result;
    QString dirPath = PreferencesDialog::aiControlNetDirectory();

    // ControlNet保存フォルダが未指定の場合、AIモデル保存フォルダ直下のControlNetサブフォルダを自動探索
    if (dirPath.isEmpty()) {
        QString modelDir = PreferencesDialog::aiModelDirectory();
        if (!modelDir.isEmpty()) {
            QDir mDir(QDir::cleanPath(QDir::fromNativeSeparators(modelDir.trimmed())));
            if (mDir.exists("ControlNet")) {
                dirPath = mDir.filePath("ControlNet");
            } else if (mDir.exists("controlnet")) {
                dirPath = mDir.filePath("controlnet");
            }
        }
    }

    if (dirPath.isEmpty()) return result;

    QString cleanBase = QDir::fromNativeSeparators(dirPath.trimmed());
    cleanBase = QDir::cleanPath(cleanBase);
    QDir baseDir(cleanBase);
    if (!baseDir.exists()) return result;

    QStringList filters;
    filters << "*.safetensors" << "*.pth" << "*.ckpt" << "*.bin" << "*.onnx";

    // サブディレクトリを含めて再帰検索
    QDirIterator it(cleanBase, filters, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QString relPath = baseDir.relativeFilePath(it.filePath());
        relPath = QDir::fromNativeSeparators(relPath);
        result.append(relPath);
    }

    result.sort(Qt::CaseInsensitive);
    return result;
}

void AiAssistantDockWidget::refreshLocalModels() {
    bool isLocal = m_radioComfyUi->isChecked() || m_radioWebUi->isChecked() || m_radioEmbedded->isChecked();
    if (!isLocal) return;

    QString current = m_comboModel->currentText();
    QStringList localModels = scanLocalModels();
    m_comboModel->clear();

    if (!localModels.isEmpty()) {
        for (const auto& m : localModels) {
            m_comboModel->addItem(m);
        }
        int idx = m_comboModel->findText(current);
        if (idx != -1) {
            m_comboModel->setCurrentIndex(idx);
        } else {
            m_comboModel->setCurrentIndex(0);
        }
        m_lblStatus->setText(tr("ローカルフォルダから %1 件のモデルを検出").arg(localModels.size()));
        m_comboModel->setToolTip(tr("選択中のモデル: %1\n保存元: %2").arg(m_comboModel->currentText(), PreferencesDialog::aiModelDirectory()));
    } else {
        QString modelDir = PreferencesDialog::aiModelDirectory();
        if (modelDir.isEmpty()) {
            m_comboModel->addItem(tr("(未設定: 環境設定でモデルフォルダを指定)"));
            m_lblStatus->setText(tr("⚠️ 環境設定からモデル保存フォルダを指定してください"));
        } else {
            m_comboModel->addItem(tr("(モデルが見つかりません: %1)").arg(modelDir));
            m_lblStatus->setText(tr("⚠️ フォルダ内にモデルが見つかりません: %1").arg(modelDir));
        }
        m_comboModel->setToolTip(tr("メニュー「ファイル」→「環境設定」から画像生成AIモデル保存フォルダを設定してください。"));
    }

    // ControlNet モデルのスキャン
    QStringList cnModels = scanLocalControlNetModels();
    if (!cnModels.isEmpty()) {
        for (const auto& cn : cnModels) {
            if (m_comboControlNetType->findText(cn) == -1) {
                m_comboControlNetType->addItem(cn);
            }
        }
    }
}

void AiAssistantDockWidget::onFetchModelsClicked() {
    ApiBackend backend = ApiBackend::LocalComfyUI;
    if (m_radioWebUi->isChecked()) backend = ApiBackend::LocalWebUI;
    else if (m_radioEmbedded->isChecked()) backend = ApiBackend::EmbeddedEngine;
    else if (m_radioCloud->isChecked()) backend = ApiBackend::CloudOpenAI;

    if (backend == ApiBackend::LocalComfyUI || backend == ApiBackend::LocalWebUI || backend == ApiBackend::EmbeddedEngine) {
        refreshLocalModels();
    }

    m_aiService->fetchModels(backend, m_editServerUrl->text(), m_editApiKey->text());
}

void AiAssistantDockWidget::detectAvailableDevices() {
    if (!m_comboDevice) return;
    m_comboDevice->clear();
    m_comboDevice->addItem(tr("CUDA (NVIDIA GPU - 推奨)"), "cuda");
    m_comboDevice->addItem(tr("ROCm (AMD GPU)"), "rocm");
    m_comboDevice->addItem(tr("OpenVINO (Intel CPU/GPU/NPU)"), "openvino");
    m_comboDevice->addItem(tr("CPU (低速)"), "cpu");
    m_comboDevice->setCurrentIndex(0);
}

QStringList AiAssistantDockWidget::scanUpscaleModels() const {
    QStringList result;
    result << tr("Bicubic (バイキュービック - 滑らか)");
    result << tr("Lanczos (高品質リサンプリング)");
    result << tr("Bilinear (バイリニア)");
    result << tr("Nearest Neighbor (ドット絵・ニアレスト)");
    result << "R-ESRGAN 4x+ Anime6B";
    result << "RealESRGAN_x4plus";

    QStringList searchDirs;
    QString modelDir = PreferencesDialog::aiModelDirectory();
    if (!modelDir.isEmpty()) {
        QDir mDir(QDir::cleanPath(QDir::fromNativeSeparators(modelDir.trimmed())));
        QDir parentDir = mDir;
        parentDir.cdUp();
        searchDirs << mDir.filePath("upscale_models");
        searchDirs << mDir.filePath("ESRGAN");
        searchDirs << mDir.filePath("RealESRGAN");
        searchDirs << parentDir.filePath("upscale_models");
        searchDirs << parentDir.filePath("ESRGAN");
        searchDirs << parentDir.filePath("RealESRGAN");
    }

    QString comfyPath = PreferencesDialog::comfyUiLaunchPath();
    if (!comfyPath.isEmpty()) {
        QFileInfo fi(comfyPath);
        QDir cDir = fi.absoluteDir();
        searchDirs << cDir.filePath("models/upscale_models");
        searchDirs << cDir.filePath("models/ESRGAN");
        searchDirs << cDir.filePath("Data/Models/ESRGAN");
        searchDirs << cDir.filePath("Data/Models/upscale_models");
    }

    QStringList filters;
    filters << "*.pth" << "*.safetensors" << "*.onnx" << "*.bin";

    for (const auto& path : searchDirs) {
        QDir d(path);
        if (d.exists()) {
            QDirIterator it(path, filters, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                QString baseName = it.fileInfo().completeBaseName();
                if (!result.contains(baseName) && !result.contains(it.fileName())) {
                    result.append(baseName);
                }
            }
        }
    }

    return result;
}

void AiAssistantDockWidget::onResolutionPresetChanged(int index) {
    if (!m_comboResolution || !m_spinWidth || !m_spinHeight) return;

    QVariant val = m_comboResolution->itemData(index);
    QSize sz = val.toSize();

    if (sz.width() > 0 && sz.height() > 0) {
        m_spinWidth->blockSignals(true);
        m_spinHeight->blockSignals(true);
        m_spinWidth->setValue(sz.width());
        m_spinHeight->setValue(sz.height());
        m_spinWidth->blockSignals(false);
        m_spinHeight->blockSignals(false);
    } else if (sz.width() == -1) {
        // キャンバス比率追従
        if (m_document) {
            float aspect = static_cast<float>(m_document->width()) / std::max(1, m_document->height());
            int w = 512, h = 512;
            if (aspect >= 1.0f) {
                w = 512;
                h = std::clamp(static_cast<int>(512.0f / aspect), 256, 1024);
            } else {
                h = 512;
                w = std::clamp(static_cast<int>(512.0f * aspect), 256, 1024);
            }
            w = (w / 64) * 64;
            h = (h / 64) * 64;
            m_spinWidth->blockSignals(true);
            m_spinHeight->blockSignals(true);
            m_spinWidth->setValue(w);
            m_spinHeight->setValue(h);
            m_spinWidth->blockSignals(false);
            m_spinHeight->blockSignals(false);
        }
    }
}

void AiAssistantDockWidget::onCustomResolutionChanged() {
    if (!m_comboResolution) return;
    int customIdx = m_comboResolution->count() - 1;
    if (m_comboResolution->currentIndex() != customIdx) {
        m_comboResolution->blockSignals(true);
        m_comboResolution->setCurrentIndex(customIdx);
        m_comboResolution->blockSignals(false);
    }
}

void AiAssistantDockWidget::onRandomSeedClicked() {
    m_spinSeed->setValue(QRandomGenerator::global()->bounded(100000000, 2147483647));
}

bool AiAssistantDockWidget::ensureRequirementsReady() {
    // 1. 組み込みエンジンの不足チェック
    if (m_radioEmbedded->isChecked()) {
        QString enginePath = PreferencesDialog::embeddedEnginePath();
        if (enginePath.isEmpty() || !QFile::exists(enginePath)) {
            auto info = AiComponentDownloader::getComponentInfo(ComponentType::EmbeddedSdExe);
            auto reply = QMessageBox::question(
                this,
                tr("組み込み推論エンジンのダウンロード確認"),
                tr("組み込み推論エンジン (sd.exe) が見つかりません。\n\n"
                   "ローカルサーバー不要で動作する Stable Diffusion 推論エンジンをダウンロードしますか？\n\n"
                   "・ファイル名: %1\n"
                   "・サイズ目安: 約 %2 MB\n\n"
                   "※ 「はい」を選択すると保存先フォルダの選択画面が開きます。")
                .arg(info.fileName)
                .arg(info.estimatedSizeBytes / (1024 * 1024)),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes
            );

            if (reply == QMessageBox::Yes) {
                QString chosenDir = QFileDialog::getExistingDirectory(
                    this,
                    tr("組み込み推論エンジンの保存先フォルダを選択"),
                    info.targetDirectory,
                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
                );
                if (chosenDir.isEmpty()) {
                    m_lblStatus->setText(tr("保存先フォルダの指定がキャンセルされました"));
                    return false;
                }

                m_lblStatus->setText(tr("組み込みエンジンをダウンロード中..."));
                AiComponentDownloader downloader(this);
                if (downloader.downloadComponentWithDialog(ComponentType::EmbeddedSdExe, this, chosenDir)) {
                    QString downloadedPath = downloader.installedFilePath();
                    if (downloadedPath.isEmpty() || !QFile::exists(downloadedPath)) {
                        downloadedPath = AiComponentDownloader::getLocalFilePath(ComponentType::EmbeddedSdExe);
                    }
                    PreferencesDialog::setEmbeddedEnginePath(downloadedPath);
                    m_lblStatus->setText(tr("組み込みエンジンのセットアップが完了しました"));
                } else {
                    if (downloader.isUserCancelled()) {
                        m_lblStatus->setText(tr("ダウンロードがキャンセルされました"));
                    } else {
                        m_lblStatus->setText(tr("ダウンロード失敗: %1").arg(downloader.errorMessage()));
                    }
                    return false;
                }
            } else {
                m_lblStatus->setText(tr("組み込みエンジンがないため処理を中断しました"));
                return false;
            }
        }
    }

    // 2. アップスケール方式で AIモデル（Anime6B等）が指定されている場合の不足チェック
    QString upscaleMethod = m_comboUpscaleMethod ? m_comboUpscaleMethod->currentText() : QString();
    if (upscaleMethod.contains("Anime6B", Qt::CaseInsensitive) || upscaleMethod.contains("RealESRGAN", Qt::CaseInsensitive)) {
        ComponentType upType = upscaleMethod.contains("Anime6B", Qt::CaseInsensitive)
            ? ComponentType::UpscaleModelAnime6B
            : ComponentType::UpscaleModelRealEsrgan;

        if (!AiComponentDownloader::isComponentInstalled(upType)) {
            auto info = AiComponentDownloader::getComponentInfo(upType);
            auto reply = QMessageBox::question(
                this,
                tr("アップスケールモデルのダウンロード確認"),
                tr("選択されたAIアップスケールモデルが見つかりません。\n\n"
                   "モデルファイルを自動ダウンロードしますか？\n\n"
                   "・モデル名: %1\n"
                   "・サイズ目安: 約 %2 MB\n\n"
                   "※ 「はい」を選択すると保存先フォルダの選択画面が開きます。\n"
                   "※ 「いいえ」を選択した場合、高品質バイキュービック補間が代用されます。")
                .arg(info.name)
                .arg(info.estimatedSizeBytes / (1024 * 1024)),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes
            );

            if (reply == QMessageBox::Yes) {
                QString chosenDir = QFileDialog::getExistingDirectory(
                    this,
                    tr("アップスケールモデルの保存先フォルダを選択"),
                    info.targetDirectory,
                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
                );
                if (!chosenDir.isEmpty()) {
                    AiComponentDownloader downloader(this);
                    if (downloader.downloadComponentWithDialog(upType, this, chosenDir)) {
                        m_lblStatus->setText(tr("アップスケールモデルをダウンロードしました"));
                    }
                }
            }
        }
    }

    // 3. ControlNet 有効時のモデル不足チェック
    if (m_chkControlNet && m_chkControlNet->isChecked() && m_chkControlNet->isEnabled()) {
        ControlNetType cnType = static_cast<ControlNetType>(m_comboControlNetType->currentData().toInt());
        if (cnType != ControlNetType::ReferenceOnly && !AiComponentDownloader::isComponentInstalled(ComponentType::ControlNetLineart)) {
            QStringList cnModels = scanLocalControlNetModels();
            if (cnModels.isEmpty()) {
                auto info = AiComponentDownloader::getComponentInfo(ComponentType::ControlNetLineart);
                auto reply = QMessageBox::question(
                    this,
                    tr("ControlNet モデルのダウンロード確認"),
                    tr("ControlNet 線画抽出モデルが見つかりません。\n\n"
                       "モデルファイルをダウンロードしますか？\n\n"
                       "・モデル名: %1\n"
                       "・サイズ目安: 約 %2 MB\n\n"
                       "※ 「はい」を選択すると保存先フォルダの選択画面が開きます。\n"
                       "※ 「いいえ」を選択した場合、ControlNet なしで生成を続行します。")
                    .arg(info.name)
                    .arg(info.estimatedSizeBytes / (1024 * 1024)),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::Yes
                );

                if (reply == QMessageBox::Yes) {
                    QString chosenDir = QFileDialog::getExistingDirectory(
                        this,
                        tr("ControlNet モデルの保存先フォルダを選択"),
                        info.targetDirectory,
                        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
                    );
                    if (!chosenDir.isEmpty()) {
                        AiComponentDownloader downloader(this);
                        if (downloader.downloadComponentWithDialog(ComponentType::ControlNetLineart, this, chosenDir)) {
                            PreferencesDialog::setAiControlNetDirectory(chosenDir);
                            m_lblStatus->setText(tr("ControlNet モデルをダウンロードしました"));
                            refreshLocalModels();
                        }
                    }
                }
            }
        }
    }

    // 4. ローカル/組み込み生成で AIモデル (Checkpoint) が1件も見つからない場合
    bool isLocalMode = m_radioComfyUi->isChecked() || m_radioWebUi->isChecked() || m_radioEmbedded->isChecked();
    if (isLocalMode) {
        QString currentModel = m_comboModel ? m_comboModel->currentText().trimmed() : QString();
        if (currentModel.isEmpty() || currentModel.startsWith("(") || m_comboModel->count() == 0) {
            auto info = AiComponentDownloader::getComponentInfo(ComponentType::BaseSdModel);
            auto reply = QMessageBox::question(
                this,
                tr("画像生成モデル (Checkpoint) の確認"),
                tr("画像生成モデルが見つかりません。\n\n"
                   "標準の画像生成モデルをダウンロードしますか？\n\n"
                   "・モデル名: %1\n"
                   "・サイズ目安: 約 %2 MB\n\n"
                   "※ 「はい」を選択すると保存先フォルダの選択画面が開きます。\n"
                   "※ 「いいえ」を選択した場合、メニュー「ファイル」→「環境設定」から既存のモデルフォルダを指定してください。")
                .arg(info.name)
                .arg(info.estimatedSizeBytes / (1024 * 1024)),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No
            );

            if (reply == QMessageBox::Yes) {
                QString chosenDir = QFileDialog::getExistingDirectory(
                    this,
                    tr("画像生成モデルの保存先フォルダを選択"),
                    info.targetDirectory,
                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
                );
                if (chosenDir.isEmpty()) {
                    m_lblStatus->setText(tr("保存先フォルダの指定がキャンセルされました"));
                    return false;
                }

                AiComponentDownloader downloader(this);
                if (downloader.downloadComponentWithDialog(ComponentType::BaseSdModel, this, chosenDir)) {
                    PreferencesDialog::setAiModelDirectory(chosenDir);
                    refreshLocalModels();
                    m_lblStatus->setText(tr("モデルをセットアップしました"));
                } else {
                    if (downloader.isUserCancelled()) {
                        m_lblStatus->setText(tr("ダウンロードがキャンセルされました"));
                    } else {
                        m_lblStatus->setText(tr("ダウンロード失敗: %1").arg(downloader.errorMessage()));
                    }
                    return false;
                }
            } else {
                m_lblStatus->setText(tr("モデルが選択されていません"));
                return false;
            }
        }
    }

    return true;
}

void AiAssistantDockWidget::onGenerateClicked() {
    if (m_aiService->isBusy()) return;

    // 不足しているコンポーネントの事前確認とダウンロード
    if (!ensureRequirementsReady()) {
        return;
    }

    AiGenerateParams params;
    if (m_radioComfyUi->isChecked()) {
        params.backend = ApiBackend::LocalComfyUI;
    } else if (m_radioWebUi->isChecked()) {
        params.backend = ApiBackend::LocalWebUI;
    } else if (m_radioEmbedded->isChecked()) {
        params.backend = ApiBackend::EmbeddedEngine;
    } else {
        params.backend = ApiBackend::CloudOpenAI;
    }

    params.serverUrl = m_editServerUrl->text();
    params.apiKey = m_editApiKey->text();
    params.modelName = m_comboModel->currentText();

    params.autoLaunchServer = PreferencesDialog::isAutoLaunchServerEnabled();
    params.comfyUiLaunchPath = PreferencesDialog::comfyUiLaunchPath();
    params.webUiLaunchPath = PreferencesDialog::webUiLaunchPath();
    params.embeddedEnginePath = PreferencesDialog::embeddedEnginePath();
    params.modelDirectory = PreferencesDialog::aiModelDirectory();

    params.computeDevice = m_comboDevice ? m_comboDevice->currentData().toString() : "cuda";
    params.upscaleMethod = m_comboUpscaleMethod ? m_comboUpscaleMethod->currentText() : "bicubic";

    params.mode = static_cast<AiMode>(m_comboMode->currentData().toInt());
    params.prompt = m_editPrompt->toPlainText().trimmed();
    params.negativePrompt = m_editNegativePrompt->toPlainText().trimmed();

    params.batchCount = m_spinBatchCount->value();
    params.steps = m_spinSteps->value();
    params.cfgScale = static_cast<float>(m_spinCfg->value());
    params.denoisingStrength = static_cast<float>(m_spinDenoising->value());

    int64_t inputSeed = m_spinSeed->value();
    if (inputSeed < 0) {
        // -1 の場合は実際に使用する乱数シードを生成して決定し、UIの表示も変更
        int64_t actualSeed = static_cast<int64_t>(QRandomGenerator::global()->bounded(1ll, 2147483647ll));
        params.seed = actualSeed;
        m_spinSeed->setValue(static_cast<int>(actualSeed));
    } else {
        params.seed = inputSeed;
    }

    // 指定された解像度を使用 (64の倍数にアライン)
    int w = m_spinWidth ? m_spinWidth->value() : 512;
    int h = m_spinHeight ? m_spinHeight->value() : 512;
    w = (w / 64) * 64;
    h = (h / 64) * 64;
    if (w < 64) w = 512;
    if (h < 64) h = 512;
    params.width = w;
    params.height = h;

    params.initImage = getInitImageForCurrentMode();
    params.maskImage = getMaskImageForCurrentMode();

    if (m_chkControlNet && m_chkControlNet->isChecked()) {
        QString cnError;
        QImage cnImg = getControlNetImage(&cnError);
        if (cnImg.isNull()) {
            QMessageBox::warning(this, tr("ControlNet エラー"), cnError);
            m_lblStatus->setText(tr("⚠️ ControlNet: %1").arg(cnError));
            m_btnGenerate->setEnabled(true);
            m_btnCancel->setEnabled(false);
            return;
        }
        params.enableControlNet = true;
        params.controlNetType = static_cast<ControlNetType>(m_comboControlNetType->currentData().toInt());
        params.controlNetWeight = static_cast<float>(m_spinControlNetWeight->value());
        params.controlNetImage = cnImg;
    } else {
        params.enableControlNet = false;
    }

    if (m_chkIpAdapter && m_chkIpAdapter->isChecked()) {
        QString ipError;
        QImage ipImg = getIpAdapterImage(&ipError);
        if (ipImg.isNull()) {
            QMessageBox::warning(this, tr("IP-Adapter エラー"), ipError);
            m_lblStatus->setText(tr("⚠️ IP-Adapter: %1").arg(ipError));
            m_btnGenerate->setEnabled(true);
            m_btnCancel->setEnabled(false);
            return;
        }
        params.enableIpAdapter = true;
        params.ipAdapterType = m_comboIpAdapterType->currentData().toString();
        params.ipAdapterWeight = static_cast<float>(m_spinIpAdapterWeight->value());
        params.ipAdapterImage = ipImg;
    } else {
        params.enableIpAdapter = false;
    }

    // 生成パラメータを記憶（レイヤー適用用）
    m_lastGeneratedParams = params;
    m_lastGeneratedJson = AiService::paramsToJson(params);

    // 前回の設定を記憶（※プロンプトとネガティブプロンプトは除く）
    saveSettings();

    m_btnGenerate->setEnabled(false);
    m_btnCancel->setEnabled(true);
    m_progressBar->setValue(0);
    m_lblStatus->setText(tr("生成中..."));

    m_aiService->generate(params);
}

void AiAssistantDockWidget::onCancelClicked() {
    m_aiService->cancel();
    m_btnGenerate->setEnabled(true);
    m_btnCancel->setEnabled(false);
}

QImage AiAssistantDockWidget::getInitImageForCurrentMode() {
    if (!m_document) return QImage();

    AiMode mode = static_cast<AiMode>(m_comboMode->currentData().toInt());
    if (mode == AiMode::BackgroundAndNew || mode == AiMode::SeamlessTexture) {
        return QImage(); // txt2img
    }

    auto activeLayer = m_document->activeLayer();
    if (activeLayer) {
        return activeLayer->tileManager().toQImage();
    }
    return m_document->compositeBuffer().toQImage();
}

QImage AiAssistantDockWidget::getMaskImageForCurrentMode() {
    if (!m_document || !m_document->hasSelection()) return QImage();

    AiMode mode = static_cast<AiMode>(m_comboMode->currentData().toInt());
    if (mode != AiMode::Inpaint) return QImage();

    int w = m_document->width();
    int h = m_document->height();
    QImage mask(w, h, QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::black); // 黒 = 保持

    QPainter p(&mask);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(Qt::white); // 白 = 再生成
    p.setPen(Qt::NoPen);
    p.drawPath(m_document->selection());
    p.end();

    return mask;
}

void AiAssistantDockWidget::onBrowseControlNetFile() {
    QString startDir = m_controlNetFilePath.isEmpty() ? QString() : QFileInfo(m_controlNetFilePath).absolutePath();
    QString file = QFileDialog::getOpenFileName(
        this,
        tr("ControlNet 参照画像を選択"),
        startDir,
        tr("画像ファイル (*.png *.jpg *.jpeg *.bmp *.webp);;すべてのファイル (*.*)")
    );
    if (!file.isEmpty()) {
        m_controlNetFilePath = file;
        if (m_lblControlNetFile) {
            m_lblControlNetFile->setText(QFileInfo(file).fileName());
            m_lblControlNetFile->setToolTip(file);
        }
    }
}

void AiAssistantDockWidget::onBrowseIpAdapterFile() {
    QString startDir = m_ipAdapterFilePath.isEmpty() ? QString() : QFileInfo(m_ipAdapterFilePath).absolutePath();
    QString file = QFileDialog::getOpenFileName(
        this,
        tr("IP-Adapter 参照画像を選択"),
        startDir,
        tr("画像ファイル (*.png *.jpg *.jpeg *.bmp *.webp);;すべてのファイル (*.*)")
    );
    if (!file.isEmpty()) {
        m_ipAdapterFilePath = file;
        if (m_lblIpAdapterFile) {
            m_lblIpAdapterFile->setText(QFileInfo(file).fileName());
            m_lblIpAdapterFile->setToolTip(file);
        }
    }
}

void AiAssistantDockWidget::onControlNetSourceChanged(int index) {
    Q_UNUSED(index);
    bool isFile = (m_comboControlNetSource && m_comboControlNetSource->currentData().toInt() == static_cast<int>(AiImageSource::File));
    if (m_widgetControlNetFile) {
        m_widgetControlNetFile->setVisible(isFile);
    }
    if (isFile && m_controlNetFilePath.isEmpty()) {
        onBrowseControlNetFile();
    }
}

void AiAssistantDockWidget::onIpAdapterSourceChanged(int index) {
    Q_UNUSED(index);
    bool isFile = (m_comboIpAdapterSource && m_comboIpAdapterSource->currentData().toInt() == static_cast<int>(AiImageSource::File));
    if (m_widgetIpAdapterFile) {
        m_widgetIpAdapterFile->setVisible(isFile);
    }
    if (isFile && m_ipAdapterFilePath.isEmpty()) {
        onBrowseIpAdapterFile();
    }
}

QImage AiAssistantDockWidget::getImageFromSource(int sourceIndex, const QString& filePath, QString* outError) {
    switch (static_cast<AiImageSource>(sourceIndex)) {
    case AiImageSource::Canvas: {
        if (!m_document) {
            if (outError) *outError = tr("キャンバスが開かれていません。");
            return QImage();
        }
        return m_document->compositeBuffer().toQImage();
    }
    case AiImageSource::ActiveLayer: {
        if (!m_document) {
            if (outError) *outError = tr("キャンバスが開かれていません。");
            return QImage();
        }
        auto activeLayer = m_document->activeLayer();
        if (activeLayer) {
            QImage img = activeLayer->tileManager().toQImage();
            if (!img.isNull()) return img;
        }
        return m_document->compositeBuffer().toQImage();
    }
    case AiImageSource::Clipboard: {
        QClipboard* cb = QApplication::clipboard();
        if (cb) {
            QImage img = cb->image();
            if (!img.isNull()) return img;
            QPixmap pix = cb->pixmap();
            if (!pix.isNull()) return pix.toImage();
        }
        if (outError) *outError = tr("クリップボードに画像が見つかりません。画像をコピーしてください。");
        return QImage();
    }
    case AiImageSource::ReferenceWindow: {
        ReferenceImageWindow* refWin = nullptr;
        if (m_mainWindow) {
            refWin = m_mainWindow->referenceImageWindow();
        } else {
            MainWindow* mw = qobject_cast<MainWindow*>(parent());
            if (!mw) mw = qobject_cast<MainWindow*>(window());
            if (mw) refWin = mw->referenceImageWindow();
        }
        if (refWin) {
            QImage refImg = refWin->currentImage();
            if (!refImg.isNull()) return refImg;
        }
        if (outError) *outError = tr("参考画像ウィンドウに画像が開かれていません。「表示」→「参考画像ウィンドウ」で画像を開いてください。");
        return QImage();
    }
    case AiImageSource::File: {
        if (filePath.isEmpty() || !QFile::exists(filePath)) {
            if (outError) *outError = tr("画像ファイルが指定されていないか、存在しません。「参照...」からファイルを選択してください。");
            return QImage();
        }
        QImage fileImg(filePath);
        if (fileImg.isNull()) {
            if (outError) *outError = tr("画像ファイルを読み込めませんでした: %1").arg(filePath);
            return QImage();
        }
        return fileImg;
    }
    }
    return QImage();
}

QImage AiAssistantDockWidget::getControlNetImage(QString* outError) {
    int src = m_comboControlNetSource ? m_comboControlNetSource->currentData().toInt() : static_cast<int>(AiImageSource::Canvas);
    return getImageFromSource(src, m_controlNetFilePath, outError);
}

QImage AiAssistantDockWidget::getIpAdapterImage(QString* outError) {
    int src = m_comboIpAdapterSource ? m_comboIpAdapterSource->currentData().toInt() : static_cast<int>(AiImageSource::Canvas);
    return getImageFromSource(src, m_ipAdapterFilePath, outError);
}

void AiAssistantDockWidget::onModelsFetched(const QStringList& models) {
    for (const auto& m : models) {
        if (m_comboModel->findText(m) == -1) {
            m_comboModel->addItem(m);
        }
    }
    m_lblStatus->setText(tr("モデル一覧を更新しました (%1 件)").arg(m_comboModel->count()));
}

void AiAssistantDockWidget::onModelsFetchFailed(const QString& errorMessage) {
    if (m_comboModel->count() > 0) {
        m_lblStatus->setText(tr("サーバー未接続 (ローカルフォルダの %1 件を表示中)").arg(m_comboModel->count()));
    } else {
        m_lblStatus->setText(errorMessage);
    }
}

void AiAssistantDockWidget::onGenerationProgress(int percent, const QString& statusMessage) {
    m_progressBar->setValue(percent);
    m_lblStatus->setText(statusMessage);
}

void AiAssistantDockWidget::onGenerationFinished(const QList<QImage>& results) {
    m_btnGenerate->setEnabled(true);
    m_btnCancel->setEnabled(false);
    m_currentResults = results;
    m_selectedIndex = -1;

    m_listThumbnails->clear();
    for (int i = 0; i < results.size(); ++i) {
        QPixmap pix = QPixmap::fromImage(results[i]).scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        auto item = new QListWidgetItem(QIcon(pix), QString("候補 %1").arg(i + 1));
        m_listThumbnails->addItem(item);
    }

    if (!results.isEmpty()) {
        m_listThumbnails->setCurrentRow(0);
    }
}

void AiAssistantDockWidget::onGenerationFailed(const QString& errorMessage) {
    m_btnGenerate->setEnabled(true);
    m_btnCancel->setEnabled(false);
    m_lblStatus->setText(errorMessage);
    QMessageBox::warning(this, tr("AI生成エラー"), errorMessage);
}

void AiAssistantDockWidget::onThumbnailSelected(int row) {
    m_selectedIndex = row;
    bool hasSelection = (row >= 0 && row < m_currentResults.size());
    m_btnApplyNewLayer->setEnabled(hasSelection);
    m_btnApplyOverwrite->setEnabled(hasSelection);
}

void AiAssistantDockWidget::onApplyNewLayerClicked() {
    if (m_selectedIndex >= 0 && m_selectedIndex < m_currentResults.size()) {
        QImage img = m_currentResults[m_selectedIndex];
        if (m_document) {
            QSize targetSize(m_document->width(), m_document->height());
            if (img.size() != targetSize) {
                QString method = m_comboUpscaleMethod ? m_comboUpscaleMethod->currentText() : "bicubic";
                img = AiService::upscaleImage(img, targetSize, method);
            }
        }
        emit applyAsNewLayer(img, m_lastGeneratedJson);
    }
}

void AiAssistantDockWidget::onApplyOverwriteClicked() {
    if (m_selectedIndex >= 0 && m_selectedIndex < m_currentResults.size()) {
        QImage img = m_currentResults[m_selectedIndex];
        if (m_document) {
            QSize targetSize(m_document->width(), m_document->height());
            if (m_document->hasSelection()) {
                QRect bounds = m_document->selection().boundingRect().toAlignedRect();
                if (bounds.isValid() && !bounds.isEmpty()) {
                    targetSize = bounds.size();
                }
            }
            if (img.size() != targetSize) {
                QString method = m_comboUpscaleMethod ? m_comboUpscaleMethod->currentText() : "bicubic";
                img = AiService::upscaleImage(img, targetSize, method);
            }
        }
        emit applyOverwriteSelection(img, m_lastGeneratedJson);
    }
}

void AiAssistantDockWidget::onPromptAssistClicked() {
    LlmPromptDialog dlg(this);
    dlg.setCurrentPrompt(m_editPrompt->toPlainText().trimmed(),
                         m_editNegativePrompt->toPlainText().trimmed());

    if (dlg.exec() == QDialog::Accepted) {
        QString newPrompt = dlg.generatedPrompt();
        QString newNeg = dlg.generatedNegativePrompt();

        if (dlg.shouldAppend()) {
            QString cur = m_editPrompt->toPlainText().trimmed();
            if (!cur.isEmpty() && !newPrompt.isEmpty()) {
                m_editPrompt->setPlainText(cur + ", " + newPrompt);
            } else if (!newPrompt.isEmpty()) {
                m_editPrompt->setPlainText(newPrompt);
            }
        } else {
            if (!newPrompt.isEmpty()) {
                m_editPrompt->setPlainText(newPrompt);
            }
        }

        if (!newNeg.isEmpty()) {
            m_editNegativePrompt->setPlainText(newNeg);
        }
    }
}

void AiAssistantDockWidget::saveSettings() {
    QSettings settings("KABURAGI", "PaintSoft KABURAGI");
    settings.beginGroup("AiAssistant");

    int backendInt = 0;
    if (m_radioComfyUi && m_radioComfyUi->isChecked()) backendInt = 1;
    else if (m_radioWebUi && m_radioWebUi->isChecked()) backendInt = 0;
    else if (m_radioEmbedded && m_radioEmbedded->isChecked()) backendInt = 2;
    else if (m_radioCloud && m_radioCloud->isChecked()) backendInt = 3;
    settings.setValue("backend", backendInt);

    if (m_editServerUrl) settings.setValue("serverUrl", m_editServerUrl->text());
    if (m_editApiKey) settings.setValue("apiKey", m_editApiKey->text());
    if (m_comboModel) settings.setValue("modelName", m_comboModel->currentText());
    if (m_comboDevice) settings.setValue("device", m_comboDevice->currentData().toString());
    if (m_comboMode) settings.setValue("mode", m_comboMode->currentData().toInt());

    if (m_spinBatchCount) settings.setValue("batchCount", m_spinBatchCount->value());
    if (m_spinSteps) settings.setValue("steps", m_spinSteps->value());
    if (m_spinCfg) settings.setValue("cfgScale", m_spinCfg->value());
    if (m_spinDenoising) settings.setValue("denoisingStrength", m_spinDenoising->value());
    if (m_spinSeed) settings.setValue("seed", m_spinSeed->value());

    if (m_comboResolution) settings.setValue("resolutionIndex", m_comboResolution->currentIndex());
    if (m_spinWidth) settings.setValue("width", m_spinWidth->value());
    if (m_spinHeight) settings.setValue("height", m_spinHeight->value());
    if (m_comboUpscaleMethod) settings.setValue("upscaleMethod", m_comboUpscaleMethod->currentText());

    if (m_chkControlNet) settings.setValue("enableControlNet", m_chkControlNet->isChecked());
    if (m_comboControlNetType) settings.setValue("controlNetType", m_comboControlNetType->currentData().toInt());
    if (m_spinControlNetWeight) settings.setValue("controlNetWeight", m_spinControlNetWeight->value());
    if (m_comboControlNetSource) settings.setValue("controlNetSource", m_comboControlNetSource->currentData().toInt());
    settings.setValue("controlNetFilePath", m_controlNetFilePath);

    if (m_chkIpAdapter) settings.setValue("enableIpAdapter", m_chkIpAdapter->isChecked());
    if (m_comboIpAdapterType) settings.setValue("ipAdapterType", m_comboIpAdapterType->currentData().toString());
    if (m_spinIpAdapterWeight) settings.setValue("ipAdapterWeight", m_spinIpAdapterWeight->value());
    if (m_comboIpAdapterSource) settings.setValue("ipAdapterSource", m_comboIpAdapterSource->currentData().toInt());
    settings.setValue("ipAdapterFilePath", m_ipAdapterFilePath);

    // ※ 注意: プロンプトとネガティブプロンプトはユーザーの指定により記憶から除外
    settings.endGroup();
}

void AiAssistantDockWidget::loadSettings() {
    QSettings settings("KABURAGI", "PaintSoft KABURAGI");
    settings.beginGroup("AiAssistant");

    if (settings.contains("backend")) {
        int backendInt = settings.value("backend", 1).toInt();
        if (backendInt == 0 && m_radioWebUi) m_radioWebUi->setChecked(true);
        else if (backendInt == 1 && m_radioComfyUi) m_radioComfyUi->setChecked(true);
        else if (backendInt == 2 && m_radioEmbedded) m_radioEmbedded->setChecked(true);
        else if (backendInt == 3 && m_radioCloud) m_radioCloud->setChecked(true);
    }

    if (m_editServerUrl && settings.contains("serverUrl")) {
        m_editServerUrl->setText(settings.value("serverUrl").toString());
    }
    if (m_editApiKey && settings.contains("apiKey")) {
        m_editApiKey->setText(settings.value("apiKey").toString());
    }
    if (m_comboModel && settings.contains("modelName")) {
        QString model = settings.value("modelName").toString();
        int idx = m_comboModel->findText(model);
        if (idx >= 0) {
            m_comboModel->setCurrentIndex(idx);
        } else if (!model.isEmpty()) {
            m_comboModel->setEditText(model);
        }
    }
    if (m_comboDevice && settings.contains("device")) {
        QString dev = settings.value("device").toString();
        int idx = m_comboDevice->findData(dev);
        if (idx >= 0) m_comboDevice->setCurrentIndex(idx);
    }
    if (m_comboMode && settings.contains("mode")) {
        int modeVal = settings.value("mode").toInt();
        int idx = m_comboMode->findData(modeVal);
        if (idx >= 0) m_comboMode->setCurrentIndex(idx);
    }

    if (m_spinBatchCount && settings.contains("batchCount")) m_spinBatchCount->setValue(settings.value("batchCount").toInt());
    if (m_spinSteps && settings.contains("steps")) m_spinSteps->setValue(settings.value("steps").toInt());
    if (m_spinCfg && settings.contains("cfgScale")) m_spinCfg->setValue(settings.value("cfgScale").toDouble());
    if (m_spinDenoising && settings.contains("denoisingStrength")) m_spinDenoising->setValue(settings.value("denoisingStrength").toDouble());
    if (m_spinSeed && settings.contains("seed")) m_spinSeed->setValue(settings.value("seed").toInt());

    if (m_comboResolution && settings.contains("resolutionIndex")) {
        m_comboResolution->setCurrentIndex(settings.value("resolutionIndex").toInt());
    }
    if (m_spinWidth && settings.contains("width")) m_spinWidth->setValue(settings.value("width").toInt());
    if (m_spinHeight && settings.contains("height")) m_spinHeight->setValue(settings.value("height").toInt());
    if (m_comboUpscaleMethod && settings.contains("upscaleMethod")) {
        QString up = settings.value("upscaleMethod").toString();
        int idx = m_comboUpscaleMethod->findText(up);
        if (idx >= 0) m_comboUpscaleMethod->setCurrentIndex(idx);
    }

    if (m_chkControlNet && settings.contains("enableControlNet")) {
        m_chkControlNet->setChecked(settings.value("enableControlNet").toBool());
    }
    if (m_comboControlNetType && settings.contains("controlNetType")) {
        int cnType = settings.value("controlNetType").toInt();
        int idx = m_comboControlNetType->findData(cnType);
        if (idx >= 0) m_comboControlNetType->setCurrentIndex(idx);
    }
    if (m_spinControlNetWeight && settings.contains("controlNetWeight")) {
        m_spinControlNetWeight->setValue(settings.value("controlNetWeight").toDouble());
    }
    if (m_comboControlNetSource && settings.contains("controlNetSource")) {
        int srcVal = settings.value("controlNetSource").toInt();
        int idx = m_comboControlNetSource->findData(srcVal);
        if (idx >= 0) m_comboControlNetSource->setCurrentIndex(idx);
    }
    if (settings.contains("controlNetFilePath")) {
        m_controlNetFilePath = settings.value("controlNetFilePath").toString();
        if (m_lblControlNetFile && !m_controlNetFilePath.isEmpty()) {
            m_lblControlNetFile->setText(QFileInfo(m_controlNetFilePath).fileName());
            m_lblControlNetFile->setToolTip(m_controlNetFilePath);
        }
    }
    if (m_comboControlNetSource && m_widgetControlNetFile) {
        m_widgetControlNetFile->setVisible(m_comboControlNetSource->currentData().toInt() == static_cast<int>(AiImageSource::File));
    }

    if (m_chkIpAdapter && settings.contains("enableIpAdapter")) {
        m_chkIpAdapter->setChecked(settings.value("enableIpAdapter").toBool());
    }
    if (m_comboIpAdapterType && settings.contains("ipAdapterType")) {
        QString ipType = settings.value("ipAdapterType").toString();
        int idx = m_comboIpAdapterType->findData(ipType);
        if (idx >= 0) m_comboIpAdapterType->setCurrentIndex(idx);
    }
    if (m_spinIpAdapterWeight && settings.contains("ipAdapterWeight")) {
        m_spinIpAdapterWeight->setValue(settings.value("ipAdapterWeight").toDouble());
    }
    if (m_comboIpAdapterSource && settings.contains("ipAdapterSource")) {
        int srcVal = settings.value("ipAdapterSource").toInt();
        int idx = m_comboIpAdapterSource->findData(srcVal);
        if (idx >= 0) m_comboIpAdapterSource->setCurrentIndex(idx);
    }
    if (settings.contains("ipAdapterFilePath")) {
        m_ipAdapterFilePath = settings.value("ipAdapterFilePath").toString();
        if (m_lblIpAdapterFile && !m_ipAdapterFilePath.isEmpty()) {
            m_lblIpAdapterFile->setText(QFileInfo(m_ipAdapterFilePath).fileName());
            m_lblIpAdapterFile->setToolTip(m_ipAdapterFilePath);
        }
    }
    if (m_comboIpAdapterSource && m_widgetIpAdapterFile) {
        m_widgetIpAdapterFile->setVisible(m_comboIpAdapterSource->currentData().toInt() == static_cast<int>(AiImageSource::File));
    }

    settings.endGroup();
}

void AiAssistantDockWidget::loadParametersFromLayer(const std::shared_ptr<Layer>& layer) {
    if (!layer || !layer->hasAiParameters()) return;

    AiGenerateParams params = AiService::jsonToParams(layer->aiParameters());

    // バックエンド
    if (params.backend == ApiBackend::LocalWebUI && m_radioWebUi) m_radioWebUi->setChecked(true);
    else if (params.backend == ApiBackend::LocalComfyUI && m_radioComfyUi) m_radioComfyUi->setChecked(true);
    else if (params.backend == ApiBackend::EmbeddedEngine && m_radioEmbedded) m_radioEmbedded->setChecked(true);
    else if (params.backend == ApiBackend::CloudOpenAI && m_radioCloud) m_radioCloud->setChecked(true);

    if (m_editServerUrl && !params.serverUrl.isEmpty()) m_editServerUrl->setText(params.serverUrl);
    if (m_editApiKey && !params.apiKey.isEmpty()) m_editApiKey->setText(params.apiKey);
    if (m_comboModel && !params.modelName.isEmpty()) {
        int idx = m_comboModel->findText(params.modelName);
        if (idx >= 0) m_comboModel->setCurrentIndex(idx);
        else m_comboModel->setEditText(params.modelName);
    }

    // 制作モード
    if (m_comboMode) {
        int modeIdx = m_comboMode->findData(static_cast<int>(params.mode));
        if (modeIdx >= 0) m_comboMode->setCurrentIndex(modeIdx);
    }

    // プロンプト・ネガティブプロンプト
    if (m_editPrompt) m_editPrompt->setPlainText(params.prompt);
    if (m_editNegativePrompt) m_editNegativePrompt->setPlainText(params.negativePrompt);

    // 生成パラメータ
    if (m_spinBatchCount) m_spinBatchCount->setValue(std::clamp(params.batchCount, 1, 8));
    if (m_spinSteps) m_spinSteps->setValue(std::clamp(params.steps, 1, 150));
    if (m_spinCfg) m_spinCfg->setValue(params.cfgScale);
    if (m_spinDenoising) m_spinDenoising->setValue(params.denoisingStrength);
    if (m_spinSeed) m_spinSeed->setValue(static_cast<int>(params.seed));

    // 解像度
    if (m_spinWidth) m_spinWidth->setValue(params.width);
    if (m_spinHeight) m_spinHeight->setValue(params.height);

    // ControlNet
    if (m_chkControlNet) m_chkControlNet->setChecked(params.enableControlNet);
    if (m_comboControlNetType) {
        int cnIdx = m_comboControlNetType->findData(static_cast<int>(params.controlNetType));
        if (cnIdx >= 0) m_comboControlNetType->setCurrentIndex(cnIdx);
    }
    if (m_spinControlNetWeight) m_spinControlNetWeight->setValue(params.controlNetWeight);

    // IP-Adapter
    if (m_chkIpAdapter) m_chkIpAdapter->setChecked(params.enableIpAdapter);
    if (m_comboIpAdapterType) {
        int ipIdx = m_comboIpAdapterType->findData(params.ipAdapterType);
        if (ipIdx >= 0) m_comboIpAdapterType->setCurrentIndex(ipIdx);
    }
    if (m_spinIpAdapterWeight) m_spinIpAdapterWeight->setValue(params.ipAdapterWeight);

    // デバイス & アップスケール
    if (m_comboDevice && !params.computeDevice.isEmpty()) {
        int devIdx = m_comboDevice->findData(params.computeDevice);
        if (devIdx >= 0) m_comboDevice->setCurrentIndex(devIdx);
    }
    if (m_comboUpscaleMethod && !params.upscaleMethod.isEmpty()) {
        int upIdx = m_comboUpscaleMethod->findText(params.upscaleMethod);
        if (upIdx >= 0) m_comboUpscaleMethod->setCurrentIndex(upIdx);
    }
}

void AiAssistantDockWidget::onLoadFromActiveLayerClicked() {
    if (!m_document) return;
    auto activeL = m_document->activeLayer();
    if (!activeL || !activeL->hasAiParameters()) {
        QMessageBox::information(this, tr("AI Parameters"), tr("選択中のレイヤーにはAI生成パラメータが保存されていません。"));
        return;
    }
    loadParametersFromLayer(activeL);
    if (m_lblStatus) {
        m_lblStatus->setText(tr("レイヤー「%1」から生成パラメータを読み込みました。").arg(activeL->name()));
    }
}

void AiAssistantDockWidget::resetToDefaults() {
    // 接続設定
    if (m_radioComfyUi) m_radioComfyUi->setChecked(true);
    if (m_editServerUrl) m_editServerUrl->setText("http://127.0.0.1:8188");
    if (m_editApiKey) m_editApiKey->clear();
    if (m_comboDevice) m_comboDevice->setCurrentIndex(0);
    if (m_comboModel && m_comboModel->count() > 0) m_comboModel->setCurrentIndex(0);

    // 制作モード
    if (m_comboMode) m_comboMode->setCurrentIndex(0); // RoughToClean

    // プロンプト
    if (m_editPrompt) m_editPrompt->clear();
    if (m_editNegativePrompt) m_editNegativePrompt->setPlainText("worst quality, low quality, bad anatomy, deformed");

    // 解像度 & アップスケール
    if (m_comboResolution) m_comboResolution->setCurrentIndex(0); // 512 x 512
    if (m_spinWidth) m_spinWidth->setValue(512);
    if (m_spinHeight) m_spinHeight->setValue(512);
    if (m_comboUpscaleMethod) m_comboUpscaleMethod->setCurrentIndex(0);

    // 生成パラメータ
    if (m_spinBatchCount) m_spinBatchCount->setValue(1);
    if (m_spinSteps) m_spinSteps->setValue(20);
    if (m_spinCfg) m_spinCfg->setValue(7.0);
    if (m_spinDenoising) m_spinDenoising->setValue(0.55);
    if (m_spinSeed) m_spinSeed->setValue(-1);

    // ControlNet
    if (m_chkControlNet) m_chkControlNet->setChecked(false);
    if (m_comboControlNetType) m_comboControlNetType->setCurrentIndex(0);
    if (m_spinControlNetWeight) m_spinControlNetWeight->setValue(1.0);
    if (m_comboControlNetSource) m_comboControlNetSource->setCurrentIndex(0);
    m_controlNetFilePath.clear();
    if (m_lblControlNetFile) m_lblControlNetFile->setText(tr("(未選択)"));
    if (m_widgetControlNetFile) m_widgetControlNetFile->setVisible(false);

    // IP-Adapter
    if (m_chkIpAdapter) m_chkIpAdapter->setChecked(false);
    if (m_comboIpAdapterType) m_comboIpAdapterType->setCurrentIndex(0);
    if (m_spinIpAdapterWeight) m_spinIpAdapterWeight->setValue(0.8);
    if (m_comboIpAdapterSource) m_comboIpAdapterSource->setCurrentIndex(0);
    m_ipAdapterFilePath.clear();
    if (m_lblIpAdapterFile) m_lblIpAdapterFile->setText(tr("(未選択)"));
    if (m_widgetIpAdapterFile) m_widgetIpAdapterFile->setVisible(false);

    saveSettings();

    if (m_lblStatus) {
        m_lblStatus->setText(tr("設定をデフォルト値にリセットしました。"));
    }
}

void AiAssistantDockWidget::onResetDefaultsClicked() {
    auto reply = QMessageBox::question(
        this,
        tr("設定のリセット確認"),
        tr("AIアシスタントのすべての設定をデフォルト値に戻しますか？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        resetToDefaults();
    }
}

} // namespace Kaburagi
