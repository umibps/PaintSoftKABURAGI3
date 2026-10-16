#include "LayerPanelWidget.hpp"
#include "GroupLayer.hpp"
#include "PreferencesDialog.hpp"
#include "UndoCommands.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QStyledItemDelegate>
#include <QLineEdit>
#include <QMouseEvent>
#include <QFontMetrics>
#include <QStyle>
#include <QDrag>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QColorDialog>
#include <QMessageBox>
#include "MergePanelDialog.hpp"

namespace Kaburagi {

class LayerItemDelegate : public QStyledItemDelegate {
public:
    explicit LayerItemDelegate(LayerPanelWidget* panel, QObject* parent = nullptr)
        : QStyledItemDelegate(parent), m_panel(panel) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->save();

        // 1. 背景描画 (選択状態 / 通常状態)
        bool isSelected = (option.state & QStyle::State_Selected);
        if (isSelected) {
            painter->fillRect(option.rect, QColor("#b0c4de"));
        } else {
            painter->fillRect(option.rect, QColor("#ffffff"));
        }
        // 行区切り線
        painter->setPen(QColor("#e8e8e8"));
        painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());

        // レイヤー取得
        bool isVisible = true;
        bool isPinned = false;
        bool isClipping = false;
        if (m_panel && m_panel->document()) {
            int count = static_cast<int>(m_panel->document()->layers().size());
            int layerIndex = count - 1 - index.row();
            auto layer = m_panel->document()->getLayer(layerIndex);
            if (layer) {
                isVisible = layer->isVisible();
                isPinned = layer->isPinned();
                isClipping = layer->isClippingMask();
            }
        } else {
            isVisible = (index.data(Qt::CheckStateRole).toInt() == Qt::Checked);
        }

        auto listWidget = qobject_cast<const LayerListWidget*>(option.widget);
        bool isEyeHovered = (listWidget && listWidget->hoverEyeRow() == index.row());
        bool isPinHovered = (listWidget && listWidget->hoverPinRow() == index.row());

        // 2. 上段: 目のアイコン描画、下段: ピン止めボタン描画 (SAI仕様)
        int slotW = 20;
        int slotH = 15;
        int gap = 2;
        int totalH = slotH * 2 + gap;
        int startY = option.rect.top() + (option.rect.height() - totalH) / 2;
        int slotX = option.rect.left() + 3;

        QRect eyeSlot(slotX, startY, slotW, slotH);
        QRect pinSlot(slotX, startY + slotH + gap, slotW, slotH);

        drawEyeIcon(painter, eyeSlot, isVisible, isEyeHovered, isSelected);
        drawPinIcon(painter, pinSlot, isPinned, isPinHovered, isSelected);

        // クリッピングインジケーター (SAI風の赤い縦ライン)
        if (isClipping) {
            QRect clipBar(option.rect.left() + 25, option.rect.top() + (option.rect.height() - 30) / 2, 3, 30);
            painter->fillRect(clipBar, QColor("#e03040"));
        }

        // 3. サムネイル描画 (x: left + 28, 幅: 36, 高さ: 30)
        QRect thumbRect(option.rect.left() + 28, option.rect.top() + (option.rect.height() - 30) / 2, 36, 30);
        QVariant iconVar = index.data(Qt::DecorationRole);
        if (iconVar.isValid()) {
            QIcon icon = qvariant_cast<QIcon>(iconVar);
            if (!icon.isNull()) {
                QPixmap pm = icon.pixmap(36, 30);
                painter->drawPixmap(thumbRect, pm);
            }
        }
        // サムネイル枠線
        painter->setPen(QPen(QColor(180, 180, 180), 1));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(thumbRect);

        // 4. テキスト描画 (x: left + 68)
        QRect textRect(option.rect.left() + 68, option.rect.top(), option.rect.width() - 72, option.rect.height());
        QString text = index.data(Qt::DisplayRole).toString();

