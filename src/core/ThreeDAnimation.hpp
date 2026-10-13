#pragma once

#include <QString>
#include <QVector3D>
#include <QQuaternion>
#include <vector>
#include <memory>
#include "ThreeDPose.hpp"

namespace Kaburagi {

enum class AnimationChannelPath {
    Translation,
    Rotation,
    Scale
};

struct AnimationTrack {
    QString targetBone; // "hips", "spine", "leftUpperArm", etc.
    AnimationChannelPath path = AnimationChannelPath::Rotation;
    std::vector<float> timestamps; // キーフレーム時刻（秒）
    std::vector<QVector3D> translations;
    std::vector<QQuaternion> rotations;

    QVector3D sampleTranslation(float timeSec) const;
    QQuaternion sampleRotation(float timeSec) const;
};

class ThreeDAnimation {
public:
    ThreeDAnimation() = default;

    QString name;
    float duration = 0.0f; // 総時間（秒）
    bool isMmd = false;    // MMD (.vmd) モーションフラグ
    std::vector<AnimationTrack> tracks;

    bool isEmpty() const { return tracks.empty(); }

    // 指定時刻のポーズを生成
    void evaluate(float timeSec, ThreeDPose& outPose) const;

    // VRMA (VRM Animation, glTF GLB形式) 読み込み
    static std::shared_ptr<ThreeDAnimation> loadVrma(const QString& filePath);

    // MMD (.vmd) モーション読み込み
    static std::shared_ptr<ThreeDAnimation> loadVmd(const QString& filePath);
};

} // namespace Kaburagi
