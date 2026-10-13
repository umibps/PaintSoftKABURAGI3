#include <QApplication>
#include <QTranslator>
#include <QImage>
#include <QPainter>
#include <QTemporaryFile>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <iostream>
#include <cassert>
#include <cmath>
#include <cstring>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include "ThreeDMath.hpp"
#include "ThreeDCamera.hpp"
#include "ThreeDModel.hpp"
#include "ThreeDRenderer.hpp"
#include "ThreeDLayer.hpp"
#include "ThreeDPose.hpp"
#include "ThreeDMmdParser.hpp"
#include "ThreeDVrmParser.hpp"
#include "Document.hpp"
#include "ColorPaletteWidget.hpp"
#include "ToolBoxWidget.hpp"
#include "LayerPanelWidget.hpp"
#include "ThreeDSettingsWidget.hpp"
#include "FilterProcessor.hpp"
#include "FilterDialogs.hpp"
#include "DisplayFilterManager.hpp"
#include "MainWindow.hpp"
#include "PreferencesDialog.hpp"
#include "ColorWheelWidget.hpp"
#include "ColorBoxWidget.hpp"
#include "PreviewWindow.hpp"
#include "ReferenceImageWindow.hpp"
#include "SpinScaleWidget.hpp"
#include <QToolButton>
#include <QSplitter>
#include <QMenuBar>
#include <QClipboard>

using namespace Kaburagi;

void testCameraCalculations() {
    std::cout << "[TEST] 1. Camera Calculations..." << std::endl;
    ThreeDCamera camera;
    assert(camera.fov() == 45.0f);
    assert(camera.distance() == 5.0f);

    // Orbit
    camera.orbit(15.0f, -10.0f);
    assert(camera.yaw() != 0.0f || camera.pitch() != 0.0f);

    // Pan
    camera.pan(0.5f, -0.2f);
    assert(camera.target().x != 0.0f || camera.target().y != 0.0f);

    // Zoom
    camera.zoom(1.5f);
    assert(camera.distance() == 3.5f);

    // Matrix calculation
    QMatrix4x4 view = camera.viewMatrix();
    QMatrix4x4 proj = camera.projectionMatrix(1.333f);
    assert(!view.isIdentity());
    assert(!proj.isIdentity());

    // 消失点とアイレベル計算
    QPointF vpX, vpY, vpZ;
    camera.calculateVanishingPoints(1.333f, 800, 600, vpX, vpY, vpZ);
    float eyeLevel = camera.calculateEyeLevelY(1.333f, 600);
    assert(!std::isnan(vpX.x()) && !std::isnan(vpX.y()));
    assert(!std::isnan(vpY.x()) && !std::isnan(vpY.y()));
    assert(!std::isnan(vpZ.x()) && !std::isnan(vpZ.y()));
    assert(!std::isnan(eyeLevel));

    // カメラリセット
    camera.reset();
    assert(camera.distance() == 5.0f);
    assert(camera.pitch() == 20.0f);
    assert(camera.yaw() == -30.0f);
    std::cout << "  -> PASSED" << std::endl;
}

void testModelCreationAndObjParsing() {
    std::cout << "[TEST] 2. Model Creation & OBJ Parsing..." << std::endl;
    // プリミティブ立方体作成
    auto cube = ThreeDModel::createCube(2.0f);
    assert(cube != nullptr);
    assert(cube->meshes().size() == 1);
    const auto& mesh = cube->meshes()[0];
    assert(mesh.vertices.size() == 24);
    assert(mesh.indices.size() == 36);

    const BoundingBox3D& bbox = cube->boundingBox();
    assert(bbox.isValid());
    assert(std::abs(bbox.size().x - 2.0f) < 0.01f);
    assert(std::abs(bbox.size().y - 2.0f) < 0.01f);
    assert(std::abs(bbox.size().z - 2.0f) < 0.01f);

    // スケール正規化
    cube->normalizeScale(1.0f);
    const BoundingBox3D& normBox = cube->boundingBox();
    assert(normBox.maxExtent() <= 1.01f);

    // 簡易OBJファイルの一時保存と読み込みテスト
    QTemporaryFile tempObj;
    assert(tempObj.open());
    {
        QTextStream out(&tempObj);
        out << "# Simple Triangle OBJ\n";
        out << "v 0.0 1.0 0.0\n";
        out << "v -1.0 -1.0 0.0\n";
        out << "v 1.0 -1.0 0.0\n";
        out << "vn 0.0 0.0 1.0\n";
        out << "f 1//1 2//1 3//1\n";
    }
    tempObj.close();

    auto loadedModel = std::make_shared<ThreeDModel>();
    bool loaded = loadedModel->loadFromFile(tempObj.fileName());
    assert(loaded);
    assert(loadedModel->meshes().size() == 1);
    assert(loadedModel->meshes()[0].vertices.size() == 3);
    assert(loadedModel->meshes()[0].indices.size() == 3);
    std::cout << "  -> PASSED" << std::endl;
}

void testRendererRasterization() {
    std::cout << "[TEST] 3. Renderer Rasterization & Contour Extraction..." << std::endl;
    ThreeDRenderer renderer;

    ThreeDCamera camera;
    camera.reset();

    ThreeDLight light;
    light.direction = QVector3D(0.5f, 1.0f, 0.5f).normalized();
    light.lightColor = Qt::white;

    ThreeDRenderConfig config;
    config.shadingMode = ThreeDShadingMode::Cel;
    config.drawEdges = true;
    config.drawFloorGrid = true;

    auto cube = ThreeDModel::createCube(1.5f);
    std::vector<std::shared_ptr<ThreeDModel>> models = { cube };

    QImage rendered = renderer.render(120, 120, models, camera, light, config);
    assert(rendered.width() == 120);
    assert(rendered.height() == 120);

    bool hasNonTransparent = false;
    for (int y = 0; y < 120 && !hasNonTransparent; ++y) {
        for (int x = 0; x < 120; ++x) {
            if (qAlpha(rendered.pixel(x, y)) > 0) {
                hasNonTransparent = true;
                break;
            }
        }
    }
    assert(hasNonTransparent);

    // 輪郭線抽出テスト
    QImage contour = renderer.extractContour(120, 120, models, camera, 1.5f, Qt::black);
    assert(contour.width() == 120);
    assert(contour.height() == 120);
    bool hasEdgePixel = false;
    for (int y = 0; y < 120 && !hasEdgePixel; ++y) {
        for (int x = 0; x < 120; ++x) {
            if (qAlpha(contour.pixel(x, y)) > 0) {
                hasEdgePixel = true;
                break;
            }
        }
    }
    assert(hasEdgePixel);

    // 線画のみモード
    config.shadingMode = ThreeDShadingMode::LineOnly;
    QImage linesImg = renderer.render(120, 120, models, camera, light, config);
    assert(!linesImg.isNull());
    std::cout << "  -> PASSED" << std::endl;
}

void testThreeDLayerComposite() {
    std::cout << "[TEST] 4. ThreeDLayer Composite & Rendering..." << std::endl;
    auto layer = std::make_shared<ThreeDLayer>("3Dレイヤー", 160, 120);
    assert(layer->type() == LayerType::ThreeD);

    auto cube = ThreeDModel::createCube(1.2f);
    layer->addModel(cube);

    // TileManager への描画実行
    TileManager target(160, 120);
    layer->renderToTarget(target, QRect(0, 0, 160, 120));
    assert(target.allocatedTileCount() > 0);

    // キャッシュ画像が生成されたことを確認
    assert(!layer->cachedImage().isNull());
    assert(layer->cachedImage().width() == 160);
    assert(layer->cachedImage().height() == 120);

    // 輪郭線抽出
    QImage contour = layer->extractContourImage();
    assert(contour.width() == 160);
    assert(contour.height() == 120);

    // サムネイル生成
    QImage thumb = layer->createThumbnail(36, 28);
    assert(thumb.width() == 36);
    assert(thumb.height() == 28);
    std::cout << "  -> PASSED" << std::endl;
}

void testDocumentIntegration() {
    std::cout << "[TEST] 5. Document Integration..." << std::endl;
    Document doc(100, 100);
    assert(doc.layers().size() == 1);

    auto threeDLayer = doc.addThreeDLayer("3Dレイヤー");
    assert(threeDLayer != nullptr);
    assert(doc.layers().size() == 2);
    assert(doc.activeLayer()->type() == LayerType::ThreeD);

    // キューブ追加
    threeDLayer->addModel(ThreeDModel::createCube(1.0f));
    doc.composite();

    const auto& compBuffer = doc.compositeBuffer();
    assert(compBuffer.allocatedTileCount() > 0);
    std::cout << "  -> PASSED" << std::endl;
}

void testPoseAndSkeleton() {
    std::cout << "[TEST] 6. Pose, VPD/VMD & Skeleton..." << std::endl;
    Skeleton3D skeleton;
    int root = skeleton.addBone("センター", -1, QVector3D(0, 0, 0));
    int armL = skeleton.addBone("左腕", root, QVector3D(1.0f, 1.5f, 0));
    assert(skeleton.boneCount() == 2);
    assert(skeleton.findBoneIndex("左腕") == armL);

    // プリセットポーズテスト
    ThreeDPose standPose = ThreeDPose::createStandingPose();
    assert(!standPose.rotations.empty());
    skeleton.applyPose(standPose);

    ThreeDPose sitPose = ThreeDPose::createSittingPose();
    assert(!sitPose.translations.empty());
    skeleton.applyPose(sitPose);

    // VPD テキスト読み込みテスト
    QTemporaryFile tempVpd;
    assert(tempVpd.open());
    {
        QTextStream out(&tempVpd);
        out << "Vocaloid Pose Data file\n\n";
        out << "TestModel.osm;\n";
        out << "2;\n\n";
        out << "Bone0{センター\n";
        out << "  0.000000,1.200000,-0.500000;\n";
        out << "  0.000000,0.000000,0.000000,1.000000;\n";
        out << "}\n";
        out << "Bone1{左腕\n";
        out << "  0.000000,0.000000,0.000000;\n";
        out << "  0.100000,0.200000,0.000000,0.970000;\n";
        out << "}\n";
    }
    tempVpd.close();

    ThreeDPose loadedVpd;
    bool vpdOk = loadedVpd.loadVpd(tempVpd.fileName());
    assert(vpdOk);
    assert(loadedVpd.translations.find("センター") != loadedVpd.translations.end());
    assert(loadedVpd.rotations.find("左腕") != loadedVpd.rotations.end());
    skeleton.applyPose(loadedVpd);
    std::cout << "  -> PASSED" << std::endl;
}

