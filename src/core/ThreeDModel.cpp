#include "ThreeDModel.hpp"
#include "ThreeDMmdParser.hpp"
#include "ThreeDVrmParser.hpp"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDataStream>
#include <QDebug>
#include <iostream>
#include <functional>

namespace Kaburagi {

ThreeDModel::ThreeDModel() {
}

void ThreeDModel::setRotationEuler(const QVector3D& eulerDeg) {
    m_rotationEuler = eulerDeg;
    // Qt: fromEulerAngles(pitch, yaw, roll) -> (X, Y, Z) 度数法
    m_rotation = QQuaternion::fromEulerAngles(eulerDeg.x(), eulerDeg.y(), eulerDeg.z());
}

QMatrix4x4 ThreeDModel::modelMatrix() const {
    QMatrix4x4 m;
    // 3D表示処理: 回転 > 移動 (頂点変換 v' = T * R * S * v)
    m.translate(m_position);
    m.rotate(m_rotation);
    m.scale(m_scale);
    return m;
}

void ThreeDModel::recalculateBoundingBox() {
    m_boundingBox = BoundingBox3D();
    for (const auto& mesh : m_meshes) {
        for (const auto& v : mesh.vertices) {
            m_boundingBox.expand(v.position);
        }
    }
}

void ThreeDModel::normalizeScale(float targetSize) {
    recalculateBoundingBox();
    if (!m_boundingBox.isValid()) return;

    float maxDim = m_boundingBox.maxDimension();
    if (maxDim > 1e-4f) {
        float factor = targetSize / maxDim;
        QVector3D center = m_boundingBox.center();
        for (auto& mesh : m_meshes) {
            for (auto& v : mesh.vertices) {
                v.position = (v.position - center) * factor;
            }
            for (auto& v : mesh.bindVertices) {
                v.position = (v.position - center) * factor;
            }
        }
        recalculateBoundingBox();
    }
}

bool ThreeDModel::loadFromFile(const QString& filePath) {
    m_filePath = filePath;
    QFileInfo fi(filePath);
    m_name = fi.baseName();
    QString ext = fi.suffix().toLower();

    if (ext == "obj") {
        return loadObj(filePath);
    } else if (ext == "gltf" || ext == "glb") {
        return loadGltf(filePath);
    } else if (ext == "vrm") {
        return loadVrm(filePath);
    } else if (ext == "pmd") {
        return loadPmd(filePath);
    } else if (ext == "pmx") {
        return loadPmx(filePath);
    }
    return false;
}

bool ThreeDModel::loadPmd(const QString& filePath) {
    return ThreeDMmdParser::loadPmd(filePath, *this, m_skeleton);
}

bool ThreeDModel::loadPmx(const QString& filePath) {
    return ThreeDMmdParser::loadPmx(filePath, *this, m_skeleton);
}

bool ThreeDModel::loadVrm(const QString& filePath) {
    return ThreeDVrmParser::loadVrm(filePath, *this, m_skeleton);
}

void ThreeDModel::applyPose(const ThreeDPose& pose, bool updateCpuSkinning) {
    m_skeleton.applyPose(pose);

    if (!m_nodes.empty()) {
        // 0. 全ノードの姿勢を初期状態にリセット
        for (auto& node : m_nodes) {
            node.rotation = node.initialRotation;
            node.translation = node.initialTranslation;
        }

        ThreeDPose effectivePose = pose;

        // MMDポーズ (.vpd等) の場合、標準MMDスケルトンでIKを解決し、VRM向けに座標系・Aポーズ変換を行う
        if (pose.isMmdPose()) {
            Skeleton3D stdSkel = Skeleton3D::createStandardMmdSkeleton();
            stdSkel.applyPose(pose);
            for (size_t b = 0; b < stdSkel.boneCount(); ++b) {
                const auto* bone = stdSkel.bone(static_cast<int>(b));
                if (bone) {
                    effectivePose.rotations[bone->name] = bone->localRotation;
                }
            }

            // 足首の向き: MMDでは足首の姿勢は足IKボーンの回転を引き継ぐ
            if (pose.rotations.count("左足ＩＫ")) {
                effectivePose.rotations["左足首"] = pose.rotations.at("左足ＩＫ");
            }
            if (pose.rotations.count("右足ＩＫ")) {
                effectivePose.rotations["右足首"] = pose.rotations.at("右足ＩＫ");
            }

            // 体幹の合成: VRMのhipsノード(骨盤) = センター(親) × 下半身(子)
            // Spineノード(胸) = 下半身^-1 × 上半身 (ワールド姿勢をMMDと一致させる)
            QQuaternion rotCenter = pose.rotations.count("センター") ? pose.rotations.at("センター") : QQuaternion();
            QQuaternion rotLower = pose.rotations.count("下半身") ? pose.rotations.at("下半身") : QQuaternion();
            QQuaternion rotUpper = pose.rotations.count("上半身") ? pose.rotations.at("上半身") : QQuaternion();

            effectivePose.rotations["下半身"] = rotCenter * rotLower;
            effectivePose.rotations["hips"] = rotCenter * rotLower;
            effectivePose.rotations.erase("センター"); // hipsを上書きしないよう除外

            effectivePose.rotations["上半身"] = rotLower.inverted() * rotUpper;
            effectivePose.rotations["spine"] = rotLower.inverted() * rotUpper;

            // VRMモデルは全体が Y軸180度回転されているため、
            // MMDのワールド姿勢 Q を VRMのローカルノード空間へ変換:
            // Q_local = R_Y180^-1 * Q * R_Y180 (すなわち xとzの反転)
            QQuaternion qY180 = QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), 180.0f);
            for (auto& [bName, rot] : effectivePose.rotations) {
                rot = qY180.inverted() * rot * qY180;
            }

            // A-pose -> T-pose オフセット調整 (VRMローカル空間での腕の下垂)
            // VRMローカルでは: 右腕が +X、左腕が -X
            // 右腕(+X)を下げる: -35度 Z回転 / 左腕(-X)を下げる: +35度 Z回転
            QQuaternion armOffsetL = QQuaternion::fromAxisAndAngle(QVector3D(0, 0, 1), 35.0f);
            QQuaternion armOffsetR = QQuaternion::fromAxisAndAngle(QVector3D(0, 0, 1), -35.0f);
            if (effectivePose.rotations.count("左腕")) {
                effectivePose.rotations["左腕"] = armOffsetL * effectivePose.rotations["左腕"];
            }
            if (effectivePose.rotations.count("右腕")) {
                effectivePose.rotations["右腕"] = armOffsetR * effectivePose.rotations["右腕"];
            }

            // センター移動量をVRMスケール (1.6 / 22.0 ≈ 0.073) で変換
            if (pose.translations.count("センター")) {
                QVector3D localTrans = qY180.inverted().rotatedVector(pose.translations.at("センター")) * 0.073f;
                effectivePose.translations["センター"] = localTrans;
                effectivePose.translations["下半身"] = localTrans;
                effectivePose.translations["hips"] = localTrans;
            }
        } else if (effectivePose.isVrmaPose) {
            // VRMA (VRMC_vrm_animation) は glTF/VRM 1.0 仕様（+Z正面、右手系）
            // VRM 0.x モデルは内部ノード空間が -Z正面（Y軸180度反転）であるため、
            // 各ボーンの回転および平行移動を Y軸180度反転 (x -> -x, z -> -z) する
            QQuaternion qY180 = QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), 180.0f);
            for (auto& [bName, rot] : effectivePose.rotations) {
                rot = qY180.inverted() * rot * qY180;
            }
            for (auto& [bName, trans] : effectivePose.translations) {
                trans = qY180.inverted().rotatedVector(trans);
            }
        }

        // 1. 各Humanoidボーンの回転・位置をノードに反映
        for (const auto& [bName, rot] : effectivePose.rotations) {
            QString lowerName = bName.toLower();
            auto it = m_humanoidBoneToNode.find(lowerName);
            if (it != m_humanoidBoneToNode.end()) {
                int nodeIdx = it->second;
                if (nodeIdx >= 0 && nodeIdx < static_cast<int>(m_nodes.size())) {
                    m_nodes[nodeIdx].rotation = rot;
                }
            }
        }

        for (const auto& [bName, trans] : effectivePose.translations) {
            QString lowerName = bName.toLower();
            auto it = m_humanoidBoneToNode.find(lowerName);
            if (it != m_humanoidBoneToNode.end()) {
                int nodeIdx = it->second;
                if (nodeIdx >= 0 && nodeIdx < static_cast<int>(m_nodes.size())) {
                    if (effectivePose.isAbsoluteTranslation) {
                        m_nodes[nodeIdx].translation = trans;
                    } else {
                        m_nodes[nodeIdx].translation = m_nodes[nodeIdx].initialTranslation + trans;
                    }
                }
            }
        }

        // 2. ルートノードから順にワールド行列をFKで更新
        updateNodeMatrices();

        // 3. メッシュの頂点をスキニング変形 (CPUレンダラー使用時のみ)
        if (updateCpuSkinning) {
            updateSkinning();
            recalculateBoundingBox();
        }
    } else if (m_skeleton.boneCount() > 0) {
        // MMD (PMD / PMX) 用のスケルトンスキニング
        if (updateCpuSkinning) {
            applySkeletonSkinning();
            recalculateBoundingBox();
        }
    }
}

