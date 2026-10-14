#pragma once

#include <QDialog>

class QCheckBox;
class QSpinBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QLabel;

namespace Kaburagi {

class LlmPromptService;

class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    enum class ScrollBarPosition {
        Right = 0,
        Left = 1
    };

    explicit PreferencesDialog(QWidget* parent = nullptr);
    ~PreferencesDialog() override = default;

    static bool isAutoBackupEnabled();
    static int autoBackupIntervalMinutes();
    static ScrollBarPosition layerScrollBarPosition();
    static QString language();
    static void setLanguage(const QString& lang);
    static QString effectiveLanguage();
    static QString aiModelDirectory();
    static QString aiControlNetDirectory();
    static QString comfyUiLaunchPath();
    static QString webUiLaunchPath();
    static bool isAutoLaunchServerEnabled();
    static QString embeddedEnginePath();
    static void setEmbeddedEnginePath(const QString& path);
    static void setAiModelDirectory(const QString& path);
    static void setAiControlNetDirectory(const QString& path);

    static int llmBackend();
    static QString llmServerUrl();
    static QString llmApiKey();
    static QString llmModelName();

    static QString monitorProfilePath();
    static void setMonitorProfilePath(const QString& path);
    static QString iccProfilePath();
    static void setIccProfilePath(const QString& path);
    static bool isRightClickEyedropperEnabled();

    void accept() override;

private:
    void loadSettings();
    void saveSettings();

    QCheckBox* m_autoBackupCheck = nullptr;
    QSpinBox* m_backupIntervalSpin = nullptr;
    QComboBox* m_layerScrollPosCombo = nullptr;
    QComboBox* m_comboLanguage = nullptr;
    QCheckBox* m_chkRightClickEyedropper = nullptr;

    QLineEdit* m_editAiModelDir = nullptr;
    QPushButton* m_btnBrowseAiModelDir = nullptr;
    QLineEdit* m_editAiControlNetDir = nullptr;
    QPushButton* m_btnBrowseAiControlNetDir = nullptr;

    QLineEdit* m_editComfyUiPath = nullptr;
    QPushButton* m_btnBrowseComfyUiPath = nullptr;
    QLineEdit* m_editWebUiPath = nullptr;
    QPushButton* m_btnBrowseWebUiPath = nullptr;
    QCheckBox* m_chkAutoLaunchServer = nullptr;

    QLineEdit* m_editEmbeddedEnginePath = nullptr;
    QPushButton* m_btnBrowseEmbeddedEnginePath = nullptr;

    QComboBox* m_comboLlmBackend = nullptr;
    QLineEdit* m_editLlmServerUrl = nullptr;
    QLineEdit* m_editLlmApiKey = nullptr;
    QComboBox* m_comboLlmModelName = nullptr;
    QPushButton* m_btnFetchLlmModels = nullptr;
    QLabel* m_lblLlmModelStatus = nullptr;
    LlmPromptService* m_llmPromptService = nullptr;

    // モニタープロファイル
    QComboBox* m_comboMonitorProfile = nullptr;
    QLineEdit* m_editMonitorProfilePath = nullptr;
    QPushButton* m_btnBrowseMonitorProfile = nullptr;
    QLabel* m_lblMonitorProfileInfo = nullptr;
    void updateMonitorProfileDetails(const QString& path);

    // 色校正用ICCプロファイル
    QComboBox* m_comboIccProfile = nullptr;
    QLineEdit* m_editIccProfilePath = nullptr;
    QPushButton* m_btnBrowseIccProfile = nullptr;
    QLabel* m_lblIccProfileInfo = nullptr;
    void updateIccProfileDetails(const QString& path);

    bool m_initialized = false;
};

} // namespace Kaburagi
