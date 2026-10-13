#pragma once

#include "Layer.hpp"
#include "ThreeDMath.hpp"
#include "ThreeDCamera.hpp"
#include "ThreeDModel.hpp"
#include "ThreeDRenderer.hpp"
#include "ThreeDGpuRenderer.hpp"
#include <memory>
#include <vector>

namespace Kaburagi {

class ThreeDLayer : public Layer {
public:
    ThreeDLayer(const QString& name, int width, int height);
    ~ThreeDLayer() override = default;

    // カメラ
    ThreeDCamera& camera() { return m_camera; }
    const ThreeDCamera& camera() const { return m_camera; }

    // ライト
    ThreeDLight& light() { return m_light; }
    const ThreeDLight& light() const { return m_light; }

    // レンダリング設定
    ThreeDRenderConfig& renderConfig() { return m_renderConfig; }
    const ThreeDRenderConfig& renderConfig() const { return m_renderConfig; }

    // モデル管理
    const std::vector<std::shared_ptr<ThreeDModel>>& models() const { return m_models; }
    std::vector<std::shared_ptr<ThreeDModel>>& models() { return m_models; }
    void addModel(std::shared_ptr<ThreeDModel> model);
    void removeModel(int index);
    void clearModels();

    void markDirty() { m_isDirty = true; }

    // 輪郭線のみの画像抽出
    QImage extractContourImage() const;

    // レンダリング
    void renderToTarget(TileManager& target, const QRect& dirtyRect, const Layer* baseLayer = nullptr) override;
    QImage createThumbnail(int width = 36, int height = 28) const override;
    QRect contentBounds() const override;

    // キャッシュ画像
    const QImage& cachedImage() const { return m_cachedImage; }

    // GPUレンダリング切り替え
    bool useGpu() const { return m_useGpu; }
    void setUseGpu(bool use) { m_useGpu = use; markDirty(); }

private:
    void updateRenderCache();

private:
    int m_width;
    int m_height;
    ThreeDCamera m_camera;
    ThreeDLight m_light;
    ThreeDRenderConfig m_renderConfig;
    std::vector<std::shared_ptr<ThreeDModel>> m_models;
    bool m_useGpu = true;

    mutable ThreeDGpuRenderer m_gpuRenderer;
    mutable ThreeDRenderer m_renderer;
    mutable QImage m_cachedImage;
    mutable bool m_isDirty = true;
};

} // namespace Kaburagi
