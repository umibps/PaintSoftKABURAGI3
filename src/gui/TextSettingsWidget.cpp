#include "TextSettingsWidget.hpp"
#include "ToolBoxWidget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QColorDialog>
#include <QInputDialog>
#include <QKeyEvent>
#include <QRandomGenerator>
#include <algorithm>

namespace Kaburagi {

TextSettingsWidget::TextSettingsWidget(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

void TextSettingsWidget::setToolType(ToolType tool) {
    if (tool == ToolType::TextBalloon) {
        if (m_tabWidget) m_tabWidget->setCurrentIndex(1);
    } else if (tool == ToolType::Text) {
        if (m_tabWidget) m_tabWidget->setCurrentIndex(0);
    }
}

void TextSettingsWidget::setupUi() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(4);

    m_tabWidget = new QTabWidget(this);

    // ==========================================
    // タブ 1: テキスト設定 (入力・フォント・配置)
    // ==========================================
    auto textTab = new QWidget(m_tabWidget);
    auto textTabLayout = new QVBoxLayout(textTab);
    textTabLayout->setContentsMargins(4, 6, 4, 4);
    textTabLayout->setSpacing(6);

    // 1. テキスト入力エリア
    auto textGroup = new QGroupBox("テキスト入力", textTab);
    auto textGroupLayout = new QVBoxLayout(textGroup);
    textGroupLayout->setContentsMargins(4, 6, 4, 4);
    textGroupLayout->setSpacing(4);

    // 入力支援ツールバー
    auto inlineToolbarLayout = new QHBoxLayout();
    inlineToolbarLayout->setContentsMargins(0, 0, 0, 0);
    inlineToolbarLayout->setSpacing(2);

    m_btnInlineBold = new QPushButton("B", textGroup);
    m_btnInlineBold->setToolTip("選択部分を太字にする (<b>...</b> または **...**)");
    m_btnInlineBold->setFixedWidth(26);
    m_btnInlineBold->setStyleSheet("font-weight: bold;");

    m_btnInlineItalic = new QPushButton("I", textGroup);
    m_btnInlineItalic->setToolTip("選択部分を斜体にする (<i>...</i>)");
    m_btnInlineItalic->setFixedWidth(26);
    m_btnInlineItalic->setStyleSheet("font-style: italic;");

    m_btnInlineColor = new QPushButton("色", textGroup);
    m_btnInlineColor->setToolTip("選択部分の文字色を指定する (<color=#RRGGBB>...</color>)");
    m_btnInlineColor->setFixedWidth(28);

    m_spinInlineSize = new QSpinBox(textGroup);
    m_spinInlineSize->setRange(6, 300);
    m_spinInlineSize->setValue(24);
    m_spinInlineSize->setSuffix(" pt");
    m_spinInlineSize->setToolTip("選択部分の文字サイズ");
    m_spinInlineSize->setMinimumWidth(95);

    m_btnInlineSize = new QPushButton("サイズ", textGroup);
    m_btnInlineSize->setToolTip("選択部分に文字サイズを適用する (<size=24>...</size>)");
    m_btnInlineSize->setFixedWidth(44);

    m_btnInlineRuby = new QPushButton("ルビ", textGroup);
    m_btnInlineRuby->setToolTip("選択部分にルビ（ふりがな）を振る (親文字《ルビ》)");
    m_btnInlineRuby->setFixedWidth(36);

    inlineToolbarLayout->addWidget(m_btnInlineBold);
    inlineToolbarLayout->addWidget(m_btnInlineItalic);
    inlineToolbarLayout->addWidget(m_btnInlineColor);
    inlineToolbarLayout->addWidget(m_spinInlineSize);
    inlineToolbarLayout->addWidget(m_btnInlineSize);
    inlineToolbarLayout->addWidget(m_btnInlineRuby);
    inlineToolbarLayout->addStretch(1);

    textGroupLayout->addLayout(inlineToolbarLayout);

    m_textEdit = new QTextEdit(textGroup);
    m_textEdit->setPlaceholderText("テキストを入力...\n※ツールバーのボタンで太字・文字色・サイズ・ルビを挿入可能");
    m_textEdit->setMinimumHeight(60);
    m_textEdit->installEventFilter(this);
    textGroupLayout->addWidget(m_textEdit);

    // 下部: タグあり / タグなし 表示切り替え & 表示倍率
    auto bottomViewModeLayout = new QHBoxLayout();
    bottomViewModeLayout->setContentsMargins(0, 0, 0, 0);
    bottomViewModeLayout->setSpacing(4);

    m_tagViewModeGroup = new QButtonGroup(textGroup);
    m_btnShowTags = new QPushButton("タグあり表示", textGroup);
    m_btnShowTags->setCheckable(true);
    m_btnShowTags->setChecked(true);
    m_btnShowTags->setToolTip("タグ（<b>や<color>等）を表示して直接編集します");
    m_btnShowTags->setFixedHeight(24);
    m_btnShowTags->setStyleSheet("QPushButton:checked { font-weight: bold; background: #0078d7; color: white; }");

    m_btnHideTags = new QPushButton("タグ無し表示", textGroup);
    m_btnHideTags->setCheckable(true);
    m_btnHideTags->setToolTip("タグを非表示にし、装飾（太字・文字色・サイズ・ルビ）をプレビュー表示します");
    m_btnHideTags->setFixedHeight(24);
    m_btnHideTags->setStyleSheet("QPushButton:checked { font-weight: bold; background: #0078d7; color: white; }");

    m_tagViewModeGroup->addButton(m_btnShowTags, 0);
    m_tagViewModeGroup->addButton(m_btnHideTags, 1);

    bottomViewModeLayout->addWidget(new QLabel("表示モード:", textGroup));
    bottomViewModeLayout->addWidget(m_btnShowTags);
    bottomViewModeLayout->addWidget(m_btnHideTags);

    bottomViewModeLayout->addSpacing(8);
    m_zoomLabel = new QLabel("表示倍率:", textGroup);
    m_previewZoomSpin = new QSpinBox(textGroup);
    m_previewZoomSpin->setRange(20, 300);
    m_previewZoomSpin->setSingleStep(10);
    m_previewZoomSpin->setValue(100);
    m_previewZoomSpin->setSuffix(" %");
    m_previewZoomSpin->setFixedWidth(80);
    m_previewZoomSpin->setToolTip("タグ無し表示時のフォント表示倍率（文字が大きすぎる場合に縮小できます）");
    m_zoomLabel->setEnabled(false);
    m_previewZoomSpin->setEnabled(false);

    bottomViewModeLayout->addWidget(m_zoomLabel);
    bottomViewModeLayout->addWidget(m_previewZoomSpin);
    bottomViewModeLayout->addStretch(1);

    textGroupLayout->addLayout(bottomViewModeLayout);
    textTabLayout->addWidget(textGroup);

    // 2. フォント・サイズ・文字色設定
    auto fontGroup = new QGroupBox("フォント設定", textTab);
    auto fontGroupLayout = new QVBoxLayout(fontGroup);
    fontGroupLayout->setContentsMargins(4, 6, 4, 4);
    fontGroupLayout->setSpacing(4);

    m_fontCombo = new QFontComboBox(fontGroup);
    m_fontCombo->setCurrentFont(QFont("Yu Gothic", 16));
    fontGroupLayout->addWidget(m_fontCombo);

    auto sizeStyleLayout = new QHBoxLayout();
    sizeStyleLayout->setContentsMargins(0, 0, 0, 0);
    sizeStyleLayout->setSpacing(4);

    m_fontSizeSpin = new QSpinBox(fontGroup);
    m_fontSizeSpin->setRange(6, 300);
    m_fontSizeSpin->setValue(24);
    m_fontSizeSpin->setSuffix(" pt");
    m_fontSizeSpin->setMinimumWidth(85);
    sizeStyleLayout->addWidget(m_fontSizeSpin, 1);

    m_btnBold = new QPushButton("B", fontGroup);
    m_btnBold->setCheckable(true);
    m_btnBold->setToolTip("太字 (Bold)");
    m_btnBold->setFixedWidth(28);
    m_btnBold->setStyleSheet("QPushButton:checked { font-weight: bold; background: #0078d7; color: white; }");
    sizeStyleLayout->addWidget(m_btnBold);

    m_btnItalic = new QPushButton("I", fontGroup);
    m_btnItalic->setCheckable(true);
    m_btnItalic->setToolTip("斜体 / イタリック (Italic)");
    m_btnItalic->setFixedWidth(28);
    m_btnItalic->setStyleSheet("QPushButton:checked { font-style: italic; background: #0078d7; color: white; }");
    sizeStyleLayout->addWidget(m_btnItalic);

    fontGroupLayout->addLayout(sizeStyleLayout);

    // 文字色変更ボタン
    auto colorRow = new QHBoxLayout();
    colorRow->setContentsMargins(0, 0, 0, 0);
    colorRow->setSpacing(4);
    colorRow->addWidget(new QLabel("文字色:", fontGroup));

    m_btnTextColor = new QPushButton("文字色変更", fontGroup);
    m_btnTextColor->setFixedHeight(26);
    m_btnTextColor->setToolTip("テキストの文字色を選択");
    colorRow->addWidget(m_btnTextColor, 1);
    fontGroupLayout->addLayout(colorRow);

    textTabLayout->addWidget(fontGroup);

    // 3. 配置・書字方向
    auto alignGroup = new QGroupBox("配置・書字方向", textTab);
    auto alignGroupLayout = new QVBoxLayout(alignGroup);
    alignGroupLayout->setContentsMargins(4, 6, 4, 4);
    alignGroupLayout->setSpacing(4);

    // 書字方向 (横書き / 縦書き)
    auto dirRow = new QHBoxLayout();
    dirRow->setContentsMargins(0, 0, 0, 0);
    dirRow->setSpacing(4);
    dirRow->addWidget(new QLabel("方向:", alignGroup));

    m_dirGroup = new QButtonGroup(this);
    m_dirGroup->setExclusive(true);

    m_btnHorizontal = new QPushButton("横書き", alignGroup);
    m_btnHorizontal->setCheckable(true);
    m_btnHorizontal->setChecked(true);
    m_btnHorizontal->setStyleSheet("QPushButton:checked { font-weight: bold; background: #0078d7; color: white; }");
    m_dirGroup->addButton(m_btnHorizontal, 0);
    dirRow->addWidget(m_btnHorizontal, 1);

    m_btnVertical = new QPushButton("縦書き", alignGroup);
    m_btnVertical->setCheckable(true);
    m_btnVertical->setStyleSheet("QPushButton:checked { font-weight: bold; background: #0078d7; color: white; }");
    m_dirGroup->addButton(m_btnVertical, 1);
    dirRow->addWidget(m_btnVertical, 1);
    alignGroupLayout->addLayout(dirRow);

    // 水平
    auto hAlignRow = new QHBoxLayout();
    hAlignRow->setContentsMargins(0, 0, 0, 0);
    hAlignRow->setSpacing(4);
    hAlignRow->addWidget(new QLabel("水平:", alignGroup));

    m_hAlignGroup = new QButtonGroup(this);
    m_hAlignGroup->setExclusive(true);

    m_btnAlignLeft = new QPushButton("左", alignGroup);
    m_btnAlignLeft->setCheckable(true);
    m_btnAlignLeft->setChecked(true);
    m_hAlignGroup->addButton(m_btnAlignLeft, 0);
    hAlignRow->addWidget(m_btnAlignLeft);

    m_btnAlignCenter = new QPushButton("中", alignGroup);
    m_btnAlignCenter->setCheckable(true);
    m_hAlignGroup->addButton(m_btnAlignCenter, 1);
    hAlignRow->addWidget(m_btnAlignCenter);

    m_btnAlignRight = new QPushButton("右", alignGroup);
    m_btnAlignRight->setCheckable(true);
    m_hAlignGroup->addButton(m_btnAlignRight, 2);
    hAlignRow->addWidget(m_btnAlignRight);
    alignGroupLayout->addLayout(hAlignRow);

    // 垂直
    auto vAlignRow = new QHBoxLayout();
    vAlignRow->setContentsMargins(0, 0, 0, 0);
    vAlignRow->setSpacing(4);
    vAlignRow->addWidget(new QLabel("垂直:", alignGroup));

    m_vAlignGroup = new QButtonGroup(this);
    m_vAlignGroup->setExclusive(true);

    m_btnAlignTop = new QPushButton("上", alignGroup);
    m_btnAlignTop->setCheckable(true);
    m_btnAlignTop->setChecked(true);
    m_vAlignGroup->addButton(m_btnAlignTop, 0);
    vAlignRow->addWidget(m_btnAlignTop);

    m_btnAlignVCenter = new QPushButton("中", alignGroup);
    m_btnAlignVCenter->setCheckable(true);
    m_vAlignGroup->addButton(m_btnAlignVCenter, 1);
    vAlignRow->addWidget(m_btnAlignVCenter);

    m_btnAlignBottom = new QPushButton("下", alignGroup);
    m_btnAlignBottom->setCheckable(true);
    m_vAlignGroup->addButton(m_btnAlignBottom, 2);
    vAlignRow->addWidget(m_btnAlignBottom);
    alignGroupLayout->addLayout(vAlignRow);

    textTabLayout->addWidget(alignGroup);
    textTabLayout->addStretch();

    m_tabWidget->addTab(textTab, "テキスト");

    // ==========================================
    // タブ 2: 吹き出し設定 (形状・しっぽ・枠線・背景・フチ)
    // ==========================================
    auto balloonTab = new QWidget(m_tabWidget);
    auto balloonTabLayout = new QVBoxLayout(balloonTab);
    balloonTabLayout->setContentsMargins(4, 6, 4, 4);
    balloonTabLayout->setSpacing(6);

    // 4. 吹き出し形状・外観
    auto balloonGroup = new QGroupBox("吹き出し形状・外観", balloonTab);
    auto balloonLayout = new QVBoxLayout(balloonGroup);
    balloonLayout->setContentsMargins(4, 6, 4, 4);
    balloonLayout->setSpacing(4);

    // 形状選択コンボボックス
    auto shapeRow = new QHBoxLayout();
    shapeRow->addWidget(new QLabel("形状:", balloonGroup));
    m_shapeCombo = new QComboBox(balloonGroup);
    m_shapeCombo->addItem("楕円吹き出し", static_cast<int>(BalloonShape::Ellipse));
    m_shapeCombo->addItem("角丸四角形吹き出し", static_cast<int>(BalloonShape::RoundedRect));
    m_shapeCombo->addItem("四角形 (モノローグ)", static_cast<int>(BalloonShape::Rectangle));
    m_shapeCombo->addItem("ウニフラッシュ (叫び)", static_cast<int>(BalloonShape::Spike));
    m_shapeCombo->addItem("モコモコ (思考)", static_cast<int>(BalloonShape::Cloud));
    m_shapeCombo->addItem("枠なし (文字のみ)", static_cast<int>(BalloonShape::None));
    shapeRow->addWidget(m_shapeCombo, 1);
    balloonLayout->addLayout(shapeRow);

    // 5. 特殊形状パラメータ (Spike / Cloud)
    m_specialShapeWidget = new QWidget(balloonGroup);
    auto specialLayout = new QFormLayout(m_specialShapeWidget);
    specialLayout->setContentsMargins(0, 2, 0, 2);
    specialLayout->setSpacing(4);

    m_spikeCountSpin = new QSpinBox(m_specialShapeWidget);
    m_spikeCountSpin->setRange(4, 64);
    m_spikeCountSpin->setValue(16);
    specialLayout->addRow("個数/密度:", m_spikeCountSpin);

    m_spikeDepthSpin = new QDoubleSpinBox(m_specialShapeWidget);
    m_spikeDepthSpin->setRange(2.0, 150.0);
    m_spikeDepthSpin->setValue(20.0);
    m_spikeDepthSpin->setSuffix(" px");
    specialLayout->addRow("深さ/大きさ:", m_spikeDepthSpin);

    m_spikePhaseSpin = new QDoubleSpinBox(m_specialShapeWidget);
    m_spikePhaseSpin->setRange(0.0, 360.0);
    m_spikePhaseSpin->setValue(0.0);
    m_spikePhaseSpin->setSuffix(" °");
    specialLayout->addRow("回転オフセット:", m_spikePhaseSpin);

    m_spikeRandomSpin = new QSpinBox(m_specialShapeWidget);
    m_spikeRandomSpin->setRange(0, 100);
    m_spikeRandomSpin->setValue(0);
    specialLayout->addRow("ランダム凸凹:", m_spikeRandomSpin);

    m_btnRandomizeSpikes = new QPushButton("ランダム再配置", m_specialShapeWidget);
    specialLayout->addRow("", m_btnRandomizeSpikes);
    balloonLayout->addWidget(m_specialShapeWidget);

    balloonTabLayout->addWidget(balloonGroup);

    // 6. しっぽ設定
    auto tailGroup = new QGroupBox("しっぽ設定", balloonTab);
    auto tailGroupLayout = new QVBoxLayout(tailGroup);
    tailGroupLayout->setContentsMargins(4, 6, 4, 4);
    tailGroupLayout->setSpacing(4);

    m_hasTailCheck = new QCheckBox("しっぽを付ける", tailGroup);
    m_hasTailCheck->setChecked(true);
    tailGroupLayout->addWidget(m_hasTailCheck);

    m_tailParamsWidget = new QWidget(tailGroup);
    auto tailLayout = new QFormLayout(m_tailParamsWidget);
    tailLayout->setContentsMargins(0, 0, 0, 0);
    tailLayout->setSpacing(4);

    m_tailBaseWidthSpin = new QDoubleSpinBox(m_tailParamsWidget);
    m_tailBaseWidthSpin->setRange(4.0, 120.0);
    m_tailBaseWidthSpin->setValue(24.0);
    m_tailBaseWidthSpin->setSuffix(" px");
    tailLayout->addRow("根元の太さ:", m_tailBaseWidthSpin);

    auto curveRow = new QHBoxLayout();
    m_tailCurvatureSlider = new QSlider(Qt::Horizontal, m_tailParamsWidget);
    m_tailCurvatureSlider->setRange(-100, 100);
    m_tailCurvatureSlider->setValue(0);
    m_tailCurvatureLabel = new QLabel("直線", m_tailParamsWidget);
    m_tailCurvatureLabel->setFixedWidth(36);
    curveRow->addWidget(m_tailCurvatureSlider, 1);
    curveRow->addWidget(m_tailCurvatureLabel);
    tailLayout->addRow("カーブ曲率:", curveRow);
    tailGroupLayout->addWidget(m_tailParamsWidget);

    balloonTabLayout->addWidget(tailGroup);

    // 7. スタイル (枠線・背景色)
    auto styleGroup = new QGroupBox("スタイル (線・背景)", balloonTab);
    auto styleGroupLayout = new QVBoxLayout(styleGroup);
    styleGroupLayout->setContentsMargins(4, 6, 4, 4);
    styleGroupLayout->setSpacing(4);

    auto styleLayout = new QFormLayout();
    styleLayout->setContentsMargins(0, 0, 0, 0);
    styleLayout->setSpacing(4);

    auto frameRow = new QHBoxLayout();
    m_frameWidthSpin = new QDoubleSpinBox(styleGroup);
    m_frameWidthSpin->setRange(0.0, 50.0);
    m_frameWidthSpin->setValue(3.0);
    m_frameWidthSpin->setSuffix(" px");
    frameRow->addWidget(m_frameWidthSpin, 1);

    m_btnFrameColor = new QPushButton("枠線色", styleGroup);
    m_btnFrameColor->setFixedWidth(56);
    frameRow->addWidget(m_btnFrameColor);
    styleLayout->addRow("枠線の太さ/色:", frameRow);

    auto fillRow = new QHBoxLayout();
    m_btnFillColor = new QPushButton("背景色", styleGroup);
    m_btnFillColor->setFixedWidth(56);
    fillRow->addWidget(m_btnFillColor);

    m_transparentFillCheck = new QCheckBox("背景透明", styleGroup);
    fillRow->addWidget(m_transparentFillCheck, 1);
    styleLayout->addRow("背景色:", fillRow);

    styleGroupLayout->addLayout(styleLayout);
    balloonTabLayout->addWidget(styleGroup);

    // 8. フチ取り設定
    auto outlineGroup = new QGroupBox("フチ取り (二重枠)", balloonTab);
    auto outlineGroupLayout = new QVBoxLayout(outlineGroup);
    outlineGroupLayout->setContentsMargins(4, 6, 4, 4);
    outlineGroupLayout->setSpacing(4);

    m_hasOutlineCheck = new QCheckBox("フチ取りを付ける", outlineGroup);
    outlineGroupLayout->addWidget(m_hasOutlineCheck);

    m_outlineParamsWidget = new QWidget(outlineGroup);
    auto outlineLayout = new QHBoxLayout(m_outlineParamsWidget);
    outlineLayout->setContentsMargins(0, 0, 0, 0);
    outlineLayout->setSpacing(4);

    m_outlineWidthSpin = new QDoubleSpinBox(m_outlineParamsWidget);
    m_outlineWidthSpin->setRange(1.0, 30.0);
    m_outlineWidthSpin->setValue(4.0);
    m_outlineWidthSpin->setSuffix(" px");
    outlineLayout->addWidget(new QLabel("太さ:", m_outlineParamsWidget));
    outlineLayout->addWidget(m_outlineWidthSpin, 1);

    m_btnOutlineColor = new QPushButton("フチ色", m_outlineParamsWidget);
    m_btnOutlineColor->setFixedWidth(56);
    outlineLayout->addWidget(m_btnOutlineColor);
    outlineGroupLayout->addWidget(m_outlineParamsWidget);

    balloonTabLayout->addWidget(outlineGroup);
    balloonTabLayout->addStretch();

    m_tabWidget->addTab(balloonTab, "吹き出し");

    mainLayout->addWidget(m_tabWidget);

    // シグナル接続
    auto onSettingsChanged = [this]() {
        if (!m_updatingUi) {
            emit settingsChanged();
        }
    };

    connect(m_textEdit, &QTextEdit::textChanged, this, [this]() {
        if (!m_updatingUi) {
            if (!m_hideTags) {
                m_rawText = m_textEdit->toPlainText();
            }
            emit textChanged(m_rawText);
        }
    });

    connect(m_tagViewModeGroup, &QButtonGroup::idClicked, this, [this](int id) {
        setHideTags(id == 1);
    });

    connect(m_previewZoomSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        if (m_hideTags) {
            updateTextDisplay();
        }
    });

