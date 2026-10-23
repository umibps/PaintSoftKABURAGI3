#include "SelectionDialogs.hpp"
#include "Document.hpp"
#include "CanvasWidget.hpp"
#include "UndoCommands.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPainter>
#include <QTransform>
#include <QSettings>
#include <cmath>
#include <algorithm>
#include <vector>

namespace Kaburagi {

// --- 距離変換ヘルパー (Felzenszwalb-Huttenlocher アルゴリズム) ---
static void dt1D(const float* f, float* d, int* v, float* z, int n) {
    int k = 0;
    v[0] = 0;
    z[0] = -1e20f;
    z[1] = 1e20f;
    for (int q = 1; q < n; ++q) {
        float s = ((f[q] + q * q) - (f[v[k]] + v[k] * v[k])) / (2.0f * q - 2.0f * v[k]);
        while (s <= z[k]) {
            --k;
            s = ((f[q] + q * q) - (f[v[k]] + v[k] * v[k])) / (2.0f * q - 2.0f * v[k]);
        }
        ++k;
        v[k] = q;
        z[k] = s;
        z[k + 1] = 1e20f;
    }
    k = 0;
    for (int q = 0; q < n; ++q) {
        while (z[k + 1] < q) ++k;
        d[q] = (q - v[k]) * (q - v[k]) + f[v[k]];
    }
}

static void dt2D(const std::vector<float>& inGrid, std::vector<float>& outGrid, int w, int h) {
    std::vector<float> temp(static_cast<size_t>(w) * h, 0.0f);
    int maxDim = std::max(w, h);
    std::vector<int> v(maxDim + 1);
    std::vector<float> z(maxDim + 2);
    std::vector<float> colIn(h);
    std::vector<float> colOut(h);

    // 縦方向 (Columns)
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) {
            colIn[y] = inGrid[static_cast<size_t>(y) * w + x];
        }
        dt1D(colIn.data(), colOut.data(), v.data(), z.data(), h);
        for (int y = 0; y < h; ++y) {
            temp[static_cast<size_t>(y) * w + x] = colOut[y];
        }
    }

    // 横方向 (Rows)
    std::vector<float> rowOut(w);
    for (int y = 0; y < h; ++y) {
        const float* rIn = temp.data() + static_cast<size_t>(y) * w;
        dt1D(rIn, rowOut.data(), v.data(), z.data(), w);
        float* rDst = outGrid.data() + static_cast<size_t>(y) * w;
        std::memcpy(rDst, rowOut.data(), w * sizeof(float));
    }
}

// 矩形（チェビシェフ）膨張ヘルパー (スライド窓法 O(N))
static std::vector<uint8_t> squareDilate(const std::vector<uint8_t>& src, int w, int h, int r) {
    std::vector<uint8_t> hPass(static_cast<size_t>(w) * h, 0);
    for (int y = 0; y < h; ++y) {
        size_t rowOffset = static_cast<size_t>(y) * w;
        int count = 0;
        int winSize = std::min(w, r + 1);
        for (int x = 0; x < winSize; ++x) {
            if (src[rowOffset + x]) ++count;
        }
        for (int x = 0; x < w; ++x) {
            if (count > 0) hPass[rowOffset + x] = 1;
            int removeIdx = x - r;
            if (removeIdx >= 0 && src[rowOffset + removeIdx]) --count;
            int addIdx = x + r + 1;
            if (addIdx < w && src[rowOffset + addIdx]) ++count;
        }
    }

    std::vector<uint8_t> dst(static_cast<size_t>(w) * h, 0);
    for (int x = 0; x < w; ++x) {
        int count = 0;
        int winSize = std::min(h, r + 1);
        for (int y = 0; y < winSize; ++y) {
            if (hPass[static_cast<size_t>(y) * w + x]) ++count;
        }
        for (int y = 0; y < h; ++y) {
            if (count > 0) dst[static_cast<size_t>(y) * w + x] = 1;
            int removeIdx = y - r;
            if (removeIdx >= 0 && hPass[static_cast<size_t>(removeIdx) * w + x]) --count;
            int addIdx = y + r + 1;
            if (addIdx < h && hPass[static_cast<size_t>(addIdx) * w + x]) ++count;
        }
    }
    return dst;
}

