#pragma once

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <memory>
#include <vector>
#include "Document.hpp"
#include "GroupLayer.hpp"

namespace Kaburagi {

class MergePanelDialog : public QDialog {
    Q_OBJECT
public:
    MergePanelDialog(const std::shared_ptr<GroupLayer>& targetPanel,
                     const std::vector<Document::AdjacentPanelInfo>& candidates,
                     QWidget* parent = nullptr)
        : QDialog(parent)
        , m_targetPanel(targetPanel)
        , m_candidates(candidates)
    {
        setWindowTitle(tr("コマ枠を結合"));
        setMinimumWidth(320);
        setupUI();
    }

    std::shared_ptr<GroupLayer> selectedPanel() const {
        int idx = m_listWidget ? m_listWidget->currentRow() : -1;
        if (idx >= 0 && idx < static_cast<int>(m_candidates.size())) {
            return m_candidates[idx].panel;
        }
        return nullptr;
    }

private:
    void setupUI() {
        auto rootLayout = new QVBoxLayout(this);
        rootLayout->setSpacing(10);

        QString targetName = m_targetPanel ? m_targetPanel->name() : tr("コマ枠");
        auto labelTarget = new QLabel(tr("基準コマ: <b>%1</b>").arg(targetName), this);
        rootLayout->addWidget(labelTarget);

        auto labelPrompt = new QLabel(tr("結合する隣接コマを選択してください:"), this);
        rootLayout->addWidget(labelPrompt);

        m_listWidget = new QListWidget(this);
        for (const auto& c : m_candidates) {
            QString name = c.panel ? c.panel->name() : tr("コマ");
            QString itemText = QString("%1 (%2に隣接 - 間隔約 %3 px)")
                                   .arg(name)
                                   .arg(c.direction)
                                   .arg(static_cast<int>(std::round(c.distance)));
            m_listWidget->addItem(itemText);
        }
        if (m_listWidget->count() > 0) {
            m_listWidget->setCurrentRow(0);
        }
        rootLayout->addWidget(m_listWidget);

        auto btnLayout = new QHBoxLayout();
        btnLayout->addStretch();

        auto okBtn = new QPushButton(tr("結合(&M)"), this);
        okBtn->setDefault(true);
        connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
        btnLayout->addWidget(okBtn);

        auto cancelBtn = new QPushButton(tr("キャンセル"), this);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        btnLayout->addWidget(cancelBtn);

        rootLayout->addLayout(btnLayout);
    }

    std::shared_ptr<GroupLayer> m_targetPanel;
    std::vector<Document::AdjacentPanelInfo> m_candidates;
    QListWidget* m_listWidget = nullptr;
};

} // namespace Kaburagi