void ThreeDModel::applySkeletonSkinning() {
    for (auto& mesh : m_meshes) {
        if (mesh.skinWeights.size() != mesh.vertices.size() ||
            mesh.bindVertices.size() != mesh.vertices.size()) {
            continue;
        }

        for (size_t i = 0; i < mesh.vertices.size(); ++i) {
            const auto& bindV = mesh.bindVertices[i];
            const auto& sw = mesh.skinWeights[i];

            QVector3D pos(0, 0, 0);
            QVector3D norm(0, 0, 0);
            float totalWeight = 0.0f;

            for (int k = 0; k < 4; ++k) {
                float w = sw.weights[k];
                if (w <= 1e-4f) continue;
                int bIdx = sw.joints[k];
                const auto* bone = m_skeleton.bone(bIdx);
                if (bone) {
                    pos += bone->combinedMatrix.map(bindV.position) * w;
                    norm += bone->combinedMatrix.mapVector(bindV.normal) * w;
                    totalWeight += w;
                }
            }

            if (totalWeight > 1e-4f) {
                mesh.vertices[i].position = pos;
                mesh.vertices[i].normal = norm.normalized();
            } else {
                mesh.vertices[i].position = bindV.position;
                mesh.vertices[i].normal = bindV.normal;
            }
        }
    }
}

