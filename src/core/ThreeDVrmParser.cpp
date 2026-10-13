#include "ThreeDVrmParser.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <cstring>

namespace Kaburagi {

bool ThreeDVrmParser::loadVrm(const QString& filePath, ThreeDModel& model, Skeleton3D& skeleton) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    if (data.size() < 20) return false;
    const char* ptr = data.constData();
    const char* end = ptr + data.size();

    uint32_t magic = *reinterpret_cast<const uint32_t*>(ptr);
    uint32_t version = *reinterpret_cast<const uint32_t*>(ptr + 4);
    uint32_t totalLength = *reinterpret_cast<const uint32_t*>(ptr + 8);
    Q_UNUSED(totalLength);

    if (magic != 0x46546C67 || version != 2) {
        return false;
    }
    ptr += 12;

    QByteArray jsonBytes;
    QByteArray binBytes;

    while (ptr + 8 <= end) {
        uint32_t chunkLength = *reinterpret_cast<const uint32_t*>(ptr);
        uint32_t chunkType = *reinterpret_cast<const uint32_t*>(ptr + 4);
        ptr += 8;

        if (ptr + chunkLength > end) break;

        if (chunkType == 0x4E4F534A) { // "JSON"
            jsonBytes = QByteArray(ptr, chunkLength);
        } else if (chunkType == 0x004E4942) { // "BIN\0"
            binBytes = QByteArray(ptr, chunkLength);
        }
        ptr += chunkLength;
    }

    if (jsonBytes.isEmpty()) return false;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    QJsonObject root = doc.object();

