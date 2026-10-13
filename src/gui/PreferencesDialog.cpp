#include "PreferencesDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QSettings>
#include <QTabWidget>
#include <QMessageBox>
#include <QLocale>
#include "LlmPromptService.hpp"
#include "DisplayFilterManager.hpp"

namespace Kaburagi {

PreferencesDialog::PreferencesDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("環境設定");
    setModal(true);
    resize(560, 460);

    auto mainLayout = new QVBoxLayout(this);
    auto tabWidget = new QTabWidget(this);

    // ==========================================
    // タブ1: 一般
    // ==========================================
    auto tabGeneral = new QWidget(this);
    auto layGeneral = new QVBoxLayout(tabGeneral);

    // 1. 自動バックアップ設定
    auto backupGroup = new QGroupBox("自動バックアップ設定", tabGeneral);
    auto backupForm = new QFormLayout(backupGroup);

    m_autoBackupCheck = new QCheckBox("自動バックアップを有効にする", this);
    backupForm->addRow(m_autoBackupCheck);

    m_backupIntervalSpin = new QSpinBox(this);
    m_backupIntervalSpin->setRange(1, 120);
    m_backupIntervalSpin->setSuffix(" 分");
    backupForm->addRow("保存間隔:", m_backupIntervalSpin);

    layGeneral->addWidget(backupGroup);

    connect(m_autoBackupCheck, &QCheckBox::toggled, m_backupIntervalSpin, &QSpinBox::setEnabled);

    // 2. レイヤービュー設定
    auto layerViewGroup = new QGroupBox("レイヤービュー設定", tabGeneral);
    auto layerViewForm = new QFormLayout(layerViewGroup);

    m_layerScrollPosCombo = new QComboBox(this);
    m_layerScrollPosCombo->addItem("右側 (標準)", static_cast<int>(ScrollBarPosition::Right));
    m_layerScrollPosCombo->addItem("左側", static_cast<int>(ScrollBarPosition::Left));
    layerViewForm->addRow("スクロールバー位置:", m_layerScrollPosCombo);

    layGeneral->addWidget(layerViewGroup);

    // 3. 言語設定 (Language Settings)
    auto langGroup = new QGroupBox(tr("Language Settings"), tabGeneral);
    auto langForm = new QFormLayout(langGroup);

    m_comboLanguage = new QComboBox(this);
    m_comboLanguage->addItem(tr("System Default"), "system");
    m_comboLanguage->addItem(tr("Japanese (日本語)"), "ja");
    m_comboLanguage->addItem(tr("English"), "en");
    langForm->addRow(tr("Language:"), m_comboLanguage);

    layGeneral->addWidget(langGroup);

    layGeneral->addStretch();
    tabWidget->addTab(tabGeneral, tr("General"));

    // ==========================================
    // タブ2: カラーマネジメント (ICCプロファイル)
    // ==========================================
    auto tabColor = new QWidget(this);
    auto layColor = new QVBoxLayout(tabColor);

    auto profiles = DisplayFilterManager::scanSystemIccProfiles();

    // 1. モニタープロファイル設定
    auto monitorGroup = new QGroupBox("モニターのプロファイル設定 (指定なし可)", tabColor);
    auto monitorForm = new QFormLayout(monitorGroup);

    m_comboMonitorProfile = new QComboBox(this);
    m_comboMonitorProfile->addItem("（未指定 / sRGB標準）", "");
    for (const auto& prof : profiles) {
        QString itemText = QString("%1 [%2]").arg(prof.name, prof.colorSpace);
        m_comboMonitorProfile->addItem(itemText, prof.path);
    }

    auto layMonBrowse = new QHBoxLayout();
    m_editMonitorProfilePath = new QLineEdit(this);
    m_editMonitorProfilePath->setPlaceholderText("モニタープロファイルファイルのパス (.icc / .icm)");
    m_btnBrowseMonitorProfile = new QPushButton("参照...", this);
    layMonBrowse->addWidget(m_editMonitorProfilePath);
    layMonBrowse->addWidget(m_btnBrowseMonitorProfile);

    m_lblMonitorProfileInfo = new QLabel(this);
    m_lblMonitorProfileInfo->setWordWrap(true);
    m_lblMonitorProfileInfo->setStyleSheet("color: #333; background: #f8f8f8; padding: 6px; border: 1px solid #ccc; border-radius: 4px; font-size: 11px;");

    monitorForm->addRow("モニタープロファイル:", m_comboMonitorProfile);
    monitorForm->addRow("ファイルから選択:", layMonBrowse);
    monitorForm->addRow("プロファイル情報:", m_lblMonitorProfileInfo);

