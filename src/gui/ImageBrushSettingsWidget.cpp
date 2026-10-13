#include "ImageBrushSettingsWidget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDir>
#include <QDirIterator>
#include <QCoreApplication>
#include <QClipboard>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QFileInfo>

namespace Kaburagi {

ImageBrushSettingsWidget::ImageBrushSettingsWidget(QWidget* parent)
    : QWidget(parent) {
    setupUI();
    reloadBrushImages();
}

void ImageBrushSettingsWidget::setupUI() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // 1. 合成モード
    auto modeLayout = new QHBoxLayout();
    auto modeLabel = new QLabel("合成モード:", this);
    modeLabel->setStyleSheet("font-size: 11px;");
    m_blendCombo = new QComboBox(this);
    m_blendCombo->setStyleSheet("font-size: 11px;");
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

    // 2. ブラシ画像選択グループ (ScrolledWindow 内に配置)
    auto imgGroup = new QGroupBox("ブラシ画像", this);
    auto imgLayout = new QVBoxLayout(imgGroup);
    imgLayout->setContentsMargins(4, 6, 4, 4);
    imgLayout->setSpacing(4);

    // ボタン行 (再読み込み / クリップボード)
    auto btnLayout = new QHBoxLayout();
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(4);

    m_btnReload = new QPushButton("🔄 再読み込み", this);
    m_btnReload->setToolTip("brush_images フォルダから画像を再読み込みします");
    btnLayout->addWidget(m_btnReload);

    m_btnClipboard = new QPushButton("📋 クリップボード", this);
    m_btnClipboard->setToolTip("クリップボードの画像をブラシとして取り込みます");
    btnLayout->addWidget(m_btnClipboard);

    imgLayout->addLayout(btnLayout);

    connect(m_btnReload, &QPushButton::clicked, this, &ImageBrushSettingsWidget::reloadBrushImages);
    connect(m_btnClipboard, &QPushButton::clicked, this, [this]() {
        importFromClipboard(true);
    });

    // サムネイル選択一覧 (QListWidget with IconMode)
    m_imageListWidget = new QListWidget(this);
    m_imageListWidget->setViewMode(QListView::IconMode);
    m_imageListWidget->setIconSize(QSize(40, 40));
    m_imageListWidget->setGridSize(QSize(48, 48));
    m_imageListWidget->setResizeMode(QListWidget::Adjust);
    m_imageListWidget->setMovement(QListView::Static);
    m_imageListWidget->setSpacing(2);
    m_imageListWidget->setFixedHeight(120);
    m_imageListWidget->setStyleSheet(
        "QListWidget { background: #ffffff; border: 1px solid #7f9db9; }"
        "QListWidget::item { border: 1px solid transparent; border-radius: 3px; }"
        "QListWidget::item:selected { background: #cce8ff; border: 1px solid #3399ff; }"
        "QListWidget::item:hover { background: #e5f3ff; }"
    );
    imgLayout->addWidget(m_imageListWidget);

