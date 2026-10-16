#include "NewPanelDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QDialogButtonBox>
#include <QColorDialog>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

namespace Kaburagi {

class PanelPreviewWidget : public QWidget {
public:
    explicit PanelPreviewWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(200, 260);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setCanvasSize(int w, int h) {
        m_canvasW = std::max(1, w);
        m_canvasH = std::max(1, h);
        update();
    }

    void setPanels(const QList<PanelSpec>& panels) {
        m_panels = panels;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // 背景
        p.fillRect(rect(), QColor(245, 245, 245));

        // キャンバスのアスペクト比に合わせて描画矩形を決定
        double availW = width() - 24;
        double availH = height() - 24;
        double aspect = static_cast<double>(m_canvasW) / m_canvasH;

        double drawW = availW;
        double drawH = drawW / aspect;
        if (drawH > availH) {
            drawH = availH;
            drawW = drawH * aspect;
        }

        QRectF pageRect((width() - drawW) * 0.5, (height() - drawH) * 0.5, drawW, drawH);
        p.fillRect(pageRect, Qt::white);
        p.setPen(QPen(QColor(190, 190, 190), 1));
        p.drawRect(pageRect);

        double scaleX = drawW / m_canvasW;
        double scaleY = drawH / m_canvasH;

        p.save();
        p.translate(pageRect.topLeft());
        p.scale(scaleX, scaleY);

        QFont font = p.font();
        font.setPixelSize(std::max(14, static_cast<int>(m_canvasH / 28)));
        font.setBold(true);
        p.setFont(font);

        int idx = 1;
        for (const auto& spec : m_panels) {
            p.setPen(QPen(QColor(40, 40, 40), 2.5 / std::min(scaleX, scaleY)));
            p.setBrush(QColor(220, 235, 252, 200));
            p.drawPolygon(spec.polygon);

            // コマ番号またはタイトル
            QRectF br = spec.polygon.boundingRect();
            p.setPen(QColor(45, 75, 130));
            QString labelText;
            if (spec.name.startsWith("タイトル")) {
                labelText = spec.name;
            } else if (spec.name.startsWith("コマ ")) {
                labelText = spec.name.mid(3);
            } else {
                labelText = QString::number(idx);
            }
            p.drawText(br, Qt::AlignCenter, labelText);
            idx++;
        }
        p.restore();
    }

private:
    int m_canvasW = 1600;
    int m_canvasH = 1200;
    QList<PanelSpec> m_panels;
};

NewPanelDialog::NewPanelDialog(int canvasWidth, int canvasHeight,
                               float defaultBorderWidth,
                               const QColor& defaultBorderColor,
                               float defaultMarginX,
                               float defaultMarginY,
                               QWidget* parent)
    : QDialog(parent),
      m_canvasWidth(std::max(10, canvasWidth)),
      m_canvasHeight(std::max(10, canvasHeight)),
      m_borderColor(defaultBorderColor) {
    setWindowTitle("新規コマフォルダ作成");
    setModal(true);
    resize(560, 460);

    setupUI();

    m_borderWidthSpin->setValue(defaultBorderWidth);
    m_marginXSpin->setValue(defaultMarginX);
    m_marginYSpin->setValue(defaultMarginY);

    onTemplateChanged(0);
    updatePreview();
}

void NewPanelDialog::setupUI() {
    auto rootLayout = new QVBoxLayout(this);
    rootLayout->setSpacing(12);

    auto contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(14);

    // 左ペイン: 設定コントロール
    auto leftLayout = new QVBoxLayout();
    leftLayout->setSpacing(10);

    // 1. 作成モードグループ
    auto modeGroup = new QGroupBox("レイアウト設定", this);
    auto modeLayout = new QVBoxLayout(modeGroup);
    modeLayout->setSpacing(6);

    m_templateRadio = new QRadioButton("テンプレートから選択", modeGroup);
    m_templateRadio->setObjectName("templateRadio");
    m_templateRadio->setChecked(true);
    modeLayout->addWidget(m_templateRadio);

    m_templateCombo = new QComboBox(modeGroup);
    m_templateCombo->setObjectName("templateCombo");
    m_templateCombo->addItem("四コマ漫画 (1列 × 4コマ)");
    m_templateCombo->addItem("四コマ漫画 (タイトル枠あり)");
    m_templateCombo->addItem("四コマ漫画 2本 (2列 × 4コマ)");
    m_templateCombo->addItem("四コマ漫画 2本 (タイトル枠あり)");
    m_templateCombo->addItem("3段均等 (1列 × 3コマ)");
    m_templateCombo->addItem("2段均等 (1列 × 2コマ)");
    m_templateCombo->addItem("6コマ均等 (2列 × 3コマ)");
    m_templateCombo->addItem("4コマグリッド (2列 × 2コマ)");
    m_templateCombo->addItem("定番5コマ (上1 / 中2 / 下2)");
    m_templateCombo->addItem("定番5コマ (上2 / 中2 / 下1)");
    m_templateCombo->addItem("定番4コマ変則 (上1 / 中2 / 下1)");
    m_templateCombo->addItem("見せ場3コマ (上大ゴマ / 下2コマ)");
    m_templateCombo->addItem("単一コマ枠 (分割なし)");
    modeLayout->addWidget(m_templateCombo);

    m_templateDescLabel = new QLabel(modeGroup);
    m_templateDescLabel->setStyleSheet("color: #666; font-size: 11px;");
    m_templateDescLabel->setWordWrap(true);
    modeLayout->addWidget(m_templateDescLabel);

    m_gridRadio = new QRadioButton("コマ数を指定 (グリッド分割)", modeGroup);
    m_gridRadio->setObjectName("gridRadio");
    modeLayout->addWidget(m_gridRadio);

    auto gridForm = new QFormLayout();
    gridForm->setContentsMargins(18, 0, 0, 0);

    m_colsSpin = new QSpinBox(modeGroup);
    m_colsSpin->setObjectName("colsSpin");
    m_colsSpin->setRange(1, 10);
    m_colsSpin->setValue(1);
    m_colsSpin->setEnabled(false);
    gridForm->addRow("横のコマ数 (列):", m_colsSpin);

    m_rowsSpin = new QSpinBox(modeGroup);
    m_rowsSpin->setObjectName("rowsSpin");
    m_rowsSpin->setRange(1, 20);
    m_rowsSpin->setValue(4);
    m_rowsSpin->setEnabled(false);
    gridForm->addRow("縦のコマ数 (行):", m_rowsSpin);

    modeLayout->addLayout(gridForm);
    leftLayout->addWidget(modeGroup);

    // 2. コマ間隔グループ
    auto marginGroup = new QGroupBox("コマの間隔", this);
    auto marginForm = new QFormLayout(marginGroup);

    m_marginXSpin = new QDoubleSpinBox(marginGroup);
    m_marginXSpin->setRange(0.0, 200.0);
    m_marginXSpin->setSingleStep(1.0);
    m_marginXSpin->setSuffix(" px");
    m_marginXSpin->setValue(20.0);
    marginForm->addRow("左右の間隔:", m_marginXSpin);

    m_marginYSpin = new QDoubleSpinBox(marginGroup);
    m_marginYSpin->setRange(0.0, 200.0);
    m_marginYSpin->setSingleStep(1.0);
    m_marginYSpin->setSuffix(" px");
    m_marginYSpin->setValue(40.0);
    marginForm->addRow("上下の間隔:", m_marginYSpin);

    leftLayout->addWidget(marginGroup);

    // 3. 枠線 & 余白グループ
    auto borderGroup = new QGroupBox("枠線・外枠余白", this);
    auto borderForm = new QFormLayout(borderGroup);

    m_drawBorderCheck = new QCheckBox("枠線を描画する", borderGroup);
    m_drawBorderCheck->setChecked(true);
    borderForm->addRow(m_drawBorderCheck);

    m_borderWidthSpin = new QDoubleSpinBox(borderGroup);
    m_borderWidthSpin->setRange(0.5, 50.0);
    m_borderWidthSpin->setSingleStep(0.5);
    m_borderWidthSpin->setSuffix(" px");
    m_borderWidthSpin->setValue(3.0);
    borderForm->addRow("線の太さ:", m_borderWidthSpin);

    m_borderColorBtn = new QPushButton(borderGroup);
    m_borderColorBtn->setFixedHeight(22);
    updateColorButtonStyle();
    borderForm->addRow("線の色:", m_borderColorBtn);

    int defPadX = std::max(20, m_canvasWidth / 16);
    int defPadY = std::max(20, m_canvasHeight / 16);

    m_padXSpin = new QSpinBox(borderGroup);
    m_padXSpin->setRange(0, m_canvasWidth / 2 - 10);
    m_padXSpin->setSuffix(" px");
    m_padXSpin->setValue(defPadX);
    borderForm->addRow("左右の余白:", m_padXSpin);

    m_padYSpin = new QSpinBox(borderGroup);
    m_padYSpin->setRange(0, m_canvasHeight / 2 - 10);
    m_padYSpin->setSuffix(" px");
    m_padYSpin->setValue(defPadY);
    borderForm->addRow("上下の余白:", m_padYSpin);

    leftLayout->addWidget(borderGroup);
    contentLayout->addLayout(leftLayout, 3);

    // 右ペイン: プレビュー
    auto rightLayout = new QVBoxLayout();
    auto previewGroup = new QGroupBox("プレビュー", this);
    auto previewInnerLayout = new QVBoxLayout(previewGroup);
    previewInnerLayout->setContentsMargins(6, 6, 6, 6);

    m_previewWidget = new PanelPreviewWidget(previewGroup);
    m_previewWidget->setCanvasSize(m_canvasWidth, m_canvasHeight);
    previewInnerLayout->addWidget(m_previewWidget);

    rightLayout->addWidget(previewGroup);
    contentLayout->addLayout(rightLayout, 2);

    rootLayout->addLayout(contentLayout);

    // ダイアログボタン
    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText("OK (作成)");
    buttonBox->button(QDialogButtonBox::Cancel)->setText("キャンセル");
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    rootLayout->addWidget(buttonBox);

    // シグナル接続
    connect(m_templateRadio, &QRadioButton::toggled, this, &NewPanelDialog::onModeToggled);
    connect(m_gridRadio, &QRadioButton::toggled, this, &NewPanelDialog::onModeToggled);
    connect(m_templateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NewPanelDialog::onTemplateChanged);
    connect(m_colsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &NewPanelDialog::onParamChanged);
    connect(m_rowsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &NewPanelDialog::onParamChanged);
    connect(m_marginXSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &NewPanelDialog::onParamChanged);
    connect(m_marginYSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &NewPanelDialog::onParamChanged);
    connect(m_padXSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &NewPanelDialog::onParamChanged);
    connect(m_padYSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &NewPanelDialog::onParamChanged);
    connect(m_borderColorBtn, &QPushButton::clicked, this, &NewPanelDialog::onChooseBorderColor);
}

void NewPanelDialog::onModeToggled() {
    bool isTpl = m_templateRadio->isChecked();
    m_templateCombo->setEnabled(isTpl);
    m_colsSpin->setEnabled(!isTpl);
    m_rowsSpin->setEnabled(!isTpl);
    updatePreview();
}

void NewPanelDialog::onTemplateChanged(int index) {
    switch (index) {
    case 0:
        m_templateDescLabel->setText("縦1列・4段の伝統的な四コマ漫画レイアウトです。");
        break;
    case 1:
        m_templateDescLabel->setText("上部にタイトル用の横長コマ枠、下部に4コマを配置した構成です。");
        break;
    case 2:
        m_templateDescLabel->setText("左右2列・各4段の計8コマ構成です。見開きや2話掲載向け。");
        break;
    case 3:
        m_templateDescLabel->setText("左右2列にそれぞれタイトル枠と4コマを配置した計10コマ構成です。");
        break;
    case 4:
        m_templateDescLabel->setText("縦1列・3段均等。ストーリー漫画で最も汎用性の高い段組みです。");
        break;
    case 5:
        m_templateDescLabel->setText("上下2段均等。ワイドな大ゴマを配置したいシーン向け。");
        break;
    case 6:
        m_templateDescLabel->setText("左右2列 × 上下3段の計6コマ。情報量の多い展開向け。");
        break;
    case 7:
        m_templateDescLabel->setText("2列 × 2段の計4コマ。イラスト風やアメコミスタイルの構成。");
        break;
    case 8:
        m_templateDescLabel->setText("上段1コマ、中段2コマ、下段2コマの王道ストーリー漫画構成。");
        break;
    case 9:
        m_templateDescLabel->setText("上段2コマ、中段2コマ、下段に大きなオチや見せ場を置く構成。");
        break;
    case 10:
        m_templateDescLabel->setText("上段1コマ、中段左右2コマ、下段1コマの安定した会話・展開用構成。");
        break;
    case 11:
        m_templateDescLabel->setText("上段に約6割の大きな見せゴマ、下段に会話・状況説明用2コマ。");
        break;
    case 12:
        m_templateDescLabel->setText("ページ全体を1つのコマ枠として作成します (分割なし)。");
        break;
    default:
        m_templateDescLabel->clear();
        break;
    }
    updatePreview();
}

void NewPanelDialog::onParamChanged() {
    updatePreview();
}

void NewPanelDialog::onChooseBorderColor() {
    QColor c = QColorDialog::getColor(m_borderColor, this, "コマ枠線の色を選択");
    if (c.isValid()) {
        m_borderColor = c;
        updateColorButtonStyle();
    }
}

void NewPanelDialog::updateColorButtonStyle() {
    if (!m_borderColorBtn) return;
    QString qss = QString("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(m_borderColor.name());
    m_borderColorBtn->setStyleSheet(qss);
}

void NewPanelDialog::updatePreview() {
    if (m_previewWidget) {
        m_previewWidget->setPanels(calculatePanels());
    }
}

QList<PanelSpec> NewPanelDialog::calculatePanels() const {
    QList<PanelSpec> result;

    int padX = m_padXSpin ? m_padXSpin->value() : 40;
    int padY = m_padYSpin ? m_padYSpin->value() : 40;
    float mx = m_marginXSpin ? static_cast<float>(m_marginXSpin->value()) : 20.0f;
    float my = m_marginYSpin ? static_cast<float>(m_marginYSpin->value()) : 40.0f;

    double left = padX;
    double top = padY;
    double w = std::max(20.0, static_cast<double>(m_canvasWidth - padX * 2));
    double h = std::max(20.0, static_cast<double>(m_canvasHeight - padY * 2));
    QRectF outer(left, top, w, h);

    auto makePoly = [](const QRectF& r) {
        QPolygonF p;
        p << r.topLeft() << r.topRight() << r.bottomRight() << r.bottomLeft();
        return p;
    };

    if (m_gridRadio && m_gridRadio->isChecked()) {
        int cols = std::max(1, m_colsSpin ? m_colsSpin->value() : 1);
        int rows = std::max(1, m_rowsSpin ? m_rowsSpin->value() : 1);

        double totalW = w - (cols - 1) * mx;
        double totalH = h - (rows - 1) * my;
        double cellW = std::max(5.0, totalW / cols);
        double cellH = std::max(5.0, totalH / rows);

        int panelNum = 1;
        // 行: 上から下、列: 右から左（日本の漫画順）
        for (int r = 0; r < rows; ++r) {
            for (int c = cols - 1; c >= 0; --c) {
                double x0 = left + c * (cellW + mx);
                double y0 = top + r * (cellH + my);
                QRectF cellRect(x0, y0, cellW, cellH);
                result.append({makePoly(cellRect), QString("コマ %1").arg(panelNum++)});
            }
        }
        return result;
    }

    // テンプレートモード
    int tpl = m_templateCombo ? m_templateCombo->currentIndex() : 0;
    switch (tpl) {
    case 0: { // 四コマ漫画 (1列 × 4コマ)
        double cellH = std::max(5.0, (h - 3 * my) / 4.0);
        for (int r = 0; r < 4; ++r) {
            double y0 = top + r * (cellH + my);
            QRectF cr(left, y0, w, cellH);
            result.append({makePoly(cr), QString("コマ %1").arg(r + 1)});
        }
        break;
    }
    case 1: { // 四コマ漫画 (タイトル枠あり)
        double availH = h - 4 * my;
        double cellH = std::max(5.0, availH / 4.6);
        double titleH = std::max(5.0, availH - 4.0 * cellH);
        result.append({makePoly(QRectF(left, top, w, titleH)), "タイトル"});
        for (int r = 0; r < 4; ++r) {
            double y0 = top + titleH + my + r * (cellH + my);
            QRectF cr(left, y0, w, cellH);
            result.append({makePoly(cr), QString("コマ %1").arg(r + 1)});
        }
        break;
    }
    case 2: { // 四コマ漫画 2本 (2列 × 4コマ)
        double cellW = std::max(5.0, (w - mx) * 0.5);
        double cellH = std::max(5.0, (h - 3 * my) / 4.0);
        int num = 1;
        // 右列 (第1話 1〜4)
        for (int r = 0; r < 4; ++r) {
            QRectF cr(left + cellW + mx, top + r * (cellH + my), cellW, cellH);
            result.append({makePoly(cr), QString("コマ %1").arg(num++)});
        }
        // 左列 (第2話 5〜8)
        for (int r = 0; r < 4; ++r) {
            QRectF cr(left, top + r * (cellH + my), cellW, cellH);
            result.append({makePoly(cr), QString("コマ %1").arg(num++)});
        }
        break;
    }
    case 3: { // 四コマ漫画 2本 (タイトル枠あり)
        double cellW = std::max(5.0, (w - mx) * 0.5);
        double availH = h - 4 * my;
        double cellH = std::max(5.0, availH / 4.6);
        double titleH = std::max(5.0, availH - 4.0 * cellH);
        // 右列 (第1話: タイトル 1, コマ 1〜4)
        result.append({makePoly(QRectF(left + cellW + mx, top, cellW, titleH)), "タイトル 1"});
        for (int r = 0; r < 4; ++r) {
            double y0 = top + titleH + my + r * (cellH + my);
            QRectF cr(left + cellW + mx, y0, cellW, cellH);
            result.append({makePoly(cr), QString("コマ %1").arg(r + 1)});
        }
        // 左列 (第2話: タイトル 2, コマ 5〜8)
        result.append({makePoly(QRectF(left, top, cellW, titleH)), "タイトル 2"});
        for (int r = 0; r < 4; ++r) {
            double y0 = top + titleH + my + r * (cellH + my);
            QRectF cr(left, y0, cellW, cellH);
            result.append({makePoly(cr), QString("コマ %1").arg(r + 5)});
        }
        break;
    }
    case 4: { // 3段均等 (1列 × 3コマ)
        double cellH = std::max(5.0, (h - 2 * my) / 3.0);
        for (int r = 0; r < 3; ++r) {
            QRectF cr(left, top + r * (cellH + my), w, cellH);
            result.append({makePoly(cr), QString("コマ %1").arg(r + 1)});
        }
        break;
    }
    case 5: { // 2段均等 (1列 × 2コマ)
        double cellH = std::max(5.0, (h - my) * 0.5);
        for (int r = 0; r < 2; ++r) {
            QRectF cr(left, top + r * (cellH + my), w, cellH);
            result.append({makePoly(cr), QString("コマ %1").arg(r + 1)});
        }
        break;
    }
    case 6: { // 6コマ均等 (2列 × 3コマ)
        double cellW = std::max(5.0, (w - mx) * 0.5);
        double cellH = std::max(5.0, (h - 2 * my) / 3.0);
        int num = 1;
        for (int r = 0; r < 3; ++r) {
            // 右
            QRectF crRight(left + cellW + mx, top + r * (cellH + my), cellW, cellH);
            result.append({makePoly(crRight), QString("コマ %1").arg(num++)});
            // 左
            QRectF crLeft(left, top + r * (cellH + my), cellW, cellH);
            result.append({makePoly(crLeft), QString("コマ %1").arg(num++)});
        }
        break;
    }
    case 7: { // 4コマグリッド (2列 × 2コマ)
        double cellW = std::max(5.0, (w - mx) * 0.5);
        double cellH = std::max(5.0, (h - my) * 0.5);
        int num = 1;
        for (int r = 0; r < 2; ++r) {
            QRectF crRight(left + cellW + mx, top + r * (cellH + my), cellW, cellH);
            result.append({makePoly(crRight), QString("コマ %1").arg(num++)});
            QRectF crLeft(left, top + r * (cellH + my), cellW, cellH);
            result.append({makePoly(crLeft), QString("コマ %1").arg(num++)});
        }
        break;
    }
    case 8: { // 定番5コマ (上1 / 中2 / 下2)
        double cellH = std::max(5.0, (h - 2 * my) / 3.0);
        double halfW = std::max(5.0, (w - mx) * 0.5);
        // 上1
        result.append({makePoly(QRectF(left, top, w, cellH)), "コマ 1"});
        // 中2 (右, 左)
        result.append({makePoly(QRectF(left + halfW + mx, top + cellH + my, halfW, cellH)), "コマ 2"});
        result.append({makePoly(QRectF(left, top + cellH + my, halfW, cellH)), "コマ 3"});
        // 下2 (右, 左)
        result.append({makePoly(QRectF(left + halfW + mx, top + (cellH + my) * 2, halfW, cellH)), "コマ 4"});
        result.append({makePoly(QRectF(left, top + (cellH + my) * 2, halfW, cellH)), "コマ 5"});
        break;
    }
    case 9: { // 定番5コマ (上2 / 中2 / 下1)
        double cellH = std::max(5.0, (h - 2 * my) / 3.0);
        double halfW = std::max(5.0, (w - mx) * 0.5);
        // 上2
        result.append({makePoly(QRectF(left + halfW + mx, top, halfW, cellH)), "コマ 1"});
        result.append({makePoly(QRectF(left, top, halfW, cellH)), "コマ 2"});
        // 中2
        result.append({makePoly(QRectF(left + halfW + mx, top + cellH + my, halfW, cellH)), "コマ 3"});
        result.append({makePoly(QRectF(left, top + cellH + my, halfW, cellH)), "コマ 4"});
        // 下1
        result.append({makePoly(QRectF(left, top + (cellH + my) * 2, w, cellH)), "コマ 5"});
        break;
    }
    case 10: { // 定番4コマ変則 (上1 / 中2 / 下1)
        double cellH = std::max(5.0, (h - 2 * my) / 3.0);
        double halfW = std::max(5.0, (w - mx) * 0.5);
        // 上1
        result.append({makePoly(QRectF(left, top, w, cellH)), "コマ 1"});
        // 中2
        result.append({makePoly(QRectF(left + halfW + mx, top + cellH + my, halfW, cellH)), "コマ 2"});
        result.append({makePoly(QRectF(left, top + cellH + my, halfW, cellH)), "コマ 3"});
        // 下1
        result.append({makePoly(QRectF(left, top + (cellH + my) * 2, w, cellH)), "コマ 4"});
        break;
    }
    case 11: { // 見せ場3コマ (上大ゴマ / 下2コマ)
        double topH = std::max(10.0, (h - my) * 0.58);
        double bottomH = std::max(5.0, h - my - topH);
        double halfW = std::max(5.0, (w - mx) * 0.5);
        // 上大ゴマ
        result.append({makePoly(QRectF(left, top, w, topH)), "コマ 1"});
        // 下2コマ (右, 左)
        result.append({makePoly(QRectF(left + halfW + mx, top + topH + my, halfW, bottomH)), "コマ 2"});
        result.append({makePoly(QRectF(left, top + topH + my, halfW, bottomH)), "コマ 3"});
        break;
    }
    case 12: // 単一コマ枠 (分割なし)
    default: {
        result.append({makePoly(outer), "コマ 1"});
        break;
    }
    }

    return result;
}

QList<PanelSpec> NewPanelDialog::panels() const {
    return calculatePanels();
}

bool NewPanelDialog::drawBorder() const {
    return m_drawBorderCheck ? m_drawBorderCheck->isChecked() : true;
}

float NewPanelDialog::borderWidth() const {
    return m_borderWidthSpin ? static_cast<float>(m_borderWidthSpin->value()) : 3.0f;
}

QColor NewPanelDialog::borderColor() const {
    return m_borderColor;
}

float NewPanelDialog::marginX() const {
    return m_marginXSpin ? static_cast<float>(m_marginXSpin->value()) : 20.0f;
}

float NewPanelDialog::marginY() const {
    return m_marginYSpin ? static_cast<float>(m_marginYSpin->value()) : 40.0f;
}

} // namespace Kaburagi