    // VRM メタ情報
    if (root.contains("extensions")) {
        QJsonObject exts = root["extensions"].toObject();
        if (exts.contains("VRM")) {
            QJsonObject vrmObj = exts["VRM"].toObject();
            if (vrmObj.contains("meta")) {
                QJsonObject meta = vrmObj["meta"].toObject();
                QString title = meta["title"].toString();
                if (!title.isEmpty()) {
                    model.setName(title);
                }
            }

            // ヒューマノイドボーン情報
            if (vrmObj.contains("humanoid")) {
                QJsonObject humanoid = vrmObj["humanoid"].toObject();
                QJsonArray humanBones = humanoid["humanBones"].toArray();
                for (const auto& hbVal : humanBones) {
                    QJsonObject hb = hbVal.toObject();
                    QString boneName = hb["bone"].toString();
                    int nodeIdx = hb["node"].toInt(-1);
                    skeleton.addBone(boneName, -1, QVector3D(0, 0, 0));
                    if (nodeIdx >= 0) {
                        model.humanoidBoneToNode()[boneName.toLower()] = nodeIdx;
                        model.humanoidBoneToNode()[boneName] = nodeIdx;

                        static const std::unordered_map<QString, QString> vrmToMmd = {
                            { "hips", QString::fromUtf8("下半身") },
                            { "spine", QString::fromUtf8("上半身") },
                            { "chest", QString::fromUtf8("上半身2") },
                            { "upperchest", QString::fromUtf8("胸") },
                            { "neck", QString::fromUtf8("首") },
                            { "head", QString::fromUtf8("頭") },
                            { "leftshoulder", QString::fromUtf8("左肩") },
                            { "leftupperarm", QString::fromUtf8("左腕") },
                            { "leftlowerarm", QString::fromUtf8("左ひじ") },
                            { "lefthand", QString::fromUtf8("左手首") },
                            { "rightshoulder", QString::fromUtf8("右肩") },
                            { "rightupperarm", QString::fromUtf8("右腕") },
                            { "rightlowerarm", QString::fromUtf8("右ひじ") },
                            { "righthand", QString::fromUtf8("右手首") },
                            { "leftupperleg", QString::fromUtf8("左足") },
                            { "leftlowerleg", QString::fromUtf8("左ひざ") },
                            { "leftfoot", QString::fromUtf8("左足首") },
                            { "lefttoes", QString::fromUtf8("左つま先") },
                            { "rightupperleg", QString::fromUtf8("右足") },
                            { "rightlowerleg", QString::fromUtf8("右ひざ") },
                            { "rightfoot", QString::fromUtf8("右足首") },
                            { "righttoes", QString::fromUtf8("右つま先") },
                            { "leftthumbproximal", QString::fromUtf8("左親指１") },
                            { "leftthumbintermediate", QString::fromUtf8("左親指２") },
                            { "leftindexproximal", QString::fromUtf8("左人指１") },
                            { "leftindexintermediate", QString::fromUtf8("左人指２") },
                            { "leftindexdistal", QString::fromUtf8("左人指３") },
                            { "leftmiddleproximal", QString::fromUtf8("左中指１") },
                            { "leftmiddleintermediate", QString::fromUtf8("左中指２") },
                            { "leftmiddledistal", QString::fromUtf8("左中指３") },
                            { "leftringproximal", QString::fromUtf8("左薬指１") },
                            { "leftringintermediate", QString::fromUtf8("左薬指２") },
                            { "leftringdistal", QString::fromUtf8("左薬指３") },
                            { "leftlittleproximal", QString::fromUtf8("左小指１") },
                            { "leftlittleintermediate", QString::fromUtf8("左小指２") },
                            { "leftlittledistal", QString::fromUtf8("左小指３") },
                            { "rightthumbproximal", QString::fromUtf8("右親指１") },
                            { "rightthumbintermediate", QString::fromUtf8("右親指２") },
                            { "rightindexproximal", QString::fromUtf8("右人指１") },
                            { "rightindexintermediate", QString::fromUtf8("右人指２") },
                            { "rightindexdistal", QString::fromUtf8("右人指３") },
                            { "rightmiddleproximal", QString::fromUtf8("右中指１") },
                            { "rightmiddleintermediate", QString::fromUtf8("右中指２") },
                            { "rightmiddledistal", QString::fromUtf8("右中指３") },
                            { "rightringproximal", QString::fromUtf8("右薬指１") },
                            { "rightringintermediate", QString::fromUtf8("右薬指２") },
                            { "rightringdistal", QString::fromUtf8("右薬指３") },
                            { "rightlittleproximal", QString::fromUtf8("右小指１") },
                            { "rightlittleintermediate", QString::fromUtf8("右小指２") },
                            { "rightlittledistal", QString::fromUtf8("右小指３") },
                            { "lefteye", QString::fromUtf8("左目") },
                            { "righteye", QString::fromUtf8("右目") },
                        };
                        auto it = vrmToMmd.find(boneName.toLower());
                        if (it != vrmToMmd.end()) {
                            model.humanoidBoneToNode()[it->second] = nodeIdx;
                            if (boneName.compare("hips", Qt::CaseInsensitive) == 0) {
                                model.humanoidBoneToNode()[QString::fromUtf8("センター")] = nodeIdx;
                            }
                        }
                    }
                }
            }
        }
    }

    // glTF メッシュ＆マテリアル＆ノード＆スキンのパース
    QJsonArray accessors = root["accessors"].toArray();
    QJsonArray bufferViews = root["bufferViews"].toArray();
    QJsonArray meshes = root["meshes"].toArray();
    QJsonArray images = root["images"].toArray();
    QJsonArray textures = root["textures"].toArray();
    QJsonArray materials = root["materials"].toArray();
    QJsonArray nodes = root["nodes"].toArray();
    QJsonArray skins = root["skins"].toArray();

    auto getBufferView = [&](int viewIdx, const char** outPtr, int* outLen) -> bool {
        if (viewIdx < 0 || viewIdx >= bufferViews.size()) return false;
        QJsonObject bv = bufferViews[viewIdx].toObject();
        int offset = bv["byteOffset"].toInt(0);
        int length = bv["byteLength"].toInt(0);
        if (offset < 0 || offset + length > binBytes.size()) return false;
        if (outPtr) *outPtr = binBytes.constData() + offset;
        if (outLen) *outLen = length;
        return true;
    };

