#include "ColorPaletteWidget.hpp"
#include "DisplayFilterManager.hpp"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMenu>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QDataStream>
#include <QFile>
#include <algorithm>

namespace Kaburagi {

ColorPaletteWidget::ColorPaletteWidget(QWidget* parent)
    : QWidget(parent) {
    setMouseTracking(true);
    connect(&DisplayFilterManager::instance(), &DisplayFilterManager::filterChanged, this, QOverload<>::of(&QWidget::update));

    // 市松模様（チェッカーボード）ピックスマップの生成 (4x4px 四方のチェック)
    m_checkerPixmap = QPixmap(8, 8);
    {
        QPainter p(&m_checkerPixmap);
        p.fillRect(0, 0, 4, 4, QColor(255, 255, 255));
        p.fillRect(4, 0, 4, 4, QColor(228, 228, 228));
        p.fillRect(0, 4, 4, 4, QColor(228, 228, 228));
        p.fillRect(4, 4, 4, 4, QColor(255, 255, 255));
    }

    // 縦スクロールバーの生成
    m_scrollBar = new QScrollBar(Qt::Vertical, this);
    m_scrollBar->setStyleSheet(
        "QScrollBar:vertical {"
        "    width: 14px;"
        "    background: #e6e6e6;"
        "    margin: 14px 0 14px 0;"
        "    border: 1px solid #c0c0c0;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background: #b8b8b8;"
        "    min-height: 20px;"
        "    border: 1px solid #999999;"
        "    border-radius: 2px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "    background: #a0a0a0;"
        "}"
        "QScrollBar::sub-line:vertical {"
        "    height: 14px;"
        "    background: #d8d8d8;"
        "    subcontrol-position: top;"
        "    subcontrol-origin: margin;"
        "    border: 1px solid #b0b0b0;"
        "}"
        "QScrollBar::add-line:vertical {"
        "    height: 14px;"
        "    background: #d8d8d8;"
        "    subcontrol-position: bottom;"
        "    subcontrol-origin: margin;"
        "    border: 1px solid #b0b0b0;"
        "}"
    );

    connect(m_scrollBar, &QScrollBar::valueChanged, this, [this](int) {
        update();
    });

    initDefaultColors();
    loadSettings();
    updateScrollRange();
}

void ColorPaletteWidget::initDefaultColors() {
    m_colors.assign(COLS * m_rows, QColor());

    // PaintTool SAI風の基本パレットカラー（先頭数行）
    static const QColor defaultPalette[] = {
        QColor(0, 0, 0),       QColor(255, 255, 255), QColor(128, 128, 128), QColor(192, 192, 192),
        QColor(255, 0, 0),     QColor(255, 128, 0),   QColor(255, 255, 0),   QColor(128, 255, 0),
        QColor(0, 255, 0),     QColor(0, 255, 255),   QColor(0, 128, 255),   QColor(0, 0, 255),

        QColor(64, 0, 0),      QColor(128, 64, 0),    QColor(128, 128, 0),   QColor(0, 128, 0),
        QColor(0, 128, 128),   QColor(0, 64, 128),    QColor(0, 0, 128),     QColor(128, 0, 128),
        QColor(255, 0, 128),   QColor(255, 128, 192), QColor(255, 220, 178), QColor(255, 204, 153)
    };

    size_t count = sizeof(defaultPalette) / sizeof(defaultPalette[0]);
    for (size_t i = 0; i < count && i < m_colors.size(); ++i) {
        m_colors[i] = defaultPalette[i];
    }
}

void ColorPaletteWidget::updateScrollRange() {
    int maxScroll = std::max(0, m_rows - VISIBLE_ROWS);
    m_scrollBar->setRange(0, maxScroll);
    m_scrollBar->setPageStep(VISIBLE_ROWS);
    m_scrollBar->setSingleStep(1);
}

QSize ColorPaletteWidget::sizeHint() const {
    int sbWidth = m_scrollBar ? m_scrollBar->sizeHint().width() : 14;
    if (sbWidth <= 0) sbWidth = 14;
    int w = COLS * m_cellSize + 2 + sbWidth;
    int h = VISIBLE_ROWS * m_cellSize + 2;
    return QSize(w, h);
}

QSize ColorPaletteWidget::minimumSizeHint() const {
    return QSize(COLS * 11 + 16, VISIBLE_ROWS * 11 + 2);
}

void ColorPaletteWidget::resizeEvent(QResizeEvent* /*event*/) {
    int sbWidth = m_scrollBar->sizeHint().width();
    if (sbWidth <= 0) sbWidth = 14;

    int availW = width() - sbWidth - 2;
    if (availW > 0) {
        m_cellSize = std::clamp(availW / COLS, 11, 18);
    }

    int gridW = COLS * m_cellSize;
    int gridH = VISIBLE_ROWS * m_cellSize;
    setFixedHeight(gridH + 2);
    m_scrollBar->setGeometry(1 + gridW, 0, sbWidth, gridH + 2);
}

int ColorPaletteWidget::cellFromPos(const QPoint& pos, int& outCol, int& outRow) const {
    int gridWidth = COLS * m_cellSize;
    int gridHeight = VISIBLE_ROWS * m_cellSize;

    if (pos.x() < 1 || pos.x() >= 1 + gridWidth || pos.y() < 1 || pos.y() >= 1 + gridHeight) {
        outCol = -1;
        outRow = -1;
        return -1;
    }

    outCol = (pos.x() - 1) / m_cellSize;
    int visibleRow = (pos.y() - 1) / m_cellSize;
    outRow = visibleRow + m_scrollBar->value();

    if (outCol < 0 || outCol >= COLS || outRow < 0 || outRow >= m_rows) {
        return -1;
    }

    int idx = outRow * COLS + outCol;
    return (idx >= 0 && idx < static_cast<int>(m_colors.size())) ? idx : -1;
}

QColor ColorPaletteWidget::colorAt(int index) const {
    if (index >= 0 && index < static_cast<int>(m_colors.size())) {
        return m_colors[index];
    }
    return QColor();
}

void ColorPaletteWidget::setColorAt(int index, const QColor& color) {
    if (index >= 0 && index < static_cast<int>(m_colors.size())) {
        m_colors[index] = color;
        saveSettings();
        update();
    }
}

void ColorPaletteWidget::clearPalette() {
    std::fill(m_colors.begin(), m_colors.end(), QColor());
    saveSettings();
    update();
}

void ColorPaletteWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    int startRow = m_scrollBar->value();
    int gridW = COLS * m_cellSize;
    int gridH = VISIBLE_ROWS * m_cellSize;