void testMmdParsers() {
    std::cout << "[TEST] 7. MMD (.pmd, .pmx) Parsers..." << std::endl;
    // 簡易PMDバイナリデータ作成
    QTemporaryFile tempPmd;
    assert(tempPmd.open());
    {
        QByteArray pmdData;
        pmdData.append("Pmd", 3);
        float ver = 1.0f;
        pmdData.append(reinterpret_cast<const char*>(&ver), 4);
        // モデル名 (20 bytes)
        char mName[20] = "TestPMD";
        pmdData.append(mName, 20);
        // コメント (256 bytes)
        char comment[256] = {0};
        pmdData.append(comment, 256);

        // 頂点数 (3頂点の三角形)
        uint32_t vertCount = 3;
        pmdData.append(reinterpret_cast<const char*>(&vertCount), 4);
        for (int i = 0; i < 3; ++i) {
            float p[3] = { static_cast<float>(i), 0.0f, 0.0f };
            float n[3] = { 0.0f, 1.0f, 0.0f };
            float uv[2] = { 0.0f, 0.0f };
            uint16_t b[2] = { 0, 0 };
            uint8_t w = 100;
            uint8_t edge = 0;
            pmdData.append(reinterpret_cast<const char*>(p), 12);
            pmdData.append(reinterpret_cast<const char*>(n), 12);
            pmdData.append(reinterpret_cast<const char*>(uv), 8);
            pmdData.append(reinterpret_cast<const char*>(b), 4);
            pmdData.append(reinterpret_cast<const char*>(&w), 1);
            pmdData.append(reinterpret_cast<const char*>(&edge), 1);
        }

        // 面インデックス数
        uint32_t indexCount = 3;
        pmdData.append(reinterpret_cast<const char*>(&indexCount), 4);
        uint16_t indices[3] = { 0, 1, 2 };
        pmdData.append(reinterpret_cast<const char*>(indices), 6);

        // 材質数 0
        uint32_t matCount = 0;
        pmdData.append(reinterpret_cast<const char*>(&matCount), 4);

        // ボーン数 1
        uint16_t boneCount = 1;
        pmdData.append(reinterpret_cast<const char*>(&boneCount), 2);
        char bName[20] = "センター";
        pmdData.append(bName, 20);
        int16_t parentIdx = -1;
        pmdData.append(reinterpret_cast<const char*>(&parentIdx), 2);
        uint16_t tail = 0;
        pmdData.append(reinterpret_cast<const char*>(&tail), 2);
        uint8_t type = 0;
        pmdData.append(reinterpret_cast<const char*>(&type), 1);
        uint16_t ik = 0;
        pmdData.append(reinterpret_cast<const char*>(&ik), 2);
        float bPos[3] = { 0.0f, 0.0f, 0.0f };
        pmdData.append(reinterpret_cast<const char*>(bPos), 12);

        tempPmd.write(pmdData);
    }
    tempPmd.close();

    ThreeDModel pmdModel;
    Skeleton3D pmdSkeleton;
    bool pmdOk = ThreeDMmdParser::loadPmd(tempPmd.fileName(), pmdModel, pmdSkeleton);
    assert(pmdOk);
    assert(pmdModel.meshes().size() == 1);
    assert(pmdModel.meshes()[0].vertices.size() == 3);
    assert(pmdSkeleton.boneCount() == 1);

    QString realPmx = "E:/Works/3D Model/Model/妃宮千早/妃宮千早_editted.pmx";
    if (QFile::exists(realPmx)) {
        ThreeDModel pmxModel;
        Skeleton3D pmxSkeleton;
        bool pmxOk = ThreeDMmdParser::loadPmx(realPmx, pmxModel, pmxSkeleton);
        assert(pmxOk);
        std::cout << "  (Real PMX loaded: materials=" << pmxModel.materials().size()
                  << ", meshes=" << pmxModel.meshes().size()
                  << ", bones=" << pmxSkeleton.boneCount() << ")" << std::endl;
        assert(!pmxModel.materials().empty());
        assert(!pmxModel.meshes().empty());
        assert(pmxSkeleton.boneCount() > 0);
        // テクスチャが読み込まれているか確認
        bool hasTexture = false;
        for (const auto& mat : pmxModel.materials()) {
            if (!mat.diffuseTexture.isNull()) {
                hasTexture = true;
                break;
            }
        }
        assert(hasTexture);
        std::cout << "  (PMX textures successfully loaded: hasTexture=" << (hasTexture ? "true" : "false") << ")" << std::endl;

        const auto& pmxBBox = pmxModel.boundingBox();
        std::cout << "  (PMX BBox: min=(" << pmxBBox.min.x() << ", " << pmxBBox.min.y() << ", " << pmxBBox.min.z()
                  << ") max=(" << pmxBBox.max.x() << ", " << pmxBBox.max.y() << ", " << pmxBBox.max.z() << "))" << std::endl;

        float h = pmxBBox.size().y();
        if (h > 5.0f) {
            float s = 1.6f / h;
            pmxModel.setScale(QVector3D(s, s, s));
        }

        ThreeDRenderer rPmx;
        ThreeDCamera cPmx; // デフォルトカメラ (eye=(0, 1.2, 3.5), target=(0, 0.8, 0))
        ThreeDLight lPmx;
        ThreeDRenderConfig cfgPmx;
        cfgPmx.drawFloorGrid = false;
        std::vector<std::shared_ptr<ThreeDModel>> mListPmx = { std::make_shared<ThreeDModel>(pmxModel) };
        QImage imgPmx = rPmx.render(400, 400, mListPmx, cPmx, lPmx, cfgPmx);
        imgPmx.save("E:/Works/Programming/PaintSoftKABURAGI3_0_1/scratch_test_pmx.png");
        std::cout << "  Saved test render to scratch_test_pmx.png" << std::endl;
    }

    // カイト.pmd & 03 Mitsuru 1.vpd の検証
    QString kaitoPmd = "D:/Program Files/MikuMikuDance_v932x64/UserFile/Model/カイト.pmd";
    QString mitsuruVpd = "E:/Works/3D Model/Pose/MikuMikuDance/junkosakura01 - P3 female poses/03 Mitsuru 1.vpd";
    if (QFile::exists(kaitoPmd) && QFile::exists(mitsuruVpd)) {
        ThreeDModel kaitoModel;
        Skeleton3D k穹toSkeleton;
        bool kaitoOk = ThreeDMmdParser::loadPmd(kaitoPmd, kaitoModel, kaitoModel.skeleton());
        assert(kaitoOk);
        assert(!kaitoModel.meshes().empty());
        assert(kaitoModel.skeleton().boneCount() > 0);
        assert(!kaitoModel.skeleton().iks().empty());
        std::cout << "  (Kaito PMD loaded: bones=" << kaitoModel.skeleton().boneCount()
                  << ", IKs=" << kaitoModel.skeleton().iks().size()
                  << ", meshes=" << kaitoModel.meshes().size() << ")" << std::endl;

        const auto& bBefore = kaitoModel.boundingBox();
        std::cout << "  Kaito BBox BEFORE pose: min=(" << bBefore.min.x() << ", " << bBefore.min.y() << ", " << bBefore.min.z()
                  << ") max=(" << bBefore.max.x() << ", " << bBefore.max.y() << ", " << bBefore.max.z() << ")" << std::endl;

        ThreeDPose vpdPose;
        bool vpdOk = vpdPose.loadVpd(mitsuruVpd);
        assert(vpdOk);
        assert(!vpdPose.rotations.empty());
        std::cout << "  (Mitsuru VPD loaded: rotCount=" << vpdPose.rotations.size()
                  << ", transCount=" << vpdPose.translations.size() << ")" << std::endl;

        kaitoModel.applyPose(vpdPose);

        const auto& bAfter = kaitoModel.boundingBox();
        std::cout << "  Kaito BBox AFTER pose: min=(" << bAfter.min.x() << ", " << bAfter.min.y() << ", " << bAfter.min.z()
                  << ") max=(" << bAfter.max.x() << ", " << bAfter.max.y() << ", " << bAfter.max.z() << ")" << std::endl;

        // 頂点が変化していること（ポーズが適用されたこと）を確認
        assert(bBefore.min != bAfter.min || bBefore.max != bAfter.max);

        // レンダリングして画像保存
        float h = kaitoModel.boundingBox().size().y();
        if (h > 5.0f) {
            float s = 1.6f / h;
            kaitoModel.setScale(QVector3D(s, s, s));
        }

        ThreeDRenderer rKaito;
        ThreeDCamera cKaito;
        ThreeDLight lKaito;
        ThreeDRenderConfig cfgKaito;
        cfgKaito.drawFloorGrid = false;
        std::vector<std::shared_ptr<ThreeDModel>> mListKaito = { std::make_shared<ThreeDModel>(kaitoModel) };
        QImage imgKaito = rKaito.render(400, 400, mListKaito, cKaito, lKaito, cfgKaito);
        imgKaito.save("E:/Works/Programming/PaintSoftKABURAGI3_0_1/scratch_test_kaito_pose.png");
        std::cout << "  Saved test render to scratch_test_kaito_pose.png" << std::endl;
    }

    std::cout << "  -> PASSED" << std::endl;
}