    auto getBufferViewData = [&](int viewIdx) -> const char* {
        const char* ptr = nullptr;
        int len = 0;
        if (getBufferView(viewIdx, &ptr, &len)) return ptr;
        return nullptr;
    };

    // 1. テクスチャ画像 (images) パース
    std::vector<QImage> loadedImages;
    loadedImages.reserve(images.size());
    for (const auto& imgVal : images) {
        QJsonObject imgObj = imgVal.toObject();
        int bvIdx = imgObj["bufferView"].toInt(-1);
        const char* imgData = nullptr;
        int imgLen = 0;
        if (getBufferView(bvIdx, &imgData, &imgLen) && imgLen > 0) {
            QImage img = QImage::fromData(reinterpret_cast<const uchar*>(imgData), imgLen);
            loadedImages.push_back(img);
        } else {
            loadedImages.push_back(QImage());
        }
    }

    // 2. テクスチャ参照 (textures) パース
    std::vector<int> textureSourceMap;
    textureSourceMap.reserve(textures.size());
    for (const auto& texVal : textures) {
        QJsonObject texObj = texVal.toObject();
        int srcIdx = texObj["source"].toInt(-1);
        textureSourceMap.push_back(srcIdx);
    }

    // VRM 0.x materialProperties の取得 (存在する場合)
    QJsonArray vrmMatProps;
    if (root.contains("extensions")) {
        QJsonObject exts = root["extensions"].toObject();
        if (exts.contains("VRM")) {
            QJsonObject vrmObj = exts["VRM"].toObject();
            if (vrmObj.contains("materialProperties")) {
                vrmMatProps = vrmObj["materialProperties"].toArray();
            }
        }
    }