void ThreeDModel::updateNodeMatrices() {
    std::vector<bool> visited(m_nodes.size(), false);

    std::function<void(int)> computeWorld = [&](int idx) {
        if (idx < 0 || idx >= static_cast<int>(m_nodes.size())) return;
        auto& node = m_nodes[idx];
        QMatrix4x4 local;
        local.translate(node.translation);
        local.rotate(node.rotation);
        local.scale(node.scale);

        if (node.parent >= 0 && node.parent < static_cast<int>(m_nodes.size())) {
            if (!visited[node.parent]) computeWorld(node.parent);
            node.worldMatrix = m_nodes[node.parent].worldMatrix * local;
        } else {
            node.worldMatrix = local;
        }
        visited[idx] = true;
    };

    for (size_t i = 0; i < m_nodes.size(); ++i) {
        if (!visited[i]) {
            computeWorld(static_cast<int>(i));
        }
    }
}

void ThreeDModel::updateSkinning() {
    // 各 skin のジョイントスキニング行列を事前計算
    std::vector<std::vector<QMatrix4x4>> skinJointMatrices(m_skins.size());
    for (size_t sIdx = 0; sIdx < m_skins.size(); ++sIdx) {
        const auto& skin = m_skins[sIdx];
        skinJointMatrices[sIdx].resize(skin.joints.size());
        for (size_t j = 0; j < skin.joints.size(); ++j) {
            int nodeIdx = skin.joints[j];
            QMatrix4x4 invBind;
            if (j < skin.inverseBindMatrices.size()) {
                invBind = skin.inverseBindMatrices[j];
            }
            if (nodeIdx >= 0 && nodeIdx < static_cast<int>(m_nodes.size())) {
                skinJointMatrices[sIdx][j] = m_nodes[nodeIdx].worldMatrix * invBind;
            } else {
                skinJointMatrices[sIdx][j].setToIdentity();
            }
        }
    }

    // 各メッシュの頂点をCPUスキニング変形
    for (auto& mesh : m_meshes) {
        if (mesh.skinIndex < 0 || mesh.skinIndex >= static_cast<int>(skinJointMatrices.size())) continue;
        if (mesh.skinWeights.size() != mesh.vertices.size() || mesh.bindVertices.size() != mesh.vertices.size()) continue;

        const auto& jointMats = skinJointMatrices[mesh.skinIndex];

        for (size_t i = 0; i < mesh.vertices.size(); ++i) {
            const auto& bindV = mesh.bindVertices[i];
            const auto& sw = mesh.skinWeights[i];

            QVector3D pos(0, 0, 0);
            QVector3D norm(0, 0, 0);

            for (int k = 0; k < 4; ++k) {
                float w = sw.weights[k];
                if (w <= 1e-4f) continue;
                int jIdx = sw.joints[k];
                if (jIdx >= 0 && jIdx < static_cast<int>(jointMats.size())) {
                    const QMatrix4x4& jm = jointMats[jIdx];
                    pos += jm.map(bindV.position) * w;
                    norm += jm.mapVector(bindV.normal) * w;
                }
            }

            mesh.vertices[i].position = pos;
            mesh.vertices[i].normal = norm.normalized();
        }
    }
}

