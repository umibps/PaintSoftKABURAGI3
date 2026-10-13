#include "ThreeDGpuRenderer.hpp"
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QDebug>
#include <cmath>
#include <iostream>

namespace Kaburagi {

static const char* kStaticVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

uniform mat4 u_viewProj;
uniform mat4 u_model;

out vec3 v_worldPos;
out vec3 v_worldNormal;
out vec2 v_texCoord;

void main() {
    vec4 worldPos = u_model * vec4(a_position, 1.0);
    v_worldPos = vec3(worldPos);
    v_worldNormal = normalize(mat3(u_model) * a_normal);
    v_texCoord = a_texCoord;
    gl_Position = u_viewProj * worldPos;
}
)";

static const char* kSkinningVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;
layout(location = 3) in vec4 a_joints;
layout(location = 4) in vec4 a_weights;

uniform mat4 u_viewProj;
uniform mat4 u_model;
uniform mat4 u_bones[200];

out vec3 v_worldPos;
out vec3 v_worldNormal;
out vec2 v_texCoord;

void main() {
    ivec4 j = ivec4(a_joints + 0.5);
    mat4 skinMat = u_bones[j.x] * a_weights.x +
                   u_bones[j.y] * a_weights.y +
                   u_bones[j.z] * a_weights.z +
                   u_bones[j.w] * a_weights.w;

    vec4 modelPos = skinMat * vec4(a_position, 1.0);
    vec3 modelNorm = mat3(skinMat) * a_normal;

    vec4 worldPos = u_model * modelPos;
    v_worldPos = vec3(worldPos);
    v_worldNormal = normalize(mat3(u_model) * modelNorm);
    v_texCoord = a_texCoord;
    gl_Position = u_viewProj * worldPos;
}
)";

static const char* kMToonFragmentShader = R"(
#version 330 core
in vec3 v_worldPos;
in vec3 v_worldNormal;
in vec2 v_texCoord;

uniform sampler2D u_mainTex;
uniform sampler2D u_shadeTex;
uniform bool u_hasMainTex;
uniform bool u_hasShadeTex;

uniform vec4 u_baseColor;
uniform vec4 u_shadeColor;
uniform float u_shadeShift;
uniform float u_shadeToony;
uniform bool u_isMToon;
uniform vec3 u_lightDir;
uniform vec3 u_lightColor;
uniform vec3 u_ambientColor;
uniform int u_shadingMode; // 0=Cel, 1=Smooth, 2=Flat, 3=WhiteModel, 4=LineOnly
uniform int u_blendMode;   // 0=Opaque, 1=Cutout, 2=Transparent, 3=TransparentWithZWrite
uniform float u_cutoff;

out vec4 fragColor;