    // 3. マテリアル (materials) パース
    std::vector<Material3D> loadedMaterials;
    loadedMaterials.reserve(materials.size());
    for (int mIdx = 0; mIdx < materials.size(); ++mIdx) {
        QJsonObject matObj = materials[mIdx].toObject();
        Material3D mat;
        mat.name = matObj["name"].toString(QString("Material_%1").arg(mIdx));

        int texIdx = -1;

        mat.alphaMode = matObj["alphaMode"].toString("OPAQUE");
        mat.alphaCutoff = static_cast<float>(matObj["alphaCutoff"].toDouble(0.5));
        mat.doubleSided = matObj["doubleSided"].toBool(false);

        // glTF PBR Metallic Roughness
        if (matObj.contains("pbrMetallicRoughness")) {
            QJsonObject pbr = matObj["pbrMetallicRoughness"].toObject();
            if (pbr.contains("baseColorFactor")) {
                QJsonArray colArr = pbr["baseColorFactor"].toArray();
                if (colArr.size() >= 4) {
                    mat.diffuseColor = QColor::fromRgbF(
                        std::clamp(colArr[0].toDouble(1.0), 0.0, 1.0),
                        std::clamp(colArr[1].toDouble(1.0), 0.0, 1.0),
                        std::clamp(colArr[2].toDouble(1.0), 0.0, 1.0),
                        std::clamp(colArr[3].toDouble(1.0), 0.0, 1.0)
                    );
                }
            }
            if (pbr.contains("baseColorTexture")) {
                texIdx = pbr["baseColorTexture"].toObject()["index"].toInt(-1);
            }
        }

        // VRM 固有の MToon materialProperties による上書き・補完
        QJsonObject vrmProp;
        if (mIdx < vrmMatProps.size()) {
            vrmProp = vrmMatProps[mIdx].toObject();
        } else {
            for (const auto& vmpVal : vrmMatProps) {
                QJsonObject vp = vmpVal.toObject();
                if (vp["name"].toString() == mat.name) {
                    vrmProp = vp;
                    break;
                }
            }
        }

        int shadeTexIdx = -1;
        if (!vrmProp.isEmpty()) {
            if (vrmProp.contains("renderQueue")) {
                mat.renderQueue = vrmProp["renderQueue"].toInt(mat.renderQueue);
            }
            if (vrmProp.contains("vectorProperties")) {
                QJsonObject vecProps = vrmProp["vectorProperties"].toObject();
                if (vecProps.contains("_Color")) {
                    QJsonArray colArr = vecProps["_Color"].toArray();
                    if (colArr.size() >= 4) {
                        mat.diffuseColor = QColor::fromRgbF(
                            std::clamp(colArr[0].toDouble(1.0), 0.0, 1.0),
                            std::clamp(colArr[1].toDouble(1.0), 0.0, 1.0),
                            std::clamp(colArr[2].toDouble(1.0), 0.0, 1.0),
                            std::clamp(colArr[3].toDouble(1.0), 0.0, 1.0)
                        );
                    }
                }
                if (vecProps.contains("_ShadeColor")) {
                    QJsonArray scArr = vecProps["_ShadeColor"].toArray();
                    if (scArr.size() >= 4) {
                        mat.shadeColor = QColor::fromRgbF(
                            std::clamp(scArr[0].toDouble(1.0), 0.0, 1.0),
                            std::clamp(scArr[1].toDouble(1.0), 0.0, 1.0),
                            std::clamp(scArr[2].toDouble(1.0), 0.0, 1.0),
                            std::clamp(scArr[3].toDouble(1.0), 0.0, 1.0)
                        );
                        mat.hasShadeColor = true;
                    }
                }
            }
            if (vrmProp.contains("textureProperties")) {
                QJsonObject texProps = vrmProp["textureProperties"].toObject();
                if (texProps.contains("_MainTex")) {
                    texIdx = texProps["_MainTex"].toInt(texIdx);
                }
                if (texProps.contains("_ShadeTexture")) {
                    shadeTexIdx = texProps["_ShadeTexture"].toInt(-1);
                }
            }
            if (vrmProp.contains("floatProperties")) {
                QJsonObject fProps = vrmProp["floatProperties"].toObject();
                if (fProps.contains("_ShadeShift")) {
                    mat.shadeShift = static_cast<float>(fProps["_ShadeShift"].toDouble(0.0));
                }
                if (fProps.contains("_ShadeToony")) {
                    mat.shadeToony = static_cast<float>(fProps["_ShadeToony"].toDouble(0.9));
                }
                if (fProps.contains("_BlendMode")) {
                    mat.blendMode = static_cast<int>(fProps["_BlendMode"].toDouble(0.0));
                }
                if (fProps.contains("_CullMode")) {
                    mat.cullMode = static_cast<int>(fProps["_CullMode"].toDouble(2.0));
                }
                if (fProps.contains("_Cutoff")) {
                    mat.alphaCutoff = static_cast<float>(fProps["_Cutoff"].toDouble(0.5));
                }
            }
        }

        // テクスチャ画像の割り当て
        if (texIdx >= 0 && texIdx < static_cast<int>(textureSourceMap.size())) {
            int imgIdx = textureSourceMap[texIdx];
            if (imgIdx >= 0 && imgIdx < static_cast<int>(loadedImages.size())) {
                mat.diffuseTexture = loadedImages[imgIdx];
            }
        }
        if (shadeTexIdx >= 0 && shadeTexIdx < static_cast<int>(textureSourceMap.size())) {
            int imgIdx = textureSourceMap[shadeTexIdx];
            if (imgIdx >= 0 && imgIdx < static_cast<int>(loadedImages.size())) {
                mat.shadeTexture = loadedImages[imgIdx];
            }
        }

        loadedMaterials.push_back(std::move(mat));
    }