std::vector<QMatrix4x4> ThreeDModel::getMeshSkinMatrices(int meshIdx) const {
    if (meshIdx < 0 || meshIdx >= static_cast<int>(m_meshes.size())) {
        return {};
    }
    const auto& mesh = m_meshes[meshIdx];

    // glTF / VRM スキン
    if (mesh.skinIndex >= 0 && mesh.skinIndex < static_cast<int>(m_skins.size())) {
        const auto& skin = m_skins[mesh.skinIndex];
        std::vector<QMatrix4x4> jointMats(skin.joints.size());
        for (size_t j = 0; j < skin.joints.size(); ++j) {
            int nodeIdx = skin.joints[j];
            QMatrix4x4 invBind;
            if (j < skin.inverseBindMatrices.size()) {
                invBind = skin.inverseBindMatrices[j];
            }
            if (nodeIdx >= 0 && nodeIdx < static_cast<int>(m_nodes.size())) {
                jointMats[j] = m_nodes[nodeIdx].worldMatrix * invBind;
            } else {
                jointMats[j].setToIdentity();
            }
        }
        return jointMats;
    }

    // PMD / PMX スケルトン
    if (m_skeleton.boneCount() > 0) {
        std::vector<QMatrix4x4> boneMats(m_skeleton.boneCount());
        for (size_t b = 0; b < m_skeleton.boneCount(); ++b) {
            const auto* bone = m_skeleton.bone(static_cast<int>(b));
            if (bone) {
                boneMats[b] = bone->combinedMatrix;
            } else {
                boneMats[b].setToIdentity();
            }
        }
        return boneMats;
    }

    return {};
}

bool ThreeDModel::loadObj(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    std::vector<QVector3D> positions;
    std::vector<QVector3D> normals;
    std::vector<QVector2D> texCoords;

    Mesh3D currentMesh;
    currentMesh.name = "default";
    m_meshes.clear();
    m_materials.clear();

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;

        QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;

        const QString& cmd = parts[0];
        if (cmd == "v" && parts.size() >= 4) {
            positions.emplace_back(parts[1].toFloat(), parts[2].toFloat(), parts[3].toFloat());
        } else if (cmd == "vn" && parts.size() >= 4) {
            normals.emplace_back(parts[1].toFloat(), parts[2].toFloat(), parts[3].toFloat());
        } else if (cmd == "vt" && parts.size() >= 3) {
            texCoords.emplace_back(parts[1].toFloat(), parts[2].toFloat());
        } else if (cmd == "f" && parts.size() >= 4) {
            // 面 (ポリゴンを三角形分割)
            std::vector<Vertex3D> faceVertices;
            for (int i = 1; i < parts.size(); ++i) {
                QStringList vertParts = parts[i].split('/');
                Vertex3D v;
                int pIdx = vertParts[0].toInt();
                if (pIdx > 0 && pIdx <= static_cast<int>(positions.size())) {
                    v.position = positions[pIdx - 1];
                } else if (pIdx < 0) {
                    v.position = positions[positions.size() + pIdx];
                }

                if (vertParts.size() >= 2 && !vertParts[1].isEmpty()) {
                    int tIdx = vertParts[1].toInt();
                    if (tIdx > 0 && tIdx <= static_cast<int>(texCoords.size())) {
                        v.texCoord = texCoords[tIdx - 1];
                    }
                }

                if (vertParts.size() >= 3 && !vertParts[2].isEmpty()) {
                    int nIdx = vertParts[2].toInt();
                    if (nIdx > 0 && nIdx <= static_cast<int>(normals.size())) {
                        v.normal = normals[nIdx - 1];
                    }
                }
                faceVertices.push_back(v);
            }

            // 三角形化 (ファン分割)
            for (size_t i = 1; i + 1 < faceVertices.size(); ++i) {
                uint32_t baseIdx = static_cast<uint32_t>(currentMesh.vertices.size());
                currentMesh.vertices.push_back(faceVertices[0]);
                currentMesh.vertices.push_back(faceVertices[i]);
                currentMesh.vertices.push_back(faceVertices[i + 1]);
                currentMesh.indices.push_back(baseIdx);
                currentMesh.indices.push_back(baseIdx + 1);
                currentMesh.indices.push_back(baseIdx + 2);
            }
        }
    }

    // 法線がない場合の自動計算
    for (size_t i = 0; i < currentMesh.indices.size(); i += 3) {
        auto& v0 = currentMesh.vertices[currentMesh.indices[i]];
        auto& v1 = currentMesh.vertices[currentMesh.indices[i + 1]];
        auto& v2 = currentMesh.vertices[currentMesh.indices[i + 2]];
        if (v0.normal.isNull() && v1.normal.isNull() && v2.normal.isNull()) {
            QVector3D n = QVector3D::crossProduct(v1.position - v0.position, v2.position - v0.position).normalized();
            v0.normal = n;
            v1.normal = n;
            v2.normal = n;
        }
    }

    if (!currentMesh.vertices.empty()) {
        m_meshes.push_back(std::move(currentMesh));
    }

    recalculateBoundingBox();
    return !m_meshes.empty();
}

