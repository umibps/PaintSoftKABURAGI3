#include "FilterDialogs.hpp"
#include "UndoCommands.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QColorDialog>

namespace Kaburagi {

// ----------------------------------------------------------------------------
// FilterBaseDialog 基底クラス
// ----------------------------------------------------------------------------
FilterBaseDialog::FilterBaseDialog(Document* doc, CanvasWidget* canvas, const QString& title, QWidget* parent)
    : QDialog(parent)
    , m_document(doc)
    , m_canvas(canvas)
{
    setWindowTitle(title);
    setModal(true);

    if (!m_document) return;

    // 1. 対象レイヤーの収集 (アクティブレイヤー + ピン留めレイヤー)
    std::vector<int> targetIndices;
    int activeIdx = m_document->activeLayerIndex();
    auto activeLayer = m_document->activeLayer();
    if (activeLayer && activeLayer->type() == LayerType::Raster) {
        targetIndices.push_back(activeIdx);
    }

    const auto& layers = m_document->layers();
    for (size_t i = 0; i < layers.size(); ++i) {
        int idx = static_cast<int>(i);
        if (idx == activeIdx) continue;
        const auto& l = layers[i];
        if (l && l->isPinned() && l->type() == LayerType::Raster) {
            targetIndices.push_back(idx);
        }
    }

    // 2. 元画像のバックアップ作成
    for (int idx : targetIndices) {
        auto l = m_document->getLayer(idx);
        if (l) {
            TargetLayerInfo info;
            info.layerIndex = idx;
            info.layer = l;
            info.originalTiles = l->tileManager().clone();
            info.originalImage = l->tileManager().toQImage();
            m_targets.push_back(std::move(info));
        }
    }
}

FilterBaseDialog::~FilterBaseDialog() {
}

void FilterBaseDialog::setupBaseUi(QWidget* controlsWidget) {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    if (controlsWidget) {
        mainLayout->addWidget(controlsWidget);
    }

    auto bottomLayout = new QHBoxLayout();
    m_chkPreview = new QCheckBox(tr("Preview (Canvas)"), this);
    m_chkPreview->setChecked(true);
    bottomLayout->addWidget(m_chkPreview);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttonBox->button(QDialogButtonBox::Ok)->setText(tr("OK"));
    m_buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    bottomLayout->addWidget(m_buttonBox);

    mainLayout->addLayout(bottomLayout);

    connect(m_chkPreview, &QCheckBox::toggled, this, &FilterBaseDialog::updatePreview);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &FilterBaseDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &FilterBaseDialog::reject);
}

void FilterBaseDialog::updatePreview() {
    if (!m_document || !m_canvas || m_isApplying || m_targets.empty()) return;
    m_isApplying = true;

    bool preview = m_chkPreview && m_chkPreview->isChecked();

    for (auto& info : m_targets) {
        if (!info.layer) continue;

        if (!preview) {
            // プレビューOFF: 元のタイルを復元
            info.layer->tileManager() = info.originalTiles.clone();
            info.layer->invalidateContentBounds();
        } else {
            // プレビューON: フィルターを適用
            QImage filtered;
            applyFilterToImage(info.originalImage, filtered);

            // 選択範囲がある場合は選択範囲内のみブレンド
            if (m_document->hasSelection()) {
                if (m_document->selectionMaskLayer()) {
                    FilterProcessor::blendWithSelectionMask(filtered, info.originalImage,
                                                           m_document->selectionMaskLayer().get(),
                                                           m_document->width(), m_document->height());
                } else {
                    FilterProcessor::blendWithSelection(filtered, info.originalImage,
                                                        m_document->selection(),
                                                        m_document->width(), m_document->height());
                }
            }

            info.layer->tileManager().fromQImage(filtered);
            info.layer->invalidateContentBounds();
        }
    }

    m_document->composite();
    m_canvas->update();
    m_isApplying = false;
}

void FilterBaseDialog::accept() {
    if (!m_document || m_targets.empty()) {
        QDialog::accept();
        return;
    }

    // 各レイヤーの postTiles を収集
    std::vector<MultiLayerFilterCommand::Entry> entries;
    for (auto& info : m_targets) {
        if (!info.layer) continue;

        // プレビュー状態によらず最終的なフィルター適用後の画像を確実に生成・適用
        QImage filtered;
        applyFilterToImage(info.originalImage, filtered);

        // 選択範囲がある場合は選択範囲内のみブレンド
        if (m_document->hasSelection()) {
            if (m_document->selectionMaskLayer()) {
                FilterProcessor::blendWithSelectionMask(filtered, info.originalImage,
                                                       m_document->selectionMaskLayer().get(),
                                                       m_document->width(), m_document->height());
            } else {
                FilterProcessor::blendWithSelection(filtered, info.originalImage,
                                                    m_document->selection(),
                                                    m_document->width(), m_document->height());
            }
        }

        info.layer->tileManager().fromQImage(filtered);
        info.layer->invalidateContentBounds();

        MultiLayerFilterCommand::Entry entry;
        entry.layerIndex = info.layerIndex;
        entry.preTiles = info.originalTiles.clone();
        entry.postTiles = info.layer->tileManager().clone();
        entry.dirtyRect = QRect(0, 0, m_document->width(), m_document->height());
        entries.push_back(std::move(entry));
    }

    m_document->composite();

    if (m_document->undoStack()) {
        auto cmd = new MultiLayerFilterCommand(m_document, filterName(), std::move(entries));
        m_document->undoStack()->push(cmd);
    }

    if (m_canvas) m_canvas->update();
    QDialog::accept();
}

