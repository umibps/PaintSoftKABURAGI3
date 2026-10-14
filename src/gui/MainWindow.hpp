#pragma once

#include <QMainWindow>
#include <QComboBox>
#include <QProgressBar>
#include <QLabel>
#include "Document.hpp"
#include "CanvasWidget.hpp"
#include "ColorWheelWidget.hpp"
#include "ColorPaletteWidget.hpp"
#include "ToolBoxWidget.hpp"
#include "BrushSettingsWidget.hpp"
#include "LayerPanelWidget.hpp"
#include "NavigatorWidget.hpp"

#include <map>
#include <QCloseEvent>
#include <QDockWidget>
#include <QColorDialog>
#include "RectSelectSettingsWidget.hpp"
#include "EllipseSelectSettingsWidget.hpp"
#include "LassoSelectSettingsWidget.hpp"
#include "ColorSelectSettingsWidget.hpp"
#include "BucketSettingsWidget.hpp"
#include "AutoSelectSettingsWidget.hpp"
#include "BlurSettingsWidget.hpp"
#include "SmudgeSettingsWidget.hpp"
#include "GradientSettingsWidget.hpp"
#include "WatercolorSettingsWidget.hpp"
#include "CompositeBrushSettingsWidget.hpp"
#include "VectorEraserSettingsWidget.hpp"
#include "VectorControlPointSettingsWidget.hpp"
#include "VectorLineSettingsWidget.hpp"
#include "VectorChangeLineSettingsWidget.hpp"
#include "VectorColorChangeSettingsWidget.hpp"
#include "VectorEllipseSettingsWidget.hpp"
#include "VectorCircleSettingsWidget.hpp"
#include "VectorPolygonSettingsWidget.hpp"
#include "VectorPressureSettingsWidget.hpp"
#include "ImageBrushSettingsWidget.hpp"
#include "TransformSettingsWidget.hpp"
#include "AdjustmentSettingsWidget.hpp"
#include "TextSettingsWidget.hpp"
#include "PanelSettingsWidget.hpp"

#include <QTimer>
#include <QStringList>

class QStackedWidget;
class QSplitter;
class QMenu;

namespace Kaburagi {

class AiAssistantDockWidget;
class PreviewWindow;
class ReferenceImageWindow;
class CanvasContainer;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    QSplitter* toolSplitter() const { return m_toolSplitter; }
    QSplitter* navSplitter() const { return m_navSplitter; }

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onNewCanvas();
    void onCloseCanvas();
    bool onSaveCanvas();
    bool onSaveAsCanvas();
    void onOpenCanvas();
    void onPreferences();
    void onAutoBackupTimer();
    void onColorChanged(const QColor& color);
    void onToolChanged(ToolType tool);
    void onSmoothLevelChanged(int index);
    void onUndo();
    void onRedo();
    void updateUndoRedoActions();
    void updateWindowTitle();
    void onDeletePixels();
    void updateDeletePixelsAction();
    void resetDockLayout();

public slots:
    void openFile(const QString& filePath);
    void onCut();
    void onCopy();
    void onCopyVisible();
    void onPaste();
    void updateEditActions();

    void onNewRasterLayer();
    void onNewVectorLayer();
    void onNewTextLayer();
    void onNewGroupLayer();
    void onNewPanelFolder();
    void onNewThreeDLayer();
    void onNewLayerFromVisible();
    void onDuplicateLayer();
    void onMergeDown();
    void onDeleteLayer();
    void onSelectOpaque();
    void onAddOpaqueToSelection();

    // フィルターメニュー
    void onFilterFocusBlur();
    void onFilterGaussianBlur();
    void onFilterMotionBlur();
    void onFilterBrightnessContrast();
    void onFilterHueSaturation();
    void onFilterToneCurve();
    void onFilterGradientMap();
    void onRasterizeLayer();
    void onFillLayer();
    void updateLayerMenuActions();


    void onAiApplyAsNewLayer(const QImage& image, const QJsonObject& aiParams = QJsonObject());
    void onAiApplyOverwriteSelection(const QImage& image, const QJsonObject& aiParams = QJsonObject());
    void loadAiParametersFromActiveLayer();
    void onTogglePreviewWindow(bool checked);
    void onToggleReferenceImageWindow(bool checked);
    void onToggleSelectionEdit(bool checked);
    void setDrawingColor(const QColor& color);

private:
    bool maybeSave();
    void setupDocument(Document* doc);
    void addRecentFile(const QString& filePath);
    void updateRecentFilesMenu();
    void restartAutoBackupTimer();
    void createMenuBar();
    void createQuickToolBar();
    void createStatusBar();
    void setupLayout();

    void initDefaultToolConfigs();
    void saveSettings();
    void loadSettings();
    void saveCurrentToolConfig();
    void updateLayerTools();
    QImage copySelectionToImage() const;
    QImage copyVisibleToImage() const;

    Document* m_document = nullptr;
    QString m_currentFilePath;

