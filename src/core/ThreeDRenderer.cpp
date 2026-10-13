#include "ThreeDRenderer.hpp"
#include <QtMath>
#include <algorithm>

namespace Kaburagi {

ThreeDRenderer::ThreeDRenderer() {
    m_floorGrid = ThreeDModel::createGrid(10.0f, 10);
}

static inline float edgeFunction(const QVector2D& a, const QVector2D& b, const QVector2D& c) {
    return (c.x() - a.x()) * (b.y() - a.y()) - (c.y() - a.y()) * (b.x() - a.x());
}

void ThreeDRenderer::rasterizeMesh(RenderBuffers& buf, const Mesh3D& mesh,
                                   const std::vector<Material3D>& materials,
                                   const QMatrix4x4& model,
                                   const ThreeDCamera& camera, const ThreeDLight& light,
                                   const ThreeDRenderConfig& config, float aspect)
{
    QMatrix4x4 view = camera.viewMatrix();
    QMatrix4x4 proj = camera.projectionMatrix(aspect);
    QMatrix4x4 mv = view * model;
    QMatrix4x4 mvp = proj * mv;
    QMatrix3x3 normalMatrix = model.normalMatrix();

    int w = buf.width;
    int h = buf.height;

    struct ScreenVert {
        QVector4D clip;
        QVector3D worldNormal;
        QVector2D screen;
        QVector2D texCoordInvW;
        float invW;
    };

    std::vector<ScreenVert> sVerts(mesh.vertices.size());
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        const auto& v = mesh.vertices[i];
        QVector4D clip = mvp * QVector4D(v.position, 1.0f);
        sVerts[i].clip = clip;
        sVerts[i].worldNormal = model.mapVector(v.normal).normalized();

        float invW = (std::abs(clip.w()) > 1e-6f) ? (1.0f / clip.w()) : 1.0f;
        sVerts[i].invW = invW;
        sVerts[i].texCoordInvW = v.texCoord * invW;

        float ndcX = clip.x() * invW;
        float ndcY = clip.y() * invW;
        sVerts[i].screen = QVector2D((ndcX + 1.0f) * 0.5f * w, (1.0f - ndcY) * 0.5f * h);
    }

    QVector3D lightDir = light.direction.normalized();

    const Material3D* mat = nullptr;
    if (mesh.materialIndex >= 0 && mesh.materialIndex < static_cast<int>(materials.size())) {
        mat = &materials[mesh.materialIndex];
    }
    bool hasTexture = (mat && !mat->diffuseTexture.isNull());
    QColor diffuseColor = (mat ? mat->diffuseColor : QColor(215, 220, 230));
    if (config.shadingMode == ThreeDShadingMode::WhiteModel) {
        diffuseColor = QColor(240, 240, 245);
        hasTexture = false;
    }

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const auto& v0 = sVerts[mesh.indices[i]];
        const auto& v1 = sVerts[mesh.indices[i + 1]];
        const auto& v2 = sVerts[mesh.indices[i + 2]];

        // ニアプレーンクリッピング簡易判定
        if (v0.clip.z() < -v0.clip.w() && v1.clip.z() < -v1.clip.w() && v2.clip.z() < -v2.clip.w()) continue;

        // バックフェースカリング
        float area = edgeFunction(v0.screen, v1.screen, v2.screen);
        if (area <= 0.0f) continue;
        float invArea = 1.0f / area;

        // 三角形バウンディングボックス
        int minX = std::clamp(static_cast<int>(std::floor(std::min({v0.screen.x(), v1.screen.x(), v2.screen.x()}))), 0, w - 1);
        int maxX = std::clamp(static_cast<int>(std::ceil(std::max({v0.screen.x(), v1.screen.x(), v2.screen.x()}))), 0, w - 1);
        int minY = std::clamp(static_cast<int>(std::floor(std::min({v0.screen.y(), v1.screen.y(), v2.screen.y()}))), 0, h - 1);
        int maxY = std::clamp(static_cast<int>(std::ceil(std::max({v0.screen.y(), v1.screen.y(), v2.screen.y()}))), 0, h - 1);

        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                QVector2D p(x + 0.5f, y + 0.5f);
                float w0 = edgeFunction(v1.screen, v2.screen, p) * invArea;
                float w1 = edgeFunction(v2.screen, v0.screen, p) * invArea;
                float w2 = edgeFunction(v0.screen, v1.screen, p) * invArea;