    layColor->addWidget(monitorGroup);

    connect(m_comboMonitorProfile, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (!m_initialized) return;
        QString path = m_comboMonitorProfile->itemData(idx).toString();
        m_editMonitorProfilePath->setText(path);
        updateMonitorProfileDetails(path);
    });

    connect(m_editMonitorProfilePath, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (!m_initialized) return;
        int idx = m_comboMonitorProfile->findData(text.trimmed());
        if (idx >= 0 && m_comboMonitorProfile->currentIndex() != idx) {
            QSignalBlocker blocker(m_comboMonitorProfile);
            m_comboMonitorProfile->setCurrentIndex(idx);
        } else if (idx < 0 && m_comboMonitorProfile->currentIndex() != 0) {
            QSignalBlocker blocker(m_comboMonitorProfile);
            m_comboMonitorProfile->setCurrentIndex(0);
        }
        updateMonitorProfileDetails(text.trimmed());
    });

    connect(m_btnBrowseMonitorProfile, &QPushButton::clicked, this, [this]() {
        QString dir = QFileInfo(m_editMonitorProfilePath->text()).absolutePath();
        if (dir.isEmpty() || !QDir(dir).exists()) {
            dir = "C:/Windows/System32/spool/drivers/color";
        }
        QString file = QFileDialog::getOpenFileName(this, "モニタープロファイルを選択", dir, "ICC/ICM プロファイル (*.icc *.icm);;すべてのファイル (*.*)");
        if (!file.isEmpty()) {
            m_editMonitorProfilePath->setText(file);
            int idx = m_comboMonitorProfile->findData(file);
            if (idx >= 0) {
                m_comboMonitorProfile->setCurrentIndex(idx);
            } else {
                m_comboMonitorProfile->setCurrentIndex(0);
            }
            updateMonitorProfileDetails(file);
        }
    });

    // 2. 色校正用 ICC プロファイル設定
    auto iccGroup = new QGroupBox("色校正用 ICC プロファイル設定 (指定なし可)", tabColor);
    auto iccForm = new QFormLayout(iccGroup);

    m_comboIccProfile = new QComboBox(this);
    m_comboIccProfile->addItem("（未指定）", "");

    for (const auto& prof : profiles) {
        QString itemText = QString("%1 [%2]").arg(prof.name, prof.colorSpace);
        m_comboIccProfile->addItem(itemText, prof.path);
    }

    auto layIccBrowse = new QHBoxLayout();
    m_editIccProfilePath = new QLineEdit(this);
    m_editIccProfilePath->setPlaceholderText("ICC/ICM プロファイルファイルのパス");
    m_btnBrowseIccProfile = new QPushButton("参照...", this);
    layIccBrowse->addWidget(m_editIccProfilePath);
    layIccBrowse->addWidget(m_btnBrowseIccProfile);

    m_lblIccProfileInfo = new QLabel(this);
    m_lblIccProfileInfo->setWordWrap(true);
    m_lblIccProfileInfo->setStyleSheet("color: #333; background: #f8f8f8; padding: 6px; border: 1px solid #ccc; border-radius: 4px; font-size: 11px;");

    iccForm->addRow("プロファイル一覧:", m_comboIccProfile);
    iccForm->addRow("ファイルから選択:", layIccBrowse);
    iccForm->addRow("プロファイル詳細:", m_lblIccProfileInfo);

    auto lblHelp = new QLabel("※ メニューの [ビュー] → [表示フィルター] → [ICCプロファイル] を有効にした際に、上記の設定に基づきキャンバスやカラーパレット等の画面表示へ適用されます。", this);
    lblHelp->setWordWrap(true);
    lblHelp->setStyleSheet("color: #666; font-size: 11px; margin-top: 4px;");
    iccForm->addRow("", lblHelp);

    layColor->addWidget(iccGroup);
    layColor->addStretch();
    tabWidget->addTab(tabColor, "カラーマネジメント");

    connect(m_comboIccProfile, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (!m_initialized) return;
        QString path = m_comboIccProfile->itemData(idx).toString();
        m_editIccProfilePath->setText(path);
        updateIccProfileDetails(path);
    });

    connect(m_editIccProfilePath, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (!m_initialized) return;
        int idx = m_comboIccProfile->findData(text.trimmed());
        if (idx >= 0 && m_comboIccProfile->currentIndex() != idx) {
            QSignalBlocker blocker(m_comboIccProfile);
            m_comboIccProfile->setCurrentIndex(idx);
        } else if (idx < 0 && m_comboIccProfile->currentIndex() != 0) {
            QSignalBlocker blocker(m_comboIccProfile);
            m_comboIccProfile->setCurrentIndex(0);
        }
        updateIccProfileDetails(text.trimmed());
    });

    connect(m_btnBrowseIccProfile, &QPushButton::clicked, this, [this]() {
        QString dir = QFileInfo(m_editIccProfilePath->text()).absolutePath();
        if (dir.isEmpty() || !QDir(dir).exists()) {
            dir = "C:/Windows/System32/spool/drivers/color";
        }
        QString file = QFileDialog::getOpenFileName(this, "ICCプロファイルを選択", dir, "ICC/ICM プロファイル (*.icc *.icm);;すべてのファイル (*.*)");
        if (!file.isEmpty()) {
            m_editIccProfilePath->setText(file);
            int idx = m_comboIccProfile->findData(file);
            if (idx >= 0) {
                m_comboIccProfile->setCurrentIndex(idx);
            } else {
                m_comboIccProfile->setCurrentIndex(0);
            }
            updateIccProfileDetails(file);
        }
    });

    // ==========================================
    // タブ3: ローカル画像生成AI
    // ==========================================
    auto tabAi = new QWidget(this);
    auto layAi = new QVBoxLayout(tabAi);

    auto aiGroup = new QGroupBox("ローカル画像生成AI設定", tabAi);
    auto aiForm = new QFormLayout(aiGroup);

    // 生成AIモデル保存フォルダ
    auto layModelDir = new QHBoxLayout();
    m_editAiModelDir = new QLineEdit(this);
    m_editAiModelDir->setPlaceholderText("例: C:/stable-diffusion-webui/models/Stable-diffusion");
    m_btnBrowseAiModelDir = new QPushButton("参照...", this);
    layModelDir->addWidget(m_editAiModelDir);
    layModelDir->addWidget(m_btnBrowseAiModelDir);
    aiForm->addRow("生成AIモデル保存フォルダ:", layModelDir);

    connect(m_btnBrowseAiModelDir, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "生成AIモデル保存フォルダを選択", m_editAiModelDir->text());
        if (!dir.isEmpty()) {
            m_editAiModelDir->setText(dir);
        }
    });

    // ControlNet保存フォルダ
    auto layControlNetDir = new QHBoxLayout();
    m_editAiControlNetDir = new QLineEdit(this);
    m_editAiControlNetDir->setPlaceholderText("例: C:/stable-diffusion-webui/models/ControlNet");
    m_btnBrowseAiControlNetDir = new QPushButton("参照...", this);
    layControlNetDir->addWidget(m_editAiControlNetDir);
    layControlNetDir->addWidget(m_btnBrowseAiControlNetDir);
    aiForm->addRow("ControlNet保存フォルダ:", layControlNetDir);

    connect(m_btnBrowseAiControlNetDir, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "ControlNet保存フォルダを選択", m_editAiControlNetDir->text());
        if (!dir.isEmpty()) {
            m_editAiControlNetDir->setText(dir);
        }
    });

    // サーバー自動起動チェック
    m_chkAutoLaunchServer = new QCheckBox("サーバー未起動(接続拒否)時に自動起動する", this);
    m_chkAutoLaunchServer->setChecked(true);
    aiForm->addRow("", m_chkAutoLaunchServer);

    // ComfyUI 起動バッチ/実行ファイル
    auto layComfyPath = new QHBoxLayout();
    m_editComfyUiPath = new QLineEdit(this);
    m_editComfyUiPath->setPlaceholderText("例: E:/ComfyUI/run_nvidia_gpu.bat");
    m_btnBrowseComfyUiPath = new QPushButton("参照...", this);
    layComfyPath->addWidget(m_editComfyUiPath);
    layComfyPath->addWidget(m_btnBrowseComfyUiPath);
    aiForm->addRow("ComfyUI 起動ファイル:", layComfyPath);

    connect(m_btnBrowseComfyUiPath, &QPushButton::clicked, this, [this]() {
        QString file = QFileDialog::getOpenFileName(this, "ComfyUI 起動ファイルを選択", m_editComfyUiPath->text(), "実行ファイル (*.bat *.cmd *.exe);;すべてのファイル (*.*)");
        if (!file.isEmpty()) {
            m_editComfyUiPath->setText(file);
        }
    });

    // Web UI 起動バッチ/実行ファイル
    auto layWebUiPath = new QHBoxLayout();
    m_editWebUiPath = new QLineEdit(this);
    m_editWebUiPath->setPlaceholderText("例: C:/stable-diffusion-webui/webui-user.bat");
    m_btnBrowseWebUiPath = new QPushButton("参照...", this);
    layWebUiPath->addWidget(m_editWebUiPath);
    layWebUiPath->addWidget(m_btnBrowseWebUiPath);
    aiForm->addRow("Web UI 起動ファイル:", layWebUiPath);

    connect(m_btnBrowseWebUiPath, &QPushButton::clicked, this, [this]() {
        QString file = QFileDialog::getOpenFileName(this, "Web UI 起動ファイルを選択", m_editWebUiPath->text(), "実行ファイル (*.bat *.cmd *.exe);;すべてのファイル (*.*)");
        if (!file.isEmpty()) {
            m_editWebUiPath->setText(file);
        }
    });

    // 組み込み推論エンジン (sd.exe等)
    auto layEmbedded = new QHBoxLayout();
    m_editEmbeddedEnginePath = new QLineEdit(this);
    m_editEmbeddedEnginePath->setPlaceholderText("例: tools/sd.exe (stable-diffusion.cpp 等)");
    m_btnBrowseEmbeddedEnginePath = new QPushButton("参照...", this);
    layEmbedded->addWidget(m_editEmbeddedEnginePath);
    layEmbedded->addWidget(m_btnBrowseEmbeddedEnginePath);
    aiForm->addRow("組み込みエンジン(sd.exe):", layEmbedded);

    connect(m_btnBrowseEmbeddedEnginePath, &QPushButton::clicked, this, [this]() {
        QString file = QFileDialog::getOpenFileName(this, "組み込み推論エンジンを選択", m_editEmbeddedEnginePath->text(), "実行ファイル (*.exe);;すべてのファイル (*.*)");
        if (!file.isEmpty()) {
            m_editEmbeddedEnginePath->setText(file);
        }
    });

    layAi->addWidget(aiGroup);
    layAi->addStretch();
    tabWidget->addTab(tabAi, "画像生成 AI");

    // ==========================================
    // タブ4: プロンプト支援 LLM
    // ==========================================
    auto tabLlm = new QWidget(this);
    auto layLlm = new QVBoxLayout(tabLlm);

    auto llmGroup = new QGroupBox("プロンプト支援 LLM 設定", tabLlm);
    auto llmForm = new QFormLayout(llmGroup);

    m_comboLlmBackend = new QComboBox(this);
    m_comboLlmBackend->addItem("Ollama (ローカル)", 0);
    m_comboLlmBackend->addItem("LM Studio (ローカル)", 1);
    m_comboLlmBackend->addItem("llama.cpp (ローカル)", 2);
    m_comboLlmBackend->addItem("クラウド / OpenAI互換", 3);

    m_editLlmServerUrl = new QLineEdit(this);
    m_editLlmApiKey = new QLineEdit(this);
    m_editLlmApiKey->setEchoMode(QLineEdit::Password);
    m_editLlmApiKey->setPlaceholderText("sk-... (クラウド利用時のみ)");

    auto layModel = new QHBoxLayout();
    m_comboLlmModelName = new QComboBox(this);
    m_comboLlmModelName->setEditable(true);
    m_comboLlmModelName->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_btnFetchLlmModels = new QPushButton("モデル取得", this);
    m_btnFetchLlmModels->setMaximumWidth(80);
    layModel->addWidget(m_comboLlmModelName);
    layModel->addWidget(m_btnFetchLlmModels);

    m_lblLlmModelStatus = new QLabel(this);
    m_lblLlmModelStatus->setStyleSheet("color: gray; font-size: 11px;");

    auto layModelCol = new QVBoxLayout();
    layModelCol->addLayout(layModel);
    layModelCol->addWidget(m_lblLlmModelStatus);

    llmForm->addRow("LLM バックエンド:", m_comboLlmBackend);
    llmForm->addRow("サーバーURL:", m_editLlmServerUrl);
    llmForm->addRow("APIキー:", m_editLlmApiKey);
    llmForm->addRow("使用モデル:", layModelCol);

    m_llmPromptService = new LlmPromptService(this);
    connect(m_llmPromptService, &LlmPromptService::modelsFetched, this, [this](const QStringList& models) {
        m_btnFetchLlmModels->setEnabled(true);
        QString current = m_comboLlmModelName->currentText();
        m_comboLlmModelName->clear();
        for (const auto& m : models) {
            m_comboLlmModelName->addItem(m);
        }
        if (!current.isEmpty()) {
            int idx = m_comboLlmModelName->findText(current);
            if (idx >= 0) {
                m_comboLlmModelName->setCurrentIndex(idx);
            } else {
                m_comboLlmModelName->setCurrentText(current);
            }
        } else if (m_comboLlmModelName->count() > 0) {
            m_comboLlmModelName->setCurrentIndex(0);
        }
        m_lblLlmModelStatus->setText(QString("取得完了 (%1 件)").arg(models.size()));
    });
    connect(m_llmPromptService, &LlmPromptService::modelsFetchFailed, this, [this](const QString& errorMessage) {
        m_btnFetchLlmModels->setEnabled(true);
        m_lblLlmModelStatus->setText(QString("取得失敗: %1").arg(errorMessage));
    });

    auto fetchModels = [this]() {
        int backendIdx = m_comboLlmBackend->currentData().toInt();
        auto backend = static_cast<LlmBackend>(backendIdx);
        QString url = m_editLlmServerUrl->text().trimmed();
        QString key = m_editLlmApiKey->text().trimmed();
        if (backend == LlmBackend::CloudOpenAI && key.isEmpty()) {
            m_lblLlmModelStatus->setText("※ APIキーを入力してください");
            return;
        }
        m_lblLlmModelStatus->setText("モデル一覧を取得中...");
        m_btnFetchLlmModels->setEnabled(false);
        m_llmPromptService->fetchModels(backend, url, key);
    };

    connect(m_btnFetchLlmModels, &QPushButton::clicked, this, fetchModels);

    connect(m_comboLlmBackend, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, fetchModels](int idx) {
        if (idx == 0 && (m_editLlmServerUrl->text().isEmpty() || m_editLlmServerUrl->text().contains("1234") || m_editLlmServerUrl->text().contains("8080"))) {
            m_editLlmServerUrl->setText("http://127.0.0.1:11434");
        } else if (idx == 1 && (m_editLlmServerUrl->text().isEmpty() || m_editLlmServerUrl->text().contains("11434") || m_editLlmServerUrl->text().contains("8080"))) {
            m_editLlmServerUrl->setText("http://127.0.0.1:1234");
        } else if (idx == 2 && (m_editLlmServerUrl->text().isEmpty() || m_editLlmServerUrl->text().contains("11434") || m_editLlmServerUrl->text().contains("1234"))) {
            m_editLlmServerUrl->setText("http://127.0.0.1:8080");
        } else if (idx == 3 && m_editLlmServerUrl->text().contains("127.0.0.1")) {
            m_editLlmServerUrl->setText("https://api.openai.com/v1");
        }
        m_editLlmApiKey->setEnabled(idx == 3);
        if (m_initialized) {
            fetchModels();
        }
    });

    layLlm->addWidget(llmGroup);
    layLlm->addStretch();
    tabWidget->addTab(tabLlm, "プロンプト支援 LLM");

    mainLayout->addWidget(tabWidget);

    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText("OK");
    buttonBox->button(QDialogButtonBox::Cancel)->setText("キャンセル");
    connect(buttonBox, &QDialogButtonBox::accepted, this, &PreferencesDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    loadSettings();
    m_initialized = true;
    fetchModels();
}

