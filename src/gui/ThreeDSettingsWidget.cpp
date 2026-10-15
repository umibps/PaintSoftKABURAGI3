#include "ThreeDSettingsWidget.hpp"
#include "Document.hpp"
#include "CanvasWidget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QMenu>
#include <QFileDialog>
#include <QColorDialog>
#include <QMessageBox>
#include <QGuiApplication>
#include <QClipboard>
#include <QtMath>

namespace Kaburagi {

ThreeDSettingsWidget::ThreeDSettingsWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void ThreeDSettingsWidget::setDocument(Document* doc) {
    m_document = doc;
}

void ThreeDSettingsWidget::setCanvasWidget(CanvasWidget* canvas) {
    m_canvasWidget = canvas;
}

void ThreeDSettingsWidget::setupUi() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    // 1. モデル管理
    auto grpModel = new QGroupBox(tr("3D モデル"), this);
    auto layModel = new QVBoxLayout(grpModel);

    auto layModelBtns = new QHBoxLayout();
    m_btnOpenModel = new QPushButton(tr("📂 モデルを開く..."), grpModel);
    m_btnAddCube = new QPushButton(tr("＋ 立方体"), grpModel);
    layModelBtns->addWidget(m_btnOpenModel);
    layModelBtns->addWidget(m_btnAddCube);
    layModel->addLayout(layModelBtns);

    auto layPose = new QHBoxLayout();
    m_btnOpenPose = new QPushButton(tr("🕺 ポーズ読込..."), grpModel);
    m_comboPresetPose = new QComboBox(grpModel);
    m_comboPresetPose->addItem(tr("プリセット: 選択..."), -1);
    m_comboPresetPose->addItem(tr("Tポーズ (初期)"), 0);
    m_comboPresetPose->addItem(tr("立ちポーズ"), 1);
    m_comboPresetPose->addItem(tr("座りポーズ"), 2);
    m_comboPresetPose->addItem(tr("アクションポーズ"), 3);
    layPose->addWidget(m_btnOpenPose);
    layPose->addWidget(m_comboPresetPose);
    layModel->addLayout(layPose);

    m_treeModels = new QTreeWidget(grpModel);
    m_treeModels->setHeaderLabels({ tr("3Dオブジェクト"), tr("表示") });
    m_treeModels->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_treeModels->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_treeModels->setMaximumHeight(130);
    m_treeModels->setContextMenuPolicy(Qt::CustomContextMenu);
    layModel->addWidget(m_treeModels);
    mainLayout->addWidget(grpModel);

    connect(m_btnOpenModel, &QPushButton::clicked, this, &ThreeDSettingsWidget::onOpenModelClicked);
    connect(m_btnAddCube, &QPushButton::clicked, this, &ThreeDSettingsWidget::onAddCubeClicked);
    connect(m_btnOpenPose, &QPushButton::clicked, this, &ThreeDSettingsWidget::onOpenPoseClicked);
    connect(m_comboPresetPose, &QComboBox::currentIndexChanged, this, &ThreeDSettingsWidget::onPresetPoseChanged);
    connect(m_treeModels, &QTreeWidget::itemChanged, this, &ThreeDSettingsWidget::onModelTreeItemChanged);
    connect(m_treeModels, &QTreeWidget::currentItemChanged, this, &ThreeDSettingsWidget::onModelTreeCurrentItemChanged);
    connect(m_treeModels, &QTreeWidget::customContextMenuRequested, this, &ThreeDSettingsWidget::onModelTreeContextMenu);

    // 1-2. モデル変形 (移動 / 回転: 3D表示処理は 回転 > 移動)
    m_grpTransform = new QGroupBox(tr("モデル変形 (回転 ➔ 移動)"), this);
    auto gridTransform = new QGridLayout(m_grpTransform);
    gridTransform->setContentsMargins(6, 6, 6, 6);
    gridTransform->setHorizontalSpacing(4);
    gridTransform->setVerticalSpacing(4);

    // 移動 X, Y, Z
    gridTransform->addWidget(new QLabel(tr("移動:"), m_grpTransform), 0, 0);
    m_spinPosX = new QDoubleSpinBox(m_grpTransform);
    m_spinPosX->setRange(-100.0, 100.0);
    m_spinPosX->setSingleStep(0.1);
    m_spinPosX->setDecimals(1);
    m_spinPosX->setPrefix("X: ");
    m_spinPosY = new QDoubleSpinBox(m_grpTransform);
    m_spinPosY->setRange(-100.0, 100.0);
    m_spinPosY->setSingleStep(0.1);
    m_spinPosY->setDecimals(1);
    m_spinPosY->setPrefix("Y: ");
    m_spinPosZ = new QDoubleSpinBox(m_grpTransform);
    m_spinPosZ->setRange(-100.0, 100.0);
    m_spinPosZ->setSingleStep(0.1);
    m_spinPosZ->setDecimals(1);
    m_spinPosZ->setPrefix("Z: ");
    gridTransform->addWidget(m_spinPosX, 0, 1);
    gridTransform->addWidget(m_spinPosY, 0, 2);
    gridTransform->addWidget(m_spinPosZ, 0, 3);

    // 回転 X, Y, Z (度数法)
    gridTransform->addWidget(new QLabel(tr("回転:"), m_grpTransform), 1, 0);
    m_spinRotX = new QDoubleSpinBox(m_grpTransform);
    m_spinRotX->setRange(-360.0, 360.0);
    m_spinRotX->setSingleStep(1.0);
    m_spinRotX->setDecimals(1);
    m_spinRotX->setPrefix("X: ");
    m_spinRotX->setSuffix("°");
    m_spinRotY = new QDoubleSpinBox(m_grpTransform);
    m_spinRotY->setRange(-360.0, 360.0);
    m_spinRotY->setSingleStep(1.0);
    m_spinRotY->setDecimals(1);
    m_spinRotY->setPrefix("Y: ");
    m_spinRotY->setSuffix("°");
    m_spinRotZ = new QDoubleSpinBox(m_grpTransform);
    m_spinRotZ->setRange(-360.0, 360.0);
    m_spinRotZ->setSingleStep(1.0);
    m_spinRotZ->setDecimals(1);
    m_spinRotZ->setPrefix("Z: ");
    m_spinRotZ->setSuffix("°");
    gridTransform->addWidget(m_spinRotX, 1, 1);
    gridTransform->addWidget(m_spinRotY, 1, 2);
    gridTransform->addWidget(m_spinRotZ, 1, 3);

