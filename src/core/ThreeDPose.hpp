#pragma once

#include <QString>
#include <QVector3D>
#include <QQuaternion>
#include <QMatrix4x4>
#include <vector>
#include <unordered_map>
#include <memory>

namespace Kaburagi {

struct ThreeDBone {
    QString name;
    int parentIndex = -1;
    QVector3D initialPosition;
    QVector3D localTranslation;
    QQuaternion localRotation;

    QMatrix4x4 transformMatrix;
    QMatrix4x4 combinedMatrix;
};

struct BoneKeyframe {
    QString boneName;
    int frame = 0;
    QVector3D translation;
    QQuaternion rotation;
};

class ThreeDPose {
public:
    ThreeDPose() = default;

    // ボーン名 -> (translation, rotation)
    std::unordered_map<QString, QVector3D> translations;
    std::unordered_map<QString, QQuaternion> rotations;
    bool isAbsoluteTranslation = false;
    bool isVrmaPose = false;

    // 読み込み
    bool loadVpd(const QString& filePath);
    bool loadVmd(const QString& filePath, int targetFrame = 0);
    bool loadVroidPose(const QString& filePath);

    bool isMmdPose() const {
        return rotations.count("左腕") || rotations.count("左足ＩＫ") || rotations.count("右腕") || rotations.count("センター");
    }

    // プリセットポーズ
    static ThreeDPose createTPose();
    static ThreeDPose createStandingPose();
    static ThreeDPose createSittingPose();
    static ThreeDPose createActionPose();
};

struct ThreeDIkLink {
    int boneIndex = -1;
    bool hasLimit = false;
    QVector3D minLimit;
    QVector3D maxLimit;
};

struct ThreeDIk {
    int ikBoneIndex = -1;
    int targetBoneIndex = -1;
    int iterations = 10;
    float angleLimit = 0.5f;
    std::vector<ThreeDIkLink> links;
};

class Skeleton3D {
public:
    Skeleton3D() = default;

    int addBone(const QString& name, int parentIndex, const QVector3D& initialPos);
    int findBoneIndex(const QString& name) const;

    ThreeDBone* bone(int index);
    const ThreeDBone* bone(int index) const;
    size_t boneCount() const { return m_bones.size(); }

    void addIk(const ThreeDIk& ik) { m_iks.push_back(ik); }
    const std::vector<ThreeDIk>& iks() const { return m_iks; }
    void solveIK();

    void applyPose(const ThreeDPose& pose);
    void updateMatrices();

    static Skeleton3D createStandardMmdSkeleton();

    const std::vector<ThreeDBone>& bones() const { return m_bones; }

private:
    std::vector<ThreeDBone> m_bones;
    std::vector<ThreeDIk> m_iks;
};

} // namespace Kaburagi