    connect(m_btnInlineBold, &QPushButton::clicked, this, &TextSettingsWidget::onInlineBoldClicked);
    connect(m_btnInlineItalic, &QPushButton::clicked, this, &TextSettingsWidget::onInlineItalicClicked);
    connect(m_btnInlineColor, &QPushButton::clicked, this, &TextSettingsWidget::onInlineColorClicked);
    connect(m_btnInlineSize, &QPushButton::clicked, this, &TextSettingsWidget::onInlineSizeClicked);
    connect(m_btnInlineRuby, &QPushButton::clicked, this, &TextSettingsWidget::onInlineRubyClicked);

    connect(m_fontCombo, &QFontComboBox::currentFontChanged, this, onSettingsChanged);
    connect(m_fontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, onSettingsChanged](int val) {
        if (m_spinInlineSize) m_spinInlineSize->setValue(val);
        onSettingsChanged();
    });
    connect(m_btnBold, &QPushButton::toggled, this, onSettingsChanged);
    connect(m_btnItalic, &QPushButton::toggled, this, onSettingsChanged);
    connect(m_btnTextColor, &QPushButton::clicked, this, [this, onSettingsChanged]() {
        QColor col = QColorDialog::getColor(m_textColor, this, "文字色を選択");
        if (col.isValid()) {
            m_textColor = col;
            updateColorButtons();
            onSettingsChanged();
        }
    });
    connect(m_hAlignGroup, &QButtonGroup::idClicked, this, onSettingsChanged);
    connect(m_vAlignGroup, &QButtonGroup::idClicked, this, onSettingsChanged);
    connect(m_dirGroup, &QButtonGroup::idClicked, this, onSettingsChanged);

    connect(m_shapeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, onSettingsChanged]() {
        updateSpecialShapeVisibility();
        onSettingsChanged();
    });
    connect(m_spikeCountSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, onSettingsChanged);
    connect(m_spikeDepthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onSettingsChanged);
    connect(m_spikePhaseSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onSettingsChanged);
    connect(m_spikeRandomSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, onSettingsChanged);
    connect(m_btnRandomizeSpikes, &QPushButton::clicked, this, [this, onSettingsChanged]() {
        if (m_spikeRandomSpin->value() == 0) {
            m_updatingUi = true;
            m_spikeRandomSpin->setValue(30);
            m_updatingUi = false;
        }
        m_currentSeed = QRandomGenerator::global()->generate();
        onSettingsChanged();
    });

    connect(m_hasTailCheck, &QCheckBox::toggled, this, [this, onSettingsChanged](bool checked) {
        m_tailParamsWidget->setVisible(checked);
        onSettingsChanged();
    });
    connect(m_tailBaseWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onSettingsChanged);
    connect(m_tailCurvatureSlider, &QSlider::valueChanged, this, [this, onSettingsChanged](int val) {
        if (val == 0) m_tailCurvatureLabel->setText("直線");
        else m_tailCurvatureLabel->setText(QString::number(val));
        onSettingsChanged();
    });

    connect(m_frameWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onSettingsChanged);
    connect(m_btnFrameColor, &QPushButton::clicked, this, [this, onSettingsChanged]() {
        QColor col = QColorDialog::getColor(m_frameColor, this, "枠線の色を選択");
        if (col.isValid()) {
            m_frameColor = col;
            updateColorButtons();
            onSettingsChanged();
        }
    });

    connect(m_btnFillColor, &QPushButton::clicked, this, [this, onSettingsChanged]() {
        QColor col = QColorDialog::getColor(m_fillColor, this, "背景色を選択");
        if (col.isValid()) {
            m_fillColor = col;
            m_transparentFillCheck->setChecked(false);
            updateColorButtons();
            onSettingsChanged();
        }
    });
    connect(m_transparentFillCheck, &QCheckBox::toggled, this, [this, onSettingsChanged](bool transparent) {
        if (transparent) {
            m_fillColor.setAlpha(0);
        } else if (m_fillColor.alpha() == 0) {
            m_fillColor = Qt::white;
        }
        updateColorButtons();
        onSettingsChanged();
    });

    connect(m_hasOutlineCheck, &QCheckBox::toggled, this, [this, onSettingsChanged](bool checked) {
        m_outlineParamsWidget->setVisible(checked);
        onSettingsChanged();
    });
    connect(m_outlineWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onSettingsChanged);
    connect(m_btnOutlineColor, &QPushButton::clicked, this, [this, onSettingsChanged]() {
        QColor col = QColorDialog::getColor(m_outlineColor, this, "フチ取りの色を選択");
        if (col.isValid()) {
            m_outlineColor = col;
            updateColorButtons();
            onSettingsChanged();
        }
    });

    updateColorButtons();
    updateSpecialShapeVisibility();
    m_tailParamsWidget->setVisible(m_hasTailCheck->isChecked());
    m_outlineParamsWidget->setVisible(m_hasOutlineCheck->isChecked());
}

