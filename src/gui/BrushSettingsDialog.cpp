#include "BrushSettingsDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLabel>

namespace Kaburagi {

static const struct {
    ToolType type;
    const char* label;
} AVAILABLE_BRUSH_TYPES[] = {
    { ToolType::Pen, "鉛筆" },
    { ToolType::Airbrush, "エアブラシ" },
    { ToolType::Watercolor, "水彩筆" },
    { ToolType::Marker, "ブラシ" },
    { ToolType::Eraser, "消しゴム" },
    { ToolType::Bucket, "バケツ" },
    { ToolType::Blur, "ぼかし" },
    { ToolType::Blend, "指先" },
    { ToolType::Gradient, "グラデーション" },
    { ToolType::CompositeBrush, "合成ブラシ" }
};

BrushSettingsDialog::BrushSettingsDialog(const ToolSlotItem& item, QWidget* parent)
    : QDialog(parent), m_item(item) {
    setWindowTitle("ブラシの設定");
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setFixedSize(320, 220);

    setupUI();
}

void BrushSettingsDialog::setupUI() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    auto formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setSpacing(8);

    // 1. ブラシの名前
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setText(m_item.name);
    formLayout->addRow("ブラシの名前:", m_nameEdit);

    // 2. ブラシのタイプ
    m_typeCombo = new QComboBox(this);
    for (const auto& entry : AVAILABLE_BRUSH_TYPES) {
        m_typeCombo->addItem(entry.label, static_cast<int>(entry.type));
    }
    int typeIdx = m_typeCombo->findData(static_cast<int>(m_item.toolType));
    if (typeIdx >= 0) {
        m_typeCombo->setCurrentIndex(typeIdx);
    }
    formLayout->addRow("ブラシのタイプ:", m_typeCombo);

    // 3. ブラシのショートカットキー
    auto shortcutContainer = new QWidget(this);
    auto shortcutLayout = new QHBoxLayout(shortcutContainer);
    shortcutLayout->setContentsMargins(0, 0, 0, 0);
    shortcutLayout->setSpacing(4);

    m_shortcutEdit = new QKeySequenceEdit(this);
    m_shortcutEdit->setKeySequence(m_item.shortcut);
    shortcutLayout->addWidget(m_shortcutEdit, 1);

    m_clearShortcutBtn = new QPushButton("クリア", this);
    m_clearShortcutBtn->setFixedWidth(50);
    connect(m_clearShortcutBtn, &QPushButton::clicked, this, [this]() {
        m_shortcutEdit->clear();
    });
    shortcutLayout->addWidget(m_clearShortcutBtn);

    formLayout->addRow("ショートカット:", shortcutContainer);

    // 4. ブラシのアイコン
    m_iconCombo = new QComboBox(this);
    for (const auto& entry : AVAILABLE_BRUSH_TYPES) {
        m_iconCombo->addItem(entry.label, static_cast<int>(entry.type));
    }
    int iconIdx = m_iconCombo->findData(static_cast<int>(m_item.iconType));
    if (iconIdx >= 0) {
        m_iconCombo->setCurrentIndex(iconIdx);
    }
    formLayout->addRow("アイコン:", m_iconCombo);

    mainLayout->addLayout(formLayout);

    // タイプ変更時にアイコンとデフォルト名前も自動で合わせる
    QString prevTypeName = m_typeCombo->currentText();
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, prevTypeName](int idx) mutable {
        if (idx >= 0 && idx < m_iconCombo->count()) {
            m_iconCombo->setCurrentIndex(idx);
            if (m_nameEdit->text().trimmed().isEmpty() || m_nameEdit->text() == prevTypeName) {
                m_nameEdit->setText(m_typeCombo->currentText());
            }
            prevTypeName = m_typeCombo->currentText();
        }
    });

    // 5. OK / Cancel ボタン
    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        m_item.name = m_nameEdit->text().trimmed();
        if (m_item.name.isEmpty()) {
            m_item.name = m_typeCombo->currentText();
        }
        m_item.toolType = static_cast<ToolType>(m_typeCombo->currentData().toInt());
        m_item.shortcut = m_shortcutEdit->keySequence();
        m_item.iconType = static_cast<ToolType>(m_iconCombo->currentData().toInt());
        m_item.isEmpty = false;
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(buttonBox);
}

ToolSlotItem BrushSettingsDialog::slotItem() const {
    return m_item;
}

} // namespace Kaburagi