    m_btnResetModelTransform = new QPushButton(tr("変形をリセット"), m_grpTransform);
    gridTransform->addWidget(m_btnResetModelTransform, 2, 0, 1, 4);

    mainLayout->addWidget(m_grpTransform);

    connect(m_spinPosX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ThreeDSettingsWidget::onModelTransformChanged);
    connect(m_spinPosY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ThreeDSettingsWidget::onModelTransformChanged);
    connect(m_spinPosZ, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ThreeDSettingsWidget::onModelTransformChanged);
    connect(m_spinRotX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ThreeDSettingsWidget::onModelTransformChanged);
    connect(m_spinRotY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ThreeDSettingsWidget::onModelTransformChanged);
    connect(m_spinRotZ, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ThreeDSettingsWidget::onModelTransformChanged);
    connect(m_btnResetModelTransform, &QPushButton::clicked, this, &ThreeDSettingsWidget::onResetModelTransformClicked);

    // 2. モーション・アニメーション設定
    auto grpAnim = new QGroupBox(tr("モーション・アニメーション"), this);
    auto layAnim = new QVBoxLayout(grpAnim);

    m_btnOpenMotion = new QPushButton(tr("モーション読込 (.vrma)..."), grpAnim);
    layAnim->addWidget(m_btnOpenMotion);

    auto layControls = new QHBoxLayout();
    m_btnFirstFrame = new QPushButton(tr("|◀"), grpAnim);
    m_btnPrevFrame = new QPushButton(tr("◀"), grpAnim);
    m_btnPlayPause = new QPushButton(tr("▶ 再生"), grpAnim);
    m_btnNextFrame = new QPushButton(tr("▶"), grpAnim);
    m_btnLastFrame = new QPushButton(tr("▶|"), grpAnim);

    m_btnFirstFrame->setToolTip(tr("先頭フレームへ"));
    m_btnPrevFrame->setToolTip(tr("前フレームへ (コマ戻し)"));
    m_btnPlayPause->setToolTip(tr("再生 / 一時停止"));
    m_btnNextFrame->setToolTip(tr("次フレームへ (コマ送り)"));
    m_btnLastFrame->setToolTip(tr("最終フレームへ"));

    layControls->addWidget(m_btnFirstFrame);
    layControls->addWidget(m_btnPrevFrame);
    layControls->addWidget(m_btnPlayPause);
    layControls->addWidget(m_btnNextFrame);
    layControls->addWidget(m_btnLastFrame);
    layAnim->addLayout(layControls);

    m_sliderTimeline = new QSlider(Qt::Horizontal, grpAnim);
    m_sliderTimeline->setRange(0, 0);
    m_sliderTimeline->setValue(0);
    layAnim->addWidget(m_sliderTimeline);

    auto layInfoFps = new QHBoxLayout();
    m_lblTimeline = new QLabel(tr("0 / 0 (0.00s)"), grpAnim);
    m_comboFps = new QComboBox(grpAnim);
    m_comboFps->addItem("30 fps", 30);
    m_comboFps->addItem("24 fps", 24);
    m_comboFps->addItem("60 fps", 60);
    m_comboFps->setCurrentIndex(0);
    layInfoFps->addWidget(m_lblTimeline);
    layInfoFps->addStretch();
    layInfoFps->addWidget(new QLabel(tr("FPS:"), grpAnim));
    layInfoFps->addWidget(m_comboFps);
    layAnim->addLayout(layInfoFps);

    m_chkSpringBone = new QCheckBox(tr("物理演算(髪・服・胸の揺れもの)"), grpAnim);
    m_chkSpringBone->setChecked(true);
    layAnim->addWidget(m_chkSpringBone);

    mainLayout->addWidget(grpAnim);

    connect(m_btnOpenMotion, &QPushButton::clicked, this, &ThreeDSettingsWidget::onOpenMotionClicked);
    connect(m_btnPlayPause, &QPushButton::clicked, this, &ThreeDSettingsWidget::onPlayPauseClicked);
    connect(m_btnFirstFrame, &QPushButton::clicked, this, &ThreeDSettingsWidget::onFirstFrameClicked);
    connect(m_btnPrevFrame, &QPushButton::clicked, this, &ThreeDSettingsWidget::onPrevFrameClicked);
    connect(m_btnNextFrame, &QPushButton::clicked, this, &ThreeDSettingsWidget::onNextFrameClicked);
    connect(m_btnLastFrame, &QPushButton::clicked, this, &ThreeDSettingsWidget::onLastFrameClicked);
    connect(m_sliderTimeline, &QSlider::valueChanged, this, &ThreeDSettingsWidget::onTimelineSliderChanged);
    connect(m_comboFps, &QComboBox::currentIndexChanged, this, &ThreeDSettingsWidget::onFpsChanged);
    connect(m_chkSpringBone, &QCheckBox::toggled, this, &ThreeDSettingsWidget::onSpringBoneToggled);
    connect(&m_animTimer, &QTimer::timeout, this, &ThreeDSettingsWidget::onAnimTimerTimeout);

    // 3. カメラ設定
    auto grpCamera = new QGroupBox(tr("カメラ"), this);
    auto formCamera = new QFormLayout(grpCamera);

    auto layFov = new QHBoxLayout();
    m_sliderFov = new QSlider(Qt::Horizontal, grpCamera);
    m_sliderFov->setRange(15, 90);
    m_sliderFov->setValue(45);
    m_lblFov = new QLabel("45°", grpCamera);
    layFov->addWidget(m_sliderFov);
    layFov->addWidget(m_lblFov);

