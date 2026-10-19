#pragma once

#include <QWidget>
#include <QFontComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QButtonGroup>
#include <QTextEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QTabWidget>
#include "TextLayer.hpp"

namespace Kaburagi {

enum class ToolType;

class TextSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit TextSettingsWidget(QWidget* parent = nullptr);
    ~TextSettingsWidget() override = default;

    void setToolType(ToolType tool);

    // テキスト属性
    QFont currentFont() const;
    int fontSize() const;
    bool isBold() const;
    bool isItalic() const;

    QColor textColor() const;
    void setTextColor(const QColor& color);

    WritingDirection direction() const;
    void setDirection(WritingDirection dir);

    Qt::Alignment horizontalAlignment() const;
    Qt::Alignment verticalAlignment() const;

    QString text() const;
    void setText(const QString& text);

    // タグ表示モード (タグあり / タグなし)
    bool isHideTags() const { return m_hideTags; }
    void setHideTags(bool hide);

    // 吹き出し属性
    BalloonShape balloonShape() const;
    bool hasTail() const;
    float tailBaseWidth() const;
    float tailCurvature() const;

    int spikeCount() const;
    float spikeDepth() const;
    float spikePhase() const;

    float frameWidth() const;
    QColor frameColor() const;
    QColor fillColor() const;
    bool isTransparentFill() const;

    bool hasOutline() const;
    float outlineWidth() const;
    QColor outlineColor() const;

    void setFromItem(const TextBalloonItem& item);
    void applyToItem(TextBalloonItem& item) const;

    void focusTextEdit();

signals:
    void settingsChanged();
    void textChanged(const QString& text);

private:
    void setupUi();
    void updateColorButtons();
    void updateSpecialShapeVisibility();

    void wrapSelectionWithTags(const QString& openTag, const QString& closeTag);
    void onInlineBoldClicked();
    void onInlineItalicClicked();
    void onInlineColorClicked();
    void onInlineSizeClicked();
    void onInlineRubyClicked();
    QString convertTagsToHtml(const QString& rawText) const;
    void updateTextDisplay();

    QTabWidget* m_tabWidget = nullptr;

    // 1. テキスト入力 & 入力支援ボタン
    QPushButton* m_btnInlineBold = nullptr;
    QPushButton* m_btnInlineItalic = nullptr;
    QPushButton* m_btnInlineColor = nullptr;
    QSpinBox* m_spinInlineSize = nullptr;
    QPushButton* m_btnInlineSize = nullptr;
    QPushButton* m_btnInlineRuby = nullptr;

    QButtonGroup* m_tagViewModeGroup = nullptr;
    QPushButton* m_btnShowTags = nullptr;
    QPushButton* m_btnHideTags = nullptr;
    bool m_hideTags = false;
    QString m_rawText;

    QTextEdit* m_textEdit = nullptr;

    // 2. フォント設定
    QFontComboBox* m_fontCombo = nullptr;
    QSpinBox* m_fontSizeSpin = nullptr;
    QPushButton* m_btnBold = nullptr;
    QPushButton* m_btnItalic = nullptr;
    QPushButton* m_btnTextColor = nullptr;

    // 3. 配置・書字方向
    QButtonGroup* m_dirGroup = nullptr;
    QPushButton* m_btnHorizontal = nullptr;
    QPushButton* m_btnVertical = nullptr;

    QButtonGroup* m_hAlignGroup = nullptr;
    QPushButton* m_btnAlignLeft = nullptr;
    QPushButton* m_btnAlignCenter = nullptr;
    QPushButton* m_btnAlignRight = nullptr;

    QButtonGroup* m_vAlignGroup = nullptr;
    QPushButton* m_btnAlignTop = nullptr;
    QPushButton* m_btnAlignVCenter = nullptr;
    QPushButton* m_btnAlignBottom = nullptr;

    // 4. 吹き出し形状
    QComboBox* m_shapeCombo = nullptr;

    // 5. 特殊形状パラメータ (Spike / Cloud)
    QWidget* m_specialShapeWidget = nullptr;
    QSpinBox* m_spikeCountSpin = nullptr;
    QDoubleSpinBox* m_spikeDepthSpin = nullptr;
    QDoubleSpinBox* m_spikePhaseSpin = nullptr;

    // 6. しっぽパラメータ
    QCheckBox* m_hasTailCheck = nullptr;
    QWidget* m_tailParamsWidget = nullptr;
    QDoubleSpinBox* m_tailBaseWidthSpin = nullptr;
    QSlider* m_tailCurvatureSlider = nullptr;
    QLabel* m_tailCurvatureLabel = nullptr;

    // 7. スタイル (枠線・背景・フチ取り)
    QDoubleSpinBox* m_frameWidthSpin = nullptr;
    QPushButton* m_btnFrameColor = nullptr;
    QPushButton* m_btnFillColor = nullptr;
    QCheckBox* m_transparentFillCheck = nullptr;

    QCheckBox* m_hasOutlineCheck = nullptr;
    QWidget* m_outlineParamsWidget = nullptr;
    QDoubleSpinBox* m_outlineWidthSpin = nullptr;
    QPushButton* m_btnOutlineColor = nullptr;

    QColor m_textColor{Qt::black};
    QColor m_frameColor{Qt::black};
    QColor m_fillColor{Qt::white};
    QColor m_outlineColor{Qt::white};

    bool m_updatingUi = false;
};

} // namespace Kaburagi