    // 4. ノード (nodes) パース & メッシュ→スキン対応の抽出
    std::unordered_map<int, int> meshToSkinMap;
    std::vector<ThreeDModel::ModelNode> loadedNodes(nodes.size());
    for (int nIdx = 0; nIdx < nodes.size(); ++nIdx) {
        QJsonObject nObj = nodes[nIdx].toObject();
        auto& nd = loadedNodes[nIdx];
        nd.name = nObj["name"].toString();
        if (nObj.contains("translation")) {
            QJsonArray tArr = nObj["translation"].toArray();
            if (tArr.size() >= 3) {
                nd.translation = QVector3D(
                    static_cast<float>(tArr[0].toDouble(0.0)),
                    static_cast<float>(tArr[1].toDouble(0.0)),
                    static_cast<float>(tArr[2].toDouble(0.0))
                );
            }
        }
        nd.initialTranslation = nd.translation;

        if (nObj.contains("rotation")) {
            QJsonArray rArr = nObj["rotation"].toArray();
            if (rArr.size() >= 4) {
                // glTF rotation: [x, y, z, w]. Qt QQuaternion: (scalar/w, x, y, z)
                nd.rotation = QQuaternion(
                    static_cast<float>(rArr[3].toDouble(1.0)),
                    static_cast<float>(rArr[0].toDouble(0.0)),
                    static_cast<float>(rArr[1].toDouble(0.0)),
                    static_cast<float>(rArr[2].toDouble(0.0))
                );
            }
        }
        nd.initialRotation = nd.rotation;

        if (nObj.contains("scale")) {
            QJsonArray sArr = nObj["scale"].toArray();
            if (sArr.size() >= 3) {
                nd.scale = QVector3D(
                    static_cast<float>(sArr[0].toDouble(1.0)),
                    static_cast<float>(sArr[1].toDouble(1.0)),
                    static_cast<float>(sArr[2].toDouble(1.0))
                );
            }
        }

        if (nObj.contains("children")) {
            for (const auto& cVal : nObj["children"].toArray()) {
                int cIdx = cVal.toInt(-1);
                if (cIdx >= 0 && cIdx < nodes.size()) {
                    nd.children.push_back(cIdx);
                    loadedNodes[cIdx].parent = nIdx;
                }
            }
        }

        if (nObj.contains("mesh")) {
            int mIdx = nObj["mesh"].toInt(-1);
            if (mIdx >= 0) {
                meshToSkinMap[mIdx] = nObj["skin"].toInt(-1);
            }
        }
    }

    // 5. スキン (skins) パース
    std::vector<ThreeDModel::ModelSkin> loadedSkins(skins.size());
    for (int sIdx = 0; sIdx < skins.size(); ++sIdx) {
        QJsonObject sObj = skins[sIdx].toObject();
        auto& sk = loadedSkins[sIdx];
        for (const auto& jVal : sObj["joints"].toArray()) {
            sk.joints.push_back(jVal.toInt(-1));
        }

        int invAccIdx = sObj["inverseBindMatrices"].toInt(-1);
        if (invAccIdx >= 0 && invAccIdx < accessors.size()) {
            QJsonObject acc = accessors[invAccIdx].toObject();
            int count = acc["count"].toInt(0);
            int bvIdx = acc["bufferView"].toInt(-1);
            int accOffset = acc["byteOffset"].toInt(0);
            const char* invBase = getBufferViewData(bvIdx);
            if (invBase && count > 0) {
                const float* fPtr = reinterpret_cast<const float*>(invBase + accOffset);
                sk.inverseBindMatrices.resize(count);
                for (int c = 0; c < count; ++c) {
                    sk.inverseBindMatrices[c] = QMatrix4x4(fPtr + c * 16).transposed();
                }
            }
        }
    }

    // 6. メッシュ (meshes) パース
    std::vector<Mesh3D> loadedMeshes;