        painter->setFont(option.font);
        if (isSelected) {
            QFont boldFont = option.font;
            boldFont.setBold(true);
            painter->setFont(boldFont);
            painter->setPen(QColor("#000000"));
        } else {
            painter->setPen(isVisible ? QColor("#000000") : QColor("#888888"));
        }

        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, text);

        painter->restore();
    }

    void drawEyeIcon(QPainter* painter, const QRect& slotRect, bool isVisible, bool isHovered, bool isSelected) const {
        Q_UNUSED(isSelected);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        // ボタン枠線と背景 (SAI風)
        if (isHovered) {
            painter->setPen(QPen(QColor(160, 195, 240), 1));
            painter->setBrush(QColor(232, 242, 255));
            painter->drawRoundedRect(slotRect, 2, 2);
        } else {
            painter->setPen(QPen(QColor(210, 210, 210), 1));
            painter->setBrush(QColor(250, 250, 250));
            painter->drawRoundedRect(slotRect, 2, 2);
        }

        float cx = slotRect.center().x() + 0.5f;
        float cy = slotRect.center().y() + 0.5f;
        float rx = 6.5f;
        float ry = 3.8f;

        if (isVisible) {
            // 白目 (強膜)
            QPainterPath eyePath;
            eyePath.moveTo(cx - rx, cy);
            eyePath.quadTo(cx, cy - ry, cx + rx, cy);
            eyePath.quadTo(cx, cy + ry, cx - rx, cy);

            painter->setPen(QPen(QColor(35, 45, 55), 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->setBrush(QColor(255, 255, 255));
            painter->drawPath(eyePath);

            // 虹彩 (深いブルーグレー)
            float irisRadius = 2.4f;
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(45, 65, 85));
            painter->drawEllipse(QPointF(cx, cy), irisRadius, irisRadius);

            // 瞳孔 (黒)
            float pupilRadius = 1.2f;
            painter->setBrush(QColor(15, 15, 15));
            painter->drawEllipse(QPointF(cx, cy), pupilRadius, pupilRadius);

            // 光のハイライト
            painter->setBrush(QColor(255, 255, 255));
            painter->drawEllipse(QPointF(cx + 0.7f, cy - 0.7f), 0.6f, 0.6f);
        } else {
            // 非表示時: 閉じ目・スラッシュ (visibility_off) アイコン
            QPainterPath eyePath;
            eyePath.moveTo(cx - rx, cy);
            eyePath.quadTo(cx, cy - ry, cx + rx, cy);
            eyePath.quadTo(cx, cy + ry, cx - rx, cy);

            painter->setPen(QPen(isHovered ? QColor(140, 140, 140) : QColor(185, 185, 185), 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(eyePath);

            painter->setPen(QPen(isHovered ? QColor(220, 60, 60) : QColor(190, 90, 90), 1.5, Qt::SolidLine, Qt::RoundCap));
            painter->drawLine(QPointF(cx - rx * 0.85f, cy - ry * 1.15f), QPointF(cx + rx * 0.85f, cy + ry * 1.15f));
        }

        painter->restore();
    }

    void drawPinIcon(QPainter* painter, const QRect& slotRect, bool isPinned, bool isHovered, bool isSelected) const {
        Q_UNUSED(isSelected);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        // ボタンの枠線と背景 (SAI風)
        if (isHovered) {
            painter->setPen(QPen(QColor(160, 195, 240), 1));
            painter->setBrush(QColor(232, 242, 255));
            painter->drawRoundedRect(slotRect, 2, 2);
        } else {
            painter->setPen(QPen(QColor(210, 210, 210), 1));
            painter->setBrush(QColor(250, 250, 250));
            painter->drawRoundedRect(slotRect, 2, 2);
        }

        float cx = slotRect.center().x() + 1.0f;
        float cy = slotRect.center().y() - 1.0f;

        if (isPinned) {
            // SAI風の赤い画鋲アイコン
            // 針 (左下に向かって伸びるシルバーの針)
            painter->setPen(QPen(QColor(130, 130, 140), 1.5, Qt::SolidLine, Qt::RoundCap));
            painter->drawLine(QPointF(cx - 1.5f, cy + 1.5f), QPointF(cx - 5.2f, cy + 5.2f));

            // 画鋲の頭 (立体感のある赤い球体)
            QRadialGradient grad(cx - 1.0f, cy - 1.0f, 3.8f);
            grad.setColorAt(0.0, QColor(255, 120, 120));
            grad.setColorAt(0.5, QColor(220, 30, 30));
            grad.setColorAt(1.0, QColor(160, 10, 10));

            painter->setPen(QPen(QColor(140, 10, 10), 0.8));
            painter->setBrush(grad);
            painter->drawEllipse(QPointF(cx, cy), 3.5f, 3.5f);

            // ハイライト点
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(255, 255, 255, 230));
            painter->drawEllipse(QPointF(cx - 1.0f, cy - 1.0f), 0.9f, 0.9f);
        } else if (isHovered) {
            // 未ピン留め時のホバー案内 (薄い画鋲のシルエット)
            painter->setPen(QPen(QColor(170, 170, 170), 1.2, Qt::SolidLine, Qt::RoundCap));
            painter->drawLine(QPointF(cx - 1.5f, cy + 1.5f), QPointF(cx - 4.8f, cy + 4.8f));

            painter->setPen(QPen(QColor(180, 180, 180), 0.8));
            painter->setBrush(QColor(240, 180, 180, 160));
            painter->drawEllipse(QPointF(cx, cy), 3.0f, 3.0f);
        }

        painter->restore();
    }

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& /*option*/, const QModelIndex& /*index*/) const override {
        auto lineEdit = new QLineEdit(parent);
        lineEdit->setLayoutDirection(Qt::LeftToRight);
        lineEdit->setStyleSheet(
            "QLineEdit { "
            "  background: #ffffff; "
            "  color: #000000; "
            "  border: 1px solid #0078d7; "
            "  border-radius: 2px; "
            "  padding: 1px 3px; "
            "  font-size: 11px; "
            "}"
        );
        return lineEdit;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override {
        auto lineEdit = qobject_cast<QLineEdit*>(editor);
        if (!lineEdit || !m_panel || !m_panel->document()) return;
        int row = index.row();
        int count = static_cast<int>(m_panel->document()->layers().size());
        if (row < 0 || row >= count) return;
        int layerIndex = count - 1 - row;
        auto layer = m_panel->document()->getLayer(layerIndex);
        if (layer) {
            lineEdit->setText(layer->name());
            lineEdit->selectAll();
        }
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override {
        auto lineEdit = qobject_cast<QLineEdit*>(editor);
        if (!lineEdit || !m_panel || !m_panel->document()) return;
        QString newName = lineEdit->text().trimmed();
        int row = index.row();
        int count = static_cast<int>(m_panel->document()->layers().size());
        if (row < 0 || row >= count) return;
        int layerIndex = count - 1 - row;
        auto layer = m_panel->document()->getLayer(layerIndex);
        if (layer) {
            if (!newName.isEmpty() && layer->name() != newName) {
                layer->setName(newName);
            }

            QString indent = "";
            auto p = layer->parentGroup();
            while (p) {
                indent = "  " + indent;
                p = p->parentGroup();
            }
            if (layer->parentGroup()) {
                indent += "└ ";
            }

            QString typePrefix = "[通常]";
            if (layer->type() == LayerType::Vector) typePrefix = "[ベクトル]";
            else if (layer->type() == LayerType::Adjustment) typePrefix = "[調整]";
            else if (layer->type() == LayerType::Text) typePrefix = "[テキスト]";
            else if (layer->type() == LayerType::Group) {
                auto grp = std::dynamic_pointer_cast<GroupLayer>(layer);
                QString foldIcon = (grp && grp->isExpanded()) ? "▼" : "▶";
                typePrefix = QString("[%1 グループ]").arg(foldIcon);
            }

            QString itemText = QString("%1%2 %3 (%4%)")
                .arg(indent, typePrefix, layer->name())
                .arg(static_cast<int>(layer->opacity() * 100));

            model->setData(index, itemText, Qt::DisplayRole);

            QTimer::singleShot(0, m_panel, [this]() {
                if (m_panel) {
                    m_panel->onLayerListChanged();
                }
            });
        }
    }

    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        if (!m_panel || !m_panel->document()) {
            editor->setGeometry(option.rect);
            return;
        }
        int row = index.row();
        int count = static_cast<int>(m_panel->document()->layers().size());
        if (row < 0 || row >= count) {
            editor->setGeometry(option.rect);
            return;
        }
        int layerIndex = count - 1 - row;
        auto layer = m_panel->document()->getLayer(layerIndex);
        if (!layer) {
            editor->setGeometry(option.rect);
            return;
        }

        QString indent = "";
        auto p = layer->parentGroup();
        while (p) {
            indent = "  " + indent;
            p = p->parentGroup();
        }
        if (layer->parentGroup()) {
            indent += "└ ";
        }

        QString typePrefix = "[通常]";
        if (layer->type() == LayerType::Vector) typePrefix = "[ベクトル]";
        else if (layer->type() == LayerType::Adjustment) typePrefix = "[調整]";
        else if (layer->type() == LayerType::Text) typePrefix = "[テキスト]";
        else if (layer->type() == LayerType::Group) {
            auto grp = std::dynamic_pointer_cast<GroupLayer>(layer);
            QString foldIcon = (grp && grp->isExpanded()) ? "▼" : "▶";
            typePrefix = QString("[%1 グループ]").arg(foldIcon);
        }

        int baseLeft = option.rect.left() + 68;

        QFontMetrics fm(option.font);
        int prefixWidth = fm.horizontalAdvance(indent + typePrefix + " ");
        int textStart = baseLeft + prefixWidth;
        int editWidth = option.rect.right() - textStart - 4;
        if (editWidth < 60) {
            textStart = baseLeft;
            editWidth = option.rect.right() - textStart - 4;
        }
        QRect editRect(textStart, option.rect.top() + 3, std::max(60, editWidth), option.rect.height() - 6);
        editor->setGeometry(editRect);
    }

private:
    LayerPanelWidget* m_panel;
};

LayerListWidget::LayerListWidget(QWidget* parent)
    : QListWidget(parent) {
    setMouseTracking(true);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDefaultDropAction(Qt::MoveAction);
    setSelectionMode(QAbstractItemView::SingleSelection);
}

void LayerListWidget::setScrollBarPosition(bool onLeft) {
    if (onLeft) {
        setLayoutDirection(Qt::RightToLeft);
    } else {
        setLayoutDirection(Qt::LeftToRight);
    }
    if (viewport()) {
        viewport()->setLayoutDirection(Qt::LeftToRight);
    }
}

bool LayerListWidget::isScrollBarOnLeft() const {
    return layoutDirection() == Qt::RightToLeft;
}

int LayerListWidget::getBackgroundRow() const {
        if (!m_panel || !m_panel->document()) return count() - 1;
        const auto& layers = m_panel->document()->layers();
        int c = static_cast<int>(layers.size());
        for (int r = 0; r < c; ++r) {
            int docIdx = c - 1 - r;
            if (docIdx >= 0 && docIdx < c && layers[docIdx]->name() == "背景") {
                return r;
            }
        }
        return count() - 1;
    }

void LayerListWidget::startDrag(Qt::DropActions supportedActions) {
    Q_UNUSED(supportedActions);
    m_dragStartRow = currentRow();
    if (m_dragStartRow < 0 && !selectedIndexes().isEmpty()) {
        m_dragStartRow = selectedIndexes().first().row();
    }
    if (m_dragStartRow < 0) return;

    if (m_panel && m_panel->document()) {
        int count = static_cast<int>(m_panel->document()->layers().size());
        int docIdx = count - 1 - m_dragStartRow;
        auto layer = m_panel->document()->getLayer(docIdx);
        if (layer && layer->name() == "背景") {
            m_dragStartRow = -1;
            return; // 背景レイヤーはドラッグ不可
        }
    }

    auto item = this->item(m_dragStartRow);
    if (!item) return;

    auto drag = new QDrag(this);
    auto mimeData = new QMimeData();
    mimeData->setData("application/x-kaburagi-layer", QByteArray::number(m_dragStartRow));
    drag->setMimeData(mimeData);

    QRect rect = visualItemRect(item);
    QPixmap pixmap(rect.size());
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setOpacity(0.85);
    render(&painter, QPoint(0, 0), QRegion(rect));
    painter.end();

    if (!pixmap.isNull()) {
        drag->setPixmap(pixmap);
        drag->setHotSpot(QPoint(24, rect.height() / 2));
    }

    drag->exec(Qt::MoveAction);
    m_dragStartRow = -1;
    m_dropSlot = -1;
    viewport()->update();
}

void LayerListWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->source() == this && event->mimeData()->hasFormat("application/x-kaburagi-layer")) {
        event->setDropAction(Qt::MoveAction);
        event->accept();
    } else {
        event->ignore();
    }
}

