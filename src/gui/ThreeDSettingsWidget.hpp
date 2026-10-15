#pragma once

#include <QWidget>
#include <memory>
#include "ThreeDLayer.hpp"

#include <QTimer>
#include <QElapsedTimer>
#include "ThreeDAnimation.hpp"

class QComboBox;
class QSlider;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QPushButton;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;
class QGroupBox;

namespace Kaburagi {

class Document;
class CanvasWidget;

class ThreeDSettingsWidget : public QWidget {
    Q_OBJECT

public:
    explicit ThreeDSettingsWidget(QWidget* parent = nullptr);
    ~ThreeDSettingsWidget() override = default;

    void setDocument(Document* doc);
    void setCanvasWidget(CanvasWidget* canvas);
    void setFromLayer(ThreeDLayer* layer);

signals:
    void settingsChanged();
    void extractContourRequested();
    void syncPerspectiveRequested();

private slots:
    void onOpenModelClicked();
    void onAddCubeClicked();
    void onResetCameraClicked();
    void onFovChanged(int val);
    void onLightAnglesChanged();
    void onLightColorClicked();
    void onShadingModeChanged(int idx);
    void onEdgeSettingsChanged();
    void onExtractContourClicked();
    void onExtractOpenPoseClicked();
    void onSyncPerspectiveClicked();
    void onOpenPoseClicked();
    void onPresetPoseChanged(int idx);
    void onModelTreeItemChanged(QTreeWidgetItem* item, int column);
    void onModelTreeCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void onModelTreeContextMenu(const QPoint& pos);
    void onModelTransformChanged();
    void onResetModelTransformClicked();

    // アニメーション関連
    void onOpenMotionClicked();
    void onPlayPauseClicked();
    void onFirstFrameClicked();
    void onPrevFrameClicked();
    void onNextFrameClicked();
    void onLastFrameClicked();
    void onTimelineSliderChanged(int value);
    void onFpsChanged(int idx);
    void onSpringBoneToggled(bool checked);
    void onAnimTimerTimeout();

private:
    void setupUi();
    ThreeDLayer* currentThreeDLayer();
    std::shared_ptr<ThreeDModel> selectedModel();
    void updateModelTransformUi();
    void updateTimelineLabel();

private:
    Document* m_document = nullptr;
    CanvasWidget* m_canvasWidget = nullptr;
    ThreeDLayer* m_currentLayer = nullptr;

    // UI
    QPushButton* m_btnOpenModel = nullptr;
    QPushButton* m_btnAddCube = nullptr;
    QPushButton* m_btnOpenPose = nullptr;
    QComboBox* m_comboPresetPose = nullptr;
    QTreeWidget* m_treeModels = nullptr;

    // モデル変形 (移動 / 回転)
    QGroupBox* m_grpTransform = nullptr;
    QDoubleSpinBox* m_spinPosX = nullptr;
    QDoubleSpinBox* m_spinPosY = nullptr;
    QDoubleSpinBox* m_spinPosZ = nullptr;
    QDoubleSpinBox* m_spinRotX = nullptr;
    QDoubleSpinBox* m_spinRotY = nullptr;
    QDoubleSpinBox* m_spinRotZ = nullptr;
    QPushButton* m_btnResetModelTransform = nullptr;
    bool m_updatingTransformUi = false;

    // アニメーションUI
    QPushButton* m_btnOpenMotion = nullptr;
    QPushButton* m_btnPlayPause = nullptr;
    QPushButton* m_btnFirstFrame = nullptr;
    QPushButton* m_btnPrevFrame = nullptr;
    QPushButton* m_btnNextFrame = nullptr;
    QPushButton* m_btnLastFrame = nullptr;
    QSlider* m_sliderTimeline = nullptr;
    QLabel* m_lblTimeline = nullptr;
    QComboBox* m_comboFps = nullptr;
    QCheckBox* m_chkSpringBone = nullptr;

    // アニメーション内部状態
    QTimer m_animTimer;
    QElapsedTimer m_elapsedTimer;
    qint64 m_lastElapsedMs = 0;
    std::shared_ptr<ThreeDAnimation> m_currentAnimation;
    float m_currentTime = 0.0f;
    int m_fps = 30;
    bool m_isPlaying = false;
    bool m_enableSpringBone = true;

    QSlider* m_sliderFov = nullptr;
    QLabel* m_lblFov = nullptr;
    QPushButton* m_btnResetCamera = nullptr;

    QSlider* m_sliderLightYaw = nullptr;
    QSlider* m_sliderLightPitch = nullptr;
    QPushButton* m_btnLightColor = nullptr;
    QColor m_lightColor = Qt::white;

    QComboBox* m_comboShading = nullptr;
    QCheckBox* m_chkUseGpu = nullptr;
    QCheckBox* m_chkFloorGrid = nullptr;
    QCheckBox* m_chkDrawEdges = nullptr;
    QDoubleSpinBox* m_spinEdgeWidth = nullptr;
    QPushButton* m_btnEdgeColor = nullptr;
    QColor m_edgeColor = Qt::black;

    QPushButton* m_btnExtractContour = nullptr;
    QPushButton* m_btnExtractOpenPose = nullptr;
    QPushButton* m_btnSyncPerspective = nullptr;
};

} // namespace Kaburagi
