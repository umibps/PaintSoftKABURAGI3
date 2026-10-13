#include "TextBalloonDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QColorDialog>
#include <QLabel>
#include <QGroupBox>

namespace Kaburagi {

TextBalloonDialog::TextBalloonDialog(const TextBalloonItem& item, QWidget* parent)
    : QDialog(parent), m_item(item) {
    setWindowTitle("文字・吹き出しの編集");
    resize(380, 420);
    setStyleSheet("QDialog { background: #ece9d8; font-size: 11px; } QLabel, QCheckBox, QGroupBox { font-size: 11px; }");

    m_frameColor = item.frameColor;
    m_fillColor = item.fillColor;

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // テキスト入力
    auto textGroup = new QGroupBox("テキスト内容", this);
    auto textLayout = new QVBoxLayout(textGroup);
    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setPlainText(item.text);
    m_textEdit->setPlaceholderText("吹き出しに表示するテキストを入力...");
    m_textEdit->setFixedHeight(80);
    textLayout->addWidget(m_textEdit);
    mainLayout->addWidget(textGroup);

    // フォント設定
    auto fontGroup = new QGroupBox("フォント設定", this);
    auto fontLayout = new QFormLayout(fontGroup);

    m_fontCombo = new QFontComboBox(this);
    m_fontCombo->setCurrentFont(item.font);
    fontLayout->addRow("フォント:", m_fontCombo);

    auto sizeLayout = new QHBoxLayout();
    m_fontSizeSpin = new QSpinBox(this);
    m_fontSizeSpin->setRange(6, 120);
    m_fontSizeSpin->setValue(item.font.pointSize() > 0 ? item.font.pointSize() : 16);
    sizeLayout->addWidget(m_fontSizeSpin);

    m_boldCheck = new QCheckBox("太字 (Bold)", this);
    m_boldCheck->setChecked(item.font.bold());
    sizeLayout->addWidget(m_boldCheck);
    fontLayout->addRow("サイズ / スタイル:", sizeLayout);

    mainLayout->addWidget(fontGroup);

    // 吹き出し形状・デザイン
    auto balloonGroup = new QGroupBox("吹き出し形状・外観", this);
    auto balloonLayout = new QFormLayout(balloonGroup);

    m_shapeCombo = new QComboBox(this);
    m_shapeCombo->addItem("楕円吹き出し", static_cast<int>(BalloonShape::Ellipse));
    m_shapeCombo->addItem("角丸四角形吹き出し", static_cast<int>(BalloonShape::RoundedRect));
    m_shapeCombo->addItem("四角形 (モノローグ)", static_cast<int>(BalloonShape::Rectangle));
    m_shapeCombo->addItem("枠なしテキストのみ", static_cast<int>(BalloonShape::None));
    int shapeIndex = m_shapeCombo->findData(static_cast<int>(item.shape));
    if (shapeIndex >= 0) m_shapeCombo->setCurrentIndex(shapeIndex);
    balloonLayout->addRow("形状:", m_shapeCombo);

    auto lineLayout = new QHBoxLayout();
    m_frameWidthSpin = new QSpinBox(this);
    m_frameWidthSpin->setRange(0, 30);
    m_frameWidthSpin->setValue(static_cast<int>(item.frameWidth));
    lineLayout->addWidget(m_frameWidthSpin);

    m_frameColorBtn = new QPushButton("線色", this);
    m_frameColorBtn->setFixedWidth(60);
    connect(m_frameColorBtn, &QPushButton::clicked, this, &TextBalloonDialog::onFrameColorClicked);
    lineLayout->addWidget(m_frameColorBtn);
    balloonLayout->addRow("線の太さ / 色:", lineLayout);

    auto fillLayout = new QHBoxLayout();
    m_fillColorBtn = new QPushButton("背景色", this);
    m_fillColorBtn->setFixedWidth(60);
    connect(m_fillColorBtn, &QPushButton::clicked, this, &TextBalloonDialog::onFillColorClicked);
    fillLayout->addWidget(m_fillColorBtn);

    m_transparentFillCheck = new QCheckBox("背景透明", this);
    m_transparentFillCheck->setChecked(item.fillColor.alpha() == 0);
    fillLayout->addWidget(m_transparentFillCheck);
    balloonLayout->addRow("背景色:", fillLayout);

    m_hasTailCheck = new QCheckBox("しっぽ（吹き出し突起）を付ける", this);
    m_hasTailCheck->setChecked(item.hasTail);
    balloonLayout->addRow("", m_hasTailCheck);

    mainLayout->addWidget(balloonGroup);

    // ボタン
    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    updateColorButtons();
}

void TextBalloonDialog::onFrameColorClicked() {
    QColor col = QColorDialog::getColor(m_frameColor, this, "枠線の色を選択");
    if (col.isValid()) {
        m_frameColor = col;
        updateColorButtons();
    }
}

void TextBalloonDialog::onFillColorClicked() {
    QColor col = QColorDialog::getColor(m_fillColor.alpha() > 0 ? m_fillColor : Qt::white, this, "背景色を選択");
    if (col.isValid()) {
        m_fillColor = col;
        m_transparentFillCheck->setChecked(false);
        updateColorButtons();
    }
}

void TextBalloonDialog::updateColorButtons() {
    m_frameColorBtn->setStyleSheet(QString("background: %1; color: %2; border: 1px solid #777; font-size: 11px;")
        .arg(m_frameColor.name(), m_frameColor.lightness() > 128 ? "black" : "white"));

    QColor fill = m_transparentFillCheck->isChecked() ? Qt::transparent : m_fillColor;
    m_fillColorBtn->setStyleSheet(QString("background: %1; color: %2; border: 1px solid #777; font-size: 11px;")
        .arg(fill.alpha() > 0 ? fill.name() : "#cccccc", fill.lightness() > 128 ? "black" : "white"));
}

TextBalloonItem TextBalloonDialog::getItem() const {
    TextBalloonItem res = m_item;
    res.text = m_textEdit->toPlainText();

    QFont f = m_fontCombo->currentFont();
    f.setPointSize(m_fontSizeSpin->value());
    f.setBold(m_boldCheck->isChecked());
    res.font = f;

    res.shape = static_cast<BalloonShape>(m_shapeCombo->currentData().toInt());
    res.frameWidth = static_cast<float>(m_frameWidthSpin->value());
    res.frameColor = m_frameColor;
    res.fillColor = m_transparentFillCheck->isChecked() ? Qt::transparent : m_fillColor;
    res.hasTail = m_hasTailCheck->isChecked();

    return res;
}

} // namespace Kaburagi
