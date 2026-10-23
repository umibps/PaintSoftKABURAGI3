#include "BatchExportDialog.hpp"
#include "Document.hpp"
#include "DocumentSerializer.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QPdfWriter>
#include <QPageSize>
#include <QApplication>

namespace Kaburagi {

BatchExportDialog::BatchExportDialog(std::shared_ptr<PageProject> project, QWidget* parent)
    : QDialog(parent), m_project(project) {
    setWindowTitle(tr("ページ一括書き出し"));
    setModal(true);
    resize(480, 480);

    setupUI();
}

void BatchExportDialog::setupUI() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // 1. 対象ページ
    auto pageGroup = new QGroupBox(tr("書き出し対象"), this);
    auto pageLayout = new QVBoxLayout(pageGroup);

    m_radioAllPages = new QRadioButton(tr("全ページ (%1 ページ)").arg(m_project ? m_project->pageCount() : 0), pageGroup);
    m_radioAllPages->setChecked(true);
    pageLayout->addWidget(m_radioAllPages);

    auto rangeRow = new QHBoxLayout();
    m_radioRangePages = new QRadioButton(tr("ページ範囲指定:"), pageGroup);
    m_rangeEdit = new QLineEdit("1-" + QString::number(m_project ? m_project->pageCount() : 1), pageGroup);
    m_rangeEdit->setEnabled(false);
    m_rangeEdit->setPlaceholderText(tr("例: 1-4, 7, 9-12"));
    rangeRow->addWidget(m_radioRangePages);
    rangeRow->addWidget(m_rangeEdit);
    pageLayout->addLayout(rangeRow);

    connect(m_radioRangePages, &QRadioButton::toggled, m_rangeEdit, &QLineEdit::setEnabled);
    mainLayout->addWidget(pageGroup);

    // 2. 出力形式 & 出力先
    auto outGroup = new QGroupBox(tr("出力先・ファイル形式"), this);
    auto outLayout = new QFormLayout(outGroup);

    m_formatCombo = new QComboBox(outGroup);
    m_formatCombo->addItem("PNG (*.png)", "png");
    m_formatCombo->addItem("JPEG (*.jpg)", "jpg");
    m_formatCombo->addItem("PDF 文書 (*.pdf)", "pdf");
    m_formatCombo->addItem("Photoshop (*.psd)", "psd");
    m_formatCombo->addItem("TIFF (*.tif)", "tif");
    m_formatCombo->addItem("BMP (*.bmp)", "bmp");
    outLayout->addRow(tr("出力フォーマット:"), m_formatCombo);

    auto dirRow = new QHBoxLayout();
    QString defaultExportDir = m_project ? QDir(m_project->projectDir()).filePath("export") : "";
    m_outDirEdit = new QLineEdit(defaultExportDir, outGroup);
    m_browseDirBtn = new QPushButton(tr("参照..."), outGroup);
    dirRow->addWidget(m_outDirEdit);
    dirRow->addWidget(m_browseDirBtn);
    outLayout->addRow(tr("出力フォルダー:"), dirRow);

    m_namePatternEdit = new QLineEdit("page_%04d", outGroup);
    outLayout->addRow(tr("ファイル名パターン:"), m_namePatternEdit);

    m_scaleCombo = new QComboBox(outGroup);
    m_scaleCombo->addItem(tr("100% (等倍原寸)"), 1.0);
    m_scaleCombo->addItem(tr("50% (Web・閲覧用)"), 0.5);
    m_scaleCombo->addItem(tr("25% (プレビュー用)"), 0.25);
    m_scaleCombo->addItem(tr("200% (拡大)"), 2.0);
    outLayout->addRow(tr("出力解像度倍率:"), m_scaleCombo);

    // JPEG品質設定 (非PDF / JPEG選択時用)
    m_jpegQualityWidget = new QWidget(outGroup);
    auto jpegLay = new QHBoxLayout(m_jpegQualityWidget);
    jpegLay->setContentsMargins(0, 0, 0, 0);
    m_jpegQualitySlider = new QSlider(Qt::Horizontal, m_jpegQualityWidget);
    m_jpegQualitySlider->setRange(1, 100);
    m_jpegQualitySlider->setValue(90);
    m_jpegQualitySpin = new QSpinBox(m_jpegQualityWidget);
    m_jpegQualitySpin->setRange(1, 100);
    m_jpegQualitySpin->setValue(90);
    jpegLay->addWidget(m_jpegQualitySlider);
    jpegLay->addWidget(m_jpegQualitySpin);
    connect(m_jpegQualitySlider, &QSlider::valueChanged, m_jpegQualitySpin, &QSpinBox::setValue);
    connect(m_jpegQualitySpin, QOverload<int>::of(&QSpinBox::valueChanged), m_jpegQualitySlider, &QSlider::setValue);
    outLayout->addRow(tr("JPEG品質:"), m_jpegQualityWidget);
    m_jpegQualityWidget->setVisible(false);

