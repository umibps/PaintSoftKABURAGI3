#pragma once

#include <QDockWidget>
#include <QImage>
#include <QList>
#include <memory>
#include "AiService.hpp"

class QRadioButton;
class QLineEdit;
class QComboBox;
class QPlainTextEdit;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QPushButton;
class QProgressBar;
class QLabel;
class QListWidget;

namespace Kaburagi {

class Document;
class CanvasWidget;
class Layer;

/**
 * @brief AI画像生成アシスタント用ドックウィジェット
 */
class AiAssistantDockWidget : public QDockWidget {
    Q_OBJECT

public:
    explicit AiAssistantDockWidget(QWidget* parent = nullptr);
    ~AiAssistantDockWidget() override;

    void setDocument(Document* doc);
    void setCanvasWidget(CanvasWidget* canvas);

    AiService* aiService() const { return m_aiService; }

    QStringList scanLocalModels() const;
    QStringList scanLocalControlNetModels() const;
    QStringList scanUpscaleModels() const;
    void refreshLocalModels();
    void saveSettings();
    void loadSettings();
    void resetToDefaults();
    void loadParametersFromLayer(const std::shared_ptr<Layer>& layer);

signals:
    void applyAsNewLayer(const QImage& image, const QJsonObject& aiParams);
    void applyOverwriteSelection(const QImage& image, const QJsonObject& aiParams);

public slots:
    void onFetchModelsClicked();
    void onLoadFromActiveLayerClicked();
    void onResetDefaultsClicked();

private slots:
    void onBackendChanged();
    void onModeChanged(int index);
    void onResolutionPresetChanged(int index);
    void onCustomResolutionChanged();
    void onRandomSeedClicked();
    void onGenerateClicked();
    void onCancelClicked();
    void onApplyNewLayerClicked();
    void onApplyOverwriteClicked();
    void onThumbnailSelected(int row);
    void onPromptAssistClicked();

    // AiService slots
    void onModelsFetched(const QStringList& models);
    void onModelsFetchFailed(const QString& errorMessage);
    void onGenerationProgress(int percent, const QString& statusMessage);
    void onGenerationFinished(const QList<QImage>& results);
    void onGenerationFailed(const QString& errorMessage);

private:
    void setupUi();
    void updateUiForMode(AiMode mode);
    void detectAvailableDevices();
    bool ensureRequirementsReady();
    QImage getInitImageForCurrentMode();
    QImage getMaskImageForCurrentMode();
    QImage getControlNetImage();

private:
    AiService* m_aiService = nullptr;
    Document* m_document = nullptr;
    CanvasWidget* m_canvasWidget = nullptr;

    // UI Widgets
    QRadioButton* m_radioComfyUi = nullptr;
    QRadioButton* m_radioWebUi = nullptr;
    QRadioButton* m_radioEmbedded = nullptr;
    QRadioButton* m_radioCloud = nullptr;
    QLineEdit* m_editServerUrl = nullptr;
    QLineEdit* m_editApiKey = nullptr;
    QComboBox* m_comboModel = nullptr;
    QPushButton* m_btnFetchModels = nullptr;

    // Device
    QComboBox* m_comboDevice = nullptr;

    QComboBox* m_comboMode = nullptr;
    QPlainTextEdit* m_editPrompt = nullptr;
    QPlainTextEdit* m_editNegativePrompt = nullptr;
    QPushButton* m_btnPromptAssist = nullptr;

    // ControlNet
    QCheckBox* m_chkControlNet = nullptr;
    QComboBox* m_comboControlNetType = nullptr;
    QDoubleSpinBox* m_spinControlNetWeight = nullptr;
    QComboBox* m_comboControlNetSource = nullptr;

    // Resolution & Upscale
    QComboBox* m_comboResolution = nullptr;
    QSpinBox* m_spinWidth = nullptr;
    QSpinBox* m_spinHeight = nullptr;
    QComboBox* m_comboUpscaleMethod = nullptr;

    // Generation parameters
    QSpinBox* m_spinBatchCount = nullptr;
    QSpinBox* m_spinSteps = nullptr;
    QDoubleSpinBox* m_spinCfg = nullptr;
    QDoubleSpinBox* m_spinDenoising = nullptr;
    QSpinBox* m_spinSeed = nullptr;
    QPushButton* m_btnRandomSeed = nullptr;

    QPushButton* m_btnGenerate = nullptr;
    QPushButton* m_btnCancel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_lblStatus = nullptr;

    // Result display
    QListWidget* m_listThumbnails = nullptr;
    QPushButton* m_btnApplyNewLayer = nullptr;
    QPushButton* m_btnApplyOverwrite = nullptr;
    QPushButton* m_btnLoadFromLayer = nullptr;
    QPushButton* m_btnResetDefaults = nullptr;

    AiGenerateParams m_lastGeneratedParams;
    QJsonObject m_lastGeneratedJson;

    QList<QImage> m_currentResults;
    int m_selectedIndex = -1;
    bool m_updatingResolution = false;
};

} // namespace Kaburagi