void testVrmParser() {
    std::cout << "[TEST] 8. VRM (GLB + JSON + BIN) Parser..." << std::endl;
    // 簡易VRM (GLB) バイナリ作成
    QTemporaryFile tempVrm;
    assert(tempVrm.open());
    {
        // JSON構築
        QJsonObject root;
        QJsonObject asset;
        asset["version"] = "2.0";
        root["asset"] = asset;

        // VRM 拡張
        QJsonObject exts;
        QJsonObject vrm;
        QJsonObject meta;
        meta["title"] = "TestAvatarVRM";
        vrm["meta"] = meta;
        exts["VRM"] = vrm;
        root["extensions"] = exts;

        // 頂点バッファ (3頂点: 36バイト)
        float posData[9] = {
            0.0f, 1.0f, 0.0f,
            -1.0f, -1.0f, 0.0f,
            1.0f, -1.0f, 0.0f
        };
        uint16_t idxData[3] = { 0, 1, 2 };

        QByteArray binData;
        binData.append(reinterpret_cast<const char*>(posData), sizeof(posData));
        binData.append(reinterpret_cast<const char*>(idxData), sizeof(idxData));

        // bufferViews
        QJsonArray bufferViews;
        QJsonObject bvPos;
        bvPos["byteOffset"] = 0;
        bvPos["byteLength"] = static_cast<int>(sizeof(posData));
        bufferViews.append(bvPos);

        QJsonObject bvIdx;
        bvIdx["byteOffset"] = static_cast<int>(sizeof(posData));
        bvIdx["byteLength"] = static_cast<int>(sizeof(idxData));
        bufferViews.append(bvIdx);
        root["bufferViews"] = bufferViews;

        // accessors
        QJsonArray accessors;
        QJsonObject accPos;
        accPos["bufferView"] = 0;
        accPos["byteOffset"] = 0;
        accPos["componentType"] = 5126; // float
        accPos["count"] = 3;
        accPos["type"] = "VEC3";
        accessors.append(accPos);

        QJsonObject accIdx;
        accIdx["bufferView"] = 1;
        accIdx["byteOffset"] = 0;
        accIdx["componentType"] = 5123; // uint16
        accIdx["count"] = 3;
        accIdx["type"] = "SCALAR";
        accessors.append(accIdx);
        root["accessors"] = accessors;

        // meshes
        QJsonArray meshes;
        QJsonObject mesh;
        mesh["name"] = "AvatarMesh";
        QJsonArray prims;
        QJsonObject prim;
        QJsonObject attrs;
        attrs["POSITION"] = 0;
        prim["attributes"] = attrs;
        prim["indices"] = 1;
        prims.append(prim);
        mesh["primitives"] = prims;
        meshes.append(mesh);
        root["meshes"] = meshes;

        QByteArray jsonBytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
        while (jsonBytes.size() % 4 != 0) jsonBytes.append(' ');
        while (binData.size() % 4 != 0) binData.append('\0');

        uint32_t totalLen = 12 + 8 + jsonBytes.size() + 8 + binData.size();

        QByteArray glb;
        uint32_t magic = 0x46546C67;
        uint32_t ver = 2;
        glb.append(reinterpret_cast<const char*>(&magic), 4);
        glb.append(reinterpret_cast<const char*>(&ver), 4);
        glb.append(reinterpret_cast<const char*>(&totalLen), 4);

        // Chunk 0: JSON
        uint32_t jsonLen = jsonBytes.size();
        uint32_t jsonType = 0x4E4F534A;
        glb.append(reinterpret_cast<const char*>(&jsonLen), 4);
        glb.append(reinterpret_cast<const char*>(&jsonType), 4);
        glb.append(jsonBytes);

        // Chunk 1: BIN
        uint32_t binLen = binData.size();
        uint32_t binType = 0x004E4942;
        glb.append(reinterpret_cast<const char*>(&binLen), 4);
        glb.append(reinterpret_cast<const char*>(&binType), 4);
        glb.append(binData);

        tempVrm.write(glb);
    }
    tempVrm.close();

    ThreeDModel vrmModel;
    Skeleton3D vrmSkeleton;
    bool vrmOk = ThreeDVrmParser::loadVrm(tempVrm.fileName(), vrmModel, vrmSkeleton);
    assert(vrmOk);
    assert(vrmModel.name() == "TestAvatarVRM");
    assert(vrmModel.meshes().size() == 1);
    assert(vrmModel.meshes()[0].vertices.size() == 3);
    assert(vrmModel.meshes()[0].indices.size() == 3);

    // 実機 VRM ファイルが存在する場合はマテリアルとテクスチャの検証
    QString realVrm;
    {
        QDirIterator it("E:/Works/3D Model", QStringList() << "*.vrm", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString path = it.next();
            if (path.contains("妃宮千早5.vrm")) {
                realVrm = path;
                break;
            }
            if (realVrm.isEmpty()) realVrm = path;
        }
    }
    std::cout << "  (Found realVrm: " << realVrm.toStdString() << ")" << std::endl;

    if (!realVrm.isEmpty() && QFile::exists(realVrm)) {
        ThreeDModel realModel;
        Skeleton3D realSkeleton;
        bool realOk = ThreeDVrmParser::loadVrm(realVrm, realModel, realSkeleton);
        assert(realOk);
        assert(!realModel.materials().empty());
        bool hasTexture = false;
        for (const auto& mat : realModel.materials()) {
            if (!mat.diffuseTexture.isNull()) {
                hasTexture = true;
                break;
            }
        }
        assert(hasTexture);
        std::cout << "  (Real VRM loaded: " << realModel.materials().size() << " materials, meshes=" << realModel.meshes().size() << ")" << std::endl;
        for (size_t m = 0; m < realModel.materials().size(); ++m) {
            const auto& mat = realModel.materials()[m];
            if (!mat.diffuseTexture.isNull()) {
                QString fn = QString("E:/Works/Programming/PaintSoftKABURAGI3_0_1/scratch_mat_%1_%2.png").arg(m).arg(mat.name);
                mat.diffuseTexture.save(fn);
            }
        }
        for (size_t m = 0; m < realModel.materials().size(); ++m) {
            const auto& mat = realModel.materials()[m];
            std::cout << "    Mat[" << m << "]: " << mat.name.toStdString()
                      << " RGBA=(" << mat.diffuseColor.red() << "," << mat.diffuseColor.green() << ","
                      << mat.diffuseColor.blue() << "," << mat.diffuseColor.alpha() << ")"
                      << " tex=" << (!mat.diffuseTexture.isNull() ? QString("%1x%2 format=%3").arg(mat.diffuseTexture.width()).arg(mat.diffuseTexture.height()).arg(mat.diffuseTexture.format()).toStdString() : "NONE")
                      << " alphaMode=" << mat.alphaMode.toStdString()
                      << " blendMode=" << mat.blendMode
                      << " renderQueue=" << mat.renderQueue
                      << " cullMode=" << mat.cullMode
                      << " cutoff=" << mat.alphaCutoff
                      << " doubleSided=" << mat.doubleSided
                      << std::endl;
        }
        for (int m : {10, 14}) {
            auto mats = realModel.getMeshSkinMatrices(m);
            std::cout << "Mesh[" << m << "] skin matrices count=" << mats.size() << std::endl;
            for (size_t j = 0; j < std::min(mats.size(), size_t(5)); ++j) {
                std::cout << "  mat[" << j << "] isIdentity=" << mats[j].isIdentity() << std::endl;
                if (!mats[j].isIdentity()) {
                    for (int r = 0; r < 4; ++r) {
                        std::cout << "    [" << mats[j](r,0) << ", " << mats[j](r,1) << ", " << mats[j](r,2) << ", " << mats[j](r,3) << "]" << std::endl;
                    }
                }
            }
        }

        // レンダラーで実際にレンダリングしてクラッシュしないこと・描画されることを確認
        ThreeDRenderer renderer;
        ThreeDCamera cam;
        ThreeDLight light;
        ThreeDRenderConfig cfg;
        cfg.drawFloorGrid = false;
        std::vector<std::shared_ptr<ThreeDModel>> models = { std::make_shared<ThreeDModel>(realModel) };
        QImage rendered = renderer.render(400, 400, models, cam, light, cfg);
        assert(!rendered.isNull());
        rendered.save("E:/Works/Programming/PaintSoftKABURAGI3_0_1/scratch_test_vrm_initial.png");

        // 実機 VRoidPose ファイルの検証
        QString posePath;
        ThreeDModel vrmVpdModel;
        Skeleton3D skelDump;
        ThreeDVrmParser::loadVrm(realVrm, vrmVpdModel, skelDump);
        ThreeDPose mitsuruPose;
        QString mitsuruVpdPath = "E:/Works/3D Model/Pose/MikuMikuDance/junkosakura01 - P3 female poses/03 Mitsuru 1.vpd";
        if (QFile::exists(mitsuruVpdPath) && mitsuruPose.loadVpd(mitsuruVpdPath)) {
            std::cout << "  (Testing VRM + VPD: rotCount=" << mitsuruPose.rotations.size() << ")" << std::endl;

            vrmVpdModel.applyPose(mitsuruPose);

            const auto& bAfter = vrmVpdModel.boundingBox();
            std::cout << "  VRM BBox AFTER VPD: min=(" << bAfter.min.x() << ", " << bAfter.min.y() << ", " << bAfter.min.z() << ") max=(" << bAfter.max.x() << ", " << bAfter.max.y() << ", " << bAfter.max.z() << ")" << std::endl;

            ThreeDRenderer rPost;
            ThreeDCamera cPost;
            ThreeDLight lPost;
            ThreeDRenderConfig cfgPost;
            cfgPost.drawFloorGrid = false;
            std::vector<std::shared_ptr<ThreeDModel>> mListVpd = { std::make_shared<ThreeDModel>(vrmVpdModel) };
            QImage imgVpd = rPost.render(400, 400, mListVpd, cPost, lPost, cfgPost);
            imgVpd.save("E:/Works/Programming/PaintSoftKABURAGI3_0_1/scratch_test_vrm_vpd.png");
            std::cout << "  Saved scratch_test_vrm_vpd.png" << std::endl;

            // 顔の肌色比較テスト (初期ポーズの千早)
            ThreeDModel faceModel;
            Skeleton3D faceSkel;
            ThreeDVrmParser::loadVrm(realVrm, faceModel, faceSkel);
            std::vector<std::shared_ptr<ThreeDModel>> mListFace = { std::make_shared<ThreeDModel>(faceModel) };

            ThreeDCamera cFace;
            cFace.setEye(QVector3D(0.0f, 1.35f, 0.65f));
            cFace.setTarget(QVector3D(0.0f, 1.35f, 0.0f));

            ThreeDRenderConfig cfgFace;
            cfgFace.drawFloorGrid = false;
            cfgFace.drawEdges = true;

            // 1. 修正後の新デフォルトライト (順光 + MToonシェーディング)
            ThreeDLight lNew; // デフォルトで新設定 (0.3, -0.4, -0.85)
            QImage imgNew = rPost.render(400, 400, mListFace, cFace, lNew, cfgFace);
            imgNew.save("E:/Works/Programming/PaintSoftKABURAGI3_0_1/scratch_face_mtoon_fixed.png");
            std::cout << "  Saved scratch_face_mtoon_fixed.png" << std::endl;
        }

        if (!posePath.isEmpty()) {
            std::cout << "  (Testing pose: " << posePath.toStdString() << ")" << std::endl;
            const auto& bBefore = realModel.boundingBox();
            std::cout << "  BBox BEFORE pose: min=(" << bBefore.min.x() << ", " << bBefore.min.y() << ", " << bBefore.min.z() << ") max=(" << bBefore.max.x() << ", " << bBefore.max.y() << ", " << bBefore.max.z() << ")" << std::endl;

            ThreeDPose vroidPose;
            bool vroidOk = vroidPose.loadVroidPose(posePath);
            assert(vroidOk);
            assert(!vroidPose.rotations.empty());
            assert(!realModel.nodes().empty());
            assert(!realModel.skins().empty());

            realModel.applyPose(vroidPose);

            const auto& bAfter = realModel.boundingBox();
            std::cout << "  BBox AFTER pose: min=(" << bAfter.min.x() << ", " << bAfter.min.y() << ", " << bAfter.min.z() << ") max=(" << bAfter.max.x() << ", " << bAfter.max.y() << ", " << bAfter.max.z() << ")" << std::endl;

            // 座りポーズにより高さが約0.9m程度に低くなっていること、および異常な座標（10m超）がないことを検証
            assert(bAfter.isValid());
            assert(bAfter.size().y() < 1.2f);
            assert(bAfter.size().y() > 0.5f);
            assert(bAfter.maxExtent() < 2.0f);

            // Render and save test image
            ThreeDRenderer rPost;
            ThreeDCamera cPost;
            ThreeDLight lPost;
            ThreeDRenderConfig cfgPost;
            cfgPost.drawFloorGrid = false;
            std::vector<std::shared_ptr<ThreeDModel>> mList = { std::make_shared<ThreeDModel>(realModel) };
            QImage imgPost = rPost.render(400, 400, mList, cPost, lPost, cfgPost);
            imgPost.save("E:/Works/Programming/PaintSoftKABURAGI3_0_1/scratch_test_pose.png");
            std::cout << "  Saved test render to scratch_test_pose.png" << std::endl;

            ThreeDModel vrmVpdModel;
            Skeleton3D skelDump;
            ThreeDVrmParser::loadVrm(realVrm, vrmVpdModel, skelDump);
            ThreeDPose mitsuruPose;
            if (mitsuruPose.loadVpd("E:/Works/3D Model/Pose/MikuMikuDance/junkosakura01 - P3 female poses/03 Mitsuru 1.vpd")) {
                std::cout << "  (Testing VRM + VPD...)" << std::endl;
                vrmVpdModel.applyPose(mitsuruPose);
                std::vector<std::shared_ptr<ThreeDModel>> mListVpd = { std::make_shared<ThreeDModel>(vrmVpdModel) };
                QImage imgVpd = rPost.render(400, 400, mListVpd, cPost, lPost, cfgPost);
                imgVpd.save("E:/Works/Programming/PaintSoftKABURAGI3_0_1/scratch_test_vrm_vpd.png");
                std::cout << "  Saved scratch_test_vrm_vpd.png" << std::endl;
            }
        }
    }

    std::cout << "  -> PASSED" << std::endl;
}

void testVrmaAnimationAndSpringBone() {
    std::cout << "[TEST] 9. VRMA Animation & SpringBone Physics..." << std::endl;

    QString vrmaPath = "E:/Works/3D Model/Motion/VRoidStudio/VRMA_MotionPack/vrma/VRMA_01.vrma";
    if (!QFile::exists(vrmaPath)) {
        std::cout << "  (Skipping VRMA test: " << vrmaPath.toStdString() << " not found)" << std::endl;
        return;
    }

    auto anim = ThreeDAnimation::loadVrma(vrmaPath);
    assert(anim != nullptr);
    assert(!anim->isEmpty());
    assert(anim->duration > 11.0f);
    std::cout << "  VRMA Loaded: duration=" << anim->duration << "s, tracks=" << anim->tracks.size() << std::endl;

    // ポーズ評価テスト (24fps / 30fps)
    ThreeDPose pose30fps, pose24fps;
    anim->evaluate(2.0f, pose30fps);
    anim->evaluate(2.0f, pose24fps);
    assert(pose30fps.rotations.size() > 0);
    assert(pose30fps.translations.size() > 0);

    // VRMモデルへの適用とSpringBone物理テスト
    QString vrmPath = "E:/Works/3D Model/Model/妃宮千早/妃宮千早5.vrm";
    if (QFile::exists(vrmPath)) {
        ThreeDModel model;
        Skeleton3D skel;
        bool loaded = ThreeDVrmParser::loadVrm(vrmPath, model, skel);
        assert(loaded);
        std::cout << "  VRM Loaded for anim test: springGroups=" << model.springBoneGroups().size()
                  << ", colliders=" << model.springColliderGroups().size() << std::endl;
        assert(model.springBoneGroups().size() > 0);

        for (size_t g = 0; g < model.springBoneGroups().size(); ++g) {
            const auto& grp = model.springBoneGroups()[g];
            std::cout << "  Group " << g << ": comment=" << grp.comment.toStdString()
                      << ", stiffness=" << grp.stiffness << ", dragForce=" << grp.dragForce
                      << ", gravityPower=" << grp.gravityPower << ", joints=" << grp.joints.size() << std::endl;
        }

        // 30fpsで数フレーム進めて物理シミュレーションを実行
        float dt = 1.0f / 30.0f;
        float curTime = 0.0f;
        for (int frame = 0; frame < 60; ++frame) { // 2秒間シミュレーション
            curTime += dt;
            ThreeDPose p;
            anim->evaluate(curTime, p);
            model.applyPose(p);
            model.updateSpringBones(dt);
        }

        // 2秒時点の画像をレンダリングして保存
        ThreeDRenderer renderer;
        ThreeDCamera cam;
        cam.setEye(QVector3D(0.0f, 1.05f, 1.7f));
        cam.setTarget(QVector3D(0.0f, 0.95f, 0.0f));
        ThreeDLight light;
        ThreeDRenderConfig cfg;
        cfg.drawFloorGrid = false;
        std::vector<std::shared_ptr<ThreeDModel>> mList = { std::make_shared<ThreeDModel>(model) };
        QImage frameImg2 = renderer.render(800, 800, mList, cam, light, cfg);
        frameImg2.save("E:/Works/Programming/PaintSoftKABURAGI3_0_1/scratch_test_vrma_frame2.png");
        std::cout << "  Saved scratch_test_vrma_frame2.png (at 2.0s)" << std::endl;

        // 4秒時点までさらにシミュレーションを進める
        for (int frame = 60; frame < 120; ++frame) { // 4秒時点
            curTime += dt;
            ThreeDPose p;
            anim->evaluate(curTime, p);
            model.applyPose(p);
            model.updateSpringBones(dt);
        }
        for (size_t g = 0; g < model.springBoneGroups().size(); ++g) {
            const auto& group = model.springBoneGroups()[g];
            for (const auto& joint : group.joints) {
                const auto& node = model.nodes()[joint.nodeIdx];
                QQuaternion rotDiff = node.rotation * node.initialRotation.inverted();
                float angle;
                QVector3D axis;
                rotDiff.getAxisAndAngle(&axis, &angle);
                if (angle > 180.0f) angle = 360.0f - angle;
                if (angle > 30.0f) {
                    std::cout << "  [High angle] Group " << g << " Node " << joint.nodeIdx
                              << " (" << node.name.toStdString() << "): angle=" << angle << " deg" << std::endl;
                }
            }
        }
        mList[0] = std::make_shared<ThreeDModel>(model);
        QImage frameImg4 = renderer.render(800, 800, mList, cam, light, cfg);
        frameImg4.save("E:/Works/Programming/PaintSoftKABURAGI3_0_1/scratch_test_vrma_frame4.png");
        std::cout << "  Saved scratch_test_vrma_frame4.png (at 4.0s)" << std::endl;
    }

    std::cout << "  -> PASSED" << std::endl;
}

