#include "NewCanvasDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QColorDialog>
#include <QSettings>

namespace Kaburagi {

NewCanvasDialog::NewCanvasDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("新規キャンバス作成");
    setModal(true);
    resize(360, 320);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    // プリセット
    auto presetLayout = new QHBoxLayout();
    m_presetCombo = new QComboBox(this);
    m_presetCombo->addItem("カスタム");
    m_presetCombo->addItem("イラスト標準 (1600 x 1200, 350 dpi)");
    m_presetCombo->addItem("フルHD (1920 x 1080, 72 dpi)");
    m_presetCombo->addItem("A4 (2894 x 4093, 350 dpi)");
    m_presetCombo->addItem("B5 (2508 x 3541, 350 dpi)");
    m_presetCombo->addItem("正方形 (1000 x 1000, 350 dpi)");
    presetLayout->addWidget(m_presetCombo);
    mainLayout->addLayout(presetLayout);

    // キャンバス寸法グループ
    auto sizeGroup = new QGroupBox("キャンバスサイズ", this);
    auto sizeForm = new QFormLayout(sizeGroup);

    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setRange(10, 10000);
    m_widthSpin->setSuffix(" px");
    m_widthSpin->setValue(1600);
    sizeForm->addRow("幅(&W):", m_widthSpin);

    m_heightSpin = new QSpinBox(this);
    m_heightSpin->setRange(10, 10000);
    m_heightSpin->setSuffix(" px");
    m_heightSpin->setValue(1200);
    sizeForm->addRow("高さ(&H):", m_heightSpin);

    m_dpiSpin = new QDoubleSpinBox(this);
    m_dpiSpin->setRange(72.0, 1200.0);
    m_dpiSpin->setDecimals(1);
    m_dpiSpin->setSuffix(" dpi");
    m_dpiSpin->setValue(350.0);
    sizeForm->addRow("解像度(&D):", m_dpiSpin);

    mainLayout->addWidget(sizeGroup);

    // 背景色設定グループ
    auto bgGroup = new QGroupBox("背景色設定", this);
    auto bgForm = new QFormLayout(bgGroup);

    m_bgModeCombo = new QComboBox(this);
    m_bgModeCombo->addItem("白 (推奨)");
    m_bgModeCombo->addItem("透明");
    m_bgModeCombo->addItem("ユーザー指定色");
    bgForm->addRow("背景色(&B):", m_bgModeCombo);

    m_bgColorBtn = new QPushButton("指定色の変更...", this);
    m_bgColorBtn->setEnabled(false);
    bgForm->addRow("背景指定色:", m_bgColorBtn);

    m_bg2ColorBtn = new QPushButton("第2背景色を変更...", this);
    bgForm->addRow("第2背景色(&S):", m_bg2ColorBtn);

    mainLayout->addWidget(bgGroup);

    // ボタン
    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText("OK");
    buttonBox->button(QDialogButtonBox::Cancel)->setText("キャンセル");
    connect(buttonBox, &QDialogButtonBox::accepted, this, &NewCanvasDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    // シグナル接続
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NewCanvasDialog::onPresetChanged);
    connect(m_bgModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NewCanvasDialog::onBgModeChanged);
    connect(m_bgColorBtn, &QPushButton::clicked, this, &NewCanvasDialog::onPickBgColor);
    connect(m_bg2ColorBtn, &QPushButton::clicked, this, &NewCanvasDialog::onPickBg2Color);

    loadSettings();
    updateColorButtons();
}

void NewCanvasDialog::loadSettings() {
    QSettings settings;
    settings.beginGroup("NewCanvas");
    m_widthSpin->setValue(settings.value("width", 1600).toInt());
    m_heightSpin->setValue(settings.value("height", 1200).toInt());
    m_dpiSpin->setValue(settings.value("dpi", 350.0).toDouble());
    int bgMode = settings.value("bgMode", 0).toInt();
    m_bgModeCombo->setCurrentIndex(std::clamp(bgMode, 0, 2));

    QString bgColorStr = settings.value("bgColor", "#ffffff").toString();
    m_customBgColor = QColor(bgColorStr);
    if (!m_customBgColor.isValid()) m_customBgColor = Qt::white;

    QString bg2ColorStr = settings.value("bg2Color", "#ffffff").toString();
    m_secondaryBgColor = QColor(bg2ColorStr);
    if (!m_secondaryBgColor.isValid()) m_secondaryBgColor = Qt::white;
    settings.endGroup();

    onBgModeChanged(m_bgModeCombo->currentIndex());
}