void TextSettingsWidget::updateColorButtons() {
    auto makeBtnStyle = [](const QColor& c) {
        QString textCol = (c.lightness() < 128 && c.alpha() > 100) ? "#ffffff" : "#000000";
        if (c.alpha() == 0) {
            return QString("QPushButton { background: #e0e0e0; color: #888888; border: 1px dashed #999; }");
        }
        return QString("QPushButton { background-color: %1; color: %2; border: 1px solid #777; border-radius: 2px; font-weight: bold; }")
            .arg(c.name(QColor::HexArgb), textCol);
    };

    if (m_btnTextColor) m_btnTextColor->setStyleSheet(makeBtnStyle(m_textColor));
    if (m_btnFrameColor) m_btnFrameColor->setStyleSheet(makeBtnStyle(m_frameColor));
    if (m_btnFillColor) m_btnFillColor->setStyleSheet(makeBtnStyle(m_transparentFillCheck->isChecked() ? Qt::transparent : m_fillColor));
    if (m_btnOutlineColor) m_btnOutlineColor->setStyleSheet(makeBtnStyle(m_outlineColor));
}

void TextSettingsWidget::updateSpecialShapeVisibility() {
    auto shape = balloonShape();
    bool isSpecial = (shape == BalloonShape::Spike || shape == BalloonShape::Cloud);
    m_specialShapeWidget->setVisible(isSpecial);
    bool isNone = (shape == BalloonShape::None);
    if (m_hasTailCheck) {
        m_hasTailCheck->setEnabled(!isNone);
    }
    if (m_tailParamsWidget) {
        m_tailParamsWidget->setVisible(!isNone && m_hasTailCheck->isChecked());
    }
}