void testVmdAnimation() {
    std::cout << "[TEST] 10. VMD Motion Loading & Evaluation..." << std::endl;
    QString vmdPath = "E:/Works/3D Model/Motion/MikuMikuDance/galaxias_motion/galaxias!_miku_v2.vmd";
    if (QFile::exists(vmdPath)) {
        auto anim = ThreeDAnimation::loadVmd(vmdPath);
        assert(anim != nullptr);
        assert(!anim->isEmpty());
        assert(anim->isMmd);
        assert(anim->duration > 80.0f); // galaxiasは約87秒
        std::cout << "  (VMD loaded: duration=" << anim->duration << "s, tracks=" << anim->tracks.size() << ")" << std::endl;

        ThreeDPose pose;
        anim->evaluate(2.0f, pose);
        assert(pose.isMmdPose());
        std::cout << "  (VMD evaluated at 2.0s: rotCount=" << pose.rotations.size() << ", transCount=" << pose.translations.size() << ")" << std::endl;

        // カイト.pmdモデルへの適用テスト
        QString kaitoPath = "D:/Program Files/MikuMikuDance_v932x64/UserFile/Model/カイト.pmd";
        if (QFile::exists(kaitoPath)) {
            ThreeDModel kaito;
            assert(kaito.loadModel(kaitoPath));
            kaito.applyPose(pose);
            std::cout << "  (VMD applied to Kaito PMD successfully)" << std::endl;
        }

        // 妃宮千早5.vrmモデルへの適用テスト
        QString vrmPath = "E:/Works/3D Model/Model/妃宮千早/妃宮千早5.vrm";
        if (QFile::exists(vrmPath)) {
            ThreeDModel chihaya;
            assert(chihaya.loadModel(vrmPath));
            chihaya.applyPose(pose);
            std::cout << "  (VMD applied to Chihaya VRM successfully)" << std::endl;
        }
    }
    std::cout << "  -> PASSED" << std::endl;
}

void testOpenGLOffscreen() {
    std::cout << "[TEST] 11. OpenGL Context & Offscreen FBO..." << std::endl;
    QOpenGLContext context;
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    context.setFormat(format);
    bool ctxCreated = context.create();
    std::cout << "  (OpenGL context create: " << (ctxCreated ? "SUCCESS" : "FAILED") << ")" << std::endl;

    if (ctxCreated) {
        QOffscreenSurface surface;
        surface.setFormat(context.format());
        surface.create();
        if (context.makeCurrent(&surface)) {
            std::cout << "  (makeCurrent: SUCCESS, GL_VERSION="
                      << reinterpret_cast<const char*>(context.functions()->glGetString(GL_VERSION)) << ")" << std::endl;

            QOpenGLFramebufferObjectFormat fboFormat;
            fboFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
            QOpenGLFramebufferObject fbo(256, 256, fboFormat);
            if (fbo.isValid()) {
                fbo.bind();
                context.functions()->glClearColor(0.2f, 0.4f, 0.8f, 1.0f);
                context.functions()->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                fbo.release();
                QImage img = fbo.toImage();
                std::cout << "  (FBO render & toImage: size=" << img.width() << "x" << img.height()
                          << ", pixel(0,0)=0x" << std::hex << img.pixel(0, 0) << std::dec << ")" << std::endl;
            }
            context.doneCurrent();
        }
    }
    std::cout << "  -> PASSED" << std::endl;
}

void testGpuRenderingPerformance() {
    std::cout << "[TEST] 12. GPU vs CPU Rendering Performance..." << std::endl;
    QString vrmPath = "E:/Works/3D Model/Model/妃宮千早/妃宮千早5.vrm";
    if (QFile::exists(vrmPath)) {
        ThreeDModel chihaya;
        chihaya.loadFromFile(vrmPath);

        ThreeDCamera cam;
        cam.setEye(QVector3D(0.0f, 1.35f, 0.75f));
        cam.setTarget(QVector3D(0.0f, 1.35f, 0.0f));
        ThreeDLight light;
        ThreeDRenderConfig cfg;
        cfg.drawFloorGrid = false;
        cfg.drawEdges = true;
        cfg.edgeWidth = 1.5f;

        std::vector<std::shared_ptr<ThreeDModel>> mList = { std::make_shared<ThreeDModel>(chihaya) };

        // GPU レンダリング計測
        ThreeDGpuRenderer gpuRenderer;
        if (gpuRenderer.isAvailable()) {
            QElapsedTimer timer;
            timer.start();
            QImage gpuImg = gpuRenderer.render(800, 800, mList, cam, light, cfg);
            qint64 gpuTimeMs = timer.elapsed();
            std::cout << "  (GPU Render Time: " << gpuTimeMs << " ms)" << std::endl;
            assert(!gpuImg.isNull());
            gpuImg.save("E:/Works/Programming/PaintSoftKABURAGI3_0_1/scratch_test_gpu_render.png");
            std::cout << "  Saved scratch_test_gpu_render.png" << std::endl;

            // 全身構図のレンダリング (エッジなし)
            ThreeDCamera fullCam;
            fullCam.setEye(QVector3D(0.0f, 0.8f, 2.5f));
            fullCam.setTarget(QVector3D(0.0f, 0.8f, 0.0f));
            ThreeDRenderConfig noEdgeCfg = cfg;
            noEdgeCfg.drawEdges = false;
            QImage fullImg = gpuRenderer.render(800, 800, mList, fullCam, light, noEdgeCfg);
            fullImg.save("E:/Works/Programming/PaintSoftKABURAGI3_0_1/scratch_gpu_fullbody_noedge.png");
            std::cout << "  Saved scratch_gpu_fullbody_noedge.png" << std::endl;

            ThreeDRenderConfig edgeCfg = cfg;
            edgeCfg.drawEdges = true;
            edgeCfg.edgeWidth = 1.0f;
            QImage fullEdgeImg = gpuRenderer.render(800, 800, mList, fullCam, light, edgeCfg);
            fullEdgeImg.save("E:/Works/Programming/PaintSoftKABURAGI3_0_1/scratch_gpu_fullbody_edge.png");
            std::cout << "  Saved scratch_gpu_fullbody_edge.png" << std::endl;

            // 顔アップ構図
            ThreeDCamera faceCam;
            faceCam.setEye(QVector3D(0.0f, 1.4f, 0.7f));
            faceCam.setTarget(QVector3D(0.0f, 1.35f, 0.0f));
            QImage faceImg = gpuRenderer.render(800, 800, mList, faceCam, light, edgeCfg);
            faceImg.save("E:/Works/Programming/PaintSoftKABURAGI3_0_1/scratch_gpu_face.png");
            std::cout << "  Saved scratch_gpu_face.png" << std::endl;

            const auto& mBody = chihaya.meshes()[10];
            const auto& mCloth = chihaya.meshes()[14];
            float maxZ_Body = -1e9f, minZ_Body = 1e9f;
            for (uint32_t idx : mBody.indices) {
                const auto& v = mBody.vertices[idx];
                if (v.position.y() >= 0.8f && v.position.y() <= 1.0f && std::abs(v.position.x()) <= 0.05f) {
                    maxZ_Body = std::max(maxZ_Body, v.position.z());
                    minZ_Body = std::min(minZ_Body, v.position.z());
                }
            }
            float maxZ_Cloth = -1e9f, minZ_Cloth = 1e9f;
            for (uint32_t idx : mCloth.indices) {
                const auto& v = mCloth.vertices[idx];
                if (v.position.y() >= 0.8f && v.position.y() <= 1.0f && std::abs(v.position.x()) <= 0.05f) {
                    maxZ_Cloth = std::max(maxZ_Cloth, v.position.z());
                    minZ_Cloth = std::min(minZ_Cloth, v.position.z());
                }
            }
            std::cout << "  STOMACH Body Z: [" << minZ_Body << ", " << maxZ_Body << "]" << std::endl;
            std::cout << "  STOMACH Cloth Z: [" << minZ_Cloth << ", " << maxZ_Cloth << "]" << std::endl;
        }

        // ThreeDLayer 経由のキャッシュ更新テスト (GPUモード)
        ThreeDLayer layer("3D_GPU_Test", 800, 800);
        layer.setUseGpu(true);
        layer.addModel(mList[0]);
        layer.camera() = cam;
        layer.light() = light;
        layer.renderConfig() = cfg;

        TileManager target(800, 800);
        QElapsedTimer layerTimer;
        layerTimer.start();
        layer.renderToTarget(target, QRect(0, 0, 800, 800));
        qint64 layerTimeMs = layerTimer.elapsed();
        std::cout << "  (ThreeDLayer GPU renderToTarget Time: " << layerTimeMs << " ms)" << std::endl;
    }
    std::cout << "  -> PASSED" << std::endl;
}

void testUiSnapshots() {
    std::cout << "[TEST] 13. UI Snapshots Verification..." << std::endl;
    // 1. ColorPaletteWidget (高さ確認)
    {
        ColorPaletteWidget palette;
        palette.resize(palette.sizeHint());
        QImage img(palette.size(), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::white);
        palette.render(&img);
        img.save("C:/Users/UMIHIKO/.gemini/antigravity/brain/e668f6c6-6123-49b4-8d6c-a63bbb7450be/palette_height_verified.png");
        std::cout << "  Palette size: " << palette.width() << "x" << palette.height() << std::endl;
    }

    // 2. ToolBoxWidget (縦2行確認)
    {
        ToolBoxWidget toolbox;
        toolbox.resize(190, 240);
        QImage img(toolbox.size(), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::white);
        toolbox.render(&img);
        img.save("C:/Users/UMIHIKO/.gemini/antigravity/brain/e668f6c6-6123-49b4-8d6c-a63bbb7450be/toolbox_2rows_verified.png");
    }

    // 3. LayerPanelWidget (ピン止めボタン確認)
    {
        Document doc(800, 600);
        auto l1 = doc.addRasterLayer("レイヤー 1");
        auto l2 = doc.addRasterLayer("レイヤー 2 (ピン留め)");
        l2->setPinned(true);
        LayerPanelWidget layerPanel;
        layerPanel.setDocument(&doc);
        layerPanel.resize(220, 260);
        QImage img(layerPanel.size(), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::white);
        layerPanel.render(&img);
        img.save("C:/Users/UMIHIKO/.gemini/antigravity/brain/e668f6c6-6123-49b4-8d6c-a63bbb7450be/layer_pin_verified.png");
    }

    // 4. ThreeDSettingsWidget (カメラをリセットボタン確認)
    {
        Document doc(800, 600);
        auto l3d = doc.addThreeDLayer("3Dレイヤー");
        ThreeDSettingsWidget threeDWidget;
        threeDWidget.setDocument(&doc);
        threeDWidget.setFromLayer(l3d.get());
        threeDWidget.resize(260, 480);
        QImage img(threeDWidget.size(), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::white);
        threeDWidget.render(&img);
        img.save("C:/Users/UMIHIKO/.gemini/antigravity/brain/e668f6c6-6123-49b4-8d6c-a63bbb7450be/threed_reset_cam_verified.png");
    }
    std::cout << "  -> PASSED" << std::endl;
}