bool ThreeDModel::loadGltf(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QByteArray fileData = file.readAll();
    file.close();

    QJsonObject gltfRoot;
    QByteArray binBuffer;

    if (fileData.startsWith("glTF")) {
        // Binary glTF (GLB)
        QDataStream stream(fileData);
        stream.setByteOrder(QDataStream::LittleEndian);

        quint32 magic, version, length;
        stream >> magic >> version >> length;
        if (magic != 0x46546C67 || version != 2) return false;

        while (!stream.atEnd()) {
            quint32 chunkLength, chunkType;
            stream >> chunkLength >> chunkType;
            QByteArray chunkData = fileData.mid(stream.device()->pos(), chunkLength);
            stream.skipRawData(chunkLength);

            if (chunkType == 0x4E4F534A) { // JSON
                QJsonDocument doc = QJsonDocument::fromJson(chunkData);
                if (doc.isObject()) gltfRoot = doc.object();
            } else if (chunkType == 0x004E4942) { // BIN
                binBuffer = chunkData;
            }
        }
    } else {
        // JSON glTF
        QJsonDocument doc = QJsonDocument::fromJson(fileData);
        if (!doc.isObject()) return false;
        gltfRoot = doc.object();

        // 別途.binファイルを読み込む
        QJsonArray buffers = gltfRoot["buffers"].toArray();
        if (!buffers.isEmpty()) {
            QString uri = buffers[0].toObject()["uri"].toString();
            QFileInfo fi(filePath);
            QFile binFile(fi.dir().filePath(uri));
            if (binFile.open(QIODevice::ReadOnly)) {
                binBuffer = binFile.readAll();
            }
        }
    }

    if (gltfRoot.isEmpty()) return false;

    QJsonArray accessors = gltfRoot["accessors"].toArray();
    QJsonArray bufferViews = gltfRoot["bufferViews"].toArray();
    QJsonArray meshesJson = gltfRoot["meshes"].toArray();

    m_meshes.clear();

    auto getBufferData = [&](int accessorIndex) -> const char* {
        if (accessorIndex < 0 || accessorIndex >= accessors.size()) return nullptr;
        QJsonObject acc = accessors[accessorIndex].toObject();
        int bvIdx = acc["bufferView"].toInt(-1);
        int accOffset = acc["byteOffset"].toInt(0);
        if (bvIdx < 0 || bvIdx >= bufferViews.size()) return nullptr;
        QJsonObject bv = bufferViews[bvIdx].toObject();
        int bvOffset = bv["byteOffset"].toInt(0);
        int totalOffset = bvOffset + accOffset;
        if (totalOffset < 0 || totalOffset >= binBuffer.size()) return nullptr;
        return binBuffer.constData() + totalOffset;
    };

    for (const auto& meshVal : meshesJson) {
        QJsonObject meshObj = meshVal.toObject();
        QJsonArray prims = meshObj["primitives"].toArray();

        for (const auto& primVal : prims) {
            QJsonObject prim = primVal.toObject();
            QJsonObject attrs = prim["attributes"].toObject();

            int posAccIdx = attrs["POSITION"].toInt(-1);
            if (posAccIdx < 0) continue;

            const float* posData = reinterpret_cast<const float*>(getBufferData(posAccIdx));
            int posCount = accessors[posAccIdx].toObject()["count"].toInt(0);
            if (!posData || posCount <= 0) continue;

            int normAccIdx = attrs["NORMAL"].toInt(-1);
            const float* normData = normAccIdx >= 0 ? reinterpret_cast<const float*>(getBufferData(normAccIdx)) : nullptr;

            int uvAccIdx = attrs["TEXCOORD_0"].toInt(-1);
            const float* uvData = uvAccIdx >= 0 ? reinterpret_cast<const float*>(getBufferData(uvAccIdx)) : nullptr;

            Mesh3D subMesh;
            subMesh.vertices.resize(posCount);
            for (int i = 0; i < posCount; ++i) {
                subMesh.vertices[i].position = QVector3D(posData[i * 3], posData[i * 3 + 1], posData[i * 3 + 2]);
                if (normData) {
                    subMesh.vertices[i].normal = QVector3D(normData[i * 3], normData[i * 3 + 1], normData[i * 3 + 2]);
                }
                if (uvData) {
                    subMesh.vertices[i].texCoord = QVector2D(uvData[i * 2], uvData[i * 2 + 1]);
                }
            }

            int idxAccIdx = prim["indices"].toInt(-1);
            if (idxAccIdx >= 0) {
                int idxCount = accessors[idxAccIdx].toObject()["count"].toInt(0);
                int compType = accessors[idxAccIdx].toObject()["componentType"].toInt(5123);
                const char* rawIdxData = getBufferData(idxAccIdx);
                if (rawIdxData && idxCount > 0) {
                    subMesh.indices.reserve(idxCount);
                    if (compType == 5123) { // UNSIGNED_SHORT
                        const quint16* sData = reinterpret_cast<const quint16*>(rawIdxData);
                        for (int i = 0; i < idxCount; ++i) subMesh.indices.push_back(sData[i]);
                    } else if (compType == 5125) { // UNSIGNED_INT
                        const quint32* iData = reinterpret_cast<const quint32*>(rawIdxData);
                        for (int i = 0; i < idxCount; ++i) subMesh.indices.push_back(iData[i]);
                    }
                }
            } else {
                subMesh.indices.resize(posCount);
                for (int i = 0; i < posCount; ++i) subMesh.indices[i] = i;
            }

            m_meshes.push_back(std::move(subMesh));
        }
    }

    recalculateBoundingBox();
    return !m_meshes.empty();
}