ModifySelectionDialog::ModifySelectionDialog(Document* doc, CanvasWidget* canvas, QWidget* parent)
    : QDialog(parent), m_document(doc), m_canvas(canvas) {
    if (m_document) {
        m_originalPath = m_document->selection();
    }
    setupUi();
    updatePreview();
}

ModifySelectionDialog::~ModifySelectionDialog() = default;

void ModifySelectionDialog::setupUi() {
    setWindowTitle(tr("選択範囲の拡大/縮小"));
    setModal(true);
    resize(360, 310);

    QSettings settings;
    settings.beginGroup("Selection/ModifySelection");
    int initAction = settings.value("actionType", static_cast<int>(ActionType::Expand)).toInt();
    int initMethod = settings.value("methodType", static_cast<int>(MethodType::Pixel)).toInt();
    int initPixel = settings.value("pixelAmount", 5).toInt();
    int initCorner = settings.value("cornerType", static_cast<int>(CornerType::Round)).toInt();
    double initRatio = settings.value("ratioPercent", 110.0).toDouble();
    bool initPreview = settings.value("preview", true).toBool();
    settings.endGroup();

    auto mainLayout = new QVBoxLayout(this);

    // 1. 動作モード (拡大 / 縮小)
    auto actionGroup = new QGroupBox(tr("処理"), this);
    auto actionLayout = new QHBoxLayout(actionGroup);
    m_radioExpand = new QRadioButton(tr("拡大"), actionGroup);
    m_radioContract = new QRadioButton(tr("縮小"), actionGroup);
    if (initAction == static_cast<int>(ActionType::Contract)) {
        m_radioContract->setChecked(true);
    } else {
        m_radioExpand->setChecked(true);
    }
    actionLayout->addWidget(m_radioExpand);
    actionLayout->addWidget(m_radioContract);
    mainLayout->addWidget(actionGroup);

    // 2. 指定方式 (ピクセル数 / 比率)
    auto methodGroup = new QGroupBox(tr("指定方式"), this);
    auto methodLayout = new QVBoxLayout(methodGroup);

    auto radioRow = new QHBoxLayout();
    m_radioPixel = new QRadioButton(tr("ピクセル数で指定"), methodGroup);
    m_radioRatio = new QRadioButton(tr("比率(%)で指定"), methodGroup);
    if (initMethod == static_cast<int>(MethodType::Ratio)) {
        m_radioRatio->setChecked(true);
    } else {
        m_radioPixel->setChecked(true);
    }
    radioRow->addWidget(m_radioPixel);
    radioRow->addWidget(m_radioRatio);
    methodLayout->addLayout(radioRow);

    // 2-1. ピクセル設定UI
    m_pixelWidget = new QWidget(methodGroup);
    auto pixelLayout = new QVBoxLayout(m_pixelWidget);
    pixelLayout->setContentsMargins(0, 5, 0, 0);

    auto spinRow = new QHBoxLayout();
    spinRow->addWidget(new QLabel(tr("拡大量 / 縮小量:"), m_pixelWidget));
    m_pixelSpin = new QSpinBox(m_pixelWidget);
    m_pixelSpin->setRange(1, 1000);
    m_pixelSpin->setValue(initPixel);
    m_pixelSpin->setSuffix(" px");
    m_pixelSpin->setMinimumWidth(90);
    spinRow->addWidget(m_pixelSpin);
    pixelLayout->addLayout(spinRow);

    auto cornerRow = new QHBoxLayout();
    cornerRow->addWidget(new QLabel(tr("角の形状:"), m_pixelWidget));
    m_radioRound = new QRadioButton(tr("円形 (丸くする)"), m_pixelWidget);
    m_radioSquare = new QRadioButton(tr("矩形 (直角)"), m_pixelWidget);
    if (initCorner == static_cast<int>(CornerType::Square)) {
        m_radioSquare->setChecked(true);
    } else {
        m_radioRound->setChecked(true);
    }
    cornerRow->addWidget(m_radioRound);
    cornerRow->addWidget(m_radioSquare);
    pixelLayout->addLayout(cornerRow);

    methodLayout->addWidget(m_pixelWidget);

    // 2-2. 比率設定UI
    m_ratioWidget = new QWidget(methodGroup);
    auto ratioLayout = new QHBoxLayout(m_ratioWidget);
    ratioLayout->setContentsMargins(0, 5, 0, 0);
    ratioLayout->addWidget(new QLabel(tr("拡大率 / 縮小率:"), m_ratioWidget));
    m_ratioSpin = new QDoubleSpinBox(m_ratioWidget);
    m_ratioSpin->setRange(1.0, 1000.0);
    m_ratioSpin->setValue(initRatio);
    m_ratioSpin->setSingleStep(5.0);
    m_ratioSpin->setDecimals(1);
    m_ratioSpin->setSuffix(" %");
    m_ratioSpin->setMinimumWidth(90);
    ratioLayout->addWidget(m_ratioSpin);
    methodLayout->addWidget(m_ratioWidget);

    bool isPixel = (initMethod != static_cast<int>(MethodType::Ratio));
    m_pixelWidget->setVisible(isPixel);
    m_ratioWidget->setVisible(!isPixel);

    mainLayout->addWidget(methodGroup);

    // 3. プレビュー
    auto previewRow = new QHBoxLayout();
    m_previewCheck = new QCheckBox(tr("プレビュー"), this);
    m_previewCheck->setChecked(initPreview);
    previewRow->addWidget(m_previewCheck);
    mainLayout->addLayout(previewRow);

    // 4. ダイアログボタン
    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ModifySelectionDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &ModifySelectionDialog::reject);
    mainLayout->addWidget(buttonBox);

    // シグナル接続
    connect(m_radioExpand, &QRadioButton::toggled, this, &ModifySelectionDialog::onActionTypeChanged);
    connect(m_radioContract, &QRadioButton::toggled, this, &ModifySelectionDialog::onActionTypeChanged);
    connect(m_radioPixel, &QRadioButton::toggled, this, &ModifySelectionDialog::onMethodTypeChanged);
    connect(m_radioRatio, &QRadioButton::toggled, this, &ModifySelectionDialog::onMethodTypeChanged);

    connect(m_pixelSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ModifySelectionDialog::updatePreview);
    connect(m_radioRound, &QRadioButton::toggled, this, &ModifySelectionDialog::updatePreview);
    connect(m_radioSquare, &QRadioButton::toggled, this, &ModifySelectionDialog::updatePreview);

    connect(m_ratioSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ModifySelectionDialog::updatePreview);
    connect(m_previewCheck, &QCheckBox::toggled, this, &ModifySelectionDialog::updatePreview);
}