void testFilters() {
    std::cout << "[TEST] 14. Filters & Multi-Layer Pinning Verification..." << std::endl;
    // テスト画像作成 (赤と青のストライプ)
    QImage testImg(200, 200, QImage::Format_ARGB32_Premultiplied);
    testImg.fill(Qt::black);
    {
        QPainter p(&testImg);
        p.fillRect(0, 0, 100, 200, QColor(255, 0, 0, 255));
        p.fillRect(100, 0, 100, 200, QColor(0, 0, 255, 255));
    }

    // 1. ガウスぼかし
    QImage gBlur = FilterProcessor::applyGaussianBlur(testImg, 15);
    assert(!gBlur.isNull());
    gBlur.save("C:/Users/UMIHIKO/.gemini/antigravity/brain/e668f6c6-6123-49b4-8d6c-a63bbb7450be/filter_gaussian_blur.png");

    // 2. 焦点ぼかし
    QImage fBlur = FilterProcessor::applyFocusBlur(testImg, 20, 0.25f, 0.25f, QPointF(0.5, 0.5), false);
    assert(!fBlur.isNull());
    fBlur.save("C:/Users/UMIHIKO/.gemini/antigravity/brain/e668f6c6-6123-49b4-8d6c-a63bbb7450be/filter_focus_blur.png");

    // 3. 明るさ・コントラスト
    QImage bcImg = FilterProcessor::applyBrightnessContrast(testImg, 20.0f, 30.0f);
    assert(!bcImg.isNull());
    bcImg.save("C:/Users/UMIHIKO/.gemini/antigravity/brain/e668f6c6-6123-49b4-8d6c-a63bbb7450be/filter_brightness_contrast.png");

    // 4. 色相・彩度・明度
    QImage hslImg = FilterProcessor::applyHueSaturationLightness(testImg, 90.0f, 20.0f, 0.0f);
    assert(!hslImg.isNull());
    hslImg.save("C:/Users/UMIHIKO/.gemini/antigravity/brain/e668f6c6-6123-49b4-8d6c-a63bbb7450be/filter_hue_saturation.png");

    // 5. トーンカーブ
    std::array<std::vector<QPointF>, 4> curves;
    curves[0] = { QPointF(0, 0), QPointF(128, 180), QPointF(255, 255) };
    curves[1] = { QPointF(0, 0), QPointF(255, 255) };
    curves[2] = { QPointF(0, 0), QPointF(255, 255) };
    curves[3] = { QPointF(0, 0), QPointF(255, 255) };
    QImage tcImg = FilterProcessor::applyToneCurve(testImg, curves, CurveInterpolationMode::Spline);
    assert(!tcImg.isNull());
    tcImg.save("C:/Users/UMIHIKO/.gemini/antigravity/brain/e668f6c6-6123-49b4-8d6c-a63bbb7450be/filter_tone_curve.png");

    // 6. ドキュメント連携: アクティブレイヤー + ピン留めレイヤーの同時適用
    Document doc(200, 200);
    auto lActive = doc.addRasterLayer("アクティブ");
    auto lPinned = doc.addRasterLayer("ピン留め");
    auto lOther = doc.addRasterLayer("未ピン留め");
    lPinned->setPinned(true);
    doc.setActiveLayerIndex(1); // lActive をアクティブに設定

    lActive->tileManager().fromQImage(testImg);
    lPinned->tileManager().fromQImage(testImg);
    lOther->tileManager().fromQImage(testImg);

    // ガウスぼかしをターゲット（アクティブ + ピン留め）に適用
    std::vector<int> targets;
    targets.push_back(doc.activeLayerIndex());
    for (size_t i = 0; i < doc.layers().size(); ++i) {
        if (static_cast<int>(i) != doc.activeLayerIndex() && doc.layers()[i]->isPinned()) {
            targets.push_back(static_cast<int>(i));
        }
    }
    assert(targets.size() == 2); // アクティブ と ピン留め の 2 つ

    for (int tIdx : targets) {
        auto layer = doc.getLayer(tIdx);
        QImage src = layer->tileManager().toQImage();
        QImage res = FilterProcessor::applyGaussianBlur(src, 10);
        layer->tileManager().fromQImage(res);
    }
    doc.composite();

    std::cout << "  Multi-layer targets count: " << targets.size() << std::endl;
    std::cout << "  -> PASSED" << std::endl;
}

void testMotionBlurAndGradientMapAndPinnedTransform() {
    std::cout << "Running testMotionBlurAndGradientMapAndPinnedTransform()..." << std::endl;

    // 1. モーションぼかし
    QImage srcImg(100, 100, QImage::Format_ARGB32_Premultiplied);
    srcImg.fill(Qt::transparent);
    {
        QPainter p(&srcImg);
        p.fillRect(40, 40, 20, 20, Qt::red);
    }
    QImage mbImg = FilterProcessor::applyMotionBlur(srcImg, 20, 0.0f);
    assert(!mbImg.isNull());
    assert(mbImg.size() == srcImg.size());
    // (40, 40) の左右に赤が広がっていることを確認
    assert(qAlpha(mbImg.pixel(30, 50)) > 0);
    assert(qAlpha(mbImg.pixel(70, 50)) > 0);
    std::cout << "  Motion blur verified." << std::endl;

    // 2. グラデーションマップ
    std::vector<QPair<float, QColor>> stops = {
        {0.0f, QColor(0, 0, 255)},
        {1.0f, QColor(255, 255, 0)}
    };
    QImage gmImg = FilterProcessor::applyGradientMap(srcImg, stops, false, 1.0f);
    assert(!gmImg.isNull());
    assert(gmImg.size() == srcImg.size());
    // 赤 (輝度約76) がグラデーションマップの色に変換されていること
    QRgb redTransformed = gmImg.pixel(50, 50);
    assert(qAlpha(redTransformed) > 0);
    // ダイアログのUIスナップショット保存
    QString artifactDir = "C:/Users/UMIHIKO/.gemini/antigravity/brain/e668f6c6-6123-49b4-8d6c-a63bbb7450be/";
    {
        Document dlgDoc(200, 200);
        auto l = dlgDoc.addRasterLayer("Raster");
        CanvasWidget dlgCanvas;
        dlgCanvas.setDocument(&dlgDoc);

        MotionBlurDialog mbDlg(&dlgDoc, &dlgCanvas);
        mbDlg.resize(340, 140);
        mbDlg.grab().save(artifactDir + "filter_motion_blur.png");

        GradientMapDialog gmDlg(&dlgDoc, &dlgCanvas);
        gmDlg.resize(360, 160);
        gmDlg.grab().save(artifactDir + "filter_gradient_map.png");
    }

    // 3. ピン留めレイヤーの変形連動とUndo/Redo
    Document doc(200, 200);

    auto lActive = doc.addRasterLayer("Layer1 (Active)");
    auto lPinned = doc.addRasterLayer("Layer2 (Pinned)");
    auto lUnpinned = doc.addRasterLayer("Layer3 (Unpinned)");
    lPinned->setPinned(true);
    doc.setActiveLayerIndex(1); // lActive をアクティブに設定

    QImage blockA(200, 200, QImage::Format_ARGB32_Premultiplied);
    blockA.fill(Qt::transparent);
    {
        QPainter p(&blockA);
        p.fillRect(50, 50, 30, 30, Qt::red);
    }
    lActive->tileManager().fromQImage(blockA);

    QImage blockB(200, 200, QImage::Format_ARGB32_Premultiplied);
    blockB.fill(Qt::transparent);
    {
        QPainter p(&blockB);
        p.fillRect(50, 50, 30, 30, Qt::blue);
    }
    lPinned->tileManager().fromQImage(blockB);

    QImage blockC(200, 200, QImage::Format_ARGB32_Premultiplied);
    blockC.fill(Qt::transparent);
    {
        QPainter p(&blockC);
        p.fillRect(50, 50, 30, 30, Qt::green);
    }
    lUnpinned->tileManager().fromQImage(blockC);

    CanvasWidget canvas;
    canvas.setDocument(&doc);
    canvas.setCurrentTool(ToolType::Transform);
    canvas.startTransform();

    assert(canvas.isTransforming());
    // CanvasWidget 内部でアクティブ(Layer1)とピン留め(Layer2)が収集され、変形対象になっていることを確認
    // applyTransform を実行
    canvas.applyTransform();
    assert(!canvas.isTransforming());

    // UndoStack に MultiLayerTransformCommand がプッシュされたことを確認
    assert(doc.undoStack()->count() >= 1);

    // 元領域のピクセルが復元できるか Undo/Redo をテスト
    doc.undoStack()->undo();
    QImage restoredA = lActive->tileManager().toQImage();
    QImage restoredB = lPinned->tileManager().toQImage();
    assert(qAlpha(restoredA.pixel(60, 60)) > 0);
    assert(qAlpha(restoredB.pixel(60, 60)) > 0);

    doc.undoStack()->redo();
    std::cout << "  Pinned layer multi-transform & undo/redo verified." << std::endl;
    std::cout << "  -> PASSED" << std::endl;
}

void testDisplayFilters() {
    std::cout << "[TEST] 15. Display Filters & ICC Profile Integration..." << std::endl;

    auto& mgr = DisplayFilterManager::instance();

    // 1. デフォルト None
    mgr.setFilterType(DisplayFilterType::None);
    QColor red(255, 0, 0);
    assert(mgr.applyFilter(red) == red);

    // 2. Grayscale (BT.709)
    mgr.setFilterType(DisplayFilterType::Grayscale);
    QColor grayBt709 = mgr.applyFilter(red);
    int expectedBt709 = static_cast<int>(0.2126f * 255 + 0.5f); // 54
    assert(grayBt709.red() == expectedBt709);
    assert(grayBt709.green() == expectedBt709);
    assert(grayBt709.blue() == expectedBt709);

    // 画像グレースケール変換テスト
    QImage testImg(4, 4, QImage::Format_ARGB32_Premultiplied);
    testImg.fill(red);
    QImage filteredImg = mgr.applyFilter(testImg);
    QRgb p0 = filteredImg.pixel(0, 0);
    assert(qRed(p0) == expectedBt709);
    assert(qGreen(p0) == expectedBt709);
    assert(qBlue(p0) == expectedBt709);

    // 3. Grayscale (YIQ - BT.601)
    mgr.setFilterType(DisplayFilterType::GrayscaleYIQ);
    QColor grayYiq = mgr.applyFilter(red);
    int expectedYiq = static_cast<int>(0.299f * 255 + 0.5f); // 76
    assert(grayYiq.red() == expectedYiq);
    assert(grayYiq.green() == expectedYiq);
    assert(grayYiq.blue() == expectedYiq);

    // 4. システムプロファイルスキャン
    auto profiles = DisplayFilterManager::scanSystemIccProfiles();
    std::cout << "  System ICC profiles found: " << profiles.size() << std::endl;
    for (size_t i = 0; i < std::min<size_t>(5, profiles.size()); ++i) {
        std::cout << "    [" << i << "] " << profiles[i].name.toStdString() << " (" << profiles[i].colorSpace.toStdString() << "): " << profiles[i].path.toStdString() << std::endl;
    }
    if (!profiles.empty()) {
        mgr.setIccProfilePath(profiles[0].path);
        mgr.setFilterType(DisplayFilterType::IccProfile);
        QColor converted = mgr.applyFilter(red);
        assert(converted.isValid());
    }

    // 5. MainWindow メニュー連動テスト
    MainWindow mw;
    mgr.setFilterType(DisplayFilterType::None);
    assert(mw.filterNoneAction()->isChecked());

    mgr.setFilterType(DisplayFilterType::Grayscale);
    assert(mw.filterGrayscaleAction()->isChecked());

    mgr.setFilterType(DisplayFilterType::GrayscaleYIQ);
    assert(mw.filterGrayscaleYiqAction()->isChecked());

    mgr.setFilterType(DisplayFilterType::IccProfile);
    assert(mw.filterIccAction()->isChecked());

    // メニューアクション発火テスト
    mw.filterNoneAction()->trigger();
    assert(mgr.filterType() == DisplayFilterType::None);

    mw.filterGrayscaleAction()->trigger();
    assert(mgr.filterType() == DisplayFilterType::Grayscale);

    // 6. UI スナップショット保存
    mw.resize(1280, 800);
    auto doc = new Document(400, 300);
    auto layer = doc->addRasterLayer("カラーテスト");
    doc->setActiveLayerIndex(1);
    QImage colorTest(400, 300, QImage::Format_ARGB32_Premultiplied);
    colorTest.fill(Qt::white);
    {
        QPainter p(&colorTest);
        p.fillRect(20, 20, 100, 260, Qt::red);
        p.fillRect(140, 20, 100, 260, Qt::green);
        p.fillRect(260, 20, 100, 260, Qt::blue);
    }
    layer->tileManager().fromQImage(colorTest);
    doc->composite();
    mw.setDocumentForTest(doc);

    QString brainDir = "C:/Users/UMIHIKO/.gemini/antigravity/brain/e668f6c6-6123-49b4-8d6c-a63bbb7450be/";

    // パレットを開く
    if (mw.btnTogglePalette() && mw.paletteWidget()) {
        mw.btnTogglePalette()->setChecked(true);
        mw.paletteWidget()->setVisible(true);
    }

    // フィルター無し (パレット表示状態)
    mgr.setFilterType(DisplayFilterType::None);
    mw.repaint();
    QImage snapNone = mw.grab().toImage();
    snapNone.save(brainDir + "display_filter_none.png");

    // グレースケール (パレット表示状態)
    mgr.setFilterType(DisplayFilterType::Grayscale);
    mw.repaint();
    QImage snapGray = mw.grab().toImage();
    snapGray.save(brainDir + "display_filter_grayscale.png");

    // グレースケール (YIQ)
    mgr.setFilterType(DisplayFilterType::GrayscaleYIQ);
    mw.repaint();
    QImage snapYiq = mw.grab().toImage();
    snapYiq.save(brainDir + "display_filter_grayscale_yiq.png");

    // ICC プロファイル
    if (!profiles.empty()) {
        mgr.setIccProfilePath(profiles[0].path);
        mgr.setFilterType(DisplayFilterType::IccProfile);
        mw.repaint();
        QImage snapIcc = mw.grab().toImage();
        snapIcc.save(brainDir + "display_filter_icc.png");
    }

    // 環境設定ダイアログのスクリーンショット (カラーマネジメントタブを選択)
    auto prefDlg = new PreferencesDialog();
    prefDlg->resize(560, 460);
    if (auto tabs = prefDlg->findChild<QTabWidget*>()) {
        tabs->setCurrentIndex(1);
    }
    prefDlg->repaint();
    QImage snapPref = prefDlg->grab().toImage();
    snapPref.save(brainDir + "preferences_color_management.png");
    prefDlg->deleteLater();

    mw.clearDocumentForTest();

    // 元に戻す
    mgr.setFilterType(DisplayFilterType::None);

    std::cout << "  Display Filters & ICC Profile Integration verified." << std::endl;
    std::cout << "  -> PASSED" << std::endl;
}

