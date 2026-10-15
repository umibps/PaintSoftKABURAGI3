#pragma once

#include <QDialog>
#include <QString>
#include <QDateTime>
#include <vector>

class QTableWidget;
class QTableWidgetItem;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QLabel;

namespace Kaburagi {

struct PromptHistoryItem {
    QString id;
    QDateTime timestamp;
    QString prompt;
    QString negativePrompt;
    QString llmPrompt;
    bool isPinned = false;
};

class PromptHistoryDialog : public QDialog {
    Q_OBJECT

public:
    explicit PromptHistoryDialog(QWidget* parent = nullptr);
    ~PromptHistoryDialog() override = default;

    // 履歴アイテムの追加・永続化
    static void addHistoryItem(const QString& prompt, const QString& negativePrompt, const QString& llmPrompt = QString());
    static std::vector<PromptHistoryItem> loadHistory();
    static void saveHistory(const std::vector<PromptHistoryItem>& items);

    // 選択されたプロンプト情報
    QString selectedPrompt() const;
    QString selectedNegativePrompt() const;

    // 適用対象フラグ (OKで閉じたときの適用先)
    bool applyPrompt() const { return m_applyPrompt; }
    bool applyNegative() const { return m_applyNegative; }

private slots:
    void onSearchTextChanged(const QString& text);
    void onTableSelectionChanged();
    void onTableItemDoubleClicked(QTableWidgetItem* item);
    void onTogglePinClicked();
    void onDeleteSelectedClicked();
    void onClearUnpinnedClicked();
    void onApplyAllClicked();
    void onApplyPromptOnlyClicked();
    void onApplyNegativeOnlyClicked();

private:
    void setupUi();
    void refreshTable();
    int currentSelectedIndex() const;

private:
    std::vector<PromptHistoryItem> m_items;
    std::vector<int> m_filteredIndices;

    QLineEdit* m_editSearch = nullptr;
    QTableWidget* m_tableHistory = nullptr;

    QPlainTextEdit* m_previewPrompt = nullptr;
    QPlainTextEdit* m_previewNegative = nullptr;
    QPlainTextEdit* m_previewLlm = nullptr;

    QPushButton* m_btnTogglePin = nullptr;
    QPushButton* m_btnDelete = nullptr;
    QPushButton* m_btnClearUnpinned = nullptr;

    QPushButton* m_btnApplyAll = nullptr;
    QPushButton* m_btnApplyPromptOnly = nullptr;
    QPushButton* m_btnApplyNegativeOnly = nullptr;

    bool m_applyPrompt = true;
    bool m_applyNegative = true;
};

} // namespace Kaburagi
