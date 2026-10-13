#pragma once

#include <QWidget>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QColor>

namespace Kaburagi {

class GroupLayer;

class PanelSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit PanelSettingsWidget(QWidget* parent = nullptr);
    ~PanelSettingsWidget() override = default;

    // 設定値の取得
    bool drawBorder() const;
    float borderWidth() const;
    QColor borderColor() const;

    float marginHorizontal() const;
    float marginVertical() const;
    bool snapToBleed() const;

    // コマフォルダからの設定読み込み & 適用
    void setFromPanel(const GroupLayer* panel);
    void applyToPanel(GroupLayer* panel) const;

signals:
    void settingsChanged();

private slots:
    void chooseBorderColor();

private:
    void setupUI();
    void updateColorButtonStyle();

    QCheckBox* m_drawBorderCheck = nullptr;
    QDoubleSpinBox* m_borderWidthSpin = nullptr;
    QPushButton* m_borderColorBtn = nullptr;
    QColor m_borderColor = Qt::black;

    QDoubleSpinBox* m_marginHSpin = nullptr;
    QDoubleSpinBox* m_marginVSpin = nullptr;
    QCheckBox* m_snapToBleedCheck = nullptr;
};

} // namespace Kaburagi