void FilterBaseDialog::reject() {
    if (m_document && !m_targets.empty()) {
        for (auto& info : m_targets) {
            if (info.layer) {
                info.layer->tileManager() = info.originalTiles.clone();
                info.layer->invalidateContentBounds();
            }
        }
        m_document->composite();
        if (m_canvas) m_canvas->update();
    }
    QDialog::reject();
}

// ----------------------------------------------------------------------------
// 1. GaussianBlurDialog
// ----------------------------------------------------------------------------
GaussianBlurDialog::GaussianBlurDialog(Document* doc, CanvasWidget* canvas, QWidget* parent)
    : FilterBaseDialog(doc, canvas, tr("Gaussian Blur"), parent)
{
    auto controls = new QWidget(this);
    auto form = new QFormLayout(controls);

    auto layRadius = new QHBoxLayout();
    m_sliderRadius = new QSlider(Qt::Horizontal, controls);
    m_sliderRadius->setRange(1, 100);
    m_sliderRadius->setValue(10);

    m_spinRadius = new QSpinBox(controls);
    m_spinRadius->setRange(1, 100);
    m_spinRadius->setValue(10);
    m_spinRadius->setSuffix(" px");

    layRadius->addWidget(m_sliderRadius);
    layRadius->addWidget(m_spinRadius);

    form->addRow(tr("Blur Radius:"), layRadius);

    connect(m_sliderRadius, &QSlider::valueChanged, m_spinRadius, &QSpinBox::setValue);
    connect(m_spinRadius, QOverload<int>::of(&QSpinBox::valueChanged), m_sliderRadius, &QSlider::setValue);
    connect(m_sliderRadius, &QSlider::valueChanged, this, &FilterBaseDialog::updatePreview);

    setupBaseUi(controls);
    resize(320, 120);

    // 初回プレビュー更新
    updatePreview();
}

void GaussianBlurDialog::applyFilterToImage(const QImage& src, QImage& out) {
    int r = m_sliderRadius ? m_sliderRadius->value() : 10;
    out = FilterProcessor::applyGaussianBlur(src, r);
}

// ----------------------------------------------------------------------------
// 2. FocusBlurDialog
// ----------------------------------------------------------------------------
FocusBlurDialog::FocusBlurDialog(Document* doc, CanvasWidget* canvas, QWidget* parent)
    : FilterBaseDialog(doc, canvas, tr("Focus Blur"), parent)
{
    auto controls = new QWidget(this);
    auto form = new QFormLayout(controls);

    // ぼかし強度
    auto layBlur = new QHBoxLayout();
    m_sliderBlurRadius = new QSlider(Qt::Horizontal, controls);
    m_sliderBlurRadius->setRange(1, 50);
    m_sliderBlurRadius->setValue(15);
    m_spinBlurRadius = new QSpinBox(controls);
    m_spinBlurRadius->setRange(1, 50);
    m_spinBlurRadius->setValue(15);
    m_spinBlurRadius->setSuffix(" px");
    layBlur->addWidget(m_sliderBlurRadius);
    layBlur->addWidget(m_spinBlurRadius);
    form->addRow(tr("Blur Strength:"), layBlur);

    // フォーカス範囲
    auto layFocus = new QHBoxLayout();
    m_sliderFocusRange = new QSlider(Qt::Horizontal, controls);
    m_sliderFocusRange->setRange(5, 95);
    m_sliderFocusRange->setValue(30);
    m_spinFocusRange = new QSpinBox(controls);
    m_spinFocusRange->setRange(5, 95);
    m_spinFocusRange->setValue(30);
    m_spinFocusRange->setSuffix(" %");
    layFocus->addWidget(m_sliderFocusRange);
    layFocus->addWidget(m_spinFocusRange);
    form->addRow(tr("Focus Range:"), layFocus);

    // ぼかし遷移幅 (境界のなめらかさ)
    auto layFeather = new QHBoxLayout();
    m_sliderFeather = new QSlider(Qt::Horizontal, controls);
    m_sliderFeather->setRange(5, 80);
    m_sliderFeather->setValue(25);
    m_spinFeather = new QSpinBox(controls);
    m_spinFeather->setRange(5, 80);
    m_spinFeather->setValue(25);
    m_spinFeather->setSuffix(" %");
    layFeather->addWidget(m_sliderFeather);
    layFeather->addWidget(m_spinFeather);
    form->addRow(tr("Blur Transition:"), layFeather);

    // タイプ (放射状 / 水平チルトシフト)
    m_comboType = new QComboBox(controls);
    m_comboType->addItem(tr("Radial (Circular Focus)"), 0);
    m_comboType->addItem(tr("Linear (Tilt-Shift / Band)"), 1);
    form->addRow(tr("Focus Shape:"), m_comboType);

    // フォーカス中心 (X, Y)
    m_sliderCenterX = new QSlider(Qt::Horizontal, controls);
    m_sliderCenterX->setRange(0, 100);
    m_sliderCenterX->setValue(50);
    form->addRow(tr("Center X (%):"), m_sliderCenterX);

    m_sliderCenterY = new QSlider(Qt::Horizontal, controls);
    m_sliderCenterY->setRange(0, 100);
    m_sliderCenterY->setValue(50);
    form->addRow(tr("Center Y (%):"), m_sliderCenterY);

    connect(m_sliderBlurRadius, &QSlider::valueChanged, m_spinBlurRadius, &QSpinBox::setValue);
    connect(m_spinBlurRadius, QOverload<int>::of(&QSpinBox::valueChanged), m_sliderBlurRadius, &QSlider::setValue);

    connect(m_sliderFocusRange, &QSlider::valueChanged, m_spinFocusRange, &QSpinBox::setValue);
    connect(m_spinFocusRange, QOverload<int>::of(&QSpinBox::valueChanged), m_sliderFocusRange, &QSlider::setValue);

    connect(m_sliderFeather, &QSlider::valueChanged, m_spinFeather, &QSpinBox::setValue);
    connect(m_spinFeather, QOverload<int>::of(&QSpinBox::valueChanged), m_sliderFeather, &QSlider::setValue);

    auto triggerPreview = [this]() { updatePreview(); };
    connect(m_sliderBlurRadius, &QSlider::valueChanged, this, triggerPreview);
    connect(m_sliderFocusRange, &QSlider::valueChanged, this, triggerPreview);
    connect(m_sliderFeather, &QSlider::valueChanged, this, triggerPreview);
    connect(m_comboType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, triggerPreview);
    connect(m_sliderCenterX, &QSlider::valueChanged, this, triggerPreview);
    connect(m_sliderCenterY, &QSlider::valueChanged, this, triggerPreview);

    setupBaseUi(controls);
    resize(360, 240);

    updatePreview();
}

