#pragma once

#include "Layer.hpp"
#include <vector>
#include <memory>
#include <QPolygonF>
#include <QColor>

namespace Kaburagi {

class GroupLayer : public Layer, public std::enable_shared_from_this<GroupLayer> {
public:
    GroupLayer(const QString& name, int width, int height);
    ~GroupLayer() override = default;

    bool isExpanded() const { return m_expanded; }
    void setExpanded(bool exp) { m_expanded = exp; }

    bool isPassThrough() const { return m_passThrough; }
    void setPassThrough(bool pt) { m_passThrough = pt; }

    void addChild(const LayerPtr& child);
    void removeChild(const LayerPtr& child);
    void clearChildren();
    const std::vector<std::weak_ptr<Layer>>& children() const { return m_children; }
    bool containsChild(const LayerPtr& layer) const;

    // コマフォルダ (Comic Panel Folder) プロパティ
    bool isPanelFolder() const { return m_isPanelFolder; }
    void setPanelFolder(bool enable) { m_isPanelFolder = enable; }

    const QPolygonF& panelPolygon() const { return m_panelPolygon; }
    void setPanelPolygon(const QPolygonF& poly) { m_panelPolygon = poly; }

    float panelBorderWidth() const { return m_panelBorderWidth; }
    void setPanelBorderWidth(float w) { m_panelBorderWidth = w; }

    QColor panelBorderColor() const { return m_panelBorderColor; }
    void setPanelBorderColor(const QColor& c) { m_panelBorderColor = c; }

    bool drawPanelBorder() const { return m_drawPanelBorder; }
    void setDrawPanelBorder(bool draw) { m_drawPanelBorder = draw; }

    // コマ枠外のマスク処理と枠線描画
    void applyPanelMaskAndBorder(const QRect& dirtyRect);

    // グループバッファのクリア
    void clearTileBuffer(const QRect& dirtyRect);

    void renderToTarget(TileManager& target, const QRect& dirtyRect, const Layer* baseLayer = nullptr, bool clipToHiddenBase = true) override;
    void translate(int dx, int dy) override;
    QRect contentBounds() const override;

private:
    bool m_expanded = true;
    bool m_passThrough = false;
    std::vector<std::weak_ptr<Layer>> m_children;

    // コマ枠データ
    bool m_isPanelFolder = false;
    QPolygonF m_panelPolygon;
    float m_panelBorderWidth = 3.0f;
    QColor m_panelBorderColor{Qt::black};
    bool m_drawPanelBorder = true;
};

} // namespace Kaburagi
