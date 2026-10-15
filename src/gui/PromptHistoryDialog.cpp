#include "PromptHistoryDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QMessageBox>
#include <QSettings>
#include <QUuid>
#include <algorithm>

namespace Kaburagi {

static const int MAX_UNPINNED_HISTORY = 50;

std::vector<PromptHistoryItem> PromptHistoryDialog::loadHistory() {
    std::vector<PromptHistoryItem> items;
    QSettings settings("KABURAGI", "PaintSoft KABURAGI");
    settings.sync();
    settings.beginGroup("PromptHistory");
    int count = settings.value("count", 0).toInt();

    for (int i = 0; i < count; ++i) {
        settings.beginGroup(QString("item_%1").arg(i));
        PromptHistoryItem item;
        item.id = settings.value("id").toString();
        item.timestamp = settings.value("timestamp").toDateTime();
        item.prompt = settings.value("prompt").toString();
        item.negativePrompt = settings.value("negativePrompt").toString();
        item.llmPrompt = settings.value("llmPrompt").toString();
        item.isPinned = settings.value("isPinned", false).toBool();
        settings.endGroup();

        if (!item.prompt.trimmed().isEmpty() || !item.negativePrompt.trimmed().isEmpty()) {
            items.push_back(item);
        }
    }
    settings.endGroup();

    // ソート: ピン留めが先頭、その中で日時降順
    std::stable_sort(items.begin(), items.end(), [](const PromptHistoryItem& a, const PromptHistoryItem& b) {
        if (a.isPinned != b.isPinned) {
            return a.isPinned > b.isPinned;
        }
        return a.timestamp > b.timestamp;
    });

    return items;
}

void PromptHistoryDialog::saveHistory(const std::vector<PromptHistoryItem>& items) {
    QSettings settings("KABURAGI", "PaintSoft KABURAGI");
    settings.remove("PromptHistory");
    settings.sync();

    settings.beginGroup("PromptHistory");
    settings.setValue("count", static_cast<int>(items.size()));
    for (size_t i = 0; i < items.size(); ++i) {
        settings.beginGroup(QString("item_%1").arg(i));
        settings.setValue("id", items[i].id);
        settings.setValue("timestamp", items[i].timestamp);
        settings.setValue("prompt", items[i].prompt);
        settings.setValue("negativePrompt", items[i].negativePrompt);
        settings.setValue("llmPrompt", items[i].llmPrompt);
        settings.setValue("isPinned", items[i].isPinned);
        settings.endGroup();
    }
    settings.endGroup();
    settings.sync();
}

void PromptHistoryDialog::addHistoryItem(const QString& prompt, const QString& negativePrompt, const QString& llmPrompt) {
    QString p = prompt.trimmed();
    QString np = negativePrompt.trimmed();
    QString lp = llmPrompt.trimmed();

    if (p.isEmpty() && np.isEmpty()) return;

    auto items = loadHistory();

    // 既存と同一のものがあるか検索
    auto it = std::find_if(items.begin(), items.end(), [&](const PromptHistoryItem& item) {
        return item.prompt.trimmed() == p && item.negativePrompt.trimmed() == np;
    });

    bool wasPinned = false;
    QString existingId;
    if (it != items.end()) {
        wasPinned = it->isPinned;
        existingId = it->id;
        if (lp.isEmpty()) {
            lp = it->llmPrompt;
        }
        items.erase(it);
    }

    PromptHistoryItem newItem;
    newItem.id = existingId.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : existingId;
    newItem.timestamp = QDateTime::currentDateTime();
    newItem.prompt = p;
    newItem.negativePrompt = np;
    newItem.llmPrompt = lp;
    newItem.isPinned = wasPinned;

    // 先頭に追加
    items.insert(items.begin(), newItem);

    // ソート (ピン留め優先)
    std::stable_sort(items.begin(), items.end(), [](const PromptHistoryItem& a, const PromptHistoryItem& b) {
        if (a.isPinned != b.isPinned) {
            return a.isPinned > b.isPinned;
        }
        return a.timestamp > b.timestamp;
    });

    // ピン留めされていないアイテムの件数制限
    std::vector<PromptHistoryItem> pruned;
    int unpinnedCount = 0;
    for (const auto& item : items) {
        if (item.isPinned) {
            pruned.push_back(item);
        } else {
            if (unpinnedCount < MAX_UNPINNED_HISTORY) {
                pruned.push_back(item);
                unpinnedCount++;
            }
        }
    }

    saveHistory(pruned);
}

PromptHistoryDialog::PromptHistoryDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("プロンプト履歴 & ピン留め"));
    resize(820, 600);
    m_items = loadHistory();
    setupUi();
    refreshTable();
}