static QSettings getSettings() {
    return QSettings(QSettings::IniFormat, QSettings::UserScope, "KaburagiPaint", "PaintSoftKABURAGI");
}

bool PreferencesDialog::isAutoBackupEnabled() {
    QSettings settings = getSettings();
    return settings.value("AutoBackup/enabled", true).toBool();
}

int PreferencesDialog::autoBackupIntervalMinutes() {
    QSettings settings = getSettings();
    return settings.value("AutoBackup/intervalMinutes", 10).toInt();
}

PreferencesDialog::ScrollBarPosition PreferencesDialog::layerScrollBarPosition() {
    QSettings settings = getSettings();
    int val = settings.value("Preferences/layerScrollBarPosition", static_cast<int>(ScrollBarPosition::Right)).toInt();
    return (val == static_cast<int>(ScrollBarPosition::Left)) ? ScrollBarPosition::Left : ScrollBarPosition::Right;
}

QString PreferencesDialog::language() {
    QSettings settings = getSettings();
    return settings.value("Preferences/language", "system").toString();
}

void PreferencesDialog::setLanguage(const QString& lang) {
    QSettings settings = getSettings();
    settings.setValue("Preferences/language", lang);
}

QString PreferencesDialog::effectiveLanguage() {
    QString lang = language();
    if (lang == "system") {
        if (QLocale::system().language() == QLocale::Japanese) {
            return "ja";
        }
        return "en";
    }
    return lang;
}

