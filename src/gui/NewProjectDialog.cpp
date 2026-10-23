#include "NewProjectDialog.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFileDialog>
#include <QStandardPaths>
#include <QMessageBox>

namespace Kaburagi {

struct PaperPreset {
    const char* name;
    int width;
    int height;
    int dpi;
};

static const PaperPreset PAPER_PRESETS[] = {
    { "B5 同人誌 / 商業誌 (350 dpi)", 2508, 3541, 350 },
    { "B5 モノクロ原稿 (600 dpi)", 4300, 6071, 600 },
    { "A4 カラーイラスト (350 dpi)", 2894, 4093, 350 },
    { "A4 一般 (300 dpi)", 2480, 3508, 300 },
    { "B6 単行本 (350 dpi)", 1795, 2551, 350 },
    { "カスタム", 0, 0, 0 }
};

NewProjectDialog::NewProjectDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("複数ページ作品の新規作成"));
    setModal(true);
    resize(460, 520);

    setupUI();
}

void NewProjectDialog::setupUI() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // 1. 作品基本情報
    auto basicGroup = new QGroupBox(tr("作品情報"), this);
    auto basicLayout = new QFormLayout(basicGroup);

    m_nameEdit = new QLineEdit(tr("新規マンガ作品"), basicGroup);
    basicLayout->addRow(tr("作品名:"), m_nameEdit);

    auto dirRow = new QHBoxLayout();
    QString defaultDocPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    m_dirEdit = new QLineEdit(defaultDocPath, basicGroup);
    m_browseBtn = new QPushButton(tr("参照..."), basicGroup);
    dirRow->addWidget(m_dirEdit);
    dirRow->addWidget(m_browseBtn);
    basicLayout->addRow(tr("保存先フォルダー:"), dirRow);

    mainLayout->addWidget(basicGroup);

    // 2. ページ数
    auto pageGroup = new QGroupBox(tr("ページ設定"), this);
    auto pageLayout = new QVBoxLayout(pageGroup);

    auto spinRow = new QHBoxLayout();
    spinRow->addWidget(new QLabel(tr("ページ数:"), pageGroup));
    m_pageCountSpin = new QSpinBox(pageGroup);
    m_pageCountSpin->setRange(1, 500);
    m_pageCountSpin->setValue(8);
    m_pageCountSpin->setSuffix(tr(" ページ"));
    m_pageCountSpin->setMinimumWidth(100);
    spinRow->addWidget(m_pageCountSpin);
    spinRow->addStretch(1);
    pageLayout->addLayout(spinRow);

    // プリセットボタン (4p, 8p, 16p, 24p, 32p)
    auto presetBtnRow = new QHBoxLayout();
    presetBtnRow->addWidget(new QLabel(tr("クイック選択:"), pageGroup));
    int quickPages[] = { 4, 8, 16, 24, 32 };
    for (int p : quickPages) {
        auto btn = new QPushButton(QString("%1p").arg(p), pageGroup);
        btn->setMaximumWidth(45);
        connect(btn, &QPushButton::clicked, this, [this, p]() {
            m_pageCountSpin->setValue(p);
        });
        presetBtnRow->addWidget(btn);
    }
    presetBtnRow->addStretch(1);
    pageLayout->addLayout(presetBtnRow);

    mainLayout->addWidget(pageGroup);

    // 3. 用紙サイズ・解像度
    auto paperGroup = new QGroupBox(tr("用紙・キャンバス設定"), this);
    auto paperLayout = new QFormLayout(paperGroup);

    m_presetCombo = new QComboBox(paperGroup);
    for (const auto& preset : PAPER_PRESETS) {
        m_presetCombo->addItem(preset.name);
    }
    paperLayout->addRow(tr("プリセット:"), m_presetCombo);

    auto sizeRow = new QHBoxLayout();
    m_widthSpin = new QSpinBox(paperGroup);
    m_widthSpin->setRange(100, 20000);
    m_widthSpin->setValue(PAPER_PRESETS[0].width);
    m_widthSpin->setSuffix(" px");

    m_heightSpin = new QSpinBox(paperGroup);
    m_heightSpin->setRange(100, 20000);
    m_heightSpin->setValue(PAPER_PRESETS[0].height);
    m_heightSpin->setSuffix(" px");

    sizeRow->addWidget(new QLabel(tr("幅:")));
    sizeRow->addWidget(m_widthSpin);
    sizeRow->addWidget(new QLabel(tr("高さ:")));
    sizeRow->addWidget(m_heightSpin);
    paperLayout->addRow(tr("キャンバスサイズ:"), sizeRow);

    m_dpiSpin = new QSpinBox(paperGroup);
    m_dpiSpin->setRange(72, 1200);
    m_dpiSpin->setValue(PAPER_PRESETS[0].dpi);
    m_dpiSpin->setSuffix(" dpi");
    paperLayout->addRow(tr("解像度:"), m_dpiSpin);

    mainLayout->addWidget(paperGroup);

    // 4. 綴じ方向 & 開始ページ
    auto bindingGroup = new QGroupBox(tr("製本・レイアウト"), this);
    auto bindingLayout = new QFormLayout(bindingGroup);

    auto bindRow = new QHBoxLayout();
    m_radioRightToLeft = new QRadioButton(tr("右綴じ (商業誌 / 日本マンガ)"), bindingGroup);
    m_radioLeftToRight = new QRadioButton(tr("左綴じ (横書き / イラスト集)"), bindingGroup);
    m_radioRightToLeft->setChecked(true);
    bindRow->addWidget(m_radioRightToLeft);
    bindRow->addWidget(m_radioLeftToRight);
    bindingLayout->addRow(tr("綴じ方向:"), bindRow);

    auto startRow = new QHBoxLayout();
    m_radioStartRight = new QRadioButton(tr("右から (通常表紙・1p単独)"), bindingGroup);
    m_radioStartLeft = new QRadioButton(tr("左から (見開き開始など)"), bindingGroup);
    m_radioStartRight->setChecked(true);
    startRow->addWidget(m_radioStartRight);
    startRow->addWidget(m_radioStartLeft);
    bindingLayout->addRow(tr("開始ページ:"), startRow);

    mainLayout->addWidget(bindingGroup);

    // 5. ボタン (作成 / キャンセル)
    auto btnRow = new QHBoxLayout();
    btnRow->addStretch(1);

    m_cancelBtn = new QPushButton(tr("キャンセル"), this);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(m_cancelBtn);

    m_createBtn = new QPushButton(tr("作品を作成"), this);
    m_createBtn->setDefault(true);
    m_createBtn->setStyleSheet("QPushButton { font-weight: bold; padding: 6px 16px; }");
    connect(m_createBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(m_createBtn);

    mainLayout->addLayout(btnRow);

    // シグナル接続
    connect(m_browseBtn, &QPushButton::clicked, this, &NewProjectDialog::onBrowseDirectory);
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NewProjectDialog::onPresetChanged);
    connect(m_nameEdit, &QLineEdit::textChanged, this, &NewProjectDialog::validateInput);
    connect(m_dirEdit, &QLineEdit::textChanged, this, &NewProjectDialog::validateInput);

    validateInput();
}

