#include "ThreeDMmdParser.hpp"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QImage>
#include <QDataStream>
#include <QStringDecoder>
#include <QDebug>
#include <cstring>
#include <unordered_map>
#include <algorithm>

namespace Kaburagi {

bool ThreeDMmdParser::loadPmd(const QString& filePath, ThreeDModel& model, Skeleton3D& skeleton) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    if (data.size() < 283) return false; // ヘッダー最低サイズ
    const char* ptr = data.constData();
    const char* end = ptr + data.size();

    // マジック "Pmd"
    if (std::memcmp(ptr, "Pmd", 3) != 0) {
        return false;
    }
    ptr += 3;

    // バージョン float (通常 1.0f)
    float version = *reinterpret_cast<const float*>(ptr);
    ptr += 4;
    Q_UNUSED(version);

    auto sjisDecoder = QStringDecoder(QStringDecoder::System);

    // モデル名 (20 bytes)
    char modelNameBuf[21] = {0};
    std::memcpy(modelNameBuf, ptr, 20);
    QString modelName = sjisDecoder(QByteArray(modelNameBuf));
    if (modelName.isEmpty()) modelName = QString::fromLatin1(modelNameBuf);
    model.setName(modelName.trimmed());
    ptr += 20;

    // コメント (256 bytes)
    ptr += 256;

    if (ptr + 4 > end) return false;
    uint32_t vertCount = *reinterpret_cast<const uint32_t*>(ptr);
    ptr += 4;

    // 頂点構造: 38 bytes 各
    // pos(12) + normal(12) + uv(8) + bone(4) + weight(1) + edge(1)
    if (ptr + vertCount * 38 > end) return false;

    Mesh3D mesh;
    mesh.name = "PMD_Mesh";
    mesh.vertices.resize(vertCount);

    std::vector<VertexSkinWeight> allSkinWeights(vertCount);
    for (uint32_t i = 0; i < vertCount; ++i) {
        float px = *reinterpret_cast<const float*>(ptr);
        float py = *reinterpret_cast<const float*>(ptr + 4);
        float pz = *reinterpret_cast<const float*>(ptr + 8);

        float nx = *reinterpret_cast<const float*>(ptr + 12);
        float ny = *reinterpret_cast<const float*>(ptr + 16);
        float nz = *reinterpret_cast<const float*>(ptr + 20);

        float u = *reinterpret_cast<const float*>(ptr + 24);
        float v = *reinterpret_cast<const float*>(ptr + 28);

        uint16_t b0 = *reinterpret_cast<const uint16_t*>(ptr + 32);
        uint16_t b1 = *reinterpret_cast<const uint16_t*>(ptr + 34);
        uint8_t w0 = *reinterpret_cast<const uint8_t*>(ptr + 36);

        // MMD座標系(右手系) -> OpenGL座標系(Z反転)
        mesh.vertices[i].position = QVector3D(px, py, -pz);
        mesh.vertices[i].normal = QVector3D(nx, ny, -nz);
        mesh.vertices[i].texCoord = QVector2D(u, v);
        mesh.vertices[i].color = QVector4D(1.0f, 1.0f, 1.0f, 1.0f);

        allSkinWeights[i].joints[0] = b0;
        allSkinWeights[i].joints[1] = b1;
        float weight0 = w0 / 100.0f;
        allSkinWeights[i].weights[0] = weight0;
        allSkinWeights[i].weights[1] = 1.0f - weight0;

        ptr += 38;
    }

    // 面インデックス数
    if (ptr + 4 > end) return false;
    uint32_t indexCount = *reinterpret_cast<const uint32_t*>(ptr);
    ptr += 4;