QFont TextSettingsWidget::currentFont() const {
    QFont font = m_fontCombo->currentFont();
    font.setPointSize(m_fontSizeSpin->value());
    font.setBold(m_btnBold->isChecked());
    font.setItalic(m_btnItalic->isChecked());
    return font;
}

int TextSettingsWidget::fontSize() const {
    return m_fontSizeSpin->value();
}

bool TextSettingsWidget::isBold() const {
    return m_btnBold->isChecked();
}

bool TextSettingsWidget::isItalic() const {
    return m_btnItalic->isChecked();
}

Qt::Alignment TextSettingsWidget::horizontalAlignment() const {
    if (m_btnAlignCenter->isChecked()) return Qt::AlignHCenter;
    if (m_btnAlignRight->isChecked()) return Qt::AlignRight;
    return Qt::AlignLeft;
}

Qt::Alignment TextSettingsWidget::verticalAlignment() const {
    if (m_btnAlignVCenter->isChecked()) return Qt::AlignVCenter;
    if (m_btnAlignBottom->isChecked()) return Qt::AlignBottom;
    return Qt::AlignTop;
}

QString TextSettingsWidget::text() const {
    if (!m_hideTags && m_textEdit) {
        return m_textEdit->toPlainText();
    }
    return m_rawText;
}