ModifySelectionDialog::ActionType ModifySelectionDialog::actionType() const {
    return (m_radioContract && m_radioContract->isChecked()) ? ActionType::Contract : ActionType::Expand;
}

void ModifySelectionDialog::setActionType(ActionType action) {
    if (action == ActionType::Contract) {
        if (m_radioContract) m_radioContract->setChecked(true);
    } else {
        if (m_radioExpand) m_radioExpand->setChecked(true);
    }
}

ModifySelectionDialog::MethodType ModifySelectionDialog::methodType() const {
    return (m_radioRatio && m_radioRatio->isChecked()) ? MethodType::Ratio : MethodType::Pixel;
}

void ModifySelectionDialog::setMethodType(MethodType method) {
    if (method == MethodType::Ratio) {
        if (m_radioRatio) m_radioRatio->setChecked(true);
    } else {
        if (m_radioPixel) m_radioPixel->setChecked(true);
    }
}

int ModifySelectionDialog::pixelAmount() const {
    return m_pixelSpin ? m_pixelSpin->value() : 5;
}

void ModifySelectionDialog::setPixelAmount(int px) {
    if (m_pixelSpin) m_pixelSpin->setValue(px);
}

ModifySelectionDialog::CornerType ModifySelectionDialog::cornerType() const {
    return (m_radioSquare && m_radioSquare->isChecked()) ? CornerType::Square : CornerType::Round;
}