void main() {
    if (u_shadingMode == 3) { // WhiteModel
        vec3 N = normalize(v_worldNormal);
        vec3 L = normalize(-u_lightDir);
        float ndotl = max(dot(N, L), 0.0);
        float shadowFactor = (ndotl > 0.35) ? 1.0 : (ndotl > 0.10 ? 0.75 : 0.50);
        vec3 col = vec3(0.92) * mix(u_ambientColor, u_lightColor, shadowFactor);
        fragColor = vec4(col, 1.0);
        return;
    }

    vec4 litColor = u_baseColor;
    if (u_hasMainTex) {
        litColor *= texture(u_mainTex, v_texCoord);
    }

    if (u_blendMode == 1) { // Cutout
        if (litColor.a < u_cutoff) discard;
        litColor.a = 1.0;
    } else if (u_blendMode == 0) { // Opaque
        litColor.a = 1.0;
    } else { // Transparent
        if (litColor.a < 0.005) discard;
    }

    vec4 shadeColor = u_shadeColor;
    if (u_hasShadeTex) {
        shadeColor *= texture(u_shadeTex, v_texCoord);
    } else if (u_hasMainTex) {
        shadeColor *= texture(u_mainTex, v_texCoord);
    }

    vec3 N = normalize(v_worldNormal);
    vec3 L = normalize(-u_lightDir);
    float dotNL = dot(N, L);

    float shadowFactor = 1.0;
    if (u_shadingMode == 0) { // Cel / MToon
        if (u_isMToon) {
            float thresholdLow = (u_shadeShift - (1.0 - u_shadeToony)) * 0.5 + 0.5;
            float thresholdHigh = (u_shadeShift + (1.0 - u_shadeToony)) * 0.5 + 0.5;
            float x = dotNL * 0.5 + 0.5;
            if (thresholdHigh <= thresholdLow + 0.0001) {
                shadowFactor = (x >= thresholdLow) ? 1.0 : 0.0;
            } else {
                shadowFactor = clamp((x - thresholdLow) / (thresholdHigh - thresholdLow), 0.0, 1.0);
            }
        } else {
            if (dotNL > 0.15) {
                shadowFactor = 1.0;
            } else if (dotNL > -0.10) {
                shadowFactor = (dotNL - (-0.10)) / 0.25;
            } else {
                shadowFactor = 0.0;
            }
        }
    } else if (u_shadingMode == 1) { // Smooth
        shadowFactor = clamp(dotNL * 0.5 + 0.5, 0.0, 1.0);
    }

    vec3 finalRgb = mix(shadeColor.rgb, litColor.rgb, shadowFactor);
    fragColor = vec4(finalRgb, litColor.a);
}
)";

static const char* kOutlineVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 3) in vec4 a_joints;
layout(location = 4) in vec4 a_weights;

uniform mat4 u_viewProj;
uniform mat4 u_model;
uniform mat4 u_bones[200];
uniform float u_outlineWidth;
uniform bool u_isSkinned;

void main() {
    vec4 pos;
    vec3 norm;
    if (u_isSkinned) {
        ivec4 j = ivec4(a_joints + 0.5);
        mat4 skinMat = u_bones[j.x] * a_weights.x +
                       u_bones[j.y] * a_weights.y +
                       u_bones[j.z] * a_weights.z +
                       u_bones[j.w] * a_weights.w;
        pos = skinMat * vec4(a_position, 1.0);
        norm = normalize(mat3(skinMat) * a_normal);
    } else {
        pos = vec4(a_position, 1.0);
        norm = normalize(a_normal);
    }

    vec4 worldPos = u_model * pos;
    vec3 worldNorm = normalize(mat3(u_model) * norm);

    worldPos.xyz += worldNorm * u_outlineWidth;
    gl_Position = u_viewProj * worldPos;
}
)";

static const char* kOutlineFragmentShader = R"(
#version 330 core
uniform vec4 u_outlineColor;
out vec4 fragColor;
void main() {
    fragColor = u_outlineColor;
}
)";

ThreeDGpuRenderer::ThreeDGpuRenderer() {
    m_floorGrid = ThreeDModel::createGrid(10.0f, 10);
    initContext();
}

ThreeDGpuRenderer::~ThreeDGpuRenderer() {
    cleanup();
}

bool ThreeDGpuRenderer::initContext() {
    if (m_initialized) return true;

    m_context = new QOpenGLContext();
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    m_context->setFormat(format);

    if (!m_context->create()) {
        std::cout << "[GPU ERROR] m_context->create() failed!" << std::endl;
        delete m_context;
        m_context = nullptr;
        return false;
    }

    m_surface = new QOffscreenSurface();
    m_surface->setFormat(m_context->format());
    m_surface->create();

    if (!m_context->makeCurrent(m_surface)) {
        std::cout << "[GPU ERROR] makeCurrent failed!" << std::endl;
        cleanup();
        return false;
    }

    if (!initShaders()) {
        std::cout << "[GPU ERROR] initShaders failed!" << std::endl;
        m_context->doneCurrent();
        cleanup();
        return false;
    }

    m_context->doneCurrent();
    m_initialized = true;
    std::cout << "[GPU INFO] ThreeDGpuRenderer initialized successfully!" << std::endl;
    return true;
}