    m_btnResetCamera = new QPushButton(tr("カメラをリセット"), grpCamera);

    formCamera->addRow(tr("画角 (FOV):"), layFov);
    formCamera->addRow(m_btnResetCamera);
    mainLayout->addWidget(grpCamera);

    connect(m_sliderFov, &QSlider::valueChanged, this, &ThreeDSettingsWidget::onFovChanged);
    connect(m_btnResetCamera, &QPushButton::clicked, this, &ThreeDSettingsWidget::onResetCameraClicked);

    // 3. ライティング設定
    auto grpLight = new QGroupBox(tr("光源・ライティング"), this);
    auto formLight = new QFormLayout(grpLight);

    m_sliderLightYaw = new QSlider(Qt::Horizontal, grpLight);
    m_sliderLightYaw->setRange(-180, 180);
    m_sliderLightYaw->setValue(20);

    m_sliderLightPitch = new QSlider(Qt::Horizontal, grpLight);
    m_sliderLightPitch->setRange(-85, 85);
    m_sliderLightPitch->setValue(-25);

    m_btnLightColor = new QPushButton(tr("■ 光源色"), grpLight);

    formLight->addRow(tr("水平角 (Yaw):"), m_sliderLightYaw);
    formLight->addRow(tr("仰角 (Pitch):"), m_sliderLightPitch);
    formLight->addRow(tr("光色:"), m_btnLightColor);
    mainLayout->addWidget(grpLight);

    connect(m_sliderLightYaw, &QSlider::valueChanged, this, &ThreeDSettingsWidget::onLightAnglesChanged);
    connect(m_sliderLightPitch, &QSlider::valueChanged, this, &ThreeDSettingsWidget::onLightAnglesChanged);
    connect(m_btnLightColor, &QPushButton::clicked, this, &ThreeDSettingsWidget::onLightColorClicked);

    // 4. 描画スタイル & 輪郭線
    auto grpStyle = new QGroupBox(tr("描画スタイル & 輪郭線"), this);
    auto formStyle = new QFormLayout(grpStyle);

    m_comboShading = new QComboBox(grpStyle);
    m_comboShading->addItem(tr("セルルック (アニメ調)"), static_cast<int>(ThreeDShadingMode::Cel));
    m_comboShading->addItem(tr("スムーズ陰影"), static_cast<int>(ThreeDShadingMode::Smooth));
    m_comboShading->addItem(tr("フラット"), static_cast<int>(ThreeDShadingMode::Flat));
    m_comboShading->addItem(tr("白モデル (石膏アタリ)"), static_cast<int>(ThreeDShadingMode::WhiteModel));
    m_comboShading->addItem(tr("輪郭線のみ (線画)"), static_cast<int>(ThreeDShadingMode::LineOnly));

    m_chkUseGpu = new QCheckBox(tr("GPUハードウェア描画 (高速)"), grpStyle);
    m_chkUseGpu->setChecked(true);

    m_chkFloorGrid = new QCheckBox(tr("地面グリッドを表示"), grpStyle);
    m_chkFloorGrid->setChecked(true);

    m_chkDrawEdges = new QCheckBox(tr("輪郭線を描画"), grpStyle);
    m_chkDrawEdges->setChecked(true);

    m_spinEdgeWidth = new QDoubleSpinBox(grpStyle);
    m_spinEdgeWidth->setRange(0.5, 5.0);
    m_spinEdgeWidth->setSingleStep(0.5);
    m_spinEdgeWidth->setValue(1.5);

    m_btnEdgeColor = new QPushButton(tr("■ 輪郭線色"), grpStyle);

    formStyle->addRow(tr("シェーディング:"), m_comboShading);
    formStyle->addRow(m_chkUseGpu);
    formStyle->addRow(m_chkFloorGrid);
    formStyle->addRow(m_chkDrawEdges);
    formStyle->addRow(tr("輪郭線の太さ:"), m_spinEdgeWidth);
    formStyle->addRow(tr("線の色:"), m_btnEdgeColor);
    mainLayout->addWidget(grpStyle);

    connect(m_comboShading, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ThreeDSettingsWidget::onShadingModeChanged);
    connect(m_chkUseGpu, &QCheckBox::toggled, this, [this](bool checked) {
        auto layer = currentThreeDLayer();
        if (layer) {
            layer->setUseGpu(checked);
            if (m_document) m_document->composite();
            if (m_canvasWidget) m_canvasWidget->update();
            emit settingsChanged();
        }
    });
    connect(m_chkFloorGrid, &QCheckBox::toggled, this, &ThreeDSettingsWidget::onEdgeSettingsChanged);
    connect(m_chkDrawEdges, &QCheckBox::toggled, this, &ThreeDSettingsWidget::onEdgeSettingsChanged);
    connect(m_spinEdgeWidth, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ThreeDSettingsWidget::onEdgeSettingsChanged);
    connect(m_btnEdgeColor, &QPushButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(m_edgeColor, this, tr("輪郭線色の選択"));
        if (c.isValid()) {
            m_edgeColor = c;
            m_btnEdgeColor->setStyleSheet(QString("background-color: %1; color: %2;")
                .arg(c.name()).arg(c.lightness() > 128 ? "black" : "white"));
            onEdgeSettingsChanged();
        }
    });

    // 5. 作画連携アクション
    auto grpAction = new QGroupBox(tr("作画支援アクション"), this);
    auto layAction = new QVBoxLayout(grpAction);

    m_btnExtractContour = new QPushButton(tr("✏️ 輪郭線を新規レイヤーに抽出"), grpAction);
    m_btnExtractContour->setStyleSheet("QPushButton { font-weight: bold; padding: 5px; }");

    m_btnExtractOpenPose = new QPushButton(tr("🎯 OpenPose骨格画像を生成..."), grpAction);
    m_btnExtractOpenPose->setStyleSheet("QPushButton { font-weight: bold; padding: 5px; }");

    m_btnSyncPerspective = new QPushButton(tr("📐 消失点・パース定規を同期"), grpAction);

    layAction->addWidget(m_btnExtractContour);
    layAction->addWidget(m_btnExtractOpenPose);
    layAction->addWidget(m_btnSyncPerspective);
    mainLayout->addWidget(grpAction);

    connect(m_btnExtractContour, &QPushButton::clicked, this, &ThreeDSettingsWidget::onExtractContourClicked);
    connect(m_btnExtractOpenPose, &QPushButton::clicked, this, &ThreeDSettingsWidget::onExtractOpenPoseClicked);
    connect(m_btnSyncPerspective, &QPushButton::clicked, this, &ThreeDSettingsWidget::onSyncPerspectiveClicked);

    mainLayout->addStretch(1);
}