void ModifySelectionDialog::setCornerType(CornerType corner) {
    if (corner == CornerType::Square) {
        if (m_radioSquare) m_radioSquare->setChecked(true);
    } else {
        if (m_radioRound) m_radioRound->setChecked(true);
    }
}

double ModifySelectionDialog::ratioPercent() const {
    return m_ratioSpin ? m_ratioSpin->value() : 100.0;
}

void ModifySelectionDialog::setRatioPercent(double pct) {
    if (m_ratioSpin) m_ratioSpin->setValue(pct);
}

bool ModifySelectionDialog::isPreviewEnabled() const {
    return m_previewCheck ? m_previewCheck->isChecked() : true;
}

void ModifySelectionDialog::setPreviewEnabled(bool enabled) {
    if (m_previewCheck) m_previewCheck->setChecked(enabled);
}

void ModifySelectionDialog::onActionTypeChanged() {
    if (methodType() == MethodType::Ratio && m_ratioSpin) {
        if (actionType() == ActionType::Expand) {
            if (m_ratioSpin->value() < 100.0) m_ratioSpin->setValue(110.0);
        } else {
            if (m_ratioSpin->value() > 100.0) m_ratioSpin->setValue(90.0);
        }
    }
    updatePreview();
}

void ModifySelectionDialog::onMethodTypeChanged() {
    bool isPixel = (methodType() == MethodType::Pixel);
    if (m_pixelWidget) m_pixelWidget->setVisible(isPixel);
    if (m_ratioWidget) m_ratioWidget->setVisible(!isPixel);
    updatePreview();
}