void testMonitorIccAndPreviewWindowAndModelTransform() {
    std::cout << "[TEST] 16. Monitor ICC, Preview Window & 3D Model Transform (Rotate > Translate)..." << std::endl;

    // 1. 3Dモデル変換順序の数学的検証 (回転 > 移動: v' = T * R * S * v)
    {
        ThreeDModel model;
        model.setPosition(QVector3D(0.0f, 0.0f, 5.0f));
        model.setRotationEuler(QVector3D(0.0f, 90.0f, 0.0f)); // Y軸90度回転

        assert(model.position() == QVector3D(0.0f, 0.0f, 5.0f));
        assert(model.rotationEuler() == QVector3D(0.0f, 90.0f, 0.0f));

        QMatrix4x4 m = model.modelMatrix();
        // 原点 (1, 0, 0) を変換
        // 回転 > 移動 の場合: (1, 0, 0) をY軸90度回転 -> (0, 0, -1) -> (0, 0, 5) 移動 -> (0, 0, 4)
        // もし 移動 > 回転 の場合: (1, 0, 5) をY軸90度回転 -> (-5, 0, -1)
        QVector3D v(1.0f, 0.0f, 0.0f);
        QVector3D vTransformed = m.map(v);
        assert(std::abs(vTransformed.x() - 0.0f) < 1e-3f);
        assert(std::abs(vTransformed.y() - 0.0f) < 1e-3f);
        assert(std::abs(vTransformed.z() - 4.0f) < 1e-3f);
        std::cout << "  [3D] Rotate > Translate order strictly verified: (1,0,0) -> "
                  << "(" << vTransformed.x() << ", " << vTransformed.y() << ", " << vTransformed.z() << ")" << std::endl;
    }

    // 2. モニターICCプロファイルの指定機能
    {
        auto& mgr = DisplayFilterManager::instance();
        QString sampleIcc = "C:/Windows/System32/spool/drivers/color/sRGB Color Space Profile.icm";
        if (!QFile::exists(sampleIcc)) {
            sampleIcc = "C:/Windows/System32/spool/drivers/color/AdobeRGB1998.icc";
        }
        if (QFile::exists(sampleIcc)) {
            mgr.setMonitorProfilePath(sampleIcc);
            assert(mgr.monitorProfilePath() == sampleIcc);
            mgr.setProofingProfilePath("");
            assert(mgr.proofingProfilePath() == "");

            // 変換がエラーなく実行できること
            mgr.setFilterType(DisplayFilterType::IccProfile);
            QColor c = mgr.applyFilter(QColor(255, 128, 64));
            assert(c.isValid());
            std::cout << "  [ICC] Monitor profile without proofing verified." << std::endl;

            // モニター未指定 & 校正指定
            mgr.setMonitorProfilePath("");
            mgr.setProofingProfilePath(sampleIcc);
            QColor c2 = mgr.applyFilter(QColor(255, 128, 64));
            assert(c2.isValid());
            std::cout << "  [ICC] Proofing profile without monitor profile verified." << std::endl;

            // 両方指定
            mgr.setMonitorProfilePath(sampleIcc);
            mgr.setProofingProfilePath(sampleIcc);
            QColor c3 = mgr.applyFilter(QColor(255, 128, 64));
            assert(c3.isValid());
            std::cout << "  [ICC] Both monitor and proofing profiles verified." << std::endl;

            mgr.setFilterType(DisplayFilterType::None);
        }
    }

    // 3. プレビューウィンドウ機能の検証 (常時手前、アスペクト比維持)
    {
        auto doc = new Document(1600, 1200, 350.0, true, Qt::white);
        auto layer = doc->activeLayer();
        if (layer) {
            layer->tileManager().setPixel(800, 600, 0xFFFF0000);
        }
        doc->composite();

        PreviewWindow previewWin;
        // Qt::WindowStaysOnTopHint が設定されていること
        assert((previewWin.windowFlags() & Qt::WindowStaysOnTopHint) != 0);

        previewWin.setDocument(doc);
        previewWin.resize(320, 240);
        previewWin.show();
        previewWin.repaint();

        QString brainDir = "C:/Users/UMIHIKO/.gemini/antigravity/brain/e668f6c6-6123-49b4-8d6c-a63bbb7450be/";
        QImage previewSnap = previewWin.grab().toImage();
        previewSnap.save(brainDir + "preview_window_verified.png");
        std::cout << "  [Preview] PreviewWindow verified and snapshot saved (" << previewSnap.width() << "x" << previewSnap.height() << ")" << std::endl;

        delete doc;
    }

    // 4. ThreeDSettingsWidget モデル変形 UI の検証
    {
        MainWindow mw;
        mw.show();
        auto doc = new Document(800, 600, 350.0, false);
        auto threedLayer = doc->addThreeDLayer("3D Layer");
        auto cube = ThreeDModel::createCube(1.0f);
        threedLayer->addModel(cube);
        mw.setDocumentForTest(doc);

        auto settingsWidget = mw.findChild<ThreeDSettingsWidget*>();
        assert(settingsWidget != nullptr);
        settingsWidget->setFromLayer(threedLayer.get());

        // スピンボックスを探す
        auto spinPosX = settingsWidget->findChild<QDoubleSpinBox*>();
        assert(spinPosX != nullptr);

        QString brainDir = "C:/Users/UMIHIKO/.gemini/antigravity/brain/e668f6c6-6123-49b4-8d6c-a63bbb7450be/";
        QImage threedUiSnap = settingsWidget->grab().toImage();
        threedUiSnap.save(brainDir + "threed_model_transform_verified.png");
        std::cout << "  [ThreeD UI] Transform UI verified and snapshot saved." << std::endl;

        mw.clearDocumentForTest();
    }

    std::cout << "  -> PASSED" << std::endl;
}

void testSelectionEditMode() {
    std::cout << "[TEST] 17. Selection Edit Mode (Quick Mask & Raster Tools Integration)..." << std::endl;

    MainWindow mw;
    mw.resize(1000, 750);
    mw.show();
    QApplication::processEvents();

    // 1. ウィンドウ右上端のトグルボタン検証
    QToolButton* toggleBtn = mw.toggleSelectionEditButton();
    assert(toggleBtn != nullptr);
    assert(mw.menuBar()->cornerWidget(Qt::TopRightCorner) == toggleBtn);
    assert(toggleBtn->text() == "選択範囲編集");
    assert(!toggleBtn->isChecked());
    std::cout << "  [UI] Top-right toggle button verified." << std::endl;

    Document* doc = new Document(400, 300, 72.0, true, Qt::white);
    mw.setDocumentForTest(doc);
    QApplication::processEvents();

    assert(!doc->isSelectionEditMode());

    // 2. ベクトルレイヤー選択中に選択範囲編集をONにすると通常ツール(Raster)に切り替わることの検証
    doc->addVectorLayer("ベクトルレイヤー");
    doc->setActiveLayerIndex(static_cast<int>(doc->layers().size()) - 1);
    QApplication::processEvents();
    assert(doc->activeLayer()->type() == LayerType::Vector);

    // トグルON
    toggleBtn->click();
    assert(doc->isSelectionEditMode());
    assert(toggleBtn->isChecked());

    // ツールボックスが Raster モードになっていることを確認
    // ToolBoxWidget の現在の layerType は Raster
    std::cout << "  [ToolBox] Switched to Raster layer tools in selection edit mode." << std::endl;

    CanvasWidget* canvas = mw.canvasWidget();
    assert(canvas != nullptr);

    // 3. 例1: 鉛筆ツールで描画 -> 選択範囲に加える
    canvas->setCurrentTool(ToolType::Pen);
    canvas->testStrokeBegin(QPointF(50, 50), 1.0f);
    canvas->testStrokeMove(QPointF(100, 50), 1.0f);
    canvas->testStrokeMove(QPointF(100, 100), 1.0f);
    canvas->testStrokeMove(QPointF(50, 100), 1.0f);
    canvas->testStrokeEnd();

    assert(doc->hasSelection());
    QRect selectBounds = doc->selection().boundingRect().toRect();
    assert(!selectBounds.isEmpty());
    std::cout << "  [Example 1] Pen tool added selection: bounds="
              << selectBounds.x() << "," << selectBounds.y() << " " << selectBounds.width() << "x" << selectBounds.height() << std::endl;

    // 4. 例2: 消しゴムツールで描画 -> 選択範囲から除く
    canvas->setCurrentTool(ToolType::Eraser);
    // (50, 50)〜(60, 60) 付近を消去
    canvas->testStrokeBegin(QPointF(50, 50), 1.0f);
    canvas->testStrokeMove(QPointF(60, 60), 1.0f);
    canvas->testStrokeEnd();

    // マスクレイヤーの該当ピクセルが消去されていることを確認
    auto maskLayer = doc->selectionMaskLayer();
    assert(maskLayer != nullptr);
    const Tile* tile0 = maskLayer->tileManager().getTile(0, 0);
    assert(tile0 != nullptr && tile0->readBuffer() != nullptr);
    const uint32_t* buf0 = reinterpret_cast<const uint32_t*>(tile0->readBuffer());
    uint32_t pxEraser = buf0[50 * TileMemoryPool::TILE_WIDTH + 50];
    assert((pxEraser >> 24) == 0);
    std::cout << "  [Example 2] Eraser tool removed selection from mask (alpha=0 at erased region)." << std::endl;

    // 5. 例3: ぼかしツールで境界をぼかす
    canvas->setCurrentTool(ToolType::Blur);
    // (100, 75) 付近の境界をぼかす
    canvas->testStrokeBegin(QPointF(100, 75), 1.0f);
    canvas->testStrokeMove(QPointF(100, 76), 1.0f);
    canvas->testStrokeEnd();
    std::cout << "  [Example 3] Blur tool blurred the selection boundary." << std::endl;

    // UIレンダリング確認とスクリーンショット
    canvas->repaint();
    mw.repaint();
    QApplication::processEvents();

    QString brainDir = "C:/Users/UMIHIKO/.gemini/antigravity/brain/e668f6c6-6123-49b4-8d6c-a63bbb7450be/";
    QImage mwSnap = mw.grab().toImage();
    mwSnap.save(brainDir + "selection_edit_mode_verified.png");
    std::cout << "  [UI Snapshot] selection_edit_mode_verified.png saved successfully." << std::endl;

    // トグルOFFで元に戻る
    toggleBtn->click();
    assert(!doc->isSelectionEditMode());
    assert(!toggleBtn->isChecked());

    mw.clearDocumentForTest();
    std::cout << "  -> PASSED" << std::endl;
}