ThreeDLayer* ThreeDSettingsWidget::currentThreeDLayer() {
    if (m_currentLayer) return m_currentLayer;
    if (!m_document) return nullptr;
    auto l = m_document->activeLayer();
    return dynamic_cast<ThreeDLayer*>(l.get());
}

void ThreeDSettingsWidget::setFromLayer(ThreeDLayer* layer) {
    m_currentLayer = layer;
    if (!layer) return;

    blockSignals(true);

    m_sliderFov->setValue(static_cast<int>(layer->camera().fov()));
    m_lblFov->setText(QString("%1°").arg(m_sliderFov->value()));

    m_comboShading->setCurrentIndex(static_cast<int>(layer->renderConfig().shadingMode));
    m_chkUseGpu->setChecked(layer->useGpu());
    m_chkFloorGrid->setChecked(layer->renderConfig().drawFloorGrid);
    m_chkDrawEdges->setChecked(layer->renderConfig().drawEdges);
    m_spinEdgeWidth->setValue(layer->renderConfig().edgeWidth);
    m_edgeColor = layer->renderConfig().edgeColor;

    m_treeModels->blockSignals(true);
    m_treeModels->clear();
    for (size_t mIdx = 0; mIdx < layer->models().size(); ++mIdx) {
        const auto& model = layer->models()[mIdx];
        if (!model) continue;

        auto modelItem = new QTreeWidgetItem(m_treeModels);
        QString mName = model->name().isEmpty() ? QString("3D Model %1").arg(mIdx + 1) : model->name();
        modelItem->setText(0, mName);
        modelItem->setData(0, Qt::UserRole, static_cast<int>(mIdx));
        modelItem->setData(0, Qt::UserRole + 1, -1);
        modelItem->setFlags(modelItem->flags() | Qt::ItemIsUserCheckable);
        modelItem->setCheckState(0, model->isVisible() ? Qt::Checked : Qt::Unchecked);

        const auto& meshes = model->meshes();
        for (size_t meshIdx = 0; meshIdx < meshes.size(); ++meshIdx) {
            const auto& mesh = meshes[meshIdx];
            auto meshItem = new QTreeWidgetItem(modelItem);
            QString meshName = mesh.name.isEmpty() ? QString("Mesh %1").arg(meshIdx + 1) : mesh.name;
            meshItem->setText(0, meshName);
            meshItem->setData(0, Qt::UserRole, static_cast<int>(mIdx));
            meshItem->setData(0, Qt::UserRole + 1, static_cast<int>(meshIdx));
            meshItem->setFlags(meshItem->flags() | Qt::ItemIsUserCheckable);
            meshItem->setCheckState(0, mesh.visible ? Qt::Checked : Qt::Unchecked);
        }
        modelItem->setExpanded(false);
    }
    if (m_treeModels->topLevelItemCount() > 0) {
        m_treeModels->setCurrentItem(m_treeModels->topLevelItem(0));
    }
    m_treeModels->blockSignals(false);

    updateModelTransformUi();
    blockSignals(false);
}

void ThreeDSettingsWidget::onOpenModelClicked() {
    auto layer = currentThreeDLayer();
    if (!layer) {
        if (m_document) {
            auto newL = m_document->addThreeDLayer(tr("3Dレイヤー"));
            layer = newL.get();
            m_currentLayer = layer;
        } else {
            return;
        }
    }

    QString path = QFileDialog::getOpenFileName(this, tr("3Dモデルを開く"), "",
        tr("3D Models (*.obj *.gltf *.glb *.vrm *.pmx *.pmd);;Wavefront OBJ (*.obj);;glTF (*.gltf *.glb);;VRM (*.vrm);;MMD (*.pmx *.pmd);;All Files (*.*)"));

    if (path.isEmpty()) return;

    auto model = std::make_shared<ThreeDModel>();
    if (model->loadFromFile(path)) {
        if (path.endsWith(".vrm", Qt::CaseInsensitive)) {
            // VRMは標準的なメートル単位（身長約1.5〜1.8m、足元Y=0）のためスケーリング不要
        } else if (path.endsWith(".pmx", Qt::CaseInsensitive) || path.endsWith(".pmd", Qt::CaseInsensitive)) {
            // MMDモデルは単位系が大きく(高さ約15〜25)、足元を地面に保ったままVRM同等(約1.6m)にスケール
            float h = model->boundingBox().size().y();
            if (h > 5.0f) {
                float s = 1.6f / h;
                model->setScale(QVector3D(s, s, s));
            }
        } else {
            model->normalizeScale(2.0f);
        }
        layer->addModel(model);
        setFromLayer(layer);
        emit settingsChanged();
    } else {
        QMessageBox::warning(this, tr("モデル読み込みエラー"), tr("3Dモデルの読み込みに失敗しました。"));
    }
}

void ThreeDSettingsWidget::onAddCubeClicked() {
    auto layer = currentThreeDLayer();
    if (!layer && m_document) {
        auto newL = m_document->addThreeDLayer(tr("3Dレイヤー"));
        layer = newL.get();
        m_currentLayer = layer;
    }
    if (!layer) return;

    layer->addModel(ThreeDModel::createCube(1.2f));
    setFromLayer(layer);
    emit settingsChanged();
}