QString PreferencesDialog::aiModelDirectory() {
    QSettings settings = getSettings();
    return settings.value("AiLocal/modelDirectory", "").toString();
}

QString PreferencesDialog::aiControlNetDirectory() {
    QSettings settings = getSettings();
    return settings.value("AiLocal/controlNetDirectory", "").toString();
}

QString PreferencesDialog::comfyUiLaunchPath() {
    QSettings settings = getSettings();
    QString path = settings.value("AiLocal/comfyUiPath", "").toString();
    if (!path.isEmpty() && QFile::exists(path)) return path;

    // 自動探索フォールバック: モデルフォルダの親ディレクトリなど
    QString modelDir = aiModelDirectory();
    if (!modelDir.isEmpty()) {
        QDir mDir(modelDir);
        // E:/ComfyUI/Data/Models -> E:/ComfyUI
        if (mDir.cdUp() && mDir.cdUp()) {
            if (mDir.exists("run_nvidia_gpu.bat")) return mDir.filePath("run_nvidia_gpu.bat");
            if (mDir.exists("run_cpu.bat")) return mDir.filePath("run_cpu.bat");
        }
    }
    if (QFile::exists("E:/ComfyUI/run_nvidia_gpu.bat")) return "E:/ComfyUI/run_nvidia_gpu.bat";
    if (QFile::exists("E:/ComfyUI/run_cpu.bat")) return "E:/ComfyUI/run_cpu.bat";
    return path;
}

