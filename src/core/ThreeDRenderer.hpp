#pragma once

#include "ThreeDMath.hpp"
#include "ThreeDCamera.hpp"
#include "ThreeDModel.hpp"
#include <QImage>
#include <QColor>
#include <memory>
#include <vector>

namespace Kaburagi {

enum class ThreeDShadingMode {
    Cel = 0,         // アニメ調セルシェーディング (2段階陰影)
    Smooth = 1,      // スムーズ陰影 (ランバート/フォン)
    Flat = 2,        // フラットシェーディング
    WhiteModel = 3,  // 石膏像・陰影アタリ用 (純白モデル)
    LineOnly = 4     // 輪郭線のみ (線画アタリ用)
};

struct ThreeDLight {
    QVector3D direction = QVector3D(0.3f, -0.4f, -0.85f).normalized();
    QColor lightColor = Qt::white;
    QColor ambientColor = QColor(100, 100, 100);
    float intensity = 1.0f;
};

struct ThreeDRenderConfig {
    ThreeDShadingMode shadingMode = ThreeDShadingMode::Cel;
    bool drawEdges = true;
    float edgeWidth = 1.5f;
    QColor edgeColor = Qt::black;
    QColor backgroundColor = Qt::transparent;
    bool drawFloorGrid = true;
};

class ThreeDRenderer {
public:
    ThreeDRenderer();
    ~ThreeDRenderer() = default;

    // シーン描画
    QImage render(int width, int height,
                  const std::vector<std::shared_ptr<ThreeDModel>>& models,
                  const ThreeDCamera& camera,
                  const ThreeDLight& light,
                  const ThreeDRenderConfig& config);

    // 輪郭線（エッジ）のみを透明背景画像として抽出
    QImage extractContour(int width, int height,
                          const std::vector<std::shared_ptr<ThreeDModel>>& models,
                          const ThreeDCamera& camera,
                          float edgeWidth = 1.5f,
                          const QColor& edgeColor = Qt::black);

private:
    struct RenderBuffers {
        int width = 0;
        int height = 0;
        std::vector<float> depth;
        std::vector<QVector3D> normals;
        std::vector<uint32_t> pixels;

        void resize(int w, int h) {
            width = w;
            height = h;
            depth.assign(w * h, 1e9f);
            normals.assign(w * h, QVector3D(0, 0, 0));
            pixels.assign(w * h, 0x00000000);
        }
    };

    void rasterizeMesh(RenderBuffers& buf, const Mesh3D& mesh,
                       const std::vector<Material3D>& materials,
                       const QMatrix4x4& model,
                       const ThreeDCamera& camera, const ThreeDLight& light,
                       const ThreeDRenderConfig& config, float aspect);

    void applyEdgeDetection(RenderBuffers& buf, float edgeWidth, const QColor& edgeColor);

    RenderBuffers m_buffers;
    std::shared_ptr<ThreeDModel> m_floorGrid;
};

} // namespace Kaburagi
