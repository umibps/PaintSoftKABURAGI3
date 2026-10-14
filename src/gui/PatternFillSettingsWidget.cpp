#include "PatternFillSettingsWidget.hpp"
#include <QGroupBox>
#include <QDir>
#include <QDirIterator>
#include <QCoreApplication>
#include <QClipboard>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QPainter>

namespace Kaburagi {

PatternFillSettingsWidget::PatternFillSettingsWidget(QWidget* parent)
    : QWidget(parent) {
    setupUI();
    reloadPatterns();
}

void PatternFillSettingsWidget::setupUI() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);
    setMinimumWidth(0);

    // 1. 合成モード
    auto modeLayout = new QHBoxLayout();
    auto modeLabel = new QLabel("合成モード:", this);
    modeLabel->setStyleSheet("font-size: 11px;");
    m_blendCombo = new QComboBox(this);
    m_blendCombo->setStyleSheet("QComboBox { font-size: 11px; }");
    m_blendCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_blendCombo->setMinimumContentsLength(4);
    m_blendCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    for (const auto& info : getAllBlendModes()) {
        if (info.mode == BlendMode::PassThrough) continue;
        m_blendCombo->addItem(QString("%1 (%2)").arg(info.nameJa, info.nameEn), static_cast<int>(info.mode));
    }
    modeLayout->addWidget(modeLabel);
    modeLayout->addWidget(m_blendCombo, 1);
    mainLayout->addLayout(modeLayout);

    connect(m_blendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx >= 0) {
            emit blendModeChanged(blendMode());
        }
    });

    // 2. 濃度 (1〜100%)
    m_densityScale = new SpinScale("濃度", 1.0, 100.0, 1.0, 1, "%", this);
    m_densityScale->setValue(100.0);
    mainLayout->addWidget(m_densityScale);

    connect(m_densityScale, &SpinScale::valueChanged, this, [this](double val) {
        m_density = static_cast<float>(val / 100.0);
        emit densityChanged(m_density);
    });

    // 3. パターン拡大率 (10〜500%)
    m_scaleScale = new SpinScale("拡大率", 10.0, 500.0, 1.0, 0, "%", this);
    m_scaleScale->setValue(100.0);
    mainLayout->addWidget(m_scaleScale);

    connect(m_scaleScale, &SpinScale::valueChanged, this, [this](double val) {
        m_patternScale = static_cast<float>(val / 100.0);
        emit patternScaleChanged(m_patternScale);
    });

    // 4. パターン角度 (0〜360°)
    m_angleScale = new SpinScale("回転角度", 0.0, 360.0, 1.0, 0, "°", this);
    m_angleScale->setValue(0.0);
    mainLayout->addWidget(m_angleScale);

    connect(m_angleScale, &SpinScale::valueChanged, this, [this](double val) {
        m_patternAngle = static_cast<float>(val);
        emit patternAngleChanged(m_patternAngle);
    });

    auto line0 = new QFrame(this);
    line0->setFrameShape(QFrame::HLine);
    line0->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line0);

    // 5. パターン画像選択グループ
    auto patGroup = new QGroupBox("パターン画像", this);
    auto patLayout = new QVBoxLayout(patGroup);
    patLayout->setContentsMargins(4, 6, 4, 4);
    patLayout->setSpacing(4);

    auto btnLayout = new QHBoxLayout();
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(4);

    m_btnReload = new QPushButton("🔄 更新", this);
    m_btnReload->setToolTip("pattern フォルダからパターン画像を再読み込みします");
    btnLayout->addWidget(m_btnReload);

    m_btnClipboard = new QPushButton("📋 貼付", this);
    m_btnClipboard->setToolTip("クリップボードの画像をパターンとして取り込みます");
    btnLayout->addWidget(m_btnClipboard);

    m_btnFile = new QPushButton("📁 参照", this);
    m_btnFile->setToolTip("画像ファイルをパターンとして取り込みます");
    btnLayout->addWidget(m_btnFile);

    patLayout->addLayout(btnLayout);

    connect(m_btnReload, &QPushButton::clicked, this, &PatternFillSettingsWidget::reloadPatterns);
    connect(m_btnClipboard, &QPushButton::clicked, this, &PatternFillSettingsWidget::importFromClipboard);
    connect(m_btnFile, &QPushButton::clicked, this, &PatternFillSettingsWidget::importFromFile);

    m_patternListWidget = new QListWidget(this);
    m_patternListWidget->setViewMode(QListView::IconMode);
    m_patternListWidget->setIconSize(QSize(40, 40));
    m_patternListWidget->setGridSize(QSize(48, 48));
    m_patternListWidget->setResizeMode(QListWidget::Adjust);
    m_patternListWidget->setMovement(QListView::Static);
    m_patternListWidget->setSpacing(2);
    m_patternListWidget->setFixedHeight(120);
    m_patternListWidget->setStyleSheet(
        "QListWidget { background: #ffffff; border: 1px solid #7f9db9; }"
        "QListWidget::item { border: 1px solid transparent; border-radius: 3px; }"
        "QListWidget::item:selected { background: #cce8ff; border: 1px solid #3399ff; }"
        "QListWidget::item:hover { background: #e5f3ff; }"
    );
    patLayout->addWidget(m_patternListWidget);

    connect(m_patternListWidget, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* current, QListWidgetItem*) {
        if (!current) return;
        QString name = current->data(Qt::UserRole).toString();
        if (name != m_selectedPatternName && m_patternImages.contains(name)) {
            m_selectedPatternName = name;
            emit patternImageChanged(m_patternImages[name], name);
        }
    });

    mainLayout->addWidget(patGroup);

    auto line1 = new QFrame(this);
    line1->setFrameShape(QFrame::HLine);
    line1->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line1);

    // 6. 参照先の選択
    auto refTitle = new QLabel("参照先レイヤー", this);
    refTitle->setStyleSheet("font-weight: bold; font-size: 11px; color: #333333;");
    mainLayout->addWidget(refTitle);

    m_refGroup = new QButtonGroup(this);
    m_radioCanvas = new QRadioButton("キャンバス (全レイヤー)", this);
    m_radioActiveLayer = new QRadioButton("アクティブなレイヤー", this);

    m_radioCanvas->setChecked(true);

    m_refGroup->addButton(m_radioCanvas, static_cast<int>(BucketReference::Canvas));
    m_refGroup->addButton(m_radioActiveLayer, static_cast<int>(BucketReference::ActiveLayer));

    mainLayout->addWidget(m_radioCanvas);
    mainLayout->addWidget(m_radioActiveLayer);

    connect(m_refGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_refMode = static_cast<BucketReference>(id);
        emit referenceModeChanged(m_refMode);
    });

    auto line2 = new QFrame(this);
    line2->setFrameShape(QFrame::HLine);
    line2->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line2);

    // 7. 領域拡縮 (-10 〜 +10 px)
    m_expandScale = new SpinScale("領域拡縮", -10.0, 10.0, 1.0, 0, " px", this);
    m_expandScale->setValue(m_expandPixels);
    mainLayout->addWidget(m_expandScale);

    connect(m_expandScale, &SpinScale::valueChanged, this, [this](double val) {
        m_expandPixels = static_cast<int>(std::round(val));
        emit expandPixelsChanged(m_expandPixels);
    });

    auto line3 = new QFrame(this);
    line3->setFrameShape(QFrame::HLine);
    line3->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line3);

    // 8. 細い領域にしみこむ
    m_narrowAreaCheck = new QCheckBox("細い領域にしみこむ", this);
    m_narrowAreaCheck->setChecked(true);
    m_narrowAreaCheck->setToolTip("三角形の頂点のような鋭角な先端や、細い隙間への塗り残しを防ぎます");
    mainLayout->addWidget(m_narrowAreaCheck);

    connect(m_narrowAreaCheck, &QCheckBox::toggled, this, &PatternFillSettingsWidget::narrowAreaFillChanged);

    auto line4 = new QFrame(this);
    line4->setFrameShape(QFrame::HLine);
    line4->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line4);

    // 9. 色の誤差 (Tolerance: 0 〜 100)
    m_toleranceScale = new SpinScale("色の誤差", 0.0, 100.0, 1.0, 0, "", this);
    m_toleranceScale->setValue(m_tolerance);
    mainLayout->addWidget(m_toleranceScale);

    connect(m_toleranceScale, &SpinScale::valueChanged, this, [this](double val) {
        m_tolerance = static_cast<int>(std::round(val));
        emit toleranceChanged(m_tolerance);
    });

    mainLayout->addStretch();
}