    CanvasWidget* m_canvasWidget = nullptr;
    CanvasContainer* m_canvasContainer = nullptr;
    QTabWidget* m_tabWidget = nullptr;
    QToolButton* m_tabCloseBtn = nullptr;
    ColorWheelWidget* m_colorWheel = nullptr;
    ColorPaletteWidget* m_paletteWidget = nullptr;
    QPushButton* m_btnTogglePalette = nullptr;
    QPushButton* m_btnPickColorDialog = nullptr;
    ToolBoxWidget* m_toolBox = nullptr;
    QSplitter* m_toolSplitter = nullptr;
    QSplitter* m_navSplitter = nullptr;
    QStackedWidget* m_toolSettingsStack = nullptr;
    BrushSettingsWidget* m_brushSettings = nullptr;
    RectSelectSettingsWidget* m_rectSelectSettings = nullptr;
    EllipseSelectSettingsWidget* m_ellipseSelectSettings = nullptr;
    LassoSelectSettingsWidget* m_lassoSelectSettings = nullptr;
    ColorSelectSettingsWidget* m_colorSelectSettings = nullptr;
    BucketSettingsWidget* m_bucketSettings = nullptr;
    AutoSelectSettingsWidget* m_autoSelectSettings = nullptr;
    BlurSettingsWidget* m_blurSettings = nullptr;
    SmudgeSettingsWidget* m_smudgeSettings = nullptr;
    WatercolorSettingsWidget* m_watercolorSettings = nullptr;
    CompositeBrushSettingsWidget* m_compositeBrushSettings = nullptr;
    ImageBrushSettingsWidget* m_imageBrushSettings = nullptr;
    GradientSettingsWidget* m_gradientSettings = nullptr;
    VectorEraserSettingsWidget* m_vectorEraserSettings = nullptr;
    VectorControlPointSettingsWidget* m_vectorControlPointSettings = nullptr;
    VectorLineSettingsWidget* m_vectorLineSettings = nullptr;
    VectorChangeLineSettingsWidget* m_vectorChangeLineSettings = nullptr;
    VectorColorChangeSettingsWidget* m_vectorColorChangeSettings = nullptr;
    VectorEllipseSettingsWidget* m_vectorEllipseSettings = nullptr;
    VectorCircleSettingsWidget* m_vectorCircleSettings = nullptr;
    VectorPolygonSettingsWidget* m_vectorPolygonSettings = nullptr;
    VectorPressureSettingsWidget* m_vectorPressureSettings = nullptr;
    TransformSettingsWidget* m_transformSettings = nullptr;
    AdjustmentSettingsWidget* m_adjustmentSettings = nullptr;
    TextSettingsWidget* m_textSettings = nullptr;
    PanelSettingsWidget* m_panelSettings = nullptr;
    ThreeDSettingsWidget* m_threeDSettings = nullptr;
    LayerPanelWidget* m_layerPanel = nullptr;
    NavigatorWidget* m_navigator = nullptr;

    QDockWidget* m_navLayerDock = nullptr;
    QDockWidget* m_colorBrushDock = nullptr;
    AiAssistantDockWidget* m_aiAssistantDock = nullptr;

    QComboBox* m_smoothCombo = nullptr;
    QLabel* m_statusMemoryLabel = nullptr;

    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QAction* m_cutAction = nullptr;
    QAction* m_copyAction = nullptr;
    QAction* m_copyVisibleAction = nullptr;
    QAction* m_pasteAction = nullptr;
    QAction* m_deletePixelsAction = nullptr;
    QAction* m_transformAction = nullptr;
    QAction* m_flipHAction = nullptr;
    QToolButton* m_btnFlipH = nullptr;
    QToolButton* m_btnSwitchBgColor = nullptr;

    // レイヤーメニューアクション
    QAction* m_newRasterLayerAction = nullptr;
    QAction* m_newVectorLayerAction = nullptr;
    QAction* m_newTextLayerAction = nullptr;
    QAction* m_newGroupLayerAction = nullptr;
    QAction* m_newPanelFolderAction = nullptr;
    QAction* m_newThreeDLayerAction = nullptr;
    QAction* m_newAdjustmentLayerAction = nullptr;
    QAction* m_newLayerFromVisibleAction = nullptr;
    QAction* m_duplicateLayerAction = nullptr;
    QAction* m_mergeDownAction = nullptr;
    QAction* m_deleteLayerAction = nullptr;
    QAction* m_selectOpaqueAction = nullptr;
    QAction* m_addOpaqueToSelectionAction = nullptr;
    QAction* m_rasterizeLayerAction = nullptr;
    QAction* m_fillLayerAction = nullptr;

    // キャンバス変形アクション
    QAction* m_changeCanvasSizeAction = nullptr;
    QAction* m_scaleCanvasAction = nullptr;
    QMenu* m_rotateCanvasMenu = nullptr;
    QAction* m_rotate90CWAction = nullptr;
    QAction* m_rotate90CCWAction = nullptr;
    QAction* m_rotate180Action = nullptr;
    QAction* m_flipCanvasHAction = nullptr;
    QAction* m_flipCanvasVAction = nullptr;