void PromptHistoryDialog::setupUi() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // 検索バー
    auto laySearch = new QHBoxLayout();
    laySearch->addWidget(new QLabel(tr("🔍 検索フィルター:"), this));
    m_editSearch = new QLineEdit(this);
    m_editSearch->setPlaceholderText(tr("プロンプト、ネガティブ、またはLLM要望のキーワードで検索..."));
    m_editSearch->setClearButtonEnabled(true);
    laySearch->addWidget(m_editSearch);
    mainLayout->addLayout(laySearch);

    // テーブル一覧
    m_tableHistory = new QTableWidget(this);
    m_tableHistory->setColumnCount(5);
    m_tableHistory->setHorizontalHeaderLabels({ tr("固定"), tr("記録日時"), tr("プロンプト"), tr("ネガティブプロンプト"), tr("LLMプロンプト") });
    m_tableHistory->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_tableHistory->setColumnWidth(0, 50);
    m_tableHistory->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tableHistory->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_tableHistory->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    m_tableHistory->setColumnWidth(3, 160);
    m_tableHistory->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);
    m_tableHistory->setColumnWidth(4, 150);
    m_tableHistory->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableHistory->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableHistory->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableHistory->setAlternatingRowColors(true);
    mainLayout->addWidget(m_tableHistory, 1);

    // プレビュー詳細グループ
    auto grpPreview = new QGroupBox(tr("選択した履歴の内容"), this);
    auto gridPreview = new QGridLayout(grpPreview);
    gridPreview->setContentsMargins(6, 6, 6, 6);
    gridPreview->setSpacing(4);

    gridPreview->addWidget(new QLabel(tr("プロンプト:"), grpPreview), 0, 0);
    m_previewPrompt = new QPlainTextEdit(grpPreview);
    m_previewPrompt->setReadOnly(true);
    m_previewPrompt->setMaximumHeight(70);
    gridPreview->addWidget(m_previewPrompt, 0, 1);

    gridPreview->addWidget(new QLabel(tr("ネガティブ:"), grpPreview), 1, 0);
    m_previewNegative = new QPlainTextEdit(grpPreview);
    m_previewNegative->setReadOnly(true);
    m_previewNegative->setMaximumHeight(50);
    gridPreview->addWidget(m_previewNegative, 1, 1);

    gridPreview->addWidget(new QLabel(tr("LLMプロンプト:"), grpPreview), 2, 0);
    m_previewLlm = new QPlainTextEdit(grpPreview);
    m_previewLlm->setReadOnly(true);
    m_previewLlm->setMaximumHeight(50);
    gridPreview->addWidget(m_previewLlm, 2, 1);

    mainLayout->addWidget(grpPreview);

    // 操作ボタン
    auto layBottom = new QHBoxLayout();

    m_btnTogglePin = new QPushButton(tr("📌 ピン留め切替"), this);
    m_btnDelete = new QPushButton(tr("🗑️ 選択項目を削除"), this);
    m_btnClearUnpinned = new QPushButton(tr("🧹 ピン留め以外を全消去"), this);
    layBottom->addWidget(m_btnTogglePin);
    layBottom->addWidget(m_btnDelete);
    layBottom->addWidget(m_btnClearUnpinned);
    layBottom->addStretch();

    m_btnApplyAll = new QPushButton(tr("✅ 全て適用"), this);
    m_btnApplyAll->setStyleSheet("QPushButton { font-weight: bold; background-color: #2e7d32; color: white; padding: 6px 12px; }");
    m_btnApplyPromptOnly = new QPushButton(tr("プロンプトのみ適用"), this);
    m_btnApplyNegativeOnly = new QPushButton(tr("ネガティブのみ適用"), this);
    auto btnClose = new QPushButton(tr("閉じる"), this);

    layBottom->addWidget(m_btnApplyPromptOnly);
    layBottom->addWidget(m_btnApplyNegativeOnly);
    layBottom->addWidget(m_btnApplyAll);
    layBottom->addWidget(btnClose);

    mainLayout->addLayout(layBottom);

    // 接続
    connect(m_editSearch, &QLineEdit::textChanged, this, &PromptHistoryDialog::onSearchTextChanged);
    connect(m_tableHistory, &QTableWidget::itemSelectionChanged, this, &PromptHistoryDialog::onTableSelectionChanged);
    connect(m_tableHistory, &QTableWidget::itemDoubleClicked, this, &PromptHistoryDialog::onTableItemDoubleClicked);
    connect(m_btnTogglePin, &QPushButton::clicked, this, &PromptHistoryDialog::onTogglePinClicked);
    connect(m_btnDelete, &QPushButton::clicked, this, &PromptHistoryDialog::onDeleteSelectedClicked);
    connect(m_btnClearUnpinned, &QPushButton::clicked, this, &PromptHistoryDialog::onClearUnpinnedClicked);

    connect(m_btnApplyAll, &QPushButton::clicked, this, &PromptHistoryDialog::onApplyAllClicked);
    connect(m_btnApplyPromptOnly, &QPushButton::clicked, this, &PromptHistoryDialog::onApplyPromptOnlyClicked);
    connect(m_btnApplyNegativeOnly, &QPushButton::clicked, this, &PromptHistoryDialog::onApplyNegativeOnlyClicked);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);
}