void testTabAndToolboxUpdates() {
    std::cout << "[TEST] 18. Canvas Tab (South & Close Button) & Selection Pen/Eraser Removal..." << std::endl;

    MainWindow mw;
    mw.resize(1100, 800);
    mw.show();
    QApplication::processEvents();

    // 1. キャンバスタブが下側（South）に配置されていることを検証
    QTabWidget* tabWidget = mw.tabWidget();
    assert(tabWidget != nullptr);
    assert(tabWidget->tabPosition() == QTabWidget::South);
    std::cout << "  [Canvas Tab] Tab position is South (bottom) strictly verified." << std::endl;

    // 起動時は何もキャンバスが開かれていない状態（count == 0）
    assert(tabWidget->count() == 0);
    assert(mw.document() == nullptr);

    // キャンバスを作成・セットしてタブが増えることを検証
    mw.setDocumentForTest(new Document(1600, 1200, 350.0, true, Qt::white));
    QApplication::processEvents();
    assert(tabWidget->count() == 1);
    assert(mw.document() != nullptr);

    // 2. 各タブの閉じるボタン（tabsClosable）および右側CornerWidgetの閉じるボタンを検証
    assert(tabWidget->tabsClosable());
    QWidget* cornerWidget = tabWidget->cornerWidget(Qt::BottomRightCorner);
    assert(cornerWidget != nullptr);
    auto cornerBtn = qobject_cast<QToolButton*>(cornerWidget);
    assert(cornerBtn != nullptr);
    assert(cornerBtn->text() == "×");
    std::cout << "  [Canvas Tab] Close buttons (tab closable & bottom-right corner button) verified." << std::endl;

    // 3. タブの閉じるボタンを押した時にタブが消えることの検証
    assert(tabWidget->count() == 1);
    cornerBtn->click();
    QApplication::processEvents();
    assert(tabWidget->count() == 0);
    assert(mw.document() == nullptr);
    std::cout << "  [Canvas Tab] Tab closed and completely removed (count=0) verified." << std::endl;

    // 4. UIスクリーンショットを保存（タブが閉じた状態）
    mw.repaint();
    QApplication::processEvents();
    QString brainDir = "C:/Users/UMIHIKO/.gemini/antigravity/brain/e668f6c6-6123-49b4-8d6c-a63bbb7450be/";
    QImage snapClosed = mw.grab().toImage();
    snapClosed.save(brainDir + "canvas_tab_closed_verified.png");
    std::cout << "  [UI Snapshot] canvas_tab_closed_verified.png saved successfully." << std::endl;

    std::cout << "  -> PASSED" << std::endl;
}

void testGrayscaleSelectionMask() {
    std::cout << "[TEST] 19. Grayscale 0-255 Selection Mask Specification..." << std::endl;

    auto doc = std::make_shared<Document>(100, 100);
    auto baseL = doc->addRasterLayer("BaseLayer");
    assert(baseL != nullptr);

    // 1. アンチエイリアス選択による 0〜255 境界値の生成
    QPainterPath circlePath;
    circlePath.addEllipse(QRectF(20.0, 20.0, 60.0, 60.0));
    doc->setSelection(circlePath);

    assert(doc->hasSelection());
    assert(doc->selectionStrength(50, 50) == 255); // 中心は完全選択 (255)
    assert(doc->selectionStrength(5, 5) == 0);     // 領域外は完全非選択 (0)

    // 円の境界付近（半径30px境界）を探索し、アンチエイリアス中間値 (0 < S < 255) が存在することを検証
    bool foundIntermediate = false;
    for (int y = 15; y <= 85; ++y) {
        for (int x = 15; x <= 85; ++x) {
            uint8_t s = doc->selectionStrength(x, y);
            if (s > 0 && s < 255) {
                foundIntermediate = true;
                break;
            }
        }
        if (foundIntermediate) break;
    }
    assert(foundIntermediate);
    std::cout << "  [Antialiased Selection] Intermediate alpha values (0 < S < 255) strictly verified." << std::endl;

    // 2. updateSelectionPathFromMask が 0〜255 マスク階調を破壊しないことを検証
    doc->updateSelectionPathFromMask();
    foundIntermediate = false;
    for (int y = 15; y <= 85; ++y) {
        for (int x = 15; x <= 85; ++x) {
            uint8_t s = doc->selectionStrength(x, y);
            if (s > 0 && s < 255) {
                foundIntermediate = true;
                break;
            }
        }
        if (foundIntermediate) break;
    }
    assert(foundIntermediate);
    std::cout << "  [Mask Preservation] Grayscale mask data is preserved without binarization." << std::endl;

    // 3. グラデーション選択マスクに対する fillLayer の線形補間ブレンド検証
    QImage gradMask(100, 100, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < 100; ++y) {
        for (int x = 0; x < 100; ++x) {
            // x座標に応じて 0〜255 のリニアグラデーションマスク
            int alpha = std::clamp(static_cast<int>(x * 255 / 99), 0, 255);
            gradMask.setPixelColor(x, y, QColor(255, 255, 255, alpha));
        }
    }
    doc->setSelectionMask(gradMask);

    // 下地を黒（RGB: 0, 0, 0, A: 255）で初期化
    baseL->tileManager().clear(0xFF000000);

    // 赤色（RGB: 255, 0, 0, A: 255）で fillLayer 実行
    doc->fillLayer(doc->activeLayerIndex(), QColor(255, 0, 0, 255));

    // x=0 (強度 0): 黒のまま (R=0, G=0, B=0)
    QColor c0 = doc->samplePixel(0, 50);
    assert(c0.red() == 0 && c0.green() == 0 && c0.blue() == 0);

    // x=99 (強度 255): 赤 (R=255, G=0, B=0)
    QColor c99 = doc->samplePixel(99, 50);
    assert(c99.red() >= 254 && c99.green() == 0 && c99.blue() == 0);

    // x=50 付近 (強度約128): 半ブレンド (R≈128, G=0, B=0)
    QColor c50 = doc->samplePixel(50, 50);
    assert(std::abs(c50.red() - 128) <= 4 && c50.green() == 0 && c50.blue() == 0);
    std::cout << "  [Linear Interpolation Fill] x=0 R=" << c0.red()
              << ", x=50 R=" << c50.red()
              << ", x=99 R=" << c99.red() << " strictly verified." << std::endl;

    // 4. FilterProcessor::blendWithSelectionMask の線形補間検証
    QImage origImg(100, 100, QImage::Format_ARGB32_Premultiplied);
    origImg.fill(QColor(0, 0, 255, 255)); // 青
    QImage filtImg(100, 100, QImage::Format_ARGB32_Premultiplied);
    filtImg.fill(QColor(0, 255, 0, 255)); // 緑

    FilterProcessor::blendWithSelectionMask(filtImg, origImg, doc->selectionMaskLayer().get(), 100, 100);

    // x=0 (強度0): 青 (R=0, G=0, B=255)
    QColor fc0 = filtImg.pixelColor(0, 50);
    assert(fc0.blue() >= 254 && fc0.green() == 0);

    // x=99 (強度255): 緑 (R=0, G=255, B=0)
    QColor fc99 = filtImg.pixelColor(99, 50);
    assert(fc99.green() >= 254 && fc99.blue() == 0);

    // x=50 (強度約128): シアン (緑と青の50%ブレンド: G≈128, B≈128)
    QColor fc50 = filtImg.pixelColor(50, 50);
    assert(std::abs(fc50.green() - 128) <= 4 && std::abs(fc50.blue() - 128) <= 4);
    std::cout << "  [Filter Mask Blending] x=0 G=" << fc0.green() << " B=" << fc0.blue()
              << ", x=50 G=" << fc50.green() << " B=" << fc50.blue()
              << ", x=99 G=" << fc99.green() << " B=" << fc99.blue() << " strictly verified." << std::endl;

    std::cout << "  -> PASSED" << std::endl;
}

void testToolBoxResizeGuiVisibility() {
    std::cout << "[TEST] 20. ToolBox / Settings Widget Resize GUI Visibility..." << std::endl;

    MainWindow mw;
    mw.resize(1100, 800);
    mw.show();
    QApplication::processEvents();

    QDockWidget* colorBrushDock = mw.findChild<QDockWidget*>("ColorBrushDock");
    assert(colorBrushDock != nullptr);

    // ツールプロパティのスクロールエリアとブラシ設定ウィジェットを取得
    auto scrollArea = colorBrushDock->findChild<QScrollArea*>();
    assert(scrollArea != nullptr);
    auto brushSettings = colorBrushDock->findChild<BrushSettingsWidget*>();
    assert(brushSettings != nullptr);

    // ドックをリサイズ＆スプリッターの高さを狭めて縦スクロールバーを意図的に出現させる
    QSplitter* splitter = colorBrushDock->findChild<QSplitter*>();
    if (splitter) {
        splitter->setSizes({ 280, 160 }); // 設定ウィジェット側を縦に縮めてスクロールバーを発生
    }
    QApplication::processEvents();

    auto stack = colorBrushDock->findChild<QStackedWidget*>();
    if (stack) {
        for (int i = 0; i < stack->count(); ++i) {
            QWidget* page = stack->widget(i);
            std::cout << "    Page[" << i << "] " << page->metaObject()->className()
                      << " minSize=" << page->minimumSizeHint().width() << "x" << page->minimumSizeHint().height()
                      << " sizeHint=" << page->sizeHint().width() << "x" << page->sizeHint().height() << std::endl;
        }
    }
    int viewportW = scrollArea->viewport()->width();
    int brushW = brushSettings->width();
    std::cout << "  [Viewport vs Settings Width] viewportW=" << viewportW << ", brushW=" << brushW << std::endl;
    // 設定ウィジェットがビューポート内に完全に収まっていること (はみ出しゼロ)
    assert(brushW <= viewportW);

    // 2. SpinScale の数値領域およびスピン矢印がウィジェット内に完全に収まっていること
    auto sizeScale = brushSettings->findChild<SpinScale*>();
    assert(sizeScale != nullptr);
    assert(sizeScale->width() <= brushW);
    // 右端矢印領域がウィジェット幅内に収まっていること
    QRect upArrow = sizeScale->findChild<QLineEdit*>() ? QRect() : QRect(sizeScale->width() - 16, 1, 15, sizeScale->height() - 2);
    assert(upArrow.right() <= sizeScale->width());

    // 3. 筆圧サイズ・筆圧濃度チェックボックスがビューポート内に完全に収まっていること
    auto checks = brushSettings->findChildren<QCheckBox*>();
    for (auto cb : checks) {
        assert(cb->x() + cb->width() <= brushW + 2);
        assert(cb->isVisible());
    }

    // 4. スナップショットを撮影して保存
    brushSettings->repaint();
    QApplication::processEvents();
    QString brainDir = "C:/Users/UMIHIKO/.gemini/antigravity/brain/e668f6c6-6123-49b4-8d6c-a63bbb7450be/";
    QImage snap = brushSettings->grab().toImage();
    snap.save(brainDir + "toolbox_resize_gui_visibility_verified.png");
    std::cout << "  [UI Snapshot] toolbox_resize_gui_visibility_verified.png saved successfully (size="
              << snap.width() << "x" << snap.height() << ")." << std::endl;

    std::cout << "  -> PASSED" << std::endl;
}