std::shared_ptr<ThreeDModel> ThreeDModel::createCube(float size) {
    auto model = std::make_shared<ThreeDModel>();
    model->setName("Cube");
    Mesh3D mesh;
    mesh.name = "CubeMesh";

    float h = size * 0.5f;

    struct FaceDef {
        QVector3D n;
        QVector3D p0, p1, p2, p3;
    };

    FaceDef faces[6] = {
        { QVector3D(0, 0, 1),  QVector3D(-h, -h,  h), QVector3D( h, -h,  h), QVector3D( h,  h,  h), QVector3D(-h,  h,  h) }, // 前
        { QVector3D(0, 0, -1), QVector3D( h, -h, -h), QVector3D(-h, -h, -h), QVector3D(-h,  h, -h), QVector3D( h,  h, -h) }, // 後
        { QVector3D(-1, 0, 0), QVector3D(-h, -h, -h), QVector3D(-h, -h,  h), QVector3D(-h,  h,  h), QVector3D(-h,  h, -h) }, // 左
        { QVector3D(1, 0, 0),  QVector3D( h, -h,  h), QVector3D( h, -h, -h), QVector3D( h,  h, -h), QVector3D( h,  h,  h) }, // 右
        { QVector3D(0, 1, 0),  QVector3D(-h,  h,  h), QVector3D( h,  h,  h), QVector3D( h,  h, -h), QVector3D(-h,  h, -h) }, // 上
        { QVector3D(0, -1, 0), QVector3D(-h, -h, -h), QVector3D( h, -h, -h), QVector3D( h, -h,  h), QVector3D(-h, -h,  h) }  // 下
    };

    for (int i = 0; i < 6; ++i) {
        uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back({ faces[i].p0, faces[i].n, QVector2D(0, 0) });
        mesh.vertices.push_back({ faces[i].p1, faces[i].n, QVector2D(1, 0) });
        mesh.vertices.push_back({ faces[i].p2, faces[i].n, QVector2D(1, 1) });
        mesh.vertices.push_back({ faces[i].p3, faces[i].n, QVector2D(0, 1) });

        mesh.indices.push_back(base);
        mesh.indices.push_back(base + 1);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 3);
    }

    model->meshes().push_back(std::move(mesh));
    model->recalculateBoundingBox();
    return model;
}

std::shared_ptr<ThreeDModel> ThreeDModel::createGrid(float size, int divisions) {
    auto model = std::make_shared<ThreeDModel>();
    model->setName("Grid");
    Mesh3D mesh;
    mesh.name = "GridMesh";

    float half = size * 0.5f;
    float step = size / divisions;
    float thick = 0.02f; // 細長い板ポリゴンとして描画

    for (int i = 0; i <= divisions; ++i) {
        float pos = -half + i * step;

        // X方向ライン
        uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back({ QVector3D(-half, 0, pos - thick), QVector3D(0, 1, 0), QVector2D(0, 0) });
        mesh.vertices.push_back({ QVector3D( half, 0, pos - thick), QVector3D(0, 1, 0), QVector2D(1, 0) });
        mesh.vertices.push_back({ QVector3D( half, 0, pos + thick), QVector3D(0, 1, 0), QVector2D(1, 1) });
        mesh.vertices.push_back({ QVector3D(-half, 0, pos + thick), QVector3D(0, 1, 0), QVector2D(0, 1) });
        mesh.indices.push_back(base);
        mesh.indices.push_back(base + 1);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 3);

        // Z方向ライン
        base = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back({ QVector3D(pos - thick, 0, -half), QVector3D(0, 1, 0), QVector2D(0, 0) });
        mesh.vertices.push_back({ QVector3D(pos + thick, 0, -half), QVector3D(0, 1, 0), QVector2D(1, 0) });
        mesh.vertices.push_back({ QVector3D(pos + thick, 0,  half), QVector3D(0, 1, 0), QVector2D(1, 1) });
        mesh.vertices.push_back({ QVector3D(pos - thick, 0,  half), QVector3D(0, 1, 0), QVector2D(0, 1) });
        mesh.indices.push_back(base);
        mesh.indices.push_back(base + 1);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 3);
    }

    model->meshes().push_back(std::move(mesh));
    model->recalculateBoundingBox();
    return model;
}