    connect(m_imageListWidget, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* current, QListWidgetItem*) {
        if (!current) return;
        QString name = current->data(Qt::UserRole).toString();
        if (name != m_selectedImageName && m_brushImages.contains(name)) {
            m_selectedImageName = name;
            emit brushImageChanged(m_brushImages[name], name);
            emit imageSelected(name, m_brushImages[name]);
        }
    });

    mainLayout->addWidget(imgGroup);

    // 3. 基本パラメータ (サイズ, 最小サイズ, 間隔, 濃度, 最低筆圧)
    m_sizeScale = new SpinScale("サイズ", 0.1, 500.0, 1.0, 1, " px", this);
    mainLayout->addWidget(m_sizeScale);
    connect(m_sizeScale, &SpinScale::valueChanged, this, [this](double val) {
        emit brushSizeChanged(static_cast<float>(val));
    });

    m_minSizeScale = new SpinScale("最小サイズ", 0.0, 100.0, 1.0, 0, "%", this);
    mainLayout->addWidget(m_minSizeScale);
    connect(m_minSizeScale, &SpinScale::valueChanged, this, [this](double val) {
        emit minSizeChanged(static_cast<float>(val) / 100.0f);
    });

    m_spacingScale = new SpinScale("間隔", 1.0, 500.0, 1.0, 0, "%", this);
    mainLayout->addWidget(m_spacingScale);
    connect(m_spacingScale, &SpinScale::valueChanged, this, [this](double val) {
        emit spacingChanged(static_cast<int>(val));
    });

    m_densityScale = new SpinScale("濃度", 1.0, 100.0, 1.0, 0, "%", this);
    mainLayout->addWidget(m_densityScale);
    connect(m_densityScale, &SpinScale::valueChanged, this, [this](double val) {
        emit densityChanged(static_cast<float>(val) / 100.0f);
    });

    m_minPressureScale = new SpinScale("最低筆圧", 0.0, 100.0, 1.0, 0, "%", this);
    mainLayout->addWidget(m_minPressureScale);
    connect(m_minPressureScale, &SpinScale::valueChanged, this, [this](double val) {
        emit minPressureChanged(static_cast<int>(val));
    });

    // 4. 筆圧影響チェックボックス
    auto pressureBox = new QHBoxLayout();
    pressureBox->setContentsMargins(0, 2, 0, 2);
    m_pressureSizeCheck = new QCheckBox("筆圧(サイズ)", this);
    m_pressureSizeCheck->setChecked(true);
    m_pressureDensityCheck = new QCheckBox("筆圧(濃度)", this);
    m_pressureDensityCheck->setChecked(false);
    pressureBox->addWidget(m_pressureSizeCheck);
    pressureBox->addWidget(m_pressureDensityCheck);
    mainLayout->addLayout(pressureBox);

    connect(m_pressureSizeCheck, &QCheckBox::toggled, this, &ImageBrushSettingsWidget::pressureSizeChanged);
    connect(m_pressureDensityCheck, &QCheckBox::toggled, this, &ImageBrushSettingsWidget::pressureDensityChanged);

    // 5. 画像の反転と回転設定
    auto transGroup = new QGroupBox("変形・回転", this);
    auto transLayout = new QVBoxLayout(transGroup);
    transLayout->setContentsMargins(4, 6, 4, 4);
    transLayout->setSpacing(4);

    auto flipBox = new QHBoxLayout();
    flipBox->setContentsMargins(0, 0, 0, 0);
    m_flipHCheck = new QCheckBox("左右反転", this);
    m_flipVCheck = new QCheckBox("上下反転", this);
    flipBox->addWidget(m_flipHCheck);
    flipBox->addWidget(m_flipVCheck);
    transLayout->addLayout(flipBox);

    connect(m_flipHCheck, &QCheckBox::toggled, this, &ImageBrushSettingsWidget::flipHChanged);
    connect(m_flipVCheck, &QCheckBox::toggled, this, &ImageBrushSettingsWidget::flipVChanged);

    m_angleScale = new SpinScale("回転角", 0.0, 360.0, 1.0, 0, "°", this);
    transLayout->addWidget(m_angleScale);
    connect(m_angleScale, &SpinScale::valueChanged, this, [this](double val) {
        emit angleChanged(static_cast<int>(val));
    });

    m_followCursorCheck = new QCheckBox("カーソル移動方向に回転", this);
    transLayout->addWidget(m_followCursorCheck);
    connect(m_followCursorCheck, &QCheckBox::toggled, this, &ImageBrushSettingsWidget::followCursorChanged);

    // ランダム回転
    m_randomRotateCheck = new QCheckBox("ランダムに画像を回転", this);
    transLayout->addWidget(m_randomRotateCheck);
    connect(m_randomRotateCheck, &QCheckBox::toggled, this, [this](bool checked) {
        updateRandomRotateVisibility();
        emit randomRotateChanged(checked);
    });

    m_randomRotateContainer = new QWidget(this);
    auto randRotLayout = new QVBoxLayout(m_randomRotateContainer);
    randRotLayout->setContentsMargins(12, 0, 0, 0);
    randRotLayout->setSpacing(2);
    m_randomRotateAmountScale = new SpinScale("回転変化量", 0.0, 360.0, 1.0, 0, "°", m_randomRotateContainer);
    randRotLayout->addWidget(m_randomRotateAmountScale);
    transLayout->addWidget(m_randomRotateContainer);

    connect(m_randomRotateAmountScale, &SpinScale::valueChanged, this, [this](double val) {
        emit randomRotateAmountChanged(static_cast<int>(val));
    });

    // ランダムサイズ
    m_randomSizeCheck = new QCheckBox("ランダムにサイズを変化", this);
    transLayout->addWidget(m_randomSizeCheck);
    connect(m_randomSizeCheck, &QCheckBox::toggled, this, [this](bool checked) {
        updateRandomSizeVisibility();
        emit randomSizeChanged(checked);
    });

    m_randomSizeContainer = new QWidget(this);
    auto randSizeLayout = new QVBoxLayout(m_randomSizeContainer);
    randSizeLayout->setContentsMargins(12, 0, 0, 0);
    randSizeLayout->setSpacing(2);
    m_randomSizeAmountScale = new SpinScale("サイズ変化量", 0.0, 100.0, 1.0, 0, "%", m_randomSizeContainer);
    randSizeLayout->addWidget(m_randomSizeAmountScale);

    m_randomSizeScaleUpCheck = new QCheckBox("ランダムサイズ拡大有無", m_randomSizeContainer);
    randSizeLayout->addWidget(m_randomSizeScaleUpCheck);
    transLayout->addWidget(m_randomSizeContainer);

    connect(m_randomSizeAmountScale, &SpinScale::valueChanged, this, [this](double val) {
        emit randomSizeAmountChanged(static_cast<int>(val));
    });
    connect(m_randomSizeScaleUpCheck, &QCheckBox::toggled, this, &ImageBrushSettingsWidget::randomSizeScaleUpChanged);

    mainLayout->addWidget(transGroup);

    // 6. 散布設定ウィジェット
    m_scatterWidget = new ScatterSettingsWidget(this);
    mainLayout->addWidget(m_scatterWidget);

    connect(m_scatterWidget, &ScatterSettingsWidget::scatterChanged, this, &ImageBrushSettingsWidget::scatterChanged);
    connect(m_scatterWidget, &ScatterSettingsWidget::scatterAmountChanged, this, &ImageBrushSettingsWidget::scatterAmountChanged);
    connect(m_scatterWidget, &ScatterSettingsWidget::scatterRangeChanged, this, &ImageBrushSettingsWidget::scatterRangeChanged);
    connect(m_scatterWidget, &ScatterSettingsWidget::scatterSizeRandomChanged, this, &ImageBrushSettingsWidget::scatterSizeRandomChanged);
    connect(m_scatterWidget, &ScatterSettingsWidget::scatterScaleUpChanged, this, &ImageBrushSettingsWidget::scatterScaleUpChanged);

    updateRandomRotateVisibility();
    updateRandomSizeVisibility();
}

