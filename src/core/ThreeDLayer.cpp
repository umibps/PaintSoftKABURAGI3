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

void ThreeDLayer::renderToTarget(TileManager& target, const QRect& dirtyRect, const Layer* baseLayer, bool clipToHiddenBase) {
    if (!effectiveVisible()) return;

    if (m_isDirty || m_cachedImage.isNull()) {
        updateRenderCache();
    }

    Layer::renderToTarget(target, dirtyRect, baseLayer, clipToHiddenBase);
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

QImage ThreeDLayer::extractOpenPoseImage(int width, int height) const {
    int w = (width > 0) ? width : m_width;
    int h = (height > 0) ? height : m_height;
    if (w <= 0 || h <= 0) return QImage();

    // OpenPose画像は黒背景
    QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // OpenPose COCO 18 limbs (ボーン接続定義)
    struct Limb {
        int j1, j2;
        QColor color;
    };
    static const Limb limbs[] = {
        { 1, 2, QColor(255, 0, 0) },      // Neck - RShoulder
        { 1, 5, QColor(255, 85, 0) },     // Neck - LShoulder
        { 2, 3, QColor(255, 170, 0) },    // RShoulder - RElbow
        { 3, 4, QColor(255, 255, 0) },    // RElbow - RWrist
        { 5, 6, QColor(170, 255, 0) },    // LShoulder - LElbow
        { 6, 7, QColor(85, 255, 0) },     // LElbow - LWrist
        { 1, 8, QColor(0, 255, 0) },      // Neck - RHip
        { 8, 9, QColor(0, 255, 85) },     // RHip - RKnee
        { 9, 10, QColor(0, 255, 170) },   // RKnee - RAnkle
        { 1, 11, QColor(0, 255, 255) },   // Neck - LHip
        { 11, 12, QColor(0, 170, 255) },  // LHip - LKnee
        { 12, 13, QColor(0, 85, 255) },   // LKnee - LAnkle
        { 1, 0, QColor(0, 0, 255) },      // Neck - Nose
        { 0, 14, QColor(85, 0, 255) },    // Nose - REye
        { 14, 16, QColor(170, 0, 255) },  // REye - REar
        { 0, 15, QColor(255, 0, 255) },   // Nose - LEye
        { 15, 17, QColor(255, 0, 170) }   // LEye - LEar
    };

    static const QColor jointColors[18] = {
        QColor(255, 0, 0),     // 0: Nose
        QColor(255, 85, 0),    // 1: Neck
        QColor(255, 170, 0),   // 2: RShoulder
        QColor(255, 255, 0),   // 3: RElbow
        QColor(170, 255, 0),   // 4: RWrist
        QColor(85, 255, 0),    // 5: LShoulder
        QColor(0, 255, 0),     // 6: LElbow
        QColor(0, 255, 85),    // 7: LWrist
        QColor(0, 255, 170),   // 8: RHip
        QColor(0, 255, 255),   // 9: RKnee
        QColor(0, 170, 255),   // 10: RAnkle
        QColor(0, 85, 255),    // 11: LHip
        QColor(0, 0, 255),     // 12: LKnee
        QColor(85, 0, 255),    // 13: LAnkle
        QColor(170, 0, 255),   // 14: REye
        QColor(255, 0, 255),   // 15: LEye
        QColor(255, 0, 170),   // 16: REar
        QColor(255, 0, 85)     // 17: LEar
    };

    float aspectRatio = static_cast<float>(w) / static_cast<float>(h);
    QMatrix4x4 vpMatrix = m_camera.projectionMatrix(aspectRatio) * m_camera.viewMatrix();

    float baseScale = std::max(2.0f, static_cast<float>(std::min(w, h)) / 140.0f);
    float boneWidth = baseScale * 1.5f;
    float jointRadius = baseScale * 1.8f;

    for (const auto& model : m_models) {
        if (!model) continue;
        auto joints = model->getOpenPoseJoints();
        if (joints.size() < 18) continue;

        struct ScreenJoint {
            bool valid = false;
            QPointF pt;
        };
        std::vector<ScreenJoint> screenJoints(18);

        for (int i = 0; i < 18; ++i) {
            if (!joints[i].valid) continue;
            QVector4D clipPos = vpMatrix.map(QVector4D(joints[i].worldPos, 1.0f));
            if (clipPos.w() <= 0.001f) continue; // カメラ後方
            float ndcX = clipPos.x() / clipPos.w();
            float ndcY = clipPos.y() / clipPos.w();
            float ndcZ = clipPos.z() / clipPos.w();

            if (ndcZ < -1.0f || ndcZ > 1.0f) continue;
            // 画面外すぎないか
            if (ndcX < -2.0f || ndcX > 2.0f || ndcY < -2.0f || ndcY > 2.0f) continue;

            float sx = (ndcX * 0.5f + 0.5f) * w;
            float sy = (1.0f - (ndcY * 0.5f + 0.5f)) * h;
            screenJoints[i] = { true, QPointF(sx, sy) };
        }

        // ボーン（線）を描画
        for (const auto& limb : limbs) {
            if (screenJoints[limb.j1].valid && screenJoints[limb.j2].valid) {
                QPen pen(limb.color, boneWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
                painter.setPen(pen);
                painter.drawLine(screenJoints[limb.j1].pt, screenJoints[limb.j2].pt);
            }
        }

        // 関節（円）を描画
        for (int i = 0; i < 18; ++i) {
            if (screenJoints[i].valid) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(jointColors[i]);
                painter.drawEllipse(screenJoints[i].pt, jointRadius, jointRadius);
            }
        }
    }

    painter.end();
    return image;
}

} // namespace Kaburagi