QPainterPath ModifySelectionDialog::calculateModifiedPath() const {
    if (!m_document || m_originalPath.isEmpty()) {
        return QPainterPath();
    }

    int docW = m_document->width();
    int docH = m_document->height();
    QRect canvasRect(0, 0, docW, docH);

    // --- 比率(%)による拡大・縮小 ---
    if (methodType() == MethodType::Ratio) {
        double pct = ratioPercent();
        double scale = pct / 100.0;
        if (scale <= 0.001) return QPainterPath();

        QRectF b = m_originalPath.boundingRect();
        QPointF center = b.center();

        QTransform t;
        t.translate(center.x(), center.y());
        t.scale(scale, scale);
        t.translate(-center.x(), -center.y());

        QPainterPath scaledPath = t.map(m_originalPath);
        QPainterPath cPath;
        cPath.addRect(canvasRect);
        return scaledPath.intersected(cPath);
    }

    // --- ピクセル数による拡大・縮小 ---
    int r = pixelAmount();
    if (r <= 0) return m_originalPath;

    // 単純な矩形の場合の高速化 (矩形角)
    bool isRect = false;
    QRectF rectBounds = m_originalPath.boundingRect();
    if (m_originalPath.elementCount() <= 5) {
        QPainterPath testRect;
        testRect.addRect(rectBounds);
        if (m_originalPath == testRect) isRect = true;
    }

    if (isRect && cornerType() == CornerType::Square) {
        QRectF adj = (actionType() == ActionType::Expand)
            ? rectBounds.adjusted(-r, -r, r, r)
            : rectBounds.adjusted(r, r, -r, -r);
        adj = adj.intersected(QRectF(canvasRect));
        if (adj.width() <= 0 || adj.height() <= 0) return QPainterPath();
        QPainterPath rp;
        rp.addRect(adj);
        return rp;
    }

    // 一般形状・円形角のモルフォロジー処理
    QRect origBounds = m_originalPath.boundingRect().toAlignedRect();
    if (origBounds.isEmpty()) return QPainterPath();

    QRect roi = (actionType() == ActionType::Expand)
        ? origBounds.adjusted(-r - 2, -r - 2, r + 2, r + 2).intersected(canvasRect)
        : origBounds.adjusted(-2, -2, 2, 2).intersected(canvasRect);

    if (roi.isEmpty()) return QPainterPath();

    int rw = roi.width();
    int rh = roi.height();

    // ROI内に元の選択範囲をレンダリング
    QImage img(rw, rh, QImage::Format_Grayscale8);
    img.fill(0);
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.translate(-roi.left(), -roi.top());
        p.fillPath(m_originalPath, Qt::white);
    }

    std::vector<uint8_t> inMask(static_cast<size_t>(rw) * rh, 0);
    for (int y = 0; y < rh; ++y) {
        const uint8_t* scan = img.constScanLine(y);
        for (int x = 0; x < rw; ++x) {
            if (scan[x] > 127) {
                inMask[static_cast<size_t>(y) * rw + x] = 1;
            }
        }
    }

    std::vector<uint8_t> outMask(static_cast<size_t>(rw) * rh, 0);

    if (cornerType() == CornerType::Square) {
        if (actionType() == ActionType::Expand) {
            outMask = squareDilate(inMask, rw, rh, r);
        } else {
            // 収縮 = 反転 -> 膨張 -> 反転
            std::vector<uint8_t> invMask(inMask.size());
            for (size_t i = 0; i < inMask.size(); ++i) {
                invMask[i] = inMask[i] ? 0 : 1;
            }
            std::vector<uint8_t> dilatedInv = squareDilate(invMask, rw, rh, r);
            for (size_t i = 0; i < dilatedInv.size(); ++i) {
                outMask[i] = (dilatedInv[i] || !inMask[i]) ? 0 : 1;
            }
        }
    } else {
        // 円形 (ユークリッド距離変換)
        std::vector<float> gridIn(static_cast<size_t>(rw) * rh);
        std::vector<float> gridDist(static_cast<size_t>(rw) * rh);
        float rSq = static_cast<float>(r * r);

        if (actionType() == ActionType::Expand) {
            for (size_t i = 0; i < inMask.size(); ++i) {
                gridIn[i] = inMask[i] ? 0.0f : 1e10f;
            }
            dt2D(gridIn, gridDist, rw, rh);
            for (size_t i = 0; i < outMask.size(); ++i) {
                outMask[i] = (gridDist[i] <= rSq) ? 1 : 0;
            }
        } else {
            for (size_t i = 0; i < inMask.size(); ++i) {
                gridIn[i] = (!inMask[i]) ? 0.0f : 1e10f;
            }
            dt2D(gridIn, gridDist, rw, rh);
            for (size_t i = 0; i < outMask.size(); ++i) {
                outMask[i] = (inMask[i] && gridDist[i] > rSq) ? 1 : 0;
            }
        }
    }

    QPainterPath path = Document::maskToPath(outMask.data(), rw, rh, roi.left(), roi.top());
    QPainterPath cPath;
    cPath.addRect(canvasRect);
    return path.intersected(cPath);
}

void ModifySelectionDialog::updatePreview() {
    if (!m_document) return;

    if (isPreviewEnabled()) {
        QPainterPath modPath = calculateModifiedPath();
        m_document->setSelection(modPath);
    } else {
        m_document->setSelection(m_originalPath);
    }

    if (m_canvas) {
        m_canvas->update();
    }
}

void ModifySelectionDialog::accept() {
    QSettings settings;
    settings.beginGroup("Selection/ModifySelection");
    settings.setValue("actionType", static_cast<int>(actionType()));
    settings.setValue("methodType", static_cast<int>(methodType()));
    settings.setValue("pixelAmount", pixelAmount());
    settings.setValue("cornerType", static_cast<int>(cornerType()));
    settings.setValue("ratioPercent", ratioPercent());
    settings.setValue("preview", isPreviewEnabled());
    settings.endGroup();

    if (m_document) {
        m_document->setSelection(m_originalPath);
        QPainterPath finalPath = calculateModifiedPath();

        if (m_document->undoStack()) {
            m_document->undoStack()->push(new SelectCommand(m_document, m_originalPath, finalPath));
        } else {
            m_document->setSelection(finalPath);
        }
    }

    if (m_canvas) {
        m_canvas->update();
    }
    QDialog::accept();
}

void ModifySelectionDialog::reject() {
    if (m_document) {
        m_document->setSelection(m_originalPath);
    }
    if (m_canvas) {
        m_canvas->update();
    }
    QDialog::reject();
}

} // namespace Kaburagi
