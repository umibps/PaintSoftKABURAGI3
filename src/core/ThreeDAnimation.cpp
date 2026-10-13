#include "ThreeDAnimation.hpp"
#include <QFile>
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QStringDecoder>
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace Kaburagi {

QVector3D AnimationTrack::sampleTranslation(float timeSec) const {
    if (timestamps.empty() || translations.empty()) return QVector3D();
    if (timestamps.size() == 1 || timeSec <= timestamps.front()) return translations.front();
    if (timeSec >= timestamps.back()) return translations.back();

    auto it = std::lower_bound(timestamps.begin(), timestamps.end(), timeSec);
    size_t idx1 = std::distance(timestamps.begin(), it);
    size_t idx0 = idx1 - 1;

    float t0 = timestamps[idx0];
    float t1 = timestamps[idx1];
    float factor = (t1 > t0) ? (timeSec - t0) / (t1 - t0) : 0.0f;
    factor = std::clamp(factor, 0.0f, 1.0f);

    return translations[idx0] * (1.0f - factor) + translations[idx1] * factor;
}

QQuaternion AnimationTrack::sampleRotation(float timeSec) const {
    if (timestamps.empty() || rotations.empty()) return QQuaternion();
    if (timestamps.size() == 1 || timeSec <= timestamps.front()) return rotations.front();
    if (timeSec >= timestamps.back()) return rotations.back();

    auto it = std::lower_bound(timestamps.begin(), timestamps.end(), timeSec);
    size_t idx1 = std::distance(timestamps.begin(), it);
    size_t idx0 = idx1 - 1;

    float t0 = timestamps[idx0];
    float t1 = timestamps[idx1];
    float factor = (t1 > t0) ? (timeSec - t0) / (t1 - t0) : 0.0f;
    factor = std::clamp(factor, 0.0f, 1.0f);

    return QQuaternion::slerp(rotations[idx0], rotations[idx1], factor);
}

void ThreeDAnimation::evaluate(float timeSec, ThreeDPose& outPose) const {
    outPose.translations.clear();
    outPose.rotations.clear();
    outPose.isAbsoluteTranslation = isMmd ? false : true;
    outPose.isVrmaPose = !isMmd;

    for (const auto& track : tracks) {
        if (track.path == AnimationChannelPath::Translation) {
            outPose.translations[track.targetBone] = track.sampleTranslation(timeSec);
        } else if (track.path == AnimationChannelPath::Rotation) {
            outPose.rotations[track.targetBone] = track.sampleRotation(timeSec);
        }
    }
}

std::shared_ptr<ThreeDAnimation> ThreeDAnimation::loadVrma(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return nullptr;

    QByteArray fileData = file.readAll();
    file.close();

    if (fileData.size() < 20 || !fileData.startsWith("glTF")) {
        return nullptr;
    }

    QDataStream stream(fileData);
    stream.setByteOrder(QDataStream::LittleEndian);

    quint32 magic, version, totalLength;
    stream >> magic >> version >> totalLength;
    if (magic != 0x46546C67 || version != 2) return nullptr;

    QJsonObject root;
    QByteArray binBuffer;

    while (!stream.atEnd()) {
        quint32 chunkLength, chunkType;
        stream >> chunkLength >> chunkType;
        if (chunkLength == 0) break;

        QByteArray chunkData = stream.device()->read(chunkLength);
        if (chunkType == 0x4E4F534A) { // JSON
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(chunkData, &err);
            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                root = doc.object();
            }
        } else if (chunkType == 0x004E4942) { // BIN
            binBuffer = chunkData;
        }
    }

    if (root.isEmpty() || binBuffer.isEmpty()) return nullptr;

    // VRMC_vrm_animation 拡張からヒューマノイドボーンのマッピングを取得
    std::unordered_map<int, QString> nodeToBone;
    if (root.contains("extensions")) {
        QJsonObject exts = root["extensions"].toObject();
        if (exts.contains("VRMC_vrm_animation")) {
            QJsonObject vrmAnim = exts["VRMC_vrm_animation"].toObject();
            if (vrmAnim.contains("humanoid")) {
                QJsonObject humanoid = vrmAnim["humanoid"].toObject();
                if (humanoid.contains("humanBones")) {
                    QJsonObject humanBones = humanoid["humanBones"].toObject();
                    for (auto it = humanBones.begin(); it != humanBones.end(); ++it) {
                        QString boneName = it.key();
                        QJsonObject bObj = it.value().toObject();
                        int nodeIdx = bObj["node"].toInt(-1);
                        if (nodeIdx >= 0) {
                            nodeToBone[nodeIdx] = boneName;
                        }
                    }
                }
            }
        }
    }

    // ノード名からもフォールバック用マップを作成
    QJsonArray nodes = root["nodes"].toArray();
    for (int i = 0; i < nodes.size(); ++i) {
        if (!nodeToBone.count(i)) {
            QString nName = nodes[i].toObject()["name"].toString();
            if (!nName.isEmpty()) {
                nodeToBone[i] = nName;
            }
        }
    }

    QJsonArray animations = root["animations"].toArray();
    if (animations.empty()) return nullptr;

    QJsonObject animObj = animations[0].toObject();
    QJsonArray channels = animObj["channels"].toArray();
    QJsonArray samplers = animObj["samplers"].toArray();
    QJsonArray accessors = root["accessors"].toArray();
    QJsonArray bufferViews = root["bufferViews"].toArray();

    auto anim = std::make_shared<ThreeDAnimation>();
    anim->name = QFileInfo(filePath).completeBaseName();

    float maxDuration = 0.0f;

    for (const auto& chVal : channels) {
        QJsonObject ch = chVal.toObject();
        int samplerIdx = ch["sampler"].toInt(-1);
        QJsonObject target = ch["target"].toObject();
        int nodeIdx = target["node"].toInt(-1);
        QString pathStr = target["path"].toString();

        if (samplerIdx < 0 || samplerIdx >= samplers.size() || nodeIdx < 0) continue;
        if (!nodeToBone.count(nodeIdx)) continue;

        QString targetBone = nodeToBone[nodeIdx];
        QJsonObject sampler = samplers[samplerIdx].toObject();
        int inAccIdx = sampler["input"].toInt(-1);
        int outAccIdx = sampler["output"].toInt(-1);

        if (inAccIdx < 0 || inAccIdx >= accessors.size() ||
            outAccIdx < 0 || outAccIdx >= accessors.size()) continue;

        QJsonObject inAcc = accessors[inAccIdx].toObject();
        QJsonObject outAcc = accessors[outAccIdx].toObject();

        int keyCount = inAcc["count"].toInt(0);
        if (keyCount <= 0) continue;

        // タイムスタンプ読み込み
        int inBvIdx = inAcc["bufferView"].toInt(-1);
        if (inBvIdx < 0 || inBvIdx >= bufferViews.size()) continue;
        QJsonObject inBv = bufferViews[inBvIdx].toObject();
        int inOffset = inBv["byteOffset"].toInt(0) + inAcc["byteOffset"].toInt(0);

        if (inOffset + keyCount * 4 > binBuffer.size()) continue;
        const float* timePtr = reinterpret_cast<const float*>(binBuffer.constData() + inOffset);

        std::vector<float> timestamps(keyCount);
        for (int k = 0; k < keyCount; ++k) {
            timestamps[k] = timePtr[k];
        }
        if (!timestamps.empty() && timestamps.back() > maxDuration) {
            maxDuration = timestamps.back();
        }

        // 値（TranslationまたはRotation）読み込み
        int outBvIdx = outAcc["bufferView"].toInt(-1);
        if (outBvIdx < 0 || outBvIdx >= bufferViews.size()) continue;
        QJsonObject outBv = bufferViews[outBvIdx].toObject();
        int outOffset = outBv["byteOffset"].toInt(0) + outAcc["byteOffset"].toInt(0);

        AnimationTrack track;
        track.targetBone = targetBone;
        track.timestamps = std::move(timestamps);

        if (pathStr == "translation") {
            track.path = AnimationChannelPath::Translation;
            if (outOffset + keyCount * 12 <= binBuffer.size()) {
                const float* valPtr = reinterpret_cast<const float*>(binBuffer.constData() + outOffset);
                track.translations.resize(keyCount);
                for (int k = 0; k < keyCount; ++k) {
                    track.translations[k] = QVector3D(valPtr[k * 3], valPtr[k * 3 + 1], valPtr[k * 3 + 2]);
                }
            }
        } else if (pathStr == "rotation") {
            track.path = AnimationChannelPath::Rotation;
            if (outOffset + keyCount * 16 <= binBuffer.size()) {
                const float* valPtr = reinterpret_cast<const float*>(binBuffer.constData() + outOffset);
                track.rotations.resize(keyCount);
                for (int k = 0; k < keyCount; ++k) {
                    // glTF クォータニオンは (x, y, z, w)、QQuaternion は (w, x, y, z)
                    float qx = valPtr[k * 4];
                    float qy = valPtr[k * 4 + 1];
                    float qz = valPtr[k * 4 + 2];
                    float qw = valPtr[k * 4 + 3];
                    track.rotations[k] = QQuaternion(qw, qx, qy, qz).normalized();
                }
            }
        } else {
            continue;
        }

        anim->tracks.push_back(std::move(track));
    }

    anim->duration = maxDuration;
    return anim;
}