void ThreeDSettingsWidget::onResetCameraClicked() {
    auto layer = currentThreeDLayer();
    if (!layer) return;

    layer->camera().reset();
    m_sliderFov->setValue(static_cast<int>(layer->camera().fov()));
    layer->markDirty();
    if (m_document) m_document->composite();
    if (m_canvasWidget) m_canvasWidget->update();
    emit settingsChanged();
}

void ThreeDSettingsWidget::onFovChanged(int val) {
    m_lblFov->setText(QString("%1°").arg(val));
    auto layer = currentThreeDLayer();
    if (!layer) return;

    layer->camera().setFov(static_cast<float>(val));
    layer->markDirty();
    emit settingsChanged();
}

void ThreeDSettingsWidget::onLightAnglesChanged() {
    auto layer = currentThreeDLayer();
    if (!layer) return;

    float yawRad = qDegreesToRadians(static_cast<float>(m_sliderLightYaw->value()));
    float pitchRad = qDegreesToRadians(static_cast<float>(m_sliderLightPitch->value()));

    QVector3D dir;
    dir.setX(qCos(pitchRad) * qSin(yawRad));
    dir.setY(qSin(pitchRad));
    dir.setZ(-qCos(pitchRad) * qCos(yawRad)); // カメラ側（手前）からモデル正面（-Z奥）へ進む光

    layer->light().direction = dir.normalized();
    layer->markDirty();
    emit settingsChanged();
}

void ThreeDSettingsWidget::onLightColorClicked() {
    QColor c = QColorDialog::getColor(m_lightColor, this, tr("光源色の選択"));
    if (c.isValid()) {
        m_lightColor = c;
        m_btnLightColor->setStyleSheet(QString("background-color: %1; color: %2;")
            .arg(c.name()).arg(c.lightness() > 128 ? "black" : "white"));
        auto layer = currentThreeDLayer();
        if (layer) {
            layer->light().lightColor = c;
            layer->markDirty();
            emit settingsChanged();
        }
    }
}

void ThreeDSettingsWidget::onShadingModeChanged(int idx) {
    auto layer = currentThreeDLayer();
    if (!layer) return;

    layer->renderConfig().shadingMode = static_cast<ThreeDShadingMode>(m_comboShading->itemData(idx).toInt());
    layer->markDirty();
    emit settingsChanged();
}

void ThreeDSettingsWidget::onEdgeSettingsChanged() {
    auto layer = currentThreeDLayer();
    if (!layer) return;

    layer->renderConfig().drawFloorGrid = m_chkFloorGrid->isChecked();
    layer->renderConfig().drawEdges = m_chkDrawEdges->isChecked();
    layer->renderConfig().edgeWidth = static_cast<float>(m_spinEdgeWidth->value());
    layer->renderConfig().edgeColor = m_edgeColor;
    layer->markDirty();
    emit settingsChanged();
}

void ThreeDSettingsWidget::onExtractContourClicked() {
    auto layer = currentThreeDLayer();
    if (!layer || !m_document) return;

    QImage contour = layer->extractContourImage();
    if (!contour.isNull()) {
        m_document->addRasterLayerWithImage(contour, tr("3D輪郭線抽出"));
    }
    emit extractContourRequested();
}

void ThreeDSettingsWidget::onExtractOpenPoseClicked() {
    auto layer = currentThreeDLayer();
    if (!layer || !m_document) return;

    if (layer->models().empty()) {
        QMessageBox::information(this, tr("OpenPose骨格抽出"), tr("3Dモデルが配置されていません。"));
        return;
    }

    QImage poseImg = layer->extractOpenPoseImage(m_document->width(), m_document->height());
    if (poseImg.isNull()) {
        QMessageBox::warning(this, tr("OpenPose骨格抽出"), tr("OpenPose骨格画像の生成に失敗しました。"));
        return;
    }

    QMenu menu(this);
    auto actLayer = menu.addAction(tr("新規通常レイヤーとして追加"));
    auto actClipboard = menu.addAction(tr("クリップボードにコピー"));
    auto actSave = menu.addAction(tr("画像ファイルとして保存..."));

    QAction* selected = menu.exec(QCursor::pos());
    if (!selected || selected == actLayer) {
        m_document->addRasterLayerWithImage(poseImg, tr("OpenPose骨格"));
    } else if (selected == actClipboard) {
        QGuiApplication::clipboard()->setImage(poseImg);
        QMessageBox::information(this, tr("OpenPose"), tr("OpenPose画像をクリップボードにコピーしました。"));
    } else if (selected == actSave) {
        QString file = QFileDialog::getSaveFileName(this, tr("OpenPose骨格画像を保存"), "openpose.png", tr("PNG画像 (*.png)"));
        if (!file.isEmpty()) {
            poseImg.save(file);
        }
    }
}

void ThreeDSettingsWidget::onSyncPerspectiveClicked() {
    auto layer = currentThreeDLayer();
    if (!layer || !m_document) return;

    float aspect = static_cast<float>(m_document->width()) / std::max(1, m_document->height());
    QPointF vpX, vpY, vpZ;
    layer->camera().calculateVanishingPoints(aspect, m_document->width(), m_document->height(), vpX, vpY, vpZ);
    float eyeLevel = layer->camera().calculateEyeLevelY(aspect, m_document->height());

    emit syncPerspectiveRequested();
}