void NewProjectDialog::onBrowseDirectory() {
    QString dir = QFileDialog::getExistingDirectory(this, tr("保存先フォルダーの選択"), m_dirEdit->text());
    if (!dir.isEmpty()) {
        m_dirEdit->setText(dir);
    }
}

void NewProjectDialog::onPresetChanged(int index) {
    if (index >= 0 && index < static_cast<int>(sizeof(PAPER_PRESETS) / sizeof(PAPER_PRESETS[0]))) {
        if (PAPER_PRESETS[index].width > 0) {
            m_widthSpin->setValue(PAPER_PRESETS[index].width);
            m_heightSpin->setValue(PAPER_PRESETS[index].height);
            m_dpiSpin->setValue(PAPER_PRESETS[index].dpi);
        }
    }
}

void NewProjectDialog::validateInput() {
    bool valid = !m_nameEdit->text().trimmed().isEmpty() && !m_dirEdit->text().trimmed().isEmpty();
    m_createBtn->setEnabled(valid);
}

QString NewProjectDialog::projectName() const {
    return m_nameEdit ? m_nameEdit->text().trimmed() : "Untitled";
}

QString NewProjectDialog::projectDirectory() const {
    return m_dirEdit ? m_dirEdit->text().trimmed() : "";
}

int NewProjectDialog::pageCount() const {
    return m_pageCountSpin ? m_pageCountSpin->value() : 8;
}

int NewProjectDialog::canvasWidth() const {
    return m_widthSpin ? m_widthSpin->value() : 2480;
}

int NewProjectDialog::canvasHeight() const {
    return m_heightSpin ? m_heightSpin->value() : 3508;
}

double NewProjectDialog::canvasDpi() const {
    return m_dpiSpin ? static_cast<double>(m_dpiSpin->value()) : 350.0;
}

BindingDirection NewProjectDialog::bindingDirection() const {
    return (m_radioLeftToRight && m_radioLeftToRight->isChecked()) ? BindingDirection::LeftToRight : BindingDirection::RightToLeft;
}

bool NewProjectDialog::startFromRight() const {
    return m_radioStartRight ? m_radioStartRight->isChecked() : true;
}

} // namespace Kaburagi