QString PreferencesDialog::webUiLaunchPath() {
    QSettings settings = getSettings();
    QString path = settings.value("AiLocal/webUiPath", "").toString();
    if (!path.isEmpty() && QFile::exists(path)) return path;

    // 自動探索フォールバック
    if (QFile::exists("C:/stable-diffusion-webui/webui-user.bat")) return "C:/stable-diffusion-webui/webui-user.bat";
    if (QFile::exists("D:/stable-diffusion-webui/webui-user.bat")) return "D:/stable-diffusion-webui/webui-user.bat";
    if (QFile::exists("E:/stable-diffusion-webui/webui-user.bat")) return "E:/stable-diffusion-webui/webui-user.bat";
    return path;
}

bool PreferencesDialog::isAutoLaunchServerEnabled() {
    QSettings settings = getSettings();
    return settings.value("AiLocal/autoLaunchServer", true).toBool();
}

QString PreferencesDialog::embeddedEnginePath() {
    QSettings settings = getSettings();
    QString path = settings.value("AiLocal/embeddedEnginePath", "").toString();
    if (!path.isEmpty() && QFile::exists(path)) return path;

    // アプリと同階層や tools フォルダ
    if (QFile::exists("tools/sd-cli.exe")) return "tools/sd-cli.exe";
    if (QFile::exists("tools/sd.exe")) return "tools/sd.exe";
    if (QFile::exists("sd-cli.exe")) return "sd-cli.exe";
    if (QFile::exists("sd.exe")) return "sd.exe";
    return path;
}

