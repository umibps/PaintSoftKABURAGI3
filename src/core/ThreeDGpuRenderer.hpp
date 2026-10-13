#pragma once

#include "ThreeDMath.hpp"
#include "ThreeDCamera.hpp"
#include "ThreeDModel.hpp"
#include "ThreeDRenderer.hpp"
#include <QImage>
#include <QColor>
#include <memory>
#include <vector>
#include <unordered_map>

class QOpenGLContext;
class QOffscreenSurface;
class QOpenGLFramebufferObject;
class QOpenGLShaderProgram;
class QOpenGLTexture;
class QOpenGLVertexArrayObject;

namespace Kaburagi {

class ThreeDGpuRenderer {
public:
    ThreeDGpuRenderer();
    ~ThreeDGpuRenderer();

    // GPU環境が利用可能かどうか
    bool isAvailable() const { return m_initialized; }

    // シーン描画 (GPUオフスクリーンレンダリング -> QImage)
    QImage render(int width, int height,
                  const std::vector<std::shared_ptr<ThreeDModel>>& models,
                  const ThreeDCamera& camera,
                  const ThreeDLight& light,
                  const ThreeDRenderConfig& config);

    // 輪郭線のみの抽出
    QImage extractContour(int width, int height,
                          const std::vector<std::shared_ptr<ThreeDModel>>& models,
                          const ThreeDCamera& camera,
                          float edgeWidth = 1.5f,
                          const QColor& edgeColor = Qt::black);

private:
    bool initContext();
    void cleanup();
    bool initShaders();

    struct GpuMeshBuffer {
        std::shared_ptr<QOpenGLVertexArrayObject> vao;
        uint32_t vbo = 0;
        uint32_t ebo = 0;
        int indexCount = 0;
        bool hasSkinning = false;
        int vertexCount = 0;
    };

    struct GpuModelCache {
        std::vector<GpuMeshBuffer> meshBuffers;
        std::vector<std::shared_ptr<QOpenGLTexture>> textures;
        uint64_t modelId = 0;
    };

    void uploadModel(const ThreeDModel& model, GpuModelCache& cache);

    QOpenGLContext* m_context = nullptr;
    QOffscreenSurface* m_surface = nullptr;
    bool m_initialized = false;

    std::unique_ptr<QOpenGLShaderProgram> m_staticShader;
    std::unique_ptr<QOpenGLShaderProgram> m_skinningShader;
    std::unique_ptr<QOpenGLShaderProgram> m_outlineShader;
    std::unique_ptr<QOpenGLShaderProgram> m_skinningOutlineShader;

    std::unordered_map<const ThreeDModel*, GpuModelCache> m_modelCaches;

    std::shared_ptr<ThreeDModel> m_floorGrid;
};

} // namespace Kaburagi