    for (int meshIdx = 0; meshIdx < meshes.size(); ++meshIdx) {
        QJsonObject meshObj = meshes[meshIdx].toObject();
        int skinIdx = meshToSkinMap.count(meshIdx) ? meshToSkinMap[meshIdx] : -1;
        QJsonArray primitives = meshObj["primitives"].toArray();

        for (const auto& primVal : primitives) {
            QJsonObject prim = primVal.toObject();
            QJsonObject attrs = prim["attributes"].toObject();

            int posAccIdx = attrs["POSITION"].toInt(-1);
            if (posAccIdx < 0 || posAccIdx >= accessors.size()) continue;

            QJsonObject posAcc = accessors[posAccIdx].toObject();
            int posCount = posAcc["count"].toInt(0);
            int posBvIdx = posAcc["bufferView"].toInt(-1);
            int posOffset = posAcc["byteOffset"].toInt(0);

            const char* posBase = getBufferViewData(posBvIdx);
            if (!posBase) continue;
            const float* posPtr = reinterpret_cast<const float*>(posBase + posOffset);

            Mesh3D subMesh;
            subMesh.name = meshObj["name"].toString("VRM_Mesh");
            subMesh.materialIndex = prim["material"].toInt(-1);
            subMesh.skinIndex = skinIdx;
            subMesh.vertices.resize(posCount);

            for (int i = 0; i < posCount; ++i) {
                subMesh.vertices[i].position = QVector3D(posPtr[i * 3], posPtr[i * 3 + 1], posPtr[i * 3 + 2]);
                subMesh.vertices[i].color = QVector4D(1.0f, 1.0f, 1.0f, 1.0f);
            }

            // 法線
            if (attrs.contains("NORMAL")) {
                int normAccIdx = attrs["NORMAL"].toInt(-1);
                if (normAccIdx >= 0 && normAccIdx < accessors.size()) {
                    QJsonObject nAcc = accessors[normAccIdx].toObject();
                    int nBvIdx = nAcc["bufferView"].toInt(-1);
                    int nOffset = nAcc["byteOffset"].toInt(0);
                    const char* nBase = getBufferViewData(nBvIdx);
                    if (nBase) {
                        const float* nPtr = reinterpret_cast<const float*>(nBase + nOffset);
                        for (int i = 0; i < posCount; ++i) {
                            subMesh.vertices[i].normal = QVector3D(nPtr[i * 3], nPtr[i * 3 + 1], nPtr[i * 3 + 2]);
                        }
                    }
                }
            }

            // UV
            if (attrs.contains("TEXCOORD_0")) {
                int uvAccIdx = attrs["TEXCOORD_0"].toInt(-1);
                if (uvAccIdx >= 0 && uvAccIdx < accessors.size()) {
                    QJsonObject uvAcc = accessors[uvAccIdx].toObject();
                    int uvBvIdx = uvAcc["bufferView"].toInt(-1);
                    int uvOffset = uvAcc["byteOffset"].toInt(0);
                    const char* uvBase = getBufferViewData(uvBvIdx);
                    if (uvBase) {
                        const float* uvPtr = reinterpret_cast<const float*>(uvBase + uvOffset);
                        for (int i = 0; i < posCount; ++i) {
                            subMesh.vertices[i].texCoord = QVector2D(uvPtr[i * 2], uvPtr[i * 2 + 1]);
                        }
                    }
                }
            }

            // インデックス
            if (prim.contains("indices")) {
                int idxAccIdx = prim["indices"].toInt(-1);
                if (idxAccIdx >= 0 && idxAccIdx < accessors.size()) {
                    QJsonObject idxAcc = accessors[idxAccIdx].toObject();
                    int idxCount = idxAcc["count"].toInt(0);
                    int compType = idxAcc["componentType"].toInt(5123); // 5123: uint16, 5125: uint32
                    int idxBvIdx = idxAcc["bufferView"].toInt(-1);
                    int idxOffset = idxAcc["byteOffset"].toInt(0);

                    const char* idxBase = getBufferViewData(idxBvIdx);
                    if (idxBase) {
                        subMesh.indices.resize(idxCount);
                        const char* iPtr = idxBase + idxOffset;
                        if (compType == 5123) { // uint16
                            const uint16_t* p16 = reinterpret_cast<const uint16_t*>(iPtr);
                            for (int i = 0; i < idxCount; ++i) {
                                subMesh.indices[i] = p16[i];
                            }
                        } else if (compType == 5125) { // uint32
                            const uint32_t* p32 = reinterpret_cast<const uint32_t*>(iPtr);
                            for (int i = 0; i < idxCount; ++i) {
                                subMesh.indices[i] = p32[i];
                            }
                        }
                    }
                }
            }

            // スキニング情報 (JOINTS_0 / WEIGHTS_0)
            if (skinIdx >= 0 && attrs.contains("JOINTS_0") && attrs.contains("WEIGHTS_0")) {
                int jAccIdx = attrs["JOINTS_0"].toInt(-1);
                int wAccIdx = attrs["WEIGHTS_0"].toInt(-1);
                if (jAccIdx >= 0 && jAccIdx < accessors.size() && wAccIdx >= 0 && wAccIdx < accessors.size()) {
                    QJsonObject jAcc = accessors[jAccIdx].toObject();
                    int jCount = jAcc["count"].toInt(0);
                    int jCompType = jAcc["componentType"].toInt(5123);
                    int jBvIdx = jAcc["bufferView"].toInt(-1);
                    int jOffset = jAcc["byteOffset"].toInt(0);
                    const char* jBase = getBufferViewData(jBvIdx);

                    QJsonObject wAcc = accessors[wAccIdx].toObject();
                    int wCount = wAcc["count"].toInt(0);
                    int wCompType = wAcc["componentType"].toInt(5126);
                    int wBvIdx = wAcc["bufferView"].toInt(-1);
                    int wOffset = wAcc["byteOffset"].toInt(0);
                    const char* wBase = getBufferViewData(wBvIdx);

                    if (jBase && wBase) {
                        subMesh.skinWeights.resize(posCount);
                        const char* jPtr = jBase + jOffset;
                        const char* wPtr = wBase + wOffset;
                        for (int i = 0; i < posCount && i < jCount && i < wCount; ++i) {
                            if (jCompType == 5121) { // uint8
                                const uint8_t* p8 = reinterpret_cast<const uint8_t*>(jPtr + i * 4);
                                subMesh.skinWeights[i].joints[0] = p8[0];
                                subMesh.skinWeights[i].joints[1] = p8[1];
                                subMesh.skinWeights[i].joints[2] = p8[2];
                                subMesh.skinWeights[i].joints[3] = p8[3];
                            } else if (jCompType == 5123) { // uint16
                                const uint16_t* p16 = reinterpret_cast<const uint16_t*>(jPtr + i * 8);
                                subMesh.skinWeights[i].joints[0] = p16[0];
                                subMesh.skinWeights[i].joints[1] = p16[1];
                                subMesh.skinWeights[i].joints[2] = p16[2];
                                subMesh.skinWeights[i].joints[3] = p16[3];
                            }

                            if (wCompType == 5126) { // float
                                const float* f4 = reinterpret_cast<const float*>(wPtr + i * 16);
                                subMesh.skinWeights[i].weights[0] = f4[0];
                                subMesh.skinWeights[i].weights[1] = f4[1];
                                subMesh.skinWeights[i].weights[2] = f4[2];
                                subMesh.skinWeights[i].weights[3] = f4[3];
                            } else if (wCompType == 5121) { // uint8 normalized
                                const uint8_t* u8 = reinterpret_cast<const uint8_t*>(wPtr + i * 4);
                                subMesh.skinWeights[i].weights[0] = u8[0] / 255.0f;
                                subMesh.skinWeights[i].weights[1] = u8[1] / 255.0f;
                                subMesh.skinWeights[i].weights[2] = u8[2] / 255.0f;
                                subMesh.skinWeights[i].weights[3] = u8[3] / 255.0f;
                            } else if (wCompType == 5123) { // uint16 normalized
                                const uint16_t* u16 = reinterpret_cast<const uint16_t*>(wPtr + i * 8);
                                subMesh.skinWeights[i].weights[0] = u16[0] / 65535.0f;
                                subMesh.skinWeights[i].weights[1] = u16[1] / 65535.0f;
                                subMesh.skinWeights[i].weights[2] = u16[2] / 65535.0f;
                                subMesh.skinWeights[i].weights[3] = u16[3] / 65535.0f;
                            }
                        }
                    }
                }
            }

            subMesh.bindVertices = subMesh.vertices;
            loadedMeshes.push_back(std::move(subMesh));
        }
    }