void ThreeDSettingsWidget::onOpenPoseClicked() {
    auto layer = currentThreeDLayer();
    if (!layer || layer->models().empty()) {
        QMessageBox::information(this, tr("ポーズ読込"), tr("先に3Dモデル（VRMまたはMMDモデル）を読み込んでください。"));
        return;
    }

    QString path = QFileDialog::getOpenFileName(this, tr("ポーズ・モーションを開く"), "",
        tr("Pose & Motion (*.vroidpose *.vpd *.vmd);;VRoid Pose (*.vroidpose);;MMD Pose (*.vpd);;MMD Motion (*.vmd);;All Files (*.*)"));
    if (path.isEmpty()) return;

    ThreeDPose pose;
    bool ok = false;
    if (path.endsWith(".vroidpose", Qt::CaseInsensitive)) {
        ok = pose.loadVroidPose(path);
    } else if (path.endsWith(".vpd", Qt::CaseInsensitive)) {
        ok = pose.loadVpd(path);
    } else if (path.endsWith(".vmd", Qt::CaseInsensitive)) {
        ok = pose.loadVmd(path, 0);
    }

    if (ok) {
        for (auto& model : layer->models()) {
            model->applyPose(pose);
        }
        layer->markDirty();
        emit settingsChanged();
    } else {
        QMessageBox::warning(this, tr("ポーズ読込エラー"), tr("ポーズデータの読み込みに失敗しました。"));
    }
}

void ThreeDSettingsWidget::onPresetPoseChanged(int idx) {
    int poseType = m_comboPresetPose->itemData(idx).toInt();
    if (poseType < 0) return;

    auto layer = currentThreeDLayer();
    if (!layer || layer->models().empty()) return;

    ThreeDPose pose;
    switch (poseType) {
    case 0: pose = ThreeDPose::createTPose(); break;
    case 1: pose = ThreeDPose::createStandingPose(); break;
    case 2: pose = ThreeDPose::createSittingPose(); break;
    case 3: pose = ThreeDPose::createActionPose(); break;
    default: break;
    }

    for (auto& model : layer->models()) {
        model->applyPose(pose);
    }
    layer->markDirty();
    emit settingsChanged();
}

void ThreeDSettingsWidget::onModelTreeItemChanged(QTreeWidgetItem* item, int column) {
    if (column != 0 || !item) return;
    auto layer = currentThreeDLayer();
    if (!layer) return;

    int modelIdx = item->data(0, Qt::UserRole).toInt();
    int meshIdx = item->data(0, Qt::UserRole + 1).toInt();

    if (modelIdx < 0 || modelIdx >= static_cast<int>(layer->models().size())) return;
    auto model = layer->models()[modelIdx];
    if (!model) return;

    bool isChecked = (item->checkState(0) == Qt::Checked);

    if (meshIdx == -1) {
        // モデル全体の表示/非表示
        model->setVisible(isChecked);
        // 子アイテム（メッシュ）のチェック状態も同期
        m_treeModels->blockSignals(true);
        for (int i = 0; i < item->childCount(); ++i) {
            auto child = item->child(i);
            child->setCheckState(0, isChecked ? Qt::Checked : Qt::Unchecked);
            int cMeshIdx = child->data(0, Qt::UserRole + 1).toInt();
            if (cMeshIdx >= 0 && cMeshIdx < static_cast<int>(model->meshes().size())) {
                model->meshes()[cMeshIdx].visible = isChecked;
            }
        }
        m_treeModels->blockSignals(false);
    } else {
        // 特定メッシュの表示/非表示
        if (meshIdx >= 0 && meshIdx < static_cast<int>(model->meshes().size())) {
            model->meshes()[meshIdx].visible = isChecked;
        }
    }

    layer->markDirty();
    if (m_document) m_document->composite();
    emit settingsChanged();
}

void ThreeDSettingsWidget::onModelTreeContextMenu(const QPoint& pos) {
    auto layer = currentThreeDLayer();
    if (!layer) return;

    QTreeWidgetItem* item = m_treeModels->itemAt(pos);
    if (!item) return;

    int modelIdx = item->data(0, Qt::UserRole).toInt();
    if (modelIdx < 0 || modelIdx >= static_cast<int>(layer->models().size())) return;

    auto menu = new QMenu(this);
    QString delText = (item->data(0, Qt::UserRole + 1).toInt() == -1)
        ? tr("モデル「%1」を削除").arg(item->text(0))
        : tr("親モデルを削除");

    menu->addAction(delText, this, [this, layer, modelIdx]() {
        layer->removeModel(modelIdx);
        setFromLayer(layer);
        if (m_document) m_document->composite();
        emit settingsChanged();
    });

    menu->exec(m_treeModels->viewport()->mapToGlobal(pos));
    delete menu;
}

void ThreeDSettingsWidget::updateTimelineLabel() {
    if (!m_lblTimeline) return;
    if (!m_currentAnimation) {
        m_lblTimeline->setText(tr("0 / 0 (0.00s)"));
        return;
    }
    int curFrame = std::round(m_currentTime * m_fps);
    int maxFrame = std::round(m_currentAnimation->duration * m_fps);
    m_lblTimeline->setText(QString("%1 / %2 (%3s / %4s)")
        .arg(curFrame)
        .arg(maxFrame)
        .arg(m_currentTime, 0, 'f', 2)
        .arg(m_currentAnimation->duration, 0, 'f', 2));
}

void ThreeDSettingsWidget::onOpenMotionClicked() {
    QString path = QFileDialog::getOpenFileName(
        this,
        tr("モーション読み込み"),
        QString(),
        tr("モーションファイル (*.vrma *.vmd);;VRoidモーション (*.vrma);;MMDモーション (*.vmd);;All Files (*.*)")
    );
    if (path.isEmpty()) return;

    std::shared_ptr<ThreeDAnimation> anim;
    if (path.endsWith(".vmd", Qt::CaseInsensitive)) {
        anim = ThreeDAnimation::loadVmd(path);
    } else {
        anim = ThreeDAnimation::loadVrma(path);
    }

    if (!anim || anim->isEmpty()) {
        QMessageBox::warning(this, tr("エラー"), tr("モーションファイルの読み込みに失敗しました。"));
        return;
    }

    if (m_isPlaying) {
        onPlayPauseClicked(); // 一時停止
    }

    m_currentAnimation = anim;
    m_currentTime = 0.0f;

    int maxFrame = std::round(anim->duration * m_fps);
    m_sliderTimeline->blockSignals(true);
    m_sliderTimeline->setRange(0, maxFrame);
    m_sliderTimeline->setValue(0);
    m_sliderTimeline->blockSignals(false);

    updateTimelineLabel();

    auto layer = currentThreeDLayer();
    if (layer) {
        ThreeDPose pose;
        m_currentAnimation->evaluate(0.0f, pose);
        for (auto& mdl : layer->models()) {
            mdl->applyPose(pose);
            mdl->resetSpringBones();
        }
        layer->markDirty();
        if (m_document) m_document->composite();
        if (m_canvasWidget) m_canvasWidget->update();
        emit settingsChanged();
    }
}

