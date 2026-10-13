#pragma once

#include "ThreeDMath.hpp"
#include <QPointF>

namespace Kaburagi {

class ThreeDCamera {
public:
    ThreeDCamera();

    const QVector3D& eye() const { return m_eye; }
    void setEye(const QVector3D& eye) { m_eye = eye; }

    const QVector3D& target() const { return m_target; }
    void setTarget(const QVector3D& target) { m_target = target; }

    const QVector3D& up() const { return m_up; }
    void setUp(const QVector3D& up) { m_up = up; }

    float fov() const { return m_fov; }
    void setFov(float fov) { m_fov = std::clamp(fov, 5.0f, 120.0f); }

    float nearPlane() const { return m_nearPlane; }
    void setNearPlane(float np) { m_nearPlane = np; }

    float farPlane() const { return m_farPlane; }
    void setFarPlane(float fp) { m_farPlane = fp; }

    // 操作
    void orbit(float deltaYawDeg, float deltaPitchDeg);
    void pan(float deltaRight, float deltaUp);
    void zoom(float factor);
    void reset();

    // 行列
    QMatrix4x4 viewMatrix() const;
    QMatrix4x4 projectionMatrix(float aspect) const;
    QMatrix4x4 viewProjectionMatrix(float aspect) const;

    // パース定規・作画連携: 消失点とアイレベルの算出
    // outVpX, outVpY, outVpZ: キャンバス座標上の各消失点 (無限遠の場合は非常に大きな値)
    void calculateVanishingPoints(float aspect, int canvasW, int canvasH,
                                  QPointF& outVpX, QPointF& outVpY, QPointF& outVpZ) const;
    float calculateEyeLevelY(float aspect, int canvasH) const;

private:
    QVector3D m_eye;
    QVector3D m_target;
    QVector3D m_up;
    float m_fov = 45.0f;
    float m_nearPlane = 0.1f;
    float m_farPlane = 500.0f;
};

} // namespace Kaburagi