void ThreeDGpuRenderer::cleanup() {
    if (m_context && m_surface) {
        m_context->makeCurrent(m_surface);
        m_staticShader.reset();
        m_skinningShader.reset();
        m_outlineShader.reset();
        m_skinningOutlineShader.reset();

        auto gl = m_context->functions();
        for (auto& [mdl, cache] : m_modelCaches) {
            for (auto& buf : cache.meshBuffers) {
                if (buf.vbo) gl->glDeleteBuffers(1, &buf.vbo);
                if (buf.ebo) gl->glDeleteBuffers(1, &buf.ebo);
            }
            cache.meshBuffers.clear();
            cache.textures.clear();
        }
        m_modelCaches.clear();
        m_context->doneCurrent();
    }

    delete m_surface;
    m_surface = nullptr;
    delete m_context;
    m_context = nullptr;
    m_initialized = false;
}

bool ThreeDGpuRenderer::initShaders() {
    m_staticShader = std::make_unique<QOpenGLShaderProgram>();
    if (!m_staticShader->addShaderFromSourceCode(QOpenGLShader::Vertex, kStaticVertexShader) ||
        !m_staticShader->addShaderFromSourceCode(QOpenGLShader::Fragment, kMToonFragmentShader) ||
        !m_staticShader->link()) {
        std::cout << "[GPU ERROR] Failed to link static shader:\n" << m_staticShader->log().toStdString() << std::endl;
        return false;
    }

    m_skinningShader = std::make_unique<QOpenGLShaderProgram>();
    if (!m_skinningShader->addShaderFromSourceCode(QOpenGLShader::Vertex, kSkinningVertexShader) ||
        !m_skinningShader->addShaderFromSourceCode(QOpenGLShader::Fragment, kMToonFragmentShader) ||
        !m_skinningShader->link()) {
        std::cout << "[GPU ERROR] Failed to link skinning shader:\n" << m_skinningShader->log().toStdString() << std::endl;
        return false;
    }

    m_outlineShader = std::make_unique<QOpenGLShaderProgram>();
    if (!m_outlineShader->addShaderFromSourceCode(QOpenGLShader::Vertex, kOutlineVertexShader) ||
        !m_outlineShader->addShaderFromSourceCode(QOpenGLShader::Fragment, kOutlineFragmentShader) ||
        !m_outlineShader->link()) {
        std::cout << "[GPU ERROR] Failed to link outline shader:\n" << m_outlineShader->log().toStdString() << std::endl;
        return false;
    }

    return true;
}

#pragma pack(push, 1)
struct GpuVertex {
    float pos[3];
    float norm[3];
    float uv[2];
    float joints[4];
    float weights[4];
};
#pragma pack(pop)