void LayerListWidget::dragMoveEvent(QDragMoveEvent* event) {
    if (event->source() == this && event->mimeData()->hasFormat("application/x-kaburagi-layer")) {
        event->setDropAction(Qt::MoveAction);
        event->accept();

        QPoint pos = event->position().toPoint();
        QListWidgetItem* targetItem = itemAt(pos);
        int slot = count();
        if (targetItem) {
            int targetRow = row(targetItem);
            QRect rect = visualItemRect(targetItem);
            if (pos.y() < rect.center().y()) {
                slot = targetRow;
            } else {
                slot = targetRow + 1;
            }
        }

        // 背景レイヤーより下には配置できないため背景の直上にクランプ
        int bgRow = getBackgroundRow();
        if (slot > bgRow) {
            slot = bgRow;
        }

        if (m_dropSlot != slot) {
            m_dropSlot = slot;
            viewport()->update();
        }
    } else {
        event->ignore();
    }
}

void LayerListWidget::dragLeaveEvent(QDragLeaveEvent* event) {
    Q_UNUSED(event);
    m_dropSlot = -1;
    viewport()->update();
}

void LayerListWidget::dropEvent(QDropEvent* event) {
    if (event->source() != this || m_dragStartRow < 0) {
        event->ignore();
        return;
    }

    int fromRow = m_dragStartRow;
    int slot = m_dropSlot;
    m_dragStartRow = -1;
    m_dropSlot = -1;
    viewport()->update();

    if (slot < 0) {
        QPoint pos = event->position().toPoint();
        QListWidgetItem* targetItem = itemAt(pos);
        slot = count();
        if (targetItem) {
            int targetRow = row(targetItem);
            QRect rect = visualItemRect(targetItem);
            if (pos.y() < rect.center().y()) {
                slot = targetRow;
            } else {
                slot = targetRow + 1;
            }
        }
    }

    // 背景レイヤーより下には配置できないため背景の直上にクランプ
    int bgRow = getBackgroundRow();
    if (slot > bgRow) {
        slot = bgRow;
    }

    int newRow = slot;
    if (fromRow < slot) {
        newRow = slot - 1;
    }
    if (newRow >= bgRow) {
        newRow = bgRow - 1;
    }

    event->setDropAction(Qt::MoveAction);
    event->accept();

    if (newRow == fromRow || fromRow < 0 || fromRow >= count() || newRow < 0 || newRow >= count()) {
        return;
    }

    QTimer::singleShot(0, this, [this, fromRow, newRow]() {
        emit layerReordered(fromRow, newRow);
    });
}

void LayerListWidget::paintEvent(QPaintEvent* event) {
    QListWidget::paintEvent(event);

    if (m_dropSlot >= 0 && m_dropSlot <= count()) {
        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing, true);
        QColor indicatorColor(0, 120, 215); // Windows / Modern UI blue
        painter.setPen(QPen(indicatorColor, 2));

        int y = 0;
        if (m_dropSlot < count()) {
            auto item = this->item(m_dropSlot);
            if (item) {
                y = visualItemRect(item).top();
            }
        } else if (count() > 0) {
            auto item = this->item(count() - 1);
            if (item) {
                y = visualItemRect(item).bottom() + 1;
            }
        }

        painter.drawLine(4, y, viewport()->width() - 6, y);

        // 両端にアクセント円を描画
        painter.setBrush(indicatorColor);
        painter.drawEllipse(QPoint(5, y), 3, 3);
        painter.drawEllipse(QPoint(viewport()->width() - 7, y), 3, 3);
    }
}

void LayerListWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        QPoint pos = event->pos();
        QListWidgetItem* item = itemAt(pos);
        if (item) {
            QRect r = visualItemRect(item);
            // 左端ボタン領域 (左端 0 ~ 25px) のクリック判定
            if (pos.x() >= r.left() && pos.x() < r.left() + 25) {
                int rowIdx = row(item);
                if (m_panel && m_panel->document()) {
                    int count = static_cast<int>(m_panel->document()->layers().size());
                    if (rowIdx >= 0 && rowIdx < count) {
                        int layerIndex = count - 1 - rowIdx;
                        auto layer = m_panel->document()->getLayer(layerIndex);
                        if (layer) {
                            int midY = r.top() + r.height() / 2;
                            if (pos.y() < midY) {
                                // 上段: 目のアイコン (表示/非表示切り替え)
                                bool newVisible = !layer->isVisible();
                                layer->setVisible(newVisible);
                                item->setCheckState(newVisible ? Qt::Checked : Qt::Unchecked);
                                m_panel->document()->composite();
                            } else {
                                // 下段: ピン留めボタン (ピン留めON/OFF切り替え)
                                layer->setPinned(!layer->isPinned());
                            }
                            viewport()->update();
                        }
                    }
                }
                return; // 選択変更やドラッグ開始を行わず早期リターン
            }

            // "[▼ グループ]" / "[▶ グループ]" 領域のクリック判定 (グループの展開/格納)
            int rowIdx = row(item);
            if (m_panel && m_panel->document()) {
                int count = static_cast<int>(m_panel->document()->layers().size());
                if (rowIdx >= 0 && rowIdx < count) {
                    int layerIndex = count - 1 - rowIdx;
                    auto layer = m_panel->document()->getLayer(layerIndex);
                    if (layer && layer->type() == LayerType::Group) {
                        auto grp = std::dynamic_pointer_cast<GroupLayer>(layer);
                        if (grp) {
                            QString indent = "";
                            auto p = layer->parentGroup();
                            while (p) {
                                indent = "  " + indent;
                                p = p->parentGroup();
                            }
                            if (layer->parentGroup()) {
                                indent += "└ ";
                            }
                            QString foldIcon = grp->isExpanded() ? "▼" : "▶";
                            QString typePrefix = QString("[%1 グループ]").arg(foldIcon);

                            QFont font = this->font();
                            if (item->isSelected()) {
                                font.setBold(true);
                            }
                            QFontMetrics fm(font);
                            int baseLeft = r.left() + 68;
                            int indentWidth = fm.horizontalAdvance(indent);
                            int prefixWidth = fm.horizontalAdvance(typePrefix);
                            int prefixStartX = baseLeft + indentWidth;
                            int prefixEndX = prefixStartX + prefixWidth;

                            // 多少の押しやすさを考慮してマージン±4px
                            if (pos.x() >= prefixStartX - 4 && pos.x() <= prefixEndX + 4) {
                                bool newExpanded = !grp->isExpanded();
                                grp->setExpanded(newExpanded);
                                m_panel->document()->setActiveLayerIndex(layerIndex);
                                m_panel->onLayerListChanged();
                                return; // 選択変更やドラッグ開始を行わず早期リターン
                            }
                        }
                    }
                }
            }
        }
    }
    QListWidget::mousePressEvent(event);
}

void LayerListWidget::mouseMoveEvent(QMouseEvent* event) {
    QPoint pos = event->pos();
    QListWidgetItem* item = itemAt(pos);
    int hoveredEye = -1;
    int hoveredPin = -1;
    bool isFoldHovered = false;

    if (item) {
        QRect r = visualItemRect(item);
        if (pos.x() >= r.left() && pos.x() < r.left() + 25) {
            int midY = r.top() + r.height() / 2;
            if (pos.y() < midY) {
                hoveredEye = row(item);
            } else {
                hoveredPin = row(item);
            }
            setCursor(Qt::PointingHandCursor);
        } else {
            // "[▼ グループ]" / "[▶ グループ]" へのホバー判定
            int rowIdx = row(item);
            if (m_panel && m_panel->document()) {
                int count = static_cast<int>(m_panel->document()->layers().size());
                if (rowIdx >= 0 && rowIdx < count) {
                    int layerIndex = count - 1 - rowIdx;
                    auto layer = m_panel->document()->getLayer(layerIndex);
                    if (layer && layer->type() == LayerType::Group) {
                        auto grp = std::dynamic_pointer_cast<GroupLayer>(layer);
                        if (grp) {
                            QString indent = "";
                            auto p = layer->parentGroup();
                            while (p) {
                                indent = "  " + indent;
                                p = p->parentGroup();
                            }
                            if (layer->parentGroup()) {
                                indent += "└ ";
                            }
                            QString foldIcon = grp->isExpanded() ? "▼" : "▶";
                            QString typePrefix = QString("[%1 グループ]").arg(foldIcon);

                            QFont font = this->font();
                            if (item->isSelected()) {
                                font.setBold(true);
                            }
                            QFontMetrics fm(font);
                            int baseLeft = r.left() + 68;
                            int indentWidth = fm.horizontalAdvance(indent);
                            int prefixWidth = fm.horizontalAdvance(typePrefix);
                            int prefixStartX = baseLeft + indentWidth;
                            int prefixEndX = prefixStartX + prefixWidth;

                            if (pos.x() >= prefixStartX - 4 && pos.x() <= prefixEndX + 4) {
                                isFoldHovered = true;
                                setCursor(Qt::PointingHandCursor);
                            }
                        }
                    }
                }
            }
            if (!isFoldHovered) {
                unsetCursor();
            }
        }
    } else {
        unsetCursor();
    }

    if (m_hoverEyeRow != hoveredEye || m_hoverPinRow != hoveredPin) {
        m_hoverEyeRow = hoveredEye;
        m_hoverPinRow = hoveredPin;
        viewport()->update();
    }

    QListWidget::mouseMoveEvent(event);
}

void LayerListWidget::leaveEvent(QEvent* event) {
    QListWidget::leaveEvent(event);
    if (m_hoverEyeRow != -1 || m_hoverPinRow != -1) {
        m_hoverEyeRow = -1;
        m_hoverPinRow = -1;
        unsetCursor();
        viewport()->update();
    }
}

void LayerListWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        auto item = itemAt(event->pos());
        if (item) {
            QRect r = visualItemRect(item);
            // 目・ピン留めボタン領域でのダブルクリックは無視
            if (event->pos().x() < r.left() + 25) {
                return;
            }
            if (m_panel && m_panel->document()) {
                int rowIdx = row(item);
                int count = static_cast<int>(m_panel->document()->layers().size());
                if (rowIdx >= 0 && rowIdx < count) {
                    int layerIndex = count - 1 - rowIdx;
                    auto layer = m_panel->document()->getLayer(layerIndex);
                    if (layer && layer->type() == LayerType::Group) {
                        auto grp = std::dynamic_pointer_cast<GroupLayer>(layer);
                        if (grp) {
                            QString indent = "";
                            auto p = layer->parentGroup();
                            while (p) {
                                indent = "  " + indent;
                                p = p->parentGroup();
                            }
                            if (layer->parentGroup()) {
                                indent += "└ ";
                            }
                            QString foldIcon = grp->isExpanded() ? "▼" : "▶";
                            QString typePrefix = QString("[%1 グループ]").arg(foldIcon);

                            QFont font = this->font();
                            if (item->isSelected()) font.setBold(true);
                            QFontMetrics fm(font);
                            int baseLeft = r.left() + 68;
                            int indentWidth = fm.horizontalAdvance(indent);
                            int prefixWidth = fm.horizontalAdvance(typePrefix);
                            int prefixStartX = baseLeft + indentWidth;
                            int prefixEndX = prefixStartX + prefixWidth;

                            // "[▼ グループ]" 領域のダブルクリックはシングルクリックで処理済みのため無視（名前編集開始を防ぐ）
                            if (event->pos().x() >= prefixStartX - 4 && event->pos().x() <= prefixEndX + 4) {
                                return;
                            }
                            // アイコン領域 (r.left() + 28 以上 r.left() + 68 未満) でダブルクリックした場合は展開/折りたたみ
                            if (event->pos().x() < r.left() + 68) {
                                grp->setExpanded(!grp->isExpanded());
                                m_panel->onLayerListChanged();
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
    QListWidget::mouseDoubleClickEvent(event);
}

LayerPanelWidget::LayerPanelWidget(QWidget* parent)
    : QWidget(parent) {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(4);

    // 1. レイヤープロパティ (合成モード & 不透明度)
    auto propLayout = new QVBoxLayout();
    propLayout->setSpacing(2);

    auto modeLayout = new QHBoxLayout();
    auto modeLabel = new QLabel("合成モード:", this);
    modeLabel->setStyleSheet("font-size: 11px;");
    m_blendCombo = new QComboBox(this);
    for (const auto& info : getAllBlendModes()) {
        m_blendCombo->addItem(QString("%1 (%2)").arg(info.nameJa, info.nameEn), static_cast<int>(info.mode));
    }
    modeLayout->addWidget(modeLabel);
    modeLayout->addWidget(m_blendCombo, 1);
    propLayout->addLayout(modeLayout);

    m_opacityScale = new SpinScale("不透明度", 0.0, 100.0, 1.0, 0, "%", this);
    m_opacityScale->setValue(100.0);
    propLayout->addWidget(m_opacityScale);

    // チェックボックス (透明度保護・下レイヤーでクリッピング)
    auto checkLayout = new QHBoxLayout();
    m_alphaLockCheck = new QCheckBox("透明度保護", this);
    m_alphaLockCheck->setStyleSheet("font-size: 11px;");
    m_clipCheck = new QCheckBox("下のレイヤーでクリッピング", this);
    m_clipCheck->setStyleSheet("font-size: 11px;");
    checkLayout->addWidget(m_alphaLockCheck);
    checkLayout->addWidget(m_clipCheck);
    propLayout->addLayout(checkLayout);

    mainLayout->addLayout(propLayout);

    // 2. レイヤー操作ツールボタン
    auto btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(2);
    m_btnNewRaster = new QPushButton("新規", this);
    m_btnNewRaster->setToolTip("新規ラスターレイヤー");
    m_btnNewRaster->setStyleSheet("font-size: 10px; padding: 2px;");

    m_btnNewVector = new QPushButton("ベクトル", this);
    m_btnNewVector->setToolTip("新規ベクトルレイヤー");
    m_btnNewVector->setStyleSheet("font-size: 10px; padding: 2px;");

    m_btnNewGroup = new QPushButton("グループ", this);
    m_btnNewGroup->setToolTip("新規レイヤーグループ");
    m_btnNewGroup->setStyleSheet("font-size: 10px; padding: 2px;");

    m_btnNewPanel = new QPushButton(QString::fromUtf8("コマ"), this);
    m_btnNewPanel->setToolTip(QString::fromUtf8("新規コマフォルダ"));
    m_btnNewPanel->setStyleSheet("font-size: 10px; padding: 2px;");

    m_btnNewThreeD = new QPushButton(QString::fromUtf8("3D"), this);
    m_btnNewThreeD->setToolTip(QString::fromUtf8("新規3Dレイヤー"));
    m_btnNewThreeD->setStyleSheet("font-size: 10px; padding: 2px;");

    m_btnNewAdj = new QPushButton("調整", this);
    m_btnNewAdj->setToolTip("新規Photoshop風調整レイヤー");
    m_btnNewAdj->setStyleSheet("font-size: 10px; padding: 2px;");

    m_btnMergeDown = new QPushButton(QString::fromUtf8("結合"), this);
    m_btnMergeDown->setToolTip(QString::fromUtf8("下のレイヤーと結合"));
    m_btnMergeDown->setStyleSheet("font-size: 10px; padding: 2px;");

    m_btnDelete = new QPushButton("削除", this);
    m_btnDelete->setToolTip("選択レイヤー削除");
    m_btnDelete->setStyleSheet("font-size: 10px; padding: 2px;");

    btnLayout->addWidget(m_btnNewRaster);
    btnLayout->addWidget(m_btnNewVector);
    btnLayout->addWidget(m_btnNewGroup);
    btnLayout->addWidget(m_btnNewPanel);
    btnLayout->addWidget(m_btnNewThreeD);
    btnLayout->addWidget(m_btnNewAdj);
    btnLayout->addWidget(m_btnMergeDown);
    btnLayout->addWidget(m_btnDelete);
    mainLayout->addLayout(btnLayout);

    // 3. レイヤー一覧リスト
    m_layerListWidget = new LayerListWidget(this);
    m_layerListWidget->setPanel(this);
    m_layerListWidget->setIconSize(QSize(36, 28));
    m_layerListWidget->setStyleSheet(
        "QListWidget { background: #ffffff; border: 1px solid #b0b0b0; font-size: 11px; outline: none; }"
        "QListWidget::item { padding: 0px; border-bottom: 1px solid #e8e8e8; }"
        "QListWidget::item:selected { background: #b0c4de; color: #000000; font-weight: bold; }"
    );
    m_layerListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_layerListWidget->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_layerListWidget->setItemDelegate(new LayerItemDelegate(this, m_layerListWidget));
    connect(m_layerListWidget, &LayerListWidget::layerReordered, this, &LayerPanelWidget::onLayerReordered);
    updateScrollBarPosition();
    mainLayout->addWidget(m_layerListWidget, 1);

    // サムネイル更新デバウンスタイマー (120ms)
    m_thumbnailUpdateTimer = new QTimer(this);
    m_thumbnailUpdateTimer->setSingleShot(true);
    connect(m_thumbnailUpdateTimer, &QTimer::timeout, this, &LayerPanelWidget::updateThumbnails);

    // シグナル接続
    connect(m_blendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LayerPanelWidget::onBlendModeChanged);
    connect(m_opacityScale, &SpinScale::valueChanged, this, &LayerPanelWidget::onOpacityScaleChanged);
    connect(m_clipCheck, &QCheckBox::toggled, this, &LayerPanelWidget::onClippingToggled);
    connect(m_alphaLockCheck, &QCheckBox::toggled, this, &LayerPanelWidget::onAlphaLockToggled);

    connect(m_btnNewRaster, &QPushButton::clicked, this, &LayerPanelWidget::onAddRasterLayer);
    connect(m_btnNewVector, &QPushButton::clicked, this, &LayerPanelWidget::onAddVectorLayer);
    connect(m_btnNewGroup, &QPushButton::clicked, this, &LayerPanelWidget::onAddGroupLayer);
    connect(m_btnNewPanel, &QPushButton::clicked, this, &LayerPanelWidget::onAddPanelFolder);
    connect(m_btnNewThreeD, &QPushButton::clicked, this, &LayerPanelWidget::onAddThreeDLayer);
    connect(m_btnNewAdj, &QPushButton::clicked, this, &LayerPanelWidget::onAddAdjustmentLayer);
    connect(m_btnMergeDown, &QPushButton::clicked, this, &LayerPanelWidget::onMergeDownLayer);
    connect(m_btnDelete, &QPushButton::clicked, this, &LayerPanelWidget::onDeleteLayer);

    connect(m_layerListWidget, &QListWidget::currentRowChanged, this, [this](int row) {
        if (!m_document) return;
        // リストは上から下（上層から下層）で表示するため反転
        int count = static_cast<int>(m_document->layers().size());
        if (row >= 0 && row < count) {
            int layerIndex = count - 1 - row;
            m_document->setActiveLayerIndex(layerIndex);
        }
    });

    // チェックボックス (表示/非表示) の切り替え
    connect(m_layerListWidget, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        if (!m_document || !item) return;
        int row = m_layerListWidget->row(item);
        int count = static_cast<int>(m_document->layers().size());
        if (row < 0 || row >= count) return;
        int layerIndex = count - 1 - row;
        auto layer = m_document->getLayer(layerIndex);
        if (layer) {
            bool visible = (item->checkState() == Qt::Checked);
            if (layer->isVisible() != visible) {
                layer->setVisible(visible);
                m_document->composite();
            }
        }
    });

    // 右クリックコンテキストメニュー (グループ所属設定など)
    connect(m_layerListWidget, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        auto menu = createContextMenu(pos);
        if (menu) {
            menu->exec(m_layerListWidget->viewport()->mapToGlobal(pos));
            delete menu;
        }
    });
}

QMenu* LayerPanelWidget::createContextMenu(const QPoint& pos) {
    if (!m_document || !m_layerListWidget) return nullptr;
    QListWidgetItem* item = m_layerListWidget->itemAt(pos);
    if (!item) return nullptr;

    int row = m_layerListWidget->row(item);
    int count = static_cast<int>(m_document->layers().size());
    int layerIndex = count - 1 - row;
    auto layer = m_document->getLayer(layerIndex);
    if (!layer) return nullptr;

    // 右クリックされたアイテムを選択状態にする
    m_layerListWidget->setCurrentItem(item);
    m_document->setActiveLayerIndex(layerIndex);

    auto menu = new QMenu(this);

    // 新規作成
    menu->addAction(QString::fromUtf8("新規通常レイヤー"), this, &LayerPanelWidget::onAddRasterLayer);
    menu->addAction(QString::fromUtf8("新規ベクトルレイヤー"), this, &LayerPanelWidget::onAddVectorLayer);
    menu->addAction(QString::fromUtf8("新規テキストレイヤー"), this, &LayerPanelWidget::onAddTextLayer);
    menu->addAction(QString::fromUtf8("新規レイヤーグループ"), this, &LayerPanelWidget::onAddGroupLayer);
    menu->addAction(QString::fromUtf8("新規コマフォルダ"), this, &LayerPanelWidget::onAddPanelFolder);
    menu->addAction(QString::fromUtf8("新規3Dレイヤー"), this, &LayerPanelWidget::onAddThreeDLayer);

    menu->addSeparator();

    // 複製・結合・削除
    menu->addAction("レイヤーを複製", this, [this, layerIndex]() {
        if (m_document) m_document->duplicateLayer(layerIndex);
    });

    auto actMergeDown = menu->addAction("下のレイヤーと結合", this, [this, layerIndex]() {
        if (m_document && layerIndex > 0) m_document->mergeDown(layerIndex);
    });
    actMergeDown->setEnabled(layerIndex > 0);

    auto grp = std::dynamic_pointer_cast<GroupLayer>(layer);
    if (grp && grp->isPanelFolder()) {
        menu->addAction("隣接するコマ枠を結合...", this, [this, grp]() {
            if (!m_document) return;
            auto candidates = m_document->findAdjacentPanels(grp);
            if (candidates.empty()) {
                QMessageBox::information(this, "コマ枠の結合", "隣接するコマ枠が見つかりませんでした。");
                return;
            }
            MergePanelDialog dlg(grp, candidates, this);
            if (dlg.exec() == QDialog::Accepted) {
                auto target = dlg.selectedPanel();
                if (target) {
                    m_document->mergePanelFolders(grp, target);
                }
            }
        });
    }

    auto actDelete = menu->addAction("レイヤーを削除", this, [this, layerIndex, layer]() {
        if (m_document && m_document->layers().size() > 1 && layer->name() != "背景") {
            m_document->removeLayer(layerIndex);
        }
    });
    actDelete->setEnabled(count > 1 && layer->name() != "背景");

    menu->addSeparator();

    // 選択範囲操作
    menu->addAction("不透明部分を選択範囲にする", this, [this, layerIndex]() {
        if (!m_document) return;
        QPainterPath opaquePath = m_document->createOpaquePath(layerIndex);
        QPainterPath oldSel = m_document->selection();
        if (m_document->undoStack()) {
            auto cmd = new SelectCommand(m_document, oldSel, opaquePath);
            cmd->setText("不透明部分を選択範囲にする");
            m_document->undoStack()->push(cmd);
        } else {
            m_document->setSelection(opaquePath);
        }
    });

    menu->addAction("不透明部分を選択範囲に加える", this, [this, layerIndex]() {
        if (!m_document) return;
        QPainterPath opaquePath = m_document->createOpaquePath(layerIndex);
        if (opaquePath.isEmpty()) return;
        QPainterPath oldSel = m_document->selection();
        QPainterPath newSel = oldSel.united(opaquePath);
        if (m_document->undoStack()) {
            auto cmd = new SelectCommand(m_document, oldSel, newSel);
            cmd->setText("不透明部分を選択範囲に加える");
            m_document->undoStack()->push(cmd);
        } else {
            m_document->setSelection(newSel);
        }
    });

    menu->addSeparator();

    if (layer->name() == "背景") {
        menu->addAction("第1背景色の変更", this, [this]() {
            if (!m_document) return;
            QColor cur = m_document->primaryBackgroundColor();
            QColor c = QColorDialog::getColor(cur, this, "第1背景色の変更", QColorDialog::ShowAlphaChannel | QColorDialog::DontUseNativeDialog);
            if (c.isValid()) {
                m_document->setPrimaryBackgroundColor(c);
            }
        });
        menu->addAction("第2背景色の変更", this, [this]() {
            if (!m_document) return;
            QColor cur = m_document->secondaryBackgroundColor();
            QColor c = QColorDialog::getColor(cur, this, "第2背景色の変更", QColorDialog::ShowAlphaChannel | QColorDialog::DontUseNativeDialog);
            if (c.isValid()) {
                m_document->setSecondaryBackgroundColor(c);
            }
        });
        menu->addSeparator();
        menu->addAction("第1背景色⇔第2背景色 切り替え", this, [this]() {
            if (m_document) {
                m_document->switchBackgroundColor();
            }
        });
        menu->addSeparator();
    }

    menu->addAction("名前の変更", [this, item]() {
        m_layerListWidget->editItem(item);
    });

    if (layer->hasAiParameters()) {
        menu->addSeparator();
        menu->addAction(tr("AI生成パラメータを呼び出し"), this, [this, layer]() {
            emit loadAiParametersRequested(layer);
        });
    }

    menu->addSeparator();

    if (layer->type() == LayerType::Group) {
        auto grp = std::dynamic_pointer_cast<GroupLayer>(layer);
        if (grp) {
            QString foldText = grp->isExpanded() ? "グループを折りたたむ" : "グループを展開する";
            menu->addAction(foldText, [this, grp]() {
                grp->setExpanded(!grp->isExpanded());
                onLayerListChanged();
            });
        }
    }

    auto groupMenu = menu->addMenu("グループへ移動");
    bool hasTargetGroups = false;
    for (const auto& l : m_document->layers()) {
        if (l->type() == LayerType::Group && l != layer) {
            auto g = std::dynamic_pointer_cast<GroupLayer>(l);
            // layer が g の先祖でないかチェック (循環防止)
            bool isAncestor = false;
            auto cur = g;
            while (cur) {
                if (cur == layer) {
                    isAncestor = true;
                    break;
                }
                cur = cur->parentGroup();
            }
            if (!isAncestor) {
                hasTargetGroups = true;
                QString actTitle = g->name();
                if (layer->parentGroup() == g) actTitle += " (所属中)";
                groupMenu->addAction(actTitle, [this, layerIndex, g]() {
                    m_document->setLayerParentGroup(layerIndex, g);
                });
            }
        }
    }
    if (!hasTargetGroups) {
        auto act = groupMenu->addAction("(移動可能なグループなし)");
        act->setEnabled(false);
    }
    if (layer->parentGroup()) {
        menu->addAction("グループから解除", [this, layerIndex]() {
            m_document->setLayerParentGroup(layerIndex, nullptr);
        });
    }

    return menu;
}

void LayerPanelWidget::setDocument(Document* doc) {
    if (m_document == doc) return;
    m_document = doc;
    if (m_document) {
        connect(m_document, &Document::layerListChanged, this, &LayerPanelWidget::onLayerListChanged);
        connect(m_document, &Document::activeLayerChanged, this, &LayerPanelWidget::onActiveLayerChanged);
        connect(m_document, &Document::documentUpdated, this, [this](const QRect&) {
            if (m_thumbnailUpdateTimer) {
                m_thumbnailUpdateTimer->start(120);
            }
        });
        onLayerListChanged();
        onActiveLayerChanged(m_document->activeLayerIndex());
    }
}

void LayerPanelWidget::onLayerReordered(int fromRow, int toRow) {
    if (!m_document) return;
    int count = static_cast<int>(m_document->layers().size());
    if (fromRow < 0 || fromRow >= count || toRow < 0 || toRow >= count) return;

    int fromDocIndex = count - 1 - fromRow;
    int toDocIndex = count - 1 - toRow;

    auto layer = m_document->getLayer(fromDocIndex);
    if (!layer) return;

    auto oldParent = layer->parentGroup();

    m_document->moveLayer(fromDocIndex, toDocIndex);

    // グループ所属の自動判定 (非グループ通常レイヤーの場合)
    if (layer->type() != LayerType::Group) {
        std::shared_ptr<GroupLayer> newParent = oldParent;
        // toDocIndex の上層(ビジュアル上は直上)は toDocIndex + 1、下層(ビジュアル上は直下)は toDocIndex - 1
        if (toDocIndex + 1 < count) {
            auto upper = m_document->getLayer(toDocIndex + 1);
            if (upper && upper->type() == LayerType::Group) {
                auto grp = std::dynamic_pointer_cast<GroupLayer>(upper);
                if (grp && grp->isExpanded()) {
                    newParent = grp;
                }
            }
        }
        if (newParent == oldParent) {
            auto pAbove = (toDocIndex + 1 < count) ? m_document->getLayer(toDocIndex + 1)->parentGroup() : nullptr;
            auto pBelow = (toDocIndex - 1 >= 0) ? m_document->getLayer(toDocIndex - 1)->parentGroup() : nullptr;
            if (pAbove == pBelow) {
                newParent = pAbove;
            }
        }
        if (newParent != oldParent) {
            m_document->setLayerParentGroup(toDocIndex, newParent);
        }
    }
}

void LayerPanelWidget::onLayerListChanged() {
    if (!m_document) return;
    m_layerListWidget->blockSignals(true);
    m_layerListWidget->clear();

    const auto& layers = m_document->layers();
    // SAI風に上位レイヤーを上に表示
    for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
        const auto& layer = *it;
        QString typePrefix = "[通常]";
        if (layer->type() == LayerType::Vector) typePrefix = "[ベクトル]";
        else if (layer->type() == LayerType::Adjustment) typePrefix = "[調整]";
        else if (layer->type() == LayerType::Text) typePrefix = "[テキスト]";
        else if (layer->type() == LayerType::Group) {
            auto grp = std::dynamic_pointer_cast<GroupLayer>(layer);
            QString foldIcon = (grp && grp->isExpanded()) ? "▼" : "▶";
            typePrefix = QString("[%1 グループ]").arg(foldIcon);
        }

        // 多階層グループの深さに応じたインデント
        QString indent = "";
        auto p = layer->parentGroup();
        while (p) {
            indent = "  " + indent;
            p = p->parentGroup();
        }
        if (layer->parentGroup()) {
            indent += "└ ";
        }

        QString itemText = QString("%1%2 %3 (%4%)")
            .arg(indent, typePrefix, layer->name())
            .arg(static_cast<int>(layer->opacity() * 100));

        auto item = new QListWidgetItem();
        auto flags = item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable;
        if (layer->name() != "背景") {
            flags |= Qt::ItemIsDragEnabled;
        }
        item->setFlags(flags);
        item->setCheckState(layer->isVisible() ? Qt::Checked : Qt::Unchecked);
        item->setIcon(QIcon(QPixmap::fromImage(layer->createThumbnail(36, 28))));
        item->setText(itemText);
        item->setSizeHint(QSize(180, 38));

        m_layerListWidget->addItem(item);
    }

    int activeRow = static_cast<int>(layers.size()) - 1 - m_document->activeLayerIndex();

    // グループの展開/格納 (isExpanded) に応じて子レイヤーの表示/非表示を設定
    for (int r = 0; r < static_cast<int>(layers.size()); ++r) {
        int docIndex = static_cast<int>(layers.size()) - 1 - r;
        auto layer = layers[docIndex];
        bool isHidden = false;
        auto p = layer ? layer->parentGroup() : nullptr;
        while (p) {
            if (!p->isExpanded()) {
                isHidden = true;
                break;
            }
            p = p->parentGroup();
        }
        m_layerListWidget->setRowHidden(r, isHidden);
    }

    // もしアクティブレイヤーが格納されたグループ内にある場合、可視な一番外側の親グループを選択状態にする
    if (activeRow >= 0 && activeRow < m_layerListWidget->count() && m_layerListWidget->isRowHidden(activeRow)) {
        auto layer = m_document->getLayer(m_document->activeLayerIndex());
        auto p = layer ? layer->parentGroup() : nullptr;
        std::shared_ptr<GroupLayer> visibleAncestor = nullptr;
        while (p) {
            if (!p->isExpanded()) {
                visibleAncestor = p;
            }
            p = p->parentGroup();
        }
        if (visibleAncestor) {
            for (int i = 0; i < static_cast<int>(layers.size()); ++i) {
                if (layers[i] == visibleAncestor) {
                    m_document->setActiveLayerIndex(i);
                    activeRow = static_cast<int>(layers.size()) - 1 - i;
                    break;
                }
            }
        }
    }

    m_layerListWidget->setCurrentRow(activeRow);
    m_layerListWidget->blockSignals(false);

    updateUIFromActiveLayer();
}

void LayerPanelWidget::updateThumbnails() {
    if (!m_document) return;
    int count = static_cast<int>(m_document->layers().size());
    if (m_layerListWidget->count() != count) return;

    for (int row = 0; row < count; ++row) {
        int layerIndex = count - 1 - row;
        auto layer = m_document->getLayer(layerIndex);
        if (layer) {
            auto item = m_layerListWidget->item(row);
            if (item) {
                item->setIcon(QIcon(QPixmap::fromImage(layer->createThumbnail(36, 28))));
            }
        }
    }
}

void LayerPanelWidget::scrollToLayer(int index, bool center) {
    if (!m_document || !m_layerListWidget) return;
    int count = static_cast<int>(m_document->layers().size());
    int targetRow = count - 1 - index;
    if (targetRow >= 0 && targetRow < m_layerListWidget->count()) {
        auto item = m_layerListWidget->item(targetRow);
        if (item) {
            auto hint = center ? QAbstractItemView::PositionAtCenter : QAbstractItemView::EnsureVisible;
            m_layerListWidget->scrollToItem(item, hint);
            QTimer::singleShot(0, this, [this, item, hint]() {
                if (m_layerListWidget && item) {
                    m_layerListWidget->scrollToItem(item, hint);
                }
            });
        }
    }
}

void LayerPanelWidget::onActiveLayerChanged(int index) {
    if (!m_document) return;
    auto layer = m_document->getLayer(index);
    if (layer) {
        auto p = layer->parentGroup();
        bool anyExpanded = false;
        while (p) {
            if (!p->isExpanded()) {
                p->setExpanded(true);
                anyExpanded = true;
            }
            p = p->parentGroup();
        }
        if (anyExpanded) {
            onLayerListChanged();
            return;
        }
    }
    int count = static_cast<int>(m_document->layers().size());
    int targetRow = count - 1 - index;
    if (m_layerListWidget->currentRow() != targetRow) {
        m_layerListWidget->blockSignals(true);
        m_layerListWidget->setCurrentRow(targetRow);
        m_layerListWidget->blockSignals(false);
    }
    updateUIFromActiveLayer();
}

void LayerPanelWidget::updateUIFromActiveLayer() {
    if (!m_document) return;
    auto layer = m_document->activeLayer();
    if (!layer) return;

    m_blendCombo->blockSignals(true);
    for (int i = 0; i < m_blendCombo->count(); ++i) {
        if (m_blendCombo->itemData(i).toInt() == static_cast<int>(layer->blendMode())) {
            m_blendCombo->setCurrentIndex(i);
            break;
        }
    }
    m_blendCombo->blockSignals(false);

    m_opacityScale->blockSignals(true);
    m_opacityScale->setValue(layer->opacity() * 100.0);
    m_opacityScale->blockSignals(false);

    m_clipCheck->blockSignals(true);
    m_clipCheck->setChecked(layer->isClippingMask());
    m_clipCheck->blockSignals(false);

    m_alphaLockCheck->blockSignals(true);
    m_alphaLockCheck->setChecked(layer->isAlphaLocked());
    m_alphaLockCheck->blockSignals(false);

    if (m_btnMergeDown) {
        m_btnMergeDown->setEnabled(m_document->activeLayerIndex() > 0);
    }
}

void LayerPanelWidget::onBlendModeChanged(int index) {
    if (!m_document) return;
    auto layer = m_document->activeLayer();
    if (layer) {
        BlendMode mode = static_cast<BlendMode>(m_blendCombo->itemData(index).toInt());
        layer->setBlendMode(mode);
        m_document->composite();
    }
}

void LayerPanelWidget::onOpacityScaleChanged(double value) {
    if (!m_document) return;
    auto layer = m_document->activeLayer();
    if (layer) {
        layer->setOpacity(static_cast<float>(value / 100.0));
        m_document->composite();
    }
}

void LayerPanelWidget::onClippingToggled(bool checked) {
    if (!m_document) return;
    auto layer = m_document->activeLayer();
    if (layer) {
        layer->setClippingMask(checked);
        if (m_layerListWidget) {
            m_layerListWidget->viewport()->update();
        }
        m_document->composite();
    }
}

void LayerPanelWidget::onAlphaLockToggled(bool checked) {
    if (!m_document) return;
    auto layer = m_document->activeLayer();
    if (layer) {
        layer->setAlphaLocked(checked);
    }
}

void LayerPanelWidget::onAddRasterLayer() {
    if (m_document) m_document->addRasterLayer();
}

void LayerPanelWidget::onAddVectorLayer() {
    if (m_document) m_document->addVectorLayer();
}

void LayerPanelWidget::onAddGroupLayer() {
    if (m_document) m_document->addGroupLayer();
}

void LayerPanelWidget::onAddPanelFolder() {
    if (receivers(SIGNAL(newPanelFolderRequested())) > 0) {
        emit newPanelFolderRequested();
        return;
    }
    if (!m_document) return;
    int w = m_document->width();
    int h = m_document->height();
    int padX = std::max(20, w / 16);
    int padY = std::max(20, h / 16);
    QRectF r(padX, padY, w - padX * 2, h - padY * 2);
    QPolygonF poly;
    poly << r.topLeft() << r.topRight() << r.bottomRight() << r.bottomLeft();
    m_document->addPanelFolder(poly);
}

void LayerPanelWidget::onAddThreeDLayer() {
    if (!m_document) return;
    m_document->clearSelection();
    m_document->addThreeDLayer(QString::fromUtf8("3Dレイヤー"));
}

void LayerPanelWidget::onAddTextLayer() {
    if (m_document) m_document->addTextLayer();
}

void LayerPanelWidget::onAddAdjustmentLayer() {
    if (m_document) m_document->addAdjustmentLayer(AdjustmentType::BrightnessContrast);
}

void LayerPanelWidget::onMergeDownLayer() {
    if (!m_document) return;
    int idx = m_document->activeLayerIndex();
    if (idx > 0) {
        m_document->mergeDown(idx);
    }
}

void LayerPanelWidget::onDeleteLayer() {
    if (!m_document) return;
    auto layer = m_document->activeLayer();
    if (layer && layer->name() == "背景") return;
    m_document->removeLayer(m_document->activeLayerIndex());
}

void LayerPanelWidget::updateScrollBarPosition() {
    bool onLeft = (PreferencesDialog::layerScrollBarPosition() == PreferencesDialog::ScrollBarPosition::Left);
    setScrollBarPosition(onLeft);
}

void LayerPanelWidget::setScrollBarPosition(bool onLeft) {
    if (m_layerListWidget) {
        m_layerListWidget->setScrollBarPosition(onLeft);
    }
}

} // namespace Kaburagi