void FocusBlurDialog::applyFilterToImage(const QImage& src, QImage& out) {
    int blurR = m_sliderBlurRadius ? m_sliderBlurRadius->value() : 15;
    float focusR = (m_sliderFocusRange ? m_sliderFocusRange->value() : 30) / 100.0f;
    float feather = (m_sliderFeather ? m_sliderFeather->value() : 25) / 100.0f;
    bool isTilt = m_comboType && m_comboType->currentIndex() == 1;
    QPointF center(
        (m_sliderCenterX ? m_sliderCenterX->value() : 50) / 100.0,
        (m_sliderCenterY ? m_sliderCenterY->value() : 50) / 100.0
    );

    out = FilterProcessor::applyFocusBlur(src, blurR, focusR, feather, center, isTilt);
}

// ----------------------------------------------------------------------------
// 3. BrightnessContrastDialog
// ----------------------------------------------------------------------------
BrightnessContrastDialog::BrightnessContrastDialog(Document* doc, CanvasWidget* canvas, QWidget* parent)
    : FilterBaseDialog(doc, canvas, tr("Brightness / Contrast"), parent)
{
    auto controls = new QWidget(this);
    auto form = new QFormLayout(controls);

    // 明るさ
    auto layB = new QHBoxLayout();
    m_sliderBrightness = new QSlider(Qt::Horizontal, controls);
    m_sliderBrightness->setRange(-100, 100);
    m_sliderBrightness->setValue(0);
    m_spinBrightness = new QSpinBox(controls);
    m_spinBrightness->setRange(-100, 100);
    m_spinBrightness->setValue(0);
    layB->addWidget(m_sliderBrightness);
    layB->addWidget(m_spinBrightness);
    form->addRow(tr("Brightness:"), layB);

    // コントラスト
    auto layC = new QHBoxLayout();
    m_sliderContrast = new QSlider(Qt::Horizontal, controls);
    m_sliderContrast->setRange(-100, 100);
    m_sliderContrast->setValue(0);
    m_spinContrast = new QSpinBox(controls);
    m_spinContrast->setRange(-100, 100);
    m_spinContrast->setValue(0);
    layC->addWidget(m_sliderContrast);
    layC->addWidget(m_spinContrast);
    form->addRow(tr("Contrast:"), layC);

    connect(m_sliderBrightness, &QSlider::valueChanged, m_spinBrightness, &QSpinBox::setValue);
    connect(m_spinBrightness, QOverload<int>::of(&QSpinBox::valueChanged), m_sliderBrightness, &QSlider::setValue);

    connect(m_sliderContrast, &QSlider::valueChanged, m_spinContrast, &QSpinBox::setValue);
    connect(m_spinContrast, QOverload<int>::of(&QSpinBox::valueChanged), m_sliderContrast, &QSlider::setValue);

    auto triggerPreview = [this]() { updatePreview(); };
    connect(m_sliderBrightness, &QSlider::valueChanged, this, triggerPreview);
    connect(m_sliderContrast, &QSlider::valueChanged, this, triggerPreview);

    setupBaseUi(controls);
    resize(340, 140);

    updatePreview();
}

void BrightnessContrastDialog::applyFilterToImage(const QImage& src, QImage& out) {
    float b = m_sliderBrightness ? static_cast<float>(m_sliderBrightness->value()) : 0.0f;
    float c = m_sliderContrast ? static_cast<float>(m_sliderContrast->value()) : 0.0f;
    out = FilterProcessor::applyBrightnessContrast(src, b, c);
}

