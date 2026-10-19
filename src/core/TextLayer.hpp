#pragma once

#include "Layer.hpp"
#include <QString>
#include <QRectF>
#include <QFont>
#include <QColor>
#include <QPointF>
#include <vector>
#include <optional>

namespace Kaburagi {

struct InlineStyle {
    bool bold = false;
    bool italic = false;
    std::optional<QColor> color;
    std::optional<qreal> fontSize; // pt

    bool operator==(const InlineStyle& o) const {
        return bold == o.bold && italic == o.italic && color == o.color && fontSize == o.fontSize;
    }
};

struct FormattedToken {
    QString baseText;
    QString rubyText;
    bool hasRuby = false;
    InlineStyle style;
};

std::vector<FormattedToken> parseFormattedTokens(const QString& line, qreal basePointSize);

enum class BalloonShape {
    Ellipse,      // 楕円吹き出し
    RoundedRect,  // 角丸四角形吹き出し
    Rectangle,    // 四角形（モノローグ）
    Spike,        // ウニフラッシュ / 叫び
    Cloud,        // モコモコ / 思考
    None          // 枠なし文字のみ
};

enum class WritingDirection {
    Horizontal = 0, // 横書き
    Vertical = 1    // 縦書き
};

struct TextBalloonItem {
    QString id;
    QRectF rect;                  // 吹き出し本体の矩形 (キャンバス座標)
    QString text;                 // 入力テキスト
    QFont font{"Yu Gothic", 16};  // フォント
    QColor textColor{Qt::black};  // 文字色
    QColor frameColor{Qt::black}; // 枠線色
    QColor fillColor{Qt::white};  // 背景色 (デフォルト白)
    float frameWidth = 3.0f;      // 枠線の太さ
    BalloonShape shape = BalloonShape::Ellipse; // 形状
    WritingDirection direction = WritingDirection::Horizontal; // 書字方向 (横書き / 縦書き)

    // しっぽパラメータ
    bool hasTail = true;          // しっぽの有無
    QPointF tailTip;              // しっぽの先端座標 (キャンバス座標)
    float tailBaseWidth = 24.0f;  // しっぽ根元の太さ (px)
    float tailCurvature = 0.0f;   // しっぽのカーブ曲率 (-100 ~ 100, 0:直線)
    bool isSelected = false;      // 選択状態

    // 特殊形状パラメータ (Spike / Cloud)
    int spikeCount = 16;          // トゲ / モコモコの個数 (4 ~ 64)
    float spikeDepth = 20.0f;     // トゲの深さ / モコモコの大きさ (px)
    float spikePhase = 0.0f;      // 形状の回転オフセット・位置 (度: 0 ~ 360)

    // フチ取りパラメータ
    bool hasOutline = false;          // フチ取りの有無
    float outlineWidth = 4.0f;        // フチ取りの太さ (px)
    QColor outlineColor{Qt::white};   // フチ取り色 (デフォルト白)

    Qt::Alignment horizontalAlign = Qt::AlignLeft;
    Qt::Alignment verticalAlign = Qt::AlignTop;
};

class TextLayer : public Layer {
public:
    TextLayer(const QString& name, int width, int height);
    ~TextLayer() override = default;

    std::vector<TextBalloonItem>& items() { return m_items; }
    const std::vector<TextBalloonItem>& items() const { return m_items; }

    int addItem(const TextBalloonItem& item);
    void removeItem(int index);
    void updateItem(int index, const TextBalloonItem& item);

    int findItemAt(const QPointF& docPos) const;

    // 全アイテムをタイルマネージャにラスタライズ描画
    void rasterizeAll();
    void translate(int dx, int dy) override;

private:
    std::vector<TextBalloonItem> m_items;
    void renderItem(const TextBalloonItem& item, QPainter& painter);
};

} // namespace Kaburagi