void TextSettingsWidget::setText(const QString& text) {
    m_rawText = text;
    updateTextDisplay();
}

void TextSettingsWidget::setHideTags(bool hide) {
    if (m_hideTags == hide) return;
    if (!m_hideTags && m_textEdit) {
        m_rawText = m_textEdit->toPlainText();
    }
    m_hideTags = hide;
    if (m_btnShowTags && m_btnHideTags) {
        m_updatingUi = true;
        m_btnShowTags->setChecked(!hide);
        m_btnHideTags->setChecked(hide);
        m_updatingUi = false;
    }
    if (m_zoomLabel && m_previewZoomSpin) {
        m_zoomLabel->setEnabled(hide);
        m_previewZoomSpin->setEnabled(hide);
    }
    updateTextDisplay();
}

int TextSettingsWidget::previewZoom() const {
    return m_previewZoomSpin ? m_previewZoomSpin->value() : 100;
}

void TextSettingsWidget::setPreviewZoom(int zoom) {
    if (m_previewZoomSpin) {
        m_previewZoomSpin->setValue(zoom);
    }
}

BalloonShape TextSettingsWidget::balloonShape() const {
    return static_cast<BalloonShape>(m_shapeCombo->currentData().toInt());
}

bool TextSettingsWidget::hasTail() const {
    if (balloonShape() == BalloonShape::None) return false;
    return m_hasTailCheck->isChecked();
}