void PromptHistoryDialog::refreshTable() {
    m_tableHistory->setRowCount(0);
    m_filteredIndices.clear();

    QString query = m_editSearch ? m_editSearch->text().trimmed().toLower() : QString();

    for (size_t i = 0; i < m_items.size(); ++i) {
        const auto& it = m_items[i];
        if (!query.isEmpty()) {
            bool match = it.prompt.toLower().contains(query) ||
                         it.negativePrompt.toLower().contains(query) ||
                         it.llmPrompt.toLower().contains(query);
            if (!match) continue;
        }

        int row = m_tableHistory->rowCount();
        m_tableHistory->insertRow(row);
        m_filteredIndices.push_back(static_cast<int>(i));

        // 0: ピン留め
        auto itemPin = new QTableWidgetItem(it.isPinned ? "📌" : "");
        itemPin->setTextAlignment(Qt::AlignCenter);
        m_tableHistory->setItem(row, 0, itemPin);

        // 1: 日時
        auto itemDate = new QTableWidgetItem(it.timestamp.isValid() ? it.timestamp.toString("yyyy-MM-dd HH:mm") : "-");
        itemDate->setTextAlignment(Qt::AlignCenter);
        m_tableHistory->setItem(row, 1, itemDate);

        // 2: プロンプト
        QString pShort = it.prompt.simplified();
        if (pShort.length() > 100) pShort = pShort.left(97) + "...";
        auto itemPrompt = new QTableWidgetItem(pShort);
        itemPrompt->setToolTip(it.prompt);
        m_tableHistory->setItem(row, 2, itemPrompt);

        // 3: ネガティブプロンプト
        QString npShort = it.negativePrompt.simplified();
        if (npShort.length() > 60) npShort = npShort.left(57) + "...";
        auto itemNeg = new QTableWidgetItem(npShort);
        itemNeg->setToolTip(it.negativePrompt);
        m_tableHistory->setItem(row, 3, itemNeg);

        // 4: LLMプロンプト
        QString lpShort = it.llmPrompt.simplified();
        if (lpShort.length() > 50) lpShort = lpShort.left(47) + "...";
        auto itemLlm = new QTableWidgetItem(lpShort);
        itemLlm->setToolTip(it.llmPrompt);
        m_tableHistory->setItem(row, 4, itemLlm);
    }

    if (m_tableHistory->rowCount() > 0) {
        m_tableHistory->selectRow(0);
    } else {
        onTableSelectionChanged();
    }
}

int PromptHistoryDialog::currentSelectedIndex() const {
    int row = m_tableHistory->currentRow();
    if (row >= 0 && row < static_cast<int>(m_filteredIndices.size())) {
        return m_filteredIndices[row];
    }
    return -1;
}