    // 外枠（SAI風サンケンボーダー）
    painter.setPen(QColor(130, 130, 130));
    painter.drawRect(0, 0, gridW + 1, gridH + 1);

    // セルの描画
    for (int vr = 0; vr < VISIBLE_ROWS; ++vr) {
        int r = startRow + vr;
        for (int c = 0; c < COLS; ++c) {
            int idx = r * COLS + c;
            int x = 1 + c * m_cellSize;
            int y = 1 + vr * m_cellSize;
            QRect cellRect(x, y, m_cellSize, m_cellSize);

            // セルの背景：市松模様（チェッカーボード）
            painter.drawTiledPixmap(cellRect, m_checkerPixmap);

            // 色が登録されている場合
            if (idx >= 0 && idx < static_cast<int>(m_colors.size()) && m_colors[idx].isValid()) {
                painter.fillRect(cellRect, DisplayFilterManager::instance().applyFilter(m_colors[idx]));
            }

            // セル境界線（薄いグレーのグリッド）
            painter.setPen(QColor(210, 210, 210));
            painter.drawRect(cellRect);

            // ホバー枠線
            if (idx == m_hoverIndex) {
                painter.setPen(QPen(QColor(0, 120, 255), 1.0));
                painter.drawRect(cellRect);
            }
        }
    }
}

void ColorPaletteWidget::mousePressEvent(QMouseEvent* event) {
    int col = -1, row = -1;
    int idx = cellFromPos(event->pos(), col, row);

    if (idx < 0) {
        QWidget::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        // 左クリック：色登録がある場合は描画色を変更、未登録なら何もしない
        if (m_colors[idx].isValid()) {
            emit colorSelected(m_colors[idx]);
        }
    } else if (event->button() == Qt::RightButton) {
        // 右クリック：ポップアップメニュー表示
        showContextMenu(event->pos(), col, row);
    }
}