    mainLayout->addWidget(outGroup);

    // 3. 進捗表示
    m_statusLabel = new QLabel(tr("準備完了"), this);
    mainLayout->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    mainLayout->addWidget(m_progressBar);

    // 4. ボタン
    auto btnRow = new QHBoxLayout();
    btnRow->addStretch(1);

    m_closeBtn = new QPushButton(tr("閉じる"), this);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(m_closeBtn);

    m_startBtn = new QPushButton(tr("書き出し開始"), this);
    m_startBtn->setDefault(true);
    m_startBtn->setStyleSheet("QPushButton { font-weight: bold; padding: 6px 16px; }");
    connect(m_startBtn, &QPushButton::clicked, this, &BatchExportDialog::onStartExport);
    btnRow->addWidget(m_startBtn);

    mainLayout->addLayout(btnRow);

    // イベント接続
    connect(m_browseDirBtn, &QPushButton::clicked, this, &BatchExportDialog::onBrowseOutDir);
    connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BatchExportDialog::onFormatChanged);
}

void BatchExportDialog::onBrowseOutDir() {
    QString dir = QFileDialog::getExistingDirectory(this, tr("出力先フォルダーを選択"), m_outDirEdit->text());
    if (!dir.isEmpty()) {
        m_outDirEdit->setText(dir);
    }
}

void BatchExportDialog::onFormatChanged(int index) {
    QString fmt = m_formatCombo->itemData(index).toString();
    m_jpegQualityWidget->setVisible(fmt == "jpg");
    if (fmt == "pdf") {
        m_namePatternEdit->setText(m_project ? m_project->name() : "output");
    } else {
        m_namePatternEdit->setText("page_%04d");
    }
}

std::vector<int> BatchExportDialog::parseSelectedPages() const {
    std::vector<int> pages;
    if (!m_project) return pages;

    int totalPages = m_project->pageCount();
    if (m_radioAllPages->isChecked()) {
        for (int i = 0; i < totalPages; ++i) pages.push_back(i);
        return pages;
    }

    // 範囲指定パース ("1-3, 5, 8-10")
    QString text = m_rangeEdit->text().trimmed();
    QStringList tokens = text.split(',', Qt::SkipEmptyParts);

    for (const QString& tok : tokens) {
        QString s = tok.trimmed();
        if (s.contains('-')) {
            QStringList bounds = s.split('-');
            if (bounds.size() == 2) {
                int start = bounds[0].trimmed().toInt();
                int end = bounds[1].trimmed().toInt();
                if (start > end) std::swap(start, end);
                for (int p = start; p <= end; ++p) {
                    int idx = p - 1;
                    if (idx >= 0 && idx < totalPages) {
                        if (std::find(pages.begin(), pages.end(), idx) == pages.end()) {
                            pages.push_back(idx);
                        }
                    }
                }
            }
        } else {
            int p = s.toInt();
            int idx = p - 1;
            if (idx >= 0 && idx < totalPages) {
                if (std::find(pages.begin(), pages.end(), idx) == pages.end()) {
                    pages.push_back(idx);
                }
            }
        }
    }

    std::sort(pages.begin(), pages.end());
    return pages;
}