// ----------------------------------------------------------------------------
// 4. HueSaturationDialog
// ----------------------------------------------------------------------------
HueSaturationDialog::HueSaturationDialog(Document* doc, CanvasWidget* canvas, QWidget* parent)
    : FilterBaseDialog(doc, canvas, tr("Hue / Saturation / Value"), parent)
{
    auto controls = new QWidget(this);
    auto form = new QFormLayout(controls);

    // 色相
    auto layH = new QHBoxLayout();
    m_sliderHue = new QSlider(Qt::Horizontal, controls);
    m_sliderHue->setRange(-180, 180);
    m_sliderHue->setValue(0);
    m_spinHue = new QSpinBox(controls);
    m_spinHue->setRange(-180, 180);
    m_spinHue->setValue(0);
    m_spinHue->setSuffix("°");
    layH->addWidget(m_sliderHue);
    layH->addWidget(m_spinHue);
    form->addRow(tr("Hue:"), layH);

    // 彩度
    auto layS = new QHBoxLayout();
    m_sliderSat = new QSlider(Qt::Horizontal, controls);
    m_sliderSat->setRange(-100, 100);
    m_sliderSat->setValue(0);
    m_spinSat = new QSpinBox(controls);
    m_spinSat->setRange(-100, 100);
    m_spinSat->setValue(0);
    layS->addWidget(m_sliderSat);
    layS->addWidget(m_spinSat);
    form->addRow(tr("Saturation:"), layS);

    // 明度
    auto layL = new QHBoxLayout();
    m_sliderLight = new QSlider(Qt::Horizontal, controls);
    m_sliderLight->setRange(-100, 100);
    m_sliderLight->setValue(0);
    m_spinLight = new QSpinBox(controls);
    m_spinLight->setRange(-100, 100);
    m_spinLight->setValue(0);
    layL->addWidget(m_sliderLight);
    layL->addWidget(m_spinLight);
    form->addRow(tr("Value:"), layL);

    connect(m_sliderHue, &QSlider::valueChanged, m_spinHue, &QSpinBox::setValue);
    connect(m_spinHue, QOverload<int>::of(&QSpinBox::valueChanged), m_sliderHue, &QSlider::setValue);

    connect(m_sliderSat, &QSlider::valueChanged, m_spinSat, &QSpinBox::setValue);
    connect(m_spinSat, QOverload<int>::of(&QSpinBox::valueChanged), m_sliderSat, &QSlider::setValue);

    connect(m_sliderLight, &QSlider::valueChanged, m_spinLight, &QSpinBox::setValue);
    connect(m_spinLight, QOverload<int>::of(&QSpinBox::valueChanged), m_sliderLight, &QSlider::setValue);

    auto triggerPreview = [this]() { updatePreview(); };
    connect(m_sliderHue, &QSlider::valueChanged, this, triggerPreview);
    connect(m_sliderSat, &QSlider::valueChanged, this, triggerPreview);
    connect(m_sliderLight, &QSlider::valueChanged, this, triggerPreview);

    setupBaseUi(controls);
    resize(360, 160);

    updatePreview();
}

void HueSaturationDialog::applyFilterToImage(const QImage& src, QImage& out) {
    float h = m_sliderHue ? static_cast<float>(m_sliderHue->value()) : 0.0f;
    float s = m_sliderSat ? static_cast<float>(m_sliderSat->value()) : 0.0f;
    float l = m_sliderLight ? static_cast<float>(m_sliderLight->value()) : 0.0f;
    out = FilterProcessor::applyHueSaturationLightness(src, h, s, l);
}

// ----------------------------------------------------------------------------
// 5. ToneCurveDialog
// ----------------------------------------------------------------------------
ToneCurveDialog::ToneCurveDialog(Document* doc, CanvasWidget* canvas, QWidget* parent)
    : FilterBaseDialog(doc, canvas, tr("Tone Curve"), parent)
{
    // 初期制御点 (各チャンネル斜め直線)
    for (int i = 0; i < 4; ++i) {
        m_curves[i] = { QPointF(0.0, 0.0), QPointF(255.0, 255.0) };
    }

    auto controls = new QWidget(this);
    auto vbox = new QVBoxLayout(controls);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(6);

    auto topBar = new QHBoxLayout();
    topBar->addWidget(new QLabel(tr("Channel:"), controls));

    m_channelCombo = new QComboBox(controls);
    m_channelCombo->addItem(tr("RGB (All)"), static_cast<int>(AdjustmentChannel::RGB));
    m_channelCombo->addItem(tr("Red (R)"), static_cast<int>(AdjustmentChannel::Red));
    m_channelCombo->addItem(tr("Green (G)"), static_cast<int>(AdjustmentChannel::Green));
    m_channelCombo->addItem(tr("Blue (B)"), static_cast<int>(AdjustmentChannel::Blue));
    topBar->addWidget(m_channelCombo);

    auto btnReset = new QPushButton(tr("Reset"), controls);
    topBar->addWidget(btnReset);
    topBar->addStretch(1);
    vbox->addLayout(topBar);

    m_curveWidget = new ToneCurveWidget(controls);
    m_curveWidget->setMinimumSize(256, 256);
    vbox->addWidget(m_curveWidget);

    connect(m_channelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        auto ch = static_cast<AdjustmentChannel>(idx);
        m_curveWidget->setCurrentChannel(ch);
    });

    connect(btnReset, &QPushButton::clicked, this, [this]() {
        auto ch = static_cast<AdjustmentChannel>(m_channelCombo->currentIndex());
        m_curves[static_cast<size_t>(ch)] = { QPointF(0.0, 0.0), QPointF(255.0, 255.0) };
        m_curveWidget->setCurvePoints(ch, m_curves[static_cast<size_t>(ch)]);
        updatePreview();
    });

    connect(m_curveWidget, &ToneCurveWidget::curveChanged, this, [this](AdjustmentChannel ch, const std::vector<QPointF>& pts) {
        m_curves[static_cast<size_t>(ch)] = pts;
        updatePreview();
    });

    setupBaseUi(controls);
    resize(360, 380);

    updatePreview();
}