float TextSettingsWidget::tailBaseWidth() const {
    return static_cast<float>(m_tailBaseWidthSpin->value());
}

float TextSettingsWidget::tailCurvature() const {
    return static_cast<float>(m_tailCurvatureSlider->value());
}

int TextSettingsWidget::spikeCount() const {
    return m_spikeCountSpin->value();
}

float TextSettingsWidget::spikeDepth() const {
    return static_cast<float>(m_spikeDepthSpin->value());
}

float TextSettingsWidget::spikePhase() const {
    return static_cast<float>(m_spikePhaseSpin->value());
}

int TextSettingsWidget::spikeRandomness() const {
    return m_spikeRandomSpin->value();
}

uint32_t TextSettingsWidget::randomSeed() const {
    return m_currentSeed;
}

float TextSettingsWidget::frameWidth() const {
    return static_cast<float>(m_frameWidthSpin->value());
}

WritingDirection TextSettingsWidget::direction() const {
    if (m_btnVertical && m_btnVertical->isChecked()) {
        return WritingDirection::Vertical;
    }
    return WritingDirection::Horizontal;
}

void TextSettingsWidget::setDirection(WritingDirection dir) {
    if (dir == WritingDirection::Vertical) {
        if (m_btnVertical) m_btnVertical->setChecked(true);
    } else {
        if (m_btnHorizontal) m_btnHorizontal->setChecked(true);
    }
}

QColor TextSettingsWidget::textColor() const {
    return m_textColor;
}

void TextSettingsWidget::setTextColor(const QColor& color) {
    m_textColor = color;
    updateColorButtons();
}

QColor TextSettingsWidget::frameColor() const {
    return m_frameColor;
}

QColor TextSettingsWidget::fillColor() const {
    if (m_transparentFillCheck->isChecked()) {
        return Qt::transparent;
    }
    return m_fillColor;
}

bool TextSettingsWidget::isTransparentFill() const {
    return m_transparentFillCheck->isChecked();
}

bool TextSettingsWidget::hasOutline() const {
    return m_hasOutlineCheck->isChecked();
}

float TextSettingsWidget::outlineWidth() const {
    return static_cast<float>(m_outlineWidthSpin->value());
}

QColor TextSettingsWidget::outlineColor() const {
    return m_outlineColor;
}

void TextSettingsWidget::setFromItem(const TextBalloonItem& item) {
    m_updatingUi = true;

    // 1. テキスト & フォント & 文字色 & 書字方向
    m_rawText = item.text;
    updateTextDisplay();
    m_fontCombo->setCurrentFont(item.font);
    m_fontSizeSpin->setValue(item.font.pointSize() > 0 ? item.font.pointSize() : 16);
    m_btnBold->setChecked(item.font.bold());
    m_btnItalic->setChecked(item.font.italic());
    m_textColor = item.textColor;
    setDirection(item.direction);

    // 2. 配置
    if (item.horizontalAlign == Qt::AlignHCenter) {
        m_btnAlignCenter->setChecked(true);
    } else if (item.horizontalAlign == Qt::AlignRight) {
        m_btnAlignRight->setChecked(true);
    } else {
        m_btnAlignLeft->setChecked(true);
    }

    if (item.verticalAlign == Qt::AlignVCenter) {
        m_btnAlignVCenter->setChecked(true);
    } else if (item.verticalAlign == Qt::AlignBottom) {
        m_btnAlignBottom->setChecked(true);
    } else {
        m_btnAlignTop->setChecked(true);
    }

    // 3. 形状
    int shapeIdx = m_shapeCombo->findData(static_cast<int>(item.shape));
    if (shapeIdx >= 0) m_shapeCombo->setCurrentIndex(shapeIdx);

    // 4. 特殊形状
    m_spikeCountSpin->setValue(item.spikeCount);
    m_spikeDepthSpin->setValue(item.spikeDepth);
    m_spikePhaseSpin->setValue(item.spikePhase);
    m_spikeRandomSpin->setValue(item.spikeRandomness);
    m_currentSeed = item.randomSeed;

    // 5. しっぽ
    m_hasTailCheck->setChecked(item.hasTail);
    m_tailBaseWidthSpin->setValue(item.tailBaseWidth);
    m_tailCurvatureSlider->setValue(static_cast<int>(item.tailCurvature));
    if (static_cast<int>(item.tailCurvature) == 0) m_tailCurvatureLabel->setText("直線");
    else m_tailCurvatureLabel->setText(QString::number(static_cast<int>(item.tailCurvature)));

    // 6. スタイル (枠線・背景・フチ取り)
    m_frameWidthSpin->setValue(item.frameWidth);
    m_frameColor = item.frameColor;
    m_fillColor = item.fillColor;
    m_transparentFillCheck->setChecked(item.fillColor.alpha() == 0);

    m_hasOutlineCheck->setChecked(item.hasOutline);
    m_outlineWidthSpin->setValue(item.outlineWidth);
    m_outlineColor = item.outlineColor;

    updateColorButtons();
    updateSpecialShapeVisibility();
    m_tailParamsWidget->setVisible(item.shape != BalloonShape::None && item.hasTail);
    m_outlineParamsWidget->setVisible(item.hasOutline);

    m_updatingUi = false;
}

