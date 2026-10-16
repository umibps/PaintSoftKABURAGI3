#pragma once

#include "TileManager.hpp"
#include "BlendModes.hpp"
#include <QString>
#include <QImage>
#include <QJsonObject>
#include <memory>

namespace Kaburagi {

enum class LayerType {
    Raster,
    Vector,
    Adjustment,
    Group,
    Text,
    ThreeD
};

class GroupLayer;

class Layer {
public:
    Layer(const QString& name, int width, int height, LayerType type = LayerType::Raster);
    virtual ~Layer() = default;

    LayerType type() const { return m_type; }
    const QString& name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    bool isVisible() const { return m_visible; }
    void setVisible(bool v) { m_visible = v; }

    float opacity() const { return m_opacity; }
    void setOpacity(float op) { m_opacity = std::max(0.0f, std::min(1.0f, op)); }

    BlendMode blendMode() const { return m_blendMode; }
    void setBlendMode(BlendMode mode) { m_blendMode = mode; }

    bool isClippingMask() const { return m_isClippingMask; }
    void setClippingMask(bool clip) { m_isClippingMask = clip; }

    bool isAlphaLocked() const { return m_isAlphaLocked; }
    void setAlphaLocked(bool lock) { m_isAlphaLocked = lock; }

    bool isPinned() const { return m_isPinned; }
    void setPinned(bool pin) { m_isPinned = pin; }

    bool isLocked() const { return m_isLocked; }
    void setLocked(bool lock) { m_isLocked = lock; }

    bool hasAiParameters() const { return !m_aiParameters.isEmpty(); }
    const QJsonObject& aiParameters() const { return m_aiParameters; }
    void setAiParameters(const QJsonObject& params) { m_aiParameters = params; }

    TileManager& tileManager() { return m_tileManager; }
    const TileManager& tileManager() const { return m_tileManager; }

    std::shared_ptr<GroupLayer> parentGroup() const;
    void setParentGroup(const std::shared_ptr<GroupLayer>& group);

    bool effectiveVisible() const;
    float effectiveOpacity() const;

    virtual void renderToTarget(TileManager& target, const QRect& dirtyRect, const Layer* baseLayer = nullptr, bool clipToHiddenBase = true);
    virtual void translate(int dx, int dy);
    virtual QImage createThumbnail(int width = 36, int height = 28) const;

    // レイヤー使用範囲（バウンディング矩形）の取得と記憶
    virtual QRect contentBounds() const;
    void invalidateContentBounds() const { m_contentBoundsValid = false; }
    void updateContentBounds(const QRect& dirtyRect) const {
        if (m_contentBoundsValid) {
            m_cachedContentBounds = m_cachedContentBounds.united(dirtyRect);
        }
    }

protected:
    LayerType m_type;
    QString m_name;
    bool m_visible = true;
    float m_opacity = 1.0f;
    BlendMode m_blendMode = BlendMode::Normal;
    bool m_isClippingMask = false;
    bool m_isAlphaLocked = false;
    bool m_isPinned = false;
    bool m_isLocked = false;
    QJsonObject m_aiParameters;

    std::weak_ptr<GroupLayer> m_parentGroup;

    TileManager m_tileManager;
    mutable QRect m_cachedContentBounds;
    mutable bool m_contentBoundsValid = false;
};

using LayerPtr = std::shared_ptr<Layer>;

} // namespace Kaburagi
