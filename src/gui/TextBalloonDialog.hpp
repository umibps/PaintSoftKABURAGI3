#pragma once

#include <QDialog>
#include <QPlainTextEdit>
#include <QFontComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include "TextLayer.hpp"

namespace Kaburagi {

class TextBalloonDialog : public QDialog {
    Q_OBJECT
public:
    explicit TextBalloonDialog(const TextBalloonItem& item, QWidget* parent = nullptr);
    ~TextBalloonDialog() override = default;

    TextBalloonItem getItem() const;

private slots:
    void onFrameColorClicked();
    void onFillColorClicked();
    void updateColorButtons();

private:
    TextBalloonItem m_item;

    QPlainTextEdit* m_textEdit = nullptr;
    QFontComboBox* m_fontCombo = nullptr;
    QSpinBox* m_fontSizeSpin = nullptr;
    QCheckBox* m_boldCheck = nullptr;

    QComboBox* m_shapeCombo = nullptr;
    QSpinBox* m_frameWidthSpin = nullptr;
    QPushButton* m_frameColorBtn = nullptr;
    QPushButton* m_fillColorBtn = nullptr;
    QCheckBox* m_hasTailCheck = nullptr;
    QCheckBox* m_transparentFillCheck = nullptr;

    QColor m_frameColor;
    QColor m_fillColor;
};

} // namespace Kaburagi