void ToneCurveDialog::applyFilterToImage(const QImage& src, QImage& out) {
    out = FilterProcessor::applyToneCurve(src, m_curves, CurveInterpolationMode::Spline);
}

// ----------------------------------------------------------------------------
// 6. MotionBlurDialog
// ----------------------------------------------------------------------------
MotionBlurDialog::MotionBlurDialog(Document* doc, CanvasWidget* canvas, QWidget* parent)
    : FilterBaseDialog(doc, canvas, tr("Motion Blur"), parent)
{
    auto controls = new QWidget(this);
    auto form = new QFormLayout(controls);

    // 距離
    auto layDist = new QHBoxLayout();
    m_sliderDistance = new QSlider(Qt::Horizontal, controls);
    m_sliderDistance->setRange(1, 100);
    m_sliderDistance->setValue(10);
    m_spinDistance = new QSpinBox(controls);
    m_spinDistance->setRange(1, 100);
    m_spinDistance->setValue(10);
    m_spinDistance->setMinimumWidth(75);
    layDist->addWidget(m_sliderDistance);
    layDist->addWidget(m_spinDistance);
    form->addRow(tr("Distance (px):"), layDist);

    // 角度
    auto layAng = new QHBoxLayout();
    m_sliderAngle = new QSlider(Qt::Horizontal, controls);
    m_sliderAngle->setRange(-180, 180);
    m_sliderAngle->setValue(0);
    m_spinAngle = new QSpinBox(controls);
    m_spinAngle->setRange(-180, 180);
    m_spinAngle->setValue(0);
    m_spinAngle->setMinimumWidth(75);
    layAng->addWidget(m_sliderAngle);
    layAng->addWidget(m_spinAngle);
    form->addRow(tr("Angle (°):"), layAng);

    connect(m_sliderDistance, &QSlider::valueChanged, m_spinDistance, &QSpinBox::setValue);
    connect(m_spinDistance, QOverload<int>::of(&QSpinBox::valueChanged), m_sliderDistance, &QSlider::setValue);

    connect(m_sliderAngle, &QSlider::valueChanged, m_spinAngle, &QSpinBox::setValue);
    connect(m_spinAngle, QOverload<int>::of(&QSpinBox::valueChanged), m_sliderAngle, &QSlider::setValue);

    auto triggerPreview = [this]() { updatePreview(); };
    connect(m_sliderDistance, &QSlider::valueChanged, this, triggerPreview);
    connect(m_sliderAngle, &QSlider::valueChanged, this, triggerPreview);

    setupBaseUi(controls);
    resize(380, 140);

    updatePreview();
}

void MotionBlurDialog::applyFilterToImage(const QImage& src, QImage& out) {
    int dist = m_sliderDistance ? m_sliderDistance->value() : 10;
    float ang = m_sliderAngle ? static_cast<float>(m_sliderAngle->value()) : 0.0f;
    out = FilterProcessor::applyMotionBlur(src, dist, ang);
}

