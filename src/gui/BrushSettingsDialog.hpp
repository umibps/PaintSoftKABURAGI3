#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QKeySequenceEdit>
#include <QPushButton>
#include "ToolBoxWidget.hpp"

namespace Kaburagi {

/**
 * @brief ブラシ設定ダイアログ
 * - ブラシの名前
 * - ブラシのタイプ (鉛筆、エアブラシ、水彩筆、マーカー、消しゴム、バケツ、ぼかし、指先、グラデーション、合成ブラシ)
 * - ブラシのショートカットキー
 * - ブラシのアイコン (鉛筆〜合成ブラシの10種)
 * - OK / Cancel ボタン
 */
class BrushSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit BrushSettingsDialog(const ToolSlotItem& item, QWidget* parent = nullptr);
    ~BrushSettingsDialog() override = default;

    ToolSlotItem slotItem() const;
    QString brushName() const { return slotItem().name; }
    ToolType brushType() const { return slotItem().toolType; }
    ToolType iconType() const { return slotItem().iconType; }
    QKeySequence shortcut() const { return slotItem().shortcut; }

private:
    void setupUI();

    ToolSlotItem m_item;

    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_typeCombo = nullptr;
    QKeySequenceEdit* m_shortcutEdit = nullptr;
    QPushButton* m_clearShortcutBtn = nullptr;
    QComboBox* m_iconCombo = nullptr;
};

} // namespace Kaburagi
