#pragma once

#include <QDialog>
#include <QString>
#include "LlmPromptService.hpp"
#include "AiService.hpp"

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

    void setAiMode(AiMode mode);
    AiMode aiMode() const { return m_aiMode; }

    void setCurrentPrompt(const QString& prompt, const QString& negativePrompt, const QImage& lastImage = QImage());

    QString generatedPrompt() const;
    QString generatedNegativePrompt() const;
    QString lastUserInput() const; // アイデアまたは改善要望
    bool shouldAppend() const; // true: 追加, false: 上書き
    bool isRefineMode() const;

private slots:
    void onModeChanged();
    void onBackendChanged();
    void onFetchModelsClicked();
    void onGenerateClicked();
    void onApplyClicked();

    // LlmPromptService slots
    void onModelsFetched(const QStringList& models);
    void onModelsFetchFailed(const QString& errorMessage);
    void onPromptGenerationProgress(int percent, const QString& statusMessage, const QString& currentContent);
    void onPromptGenerated(const LlmPromptResult& result);
    void onPromptGenerationFailed(const QString& errorMessage);

private:
    void setupUi();
    void loadSettings();
    void saveSettings();
    LlmConfig currentConfig() const;

private:
    LlmPromptService* m_llmService = nullptr;

    // Mode selection (Create new vs Refine previous)
    QRadioButton* m_radioModeCreate = nullptr;
    QRadioButton* m_radioModeRefine = nullptr;
    QWidget* m_widgetCreateMode = nullptr;
    QWidget* m_widgetRefineMode = nullptr;

    // Backend controls
    QRadioButton* m_radioOllama = nullptr;
    QRadioButton* m_radioLmStudio = nullptr;
    QRadioButton* m_radioLlamaCpp = nullptr;
    QRadioButton* m_radioCloud = nullptr;

    QLineEdit* m_editServerUrl = nullptr;
    QLineEdit* m_editApiKey = nullptr;
    QComboBox* m_comboModel = nullptr;
    QPushButton* m_btnFetchModels = nullptr;

    // Mode 1: Create Idea
    QPlainTextEdit* m_editIdea = nullptr;

    // Mode 2: Refine Previous
    QLabel* m_lblRefineImagePreview = nullptr;
    QPlainTextEdit* m_editRefinePrevPrompt = nullptr;
    QPlainTextEdit* m_editRefineRequest = nullptr;

    // Style & Apply options
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
    QImage m_lastImage;
    bool m_initialized = false;
    AiMode m_aiMode = AiMode::BackgroundAndNew;
    QLabel* m_lblInpaintNotice = nullptr;
};

} // namespace Kaburagi