void ThreeDSettingsWidget::onPlayPauseClicked() {
    if (m_isPlaying) {
        m_isPlaying = false;
        m_animTimer.stop();
        m_btnPlayPause->setText(tr("▶ 再生"));
    } else {
        if (!m_currentAnimation) return;
        m_isPlaying = true;
        m_btnPlayPause->setText(tr("❚❚ 停止"));
        m_elapsedTimer.start();
        m_lastElapsedMs = 0;
        m_animTimer.start(1000 / m_fps);
    }
}

void ThreeDSettingsWidget::onFirstFrameClicked() {
    if (!m_currentAnimation) return;
    if (m_isPlaying) onPlayPauseClicked();
    m_currentTime = 0.0f;
    m_sliderTimeline->blockSignals(true);
    m_sliderTimeline->setValue(0);
    m_sliderTimeline->blockSignals(false);
    updateTimelineLabel();

    auto layer = currentThreeDLayer();
    if (layer) {
        ThreeDPose pose;
        m_currentAnimation->evaluate(m_currentTime, pose);
        for (auto& mdl : layer->models()) {
            mdl->applyPose(pose);
            mdl->resetSpringBones();
        }
        layer->markDirty();
        if (m_document) m_document->composite();
        if (m_canvasWidget) m_canvasWidget->update();
        emit settingsChanged();
    }
}

void ThreeDSettingsWidget::onLastFrameClicked() {
    if (!m_currentAnimation) return;
    if (m_isPlaying) onPlayPauseClicked();
    m_currentTime = m_currentAnimation->duration;
    int maxFrame = std::round(m_currentAnimation->duration * m_fps);
    m_sliderTimeline->blockSignals(true);
    m_sliderTimeline->setValue(maxFrame);
    m_sliderTimeline->blockSignals(false);
    updateTimelineLabel();

    auto layer = currentThreeDLayer();
    if (layer) {
        ThreeDPose pose;
        m_currentAnimation->evaluate(m_currentTime, pose);
        for (auto& mdl : layer->models()) {
            mdl->applyPose(pose);
            mdl->resetSpringBones();
        }
        layer->markDirty();
        if (m_document) m_document->composite();
        if (m_canvasWidget) m_canvasWidget->update();
        emit settingsChanged();
    }
}

void ThreeDSettingsWidget::onPrevFrameClicked() {
    if (!m_currentAnimation) return;
    if (m_isPlaying) onPlayPauseClicked();
    int curFrame = std::round(m_currentTime * m_fps);
    curFrame = std::max(0, curFrame - 1);
    m_currentTime = curFrame / static_cast<float>(m_fps);
    m_sliderTimeline->blockSignals(true);
    m_sliderTimeline->setValue(curFrame);
    m_sliderTimeline->blockSignals(false);
    updateTimelineLabel();

    auto layer = currentThreeDLayer();
    if (layer) {
        ThreeDPose pose;
        m_currentAnimation->evaluate(m_currentTime, pose);
        float dt = 1.0f / m_fps;
        bool updateCpuSkin = !layer->useGpu();
        for (auto& mdl : layer->models()) {
            mdl->applyPose(pose, updateCpuSkin);
            if (m_enableSpringBone) {
                mdl->updateSpringBones(dt, updateCpuSkin);
            }
        }
        layer->markDirty();
        if (m_document) m_document->composite();
        if (m_canvasWidget) m_canvasWidget->update();
        emit settingsChanged();
    }
}

void ThreeDSettingsWidget::onNextFrameClicked() {
    if (!m_currentAnimation) return;
    if (m_isPlaying) onPlayPauseClicked();
    int maxFrame = std::round(m_currentAnimation->duration * m_fps);
    int curFrame = std::round(m_currentTime * m_fps);
    curFrame = std::min(maxFrame, curFrame + 1);
    m_currentTime = curFrame / static_cast<float>(m_fps);
    m_sliderTimeline->blockSignals(true);
    m_sliderTimeline->setValue(curFrame);
    m_sliderTimeline->blockSignals(false);
    updateTimelineLabel();

    auto layer = currentThreeDLayer();
    if (layer) {
        ThreeDPose pose;
        m_currentAnimation->evaluate(m_currentTime, pose);
        float dt = 1.0f / m_fps;
        bool updateCpuSkin = !layer->useGpu();
        for (auto& mdl : layer->models()) {
            mdl->applyPose(pose, updateCpuSkin);
            if (m_enableSpringBone) {
                mdl->updateSpringBones(dt, updateCpuSkin);
            }
        }
        layer->markDirty();
        if (m_document) m_document->composite();
        if (m_canvasWidget) m_canvasWidget->update();
        emit settingsChanged();
    }
}

void ThreeDSettingsWidget::onTimelineSliderChanged(int value) {
    if (!m_currentAnimation) return;
    m_currentTime = value / static_cast<float>(m_fps);
    updateTimelineLabel();

    auto layer = currentThreeDLayer();
    if (layer) {
        ThreeDPose pose;
        m_currentAnimation->evaluate(m_currentTime, pose);
        bool updateCpuSkin = !layer->useGpu();
        for (auto& mdl : layer->models()) {
            mdl->applyPose(pose, updateCpuSkin);
            if (m_enableSpringBone) {
                mdl->resetSpringBones();
            }
        }
        layer->markDirty();
        if (m_document) m_document->composite();
        if (m_canvasWidget) m_canvasWidget->update();
        emit settingsChanged();
    }
}