    // キャンバスカラモードアクション
    QActionGroup* m_colorModeGroup = nullptr;
    QAction* m_colorModeRgbAction = nullptr;
    QAction* m_colorModeCmykProofAction = nullptr;
    QAction* m_colorModeCmykNativeAction = nullptr;

public:
    void updateCanvasMenuActions();
    void onChangeCanvasSize();
    void onScaleCanvas();
    QAction* changeCanvasSizeAction() const { return m_changeCanvasSizeAction; }
    QAction* scaleCanvasAction() const { return m_scaleCanvasAction; }
    QAction* rotate90CWAction() const { return m_rotate90CWAction; }
    QAction* rotate90CCWAction() const { return m_rotate90CCWAction; }
    QAction* rotate180Action() const { return m_rotate180Action; }
    QAction* flipCanvasHAction() const { return m_flipCanvasHAction; }
    QAction* flipCanvasVAction() const { return m_flipCanvasVAction; }
    void updateColorModeActions();
    QAction* colorModeRgbAction() const { return m_colorModeRgbAction; }
    QAction* colorModeCmykProofAction() const { return m_colorModeCmykProofAction; }
    QAction* colorModeCmykNativeAction() const { return m_colorModeCmykNativeAction; }
    QToolButton* flipHorizontalButton() const { return m_btnFlipH; }
    QToolButton* switchBackgroundColorButton() const { return m_btnSwitchBgColor; }
    QAction* cutAction() const { return m_cutAction; }
    QAction* copyAction() const { return m_copyAction; }
    QAction* copyVisibleAction() const { return m_copyVisibleAction; }
    QAction* pasteAction() const { return m_pasteAction; }
    QAction* newRasterLayerAction() const { return m_newRasterLayerAction; }
    QAction* newVectorLayerAction() const { return m_newVectorLayerAction; }
    QAction* newTextLayerAction() const { return m_newTextLayerAction; }
    QAction* newGroupLayerAction() const { return m_newGroupLayerAction; }
    QAction* newPanelFolderAction() const { return m_newPanelFolderAction; }
    QAction* newThreeDLayerAction() const { return m_newThreeDLayerAction; }
    QAction* newAdjustmentLayerAction() const { return m_newAdjustmentLayerAction; }
    QAction* newLayerFromVisibleAction() const { return m_newLayerFromVisibleAction; }
    QAction* duplicateLayerAction() const { return m_duplicateLayerAction; }
    QAction* mergeDownAction() const { return m_mergeDownAction; }
    QAction* deleteLayerAction() const { return m_deleteLayerAction; }
    QAction* selectOpaqueAction() const { return m_selectOpaqueAction; }
    QAction* addOpaqueToSelectionAction() const { return m_addOpaqueToSelectionAction; }
    QAction* rasterizeLayerAction() const { return m_rasterizeLayerAction; }
    QAction* fillLayerAction() const { return m_fillLayerAction; }
    QAction* filterNoneAction() const { return m_filterNoneAction; }
    QAction* filterGrayscaleAction() const { return m_filterGrayscaleAction; }
    QAction* filterGrayscaleYiqAction() const { return m_filterGrayscaleYiqAction; }
    QAction* filterIccAction() const { return m_filterIccAction; }
    QAction* previewWindowAction() const { return m_previewWindowAction; }
    PreviewWindow* previewWindow() const { return m_previewWindow; }
    QAction* referenceImageWindowAction() const { return m_referenceImageWindowAction; }
    ReferenceImageWindow* referenceImageWindow() const { return m_referenceImageWindow; }
    Document* document() const { return m_document; }
    CanvasWidget* canvasWidget() const { return m_canvasWidget; }
    QTabWidget* tabWidget() const { return m_tabWidget; }
    ColorPaletteWidget* paletteWidget() const { return m_paletteWidget; }
    QPushButton* btnTogglePalette() const { return m_btnTogglePalette; }
    QToolButton* toggleSelectionEditButton() const { return m_btnToggleSelectionEdit; }
    AiAssistantDockWidget* aiAssistantDock() const { return m_aiAssistantDock; }
    void setDocumentForTest(Document* doc) { setupDocument(doc); }
    void clearDocumentForTest() { setupDocument(nullptr); }

private:

    ToolType m_currentTool = ToolType::Pen;
    std::map<ToolType, BrushConfig> m_toolConfigs;

    QMenu* m_recentFilesMenu = nullptr;
    QStringList m_recentFiles;
    QTimer m_autoBackupTimer;

    QActionGroup* m_displayFilterActionGroup = nullptr;
    QAction* m_filterNoneAction = nullptr;
    QAction* m_filterGrayscaleAction = nullptr;
    QAction* m_filterGrayscaleYiqAction = nullptr;
    QAction* m_filterIccAction = nullptr;
    void updateDisplayFilterActions();

    QAction* m_previewWindowAction = nullptr;
    PreviewWindow* m_previewWindow = nullptr;
    QAction* m_referenceImageWindowAction = nullptr;
    ReferenceImageWindow* m_referenceImageWindow = nullptr;
    QToolButton* m_btnToggleSelectionEdit = nullptr;

    bool m_isRestoringSettings = true;
    bool m_firstShow = true;
};

} // namespace Kaburagi
