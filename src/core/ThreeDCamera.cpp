#include "ThreeDCamera.hpp"
#include <QtMath>

namespace Kaburagi {

ThreeDCamera::ThreeDCamera() {
    reset();
}

void ThreeDCamera::reset() {
    m_eye = QVector3D(0.0f, 1.2f, 3.5f);
    m_target = QVector3D(0.0f, 0.8f, 0.0f);
    m_up = QVector3D(0.0f, 1.0f, 0.0f);
    m_fov = 45.0f;
    m_nearPlane = 0.05f;
    m_farPlane = 500.0f;
}

void ThreeDCamera::orbit(float deltaYawDeg, float deltaPitchDeg) {
    QVector3D dir = m_eye - m_target;
    float dist = dir.length();
    if (dist < 1e-4f) dist = 1.0f;

    // 現在のヨー・ピッチ角の計算
    float pitch = qAsin(std::clamp(dir.y() / dist, -0.999f, 0.999f));
    float yaw = qAtan2(dir.x(), dir.z());

    yaw += qDegreesToRadians(deltaYawDeg);
    pitch = std::clamp(pitch + qDegreesToRadians(deltaPitchDeg), -1.50f, 1.50f);

    dir.setX(dist * qCos(pitch) * qSin(yaw));
    dir.setY(dist * qSin(pitch));
    dir.setZ(dist * qCos(pitch) * qCos(yaw));

    m_eye = m_target + dir;
}

void ThreeDCamera::pan(float deltaRight, float deltaUp) {
    QVector3D forward = (m_target - m_eye).normalized();
    QVector3D right = QVector3D::crossProduct(forward, m_up).normalized();
    QVector3D actualUp = QVector3D::crossProduct(right, forward).normalized();

    QVector3D offset = right * deltaRight + actualUp * deltaUp;
    m_eye += offset;
    m_target += offset;
}

void ThreeDCamera::zoom(float factor) {
    QVector3D dir = m_eye - m_target;
    float dist = dir.length();
    dist *= factor;
    dist = std::clamp(dist, 0.1f, 200.0f);
    m_eye = m_target + dir.normalized() * dist;
}

QMatrix4x4 ThreeDCamera::viewMatrix() const {
    QMatrix4x4 m;
    m.lookAt(m_eye, m_target, m_up);
    return m;
}

QMatrix4x4 ThreeDCamera::projectionMatrix(float aspect) const {
    QMatrix4x4 m;
    m.perspective(m_fov, aspect > 0.0f ? aspect : 1.0f, m_nearPlane, m_farPlane);
    return m;
}

QMatrix4x4 ThreeDCamera::viewProjectionMatrix(float aspect) const {
    return projectionMatrix(aspect) * viewMatrix();
}

static QPointF projectToCanvas(const QVector4D& clipPos, int canvasW, int canvasH) {
    if (std::abs(clipPos.w()) < 1e-5f) {
        return QPointF(canvasW * 0.5f, canvasH * 0.5f);
    }
    float ndcX = clipPos.x() / clipPos.w();
    float ndcY = clipPos.y() / clipPos.w();

    // NDC (-1〜+1) -> キャンバス座標 (0〜W, 0〜H)
    float screenX = (ndcX + 1.0f) * 0.5f * canvasW;
    float screenY = (1.0f - ndcY) * 0.5f * canvasH;
    return QPointF(screenX, screenY);
}

void ThreeDCamera::calculateVanishingPoints(float aspect, int canvasW, int canvasH,
                                          QPointF& outVpX, QPointF& outVpY, QPointF& outVpZ) const
{
    QMatrix4x4 vp = viewProjectionMatrix(aspect);

    // 各軸方向の十分遠い点 (10000ユニット)
    const float INF_DIST = 10000.0f;
    QVector4D pX = vp * QVector4D(INF_DIST, 0.0f, 0.0f, 1.0f);
    QVector4D pY = vp * QVector4D(0.0f, INF_DIST, 0.0f, 1.0f);
    QVector4D pZ = vp * QVector4D(0.0f, 0.0f, INF_DIST, 1.0f);

    outVpX = projectToCanvas(pX, canvasW, canvasH);
    outVpY = projectToCanvas(pY, canvasW, canvasH);
    outVpZ = projectToCanvas(pZ, canvasW, canvasH);
}

float ThreeDCamera::calculateEyeLevelY(float aspect, int canvasH) const {
    // カメラの正面水平方向の無限遠点を投影してアイレベルのY座標を算出
    QVector3D forward = (m_target - m_eye).normalized();
    forward.setY(0.0f); // 水平
    if (forward.lengthSquared() < 1e-4f) forward = QVector3D(0.0f, 0.0f, -1.0f);
    forward.normalize();

    QVector4D horizFar = viewProjectionMatrix(aspect) * QVector4D(m_eye + forward * 10000.0f, 1.0f);
    if (std::abs(horizFar.w()) < 1e-5f) return canvasH * 0.5f;

    float ndcY = horizFar.y() / horizFar.w();
    return (1.0f - ndcY) * 0.5f * canvasH;
}

} // namespace Kaburagi