std::shared_ptr<ThreeDAnimation> ThreeDAnimation::loadVmd(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return nullptr;

    QByteArray bytes = file.readAll();
    file.close();

    if (bytes.size() < 54) return nullptr;

    if (!bytes.startsWith("Vocaloid Motion Data")) return nullptr;

    const char* ptr = bytes.constData() + 50;
    uint32_t keyframeCount = *reinterpret_cast<const uint32_t*>(ptr);
    ptr += 4;

    size_t expectedSize = 54 + static_cast<size_t>(keyframeCount) * 111;
    if (static_cast<size_t>(bytes.size()) < expectedSize) return nullptr;

    auto sjisDecoder = QStringDecoder(QStringDecoder::System);

    struct RawKeyframe {
        float timeSec;
        QVector3D trans;
        QQuaternion rot;
    };
    std::unordered_map<QString, std::vector<RawKeyframe>> boneFrames;

    float maxDuration = 0.0f;

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

        ptr += 111;

        float timeSec = frame / 30.0f;
        if (timeSec > maxDuration) maxDuration = timeSec;

        // MMD(右手系) -> OpenGL(Z反転)
        QVector3D trans(tx, ty, -tz);
        QQuaternion rot(rw, -rx, -ry, rz);
        rot.normalize();

        boneFrames[boneName].push_back({ timeSec, trans, rot });
    }

    auto anim = std::make_shared<ThreeDAnimation>();
    anim->name = QFileInfo(filePath).baseName();
    anim->isMmd = true;
    anim->duration = maxDuration;

    for (auto& [boneName, kfs] : boneFrames) {
        if (kfs.empty()) continue;

        // 時系列順にソート
        std::sort(kfs.begin(), kfs.end(), [](const RawKeyframe& a, const RawKeyframe& b) {
            return a.timeSec < b.timeSec;
        });

        // 重複タイムスタンプを統合
        std::vector<RawKeyframe> uniqueKfs;
        for (const auto& kf : kfs) {
            if (!uniqueKfs.empty() && std::abs(uniqueKfs.back().timeSec - kf.timeSec) < 1e-4f) {
                uniqueKfs.back() = kf;
            } else {
                uniqueKfs.push_back(kf);
            }
        }

        // Translation トラック作成 (移動があるものまたはIK/ルート系ボーン)
        bool hasNonZeroTrans = false;
        for (const auto& kf : uniqueKfs) {
            if (kf.trans.lengthSquared() > 1e-5f) {
                hasNonZeroTrans = true;
                break;
            }
        }
        if (hasNonZeroTrans || boneName == QString::fromUtf8("センター") || boneName.contains(QString::fromUtf8("ＩＫ")) ||
            boneName == QString::fromUtf8("全ての親") || boneName == QString::fromUtf8("グルーブ")) {
            AnimationTrack transTrack;
            transTrack.targetBone = boneName;
            transTrack.path = AnimationChannelPath::Translation;
            transTrack.timestamps.reserve(uniqueKfs.size());
            transTrack.translations.reserve(uniqueKfs.size());
            for (const auto& kf : uniqueKfs) {
                transTrack.timestamps.push_back(kf.timeSec);
                transTrack.translations.push_back(kf.trans);
            }
            anim->tracks.push_back(std::move(transTrack));
        }

        // Rotation トラック作成
        AnimationTrack rotTrack;
        rotTrack.targetBone = boneName;
        rotTrack.path = AnimationChannelPath::Rotation;
        rotTrack.timestamps.reserve(uniqueKfs.size());
        rotTrack.rotations.reserve(uniqueKfs.size());
        for (const auto& kf : uniqueKfs) {
            rotTrack.timestamps.push_back(kf.timeSec);
            rotTrack.rotations.push_back(kf.rot);
        }
        anim->tracks.push_back(std::move(rotTrack));
    }

    return anim;
}

} // namespace Kaburagi