                if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                    float z = w0 * v0.clip.z() * v0.invW + w1 * v1.clip.z() * v1.invW + w2 * v2.clip.z() * v2.invW;
                    float invZ = w0 * v0.invW + w1 * v1.invW + w2 * v2.invW;
                    float depthVal = (std::abs(invZ) > 1e-6f) ? (z / invZ) : z;

                    int pixelIdx = y * w + x;
                    if (depthVal < buf.depth[pixelIdx]) {
                        QColor litColor = diffuseColor;
                        QColor shadeColor = (mat && mat->hasShadeColor) ? mat->shadeColor : QColor(220, 205, 215);

                        if (hasTexture) {
                            float u = (w0 * v0.texCoordInvW.x() + w1 * v1.texCoordInvW.x() + w2 * v2.texCoordInvW.x()) / invZ;
                            float v = (w0 * v0.texCoordInvW.y() + w1 * v1.texCoordInvW.y() + w2 * v2.texCoordInvW.y()) / invZ;

                            // UVラッピング (0.0 - 1.0)
                            u = u - std::floor(u);
                            v = v - std::floor(v);

                            const QImage& tex = mat->diffuseTexture;
                            int tx = std::clamp(static_cast<int>(u * tex.width()), 0, tex.width() - 1);
                            int ty = std::clamp(static_cast<int>(v * tex.height()), 0, tex.height() - 1);
                            QRgb sampleRgb = tex.pixel(tx, ty);

                            int texAlpha = qAlpha(sampleRgb);
                            if (texAlpha < 32) {
                                // アルファテスト: 透過ピクセル（まつ毛、毛先等）をディスカード
                                continue;
                            }

                            int rLit = (qRed(sampleRgb) * diffuseColor.red()) / 255;
                            int gLit = (qGreen(sampleRgb) * diffuseColor.green()) / 255;
                            int bLit = (qBlue(sampleRgb) * diffuseColor.blue()) / 255;
                            litColor = QColor(rLit, gLit, bLit);

                            // 陰色テクスチャまたは陰色のブレンド
                            if (mat && !mat->shadeTexture.isNull()) {
                                const QImage& sTex = mat->shadeTexture;
                                int stx = std::clamp(static_cast<int>(u * sTex.width()), 0, sTex.width() - 1);
                                int sty = std::clamp(static_cast<int>(v * sTex.height()), 0, sTex.height() - 1);
                                QRgb sSample = sTex.pixel(stx, sty);
                                int rShade = (qRed(sSample) * shadeColor.red()) / 255;
                                int gShade = (qGreen(sSample) * shadeColor.green()) / 255;
                                int bShade = (qBlue(sSample) * shadeColor.blue()) / 255;
                                shadeColor = QColor(rShade, gShade, bShade);
                            } else {
                                int rShade = (qRed(sampleRgb) * shadeColor.red()) / 255;
                                int gShade = (qGreen(sampleRgb) * shadeColor.green()) / 255;
                                int bShade = (qBlue(sampleRgb) * shadeColor.blue()) / 255;
                                shadeColor = QColor(rShade, gShade, bShade);
                            }
                        } else {
                            int rShade = (diffuseColor.red() * shadeColor.red()) / 255;
                            int gShade = (diffuseColor.green() * shadeColor.green()) / 255;
                            int bShade = (diffuseColor.blue() * shadeColor.blue()) / 255;
                            shadeColor = QColor(rShade, gShade, bShade);
                        }

                        buf.depth[pixelIdx] = depthVal;

                        QVector3D interpNormal = (w0 * v0.worldNormal + w1 * v1.worldNormal + w2 * v2.worldNormal).normalized();
                        buf.normals[pixelIdx] = interpNormal;

                        if (config.shadingMode == ThreeDShadingMode::LineOnly) {
                            // 線画のみモードでは背景を透明（または白）に保ち、後段のエッジ検出でのみ描画
                            buf.pixels[pixelIdx] = 0x00000000;
                            continue;
                        }

                        // 光と法線の内積 NoL (-1.0 ~ 1.0)
                        float NoL = QVector3D::dotProduct(interpNormal, -lightDir);

                        // シェーディング係数 (1.0: 日向, 0.0: 陰)
                        float shadowFactor = 1.0f;
                        if (config.shadingMode == ThreeDShadingMode::Cel) {
                            if (mat && (mat->hasShadeColor || !mat->shadeTexture.isNull() || mat->shadeShift != 0.0f)) {
                                // VRM MToon 公式陰影計算式
                                float shift = mat->shadeShift;
                                float toony = mat->shadeToony;
                                float thresholdLow = (shift - (1.0f - toony)) * 0.5f + 0.5f;
                                float thresholdHigh = (shift + (1.0f - toony)) * 0.5f + 0.5f;
                                float x = NoL * 0.5f + 0.5f;
                                if (thresholdHigh <= thresholdLow + 1e-4f) {
                                    shadowFactor = (x >= thresholdLow) ? 1.0f : 0.0f;
                                } else {
                                    shadowFactor = std::clamp((x - thresholdLow) / (thresholdHigh - thresholdLow), 0.0f, 1.0f);
                                }
                            } else {
                                // 一般モデル向けアニメ調セルシェーディング (柔らかな陰影階調)
                                if (NoL > 0.15f) {
                                    shadowFactor = 1.0f;
                                } else if (NoL > -0.10f) {
                                    shadowFactor = (NoL - (-0.10f)) / 0.25f;
                                } else {
                                    shadowFactor = 0.0f;
                                }
                            }
                        } else if (config.shadingMode == ThreeDShadingMode::WhiteModel) {
                            float diff = std::max(0.0f, NoL);
                            shadowFactor = (diff > 0.35f) ? 1.0f : (diff > 0.10f ? 0.75f : 0.50f);
                        } else if (config.shadingMode == ThreeDShadingMode::Smooth) {
                            shadowFactor = std::clamp(NoL * 0.5f + 0.5f, 0.0f, 1.0f);
                        }

                        int r = std::clamp(static_cast<int>(shadeColor.red() * (1.0f - shadowFactor) + litColor.red() * shadowFactor), 0, 255);
                        int g = std::clamp(static_cast<int>(shadeColor.green() * (1.0f - shadowFactor) + litColor.green() * shadowFactor), 0, 255);
                        int b = std::clamp(static_cast<int>(shadeColor.blue() * (1.0f - shadowFactor) + litColor.blue() * shadowFactor), 0, 255);
                        buf.pixels[pixelIdx] = 0xFF000000 | (r << 16) | (g << 8) | b;
                    }
                }
            }
        }
    }
}

