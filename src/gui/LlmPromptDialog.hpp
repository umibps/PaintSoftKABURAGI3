#pragma once

#include <QDialog>
#include <QString>
#include "LlmPromptService.hpp"

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QProgressBar;
class QLabel;
class QRadioButton;

namespace Kaburagi {

class LlmPromptDialog : public QDialog {
    Q_OBJECT

public:
    explicit LlmPromptDialog(QWidget* parent = nullptr);
    ~LlmPromptDialog() override;

    void setCurrentPrompt(const QString& prompt, const QString& negativePrompt);

    QString generatedPrompt() const;
    QString generatedNegativePrompt() const;
    bool shouldAppend() const; // true: 追加, false: 上書き

private slots:
    void onBackendChanged();
    void onFetchModelsClicked();
    void onGenerateClicked();
    void onApplyClicked();

    // LlmPromptService slots
    void onModelsFetched(const QStringList& models);
    void onModelsFetchFailed(const QString& errorMessage);
    void onPromptGenerated(const LlmPromptResult& result);
    void onPromptGenerationFailed(const QString& errorMessage);

private:
    void setupUi();
    void loadSettings();
    void saveSettings();
    LlmConfig currentConfig() const;

private:
    LlmPromptService* m_llmService = nullptr;

    // Backend controls
    QRadioButton* m_radioOllama = nullptr;
    QRadioButton* m_radioLmStudio = nullptr;
    QRadioButton* m_radioLlamaCpp = nullptr;
    QRadioButton* m_radioCloud = nullptr;

    QLineEdit* m_editServerUrl = nullptr;
    QLineEdit* m_editApiKey = nullptr;
    QComboBox* m_comboModel = nullptr;
    QPushButton* m_btnFetchModels = nullptr;

    // Idea & Style
    QPlainTextEdit* m_editIdea = nullptr;
    QComboBox* m_comboStyle = nullptr;
    QComboBox* m_comboApplyMode = nullptr;

    // Generation Action & Status
    QPushButton* m_btnGenerate = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_lblStatus = nullptr;

    // Result Preview
    QPlainTextEdit* m_editResultPrompt = nullptr;
    QPlainTextEdit* m_editResultNegative = nullptr;
    QPushButton* m_btnApply = nullptr;

    QString m_existingPrompt;
    QString m_existingNegative;
    bool m_initialized = false;
};

} // namespace Kaburagi