void PreferencesDialog::setEmbeddedEnginePath(const QString& path) {
    QSettings settings = getSettings();
    settings.setValue("AiLocal/embeddedEnginePath", path);
}

void PreferencesDialog::setAiModelDirectory(const QString& path) {
    QSettings settings = getSettings();
    settings.setValue("AiLocal/modelDirectory", path);
}

void PreferencesDialog::setAiControlNetDirectory(const QString& path) {
    QSettings settings = getSettings();
    settings.setValue("AiLocal/controlNetDirectory", path);
}

int PreferencesDialog::llmBackend() {
    QSettings settings = getSettings();
    return settings.value("LlmPrompt/backend", 0).toInt();
}

QString PreferencesDialog::llmServerUrl() {
    QSettings settings = getSettings();
    return settings.value("LlmPrompt/serverUrl", "http://127.0.0.1:11434").toString();
}

QString PreferencesDialog::llmApiKey() {
    QSettings settings = getSettings();
    return settings.value("LlmPrompt/apiKey", "").toString();
}

QString PreferencesDialog::llmModelName() {
    QSettings settings = getSettings();
    return settings.value("LlmPrompt/modelName", "llama3").toString();
}

QString PreferencesDialog::monitorProfilePath() {
    QSettings settings = getSettings();
    return settings.value("display/monitorProfilePath", "").toString();
}

void PreferencesDialog::setMonitorProfilePath(const QString& path) {
    QSettings settings = getSettings();
    settings.setValue("display/monitorProfilePath", path);
    DisplayFilterManager::instance().setMonitorProfilePath(path);
}

QString PreferencesDialog::iccProfilePath() {
    QSettings settings = getSettings();
    return settings.value("display/iccProfilePath", "").toString();
}