void ThreeDRenderer::applyEdgeDetection(RenderBuffers& buf, float edgeWidth, const QColor& edgeColor) {
    int w = buf.width;
    int h = buf.height;
    std::vector<uint8_t> edgeMask(w * h, 0);

    uint32_t edgeArgb = static_cast<uint32_t>(edgeColor.rgba());

    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            int idx = y * w + x;
            float zCenter = buf.depth[idx];
            if (zCenter >= 1e8f) continue; // 背景

            float zRight = buf.depth[idx + 1];
            float zDown  = buf.depth[idx + w];

            const QVector3D& nCenter = buf.normals[idx];
            const QVector3D& nRight  = buf.normals[idx + 1];
            const QVector3D& nDown   = buf.normals[idx + w];

            bool isEdge = false;

            // 1. 深度の不連続性（シルエット / 遮蔽輪郭）
            float depthDiffR = std::abs(zCenter - zRight);
            float depthDiffD = std::abs(zCenter - zDown);
            if (depthDiffR > 0.15f || depthDiffD > 0.15f) {
                isEdge = true;
            }

            // 2. 法線の不連続性（鋭角クリース・服の折り目・稜線）
            if (!isEdge) {
                if (zRight < 1e8f && QVector3D::dotProduct(nCenter, nRight) < 0.65f) isEdge = true;
                if (zDown  < 1e8f && QVector3D::dotProduct(nCenter, nDown)  < 0.65f) isEdge = true;
            }

            if (isEdge) {
                edgeMask[idx] = 1;
            }
        }
    }

    // エッジ太さ (Dilation)
    int radius = std::clamp(static_cast<int>(std::round(edgeWidth * 0.8f)), 1, 4);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int idx = y * w + x;
            if (edgeMask[idx]) {
                for (int dy = -radius + 1; dy < radius; ++dy) {
                    int ny = y + dy;
                    if (ny < 0 || ny >= h) continue;
                    for (int dx = -radius + 1; dx < radius; ++dx) {
                        int nx = x + dx;
                        if (nx < 0 || nx >= w) continue;
                        buf.pixels[ny * w + nx] = edgeArgb;
                    }
                }
            }
        }
    }
}