void ThreeDGpuRenderer::uploadModel(const ThreeDModel& model, GpuModelCache& cache) {
    auto gl = m_context->functions();

    for (auto& buf : cache.meshBuffers) {
        if (buf.vbo) gl->glDeleteBuffers(1, &buf.vbo);
        if (buf.ebo) gl->glDeleteBuffers(1, &buf.ebo);
    }
    cache.meshBuffers.clear();
    cache.textures.clear();

    // テクスチャアップロード
    for (const auto& mat : model.materials()) {
        if (!mat.diffuseTexture.isNull()) {
            auto tex = std::make_shared<QOpenGLTexture>(mat.diffuseTexture);
            tex->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
            tex->setMagnificationFilter(QOpenGLTexture::Linear);
            tex->setWrapMode(QOpenGLTexture::Repeat);
            cache.textures.push_back(tex);
        } else {
            cache.textures.push_back(nullptr);
        }
    }

    // メッシュVAO/VBO/EBO作成
    for (size_t mIdx = 0; mIdx < model.meshes().size(); ++mIdx) {
        const auto& mesh = model.meshes()[mIdx];
        GpuMeshBuffer buf;
        buf.indexCount = static_cast<int>(mesh.indices.size());
        buf.vertexCount = static_cast<int>(mesh.vertices.size());
        buf.hasSkinning = (!mesh.skinWeights.empty() && mesh.skinWeights.size() == mesh.vertices.size());

        const auto& srcVertices = (buf.hasSkinning && !mesh.bindVertices.empty()) ? mesh.bindVertices : mesh.vertices;

        std::vector<GpuVertex> gpuVertices(srcVertices.size());
        for (size_t v = 0; v < srcVertices.size(); ++v) {
            auto& gv = gpuVertices[v];
            gv.pos[0] = srcVertices[v].position.x();
            gv.pos[1] = srcVertices[v].position.y();
            gv.pos[2] = srcVertices[v].position.z();
            gv.norm[0] = srcVertices[v].normal.x();
            gv.norm[1] = srcVertices[v].normal.y();
            gv.norm[2] = srcVertices[v].normal.z();
            gv.uv[0] = srcVertices[v].texCoord.x();
            gv.uv[1] = srcVertices[v].texCoord.y();

            if (buf.hasSkinning) {
                const auto& sw = mesh.skinWeights[v];
                for (int k = 0; k < 4; ++k) {
                    gv.joints[k] = static_cast<float>(sw.joints[k]);
                    gv.weights[k] = sw.weights[k];
                }
            } else {
                gv.joints[0] = 0.0f; gv.joints[1] = 0.0f; gv.joints[2] = 0.0f; gv.joints[3] = 0.0f;
                gv.weights[0] = 1.0f; gv.weights[1] = 0.0f; gv.weights[2] = 0.0f; gv.weights[3] = 0.0f;
            }
        }

        buf.vao = std::make_shared<QOpenGLVertexArrayObject>();
        buf.vao->create();
        buf.vao->bind();

        gl->glGenBuffers(1, &buf.vbo);
        gl->glBindBuffer(GL_ARRAY_BUFFER, buf.vbo);
        gl->glBufferData(GL_ARRAY_BUFFER, gpuVertices.size() * sizeof(GpuVertex), gpuVertices.data(), GL_STATIC_DRAW);

        gl->glGenBuffers(1, &buf.ebo);
        gl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf.ebo);
        gl->glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(uint32_t), mesh.indices.data(), GL_STATIC_DRAW);

        // 属性設定
        gl->glEnableVertexAttribArray(0);
        gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), reinterpret_cast<void*>(offsetof(GpuVertex, pos)));

        gl->glEnableVertexAttribArray(1);
        gl->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), reinterpret_cast<void*>(offsetof(GpuVertex, norm)));

        gl->glEnableVertexAttribArray(2);
        gl->glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), reinterpret_cast<void*>(offsetof(GpuVertex, uv)));

        gl->glEnableVertexAttribArray(3);
        gl->glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), reinterpret_cast<void*>(offsetof(GpuVertex, joints)));

        gl->glEnableVertexAttribArray(4);
        gl->glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), reinterpret_cast<void*>(offsetof(GpuVertex, weights)));

        buf.vao->release();
        gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
        gl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        cache.meshBuffers.push_back(buf);
    }
}