void TextSettingsWidget::applyToItem(TextBalloonItem& item) const {
    item.text = m_textEdit->toPlainText();
    item.font = currentFont();
    item.textColor = textColor();
    item.direction = direction();
    item.horizontalAlign = horizontalAlignment();
    item.verticalAlign = verticalAlignment();

    item.shape = balloonShape();
    if (item.spikeCount != spikeCount() || item.spikeRandomness != spikeRandomness() || item.randomSeed != randomSeed()) {
        item.customVertices.clear();
    }
    item.spikeCount = spikeCount();
    item.spikeDepth = spikeDepth();
    item.spikePhase = spikePhase();
    item.spikeRandomness = spikeRandomness();
    item.randomSeed = randomSeed();

    item.hasTail = hasTail();
    item.tailBaseWidth = tailBaseWidth();
    item.tailCurvature = tailCurvature();

    item.frameWidth = frameWidth();
    item.frameColor = frameColor();
    item.fillColor = fillColor();

    item.hasOutline = hasOutline();
    item.outlineWidth = outlineWidth();
    item.outlineColor = outlineColor();
}

void TextSettingsWidget::focusTextEdit() {
    if (m_tabWidget) {
        m_tabWidget->setCurrentIndex(0);
    }
    if (m_hideTags) {
        setHideTags(false);
    }
    m_textEdit->setFocus();
    m_textEdit->selectAll();
}

