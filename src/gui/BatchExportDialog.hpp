#pragma once

#include <QDialog>
#include <QRadioButton>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <memory>
#include "PageProject.hpp"

namespace Kaburagi {

class BatchExportDialog : public QDialog {
    Q_OBJECT
public:
    explicit BatchExportDialog(std::shared_ptr<PageProject> project, QWidget* parent = nullptr);
    ~BatchExportDialog() override = default;

private slots:
    void onBrowseOutDir();
    void onFormatChanged(int index);
    void onStartExport();

private:
    void setupUI();
    std::vector<int> parseSelectedPages() const;

    std::shared_ptr<PageProject> m_project;

    QRadioButton* m_radioAllPages = nullptr;
    QRadioButton* m_radioRangePages = nullptr;
    QLineEdit* m_rangeEdit = nullptr;

    QComboBox* m_formatCombo = nullptr;
    QLineEdit* m_outDirEdit = nullptr;
    QPushButton* m_browseDirBtn = nullptr;

    QLineEdit* m_namePatternEdit = nullptr;
    QComboBox* m_scaleCombo = nullptr;

    QWidget* m_jpegQualityWidget = nullptr;
    QSlider* m_jpegQualitySlider = nullptr;
    QSpinBox* m_jpegQualitySpin = nullptr;

    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_startBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;
};

} // namespace Kaburagi