void ImageBrushSettingsWidget::updateRandomRotateVisibility() {
    if (m_randomRotateContainer && m_randomRotateCheck) {
        m_randomRotateContainer->setVisible(m_randomRotateCheck->isChecked());
    }
}

void ImageBrushSettingsWidget::updateRandomSizeVisibility() {
    if (m_randomSizeContainer && m_randomSizeCheck) {
        m_randomSizeContainer->setVisible(m_randomSizeCheck->isChecked());
    }
}

void ImageBrushSettingsWidget::reloadBrushImages() {
    m_brushImages.clear();
    m_imageListWidget->clear();

    // 探索パスの決定
    QStringList searchDirs;
    searchDirs << QCoreApplication::applicationDirPath() + "/brush_images";
    searchDirs << QDir::currentPath() + "/build/brush_images";
    searchDirs << QDir::currentPath() + "/brush_images";

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

    // 画像ファイルの再帰走査
    QStringList nameFilters;
    nameFilters << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp" << "*.webp" << "*.tif" << "*.tiff";

    QDirIterator it(targetDir, nameFilters, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString filePath = it.next();
        QImage img(filePath);
        if (!img.isNull()) {
            QFileInfo fi(filePath);
            QString key = fi.fileName();
            // 重複キーの回避
            int dup = 1;
            while (m_brushImages.contains(key)) {
                key = QString("%1 (%2)").arg(fi.fileName()).arg(dup++);
            }
            m_brushImages[key] = img;

            auto item = new QListWidgetItem(QIcon(QPixmap::fromImage(img.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation))), "");
            item->setData(Qt::UserRole, key);
            item->setToolTip(QString("%1 (%2x%3)").arg(fi.fileName()).arg(img.width()).arg(img.height()));
            m_imageListWidget->addItem(item);
        }
    }

    // クリップボード画像の復元（もし保持されていれば）
    if (!m_clipboardImage.isNull()) {
        QString key = "clipboard";
        m_brushImages[key] = m_clipboardImage;
        auto item = new QListWidgetItem(QIcon(QPixmap::fromImage(m_clipboardImage.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation))), "");
        item->setData(Qt::UserRole, key);
        item->setToolTip("クリップボード画像");
        m_imageListWidget->insertItem(0, item);
    }

    // もし1枚も画像がなければデフォルトの円形ブラシを生成
    if (m_brushImages.isEmpty()) {
        QImage defImg(64, 64, QImage::Format_ARGB32_Premultiplied);
        defImg.fill(Qt::transparent);
        QPainter p(&defImg);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(Qt::black);
        p.setPen(Qt::NoPen);
        p.drawEllipse(4, 4, 56, 56);
        p.end();

        QString key = "default_circle";
        m_brushImages[key] = defImg;
        auto item = new QListWidgetItem(QIcon(QPixmap::fromImage(defImg.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation))), "");
        item->setData(Qt::UserRole, key);
        item->setToolTip("標準丸ブラシ");
        m_imageListWidget->addItem(item);
    }

    // 以前選択されていた画像、または最初の画像を選択
    selectImageByName(m_selectedImageName);
}

