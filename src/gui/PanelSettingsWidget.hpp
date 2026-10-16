#pragma once

#include <QWidget>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QColor>
#include <QGroupBox>

namespace Kaburagi {

class GroupLayer;
enum class ToolType;

class PanelSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit PanelSettingsWidget(QWidget* parent = nullptr);
    ~PanelSettingsWidget() override = default;

    void setToolType(ToolType tool);

    // 設定値の取得
    bool drawBorder() const;
    float borderWidth() const;
    QColor borderColor() const;

    float marginHorizontal() const;
    float marginVertical() const;
    bool snapToBleed() const;
    bool linkAdjacentPanels() const;
    void setLinkAdjacentPanels(bool enable);

    // 表示状態の取得
    bool isBorderGroupVisible() const;
    bool isMarginGroupVisible() const;

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

    QGroupBox* m_borderGroup = nullptr;
    QCheckBox* m_drawBorderCheck = nullptr;
    QDoubleSpinBox* m_borderWidthSpin = nullptr;
    QPushButton* m_borderColorBtn = nullptr;
    QColor m_borderColor = Qt::black;

    QGroupBox* m_marginGroup = nullptr;
    QDoubleSpinBox* m_marginHSpin = nullptr;
    QDoubleSpinBox* m_marginVSpin = nullptr;
    QCheckBox* m_snapToBleedCheck = nullptr;
    QCheckBox* m_linkAdjacentPanelsCheck = nullptr;
};

} // namespace Kaburagi
