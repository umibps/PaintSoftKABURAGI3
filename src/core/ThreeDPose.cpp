#include "ThreeDPose.hpp"
#include <QFile>
#include <QTextStream>
#include <QDataStream>
#include <QStringDecoder>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QDebug>
#include <cmath>

namespace Kaburagi {

int Skeleton3D::addBone(const QString& name, int parentIndex, const QVector3D& initialPos) {
    ThreeDBone b;
    b.name = name;
    b.parentIndex = parentIndex;
    b.initialPosition = initialPos;
    b.localTranslation = QVector3D(0, 0, 0);
    b.localRotation = QQuaternion();
    m_bones.push_back(b);
    return static_cast<int>(m_bones.size()) - 1;
}

int Skeleton3D::findBoneIndex(const QString& name) const {
    for (size_t i = 0; i < m_bones.size(); ++i) {
        if (m_bones[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

ThreeDBone* Skeleton3D::bone(int index) {
    if (index >= 0 && index < static_cast<int>(m_bones.size())) {
        return &m_bones[index];
    }
    return nullptr;
}

const ThreeDBone* Skeleton3D::bone(int index) const {
    if (index >= 0 && index < static_cast<int>(m_bones.size())) {
        return &m_bones[index];
    }
    return nullptr;
}

void Skeleton3D::applyPose(const ThreeDPose& pose) {
    for (auto& b : m_bones) {
        b.localTranslation = QVector3D(0, 0, 0);
        b.localRotation = QQuaternion();
        auto itTrans = pose.translations.find(b.name);
        if (itTrans != pose.translations.end()) {
            b.localTranslation = itTrans->second;
        }
        auto itRot = pose.rotations.find(b.name);
        if (itRot != pose.rotations.end()) {
            b.localRotation = itRot->second;
        }
    }
    updateMatrices();
    solveIK();
    updateMatrices();
}

void Skeleton3D::updateMatrices() {
    std::vector<bool> computed(m_bones.size(), false);
    std::function<void(int)> computeBone = [&](int idx) {
        if (idx < 0 || idx >= static_cast<int>(m_bones.size()) || computed[idx]) return;
        auto& b = m_bones[idx];
        QMatrix4x4 local;
        local.translate(b.initialPosition + b.localTranslation);
        local.rotate(b.localRotation);
        local.translate(-b.initialPosition);

        if (b.parentIndex >= 0 && b.parentIndex < static_cast<int>(m_bones.size())) {
            if (!computed[b.parentIndex]) {
                computeBone(b.parentIndex);
            }
            b.combinedMatrix = m_bones[b.parentIndex].combinedMatrix * local;
        } else {
            b.combinedMatrix = local;
        }
        computed[idx] = true;
    };

    for (size_t i = 0; i < m_bones.size(); ++i) {
        computeBone(static_cast<int>(i));
    }
}

void Skeleton3D::solveIK() {
    for (const auto& ik : m_iks) {
        if (ik.ikBoneIndex < 0 || ik.ikBoneIndex >= static_cast<int>(m_bones.size())) continue;
        if (ik.targetBoneIndex < 0 || ik.targetBoneIndex >= static_cast<int>(m_bones.size())) continue;

        for (int iter = 0; iter < ik.iterations; ++iter) {
            bool allConverged = true;
            for (const auto& link : ik.links) {
                if (link.boneIndex < 0 || link.boneIndex >= static_cast<int>(m_bones.size())) continue;

                updateMatrices();

                QVector3D targetPos = m_bones[ik.ikBoneIndex].combinedMatrix.map(m_bones[ik.ikBoneIndex].initialPosition);
                QVector3D effectorPos = m_bones[ik.targetBoneIndex].combinedMatrix.map(m_bones[ik.targetBoneIndex].initialPosition);

                if ((targetPos - effectorPos).lengthSquared() < 1e-4f) {
                    continue;
                }
                allConverged = false;

                QVector3D linkPos = m_bones[link.boneIndex].combinedMatrix.map(m_bones[link.boneIndex].initialPosition);
                QVector3D worldEffector = effectorPos - linkPos;
                QVector3D worldTarget = targetPos - linkPos;

                if (worldEffector.lengthSquared() < 1e-6f || worldTarget.lengthSquared() < 1e-6f) continue;

                // リンクボーンの親の座標系に変換
                QMatrix4x4 parentInv;
                int pIdx = m_bones[link.boneIndex].parentIndex;
                if (pIdx >= 0 && pIdx < static_cast<int>(m_bones.size())) {
                    parentInv = m_bones[pIdx].combinedMatrix.inverted();
                }
                QVector3D localEffector = parentInv.mapVector(worldEffector).normalized();
                QVector3D localTarget = parentInv.mapVector(worldTarget).normalized();

                float dot = std::clamp(QVector3D::dotProduct(localEffector, localTarget), -1.0f, 1.0f);
                if (dot > 0.99999f) continue;

                float angle = std::acos(dot);
                if (angle > ik.angleLimit) {
                    angle = ik.angleLimit;
                }

                QVector3D rotAxis = QVector3D::crossProduct(localEffector, localTarget).normalized();
                if (rotAxis.lengthSquared() < 1e-4f) continue;

                QQuaternion deltaRot = QQuaternion::fromAxisAndAngle(rotAxis, qRadiansToDegrees(angle));
                QQuaternion newRot = deltaRot * m_bones[link.boneIndex].localRotation;

                if (link.hasLimit) {
                    QVector3D euler = newRot.toEulerAngles();
                    float pitch = std::clamp(euler.x(), qRadiansToDegrees(link.minLimit.x()), qRadiansToDegrees(link.maxLimit.x()));
                    newRot = QQuaternion::fromEulerAngles(pitch, 0.0f, 0.0f);
                }

                m_bones[link.boneIndex].localRotation = newRot.normalized();
            }
            if (allConverged) break;
        }
    }
}

Skeleton3D Skeleton3D::createStandardMmdSkeleton() {
    Skeleton3D skel;
    int root = skel.addBone(QString::fromUtf8("センター"), -1, QVector3D(0.0f, 10.0f, 0.0f));
    int lowerBody = skel.addBone(QString::fromUtf8("下半身"), root, QVector3D(0.0f, 10.0f, 0.0f));
    int upperBody = skel.addBone(QString::fromUtf8("上半身"), root, QVector3D(0.0f, 13.18f, -0.14f));
    int neck = skel.addBone(QString::fromUtf8("首"), upperBody, QVector3D(0.0f, 17.63f, 0.22f));
    int head = skel.addBone(QString::fromUtf8("頭"), neck, QVector3D(0.0f, 18.18f, 0.21f));

    int shoulderL = skel.addBone(QString::fromUtf8("左肩"), upperBody, QVector3D(0.77f, 17.31f, 0.33f));
    int armL = skel.addBone(QString::fromUtf8("左腕"), shoulderL, QVector3D(1.79f, 16.47f, 0.46f));
    int elbowL = skel.addBone(QString::fromUtf8("左ひじ"), armL, QVector3D(3.95f, 15.20f, 0.47f));
    int wristL = skel.addBone(QString::fromUtf8("左手首"), elbowL, QVector3D(6.17f, 13.34f, 0.38f));

    int shoulderR = skel.addBone(QString::fromUtf8("右肩"), upperBody, QVector3D(-0.77f, 17.31f, 0.33f));
    int armR = skel.addBone(QString::fromUtf8("右腕"), shoulderR, QVector3D(-1.79f, 16.47f, 0.46f));
    int elbowR = skel.addBone(QString::fromUtf8("右ひじ"), armR, QVector3D(-3.95f, 15.20f, 0.47f));
    int wristR = skel.addBone(QString::fromUtf8("右手首"), elbowR, QVector3D(-6.17f, 13.34f, 0.38f));

    int legL = skel.addBone(QString::fromUtf8("左足"), lowerBody, QVector3D(0.89f, 11.33f, 0.0f));
    int kneeL = skel.addBone(QString::fromUtf8("左ひざ"), legL, QVector3D(1.02f, 6.67f, 0.0f));
    int ankleL = skel.addBone(QString::fromUtf8("左足首"), kneeL, QVector3D(1.08f, 1.70f, 0.0f));

    int legR = skel.addBone(QString::fromUtf8("右足"), lowerBody, QVector3D(-0.89f, 11.33f, 0.0f));
    int kneeR = skel.addBone(QString::fromUtf8("右ひざ"), legR, QVector3D(-1.02f, 6.67f, 0.0f));
    int ankleR = skel.addBone(QString::fromUtf8("右足首"), kneeR, QVector3D(-1.08f, 1.70f, 0.0f));

    int ikLegL = skel.addBone(QString::fromUtf8("左足ＩＫ"), -1, QVector3D(1.08f, 1.70f, 0.0f));
    int ikLegR = skel.addBone(QString::fromUtf8("右足ＩＫ"), -1, QVector3D(-1.08f, 1.70f, 0.0f));

    // 左足IK
    ThreeDIk ikL;
    ikL.ikBoneIndex = ikLegL;
    ikL.targetBoneIndex = ankleL;
    ikL.iterations = 40;
    ikL.angleLimit = 0.5f;
    ikL.links.push_back({ kneeL, true, QVector3D(-3.14f, 0.0f, 0.0f), QVector3D(-0.002f, 0.0f, 0.0f) });
    ikL.links.push_back({ legL, false, QVector3D(), QVector3D() });
    skel.addIk(ikL);

    // 右足IK
    ThreeDIk ikR;
    ikR.ikBoneIndex = ikLegR;
    ikR.targetBoneIndex = ankleR;
    ikR.iterations = 40;
    ikR.angleLimit = 0.5f;
    ikR.links.push_back({ kneeR, true, QVector3D(-3.14f, 0.0f, 0.0f), QVector3D(-0.002f, 0.0f, 0.0f) });
    ikR.links.push_back({ legR, false, QVector3D(), QVector3D() });
    skel.addIk(ikR);

    return skel;
}

// -------------------------------------------------------------
// ThreeDPose
// -------------------------------------------------------------

bool ThreeDPose::loadVpd(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    // VPDは通常Shift-JIS
    QString content = QStringDecoder("Shift-JIS")(data);
    if (content.isEmpty() || !content.contains("Vocaloid Pose Data")) {
        content = QStringDecoder("CP932")(data);
    }
    if (content.isEmpty() || !content.contains("Vocaloid Pose Data")) {
        content = QStringDecoder(QStringDecoder::System)(data);
    }
    if (content.isEmpty() || !content.contains("Vocaloid Pose Data")) {
        content = QString::fromUtf8(data);
    }
    if (!content.contains("Vocaloid Pose Data")) {
        return false;
    }

    translations.clear();
    rotations.clear();

    QStringList lines = content.split('\n');
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        if (line.startsWith("Bone") && line.contains('{')) {
            int braceOpen = line.indexOf('{');
            QString boneName = line.mid(braceOpen + 1);
            if (boneName.endsWith('}')) {
                boneName.chop(1);
            }
            boneName = boneName.trimmed();

            // 次の行: 移動 x, y, z;
            QVector3D trans(0, 0, 0);
            if (i + 1 < lines.size()) {
                QString transLine = lines[++i].trimmed();
                int semi = transLine.indexOf(';');
                if (semi >= 0) transLine = transLine.left(semi).trimmed();
                QStringList parts = transLine.split(',');
                if (parts.size() >= 3) {
                    float tx = parts[0].trimmed().toFloat();
                    float ty = parts[1].trimmed().toFloat();
                    float tz = parts[2].trimmed().toFloat();
                    // MMD(右手系) -> OpenGL(Z反転): (tx, ty, -tz)
                    trans = QVector3D(tx, ty, -tz);
                }
            }

            // 次の行: 回転 x, y, z, w;
            QQuaternion rot;
            if (i + 1 < lines.size()) {
                QString rotLine = lines[++i].trimmed();
                int semi = rotLine.indexOf(';');
                if (semi >= 0) rotLine = rotLine.left(semi).trimmed();
                QStringList parts = rotLine.split(',');
                if (parts.size() >= 4) {
                    float qx = parts[0].trimmed().toFloat();
                    float qy = parts[1].trimmed().toFloat();
                    float qz = parts[2].trimmed().toFloat();
                    float qw = parts[3].trimmed().toFloat();
                    // MMD右手系 -> OpenGL Z反転: (-qx, -qy, qz, qw)
                    rot = QQuaternion(qw, -qx, -qy, qz);
                    rot.normalize();
                }
            }

            if (!boneName.isEmpty()) {
                translations[boneName] = trans;
                rotations[boneName] = rot;
            }
        }
    }

    return !rotations.empty();
}

bool ThreeDPose::loadVmd(const QString& filePath, int targetFrame) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray bytes = file.readAll();
    file.close();

    if (bytes.size() < 54) return false;

    // ヘッダー確認 (30 bytes)
    QByteArray header = bytes.left(30);
    if (!header.startsWith("Vocaloid Motion Data")) {
        return false;
    }

    const char* ptr = bytes.constData() + 50; // 30 header + 20 modelName
    if (bytes.size() < 54) return false;

    uint32_t keyframeCount = *reinterpret_cast<const uint32_t*>(ptr);
    ptr += 4;

    size_t expectedSize = 54 + static_cast<size_t>(keyframeCount) * 111;
    if (static_cast<size_t>(bytes.size()) < expectedSize) {
        return false;
    }

    translations.clear();
    rotations.clear();

    auto sjisDecoder = QStringDecoder(QStringDecoder::System);

    struct CandidateFrame {
        int frame = 0;
        QVector3D trans;
        QQuaternion rot;
    };
    std::unordered_map<QString, CandidateFrame> bestFrames;

    for (uint32_t k = 0; k < keyframeCount; ++k) {
        char nameBuf[16] = {0};
        memcpy(nameBuf, ptr, 15);
        QString boneName = sjisDecoder(QByteArray(nameBuf));
        if (boneName.isEmpty()) boneName = QString::fromLatin1(nameBuf);

        uint32_t frame = *reinterpret_cast<const uint32_t*>(ptr + 15);
        float tx = *reinterpret_cast<const float*>(ptr + 19);
        float ty = *reinterpret_cast<const float*>(ptr + 23);
        float tz = *reinterpret_cast<const float*>(ptr + 27);
        float rx = *reinterpret_cast<const float*>(ptr + 31);
        float ry = *reinterpret_cast<const float*>(ptr + 35);
        float rz = *reinterpret_cast<const float*>(ptr + 39);
        float rw = *reinterpret_cast<const float*>(ptr + 43);

        ptr += 111; // 15 + 4 + 12 + 16 + 64 (interpolation)

        int diff = std::abs(static_cast<int>(frame) - targetFrame);
        auto it = bestFrames.find(boneName);
        if (it == bestFrames.end() || diff < std::abs(it->second.frame - targetFrame)) {
            bestFrames[boneName] = { static_cast<int>(frame), QVector3D(tx, ty, tz), QQuaternion(rw, rx, ry, rz) };
        }
    }

    for (const auto& [name, c] : bestFrames) {
        translations[name] = c.trans;
        rotations[name] = c.rot;
    }

    return !rotations.empty();
}

bool ThreeDPose::loadVroidPose(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    QJsonObject root = doc.object();
    if (!root.contains("BoneDefinition") || !root["BoneDefinition"].isObject()) {
        return false;
    }

    QJsonObject bd = root["BoneDefinition"].toObject();

    translations.clear();
    rotations.clear();
    isAbsoluteTranslation = true;

    // HipsPosition (ヒップ移動)
    if (bd.contains("HipsPosition") && bd["HipsPosition"].isObject()) {
        QJsonObject posObj = bd["HipsPosition"].toObject();
        float px = static_cast<float>(posObj["x"].toDouble(0.0));
        float py = static_cast<float>(posObj["y"].toDouble(0.0));
        float pz = static_cast<float>(posObj["z"].toDouble(0.0));
        // Unity(左手系) -> glTF(右手系): Z軸反転 ReverseZ
        QVector3D hipPos(px, py, -pz);
        translations["Hips"] = hipPos;
        translations["hips"] = hipPos;
        translations["センター"] = hipPos;
        translations["下半身"] = hipPos;
    }

    // VRoid BoneDefinition key -> MMD名 / VRM名マッピング
    struct BoneMapping {
        const char* vroidKey;
        const char* vrmName;
        const char* mmdName;
    };

    static const BoneMapping mappings[] = {
        { "Hips", "hips", "下半身" },
        { "Spine", "spine", "上半身" },
        { "Chest", "chest", "上半身2" },
        { "UpperChest", "upperChest", "胸" },
        { "Neck", "neck", "首" },
        { "Head", "head", "頭" },
        { "LeftShoulder", "leftShoulder", "左肩" },
        { "LeftUpperArm", "leftUpperArm", "左腕" },
        { "LeftLowerArm", "leftLowerArm", "左ひじ" },
        { "LeftHand", "leftHand", "左手首" },
        { "RightShoulder", "rightShoulder", "右肩" },
        { "RightUpperArm", "rightUpperArm", "右腕" },
        { "RightLowerArm", "rightLowerArm", "右ひじ" },
        { "RightHand", "rightHand", "右手首" },
        { "LeftUpperLeg", "leftUpperLeg", "左足" },
        { "LeftLowerLeg", "leftLowerLeg", "左ひざ" },
        { "LeftFoot", "leftFoot", "左足首" },
        { "LeftToes", "leftToes", "左つま先" },
        { "RightUpperLeg", "rightUpperLeg", "右足" },
        { "RightLowerLeg", "rightLowerLeg", "右ひざ" },
        { "RightFoot", "rightFoot", "右足首" },
        { "RightToes", "rightToes", "右つま先" },
    };

    for (const auto& m : mappings) {
        if (bd.contains(m.vroidKey) && bd[m.vroidKey].isObject()) {
            QJsonObject qObj = bd[m.vroidKey].toObject();
            float qx = static_cast<float>(qObj["x"].toDouble(0.0));
            float qy = static_cast<float>(qObj["y"].toDouble(0.0));
            float qz = static_cast<float>(qObj["z"].toDouble(0.0));
            float qw = static_cast<float>(qObj["w"].toDouble(1.0));
            // Unity(左手系) -> glTF(右手系): ReverseZ
            // AngleAxis(-angle, (axis.x, axis.y, -axis.z)) -> (-qx, -qy, qz, qw)
            // QtのQQuaternionコンストラクタは (scalar/w, x, y, z)
            QQuaternion q(qw, -qx, -qy, qz);

            // VRoid名、VRM名、MMD名のすべてに登録
            rotations[m.vroidKey] = q;
            rotations[m.vrmName] = q;
            rotations[m.mmdName] = q;
        }
    }

    return !rotations.empty();
}

ThreeDPose ThreeDPose::createTPose() {
    ThreeDPose p;
    // デフォルト（無回転・無移動）
    return p;
}

ThreeDPose ThreeDPose::createStandingPose() {
    ThreeDPose p;
    // 腕を自然に斜め下に降ろす
    p.rotations["左腕"] = QQuaternion::fromAxisAndAngle(QVector3D(0, 0, 1), -40.0f);
    p.rotations["右腕"] = QQuaternion::fromAxisAndAngle(QVector3D(0, 0, 1), 40.0f);
    p.rotations["左ひじ"] = QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), -15.0f);
    p.rotations["右ひじ"] = QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), 15.0f);
    return p;
}