bool ImageBrushSettingsWidget::importFromClipboard(bool selectIt) {
    QClipboard* clipboard = QApplication::clipboard();
    if (!clipboard) return false;
    QImage clipImg = clipboard->image();
    if (clipImg.isNull()) return false;

    m_clipboardImage = clipImg;
    QString key = "clipboard";
    m_brushImages[key] = clipImg;

    // 既存のクリップボードアイテムがあれば削除して先頭に追加
    for (int i = 0; i < m_imageListWidget->count(); ++i) {
        if (m_imageListWidget->item(i)->data(Qt::UserRole).toString() == key) {
            delete m_imageListWidget->takeItem(i);
            break;
        }
    }

    auto item = new QListWidgetItem(QIcon(QPixmap::fromImage(clipImg.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation))), "");
    item->setData(Qt::UserRole, key);
    item->setToolTip(QString("クリップボード画像 (%1x%2)").arg(clipImg.width()).arg(clipImg.height()));
    m_imageListWidget->insertItem(0, item);

    if (selectIt) {
        m_imageListWidget->setCurrentItem(item);
    }
    return true;
}

void ImageBrushSettingsWidget::selectImageByName(const QString& name) {
    if (m_imageListWidget->count() == 0) return;

    for (int i = 0; i < m_imageListWidget->count(); ++i) {
        auto item = m_imageListWidget->item(i);
        if (item->data(Qt::UserRole).toString() == name) {
            m_imageListWidget->setCurrentItem(item);
            return;
        }
    }
    // 見つからなければ最初のアイテムを選択
    m_imageListWidget->setCurrentRow(0);
}

BlendMode ImageBrushSettingsWidget::blendMode() const {
    if (!m_blendCombo || m_blendCombo->currentIndex() < 0) return BlendMode::Normal;
    return static_cast<BlendMode>(m_blendCombo->currentData().toInt());
}

void ImageBrushSettingsWidget::setBlendMode(BlendMode mode) {
    if (!m_blendCombo) return;
    int idx = m_blendCombo->findData(static_cast<int>(mode));
    if (idx >= 0) {
        m_blendCombo->setCurrentIndex(idx);
    }
}

float ImageBrushSettingsWidget::brushSize() const {
    return m_sizeScale ? static_cast<float>(m_sizeScale->value()) : 40.0f;
}

void ImageBrushSettingsWidget::setBrushSize(float sz) {
    if (m_sizeScale) m_sizeScale->setValue(sz);
}