// ----------------------------------------------------------------------------
// 7. GradientMapDialog
// ----------------------------------------------------------------------------
GradientMapDialog::GradientMapDialog(Document* doc, CanvasWidget* canvas, QWidget* parent)
    : FilterBaseDialog(doc, canvas, tr("Gradient Map"), parent)
{
    // 初期色: キャンバスがあれば描画色と背景色、なければ黒と白
    if (m_canvas) {
        m_color1 = m_canvas->currentColor();
        m_color2 = m_canvas->secondaryColor();
    }
    if (!m_color1.isValid()) m_color1 = QColor(0, 0, 0);
    if (!m_color2.isValid()) m_color2 = QColor(255, 255, 255);

    // プリセット定義 (インデックス 2 以降に対応)
    m_presets = {
        // 0: 白黒
        { {0.0f, QColor(0, 0, 0)}, {1.0f, QColor(255, 255, 255)} },
        // 1: セピア
        { {0.0f, QColor(36, 18, 6)}, {0.5f, QColor(145, 96, 52)}, {1.0f, QColor(245, 225, 195)} },
        // 2: 夕焼け
        { {0.0f, QColor(20, 10, 40)}, {0.35f, QColor(140, 30, 80)}, {0.7f, QColor(230, 100, 40)}, {1.0f, QColor(255, 230, 120)} },
        // 3: サイバーパンク / ネオン
        { {0.0f, QColor(10, 5, 30)}, {0.4f, QColor(120, 0, 140)}, {0.75f, QColor(0, 180, 240)}, {1.0f, QColor(255, 255, 255)} },
        // 4: サーマル (熱波)
        { {0.0f, QColor(0, 0, 80)}, {0.25f, QColor(0, 120, 220)}, {0.5f, QColor(240, 220, 0)}, {0.75f, QColor(230, 40, 0)}, {1.0f, QColor(255, 255, 255)} },
        // 5: フォレスト (森)
        { {0.0f, QColor(10, 30, 15)}, {0.5f, QColor(40, 120, 60)}, {1.0f, QColor(210, 240, 170)} },
        // 6: パステル
        { {0.0f, QColor(90, 110, 160)}, {0.5f, QColor(210, 140, 180)}, {1.0f, QColor(255, 240, 220)} }
    };

    auto controls = new QWidget(this);
    auto form = new QFormLayout(controls);

    // モード
    m_comboPreset = new QComboBox(controls);
    m_comboPreset->setObjectName("comboMode");
    m_comboPreset->addItem(tr("描画色⇔背景色"));
    m_comboPreset->addItem(tr("選択2色"));
    m_comboPreset->addItem(tr("Grayscale (Monochrome)"));
    m_comboPreset->addItem(tr("Sepia"));
    m_comboPreset->addItem(tr("Sunset"));
    m_comboPreset->addItem(tr("Cyberpunk / Neon"));
    m_comboPreset->addItem(tr("Thermal"));
    m_comboPreset->addItem(tr("Forest"));
    m_comboPreset->addItem(tr("Pastel"));
    form->addRow(tr("モード:"), m_comboPreset);

    // 選択2色時の色選択ボタン
    m_colorSelectWidget = new QWidget(controls);
    m_colorSelectWidget->setObjectName("colorSelectWidget");
    auto layColors = new QHBoxLayout(m_colorSelectWidget);
    layColors->setContentsMargins(0, 0, 0, 0);
    layColors->setSpacing(8);

    m_btnColor1 = new QPushButton(m_colorSelectWidget);
    m_btnColor1->setObjectName("btnColor1");
    m_btnColor1->setMinimumWidth(80);
    m_btnColor1->setMinimumHeight(26);
    m_btnColor1->setToolTip(tr("色1 (暗部) を選択"));

    m_btnColor2 = new QPushButton(m_colorSelectWidget);
    m_btnColor2->setObjectName("btnColor2");
    m_btnColor2->setMinimumWidth(80);
    m_btnColor2->setMinimumHeight(26);
    m_btnColor2->setToolTip(tr("色2 (明部) を選択"));

    layColors->addWidget(new QLabel(tr("色 1:"), m_colorSelectWidget));
    layColors->addWidget(m_btnColor1);
    layColors->addWidget(new QLabel(tr("色 2:"), m_colorSelectWidget));
    layColors->addWidget(m_btnColor2);
    layColors->addStretch();

    m_lblColorSelect = new QLabel(tr("選択色:"), controls);
    form->addRow(m_lblColorSelect, m_colorSelectWidget);

    connect(m_btnColor1, &QPushButton::clicked, this, &GradientMapDialog::onColor1Clicked);
    connect(m_btnColor2, &QPushButton::clicked, this, &GradientMapDialog::onColor2Clicked);
    updateColorButtonStyles();

    // 反転
    m_chkReverse = new QCheckBox(tr("Reverse Gradient"), controls);
    form->addRow(QString(), m_chkReverse);

    // 不透明度
    auto layOp = new QHBoxLayout();
    m_sliderOpacity = new QSlider(Qt::Horizontal, controls);
    m_sliderOpacity->setRange(0, 100);
    m_sliderOpacity->setValue(100);
    m_spinOpacity = new QSpinBox(controls);
    m_spinOpacity->setRange(0, 100);
    m_spinOpacity->setValue(100);
    m_spinOpacity->setSuffix("%");
    m_spinOpacity->setMinimumWidth(75);
    layOp->addWidget(m_sliderOpacity);
    layOp->addWidget(m_spinOpacity);
    form->addRow(tr("Opacity:"), layOp);

    connect(m_sliderOpacity, &QSlider::valueChanged, m_spinOpacity, &QSpinBox::setValue);
    connect(m_spinOpacity, QOverload<int>::of(&QSpinBox::valueChanged), m_sliderOpacity, &QSlider::setValue);

    auto triggerPreview = [this]() { updatePreview(); };
    connect(m_comboPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, triggerPreview]() {
        updateModeVisibility();
        triggerPreview();
    });
    connect(m_chkReverse, &QCheckBox::toggled, this, triggerPreview);
    connect(m_sliderOpacity, &QSlider::valueChanged, this, triggerPreview);

    updateModeVisibility();

    setupBaseUi(controls);
    resize(400, 180);

    updatePreview();
}

GradientMapDialog::GradientMode GradientMapDialog::mode() const {
    int idx = m_comboPreset ? m_comboPreset->currentIndex() : 0;
    if (idx == 0) return GradientMode::ForegroundBackground;
    if (idx == 1) return GradientMode::CustomTwoColors;
    return GradientMode::Preset;
}