void PatternFillSettingsWidget::reloadPatterns() {
    m_patternImages.clear();
    m_patternListWidget->clear();

    QStringList searchDirs;
    searchDirs << QCoreApplication::applicationDirPath() + "/pattern";
    searchDirs << QDir::currentPath() + "/build/pattern";
    searchDirs << QDir::currentPath() + "/pattern";

    QString targetDir;
    for (const auto& path : searchDirs) {
        if (QDir(path).exists()) {
            targetDir = path;
            break;
        }
    }
    if (targetDir.isEmpty()) {
        targetDir = searchDirs.first();
        QDir().mkpath(targetDir);
    }

    QStringList nameFilters;
    nameFilters << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp" << "*.webp" << "*.tif" << "*.tiff";

    QDirIterator it(targetDir, nameFilters, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString filePath = it.next();
        QImage img(filePath);
        if (!img.isNull()) {
            QFileInfo fi(filePath);
            QString key = fi.fileName();
            int dup = 1;
            while (m_patternImages.contains(key)) {
                key = QString("%1 (%2)").arg(fi.fileName()).arg(dup++);
            }
            m_patternImages[key] = img;

            auto item = new QListWidgetItem(QIcon(QPixmap::fromImage(img.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation))), "");
            item->setData(Qt::UserRole, key);
            item->setToolTip(QString("%1 (%2x%3)").arg(fi.fileName()).arg(img.width()).arg(img.height()));
            m_patternListWidget->addItem(item);
        }
    }

    if (!m_clipboardImage.isNull()) {
        QString key = "clipboard";
        m_patternImages[key] = m_clipboardImage;
        auto item = new QListWidgetItem(QIcon(QPixmap::fromImage(m_clipboardImage.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation))), "");
        item->setData(Qt::UserRole, key);
        item->setToolTip("クリップボード画像");
        m_patternListWidget->insertItem(0, item);
    }

    // デフォルトパターンのフォールバック
    if (m_patternImages.isEmpty()) {
        QImage defImg(16, 16, QImage::Format_ARGB32);
        defImg.fill(Qt::white);
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                if ((x / 4 + y / 4) % 2 == 0) {
                    defImg.setPixelColor(x, y, Qt::black);
                }
            }
        }
        QString key = "checkerboard";
        m_patternImages[key] = defImg;
        auto item = new QListWidgetItem(QIcon(QPixmap::fromImage(defImg.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation))), "");
        item->setData(Qt::UserRole, key);
        item->setToolTip("チェッカーパターン");
        m_patternListWidget->addItem(item);
    }

    // 以前選択されていたパターンがあれば再選択、無ければ最初の項目を選択
    int selectIdx = 0;
    if (!m_selectedPatternName.isEmpty()) {
        for (int i = 0; i < m_patternListWidget->count(); ++i) {
            if (m_patternListWidget->item(i)->data(Qt::UserRole).toString() == m_selectedPatternName) {
                selectIdx = i;
                break;
            }
        }
    }

    if (m_patternListWidget->count() > 0) {
        m_patternListWidget->setCurrentRow(selectIdx);
        m_selectedPatternName = m_patternListWidget->item(selectIdx)->data(Qt::UserRole).toString();
        emit patternImageChanged(m_patternImages[m_selectedPatternName], m_selectedPatternName);
    }
}