static QQuaternion getNodeWorldRotationHelper(const std::vector<ThreeDModel::ModelNode>& nodes, const QQuaternion& modelRot, int idx) {
    if (idx < 0 || idx >= static_cast<int>(nodes.size())) return modelRot;
    QQuaternion rot = nodes[idx].rotation;
    int p = nodes[idx].parent;
    while (p >= 0 && p < static_cast<int>(nodes.size())) {
        rot = nodes[p].rotation * rot;
        p = nodes[p].parent;
    }
    return modelRot * rot;
}

static void updateNodeBranchWorldMatricesHelper(std::vector<ThreeDModel::ModelNode>& nodes, int idx) {
    if (idx < 0 || idx >= static_cast<int>(nodes.size())) return;
    auto& node = nodes[idx];
    QMatrix4x4 local;
    local.translate(node.translation);
    local.rotate(node.rotation);
    local.scale(node.scale);

    if (node.parent >= 0 && node.parent < static_cast<int>(nodes.size())) {
        node.worldMatrix = nodes[node.parent].worldMatrix * local;
    } else {
        node.worldMatrix = local;
    }

    for (int childIdx : node.children) {
        updateNodeBranchWorldMatricesHelper(nodes, childIdx);
    }
}

void ThreeDModel::initSpringBones() {
    for (auto& group : m_springBoneGroups) {
        group.joints.clear();

        std::function<void(int)> traverse = [&](int nodeIdx) {
            if (nodeIdx < 0 || nodeIdx >= static_cast<int>(m_nodes.size())) return;
            const auto& node = m_nodes[nodeIdx];

            if (!node.children.empty()) {
                // 最初の子ノードを先端(Tail)の基準とする
                int cIdx = node.children[0];
                if (cIdx >= 0 && cIdx < static_cast<int>(m_nodes.size())) {
                    SpringBoneJoint joint;
                    joint.nodeIdx = nodeIdx;
                    joint.childNodeIdx = cIdx;
                    joint.localTail = m_nodes[cIdx].initialTranslation;
                    joint.boneLength = joint.localTail.length();
                    if (joint.boneLength < 1e-4f) {
                        joint.boneLength = 0.07f;
                        joint.localTail = QVector3D(0.0f, -0.07f, 0.0f);
                    }
                    group.joints.push_back(joint);
                }

                // 子ノードを再帰探索
                for (int c : node.children) {
                    traverse(c);
                }
            }
            // 子ノードがない末端（Tip/Leaf）ノードは親のTailとしてのみ機能し、自身は回転させない
        };

        for (int rNode : group.rootNodes) {
            traverse(rNode);
        }
    }
    resetSpringBones();
}

void ThreeDModel::resetSpringBones() {
    updateNodeMatrices();
    for (auto& group : m_springBoneGroups) {
        for (auto& joint : group.joints) {
            if (joint.nodeIdx < 0 || joint.nodeIdx >= static_cast<int>(m_nodes.size())) continue;
            auto& node = m_nodes[joint.nodeIdx];
            QQuaternion parentWorldRot = (node.parent >= 0) ?
                getNodeWorldRotationHelper(m_nodes, m_rotation, node.parent) : m_rotation;
            QVector3D worldPos = node.worldMatrix.map(QVector3D(0.0f, 0.0f, 0.0f));
            QVector3D baseDir = (parentWorldRot * node.initialRotation).rotatedVector(joint.localTail);
            QVector3D target = worldPos + baseDir;

            joint.currentTail = target;
            joint.prevTail = target;
            joint.initialized = true;
        }
    }
}