QImage ThreeDGpuRenderer::render(int width, int height,
                                 const std::vector<std::shared_ptr<ThreeDModel>>& models,
                                 const ThreeDCamera& camera,
                                 const ThreeDLight& light,
                                 const ThreeDRenderConfig& config) {
    if (!initContext()) return QImage();
    if (width <= 0 || height <= 0) return QImage();

    if (!m_context->makeCurrent(m_surface)) return QImage();

    auto gl = m_context->functions();

    QOpenGLFramebufferObjectFormat fboFormat;
    fboFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    QOpenGLFramebufferObject fbo(width, height, fboFormat);
    if (!fbo.isValid()) {
        m_context->doneCurrent();
        return QImage();
    }

    fbo.bind();
    gl->glViewport(0, 0, width, height);

    // 背景クリア (透過)
    gl->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    gl->glEnable(GL_DEPTH_TEST);
    gl->glDepthFunc(GL_LEQUAL);
    gl->glEnable(GL_BLEND);
    gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // カメラ行列
    float aspect = static_cast<float>(width) / static_cast<float>(height);
    QMatrix4x4 proj = camera.projectionMatrix(aspect);
    QMatrix4x4 view = camera.viewMatrix();
    QMatrix4x4 viewProj = proj * view;

    // ライトパラメータ
    QVector3D lightDir = light.direction.normalized();
    QVector3D lightCol(light.lightColor.redF() * light.intensity,
                       light.lightColor.greenF() * light.intensity,
                       light.lightColor.blueF() * light.intensity);
    QVector3D ambCol(light.ambientColor.redF(),
                     light.ambientColor.greenF(),
                     light.ambientColor.blueF());

    struct RenderItem {
        const ThreeDModel* model = nullptr;
        int meshIndex = 0;
        int renderQueue = 2000;
        int blendMode = 0;
        int cullMode = 2;
        float alphaCutoff = 0.5f;
        bool isTransparent = false;
        const Material3D* mat = nullptr;
        int originalOrder = 0;
    };

    std::vector<RenderItem> opaqueItems;
    std::vector<RenderItem> transparentItems;
    int orderCounter = 0;

    auto collectItems = [&](const ThreeDModel& mdl) {
        auto itCache = m_modelCaches.find(&mdl);
        if (itCache == m_modelCaches.end()) {
            uploadModel(mdl, m_modelCaches[&mdl]);
        }
        for (size_t mIdx = 0; mIdx < mdl.meshes().size(); ++mIdx) {
            const auto& mesh = mdl.meshes()[mIdx];
            if (!mesh.visible) continue;
            RenderItem item;
            item.model = &mdl;
            item.meshIndex = static_cast<int>(mIdx);
            item.originalOrder = orderCounter++;

            if (mesh.materialIndex >= 0 && mesh.materialIndex < static_cast<int>(mdl.materials().size())) {
                item.mat = &mdl.materials()[mesh.materialIndex];
                item.renderQueue = item.mat->renderQueue;
                item.blendMode = item.mat->blendMode;
                item.cullMode = item.mat->cullMode;
                item.alphaCutoff = item.mat->alphaCutoff;
                item.isTransparent = (item.renderQueue >= 2500 || item.blendMode >= 2);
            } else {
                item.renderQueue = 2000;
                item.blendMode = 0;
                item.cullMode = 2;
                item.alphaCutoff = 0.5f;
                item.isTransparent = false;
            }

            if (item.isTransparent) {
                transparentItems.push_back(item);
            } else {
                opaqueItems.push_back(item);
            }
        }
    };

    for (const auto& mdl : models) {
        if (!mdl || !mdl->isVisible()) continue;
        collectItems(*mdl);
    }

    // 不透明アイテムは RenderQueue 昇順 (同一なら元の配列順)
    std::sort(opaqueItems.begin(), opaqueItems.end(), [](const RenderItem& a, const RenderItem& b) {
        if (a.renderQueue != b.renderQueue) return a.renderQueue < b.renderQueue;
        return a.originalOrder < b.originalOrder;
    });

    // 半透明アイテムも RenderQueue 昇順 (同一なら元の配列順)
    std::sort(transparentItems.begin(), transparentItems.end(), [](const RenderItem& a, const RenderItem& b) {
        if (a.renderQueue != b.renderQueue) return a.renderQueue < b.renderQueue;
        return a.originalOrder < b.originalOrder;
    });

    auto applyCullMode = [&](int cullMode) {
        if (cullMode == 0) {
            gl->glDisable(GL_CULL_FACE);
        } else if (cullMode == 1) {
            gl->glEnable(GL_CULL_FACE);
            gl->glCullFace(GL_FRONT);
        } else { // 2 = Back
            gl->glEnable(GL_CULL_FACE);
            gl->glCullFace(GL_BACK);
        }
    };

    auto renderItem = [&](const RenderItem& item, bool isOutlinePass) {
        const auto& mdl = *item.model;
        auto& cache = m_modelCaches[&mdl];
        int mIdx = item.meshIndex;
        if (mIdx >= static_cast<int>(cache.meshBuffers.size())) return;
        const auto& buf = cache.meshBuffers[mIdx];
        if (buf.indexCount == 0 || !buf.vao) return;

        const Material3D* mat = item.mat;

        QOpenGLShaderProgram* prog = nullptr;
        if (isOutlinePass) {
            prog = m_outlineShader.get();
        } else {
            prog = buf.hasSkinning ? m_skinningShader.get() : m_staticShader.get();
        }

        prog->bind();
        prog->setUniformValue("u_viewProj", viewProj);
        prog->setUniformValue("u_model", mdl.modelMatrix());

        // ボーン行列の転送 (最大200ボーン)
        if (buf.hasSkinning) {
            std::vector<QMatrix4x4> skinMats = mdl.getMeshSkinMatrices(mIdx);
            if (skinMats.empty()) {
                skinMats.resize(1);
                skinMats[0].setToIdentity();
            }
            int count = std::min(static_cast<int>(skinMats.size()), 200);
            prog->setUniformValueArray("u_bones", skinMats.data(), count);
        }

        if (isOutlinePass) {
            prog->setUniformValue("u_outlineWidth", config.edgeWidth * 0.003f);
            prog->setUniformValue("u_isSkinned", buf.hasSkinning);
            prog->setUniformValue("u_outlineColor", QVector4D(
                config.edgeColor.redF(), config.edgeColor.greenF(),
                config.edgeColor.blueF(), config.edgeColor.alphaF()));
        } else {
            prog->setUniformValue("u_shadingMode", static_cast<int>(config.shadingMode));
            prog->setUniformValue("u_lightDir", lightDir);
            prog->setUniformValue("u_lightColor", lightCol);
            prog->setUniformValue("u_ambientColor", ambCol);
            prog->setUniformValue("u_blendMode", item.blendMode);
            prog->setUniformValue("u_cutoff", item.alphaCutoff);

            if (mat) {
                QColor baseC = mat->diffuseColor;
                prog->setUniformValue("u_baseColor", QVector4D(baseC.redF(), baseC.greenF(), baseC.blueF(), baseC.alphaF()));

                QColor shadeC = mat->hasShadeColor && mat->shadeColor.isValid() ? mat->shadeColor : QColor(
                    std::round(baseC.red() * 0.7f), std::round(baseC.green() * 0.7f), std::round(baseC.blue() * 0.7f));
                prog->setUniformValue("u_shadeColor", QVector4D(shadeC.redF(), shadeC.greenF(), shadeC.blueF(), 1.0f));

                prog->setUniformValue("u_shadeShift", mat->shadeShift);
                prog->setUniformValue("u_shadeToony", mat->shadeToony);
                bool isMToon = (mat->hasShadeColor || !mat->shadeTexture.isNull() || mat->shadeShift != 0.0f);
                prog->setUniformValue("u_isMToon", isMToon);

                if (mdl.meshes()[mIdx].materialIndex >= 0 && mdl.meshes()[mIdx].materialIndex < static_cast<int>(cache.textures.size()) && cache.textures[mdl.meshes()[mIdx].materialIndex]) {
                    cache.textures[mdl.meshes()[mIdx].materialIndex]->bind(0);
                    prog->setUniformValue("u_mainTex", 0);
                    prog->setUniformValue("u_hasMainTex", true);
                } else {
                    prog->setUniformValue("u_hasMainTex", false);
                }
                prog->setUniformValue("u_hasShadeTex", false);
            } else {
                prog->setUniformValue("u_baseColor", QVector4D(0.8f, 0.8f, 0.8f, 1.0f));
                prog->setUniformValue("u_shadeColor", QVector4D(0.5f, 0.5f, 0.5f, 1.0f));
                prog->setUniformValue("u_shadeShift", 0.0f);
                prog->setUniformValue("u_shadeToony", 0.9f);
                prog->setUniformValue("u_isMToon", false);
                prog->setUniformValue("u_hasMainTex", false);
                prog->setUniformValue("u_hasShadeTex", false);
            }
        }

        // VAOをバインドして描画
        buf.vao->bind();
        gl->glDrawElements(GL_TRIANGLES, buf.indexCount, GL_UNSIGNED_INT, nullptr);
        buf.vao->release();

        prog->release();
    };

    // 1. 不透明・カットアウト描画パス (Opaque & Cutout, ZWrite 有効, Blend 無効)
    gl->glDepthMask(GL_TRUE);
    gl->glDisable(GL_BLEND);
    for (const auto& item : opaqueItems) {
        applyCullMode(item.cullMode);
        renderItem(item, false);
    }

    // 2. 輪郭線パス (不透明・カットアウトのみ対象、表面カリングによる背面押し出し)
    if (config.drawEdges && config.edgeWidth > 0.05f) {
        gl->glDepthMask(GL_TRUE);
        gl->glDisable(GL_BLEND);
        gl->glEnable(GL_CULL_FACE);
        gl->glCullFace(GL_FRONT);
        for (const auto& item : opaqueItems) {
            renderItem(item, true);
        }
        gl->glDisable(GL_CULL_FACE);
    }

    // 3. 半透明描画パス (眉毛、まつ毛、アイライン、瞳ハイライト等: ZWrite 無効, Blend 有効)
    if (!transparentItems.empty()) {
        gl->glDepthMask(GL_FALSE);
        gl->glEnable(GL_BLEND);
        gl->glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        for (const auto& item : transparentItems) {
            applyCullMode(item.cullMode);
            renderItem(item, false);
        }
        gl->glDepthMask(GL_TRUE);
    }

    // 4. 床グリッド描画
    if (config.drawFloorGrid && m_floorGrid) {
        auto itCache = m_modelCaches.find(m_floorGrid.get());
        if (itCache == m_modelCaches.end()) {
            uploadModel(*m_floorGrid, m_modelCaches[m_floorGrid.get()]);
        }
        gl->glDisable(GL_CULL_FACE);
        gl->glDepthMask(GL_TRUE);
        gl->glDisable(GL_BLEND);
        RenderItem gridItem;
        gridItem.model = m_floorGrid.get();
        gridItem.meshIndex = 0;
        gridItem.cullMode = 0;
        gridItem.blendMode = 0;
        renderItem(gridItem, false);
    }

    fbo.release();
    QImage res = fbo.toImage();
    m_context->doneCurrent();

    return res;
}

QImage ThreeDGpuRenderer::extractContour(int width, int height,
                                         const std::vector<std::shared_ptr<ThreeDModel>>& models,
                                         const ThreeDCamera& camera,
                                         float edgeWidth,
                                         const QColor& edgeColor) {
    ThreeDRenderConfig cfg;
    cfg.drawEdges = true;
    cfg.edgeWidth = edgeWidth;
    cfg.edgeColor = edgeColor;
    cfg.drawFloorGrid = false;
    cfg.shadingMode = ThreeDShadingMode::LineOnly;

    return render(width, height, models, camera, ThreeDLight(), cfg);
}

} // namespace Kaburagi