void PreferencesDialog::setIccProfilePath(const QString& path) {
    QSettings settings = getSettings();
    settings.setValue("display/iccProfilePath", path);
    DisplayFilterManager::instance().setProofingProfilePath(path);
}

void PreferencesDialog::updateMonitorProfileDetails(const QString& path) {
    if (!m_lblMonitorProfileInfo) return;

    if (path.isEmpty()) {
        m_lblMonitorProfileInfo->setText("（未指定: 標準 sRGB 色空間として表示されます）");
        return;
    }

    QFile file(path);
    if (!file.exists()) {
        m_lblMonitorProfileInfo->setText(QString("ファイルが見つかりません:\n%1").arg(path));
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        m_lblMonitorProfileInfo->setText(QString("ファイルを読み込めません:\n%1").arg(path));
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QColorSpace cs = QColorSpace::fromIccProfile(data);
    if (!cs.isValid()) {
        m_lblMonitorProfileInfo->setText(QString("無効な ICC プロファイル形式です:\n%1").arg(path));
        return;
    }

    QString desc = cs.description().isEmpty() ? QFileInfo(path).completeBaseName() : cs.description();
    QString modelStr = "その他";
    switch (cs.colorModel()) {
    case QColorSpace::ColorModel::Rgb: modelStr = "RGB"; break;
    case QColorSpace::ColorModel::Cmyk: modelStr = "CMYK"; break;
    case QColorSpace::ColorModel::Gray: modelStr = "Gray (グレースケール)"; break;
    default: break;
    }

    QString info = QString("名前: %1 | 色空間: %2 | サイズ: %3 KB\nパス: %4")
        .arg(desc)
        .arg(modelStr)
        .arg(data.size() / 1024)
        .arg(path);

    m_lblMonitorProfileInfo->setText(info);
}

void PreferencesDialog::updateIccProfileDetails(const QString& path) {
    if (!m_lblIccProfileInfo) return;

    if (path.isEmpty()) {
        m_lblIccProfileInfo->setText("（未指定: 校正シミュレーションは行われません）");
        return;
    }

    QFile file(path);
    if (!file.exists()) {
        m_lblIccProfileInfo->setText(QString("ファイルが見つかりません:\n%1").arg(path));
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        m_lblIccProfileInfo->setText(QString("ファイルを読み込めません:\n%1").arg(path));
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QColorSpace cs = QColorSpace::fromIccProfile(data);
    if (!cs.isValid()) {
        m_lblIccProfileInfo->setText(QString("無効な ICC プロファイル形式です:\n%1").arg(path));
        return;
    }

    QString desc = cs.description().isEmpty() ? QFileInfo(path).completeBaseName() : cs.description();
    QString modelStr = "その他";
    switch (cs.colorModel()) {
    case QColorSpace::ColorModel::Rgb: modelStr = "RGB"; break;
    case QColorSpace::ColorModel::Cmyk: modelStr = "CMYK"; break;
    case QColorSpace::ColorModel::Gray: modelStr = "Gray (グレースケール)"; break;
    default: break;
    }

    QString info = QString("名前: %1 | 色空間: %2 | サイズ: %3 KB\nパス: %4")
        .arg(desc)
        .arg(modelStr)
        .arg(data.size() / 1024)
        .arg(path);

    m_lblIccProfileInfo->setText(info);
}

void PreferencesDialog::loadSettings() {
    QSettings settings = getSettings();
    settings.beginGroup("AutoBackup");
    bool enabled = settings.value("enabled", true).toBool();
    int interval = settings.value("intervalMinutes", 10).toInt();
    settings.endGroup();

    m_autoBackupCheck->setChecked(enabled);
    m_backupIntervalSpin->setValue(interval);
    m_backupIntervalSpin->setEnabled(enabled);

    int scrollPos = settings.value("Preferences/layerScrollBarPosition", static_cast<int>(ScrollBarPosition::Right)).toInt();
    int idx = m_layerScrollPosCombo->findData(scrollPos);
    if (idx >= 0) {
        m_layerScrollPosCombo->setCurrentIndex(idx);
    }

    QString lang = settings.value("Preferences/language", "system").toString();
    int lIdx = m_comboLanguage->findData(lang);
    if (lIdx >= 0) {
        m_comboLanguage->setCurrentIndex(lIdx);
    } else {
        m_comboLanguage->setCurrentIndex(0);
    }

    // モニタープロファイル読み込み
    QString monPath = settings.value("display/monitorProfilePath", DisplayFilterManager::instance().monitorProfilePath()).toString();
    m_editMonitorProfilePath->setText(monPath);
    int mIdx = m_comboMonitorProfile->findData(monPath);
    if (mIdx >= 0) {
        m_comboMonitorProfile->setCurrentIndex(mIdx);
    } else {
        m_comboMonitorProfile->setCurrentIndex(0);
    }
    updateMonitorProfileDetails(monPath);

    // 色校正用 ICC プロファイル読み込み
    QString iccPath = settings.value("display/iccProfilePath", DisplayFilterManager::instance().proofingProfilePath()).toString();
    m_editIccProfilePath->setText(iccPath);
    int pIdx = m_comboIccProfile->findData(iccPath);
    if (pIdx >= 0) {
        m_comboIccProfile->setCurrentIndex(pIdx);
    } else {
        m_comboIccProfile->setCurrentIndex(0);
    }
    updateIccProfileDetails(iccPath);

    settings.beginGroup("AiLocal");
    m_editAiModelDir->setText(settings.value("modelDirectory", "").toString());
    m_editAiControlNetDir->setText(settings.value("controlNetDirectory", "").toString());

    QString comfyPath = settings.value("comfyUiPath", "").toString();
    if (comfyPath.isEmpty()) comfyPath = comfyUiLaunchPath();
    m_editComfyUiPath->setText(comfyPath);

    QString webUiPath = settings.value("webUiPath", "").toString();
    if (webUiPath.isEmpty()) webUiPath = webUiLaunchPath();
    m_editWebUiPath->setText(webUiPath);

    m_chkAutoLaunchServer->setChecked(settings.value("autoLaunchServer", true).toBool());
    m_editEmbeddedEnginePath->setText(settings.value("embeddedEnginePath", "").toString());
    settings.endGroup();

    settings.beginGroup("LlmPrompt");
    int llmB = settings.value("backend", 0).toInt();
    int bIdx = m_comboLlmBackend->findData(llmB);
    if (bIdx >= 0) m_comboLlmBackend->setCurrentIndex(bIdx);
    m_editLlmServerUrl->setText(settings.value("serverUrl", "http://127.0.0.1:11434").toString());
    m_editLlmApiKey->setText(settings.value("apiKey", "").toString());
    m_comboLlmModelName->setCurrentText(settings.value("modelName", "llama3").toString());
    m_editLlmApiKey->setEnabled(llmB == 3);
    settings.endGroup();
}

void PreferencesDialog::saveSettings() {
    QSettings settings = getSettings();
    settings.beginGroup("AutoBackup");
    settings.setValue("enabled", m_autoBackupCheck->isChecked());
    settings.setValue("intervalMinutes", m_backupIntervalSpin->value());
    settings.endGroup();

    settings.setValue("Preferences/layerScrollBarPosition", m_layerScrollPosCombo->currentData().toInt());

    QString oldLang = language();
    QString newLang = m_comboLanguage->currentData().toString();
    settings.setValue("Preferences/language", newLang);
    if (m_initialized && oldLang != newLang) {
        QMessageBox::information(this, tr("Language Setting"),
            tr("The language setting will take effect after restarting the application."));
    }

    // モニタープロファイル保存
    QString monPath = m_editMonitorProfilePath->text().trimmed();
    settings.setValue("display/monitorProfilePath", monPath);
    DisplayFilterManager::instance().setMonitorProfilePath(monPath);

    // ICC プロファイル保存
    QString iccPath = m_editIccProfilePath->text().trimmed();
    settings.setValue("display/iccProfilePath", iccPath);
    DisplayFilterManager::instance().setIccProfilePath(iccPath);

    settings.beginGroup("AiLocal");
    settings.setValue("modelDirectory", m_editAiModelDir->text().trimmed());
    settings.setValue("controlNetDirectory", m_editAiControlNetDir->text().trimmed());
    settings.setValue("comfyUiPath", m_editComfyUiPath->text().trimmed());
    settings.setValue("webUiPath", m_editWebUiPath->text().trimmed());
    settings.setValue("autoLaunchServer", m_chkAutoLaunchServer->isChecked());
    settings.setValue("embeddedEnginePath", m_editEmbeddedEnginePath->text().trimmed());
    settings.endGroup();

    settings.beginGroup("LlmPrompt");
    settings.setValue("backend", m_comboLlmBackend->currentData().toInt());
    settings.setValue("serverUrl", m_editLlmServerUrl->text().trimmed());
    settings.setValue("apiKey", m_editLlmApiKey->text().trimmed());
    settings.setValue("modelName", m_comboLlmModelName->currentText().trimmed());
    settings.endGroup();
}

void PreferencesDialog::accept() {
    saveSettings();
    QDialog::accept();
}

} // namespace Kaburagi