void GradientMapDialog::setMode(GradientMode mode) {
    if (!m_comboPreset) return;
    if (mode == GradientMode::ForegroundBackground) {
        m_comboPreset->setCurrentIndex(0);
    } else if (mode == GradientMode::CustomTwoColors) {
        m_comboPreset->setCurrentIndex(1);
    }
}

int GradientMapDialog::modeIndex() const {
    return m_comboPreset ? m_comboPreset->currentIndex() : 0;
}

void GradientMapDialog::setModeIndex(int idx) {
    if (m_comboPreset && idx >= 0 && idx < m_comboPreset->count()) {
        m_comboPreset->setCurrentIndex(idx);
    }
}

void GradientMapDialog::setColor1(const QColor& c) {
    if (c.isValid() && m_color1 != c) {
        m_color1 = c;
        updateColorButtonStyles();
        updatePreview();
    }
}

void GradientMapDialog::setColor2(const QColor& c) {
    if (c.isValid() && m_color2 != c) {
        m_color2 = c;
        updateColorButtonStyles();
        updatePreview();
    }
}

void GradientMapDialog::onColor1Clicked() {
    QColor c = QColorDialog::getColor(m_color1, this, tr("色 1 を選択"), QColorDialog::ShowAlphaChannel | QColorDialog::DontUseNativeDialog);
    if (c.isValid()) {
        setColor1(c);
    }
}

void GradientMapDialog::onColor2Clicked() {
    QColor c = QColorDialog::getColor(m_color2, this, tr("色 2 を選択"), QColorDialog::ShowAlphaChannel | QColorDialog::DontUseNativeDialog);
    if (c.isValid()) {
        setColor2(c);
    }
}

void GradientMapDialog::updateColorButtonStyles() {
    auto applyStyle = [](QPushButton* btn, const QColor& color) {
        if (!btn) return;
        QString hex = color.name(QColor::HexRgb).toUpper();
        int brightness = static_cast<int>(0.299 * color.red() + 0.587 * color.green() + 0.114 * color.blue());
        QString textColor = (brightness > 128) ? "#000000" : "#ffffff";
        btn->setText(hex);
        btn->setStyleSheet(QString(
            "QPushButton { background-color: %1; color: %2; font-weight: bold; border: 1px solid #777; border-radius: 4px; padding: 4px 8px; }"
            "QPushButton:hover { border: 2px solid #333; }"
        ).arg(hex, textColor));
    };

    applyStyle(m_btnColor1, m_color1);
    applyStyle(m_btnColor2, m_color2);
}

void GradientMapDialog::updateModeVisibility() {
    bool isCustom = (m_comboPreset && m_comboPreset->currentIndex() == 1);
    if (m_colorSelectWidget) m_colorSelectWidget->setVisible(isCustom);
    if (m_lblColorSelect) m_lblColorSelect->setVisible(isCustom);
}

void GradientMapDialog::applyFilterToImage(const QImage& src, QImage& out) {
    int idx = m_comboPreset ? m_comboPreset->currentIndex() : 0;
    std::vector<QPair<float, QColor>> stops;

    if (idx == 0) {
        // 描画色⇔背景色
        QColor fg = m_canvas ? m_canvas->currentColor() : Qt::black;
        QColor bg = m_canvas ? m_canvas->secondaryColor() : Qt::white;
        stops = { {0.0f, fg}, {1.0f, bg} };
    } else if (idx == 1) {
        // 選択2色
        stops = { {0.0f, m_color1}, {1.0f, m_color2} };
    } else {
        // プリセット (インデックス 2 以降)
        int presetIdx = idx - 2;
        if (presetIdx >= 0 && presetIdx < static_cast<int>(m_presets.size())) {
            stops = m_presets[presetIdx];
        } else {
            stops = { {0.0f, QColor(0, 0, 0)}, {1.0f, QColor(255, 255, 255)} };
        }
    }

    bool rev = m_chkReverse && m_chkReverse->isChecked();
    float op = m_sliderOpacity ? (m_sliderOpacity->value() / 100.0f) : 1.0f;

    out = FilterProcessor::applyGradientMap(src, stops, rev, op);
}