void BatchExportDialog::onStartExport() {
    if (!m_project) return;

    std::vector<int> targetPages = parseSelectedPages();
    if (targetPages.empty()) {
        QMessageBox::warning(this, tr("警告"), tr("書き出し対象のページがありません。"));
        return;
    }

    QString outDir = m_outDirEdit->text().trimmed();
    if (outDir.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("出力先フォルダーを指定してください。"));
        return;
    }

    QDir dir(outDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        QMessageBox::critical(this, tr("エラー"), tr("出力先フォルダーを作成できませんでした:\n%1").arg(outDir));
        return;
    }

    QString fmt = m_formatCombo->currentData().toString();
    double scale = m_scaleCombo->currentData().toDouble();
    int jpegQuality = m_jpegQualitySpin->value();
    QString pattern = m_namePatternEdit->text().trimmed();

    m_startBtn->setEnabled(false);
    m_progressBar->setMaximum(static_cast<int>(targetPages.size()));
    m_progressBar->setValue(0);

    // --- PDF 一括書き出し ---
    if (fmt == "pdf") {
        QString pdfFile = pattern.endsWith(".pdf", Qt::CaseInsensitive) ? pattern : (pattern + ".pdf");
        QString pdfFullPath = dir.filePath(pdfFile);

        QPdfWriter pdfWriter(pdfFullPath);
        pdfWriter.setResolution(static_cast<int>(m_project->standardDpi()));
        QPainter painter;
        bool painterStarted = false;

        for (size_t i = 0; i < targetPages.size(); ++i) {
            int pageIdx = targetPages[i];
            m_statusLabel->setText(tr("PDFページ出力中: %1 / %2 ページ...").arg(i + 1).arg(targetPages.size()));
            QApplication::processEvents();

            QString kabPath = m_project->pageFullPath(pageIdx);
            auto doc = std::unique_ptr<Document>(DocumentSerializer::loadFromFile(kabPath));
            if (!doc) continue;

            QImage pageImg = doc->renderToImage();
            if (scale != 1.0) {
                pageImg = pageImg.scaled(pageImg.width() * scale, pageImg.height() * scale, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }

            // PDF ページサイズ設定 (ピクセルからミリメートルへ換算)
            double dpi = doc->dpi() > 0 ? doc->dpi() : 350.0;
            double mmW = (pageImg.width() / dpi) * 25.4;
            double mmH = (pageImg.height() / dpi) * 25.4;

            QPageSize pageSize(QSizeF(mmW, mmH), QPageSize::Millimeter);
            pdfWriter.setPageSize(pageSize);

            if (!painterStarted) {
                if (!painter.begin(&pdfWriter)) {
                    QMessageBox::critical(this, tr("エラー"), tr("PDFファイルの作成に失敗しました:\n%1").arg(pdfFullPath));
                    m_startBtn->setEnabled(true);
                    return;
                }
                painterStarted = true;
            } else {
                pdfWriter.newPage();
            }

            painter.drawImage(QRectF(0, 0, pdfWriter.width(), pdfWriter.height()), pageImg);
            m_progressBar->setValue(static_cast<int>(i + 1));
        }

        if (painterStarted) {
            painter.end();
        }

        m_statusLabel->setText(tr("PDF書き出しが完了しました！"));
        QMessageBox::information(this, tr("完了"), tr("PDF書き出しが完了しました:\n%1").arg(pdfFullPath));
        m_startBtn->setEnabled(true);
        return;
    }

    // --- 連番画像書き出し (PNG, JPEG, PSD, etc.) ---
    for (size_t i = 0; i < targetPages.size(); ++i) {
        int pageIdx = targetPages[i];
        m_statusLabel->setText(tr("画像出力中: %1 / %2 ページ...").arg(i + 1).arg(targetPages.size()));
        QApplication::processEvents();

        QString kabPath = m_project->pageFullPath(pageIdx);
        auto doc = std::unique_ptr<Document>(DocumentSerializer::loadFromFile(kabPath));
        if (!doc) continue;

        QImage pageImg = doc->renderToImage();
        if (scale != 1.0) {
            pageImg = pageImg.scaled(pageImg.width() * scale, pageImg.height() * scale, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }

        // ファイル名生成 (例: page_0001.png)
        QString baseName;
        if (pattern.contains("%")) {
            char buf[256];
            snprintf(buf, sizeof(buf), pattern.toUtf8().constData(), pageIdx + 1);
            baseName = QString::fromUtf8(buf);
        } else {
            baseName = QString("%1_%2").arg(pattern).arg(pageIdx + 1, 4, 10, QChar('0'));
        }

        QString outFilePath = dir.filePath(QString("%1.%2").arg(baseName, fmt));

        if (fmt == "jpg" || fmt == "jpeg") {
            pageImg.save(outFilePath, "JPG", jpegQuality);
        } else if (fmt == "png") {
            pageImg.save(outFilePath, "PNG");
        } else {
            // DocumentSerializer::exportImage (PSD, TIFF, BMP等に対応)
            DocumentSerializer::exportImage(doc.get(), outFilePath);
        }

        m_progressBar->setValue(static_cast<int>(i + 1));
    }

    m_statusLabel->setText(tr("全画像の書き出しが完了しました！"));
    QMessageBox::information(this, tr("完了"), tr("書き出しが完了しました:\n%1").arg(outDir));
    m_startBtn->setEnabled(true);
}

} // namespace Kaburagi