void ThreeDModel::updateSpringBones(float dt, bool updateCpuSkinning) {
    if (m_springBoneGroups.empty() || m_nodes.empty()) return;

    dt = std::clamp(dt, 0.001f, 0.05f);

    struct ActiveCollider {
        QVector3D center;
        float radius;
    };
    std::vector<std::vector<ActiveCollider>> activeColGroups(m_springColliderGroups.size());
    for (size_t g = 0; g < m_springColliderGroups.size(); ++g) {
        const auto& cg = m_springColliderGroups[g];
        if (cg.nodeIdx >= 0 && cg.nodeIdx < static_cast<int>(m_nodes.size())) {
            const auto& nMat = m_nodes[cg.nodeIdx].worldMatrix;
            for (const auto& col : cg.colliders) {
                ActiveCollider ac;
                ac.center = nMat.map(col.offset);
                ac.radius = col.radius;
                activeColGroups[g].push_back(ac);
            }
        }
    }

    for (auto& group : m_springBoneGroups) {
        std::vector<ActiveCollider> colliders;
        for (int cgIdx : group.colliderGroupIndices) {
            if (cgIdx >= 0 && cgIdx < static_cast<int>(activeColGroups.size())) {
                const auto& cols = activeColGroups[cgIdx];
                colliders.insert(colliders.end(), cols.begin(), cols.end());
            }
        }

        // 重力方向をモデルローカル空間に変換
        QVector3D localGravityDir = m_rotation.inverted().rotatedVector(group.gravityDir);

        for (auto& joint : group.joints) {
            if (joint.nodeIdx < 0 || joint.nodeIdx >= static_cast<int>(m_nodes.size())) continue;
            auto& node = m_nodes[joint.nodeIdx];

            // 親ノードの現在のモデルローカル累積回転 (m_rotationを含まない)
            QQuaternion parentModelRot = (node.parent >= 0) ?
                getNodeWorldRotationHelper(m_nodes, QQuaternion(), node.parent) : QQuaternion();

            // nodeのモデルローカル位置
            QVector3D worldPos = node.worldMatrix.map(QVector3D(0.0f, 0.0f, 0.0f));

            // 基準となる初期姿勢での目標先端方向・位置 (モデルローカル空間)
            QVector3D baseDir = (parentModelRot * node.initialRotation).rotatedVector(joint.localTail);
            QVector3D target = worldPos + baseDir;

            if (!joint.initialized) {
                joint.currentTail = target;
                joint.prevTail = target;
                joint.initialized = true;
            }

            // Verlet積分
            // 1. 慣性 (前フレームの速度 × 減衰) - 落ち着いた揺れのため慣性影響を0.75倍にスケーリング
            QVector3D inertia = (joint.currentTail - joint.prevTail) * ((1.0f - group.dragForce) * 0.75f);
            // 2. 剛性復元力: 目標位置 target への引き戻し力
            float stiffFactor = std::clamp(group.stiffness * dt * 30.0f, 0.1f, 0.95f);
            QVector3D stiffness = (target - joint.currentTail) * stiffFactor;
            // 3. 重力 (モデルローカル空間)
            QVector3D gravity = localGravityDir * (group.gravityPower * dt * 30.0f);

            QVector3D nextTail = joint.currentTail + inertia + stiffness + gravity;

            // 長さ拘束
            QVector3D toNext = nextTail - worldPos;
            if (toNext.lengthSquared() < 1e-6f) {
                toNext = baseDir;
            }
            toNext = toNext.normalized();
            nextTail = worldPos + toNext * joint.boneLength;

            // コライダー球押し出し (頭部・胸等へのめり込み防止)
            for (const auto& col : colliders) {
                float minRadius = col.radius + group.hitRadius;
                QVector3D diff = nextTail - col.center;
                float distSq = diff.lengthSquared();
                if (distSq < minRadius * minRadius && distSq > 1e-6f) {
                    float dist = std::sqrt(distSq);
                    nextTail = col.center + (diff / dist) * minRadius;
                    nextTail = worldPos + (nextTail - worldPos).normalized() * joint.boneLength;
                }
            }

            // 逆立ち・過剰揺れ防止ガード: 基準方向 baseDir に対する最大曲げ角度を 25度に制限
            toNext = (nextTail - worldPos).normalized();
            float cosAngle = QVector3D::dotProduct(baseDir.normalized(), toNext);
            constexpr float maxAngleDeg = 25.0f;
            const float minCos = std::cos(qDegreesToRadians(maxAngleDeg));
            if (cosAngle < minCos) {
                QVector3D normBase = baseDir.normalized();
                QVector3D ortho = toNext - normBase * cosAngle;
                if (ortho.lengthSquared() > 1e-6f) {
                    float sinMax = std::sin(qDegreesToRadians(maxAngleDeg));
                    toNext = (normBase * minCos + ortho.normalized() * sinMax).normalized();
                } else {
                    toNext = normBase;
                }
                nextTail = worldPos + toNext * joint.boneLength;
            }

            joint.prevTail = joint.currentTail;
            joint.currentTail = nextTail;

            // 回転差分を初期姿勢を基準にして計算・適用
            QVector3D vTail = nextTail - worldPos;
            if (baseDir.lengthSquared() > 1e-6f && vTail.lengthSquared() > 1e-6f) {
                QQuaternion deltaRot = QQuaternion::rotationTo(baseDir.normalized(), vTail.normalized());
                // 多段連鎖による過剰な暴れを防ぎ自然なふんわり感を出すため0.7倍にブレンド
                deltaRot = QQuaternion::slerp(QQuaternion(), deltaRot, 0.70f);

                // 物理回転を初期姿勢に乗算
                QQuaternion newWorldRot = deltaRot * (parentModelRot * node.initialRotation);

                node.rotation = parentModelRot.inverted() * newWorldRot;
                updateNodeBranchWorldMatricesHelper(m_nodes, joint.nodeIdx);
            }
        }
    }

    if (updateCpuSkinning) {
        updateSkinning();
    }
}

} // namespace Kaburagi