ThreeDPose ThreeDPose::createSittingPose() {
    ThreeDPose p;
    p.translations["センター"] = QVector3D(0, -1.2f, 0);
    p.rotations["左足"] = QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), -80.0f);
    p.rotations["右足"] = QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), -80.0f);
    p.rotations["左ひざ"] = QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), 80.0f);
    p.rotations["右ひざ"] = QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), 80.0f);
    p.rotations["左腕"] = QQuaternion::fromAxisAndAngle(QVector3D(0, 0, 1), -30.0f);
    p.rotations["右腕"] = QQuaternion::fromAxisAndAngle(QVector3D(0, 0, 1), 30.0f);
    return p;
}

ThreeDPose ThreeDPose::createActionPose() {
    ThreeDPose p;
    // 右手を前に掲げ、左足を後ろに引いたダイナミックなアクションポーズ
    p.rotations["上半身"] = QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), 20.0f);
    p.rotations["右腕"] = QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), -70.0f) * QQuaternion::fromAxisAndAngle(QVector3D(0, 0, 1), 20.0f);
    p.rotations["右ひじ"] = QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), -30.0f);
    p.rotations["左腕"] = QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), 40.0f) * QQuaternion::fromAxisAndAngle(QVector3D(0, 0, 1), -30.0f);
    p.rotations["右足"] = QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), -30.0f);
    p.rotations["左足"] = QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), 25.0f);
    return p;
}

} // namespace Kaburagi