QImage ThreeDRenderer::render(int width, int height,
                             const std::vector<std::shared_ptr<ThreeDModel>>& models,
                             const ThreeDCamera& camera,
                             const ThreeDLight& light,
                             const ThreeDRenderConfig& config)
{
    if (width <= 0 || height <= 0) return QImage();

    m_buffers.resize(width, height);
    float aspect = static_cast<float>(width) / std::max(1, height);

    // 地面グリッドの描画
    if (config.drawFloorGrid && m_floorGrid) {
        ThreeDRenderConfig gridCfg = config;
        gridCfg.shadingMode = ThreeDShadingMode::Flat;
        for (const auto& mesh : m_floorGrid->meshes()) {
            rasterizeMesh(m_buffers, mesh, m_floorGrid->materials(), m_floorGrid->modelMatrix(), camera, light, gridCfg, aspect);
        }
    }

    // 各モデルの描画
    for (const auto& model : models) {
        if (!model || !model->isVisible()) continue;
        QMatrix4x4 modelMat = model->modelMatrix();
        for (const auto& mesh : model->meshes()) {
            if (!mesh.visible) continue;
            rasterizeMesh(m_buffers, mesh, model->materials(), modelMat, camera, light, config, aspect);
        }
    }

    // 輪郭線（エッジ）検出・描画
    if (config.drawEdges || config.shadingMode == ThreeDShadingMode::LineOnly) {
        applyEdgeDetection(m_buffers, config.edgeWidth, config.edgeColor);
    }

    // QImageへコピー
    QImage result(width, height, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < height; ++y) {
        uint32_t* dstLine = reinterpret_cast<uint32_t*>(result.scanLine(y));
        const uint32_t* srcLine = &m_buffers.pixels[y * width];
        std::copy(srcLine, srcLine + width, dstLine);
    }

    return result;
}

QImage ThreeDRenderer::extractContour(int width, int height,
                                     const std::vector<std::shared_ptr<ThreeDModel>>& models,
                                     const ThreeDCamera& camera,
                                     float edgeWidth,
                                     const QColor& edgeColor)
{
    ThreeDRenderConfig config;
    config.shadingMode = ThreeDShadingMode::LineOnly;
    config.drawEdges = true;
    config.edgeWidth = edgeWidth;
    config.edgeColor = edgeColor;
    config.drawFloorGrid = false;

    ThreeDLight dummyLight;
    return render(width, height, models, camera, dummyLight, config);
}

} // namespace Kaburagi