float ImageBrushSettingsWidget::minSizeRatio() const {
    return m_minSizeScale ? static_cast<float>(m_minSizeScale->value()) / 100.0f : 0.0f;
}

void ImageBrushSettingsWidget::setMinSizeRatio(float r) {
    if (m_minSizeScale) m_minSizeScale->setValue(r * 100.0);
}

int ImageBrushSettingsWidget::spacing() const {
    return m_spacingScale ? static_cast<int>(m_spacingScale->value()) : 20;
}

void ImageBrushSettingsWidget::setSpacing(int sp) {
    if (m_spacingScale) m_spacingScale->setValue(sp);
}

float ImageBrushSettingsWidget::density() const {
    return m_densityScale ? static_cast<float>(m_densityScale->value()) / 100.0f : 1.0f;
}

void ImageBrushSettingsWidget::setDensity(float d) {
    if (m_densityScale) m_densityScale->setValue(d * 100.0);
}

int ImageBrushSettingsWidget::minPressure() const {
    return m_minPressureScale ? static_cast<int>(m_minPressureScale->value()) : 0;
}

void ImageBrushSettingsWidget::setMinPressure(int p) {
    if (m_minPressureScale) m_minPressureScale->setValue(p);
}

bool ImageBrushSettingsWidget::isPressureSizeEnabled() const {
    return m_pressureSizeCheck ? m_pressureSizeCheck->isChecked() : true;
}

void ImageBrushSettingsWidget::setPressureSizeEnabled(bool en) {
    if (m_pressureSizeCheck) m_pressureSizeCheck->setChecked(en);
}

bool ImageBrushSettingsWidget::isPressureDensityEnabled() const {
    return m_pressureDensityCheck ? m_pressureDensityCheck->isChecked() : false;
}

void ImageBrushSettingsWidget::setPressureDensityEnabled(bool en) {
    if (m_pressureDensityCheck) m_pressureDensityCheck->setChecked(en);
}

bool ImageBrushSettingsWidget::isScatterEnabled() const {
    return m_scatterWidget ? m_scatterWidget->isScatterEnabled() : false;
}

int ImageBrushSettingsWidget::scatterAmount() const {
    return m_scatterWidget ? m_scatterWidget->scatterAmount() : 5;
}

int ImageBrushSettingsWidget::scatterRange() const {
    return m_scatterWidget ? m_scatterWidget->scatterRange() : 100;
}

int ImageBrushSettingsWidget::scatterSizeRandom() const {
    return m_scatterWidget ? m_scatterWidget->scatterSizeRandom() : 0;
}

bool ImageBrushSettingsWidget::isScatterScaleUpEnabled() const {
    return m_scatterWidget ? m_scatterWidget->isScatterScaleUpEnabled() : false;
}

bool ImageBrushSettingsWidget::isFlipH() const {
    return m_flipHCheck ? m_flipHCheck->isChecked() : false;
}

bool ImageBrushSettingsWidget::isFlipV() const {
    return m_flipVCheck ? m_flipVCheck->isChecked() : false;
}

int ImageBrushSettingsWidget::angle() const {
    return m_angleScale ? static_cast<int>(m_angleScale->value()) : 0;
}

bool ImageBrushSettingsWidget::isFollowCursor() const {
    return m_followCursorCheck ? m_followCursorCheck->isChecked() : false;
}

bool ImageBrushSettingsWidget::isRandomRotate() const {
    return m_randomRotateCheck ? m_randomRotateCheck->isChecked() : false;
}

int ImageBrushSettingsWidget::randomRotateAmount() const {
    return m_randomRotateAmountScale ? static_cast<int>(m_randomRotateAmountScale->value()) : 360;
}

bool ImageBrushSettingsWidget::isRandomSize() const {
    return m_randomSizeCheck ? m_randomSizeCheck->isChecked() : false;
}

int ImageBrushSettingsWidget::randomSizeAmount() const {
    return m_randomSizeAmountScale ? static_cast<int>(m_randomSizeAmountScale->value()) : 0;
}