    if (ptr + indexCount * 2 > end) return false;
    std::vector<uint32_t> allIndices(indexCount);
    for (uint32_t i = 0; i < indexCount; i += 3) {
        // Z反転に伴いワインディング順を反転 (0, 2, 1)
        uint16_t i0 = *reinterpret_cast<const uint16_t*>(ptr);
        uint16_t i1 = *reinterpret_cast<const uint16_t*>(ptr + 2);
        uint16_t i2 = *reinterpret_cast<const uint16_t*>(ptr + 4);
        ptr += 6;

        allIndices[i] = i0;
        allIndices[i + 1] = i2;
        allIndices[i + 2] = i1;
    }

    // 材質数
    if (ptr + 4 > end) return false;
    uint32_t matCount = *reinterpret_cast<const uint32_t*>(ptr);
    ptr += 4;

    QFileInfo pmdFi(filePath);
    QDir pmdDir = pmdFi.dir();

    std::vector<Material3D> pmdMaterials;
    pmdMaterials.reserve(matCount);
    std::vector<Mesh3D> pmdMeshes;
    pmdMeshes.reserve(matCount);

    uint32_t pmdIndexOffset = 0;

    for (uint32_t m = 0; m < matCount; ++m) {
        if (ptr + 70 > end) break;
        float diffR = *reinterpret_cast<const float*>(ptr);
        float diffG = *reinterpret_cast<const float*>(ptr + 4);
        float diffB = *reinterpret_cast<const float*>(ptr + 8);
        float diffA = *reinterpret_cast<const float*>(ptr + 12);
        ptr += 16;
        ptr += 16; // spec 12 + power 4
        ptr += 12; // amb 12
        ptr += 2;  // toon 1 + edge 1
        uint32_t faceVertCount = *reinterpret_cast<const uint32_t*>(ptr);
        ptr += 4;

        char texBuf[21] = {0};
        std::memcpy(texBuf, ptr, 20);
        ptr += 20;
        QString texName = QString(sjisDecoder(QByteArray(texBuf))).trimmed();
        if (texName.contains('*')) {
            texName = texName.split('*')[0].trimmed();
        }

        Material3D mat;
        mat.name = QString("Material_%1").arg(m);
        mat.diffuseColor = QColor::fromRgbF(
            std::clamp(diffR, 0.0f, 1.0f),
            std::clamp(diffG, 0.0f, 1.0f),
            std::clamp(diffB, 0.0f, 1.0f),
            std::clamp(diffA, 0.0f, 1.0f)
        );
        if (!texName.isEmpty()) {
            texName.replace('\\', '/');
            QString fullTex = pmdDir.filePath(texName);
            if (QFile::exists(fullTex)) {
                mat.diffuseTexture = QImage(fullTex);
            }
        }
        pmdMaterials.push_back(std::move(mat));

        if (faceVertCount > 0 && pmdIndexOffset + faceVertCount <= allIndices.size()) {
            Mesh3D subMesh;
            subMesh.name = QString("PMD_Mesh_%1").arg(m);
            subMesh.materialIndex = static_cast<int>(m);
            std::unordered_map<uint32_t, uint32_t> oldToNew;
            subMesh.indices.reserve(faceVertCount);

            for (uint32_t k = 0; k < faceVertCount; ++k) {
                uint32_t oldIdx = allIndices[pmdIndexOffset + k];
                if (oldIdx < mesh.vertices.size()) {
                    auto it = oldToNew.find(oldIdx);
                    if (it == oldToNew.end()) {
                        uint32_t newIdx = static_cast<uint32_t>(subMesh.vertices.size());
                        oldToNew[oldIdx] = newIdx;
                        subMesh.vertices.push_back(mesh.vertices[oldIdx]);
                        subMesh.skinWeights.push_back(allSkinWeights[oldIdx]);
                        subMesh.indices.push_back(newIdx);
                    } else {
                        subMesh.indices.push_back(it->second);
                    }
                }
            }
            subMesh.bindVertices = subMesh.vertices;
            pmdMeshes.push_back(std::move(subMesh));
            pmdIndexOffset += faceVertCount;
        }
    }