void PatternFillSettingsWidget::importFromClipboard() {
    QClipboard* clipboard = QApplication::clipboard();
    if (!clipboard) return;
    QImage img = clipboard->image();
    if (img.isNull()) return;

    m_clipboardImage = img;
    reloadPatterns();
    setSelectedPatternByName("clipboard");
}

void PatternFillSettingsWidget::importFromFile() {
    QString file = QFileDialog::getOpenFileName(this, "パターン画像を開く", "", "画像ファイル (*.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff)");
    if (file.isEmpty()) return;

    QImage img(file);
    if (img.isNull()) return;

    QFileInfo fi(file);
    QString key = fi.fileName();
    int dup = 1;
    while (m_patternImages.contains(key)) {
        key = QString("%1 (%2)").arg(fi.fileName()).arg(dup++);
    }
    m_patternImages[key] = img;

    auto item = new QListWidgetItem(QIcon(QPixmap::fromImage(img.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation))), "");
    item->setData(Qt::UserRole, key);
    item->setToolTip(QString("%1 (%2x%3)").arg(fi.fileName()).arg(img.width()).arg(img.height()));
    m_patternListWidget->addItem(item);

    setSelectedPatternByName(key);
}

BlendMode PatternFillSettingsWidget::blendMode() const {
    if (!m_blendCombo || m_blendCombo->currentIndex() < 0) return BlendMode::Normal;
    return static_cast<BlendMode>(m_blendCombo->currentData().toInt());
}