bool ImageBrushSettingsWidget::isRandomSizeScaleUp() const {
    return m_randomSizeScaleUpCheck ? m_randomSizeScaleUpCheck->isChecked() : false;
}

QString ImageBrushSettingsWidget::currentImageName() const {
    return m_selectedImageName;
}

QImage ImageBrushSettingsWidget::currentBrushImage() const {
    if (m_brushImages.contains(m_selectedImageName)) {
        return m_brushImages[m_selectedImageName];
    }
    return QImage();
}

ImageBrushConfig ImageBrushSettingsWidget::config() const {
    ImageBrushConfig cfg;
    cfg.size = brushSize();
    cfg.minSizeRatio = minSizeRatio();
    cfg.spacing = spacing();
    cfg.density = density();
    cfg.blendMode = blendMode();
    cfg.minPressure = minPressure();
    cfg.pressureSize = isPressureSizeEnabled();
    cfg.pressureDensity = isPressureDensityEnabled();
    cfg.scatter = isScatterEnabled();
    cfg.scatterAmount = scatterAmount();
    cfg.scatterRange = scatterRange();
    cfg.scatterSizeRandom = scatterSizeRandom();
    cfg.scatterScaleUp = isScatterScaleUpEnabled();

    cfg.currentImageName = currentImageName();
    cfg.flipH = isFlipH();
    cfg.flipV = isFlipV();
    cfg.angle = angle();
    cfg.followCursor = isFollowCursor();
    cfg.randomRotate = isRandomRotate();
    cfg.randomRotateAmount = randomRotateAmount();
    cfg.randomSize = isRandomSize();
    cfg.randomSizeAmount = randomSizeAmount();
    cfg.randomSizeScaleUp = isRandomSizeScaleUp();
    return cfg;
}

void ImageBrushSettingsWidget::setConfig(const ImageBrushConfig& cfg) {
    if (m_sizeScale) m_sizeScale->setValue(cfg.size);
    if (m_minSizeScale) m_minSizeScale->setValue(cfg.minSizeRatio * 100.0);
    if (m_spacingScale) m_spacingScale->setValue(cfg.spacing);
    if (m_densityScale) m_densityScale->setValue(cfg.density * 100.0);
    setBlendMode(cfg.blendMode);
    if (m_minPressureScale) m_minPressureScale->setValue(cfg.minPressure);
    if (m_pressureSizeCheck) m_pressureSizeCheck->setChecked(cfg.pressureSize);
    if (m_pressureDensityCheck) m_pressureDensityCheck->setChecked(cfg.pressureDensity);

    if (m_scatterWidget) {
        m_scatterWidget->setScatterEnabled(cfg.scatter);
        m_scatterWidget->setScatterAmount(cfg.scatterAmount);
        m_scatterWidget->setScatterRange(cfg.scatterRange);
        m_scatterWidget->setScatterSizeRandom(cfg.scatterSizeRandom);
        m_scatterWidget->setScatterScaleUpEnabled(cfg.scatterScaleUp);
    }

    if (m_flipHCheck) m_flipHCheck->setChecked(cfg.flipH);
    if (m_flipVCheck) m_flipVCheck->setChecked(cfg.flipV);
    if (m_angleScale) m_angleScale->setValue(cfg.angle);
    if (m_followCursorCheck) m_followCursorCheck->setChecked(cfg.followCursor);
    if (m_randomRotateCheck) m_randomRotateCheck->setChecked(cfg.randomRotate);
    if (m_randomRotateAmountScale) m_randomRotateAmountScale->setValue(cfg.randomRotateAmount);
    if (m_randomSizeCheck) m_randomSizeCheck->setChecked(cfg.randomSize);
    if (m_randomSizeAmountScale) m_randomSizeAmountScale->setValue(cfg.randomSizeAmount);
    if (m_randomSizeScaleUpCheck) m_randomSizeScaleUpCheck->setChecked(cfg.randomSizeScaleUp);

    updateRandomRotateVisibility();
    updateRandomSizeVisibility();

    QString imgName = !cfg.currentImageName.isEmpty() ? cfg.currentImageName : cfg.selectedImageId;
    if (!imgName.isEmpty()) {
        selectImageByName(imgName);
    }
}

} // namespace Kaburagi