    // ボーン数
    if (ptr + 2 <= end) {
        uint16_t boneCount = *reinterpret_cast<const uint16_t*>(ptr);
        ptr += 2;

        // ボーン構造: 39 bytes 各 (名前20, 親2, 子2, 種類1, IK2, 位置12)
        for (uint16_t b = 0; b < boneCount; ++b) {
            if (ptr + 39 > end) break;

            char bNameBuf[21] = {0};
            std::memcpy(bNameBuf, ptr, 20);
            int len = 0;
            while (len < 20 && bNameBuf[len] != '\0') ++len;
            QString bName = QString(sjisDecoder(QByteArray(bNameBuf, len))).trimmed();
            if (bName.isEmpty()) bName = QString::fromLatin1(bNameBuf, len).trimmed();

            int16_t parentIdx = *reinterpret_cast<const int16_t*>(ptr + 20);

            float bx = *reinterpret_cast<const float*>(ptr + 27);
            float by = *reinterpret_cast<const float*>(ptr + 31);
            float bz = *reinterpret_cast<const float*>(ptr + 35);

            skeleton.addBone(bName, parentIdx, QVector3D(bx, by, -bz));
            ptr += 39;
        }
    }

    // IKテーブル
    if (ptr + 2 <= end) {
        uint16_t ikCount = *reinterpret_cast<const uint16_t*>(ptr);
        ptr += 2;
        for (uint16_t ikIdx = 0; ikIdx < ikCount; ++ikIdx) {
            if (ptr + 11 > end) break;
            uint16_t ikBone = *reinterpret_cast<const uint16_t*>(ptr);
            uint16_t targetBone = *reinterpret_cast<const uint16_t*>(ptr + 2);
            uint8_t chainLen = *reinterpret_cast<const uint8_t*>(ptr + 4);
            uint16_t iterations = *reinterpret_cast<const uint16_t*>(ptr + 5);
            float weight = *reinterpret_cast<const float*>(ptr + 7);
            ptr += 11;

            if (ptr + chainLen * 2 > end) break;
            ThreeDIk ik;
            ik.ikBoneIndex = ikBone;
            ik.targetBoneIndex = targetBone;
            ik.iterations = iterations;
            ik.angleLimit = weight * 4.0f;

            for (uint8_t l = 0; l < chainLen; ++l) {
                uint16_t linkBone = *reinterpret_cast<const uint16_t*>(ptr);
                ptr += 2;
                ThreeDIkLink link;
                link.boneIndex = linkBone;
                const auto* bPtr = skeleton.bone(linkBone);
                if (bPtr && bPtr->name.contains(QString::fromUtf8("ひざ"))) {
                    link.hasLimit = true;
                    link.minLimit = QVector3D(qDegreesToRadians(0.5f), 0.0f, 0.0f);
                    link.maxLimit = QVector3D(qDegreesToRadians(180.0f), 0.0f, 0.0f);
                }
                ik.links.push_back(link);
            }
            skeleton.addIk(ik);
        }
    }

    if (pmdMeshes.empty()) {
        mesh.bindVertices = mesh.vertices;
        mesh.skinWeights = std::move(allSkinWeights);
        pmdMeshes.push_back(std::move(mesh));
    }

    model.meshes() = std::move(pmdMeshes);
    model.materials() = std::move(pmdMaterials);
    model.recalculateBoundingBox();
    return true;
}