void PatternFillSettingsWidget::setBlendMode(BlendMode mode) {
    if (!m_blendCombo) return;
    int idx = m_blendCombo->findData(static_cast<int>(mode));
    if (idx >= 0) m_blendCombo->setCurrentIndex(idx);
}

void PatternFillSettingsWidget::setDensity(float density) {
    m_density = std::clamp(density, 0.01f, 1.0f);
    if (m_densityScale) m_densityScale->setValue(m_density * 100.0);
}

void PatternFillSettingsWidget::setPatternScale(float scale) {
    m_patternScale = std::clamp(scale, 0.1f, 5.0f);
    if (m_scaleScale) m_scaleScale->setValue(m_patternScale * 100.0);
}

void PatternFillSettingsWidget::setPatternAngle(float angle) {
    m_patternAngle = std::clamp(angle, 0.0f, 360.0f);
    if (m_angleScale) m_angleScale->setValue(m_patternAngle);
}

void PatternFillSettingsWidget::setReferenceMode(BucketReference mode) {
    m_refMode = mode;
    if (m_radioActiveLayer && m_radioCanvas) {
        if (mode == BucketReference::ActiveLayer) {
            m_radioActiveLayer->setChecked(true);
        } else {
            m_radioCanvas->setChecked(true);
        }
    }
}

void PatternFillSettingsWidget::setExpandPixels(int pixels) {
    m_expandPixels = std::clamp(pixels, -10, 10);
    if (m_expandScale) m_expandScale->setValue(m_expandPixels);
}

void PatternFillSettingsWidget::setNarrowAreaFill(bool enable) {
    if (m_narrowAreaCheck) m_narrowAreaCheck->setChecked(enable);
}

void PatternFillSettingsWidget::setTolerance(int tol) {
    m_tolerance = std::clamp(tol, 0, 100);
    if (m_toleranceScale) m_toleranceScale->setValue(m_tolerance);
}

QImage PatternFillSettingsWidget::currentPatternImage() const {
    if (m_patternImages.contains(m_selectedPatternName)) {
        return m_patternImages[m_selectedPatternName];
    }
    return QImage();
}

void PatternFillSettingsWidget::setSelectedPatternByName(const QString& name) {
    if (name.isEmpty() || !m_patternListWidget) return;
    for (int i = 0; i < m_patternListWidget->count(); ++i) {
        if (m_patternListWidget->item(i)->data(Qt::UserRole).toString() == name) {
            m_patternListWidget->setCurrentRow(i);
            break;
        }
    }
}

} // namespace Kaburagi