bool TextSettingsWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_textEdit && m_hideTags) {
        if (event->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            bool isModifierOnly = (keyEvent->key() >= Qt::Key_Shift && keyEvent->key() <= Qt::Key_Alt) ||
                                  keyEvent->key() == Qt::Key_Control || keyEvent->key() == Qt::Key_Meta;
            bool isCopy = (keyEvent->modifiers() & Qt::ControlModifier) && keyEvent->key() == Qt::Key_C;
            bool isNav = keyEvent->key() == Qt::Key_Left || keyEvent->key() == Qt::Key_Right ||
                         keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down;

            if (!isModifierOnly && !isCopy && !isNav) {
                // 文字入力やBackspace等の編集キーが押されたら、自動でタグあり表示に遷移して編集可能にする
                setHideTags(false);
                return false;
            }
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            // ダブルクリック時もタグあり表示に遷移して直接テキスト編集可能にする
            setHideTags(false);
            return false;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TextSettingsWidget::updateTextDisplay() {
    m_updatingUi = true;
    if (m_hideTags) {
        m_textEdit->setHtml(convertTagsToHtml(m_rawText));
        m_textEdit->setReadOnly(true);
        rebuildDisplayMapping();
    } else {
        m_textEdit->setPlainText(m_rawText);
        m_textEdit->setReadOnly(false);
    }
    m_updatingUi = false;
}

void TextSettingsWidget::rebuildDisplayMapping() {
    m_displayToRawMap.clear();
    int len = m_rawText.length();
    int i = 0;
    while (i < len) {
        // 1. タグ <...> のスキップ
        if (m_rawText[i] == QLatin1Char('<')) {
            int closeTag = m_rawText.indexOf(QLatin1Char('>'), i + 1);
            if (closeTag != -1) {
                i = closeTag + 1;
                continue;
            }
        }
        // 2. Markdown ** のスキップ
        if (i + 1 < len && m_rawText[i] == QLatin1Char('*') && m_rawText[i + 1] == QLatin1Char('*')) {
            i += 2;
            continue;
        }
        // 3. 青空文庫 | または ｜ のスキップ
        if (m_rawText[i] == QLatin1Char('|') || m_rawText[i] == QChar(0xFF5C)) {
            ++i;
            continue;
        }
        // 4. ルビ 《...》 のスキップ
        if (m_rawText[i] == QChar(0x300A)) {
            int rubyEnd = m_rawText.indexOf(QChar(0x300B), i + 1);
            if (rubyEnd != -1) {
                i = rubyEnd + 1;
                continue;
            }
        }
        // 5. Markdown [親文字](ルビ) の [ と ](ルビ) のスキップ
        if (m_rawText[i] == QLatin1Char('[')) {
            ++i;
            continue;
        }
        if (m_rawText[i] == QLatin1Char(']')) {
            if (i + 1 < len && m_rawText[i + 1] == QLatin1Char('(')) {
                int closeParen = m_rawText.indexOf(QLatin1Char(')'), i + 2);
                if (closeParen != -1) {
                    i = closeParen + 1;
                    continue;
                }
            }
        }

        // 表示される本文文字（改行含む）
        m_displayToRawMap.push_back(i);
        ++i;
    }
}

QString TextSettingsWidget::convertTagsToHtml(const QString& rawText) const {
    QStringList lines = rawText.split(QLatin1Char('\n'));
    QString html;
    qreal zoomFactor = (m_previewZoomSpin ? m_previewZoomSpin->value() : 100) / 100.0;
    qreal basePt = fontSize() * zoomFactor;

    for (int l = 0; l < lines.size(); ++l) {
        if (l > 0) html += "<br>";
        auto tokens = parseFormattedTokens(lines[l], basePt);
        for (const auto& tok : tokens) {
            QString spanStyle;
            if (tok.style.color.has_value()) {
                spanStyle += QString("color:%1;").arg(tok.style.color->name(QColor::HexRgb));
            }
            if (tok.style.fontSize.has_value()) {
                spanStyle += QString("font-size:%1pt;").arg((*tok.style.fontSize) * zoomFactor);
            } else {
                spanStyle += QString("font-size:%1pt;").arg(basePt);
            }

            QString content = tok.baseText.toHtmlEscaped();
            if (tok.style.bold) content = "<b>" + content + "</b>";
            if (tok.style.italic) content = "<i>" + content + "</i>";

            if (!spanStyle.isEmpty()) {
                content = QString("<span style=\"%1\">%2</span>").arg(spanStyle, content);
            }

            if (tok.hasRuby && !tok.rubyText.isEmpty()) {
                QString rubyEscaped = tok.rubyText.toHtmlEscaped();
                content += QString("<span style=\"font-size:%1pt;vertical-align:super;color:#888888;\">(%2)</span>")
                               .arg(basePt * 0.7, 0, 'f', 1)
                               .arg(rubyEscaped);
            }

            html += content;
        }
    }
    return html;
}

void TextSettingsWidget::wrapSelectionWithTags(const QString& openTag, const QString& closeTag) {
    if (!m_textEdit) return;

    if (!m_hideTags) {
        // タグあり表示モード
        QTextCursor cursor = m_textEdit->textCursor();
        if (cursor.hasSelection()) {
            QString selected = cursor.selectedText();
            cursor.insertText(openTag + selected + closeTag);
        } else {
            int pos = cursor.position();
            cursor.insertText(openTag + closeTag);
            cursor.setPosition(pos + openTag.length());
        }
        m_textEdit->setTextCursor(cursor);
        m_rawText = m_textEdit->toPlainText();
        emit textChanged(m_rawText);
        emit settingsChanged();
        m_textEdit->setFocus();
    } else {
        // タグ無し表示モード: モード遷移せず、生テキストの対応箇所にタグを挿入！
        QTextCursor cursor = m_textEdit->textCursor();
        if (cursor.hasSelection()) {
            int selStart = cursor.selectionStart();
            int selEnd = cursor.selectionEnd();
            if (selStart >= 0 && selEnd <= static_cast<int>(m_displayToRawMap.size()) && selStart < selEnd) {
                int rawStart = m_displayToRawMap[selStart];
                int rawEnd = m_displayToRawMap[selEnd - 1] + 1;
                QString target = m_rawText.mid(rawStart, rawEnd - rawStart);
                m_rawText.replace(rawStart, rawEnd - rawStart, openTag + target + closeTag);

                updateTextDisplay();
                emit textChanged(m_rawText);
                emit settingsChanged();

                // 選択範囲を復元
                QTextCursor newCursor = m_textEdit->textCursor();
                newCursor.setPosition(selStart);
                newCursor.setPosition(selEnd, QTextCursor::KeepAnchor);
                m_textEdit->setTextCursor(newCursor);
                m_textEdit->setFocus();
                return;
            }
        }
        // 選択がない場合
        int pos = cursor.position();
        int rawPos = (pos >= 0 && pos < static_cast<int>(m_displayToRawMap.size()))
                     ? m_displayToRawMap[pos] : m_rawText.length();
        m_rawText.insert(rawPos, openTag + closeTag);
        updateTextDisplay();
        emit textChanged(m_rawText);
        emit settingsChanged();
        m_textEdit->setFocus();
    }
}

void TextSettingsWidget::onInlineBoldClicked() {
    wrapSelectionWithTags("<b>", "</b>");
}

void TextSettingsWidget::onInlineItalicClicked() {
    wrapSelectionWithTags("<i>", "</i>");
}

void TextSettingsWidget::onInlineColorClicked() {
    QColor c = QColorDialog::getColor(textColor(), this, "選択部分の文字色を選択");
    if (c.isValid()) {
        wrapSelectionWithTags(QString("<color=%1>").arg(c.name(QColor::HexRgb)), "</color>");
    }
}

void TextSettingsWidget::onInlineSizeClicked() {
    int sz = m_spinInlineSize ? m_spinInlineSize->value() : fontSize();
    wrapSelectionWithTags(QString("<size=%1>").arg(sz), "</size>");
}

void TextSettingsWidget::onInlineRubyClicked() {
    if (!m_textEdit) return;

    QTextCursor cursor = m_textEdit->textCursor();
    QString selected = cursor.selectedText();
    bool ok = false;
    QString prompt = selected.isEmpty() ? "ルビ（ふりがな）を入力:" : QString("「%1」のルビ（ふりがな）を入力:").arg(selected);
    QString ruby = QInputDialog::getText(this, "ルビ設定", prompt, QLineEdit::Normal, "", &ok);
    if (!ok || ruby.isEmpty()) return;

    if (!m_hideTags) {
        // タグあり表示モード
        if (selected.isEmpty()) {
            cursor.insertText(QString("｜親文字《%1》").arg(ruby));
        } else {
            // 漢字以外でも確実に動作するよう、青空文庫標準の ｜ を親文字の前に付ける
            cursor.insertText(QString("｜%1《%2》").arg(selected, ruby));
        }
        m_textEdit->setTextCursor(cursor);
        m_rawText = m_textEdit->toPlainText();
        emit textChanged(m_rawText);
        emit settingsChanged();
        m_textEdit->setFocus();
    } else {
        // タグ無し表示モード: モード遷移せず、生テキストにルビタグを反映
        if (cursor.hasSelection()) {
            int selStart = cursor.selectionStart();
            int selEnd = cursor.selectionEnd();
            if (selStart >= 0 && selEnd <= static_cast<int>(m_displayToRawMap.size()) && selStart < selEnd) {
                int rawStart = m_displayToRawMap[selStart];
                int rawEnd = m_displayToRawMap[selEnd - 1] + 1;
                QString target = m_rawText.mid(rawStart, rawEnd - rawStart);
                QString replacement = QString("｜%1《%2》").arg(target, ruby);
                m_rawText.replace(rawStart, rawEnd - rawStart, replacement);

                updateTextDisplay();
                emit textChanged(m_rawText);
                emit settingsChanged();

                // 選択範囲を復元
                QTextCursor newCursor = m_textEdit->textCursor();
                newCursor.setPosition(selStart);
                newCursor.setPosition(selEnd, QTextCursor::KeepAnchor);
                m_textEdit->setTextCursor(newCursor);
                m_textEdit->setFocus();
                return;
            }
        }

        // 選択がない場合
        int pos = cursor.position();
        int rawPos = (pos >= 0 && pos < static_cast<int>(m_displayToRawMap.size()))
                     ? m_displayToRawMap[pos] : m_rawText.length();
        m_rawText.insert(rawPos, QString("｜親文字《%1》").arg(ruby));
        updateTextDisplay();
        emit textChanged(m_rawText);
        emit settingsChanged();
        m_textEdit->setFocus();
    }
}

} // namespace Kaburagi