void ColorPaletteWidget::mouseMoveEvent(QMouseEvent* event) {
    int col = -1, row = -1;
    int idx = cellFromPos(event->pos(), col, row);
    if (idx != m_hoverIndex) {
        m_hoverIndex = idx;
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void ColorPaletteWidget::leaveEvent(QEvent* event) {
    if (m_hoverIndex != -1) {
        m_hoverIndex = -1;
        update();
    }
    QWidget::leaveEvent(event);
}

void ColorPaletteWidget::wheelEvent(QWheelEvent* event) {
    if (m_scrollBar && m_scrollBar->isVisible()) {
        int numDegrees = event->angleDelta().y() / 8;
        int numSteps = numDegrees / 15;
        m_scrollBar->setValue(m_scrollBar->value() - numSteps);
        event->accept();
    } else {
        QWidget::wheelEvent(event);
    }
}

void ColorPaletteWidget::showContextMenu(const QPoint& pos, int col, int row) {
    int idx = row * COLS + col;
    if (idx < 0 || idx >= static_cast<int>(m_colors.size())) return;

    QMenu menu(this);
    menu.setStyleSheet("QMenu { font-size: 11px; }");

    bool hasColor = m_colors[idx].isValid();

    if (hasColor) {
        menu.addAction(tr("色を上書き (描画色を登録する)"), this, [this, idx]() {
            setColorAt(idx, m_currentColor);
        });
        menu.addAction(tr("色を削除"), this, [this, idx]() {
            setColorAt(idx, QColor());
        });
    } else {
        menu.addAction(tr("色を登録 (描画色を登録する)"), this, [this, idx]() {
            setColorAt(idx, m_currentColor);
        });
    }

    menu.addSeparator();

    menu.addAction(tr("パレットをインポート (.acoファイルをインポート)..."), this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("パレットをインポート"), "", tr("Adobe Color Swatch (*.aco);;All Files (*)"));
        if (!path.isEmpty()) {
            if (!importAco(path, false)) {
                QMessageBox::warning(this, tr("インポート失敗"), tr("ACOファイルの読み込みに失敗しました。"));
            }
        }
    });

    menu.addAction(tr("パレットを追加インポート..."), this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("パレットを追加インポート"), "", tr("Adobe Color Swatch (*.aco);;All Files (*)"));
        if (!path.isEmpty()) {
            if (!importAco(path, true)) {
                QMessageBox::warning(this, tr("追加インポート失敗"), tr("ACOファイルの読み込みに失敗しました。"));
            }
        }
    });

    menu.addAction(tr("パレットをエクスポート (.acoファイルへエクスポート)..."), this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, tr("パレットをエクスポート"), "palette.aco", tr("Adobe Color Swatch (*.aco);;All Files (*)"));
        if (!path.isEmpty()) {
            if (!exportAco(path)) {
                QMessageBox::warning(this, tr("エクスポート失敗"), tr("ACOファイルの保存に失敗しました。"));
            }
        }
    });

    menu.addAction(tr("パレットをクリア"), this, [this]() {
        auto ret = QMessageBox::question(this, tr("パレットのクリア"), tr("パレットの登録色をすべて消去しますか？"), QMessageBox::Yes | QMessageBox::No);
        if (ret == QMessageBox::Yes) {
            clearPalette();
        }
    });

    menu.exec(mapToGlobal(pos));
}

bool ColorPaletteWidget::importAco(const QString& filePath, bool append) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QDataStream in(&file);
    in.setByteOrder(QDataStream::BigEndian);

    quint16 version = 0;
    in >> version;
    if (version != 1 && version != 2) return false;

    quint16 count = 0;
    in >> count;

    std::vector<QColor> loadedColors;

    // Version 1 ブロックの読み込み
    for (quint16 i = 0; i < count; ++i) {
        if (in.atEnd()) break;
        quint16 colorSpace = 0;
        quint16 w = 0, x = 0, y = 0, z = 0;
        in >> colorSpace >> w >> x >> y >> z;
        if (colorSpace == 0) { // RGB
            int r = qRound(w / 65535.0 * 255.0);
            int g = qRound(x / 65535.0 * 255.0);
            int b = qRound(y / 65535.0 * 255.0);
            loadedColors.push_back(QColor(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255)));
        } else if (colorSpace == 1) { // HSB
            int h = qRound(w / 65535.0 * 359.0);
            int s = qRound(x / 65535.0 * 255.0);
            int v = qRound(y / 65535.0 * 255.0);
            QColor col;
            col.setHsv(h, s, v);
            loadedColors.push_back(col);
        } else if (colorSpace == 8) { // Grayscale
            int v = qRound(w / 10000.0 * 255.0);
            v = std::clamp(v, 0, 255);
            loadedColors.push_back(QColor(v, v, v));
        } else {
            int r = (w >> 8) & 0xFF;
            int g = (x >> 8) & 0xFF;
            int b = (y >> 8) & 0xFF;
            loadedColors.push_back(QColor(r, g, b));
        }
    }

    // 後続に Version 2 ブロックが存在する場合は、より詳細なV2の色を優先
    if (!in.atEnd()) {
        quint16 v2 = 0;
        in >> v2;
        if (v2 == 2) {
            quint16 count2 = 0;
            in >> count2;
            std::vector<QColor> v2Colors;
            for (quint16 i = 0; i < count2; ++i) {
                if (in.atEnd()) break;
                quint16 colorSpace = 0;
                quint16 w = 0, x = 0, y = 0, z = 0;
                in >> colorSpace >> w >> x >> y >> z;
                quint32 nameLen = 0;
                in >> nameLen;
                for (quint32 c = 0; c < nameLen; ++c) {
                    quint16 dummyChar;
                    in >> dummyChar;
                }
                if (colorSpace == 0) {
                    int r = qRound(w / 65535.0 * 255.0);
                    int g = qRound(x / 65535.0 * 255.0);
                    int b = qRound(y / 65535.0 * 255.0);
                    v2Colors.push_back(QColor(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255)));
                }
            }
            if (!v2Colors.empty()) {
                loadedColors = std::move(v2Colors);
            }
        }
    }

    if (loadedColors.empty()) return false;

    if (!append) {
        // パレットをクリアして上書きインポート
        std::fill(m_colors.begin(), m_colors.end(), QColor());
        for (size_t i = 0; i < loadedColors.size(); ++i) {
            if (i >= m_colors.size()) {
                m_colors.resize(m_colors.size() + COLS * 4, QColor());
                m_rows = static_cast<int>(m_colors.size()) / COLS;
            }
            m_colors[i] = loadedColors[i];
        }
    } else {
        // 最初の空きセルから順に追加インポート
        size_t writeIdx = 0;
        while (writeIdx < m_colors.size() && m_colors[writeIdx].isValid()) {
            ++writeIdx;
        }
        for (const auto& col : loadedColors) {
            if (writeIdx >= m_colors.size()) {
                m_colors.resize(m_colors.size() + COLS * 4, QColor());
                m_rows = static_cast<int>(m_colors.size()) / COLS;
            }
            m_colors[writeIdx++] = col;
        }
    }

    updateScrollRange();
    saveSettings();
    update();
    return true;
}