void NewCanvasDialog::saveSettings() {
    QSettings settings;
    settings.beginGroup("NewCanvas");
    settings.setValue("width", m_widthSpin->value());
    settings.setValue("height", m_heightSpin->value());
    settings.setValue("dpi", m_dpiSpin->value());
    settings.setValue("bgMode", m_bgModeCombo->currentIndex());
    settings.setValue("bgColor", m_customBgColor.name(QColor::HexArgb));
    settings.setValue("bg2Color", m_secondaryBgColor.name(QColor::HexArgb));
    settings.endGroup();
}

void NewCanvasDialog::accept() {
    saveSettings();
    QDialog::accept();
}

int NewCanvasDialog::canvasWidth() const {
    return m_widthSpin->value();
}

int NewCanvasDialog::canvasHeight() const {
    return m_heightSpin->value();
}

double NewCanvasDialog::canvasDpi() const {
    return m_dpiSpin->value();
}

QColor NewCanvasDialog::backgroundColor() const {
    switch (m_bgModeCombo->currentIndex()) {
    case 0: return Qt::white;
    case 1: return QColor(0, 0, 0, 0); // 透明
    case 2: return m_customBgColor;
    default: return Qt::white;
    }
}

QColor NewCanvasDialog::secondaryBackgroundColor() const {
    return m_secondaryBgColor;
}

void NewCanvasDialog::onPresetChanged(int index) {
    switch (index) {
    case 1: // イラスト標準
        m_widthSpin->setValue(1600);
        m_heightSpin->setValue(1200);
        m_dpiSpin->setValue(350.0);
        break;
    case 2: // フルHD
        m_widthSpin->setValue(1920);
        m_heightSpin->setValue(1080);
        m_dpiSpin->setValue(72.0);
        break;
    case 3: // A4
        m_widthSpin->setValue(2894);
        m_heightSpin->setValue(4093);
        m_dpiSpin->setValue(350.0);
        break;
    case 4: // B5
        m_widthSpin->setValue(2508);
        m_heightSpin->setValue(3541);
        m_dpiSpin->setValue(350.0);
        break;
    case 5: // 正方形
        m_widthSpin->setValue(1000);
        m_heightSpin->setValue(1000);
        m_dpiSpin->setValue(350.0);
        break;
    default:
        break;
    }
}

void NewCanvasDialog::onBgModeChanged(int index) {
    m_bgColorBtn->setEnabled(index == 2);
    updateColorButtons();
}

void NewCanvasDialog::onPickBgColor() {
    QColor c = QColorDialog::getColor(m_customBgColor, this, "背景色の選択", QColorDialog::ShowAlphaChannel | QColorDialog::DontUseNativeDialog);
    if (c.isValid()) {
        m_customBgColor = c;
        updateColorButtons();
    }
}

void NewCanvasDialog::onPickBg2Color() {
    QColor c = QColorDialog::getColor(m_secondaryBgColor, this, "第2背景色の選択", QColorDialog::ShowAlphaChannel | QColorDialog::DontUseNativeDialog);
    if (c.isValid()) {
        m_secondaryBgColor = c;
        updateColorButtons();
    }
}

void NewCanvasDialog::updateColorButtons() {
    if (m_bgModeCombo->currentIndex() == 2) {
        m_bgColorBtn->setStyleSheet(QString("background-color: %1; color: %2; font-weight: bold; padding: 3px;")
            .arg(m_customBgColor.name())
            .arg(m_customBgColor.lightness() > 128 ? "#000000" : "#ffffff"));
        m_bgColorBtn->setText(QString("指定色: %1").arg(m_customBgColor.name()));
    } else {
        m_bgColorBtn->setStyleSheet("");
        m_bgColorBtn->setText("指定色の変更...");
    }

    m_bg2ColorBtn->setStyleSheet(QString("background-color: %1; color: %2; font-weight: bold; padding: 3px;")
        .arg(m_secondaryBgColor.name())
        .arg(m_secondaryBgColor.lightness() > 128 ? "#000000" : "#ffffff"));
    m_bg2ColorBtn->setText(QString("第2背景色: %1").arg(m_secondaryBgColor.name()));
}

} // namespace Kaburagi
