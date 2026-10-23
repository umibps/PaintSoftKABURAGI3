#pragma once

#include <QDialog>
#include <QPainterPath>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QDialogButtonBox>

namespace Kaburagi {

class Document;
class CanvasWidget;

/**
 * @brief 選択範囲の拡大/縮小ダイアログ
 * 現在の選択範囲をピクセル数（円形/矩形）または比率(%)で拡大・縮小する。
 */
class ModifySelectionDialog : public QDialog {
    Q_OBJECT
public:
    enum class ActionType {
        Expand,   // 拡大
        Contract  // 縮小
    };

    enum class MethodType {
        Pixel,  // ピクセル数指定
        Ratio   // 比率(%)指定
    };

    enum class CornerType {
        Round,  // 円形 (角を丸くする)
        Square  // 矩形 (角を直角にする)
    };

    explicit ModifySelectionDialog(Document* doc, CanvasWidget* canvas, QWidget* parent = nullptr);
    ~ModifySelectionDialog() override;

    ActionType actionType() const;
    void setActionType(ActionType action);

    MethodType methodType() const;
    void setMethodType(MethodType method);

    int pixelAmount() const;
    void setPixelAmount(int px);

    CornerType cornerType() const;
    void setCornerType(CornerType corner);

    double ratioPercent() const;
    void setRatioPercent(double pct);

    bool isPreviewEnabled() const;
    void setPreviewEnabled(bool enabled);

    QPainterPath calculateModifiedPath() const;

public slots:
    void accept() override;
    void reject() override;

private slots:
    void updatePreview();
    void onActionTypeChanged();
    void onMethodTypeChanged();

private:
    void setupUi();

    Document* m_document = nullptr;
    CanvasWidget* m_canvas = nullptr;
    QPainterPath m_originalPath;

    QRadioButton* m_radioExpand = nullptr;
    QRadioButton* m_radioContract = nullptr;

    QRadioButton* m_radioPixel = nullptr;
    QRadioButton* m_radioRatio = nullptr;

    QWidget* m_pixelWidget = nullptr;
    QSpinBox* m_pixelSpin = nullptr;
    QRadioButton* m_radioRound = nullptr;
    QRadioButton* m_radioSquare = nullptr;

    QWidget* m_ratioWidget = nullptr;
    QDoubleSpinBox* m_ratioSpin = nullptr;

    QCheckBox* m_previewCheck = nullptr;
};

} // namespace Kaburagi