void PromptHistoryDialog::onSearchTextChanged(const QString& /*text*/) {
    refreshTable();
}

void PromptHistoryDialog::onTableSelectionChanged() {
    int idx = currentSelectedIndex();
    if (idx >= 0 && idx < static_cast<int>(m_items.size())) {
        const auto& it = m_items[idx];
        m_previewPrompt->setPlainText(it.prompt);
        m_previewNegative->setPlainText(it.negativePrompt);
        m_previewLlm->setPlainText(it.llmPrompt);

        m_btnTogglePin->setEnabled(true);
        m_btnTogglePin->setText(it.isPinned ? tr("📌 ピン留め解除") : tr("📌 ピン留めする"));
        m_btnDelete->setEnabled(true);
        m_btnApplyAll->setEnabled(true);
        m_btnApplyPromptOnly->setEnabled(true);
        m_btnApplyNegativeOnly->setEnabled(true);
    } else {
        m_previewPrompt->clear();
        m_previewNegative->clear();
        m_previewLlm->clear();

        m_btnTogglePin->setEnabled(false);
        m_btnDelete->setEnabled(false);
        m_btnApplyAll->setEnabled(false);
        m_btnApplyPromptOnly->setEnabled(false);
        m_btnApplyNegativeOnly->setEnabled(false);
    }
}

void PromptHistoryDialog::onTableItemDoubleClicked(QTableWidgetItem* /*item*/) {
    onApplyAllClicked();
}

void PromptHistoryDialog::onTogglePinClicked() {
    int idx = currentSelectedIndex();
    if (idx >= 0 && idx < static_cast<int>(m_items.size())) {
        m_items[idx].isPinned = !m_items[idx].isPinned;
        // ソート
        std::stable_sort(m_items.begin(), m_items.end(), [](const PromptHistoryItem& a, const PromptHistoryItem& b) {
            if (a.isPinned != b.isPinned) {
                return a.isPinned > b.isPinned;
            }
            return a.timestamp > b.timestamp;
        });
        saveHistory(m_items);
        refreshTable();
    }
}

void PromptHistoryDialog::onDeleteSelectedClicked() {
    int idx = currentSelectedIndex();
    if (idx >= 0 && idx < static_cast<int>(m_items.size())) {
        if (QMessageBox::question(this, tr("履歴削除"), tr("選択した履歴を削除しますか？")) == QMessageBox::Yes) {
            m_items.erase(m_items.begin() + idx);
            saveHistory(m_items);
            refreshTable();
        }
    }
}

void PromptHistoryDialog::onClearUnpinnedClicked() {
    if (QMessageBox::question(this, tr("ピン留め以外の全消去"), tr("ピン留めされていない履歴をすべて消去しますか？\n（ピン留め項目は残ります）")) == QMessageBox::Yes) {
        std::vector<PromptHistoryItem> pinnedOnly;
        for (const auto& it : m_items) {
            if (it.isPinned) pinnedOnly.push_back(it);
        }
        m_items = pinnedOnly;
        saveHistory(m_items);
        refreshTable();
    }
}

QString PromptHistoryDialog::selectedPrompt() const {
    int idx = currentSelectedIndex();
    if (idx >= 0 && idx < static_cast<int>(m_items.size())) {
        return m_items[idx].prompt;
    }
    return QString();
}

QString PromptHistoryDialog::selectedNegativePrompt() const {
    int idx = currentSelectedIndex();
    if (idx >= 0 && idx < static_cast<int>(m_items.size())) {
        return m_items[idx].negativePrompt;
    }
    return QString();
}

void PromptHistoryDialog::onApplyAllClicked() {
    if (currentSelectedIndex() >= 0) {
        m_applyPrompt = true;
        m_applyNegative = true;
        accept();
    }
}

void PromptHistoryDialog::onApplyPromptOnlyClicked() {
    if (currentSelectedIndex() >= 0) {
        m_applyPrompt = true;
        m_applyNegative = false;
        accept();
    }
}

void PromptHistoryDialog::onApplyNegativeOnlyClicked() {
    if (currentSelectedIndex() >= 0) {
        m_applyPrompt = false;
        m_applyNegative = true;
        accept();
    }
}

} // namespace Kaburagi