// ----------------------------------------------------------------------------
// 8. ColorToAlphaDialog (色を透明度に - GIMP互換)
// ----------------------------------------------------------------------------
ColorToAlphaDialog::ColorToAlphaDialog(Document* doc, CanvasWidget* canvas, QWidget* parent)
    : FilterBaseDialog(doc, canvas, tr("Color to Alpha"), parent)
{
    auto controls = new QWidget(this);
    auto form = new QFormLayout(controls);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(8);

    // 1. 透明にする色の選択
    auto layColor = new QHBoxLayout();
    m_btnColor = new QPushButton(controls);
    m_btnColor->setMinimumHeight(28);
    m_btnColor->setToolTip(tr("Click to select the color to make transparent"));
    connect(m_btnColor, &QPushButton::clicked, this, &ColorToAlphaDialog::onPickColorClicked);

    m_btnFromFg = new QPushButton(tr("From &Foreground"), controls);
    m_btnFromFg->setToolTip(tr("Set the current foreground color as the transparent color"));
    connect(m_btnFromFg, &QPushButton::clicked, this, &ColorToAlphaDialog::onFromFgColorClicked);

    layColor->addWidget(m_btnColor, 1);
    layColor->addWidget(m_btnFromFg, 0);
    form->addRow(tr("Color to make transparent:"), layColor);

    // 2. 透明度しきい値 (0.000 ~ 1.000, デフォルト 0.000)
    auto layTrans = new QHBoxLayout();
    m_sliderTransThreshold = new QSlider(Qt::Horizontal, controls);
    m_sliderTransThreshold->setRange(0, 1000);
    m_sliderTransThreshold->setValue(0);

    m_spinTransThreshold = new QDoubleSpinBox(controls);
    m_spinTransThreshold->setRange(0.0, 1.0);
    m_spinTransThreshold->setSingleStep(0.01);
    m_spinTransThreshold->setDecimals(3);
    m_spinTransThreshold->setValue(0.0);

    layTrans->addWidget(m_sliderTransThreshold);
    layTrans->addWidget(m_spinTransThreshold);
    form->addRow(tr("Transparency threshold:"), layTrans);

    // 3. 不透明度しきい値 (0.000 ~ 1.000, デフォルト 1.000)
    auto layOp = new QHBoxLayout();
    m_sliderOpacityThreshold = new QSlider(Qt::Horizontal, controls);
    m_sliderOpacityThreshold->setRange(0, 1000);
    m_sliderOpacityThreshold->setValue(1000);

    m_spinOpacityThreshold = new QDoubleSpinBox(controls);
    m_spinOpacityThreshold->setRange(0.0, 1.0);
    m_spinOpacityThreshold->setSingleStep(0.01);
    m_spinOpacityThreshold->setDecimals(3);
    m_spinOpacityThreshold->setValue(1.0);

    layOp->addWidget(m_sliderOpacityThreshold);
    layOp->addWidget(m_spinOpacityThreshold);
    form->addRow(tr("Opacity threshold:"), layOp);

    // スライダーとスピンボックスの同期
    connect(m_sliderTransThreshold, &QSlider::valueChanged, this, [this](int v) {
        m_spinTransThreshold->blockSignals(true);
        m_spinTransThreshold->setValue(v / 1000.0);
        m_spinTransThreshold->blockSignals(false);
        updatePreview();
    });
    connect(m_spinTransThreshold, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        m_sliderTransThreshold->blockSignals(true);
        m_sliderTransThreshold->setValue(static_cast<int>(v * 1000.0 + 0.5));
        m_sliderTransThreshold->blockSignals(false);
        updatePreview();
    });

    connect(m_sliderOpacityThreshold, &QSlider::valueChanged, this, [this](int v) {
        m_spinOpacityThreshold->blockSignals(true);
        m_spinOpacityThreshold->setValue(v / 1000.0);
        m_spinOpacityThreshold->blockSignals(false);
        updatePreview();
    });
    connect(m_spinOpacityThreshold, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        m_sliderOpacityThreshold->blockSignals(true);
        m_sliderOpacityThreshold->setValue(static_cast<int>(v * 1000.0 + 0.5));
        m_sliderOpacityThreshold->blockSignals(false);
        updatePreview();
    });

    updateColorButtonStyle();

    setupBaseUi(controls);
    resize(400, 200);

    updatePreview();
}

void ColorToAlphaDialog::setTargetColor(const QColor& color) {
    if (color.isValid()) {
        m_targetColor = color;
        updateColorButtonStyle();
        updatePreview();
    }
}

float ColorToAlphaDialog::transparencyThreshold() const {
    return m_spinTransThreshold ? static_cast<float>(m_spinTransThreshold->value()) : 0.0f;
}

float ColorToAlphaDialog::opacityThreshold() const {
    return m_spinOpacityThreshold ? static_cast<float>(m_spinOpacityThreshold->value()) : 1.0f;
}

void ColorToAlphaDialog::onPickColorClicked() {
    QColor c = QColorDialog::getColor(m_targetColor, this, tr("Select Color to Make Transparent"));
    if (c.isValid()) {
        setTargetColor(c);
    }
}

void ColorToAlphaDialog::onFromFgColorClicked() {
    if (m_canvas) {
        setTargetColor(m_canvas->currentColor());
    }
}

void ColorToAlphaDialog::updateColorButtonStyle() {
    if (!m_btnColor) return;
    QString hex = m_targetColor.name(QColor::HexRgb).toUpper();
    // 明度に応じて文字色を黒または白に
    int brightness = static_cast<int>(0.299 * m_targetColor.red() + 0.587 * m_targetColor.green() + 0.114 * m_targetColor.blue());
    QString textColor = (brightness > 128) ? "#000000" : "#ffffff";
    m_btnColor->setText(hex);
    m_btnColor->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: %2; font-weight: bold; border: 1px solid #777; border-radius: 4px; padding: 4px; }"
        "QPushButton:hover { border: 2px solid #333; }"
    ).arg(hex, textColor));
}

void ColorToAlphaDialog::applyFilterToImage(const QImage& src, QImage& out) {
    float tLow = transparencyThreshold();
    float tHigh = opacityThreshold();
    out = FilterProcessor::colorToAlpha(src, m_targetColor, tLow, tHigh);
}

} // namespace Kaburagi


