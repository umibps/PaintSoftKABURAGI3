#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QRadioButton>
#include <QPushButton>
#include "PageProject.hpp"

namespace Kaburagi {

class NewProjectDialog : public QDialog {
    Q_OBJECT
public:
    explicit NewProjectDialog(QWidget* parent = nullptr);
    ~NewProjectDialog() override = default;

    QString projectName() const;
    QString projectDirectory() const;
    int pageCount() const;
    int canvasWidth() const;
    int canvasHeight() const;
    double canvasDpi() const;
    BindingDirection bindingDirection() const;
    bool startFromRight() const;

private slots:
    void onBrowseDirectory();
    void onPresetChanged(int index);
    void validateInput();

private:
    void setupUI();

    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_dirEdit = nullptr;
    QPushButton* m_browseBtn = nullptr;

    QSpinBox* m_pageCountSpin = nullptr;

    QComboBox* m_presetCombo = nullptr;
    QSpinBox* m_widthSpin = nullptr;
    QSpinBox* m_heightSpin = nullptr;
    QSpinBox* m_dpiSpin = nullptr;

    QRadioButton* m_radioRightToLeft = nullptr;
    QRadioButton* m_radioLeftToRight = nullptr;

    QRadioButton* m_radioStartRight = nullptr;
    QRadioButton* m_radioStartLeft = nullptr;

    QPushButton* m_createBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
};

} // namespace Kaburagi