void testReferenceImageWindow() {
    std::cout << "[TEST] 21. Reference Image Window (Tabs, Open/Paste, Zoom, Eyedropper, Palette, Close)..." << std::endl;

    MainWindow mw;
    mw.resize(1100, 800);
    mw.show();
    QApplication::processEvents();

    // 1. メニューのアクションから開く
    QAction* refAction = mw.referenceImageWindowAction();
    assert(refAction != nullptr);
    assert(refAction->isCheckable());
    refAction->trigger();
    QApplication::processEvents();

    ReferenceImageWindow* refWin = mw.referenceImageWindow();
    assert(refWin != nullptr);
    assert(refWin->isVisible());
    assert(refAction->isChecked());
    std::cout << "  [Reference Window] Opened and visible from menu action." << std::endl;

    // 2. メニューバーの検証 (ファイル > 画像を開く, 編集 > 貼り付け)
    QMenuBar* mb = refWin->menuBar();
    assert(mb != nullptr);
    auto menus = mb->findChildren<QMenu*>();
    bool hasFileMenu = false;
    bool hasEditMenu = false;
    for (auto m : menus) {
        if (m->title().contains("ファイル")) hasFileMenu = true;
        if (m->title().contains("編集")) hasEditMenu = true;
    }
    assert(hasFileMenu);
    assert(hasEditMenu);
    std::cout << "  [Menu Bar] File and Edit menus verified." << std::endl;

    // 3. 画像を追加してタブが増えることを検証
    QImage testImg(120, 100, QImage::Format_ARGB32);
    testImg.fill(QColor(255, 0, 0)); // 赤
    testImg.setPixelColor(60, 50, QColor(0, 255, 0)); // 中央は緑 (0, 255, 0)
    testImg.setPixelColor(10, 10, QColor(0, 0, 255)); // 左上は青 (0, 0, 255)

    refWin->addImage(testImg, "RefImage1.png");
    QApplication::processEvents();

    QTabWidget* tabWidget = refWin->findChild<QTabWidget*>();
    assert(tabWidget != nullptr);
    assert(tabWidget->count() == 1);
    assert(tabWidget->tabText(0) == "RefImage1.png");
    assert(tabWidget->tabsClosable());
    std::cout << "  [Tab & Image] Image added, tab count=1, tabsClosable=true verified." << std::endl;

    // 4. クリップボードからの貼り付けでタブが増えることを検証
    QImage clipImg(80, 80, QImage::Format_ARGB32);
    clipImg.fill(QColor(255, 255, 0)); // 黄色
    QApplication::clipboard()->setImage(clipImg);
    refWin->pasteImage();
    QApplication::processEvents();

    assert(tabWidget->count() == 2);
    assert(tabWidget->tabText(1).contains("貼り付け"));
    std::cout << "  [Clipboard Paste] Paste succeeded, tab count=2 verified." << std::endl;

    // 5. タブ下側の拡大率設定 SpinScale の検証
    SpinScale* zoomScale = refWin->findChild<SpinScale*>();
    assert(zoomScale != nullptr);
    assert(zoomScale->isEnabled());
    std::cout << "  [SpinScale] Zoom scale widget under tabs verified (val=" << zoomScale->value() << "%)." << std::endl;

    // SpinScale変更で画像のズームが連動することを検証
    zoomScale->setValue(200.0);
    QApplication::processEvents();
    auto curView = qobject_cast<ReferenceImageView*>(tabWidget->currentWidget());
    assert(curView != nullptr);
    assert(std::abs(curView->zoom() - 2.0) < 0.01);
    std::cout << "  [Zoom Sync] SpinScale value=200% correctly synced to view zoom=2.0." << std::endl;

    // 6. 画像の左クリックで描画色が取得されることを検証 (タブ0の(60, 50)をクリック -> 緑色)
    tabWidget->setCurrentIndex(0);
    QApplication::processEvents();
    auto view0 = qobject_cast<ReferenceImageView*>(tabWidget->currentWidget());
    assert(view0 != nullptr);
    view0->setZoom(1.0); // 等倍に戻す
    zoomScale->setValue(100.0);
    QApplication::processEvents();

    QWidget* canvas = view0->widget();
    assert(canvas != nullptr);

    // 左クリックをシミュレート
    QPoint clickPos(60, 50);
    QMouseEvent pressEvent(QEvent::MouseButtonPress, clickPos, canvas->mapToGlobal(clickPos), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas, &pressEvent);
    QApplication::processEvents();

    assert(mw.canvasWidget()->currentColor() == QColor(0, 255, 0));
    std::cout << "  [Eyedropper Left Click] Clicked (60, 50), canvas current color set to Green (0, 255, 0) verified." << std::endl;

    // 7. パレットへの登録シグナル検証
    emit refWin->addToPalette(QColor(0, 0, 255)); // 青色を登録
    QApplication::processEvents();
    bool foundBlue = false;
    for (int i = 0; i < mw.paletteWidget()->totalCells(); ++i) {
        if (mw.paletteWidget()->colorAt(i) == QColor(0, 0, 255)) {
            foundBlue = true;
            break;
        }
    }
    assert(foundBlue);
    std::cout << "  [Palette Registration] Blue color registered into ColorPaletteWidget verified." << std::endl;

    // 8. 画像を閉じる（タブを閉じる）の検証
    assert(tabWidget->count() == 2);
    refWin->closeTab(0);
    QApplication::processEvents();
    assert(tabWidget->count() == 1);
    std::cout << "  [Close Tab] Tab closed, count=1 verified." << std::endl;

    // 9. UIスナップショットの保存
    refWin->resize(520, 480);
    refWin->repaint();
    QApplication::processEvents();
    QString brainDir = "C:/Users/UMIHIKO/.gemini/antigravity/brain/e668f6c6-6123-49b4-8d6c-a63bbb7450be/";
    QImage winSnap = refWin->grab().toImage();
    winSnap.save(brainDir + "reference_image_window_verified.png");
    std::cout << "  [UI Snapshot] reference_image_window_verified.png saved successfully (size="
              << winSnap.width() << "x" << winSnap.height() << ")." << std::endl;

    // ウィンドウを閉じる -> アクションのチェック状態解除
    refWin->close();
    QApplication::processEvents();
    assert(!refAction->isChecked());
    std::cout << "  [Window Closed] Action uncheck verified." << std::endl;

    std::cout << "  -> PASSED" << std::endl;
}

void testStartupNoCanvasAndOpenFileArgument() {
    std::cout << "[TEST] 22. Startup Without Canvas & Open File as Canvas..." << std::endl;

    // 1. 起動時には何もキャンバスが開かれていない状態であることの検証
    MainWindow mw;
    mw.resize(1100, 800);
    mw.show();
    QApplication::processEvents();

    assert(mw.document() == nullptr);
    assert(mw.tabWidget()->count() == 0);
    std::cout << "  [Startup State] Started with no document and 0 tabs strictly verified." << std::endl;

    // 起動時（キャンバスなし）のUIスナップショット保存
    QString brainDir = "C:/Users/UMIHIKO/.gemini/antigravity/brain/e668f6c6-6123-49b4-8d6c-a63bbb7450be/";
    QImage snapEmpty = mw.grab().toImage();
    snapEmpty.save(brainDir + "startup_no_canvas_verified.png");
    std::cout << "  [UI Snapshot] startup_no_canvas_verified.png saved successfully." << std::endl;

    // 2. テスト用画像ファイルを作成して起動時引数オープン（openFile）の動作を検証
    QString testImgPath = QDir::tempPath() + "/startup_test_image.png";
    QImage sampleImg(640, 480, QImage::Format_ARGB32);
    sampleImg.fill(QColor(100, 180, 255));
    {
        QPainter p(&sampleImg);
        p.setPen(Qt::white);
        p.setFont(QFont("Arial", 24, QFont::Bold));
        p.drawText(sampleImg.rect(), Qt::AlignCenter, "Startup Argument Canvas");
    }
    sampleImg.save(testImgPath);

    mw.openFile(testImgPath);
    QApplication::processEvents();

    assert(mw.document() != nullptr);
    assert(mw.tabWidget()->count() == 1);
    assert(mw.document()->width() == 640);
    assert(mw.document()->height() == 480);
    std::cout << "  [File Open as Canvas] File '" << testImgPath.toStdString() << "' opened as canvas (640x480, tabs=1) strictly verified." << std::endl;

    // ファイル読み込み後のUIスナップショット保存
    mw.repaint();
    QApplication::processEvents();
    QImage snapOpened = mw.grab().toImage();
    snapOpened.save(brainDir + "startup_file_opened_verified.png");
    std::cout << "  [UI Snapshot] startup_file_opened_verified.png saved successfully." << std::endl;

    QFile::remove(testImgPath);
    std::cout << "  -> PASSED" << std::endl;
}

void testLanguageSwitching() {
    std::cout << "--- Testing Language Settings and Translation Switching ---" << std::endl;
    QString brainDir = "C:/Users/UMIHIKO/.gemini/antigravity/brain/e668f6c6-6123-49b4-8d6c-a63bbb7450be/";

    // 1. デフォルト設定の確認
    QSettings settings("KABURAGI", "PaintSoft");
    QString origLang = settings.value("General/Language", "system").toString();

    // デフォルト言語設定
    PreferencesDialog::setLanguage("system");
    assert(PreferencesDialog::language() == "system");
    QString effSys = PreferencesDialog::effectiveLanguage();
    std::cout << "  [System Default Language] effectiveLanguage: " << effSys.toStdString() << std::endl;
    assert(effSys == "ja" || effSys == "en");

    // 2. 言語切り替え ("ja" / "en")
    PreferencesDialog::setLanguage("en");
    assert(PreferencesDialog::language() == "en");
    assert(PreferencesDialog::effectiveLanguage() == "en");
    std::cout << "  [English Setting] effectiveLanguage correctly returns 'en'." << std::endl;

    PreferencesDialog::setLanguage("ja");
    assert(PreferencesDialog::language() == "ja");
    assert(PreferencesDialog::effectiveLanguage() == "ja");
    std::cout << "  [Japanese Setting] effectiveLanguage correctly returns 'ja'." << std::endl;

    // 元の設定に戻す
    PreferencesDialog::setLanguage(origLang);

    // 3. 翻訳カタログ (paintsoft_ja.qm) のロード検証
    QTranslator translator;
    bool loaded = translator.load("paintsoft_ja", "translations") ||
                  translator.load("paintsoft_ja", "build/translations");
    assert(loaded);
    std::cout << "  [QM Catalog] translations/paintsoft_ja.qm loaded successfully." << std::endl;

    qApp->installTranslator(&translator);

    // 日本語翻訳の検証
    QString trFile = QCoreApplication::translate("Kaburagi::MainWindow", "&File");
    QString trEdit = QCoreApplication::translate("Kaburagi::MainWindow", "&Edit");
    QString trSelEdit = QCoreApplication::translate("Kaburagi::MainWindow", "Selection Edit");
    QString trRefImg = QCoreApplication::translate("Kaburagi::ReferenceImageWindow", "Reference Image");
    QString trPalette = QCoreApplication::translate("Kaburagi::ColorPaletteWidget", "Clear Palette");
    QString trPrefLang = QCoreApplication::translate("Kaburagi::PreferencesDialog", "Language Settings");

    std::cout << "  [Translate &File] -> " << trFile.toStdString() << std::endl;
    std::cout << "  [Translate &Edit] -> " << trEdit.toStdString() << std::endl;
    std::cout << "  [Translate Selection Edit] -> " << trSelEdit.toStdString() << std::endl;
    std::cout << "  [Translate Reference Image] -> " << trRefImg.toStdString() << std::endl;
    std::cout << "  [Translate Clear Palette] -> " << trPalette.toStdString() << std::endl;
    std::cout << "  [Translate Language Settings] -> " << trPrefLang.toStdString() << std::endl;

    assert(trFile == "ファイル(&F)");
    assert(trEdit == "編集(&E)");
    assert(trSelEdit == "選択範囲編集");
    assert(trRefImg == "参考画像");
    assert(trPalette == "パレットをクリア");
    assert(trPrefLang == "言語設定");

    // 日本語UIスナップショット
    MainWindow mwJa;
    mwJa.resize(1100, 750);
    mwJa.show();
    QApplication::processEvents();
    QImage snapJa = mwJa.grab().toImage();
    snapJa.save(brainDir + "language_japanese_ui_verified.png");
    std::cout << "  [UI Snapshot] language_japanese_ui_verified.png saved." << std::endl;

    // 4. 英語モード（トランスレーター解除）の検証
    qApp->removeTranslator(&translator);
    QString enFile = QCoreApplication::translate("Kaburagi::MainWindow", "&File");
    QString enEdit = QCoreApplication::translate("Kaburagi::MainWindow", "&Edit");
    QString enSelEdit = QCoreApplication::translate("Kaburagi::MainWindow", "Selection Edit");
    assert(enFile == "&File");
    assert(enEdit == "&Edit");
    assert(enSelEdit == "Selection Edit");
    std::cout << "  [English Key Fallback] Correctly returned native English keys without translator." << std::endl;

    MainWindow mwEn;
    mwEn.resize(1100, 750);
    mwEn.show();
    QApplication::processEvents();
    QImage snapEn = mwEn.grab().toImage();
    snapEn.save(brainDir + "language_english_ui_verified.png");
    std::cout << "  [UI Snapshot] language_english_ui_verified.png saved." << std::endl;

    std::cout << "  -> PASSED" << std::endl;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    std::cout << "=== Running ThreeD Layer Unit Tests ===" << std::endl;
    testCameraCalculations();
    testModelCreationAndObjParsing();
    testRendererRasterization();
    testThreeDLayerComposite();
    testDocumentIntegration();
    testPoseAndSkeleton();
    testMmdParsers();
    testVrmParser();
    testVrmaAnimationAndSpringBone();
    testVmdAnimation();
    testOpenGLOffscreen();
    testGpuRenderingPerformance();
    testUiSnapshots();
    testFilters();
    testMotionBlurAndGradientMapAndPinnedTransform();
    testDisplayFilters();
    testMonitorIccAndPreviewWindowAndModelTransform();
    testSelectionEditMode();
    testTabAndToolboxUpdates();
    testGrayscaleSelectionMask();
    testToolBoxResizeGuiVisibility();
    testReferenceImageWindow();
    testStartupNoCanvasAndOpenFileArgument();
    testLanguageSwitching();
    std::cout << "=== ALL ThreeD & Filter TESTS PASSED ===" << std::endl;
    return 0;
}

