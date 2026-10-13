#include "ThreeDLayer.hpp"
#include <QPainter>

namespace Kaburagi {

ThreeDLayer::ThreeDLayer(const QString& name, int width, int height)
    : Layer(name, width, height, LayerType::ThreeD)
    , m_width(width)
    , m_height(height)
{
}

void ThreeDLayer::addModel(std::shared_ptr<ThreeDModel> model) {
    if (model) {
        m_models.push_back(model);
        markDirty();
    }
}

void ThreeDLayer::removeModel(int index) {
    if (index >= 0 && index < static_cast<int>(m_models.size())) {
        m_models.erase(m_models.begin() + index);
        markDirty();
    }
}

void ThreeDLayer::clearModels() {
    m_models.clear();
    markDirty();
}

void ThreeDLayer::updateRenderCache() {
    QImage rendered;
    if (m_useGpu && m_gpuRenderer.isAvailable()) {
        rendered = m_gpuRenderer.render(m_width, m_height, m_models, m_camera, m_light, m_renderConfig);
    }
    if (rendered.isNull()) {
        // GPU不可または失敗時のCPUソフトウェアフォールバック
        rendered = m_renderer.render(m_width, m_height, m_models, m_camera, m_light, m_renderConfig);
    }
    m_cachedImage = rendered;

    // タイルマネージャにピクセルを反映
    m_tileManager.clear();
    for (int y = 0; y < m_height; ++y) {
        const uint32_t* srcLine = reinterpret_cast<const uint32_t*>(m_cachedImage.constScanLine(y));
        for (int x = 0; x < m_width; ++x) {
            uint32_t c = srcLine[x];
            if ((c & 0xFF000000) != 0) {
                m_tileManager.setPixel(x, y, c);
            }
        }
    }

    m_isDirty = false;
    invalidateContentBounds();
}

void ThreeDLayer::renderToTarget(TileManager& target, const QRect& dirtyRect, const Layer* baseLayer) {
    if (!effectiveVisible()) return;

    if (m_isDirty || m_cachedImage.isNull()) {
        updateRenderCache();
    }

    Layer::renderToTarget(target, dirtyRect, baseLayer);
}

QImage ThreeDLayer::createThumbnail(int width, int height) const {
    if (m_isDirty || m_cachedImage.isNull()) {
        const_cast<ThreeDLayer*>(this)->updateRenderCache();
    }
    if (m_cachedImage.isNull()) {
        return Layer::createThumbnail(width, height);
    }
    return m_cachedImage.scaled(width, height, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QRect ThreeDLayer::contentBounds() const {
    return QRect(0, 0, m_width, m_height);
}

QImage ThreeDLayer::extractContourImage() const {
    QImage res;
    if (m_useGpu && m_gpuRenderer.isAvailable()) {
        res = m_gpuRenderer.extractContour(m_width, m_height, m_models, m_camera,
                                          m_renderConfig.edgeWidth, m_renderConfig.edgeColor);
    }
    if (res.isNull()) {
        res = m_renderer.extractContour(m_width, m_height, m_models, m_camera,
                                        m_renderConfig.edgeWidth, m_renderConfig.edgeColor);
    }
    return res;
}

} // namespace Kaburagi