bool ColorPaletteWidget::exportAco(const QString& filePath) const {
    std::vector<QColor> validColors;
    for (const auto& c : m_colors) {
        if (c.isValid()) {
            validColors.push_back(c);
        }
    }
    if (validColors.empty()) return false;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;

    QDataStream out(&file);
    out.setByteOrder(QDataStream::BigEndian);

    quint16 count = static_cast<quint16>(std::min<size_t>(validColors.size(), 65535));

    // --- Version 1 Block ---
    out << static_cast<quint16>(1);
    out << count;
    for (quint16 i = 0; i < count; ++i) {
        const auto& c = validColors[i];
        quint16 cs = 0; // RGB
        quint16 r = static_cast<quint16>((c.red() << 8) | c.red());
        quint16 g = static_cast<quint16>((c.green() << 8) | c.green());
        quint16 b = static_cast<quint16>((c.blue() << 8) | c.blue());
        quint16 z = 0;
        out << cs << r << g << b << z;
    }

    // --- Version 2 Block (名前付き、Photoshop/クリスタ等への完全互換) ---
    out << static_cast<quint16>(2);
    out << count;
    for (quint16 i = 0; i < count; ++i) {
        const auto& c = validColors[i];
        quint16 cs = 0; // RGB
        quint16 r = static_cast<quint16>((c.red() << 8) | c.red());
        quint16 g = static_cast<quint16>((c.green() << 8) | c.green());
        quint16 b = static_cast<quint16>((c.blue() << 8) | c.blue());
        quint16 z = 0;
        out << cs << r << g << b << z;

        QString name = QString("Color %1").arg(i + 1);
        quint32 nameLen = static_cast<quint32>(name.length() + 1);
        out << nameLen;
        for (int ch = 0; ch < name.length(); ++ch) {
            out << static_cast<quint16>(name[ch].unicode());
        }
        out << static_cast<quint16>(0);
    }

    return true;
}

void ColorPaletteWidget::saveSettings() {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "KaburagiPaint", "PaintSoftKABURAGI");
    settings.beginGroup("ColorPalette");
    settings.setValue("rows", m_rows);

    QStringList colorList;
    colorList.reserve(m_colors.size());
    for (const auto& c : m_colors) {
        if (c.isValid()) {
            colorList << c.name(QColor::HexRgb);
        } else {
            colorList << "";
        }
    }
    settings.setValue("colors", colorList);
    settings.endGroup();
}

void ColorPaletteWidget::loadSettings() {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "KaburagiPaint", "PaintSoftKABURAGI");
    settings.beginGroup("ColorPalette");
    if (settings.contains("colors")) {
        int savedRows = settings.value("rows", m_rows).toInt();
        if (savedRows >= VISIBLE_ROWS) {
            m_rows = savedRows;
        }
        QStringList colorList = settings.value("colors").toStringList();
        m_colors.assign(m_rows * COLS, QColor());
        for (int i = 0; i < colorList.size() && i < static_cast<int>(m_colors.size()); ++i) {
            if (!colorList[i].isEmpty()) {
                m_colors[i] = QColor(colorList[i]);
            }
        }
    }
    settings.endGroup();
}

} // namespace Kaburagi