void ThreeDSettingsWidget::onFpsChanged(int idx) {
    Q_UNUSED(idx);
    m_fps = m_comboFps->currentData().toInt();
    if (m_fps <= 0) m_fps = 30;

    if (m_isPlaying) {
        m_animTimer.setInterval(1000 / m_fps);
    }

    if (m_currentAnimation) {
        int maxFrame = std::round(m_currentAnimation->duration * m_fps);
        int curFrame = std::round(m_currentTime * m_fps);
        m_sliderTimeline->blockSignals(true);
        m_sliderTimeline->setRange(0, maxFrame);
        m_sliderTimeline->setValue(curFrame);
        m_sliderTimeline->blockSignals(false);
        updateTimelineLabel();
    }
}

void ThreeDSettingsWidget::onSpringBoneToggled(bool checked) {
    m_enableSpringBone = checked;
    if (checked) {
        auto layer = currentThreeDLayer();
        if (layer) {
            for (auto& mdl : layer->models()) {
                mdl->resetSpringBones();
            }
        }
    }
}

void ThreeDSettingsWidget::onAnimTimerTimeout() {
    if (!m_currentAnimation) return;

    qint64 currentElapsedMs = m_elapsedTimer.elapsed();
    qint64 deltaMs = currentElapsedMs - m_lastElapsedMs;
    m_lastElapsedMs = currentElapsedMs;

    float dt = deltaMs / 1000.0f;
    if (dt <= 0.0f) return;
    if (dt > 0.5f) dt = 0.5f; // スパイク対策

    m_currentTime += dt;
    if (m_currentTime > m_currentAnimation->duration) {
        m_currentTime = std::fmod(m_currentTime, m_currentAnimation->duration);
        m_elapsedTimer.restart();
        m_lastElapsedMs = 0;
    }

    int curFrame = std::round(m_currentTime * m_fps);
    m_sliderTimeline->blockSignals(true);
    m_sliderTimeline->setValue(curFrame);
    m_sliderTimeline->blockSignals(false);
    updateTimelineLabel();

    auto layer = currentThreeDLayer();
    if (layer) {
        ThreeDPose pose;
        m_currentAnimation->evaluate(m_currentTime, pose);
        bool updateCpuSkin = !layer->useGpu();
        for (auto& mdl : layer->models()) {
            mdl->applyPose(pose, updateCpuSkin);
            if (m_enableSpringBone) {
                // SpringBone Verlet積分を最大約33ms単位のサブステップで安全に更新
                float remainingDt = dt;
                while (remainingDt > 0.001f) {
                    float stepDt = std::min(remainingDt, 0.0333f);
                    mdl->updateSpringBones(stepDt, updateCpuSkin);
                    remainingDt -= stepDt;
                }
            }
        }
        layer->markDirty();
        if (m_document) m_document->composite();
        if (m_canvasWidget) m_canvasWidget->update();
        emit settingsChanged();
    }
}

std::shared_ptr<ThreeDModel> ThreeDSettingsWidget::selectedModel() {
    auto layer = currentThreeDLayer();
    if (!layer || layer->models().empty()) return nullptr;

    auto currentItem = m_treeModels->currentItem();
    if (currentItem) {
        int modelIdx = currentItem->data(0, Qt::UserRole).toInt();
        if (modelIdx >= 0 && modelIdx < static_cast<int>(layer->models().size())) {
            return layer->models()[modelIdx];
        }
    }
    return layer->models()[0];
}

void ThreeDSettingsWidget::updateModelTransformUi() {
    auto model = selectedModel();
    m_updatingTransformUi = true;
    if (!model) {
        m_grpTransform->setEnabled(false);
        m_spinPosX->setValue(0.0);
        m_spinPosY->setValue(0.0);
        m_spinPosZ->setValue(0.0);
        m_spinRotX->setValue(0.0);
        m_spinRotY->setValue(0.0);
        m_spinRotZ->setValue(0.0);
    } else {
        m_grpTransform->setEnabled(true);
        const auto& pos = model->position();
        const auto& rot = model->rotationEuler();
        m_spinPosX->setValue(pos.x());
        m_spinPosY->setValue(pos.y());
        m_spinPosZ->setValue(pos.z());
        m_spinRotX->setValue(rot.x());
        m_spinRotY->setValue(rot.y());
        m_spinRotZ->setValue(rot.z());
    }
    m_updatingTransformUi = false;
}

void ThreeDSettingsWidget::onModelTreeCurrentItemChanged(QTreeWidgetItem* /*current*/, QTreeWidgetItem* /*previous*/) {
    updateModelTransformUi();
}

void ThreeDSettingsWidget::onModelTransformChanged() {
    if (m_updatingTransformUi) return;
    auto model = selectedModel();
    auto layer = currentThreeDLayer();
    if (!model || !layer) return;

    QVector3D pos(m_spinPosX->value(), m_spinPosY->value(), m_spinPosZ->value());
    QVector3D rot(m_spinRotX->value(), m_spinRotY->value(), m_spinRotZ->value());

    model->setPosition(pos);
    model->setRotationEuler(rot);

    layer->markDirty();
    if (m_document) m_document->composite();
    if (m_canvasWidget) m_canvasWidget->update();
    emit settingsChanged();
}

void ThreeDSettingsWidget::onResetModelTransformClicked() {
    auto model = selectedModel();
    auto layer = currentThreeDLayer();
    if (!model || !layer) return;

    m_updatingTransformUi = true;
    model->setPosition(QVector3D(0.0f, 0.0f, 0.0f));
    model->setRotationEuler(QVector3D(0.0f, 0.0f, 0.0f));
    m_spinPosX->setValue(0.0);
    m_spinPosY->setValue(0.0);
    m_spinPosZ->setValue(0.0);
    m_spinRotX->setValue(0.0);
    m_spinRotY->setValue(0.0);
    m_spinRotZ->setValue(0.0);
    m_updatingTransformUi = false;

    layer->markDirty();
    if (m_document) m_document->composite();
    if (m_canvasWidget) m_canvasWidget->update();
    emit settingsChanged();
}

} // namespace Kaburagi