    if (loadedMeshes.empty()) return false;

    model.meshes() = std::move(loadedMeshes);
    model.materials() = std::move(loadedMaterials);
    model.nodes() = std::move(loadedNodes);
    model.skins() = std::move(loadedSkins);
    // VRMモデルはglTF規約で-Z方向（正面奥）を向いているため、+Z（カメラ視点）を正面として向くよう180度回転
    model.setRotation(QQuaternion::fromAxisAndAngle(QVector3D(0.0f, 1.0f, 0.0f), 180.0f));
    model.updateNodeMatrices();

    // VRM 0.x secondaryAnimation (SpringBone) の読み込み
    if (root.contains("extensions")) {
        QJsonObject exts = root["extensions"].toObject();
        if (exts.contains("VRM")) {
            QJsonObject vrmObj = exts["VRM"].toObject();
            if (vrmObj.contains("secondaryAnimation")) {
                QJsonObject secObj = vrmObj["secondaryAnimation"].toObject();

                // 1. colliderGroups
                QJsonArray colGroupsArr = secObj["colliderGroups"].toArray();
                for (const auto& cgVal : colGroupsArr) {
                    QJsonObject cgObj = cgVal.toObject();
                    ThreeDModel::SpringBoneColliderGroup group;
                    group.nodeIdx = cgObj["node"].toInt(-1);
                    QJsonArray colsArr = cgObj["colliders"].toArray();
                    for (const auto& colVal : colsArr) {
                        QJsonObject colObj = colVal.toObject();
                        ThreeDModel::SpringBoneCollider col;
                        QJsonObject offsetObj = colObj["offset"].toObject();
                        col.offset = QVector3D(
                            static_cast<float>(offsetObj["x"].toDouble(0.0)),
                            static_cast<float>(offsetObj["y"].toDouble(0.0)),
                            static_cast<float>(offsetObj["z"].toDouble(0.0))
                        );
                        col.radius = static_cast<float>(colObj["radius"].toDouble(0.0));
                        group.colliders.push_back(col);
                    }
                    model.springColliderGroups().push_back(std::move(group));
                }

                // 2. boneGroups
                QJsonArray boneGroupsArr = secObj["boneGroups"].toArray();
                for (const auto& bgVal : boneGroupsArr) {
                    QJsonObject bgObj = bgVal.toObject();
                    ThreeDModel::SpringBoneGroup group;
                    group.comment = bgObj["comment"].toString();
                    group.stiffness = static_cast<float>(bgObj["stiffiness"].toDouble(0.5));
                    group.dragForce = static_cast<float>(bgObj["dragForce"].toDouble(0.4));
                    group.hitRadius = static_cast<float>(bgObj["hitRadius"].toDouble(0.02));
                    group.gravityPower = static_cast<float>(bgObj["gravityPower"].toDouble(0.0));
                    QJsonObject gDirObj = bgObj["gravityDir"].toObject();
                    group.gravityDir = QVector3D(
                        static_cast<float>(gDirObj["x"].toDouble(0.0)),
                        static_cast<float>(gDirObj["y"].toDouble(-1.0)),
                        static_cast<float>(gDirObj["z"].toDouble(0.0))
                    );

                    QJsonArray bonesArr = bgObj["bones"].toArray();
                    for (const auto& bVal : bonesArr) {
                        int bNode = bVal.toInt(-1);
                        if (bNode >= 0) {
                            group.rootNodes.push_back(bNode);
                        }
                    }

                    QJsonArray colIdxArr = bgObj["colliderGroups"].toArray();
                    for (const auto& ciVal : colIdxArr) {
                        group.colliderGroupIndices.push_back(ciVal.toInt());
                    }

                    model.springBoneGroups().push_back(std::move(group));
                }
            }
        }
    }

    model.initSpringBones();
    model.recalculateBoundingBox();
    return true;
}

} // namespace Kaburagi