// PMX パーサー
bool ThreeDMmdParser::loadPmx(const QString& filePath, ThreeDModel& model, Skeleton3D& skeleton) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    if (data.size() < 12) return false;
    const char* ptr = data.constData();
    const char* end = ptr + data.size();

    // マジック "PMX "
    if (std::memcmp(ptr, "PMX ", 4) != 0) {
        return false;
    }
    ptr += 4;

    float version = *reinterpret_cast<const float*>(ptr);
    ptr += 4;
    Q_UNUSED(version);

    uint8_t globalCount = *reinterpret_cast<const uint8_t*>(ptr);
    ptr += 1;
    if (globalCount < 8 || ptr + globalCount > end) return false;

    uint8_t encode = ptr[0]; // 0: UTF-16LE, 1: UTF-8
    uint8_t additionalUV = ptr[1];
    uint8_t vertIndexSize = ptr[2];
    uint8_t texIndexSize = ptr[3];
    uint8_t matIndexSize = ptr[4];
    uint8_t boneIndexSize = ptr[5];
    ptr += globalCount;

    auto readPmxString = [&](const char*& p) -> QString {
        if (p + 4 > end) return QString();
        int32_t len = *reinterpret_cast<const int32_t*>(p);
        p += 4;
        if (len <= 0 || p + len > end) return QString();
        QByteArray sBytes(p, len);
        p += len;
        if (encode == 1) {
            return QString::fromUtf8(sBytes);
        } else {
            return QString::fromUtf16(reinterpret_cast<const char16_t*>(sBytes.constData()), len / 2);
        }
    };

    QString modelName = readPmxString(ptr);
    QString modelNameEn = readPmxString(ptr);
    QString comment = readPmxString(ptr);
    QString commentEn = readPmxString(ptr);
    model.setName(modelName.isEmpty() ? modelNameEn : modelName);

    auto readIndex = [&](const char*& p) -> uint32_t {
        if (vertIndexSize == 1) {
            uint8_t val = *reinterpret_cast<const uint8_t*>(p);
            p += 1;
            return val;
        } else if (vertIndexSize == 2) {
            uint16_t val = *reinterpret_cast<const uint16_t*>(p);
            p += 2;
            return val;
        } else {
            uint32_t val = *reinterpret_cast<const uint32_t*>(p);
            p += 4;
            return val;
        }
    };

    auto readSignedIndex = [&](const char*& p, int size) -> int {
        if (size == 1) {
            int8_t val = *reinterpret_cast<const int8_t*>(p);
            p += 1;
            return val;
        } else if (size == 2) {
            int16_t val = *reinterpret_cast<const int16_t*>(p);
            p += 2;
            return val;
        } else {
            int32_t val = *reinterpret_cast<const int32_t*>(p);
            p += 4;
            return val;
        }
    };

    if (ptr + 4 > end) return false;
    uint32_t vertCount = *reinterpret_cast<const uint32_t*>(ptr);
    ptr += 4;

    std::vector<Vertex3D> allVertices(vertCount);
    std::vector<VertexSkinWeight> allSkinWeights(vertCount);

    for (uint32_t i = 0; i < vertCount; ++i) {
        if (ptr + 32 > end) return false;
        float px = *reinterpret_cast<const float*>(ptr);
        float py = *reinterpret_cast<const float*>(ptr + 4);
        float pz = *reinterpret_cast<const float*>(ptr + 8);

        float nx = *reinterpret_cast<const float*>(ptr + 12);
        float ny = *reinterpret_cast<const float*>(ptr + 16);
        float nz = *reinterpret_cast<const float*>(ptr + 20);

        float u = *reinterpret_cast<const float*>(ptr + 24);
        float v = *reinterpret_cast<const float*>(ptr + 28);
        ptr += 32;

        // 追加UV
        ptr += additionalUV * 16;

        // ウェイト種別 (0: BDEF1, 1: BDEF2, 2: BDEF4, 3: SDEF, 4: QDEF)
        if (ptr >= end) return false;
        uint8_t weightType = *reinterpret_cast<const uint8_t*>(ptr);
        ptr += 1;

        VertexSkinWeight sw;
        if (weightType == 0) {
            int b0 = readSignedIndex(ptr, boneIndexSize);
            sw.joints[0] = static_cast<uint16_t>(std::max(0, b0));
            sw.weights[0] = 1.0f;
        } else if (weightType == 1) {
            int b0 = readSignedIndex(ptr, boneIndexSize);
            int b1 = readSignedIndex(ptr, boneIndexSize);
            float w = *reinterpret_cast<const float*>(ptr);
            ptr += 4;
            sw.joints[0] = static_cast<uint16_t>(std::max(0, b0));
            sw.joints[1] = static_cast<uint16_t>(std::max(0, b1));
            sw.weights[0] = w;
            sw.weights[1] = 1.0f - w;
        } else if (weightType == 2 || weightType == 4) {
            for (int k = 0; k < 4; ++k) {
                int b = readSignedIndex(ptr, boneIndexSize);
                sw.joints[k] = static_cast<uint16_t>(std::max(0, b));
            }
            for (int k = 0; k < 4; ++k) {
                sw.weights[k] = *reinterpret_cast<const float*>(ptr);
                ptr += 4;
            }
        } else if (weightType == 3) {
            int b0 = readSignedIndex(ptr, boneIndexSize);
            int b1 = readSignedIndex(ptr, boneIndexSize);
            float w = *reinterpret_cast<const float*>(ptr);
            ptr += 4;
            sw.joints[0] = static_cast<uint16_t>(std::max(0, b0));
            sw.joints[1] = static_cast<uint16_t>(std::max(0, b1));
            sw.weights[0] = w;
            sw.weights[1] = 1.0f - w;
            ptr += 36;
        }
        allSkinWeights[i] = sw;

        // エッジ倍率 float
        ptr += 4;

        allVertices[i].position = QVector3D(px, py, -pz);
        allVertices[i].normal = QVector3D(nx, ny, -nz);
        allVertices[i].texCoord = QVector2D(u, v);
        allVertices[i].color = QVector4D(1.0f, 1.0f, 1.0f, 1.0f);
    }

    // 面インデックス数
    if (ptr + 4 > end) return false;
    uint32_t indexCount = *reinterpret_cast<const uint32_t*>(ptr);
    ptr += 4;

    std::vector<uint32_t> allIndices(indexCount);
    for (uint32_t i = 0; i < indexCount; i += 3) {
        if (ptr >= end) break;
        uint32_t i0 = readIndex(ptr);
        uint32_t i1 = readIndex(ptr);
        uint32_t i2 = readIndex(ptr);

        allIndices[i] = i0;
        allIndices[i + 1] = i2;
        allIndices[i + 2] = i1;
    }

    // テクスチャテーブル
    if (ptr + 4 > end) return false;
    uint32_t texCount = *reinterpret_cast<const uint32_t*>(ptr);
    ptr += 4;

    QFileInfo pmxFileInfo(filePath);
    QDir pmxDir = pmxFileInfo.dir();

    std::vector<QImage> loadedTextures(texCount);
    for (uint32_t t = 0; t < texCount; ++t) {
        QString texRelPath = readPmxString(ptr);
        texRelPath.replace('\\', '/');
        while (texRelPath.startsWith("./")) {
            texRelPath = texRelPath.mid(2);
        }

        QString fullPath = pmxDir.filePath(texRelPath);
        if (QFile::exists(fullPath)) {
            loadedTextures[t] = QImage(fullPath);
        } else {
            QFileInfo tFi(texRelPath);
            QString fallback = pmxDir.filePath(tFi.fileName());
            if (QFile::exists(fallback)) {
                loadedTextures[t] = QImage(fallback);
            }
        }
    }

    // マテリアルテーブル
    if (ptr + 4 > end) return false;
    uint32_t matCount = *reinterpret_cast<const uint32_t*>(ptr);
    ptr += 4;

    std::vector<Material3D> loadedMaterials;
    loadedMaterials.reserve(matCount);
    std::vector<Mesh3D> loadedMeshes;
    loadedMeshes.reserve(matCount);

    uint32_t currentIndexOffset = 0;

    for (uint32_t m = 0; m < matCount; ++m) {
        if (ptr >= end) break;
        QString matName = readPmxString(ptr);
        QString matNameEn = readPmxString(ptr);
        if (matName.isEmpty()) matName = matNameEn;
        if (matName.isEmpty()) matName = QString("Material_%1").arg(m);

        if (ptr + 64 > end) break;
        float diffR = *reinterpret_cast<const float*>(ptr);
        float diffG = *reinterpret_cast<const float*>(ptr + 4);
        float diffB = *reinterpret_cast<const float*>(ptr + 8);
        float diffA = *reinterpret_cast<const float*>(ptr + 12);
        ptr += 16;

        float specR = *reinterpret_cast<const float*>(ptr);
        float specG = *reinterpret_cast<const float*>(ptr + 4);
        float specB = *reinterpret_cast<const float*>(ptr + 8);
        float specPower = *reinterpret_cast<const float*>(ptr + 12);
        ptr += 16;

        float ambR = *reinterpret_cast<const float*>(ptr);
        float ambG = *reinterpret_cast<const float*>(ptr + 4);
        float ambB = *reinterpret_cast<const float*>(ptr + 8);
        ptr += 12;

        uint8_t drawFlags = *reinterpret_cast<const uint8_t*>(ptr);
        ptr += 1;

        ptr += 16; // edgeColor
        ptr += 4;  // edgeSize

        int texIdx = readSignedIndex(ptr, texIndexSize);
        int sphereTexIdx = readSignedIndex(ptr, texIndexSize);
        uint8_t sphereMode = *reinterpret_cast<const uint8_t*>(ptr);
        ptr += 1;

        uint8_t sharedToon = *reinterpret_cast<const uint8_t*>(ptr);
        ptr += 1;
        if (sharedToon == 0) {
            int toonTexIdx = readSignedIndex(ptr, texIndexSize);
        } else {
            uint8_t toonIdx = *reinterpret_cast<const uint8_t*>(ptr);
            ptr += 1;
        }

        QString memo = readPmxString(ptr);

        if (ptr + 4 > end) break;
        int32_t matIndexCount = *reinterpret_cast<const int32_t*>(ptr);
        ptr += 4;

        Material3D mat;
        mat.name = matName;
        mat.diffuseColor = QColor::fromRgbF(
            std::clamp(diffR, 0.0f, 1.0f),
            std::clamp(diffG, 0.0f, 1.0f),
            std::clamp(diffB, 0.0f, 1.0f),
            std::clamp(diffA, 0.0f, 1.0f)
        );
        if (texIdx >= 0 && texIdx < static_cast<int>(loadedTextures.size())) {
            mat.diffuseTexture = loadedTextures[texIdx];
        }
        loadedMaterials.push_back(std::move(mat));

        // サブメッシュ抽出
        if (matIndexCount > 0 && currentIndexOffset + matIndexCount <= allIndices.size()) {
            Mesh3D subMesh;
            subMesh.name = matName;
            subMesh.materialIndex = static_cast<int>(m);
            std::unordered_map<uint32_t, uint32_t> oldToNew;
            subMesh.indices.reserve(matIndexCount);

            for (int32_t k = 0; k < matIndexCount; ++k) {
                uint32_t oldIdx = allIndices[currentIndexOffset + k];
                if (oldIdx < allVertices.size()) {
                    auto it = oldToNew.find(oldIdx);
                    if (it == oldToNew.end()) {
                        uint32_t newIdx = static_cast<uint32_t>(subMesh.vertices.size());
                        oldToNew[oldIdx] = newIdx;
                        subMesh.vertices.push_back(allVertices[oldIdx]);
                        subMesh.skinWeights.push_back(allSkinWeights[oldIdx]);
                        subMesh.indices.push_back(newIdx);
                    } else {
                        subMesh.indices.push_back(it->second);
                    }
                }
            }
            subMesh.bindVertices = subMesh.vertices;
            loadedMeshes.push_back(std::move(subMesh));
            currentIndexOffset += matIndexCount;
        }
    }

    // ボーン読み込み
    if (ptr + 4 <= end) {
        uint32_t boneCount = *reinterpret_cast<const uint32_t*>(ptr);
        ptr += 4;
        for (uint32_t b = 0; b < boneCount; ++b) {
            if (ptr >= end) break;
            QString bName = readPmxString(ptr);
            QString bNameEn = readPmxString(ptr);
            if (bName.isEmpty()) bName = bNameEn;

            if (ptr + 12 > end) break;
            float bx = *reinterpret_cast<const float*>(ptr);
            float by = *reinterpret_cast<const float*>(ptr + 4);
            float bz = *reinterpret_cast<const float*>(ptr + 8);
            ptr += 12;

            int parentIdx = readSignedIndex(ptr, boneIndexSize);
            ptr += 4; // transformLevel

            if (ptr + 2 > end) break;
            uint16_t flags = *reinterpret_cast<const uint16_t*>(ptr);
            ptr += 2;

            if (flags & 0x0001) {
                ptr += boneIndexSize;
            } else {
                ptr += 12;
            }
            if (flags & (0x0100 | 0x0200)) {
                ptr += boneIndexSize + 4;
            }
            if (flags & 0x0400) {
                ptr += 12;
            }
            if (flags & 0x0800) {
                ptr += 24;
            }
            if (flags & 0x2000) {
                ptr += 4;
            }
            if (flags & 0x0020) {
                int targetIdx = readSignedIndex(ptr, boneIndexSize);
                int loopCount = *reinterpret_cast<const int32_t*>(ptr); ptr += 4;
                float limitRadian = *reinterpret_cast<const float*>(ptr); ptr += 4;
                ThreeDIk ik;
                ik.ikBoneIndex = static_cast<int>(b);
                ik.targetBoneIndex = targetIdx;
                ik.iterations = loopCount;
                ik.angleLimit = limitRadian;

                if (ptr + 4 <= end) {
                    int32_t ikLinks = *reinterpret_cast<const int32_t*>(ptr);
                    ptr += 4;
                    for (int32_t k = 0; k < ikLinks; ++k) {
                        if (ptr >= end) break;
                        int linkIdx = readSignedIndex(ptr, boneIndexSize);
                        uint8_t hasLimit = *reinterpret_cast<const uint8_t*>(ptr);
                        ptr += 1;
                        ThreeDIkLink link;
                        link.boneIndex = linkIdx;
                        link.hasLimit = (hasLimit != 0);
                        if (hasLimit != 0) {
                            if (ptr + 24 <= end) {
                                float minX = *reinterpret_cast<const float*>(ptr);
                                float minY = *reinterpret_cast<const float*>(ptr + 4);
                                float minZ = *reinterpret_cast<const float*>(ptr + 8);
                                float maxX = *reinterpret_cast<const float*>(ptr + 12);
                                float maxY = *reinterpret_cast<const float*>(ptr + 16);
                                float maxZ = *reinterpret_cast<const float*>(ptr + 20);
                                ptr += 24;
                                link.minLimit = QVector3D(minX, minY, minZ);
                                link.maxLimit = QVector3D(maxX, maxY, maxZ);
                            }
                        }
                        ik.links.push_back(link);
                    }
                }
                skeleton.addIk(ik);
            }

            skeleton.addBone(bName.trimmed(), parentIdx, QVector3D(bx, by, -bz));
        }
    }

    if (loadedMeshes.empty()) {
        Mesh3D m;
        m.name = "PMX_Mesh";
        m.vertices = std::move(allVertices);
        m.indices = std::move(allIndices);
        m.bindVertices = m.vertices;
        m.skinWeights = std::move(allSkinWeights);
        loadedMeshes.push_back(std::move(m));
    }

    model.meshes() = std::move(loadedMeshes);
    model.materials() = std::move(loadedMaterials);
    model.recalculateBoundingBox();
    return true;
}

} // namespace Kaburagi
