#include <QApplication>
#include <cassert>
#include <iostream>
#include <QPainter>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDir>
#include <QFile>
#include "AiService.hpp"
#include "AiAssistantDockWidget.hpp"
#include "AiComponentDownloader.hpp"
#include "LlmPromptService.hpp"
#include "PromptHistoryDialog.hpp"
#include "ThreeDLayer.hpp"
#include "ThreeDModel.hpp"
#include "Document.hpp"
#include "Layer.hpp"
#include "DocumentSerializer.hpp"
#include "MainWindow.hpp"

using namespace Kaburagi;

void testBase64Conversion() {
    std::cout << "[TEST] 1. Base64 Conversion..." << std::endl;

    QImage src(32, 32, QImage::Format_ARGB32_Premultiplied);
    src.fill(QColor(255, 0, 128, 200));

    QString b64 = AiService::imageToBase64Png(src);
    assert(!b64.isEmpty());

    QImage restored = AiService::base64ToImage(b64);
    assert(!restored.isNull());
    assert(restored.size() == src.size());
    assert(restored.pixelColor(10, 10) == src.pixelColor(10, 10));

    std::cout << "  -> PASSED!" << std::endl;
}

void testWebUiJsonBuilding() {
    std::cout << "[TEST] 2. WebUI JSON Request Building..." << std::endl;

    AiGenerateParams params;
    params.prompt = "masterpiece, 1girl";
    params.negativePrompt = "worst quality";
    params.steps = 28;
    params.cfgScale = 8.5f;
    params.seed = 12345678;
    params.width = 768;
    params.height = 512;
    params.mode = AiMode::SeamlessTexture; // tiling should be true

    QJsonObject txtJson = AiService::buildWebUiTxt2ImgJson(params);
    assert(txtJson["prompt"].toString() == "masterpiece, 1girl");
    assert(txtJson["negative_prompt"].toString() == "worst quality");
    assert(txtJson["steps"].toInt() == 28);
    assert(txtJson["seed"].toInteger() == 12345678);
    assert(txtJson["width"].toInt() == 768);
    assert(txtJson["height"].toInt() == 512);
    assert(txtJson["tiling"].toBool() == true);

    // img2img with Inpaint & ControlNet
    params.mode = AiMode::Inpaint;
    params.denoisingStrength = 0.75f;
    QImage dummyImg(64, 64, QImage::Format_ARGB32_Premultiplied);
    dummyImg.fill(Qt::blue);
    QImage dummyMask(64, 64, QImage::Format_ARGB32_Premultiplied);
    dummyMask.fill(Qt::white);
    params.initImage = dummyImg;
    params.maskImage = dummyMask;

    params.enableControlNet = true;
    params.controlNetType = ControlNetType::Lineart;
    params.controlNetWeight = 1.2f;
    params.controlNetImage = dummyImg;

    QJsonObject imgJson = AiService::buildWebUiImg2ImgJson(params);
    assert(imgJson.contains("init_images"));
    assert(imgJson.contains("mask"));
    assert(imgJson["denoising_strength"].toDouble() == 0.75);
    assert(imgJson["inpainting_fill"].toInt() == 1);

    assert(imgJson.contains("alwayson_scripts"));
    QJsonObject alwaysOn = imgJson["alwayson_scripts"].toObject();
    assert(alwaysOn.contains("controlnet"));
    QJsonArray argsArr = alwaysOn["controlnet"].toObject()["args"].toArray();
    assert(!argsArr.isEmpty());
    QJsonObject cnUnit = argsArr[0].toObject();
    assert(cnUnit["enabled"].toBool() == true);
    assert(cnUnit["weight"].toDouble() == 1.2);
    assert(cnUnit["model"].toString().contains("lineart"));

    // Test ReferenceOnly ControlNet
    params.controlNetType = ControlNetType::ReferenceOnly;
    params.enableIpAdapter = true;
    params.ipAdapterType = "ip-adapter_plus_sd15";
    params.ipAdapterWeight = 0.85f;
    params.ipAdapterImage = dummyImg;

    QJsonObject refIpJson = AiService::buildWebUiImg2ImgJson(params);
    QJsonArray refIpArgs = refIpJson["alwayson_scripts"].toObject()["controlnet"].toObject()["args"].toArray();
    assert(refIpArgs.size() == 2);
    // Unit 0: reference_only
    QJsonObject unit0 = refIpArgs[0].toObject();
    assert(unit0["module"].toString() == "reference_only");
    assert(unit0["model"].toString() == "None");
    // Unit 1: ip-adapter
    QJsonObject unit1 = refIpArgs[1].toObject();
    assert(unit1["module"].toString() == "ip-adapter_plus_sd15");
    assert(unit1["model"].toString() == "ip-adapter-plus_sd15");
    assert(unit1["weight"].toDouble() == 0.85);

    // Test serialization / deserialization of ReferenceOnly and IP-Adapter
    QJsonObject serialized = AiService::paramsToJson(params);
    AiGenerateParams restoredParams = AiService::jsonToParams(serialized);
    assert(restoredParams.enableControlNet == true);
    assert(restoredParams.controlNetType == ControlNetType::ReferenceOnly);
    assert(restoredParams.enableIpAdapter == true);
    assert(restoredParams.ipAdapterType == "ip-adapter_plus_sd15");
    assert(std::abs(restoredParams.ipAdapterWeight - 0.85f) < 0.001f);

    std::cout << "  -> PASSED!" << std::endl;
}

void testComfyUiJsonBuilding() {
    std::cout << "[TEST] 2b. ComfyUI Workflow JSON Building..." << std::endl;

    AiGenerateParams params;
    params.backend = ApiBackend::LocalComfyUI;
    params.prompt = "masterpiece, 1girl, red ribbons";
    params.negativePrompt = "low quality";
    params.modelName = "animagineXL40_v10.safetensors";
    params.steps = 28;
    params.cfgScale = 6.5f;
    params.seed = 987654321;
    params.width = 832;
    params.height = 1216;

    QJsonObject root = AiService::buildComfyUiPromptJson(params);
    assert(root.contains("prompt"));
    assert(root.contains("client_id"));
    QJsonObject prompt = root["prompt"].toObject();

    // Check Node 4 (CheckpointLoaderSimple)
    assert(prompt.contains("4"));
    QJsonObject n4 = prompt["4"].toObject();
    assert(n4["class_type"].toString() == "CheckpointLoaderSimple");
    assert(n4["inputs"].toObject()["ckpt_name"].toString() == "animagineXL40_v10.safetensors");

    // Check Node 6 (Positive Prompt)
    assert(prompt.contains("6"));
    QJsonObject n6 = prompt["6"].toObject();
    assert(n6["inputs"].toObject()["text"].toString() == "masterpiece, 1girl, red ribbons");

    // Check Node 3 (KSampler)
    assert(prompt.contains("3"));
    QJsonObject n3 = prompt["3"].toObject();
    assert(n3["inputs"].toObject()["steps"].toInt() == 28);
    assert(n3["inputs"].toObject()["seed"].toInteger() == 987654321);

    // Check Node 5 (EmptyLatentImage)
    assert(prompt.contains("5"));
    QJsonObject n5 = prompt["5"].toObject();
    assert(n5["inputs"].toObject()["width"].toInt() == 832);
    assert(n5["inputs"].toObject()["height"].toInt() == 1216);

    std::cout << "  -> PASSED!" << std::endl;
}

void testResponseParsing() {
    std::cout << "[TEST] 3. Response JSON Parsing..." << std::endl;

    QImage img1(16, 16, QImage::Format_ARGB32_Premultiplied);
    img1.fill(Qt::red);
    QImage img2(16, 16, QImage::Format_ARGB32_Premultiplied);
    img2.fill(Qt::green);

    QString b64_1 = AiService::imageToBase64Png(img1);
    QString b64_2 = AiService::imageToBase64Png(img2);

    // 1. WebUI response mock
    QJsonObject webUiObj;
    QJsonArray imgArr;
    imgArr.append(b64_1);
    imgArr.append(b64_2);
    webUiObj["images"] = imgArr;
    QByteArray webUiBytes = QJsonDocument(webUiObj).toJson();

    QString err;
    QList<QImage> results = AiService::parseWebUiResponse(webUiBytes, &err);
    assert(err.isEmpty());
    assert(results.size() == 2);
    assert(results[0].size() == QSize(16, 16));
    assert(results[1].size() == QSize(16, 16));

    // 2. OpenAI response mock
    QJsonObject openAiObj;
    QJsonArray dataArr;
    QJsonObject d1, d2;
    d1["b64_json"] = b64_1;
    d2["b64_json"] = b64_2;
    dataArr.append(d1);
    dataArr.append(d2);
    openAiObj["data"] = dataArr;
    QByteArray openAiBytes = QJsonDocument(openAiObj).toJson();

    results = AiService::parseOpenAiResponse(openAiBytes, &err);
    assert(err.isEmpty());
    assert(results.size() == 2);

    std::cout << "  -> PASSED!" << std::endl;
}

void testAiAssistantDockWidget() {
    std::cout << "[TEST] 4. AiAssistantDockWidget UI & Image Sources..." << std::endl;

    auto doc = new Document(400, 300, 72.0);
    AiAssistantDockWidget dock;
    dock.setDocument(doc);

    // Initial state check
    assert(dock.aiService() != nullptr);

    // 1. Canvas source
    QString err;
    QImage canvasImg = dock.getImageFromSource(static_cast<int>(AiImageSource::Canvas), "", &err);
    assert(err.isEmpty());
    assert(canvasImg.size() == QSize(400, 300));

    // 2. Active Layer source
    QImage layerImg = dock.getImageFromSource(static_cast<int>(AiImageSource::ActiveLayer), "", &err);
    assert(err.isEmpty());
    assert(!layerImg.isNull());

    // 3. File source
    QString tempFilePath = QDir::temp().filePath("kaburagi_test_cn_file.png");
    QImage dummyFileImg(128, 128, QImage::Format_ARGB32_Premultiplied);
    dummyFileImg.fill(Qt::yellow);
    dummyFileImg.save(tempFilePath, "PNG");

    QImage loadedFileImg = dock.getImageFromSource(static_cast<int>(AiImageSource::File), tempFilePath, &err);
    assert(err.isEmpty());
    assert(loadedFileImg.size() == QSize(128, 128));
    QFile::remove(tempFilePath);

    // 4. Reset to defaults
    dock.resetToDefaults();

    delete doc;
    std::cout << "  -> PASSED!" << std::endl;
}

void testCanvasIntegration() {
    std::cout << "[TEST] 5. Canvas & Document Integration (New Layer & Overwrite)..." << std::endl;

    MainWindow win;
    auto doc = new Document(400, 300, 72.0);
    win.setDocumentForTest(doc);
    assert(doc != nullptr);
    assert(win.aiAssistantDock() != nullptr);

    // 1. New Layer insertion
    QImage testImg(200, 150, QImage::Format_ARGB32_Premultiplied);
    testImg.fill(QColor(100, 150, 200));

    size_t beforeCount = doc->layers().size();
    win.onAiApplyAsNewLayer(testImg);
    assert(doc->layers().size() == beforeCount + 1);
    assert(doc->activeLayer()->name() == "AI生成レイヤー" || doc->activeLayer()->name() == "AI Generated Layer");

    // 2. Overwrite Selection
    // Set rectangular selection
    QPainterPath selPath;
    selPath.addRect(50, 50, 100, 100);
    doc->setSelection(selPath);
    assert(doc->hasSelection());

    QImage overwriteImg(100, 100, QImage::Format_ARGB32_Premultiplied);
    overwriteImg.fill(QColor(255, 0, 0));

    win.onAiApplyOverwriteSelection(overwriteImg);

    // Check pixel at (60, 60) inside selection
    uint32_t pxIn = doc->activeLayer()->tileManager().getPixel(60, 60);
    QColor cIn(pxIn);
    assert(cIn.red() == 255);

    // Undo check
    assert(doc->undoStack()->canUndo());
    doc->undoStack()->undo();
    uint32_t pxUndone = doc->activeLayer()->tileManager().getPixel(60, 60);
    assert(pxUndone != pxIn);

    // 2b. Full-canvas sized Inpaint overwrite test:
    // Canvas is 400x300, selection is (50, 50, 100, 100).
    // An inpaint image of 400x300 has green pixel at (70, 70) (inside), and green pixel at (200, 200) (outside).
    QImage fullCanvasInpaintImg(400, 300, QImage::Format_ARGB32_Premultiplied);
    fullCanvasInpaintImg.fill(Qt::black);
    fullCanvasInpaintImg.setPixelColor(70, 70, Qt::green);
    fullCanvasInpaintImg.setPixelColor(200, 200, Qt::green);

    win.onAiApplyOverwriteSelection(fullCanvasInpaintImg);

    // Pixel at (70, 70) inside selection must be green
    uint32_t pxInSelection = doc->activeLayer()->tileManager().getPixel(70, 70);
    assert(QColor(pxInSelection) == Qt::green);

    // Pixel at (200, 200) outside selection must NOT be green
    uint32_t pxOutSelection = doc->activeLayer()->tileManager().getPixel(200, 200);
    assert(QColor(pxOutSelection) != Qt::green);

    // 3. RoughToClean mode: 2nd generation layer fallback & white background test
    auto roughLayer = doc->addRasterLayer("ラフ");
    // Draw black pixel on transparent rough layer
    roughLayer->tileManager().setPixel(10, 10, 0xFF000000); // opaque black
    doc->setActiveLayerIndex(doc->layers().size() - 1);

    auto dock = win.aiAssistantDock();
    dock->setDocument(doc);
    // Switch to RoughToClean mode
    dock->setMode(AiMode::RoughToClean);

    // 1st run image: should be white background with black pixel at (10, 10)
    QImage init1 = dock->getInitImageForCurrentMode();
    assert(!init1.isNull());
    assert(init1.pixelColor(0, 0) == Qt::white); // Transparent converted to white!
    assert(init1.pixelColor(10, 10) == Qt::black);

    // Now insert AI layer as new layer (active layer becomes the AI layer)
    QImage dummyAiImg(400, 300, QImage::Format_RGB32);
    dummyAiImg.fill(Qt::blue);
    QJsonObject dummyAiParams; dummyAiParams["prompt"] = "test";
    win.onAiApplyAsNewLayer(dummyAiImg, dummyAiParams);
    assert(doc->activeLayer()->hasAiParameters());

    // 2nd run: Even though active layer is the AI layer, getInitImageForCurrentMode and Canvas source should bypass it and get roughLayer!
    QImage init2 = dock->getInitImageForCurrentMode();
    assert(!init2.isNull());
    assert(init2.pixelColor(0, 0) == Qt::white);
    assert(init2.pixelColor(10, 10) == Qt::black);

    QString err2;
    QImage cnImg2 = dock->getImageFromSource(static_cast<int>(AiImageSource::ActiveLayer), "", &err2);
    assert(!cnImg2.isNull());
    assert(cnImg2.pixelColor(0, 0) == Qt::white);
    assert(cnImg2.pixelColor(10, 10) == Qt::black);

    std::cout << "  -> PASSED!" << std::endl;
}

#include "PreferencesDialog.hpp"
#include <QTemporaryDir>
#include <QFile>

void testPreferencesAiDirectories() {
    std::cout << "[TEST] 6. Preferences AI Model & ControlNet Directories..." << std::endl;

    QTemporaryDir tempModelDir;
    if (!tempModelDir.isValid()) {
        std::cerr << "tempModelDir invalid" << std::endl;
        std::exit(1);
    }
    QString modelFilePath = QDir(tempModelDir.path()).filePath("anime_final_v1.safetensors");
    {
        QFile dummyModel(modelFilePath);
        if (!dummyModel.open(QIODevice::WriteOnly)) {
            std::cerr << "failed to create dummyModel" << std::endl;
            std::exit(1);
        }
        dummyModel.write("dummy-weights");
        dummyModel.close();
    }

    // サブフォルダ (ComfyUI / WebUI 構造) のテスト
    QDir(tempModelDir.path()).mkdir("StableDiffusion");
    QString nestedModelPath = QDir(tempModelDir.path()).filePath("StableDiffusion/animagineXL40.safetensors");
    {
        QFile nestedModel(nestedModelPath);
        if (!nestedModel.open(QIODevice::WriteOnly)) {
            std::cerr << "failed to create nestedModel" << std::endl;
            std::exit(1);
        }
        nestedModel.write("dummy-nested-weights");
        nestedModel.close();
    }

    QTemporaryDir tempCnDir;
    if (!tempCnDir.isValid()) {
        std::cerr << "tempCnDir invalid" << std::endl;
        std::exit(1);
    }
    QString cnFilePath = QDir(tempCnDir.path()).filePath("control_v11p_sd15_canny.pth");
    {
        QFile dummyCn(cnFilePath);
        if (!dummyCn.open(QIODevice::WriteOnly)) {
            std::cerr << "failed to create dummyCn" << std::endl;
            std::exit(1);
        }
        dummyCn.write("dummy-cn-weights");
        dummyCn.close();
    }

    // 元のユーザー設定を退避してテスト後に復元
    QString origModelDir = PreferencesDialog::aiModelDirectory();
    QString origCnDir = PreferencesDialog::aiControlNetDirectory();

    struct SettingsRestorer {
        QString m_dir, m_cn;
        ~SettingsRestorer() {
            QSettings s(QSettings::IniFormat, QSettings::UserScope, "KaburagiPaint", "PaintSoftKABURAGI");
            s.beginGroup("AiLocal");
            s.setValue("modelDirectory", m_dir);
            s.setValue("controlNetDirectory", m_cn);
            s.endGroup();
        }
    } restorer{origModelDir, origCnDir};

    // QSettingsに設定してPreferencesDialogで読み込み・保存の整合性を検証
    {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, "KaburagiPaint", "PaintSoftKABURAGI");
        settings.beginGroup("AiLocal");
        settings.setValue("modelDirectory", tempModelDir.path());
        settings.setValue("controlNetDirectory", tempCnDir.path());
        settings.endGroup();
    }

    if (PreferencesDialog::aiModelDirectory() != tempModelDir.path()) {
        std::cerr << "PreferencesDialog::aiModelDirectory mismatch: "
                  << PreferencesDialog::aiModelDirectory().toStdString() << " vs "
                  << tempModelDir.path().toStdString() << std::endl;
        std::exit(1);
    }
    if (PreferencesDialog::aiControlNetDirectory() != tempCnDir.path()) {
        std::cerr << "PreferencesDialog::aiControlNetDirectory mismatch" << std::endl;
        std::exit(1);
    }

    // scanLocalModels 直接テスト
    AiAssistantDockWidget dock;
    QStringList models = dock.scanLocalModels();
    if (!models.contains("anime_final_v1.safetensors")) {
        std::cerr << "models does not contain anime_final_v1.safetensors, actual: "
                  << models.join(", ").toStdString() << std::endl;
        std::exit(1);
    }
    if (!models.contains("StableDiffusion/animagineXL40.safetensors")) {
        std::cerr << "models does not contain StableDiffusion/animagineXL40.safetensors, actual: "
                  << models.join(", ").toStdString() << std::endl;
        std::exit(1);
    }

    QStringList cns = dock.scanLocalControlNetModels();
    if (!cns.contains("control_v11p_sd15_canny.pth")) {
        std::cerr << "cns does not contain control_v11p_sd15_canny.pth, actual: "
                  << cns.join(", ").toStdString() << std::endl;
        std::exit(1);
    }

    std::cout << "  -> PASSED!" << std::endl;

    // 復元された実環境のモデルスキャン確認
    if (!origModelDir.isEmpty()) {
        std::cout << "  [INFO] Real Model Dir: " << origModelDir.toStdString() << std::endl;
        QSettings s(QSettings::IniFormat, QSettings::UserScope, "KaburagiPaint", "PaintSoftKABURAGI");
        s.beginGroup("AiLocal");
        s.setValue("modelDirectory", origModelDir);
        s.setValue("controlNetDirectory", origCnDir);
        s.endGroup();
        QStringList realModels = dock.scanLocalModels();
        std::cout << "  [INFO] Real Models detected: " << realModels.size() << std::endl;
        for (const auto& rm : realModels) {
            std::cout << "    - " << rm.toStdString() << std::endl;
        }
    }
}

void testAiComponentDownloader() {
    std::cout << "[TEST] 7. AiComponentDownloader Component Info & Path Resolution..." << std::endl;

    auto infoSd = AiComponentDownloader::getComponentInfo(ComponentType::EmbeddedSdExe);
    assert(infoSd.type == ComponentType::EmbeddedSdExe);
    assert(infoSd.fileName == "sd.exe");
    assert(!infoSd.downloadUrl.isEmpty());
    assert(infoSd.estimatedSizeBytes > 0);

    auto infoAnime = AiComponentDownloader::getComponentInfo(ComponentType::UpscaleModelAnime6B);
    assert(infoAnime.fileName == "RealESRGAN_x4plus_anime_6B.pth");
    assert(!infoAnime.downloadUrl.isEmpty());

    auto infoCn = AiComponentDownloader::getComponentInfo(ComponentType::ControlNetLineart);
    assert(infoCn.fileName == "control_v11p_sd15_lineart.pth");
    assert(infoCn.downloadUrl.contains("lineart"));
    assert(infoCn.estimatedSizeBytes > 0);

    // Test all ControlNet component mappings and metadata
    assert(AiComponentDownloader::fromControlNetType(ControlNetType::Lineart) == ComponentType::ControlNetLineart);
    assert(AiComponentDownloader::fromControlNetType(ControlNetType::Depth) == ComponentType::ControlNetDepth);
    assert(AiComponentDownloader::fromControlNetType(ControlNetType::Openpose) == ComponentType::ControlNetOpenpose);
    assert(AiComponentDownloader::fromControlNetType(ControlNetType::Scribble) == ComponentType::ControlNetScribble);
    assert(AiComponentDownloader::fromControlNetType(ControlNetType::Canny) == ComponentType::ControlNetCanny);

    assert(AiComponentDownloader::hasComponentForControlNetType(ControlNetType::Lineart));
    assert(AiComponentDownloader::hasComponentForControlNetType(ControlNetType::Depth));
    assert(AiComponentDownloader::hasComponentForControlNetType(ControlNetType::Openpose));
    assert(AiComponentDownloader::hasComponentForControlNetType(ControlNetType::Scribble));
    assert(AiComponentDownloader::hasComponentForControlNetType(ControlNetType::Canny));
    assert(!AiComponentDownloader::hasComponentForControlNetType(ControlNetType::ReferenceOnly));

    auto infoDepth = AiComponentDownloader::getComponentInfo(ComponentType::ControlNetDepth);
    assert(infoDepth.fileName == "control_v11f1p_sd15_depth.pth");
    assert(infoDepth.downloadUrl.contains("depth"));

    auto infoOpenpose = AiComponentDownloader::getComponentInfo(ComponentType::ControlNetOpenpose);
    assert(infoOpenpose.fileName == "control_v11p_sd15_openpose.pth");
    assert(infoOpenpose.downloadUrl.contains("openpose"));

    auto infoScribble = AiComponentDownloader::getComponentInfo(ComponentType::ControlNetScribble);
    assert(infoScribble.fileName == "control_v11p_sd15_scribble.pth");
    assert(infoScribble.downloadUrl.contains("scribble"));

    auto infoCanny = AiComponentDownloader::getComponentInfo(ComponentType::ControlNetCanny);
    assert(infoCanny.fileName == "control_v11p_sd15_canny.pth");
    assert(infoCanny.downloadUrl.contains("canny"));

    QString pathSd = AiComponentDownloader::getLocalFilePath(ComponentType::EmbeddedSdExe);
    assert(pathSd.endsWith("sd.exe"));

    std::cout << "  -> PASSED!" << std::endl;
}

void testLlmPromptService() {
    std::cout << "[TEST] 8. LlmPromptService Request Building & Response Parsing..." << std::endl;

    // 1. Check Chat Request JSON building
    LlmConfig cfg;
    cfg.backend = LlmBackend::Ollama;
    cfg.modelName = "llama3:latest";
    cfg.serverUrl = "http://127.0.0.1:11434";

    QJsonObject req = LlmPromptService::buildChatRequestJson(cfg, "夕暮れの海辺の少女", "アニメ・美麗イラスト調 (Anime / Masterpiece)", "1girl");
    assert(req["model"].toString() == "llama3:latest");
    assert(req.contains("messages"));
    QJsonArray msgs = req["messages"].toArray();
    assert(msgs.size() == 2);
    assert(msgs[0].toObject()["role"].toString() == "system");
    assert(msgs[1].toObject()["role"].toString() == "user");
    assert(msgs[1].toObject()["content"].toString().contains("夕暮れの海辺の少女"));

    // 2. Check OpenAI-format response parsing
    QJsonObject choice;
    choice["message"] = QJsonObject{
        {"role", "assistant"},
        {"content", "{\n  \"prompt\": \"1girl, solo, beach, sunset, ocean, school uniform, masterpiece, highly detailed\",\n  \"negative_prompt\": \"worst quality, low quality, bad anatomy\"\n}"}
    };
    QJsonObject openAiResp;
    openAiResp["choices"] = QJsonArray{choice};
    QByteArray openAiData = QJsonDocument(openAiResp).toJson();

    QString err;
    LlmPromptResult res = LlmPromptService::parseChatResponseJson(openAiData, &err);
    assert(err.isEmpty());
    assert(res.prompt.contains("1girl"));
    assert(res.prompt.contains("beach"));
    assert(res.negativePrompt.contains("bad anatomy"));

    // 3. Check Markdown code block stripping
    QJsonObject choiceMd;
    choiceMd["message"] = QJsonObject{
        {"role", "assistant"},
        {"content", "```json\n{\n  \"prompt\": \"cyberpunk city, neon lights\",\n  \"negative_prompt\": \"low quality\"\n}\n```"}
    };
    QJsonObject mdResp;
    mdResp["choices"] = QJsonArray{choiceMd};
    LlmPromptResult resMd = LlmPromptService::parseChatResponseJson(QJsonDocument(mdResp).toJson(), &err);
    assert(err.isEmpty());
    assert(resMd.prompt.contains("cyberpunk"));

    // 4. Check Ollama models response parsing
    QJsonObject ollamaModel1{{"name", "llama3:latest"}};
    QJsonObject ollamaModel2{{"name", "qwen2.5:latest"}};
    QJsonObject ollamaTagsResp{{"models", QJsonArray{ollamaModel1, ollamaModel2}}};
    QStringList ollamaModels = LlmPromptService::parseModelsResponse(LlmBackend::Ollama, QJsonDocument(ollamaTagsResp).toJson());
    assert(ollamaModels.size() == 2);
    assert(ollamaModels.contains("llama3:latest"));
    assert(ollamaModels.contains("qwen2.5:latest"));

    std::cout << "  -> PASSED!" << std::endl;
}

static void testSamplerAndScheduler() {
    std::cout << "[TEST] 10. Sampler & Scheduler Mapping & Serialization..." << std::endl;

    AiGenerateParams params;
    params.sampler = "DPM++ 2M SDE";
    params.scheduler = "karras";
    params.removeBackground = true;

    // 1. Serialization
    QJsonObject json = AiService::paramsToJson(params);
    assert(json["sampler"].toString() == "DPM++ 2M SDE");
    assert(json["scheduler"].toString() == "karras");
    assert(json["removeBackground"].toBool() == true);

    AiGenerateParams restored = AiService::jsonToParams(json);
    assert(restored.sampler == "DPM++ 2M SDE");
    assert(restored.scheduler == "karras");
    assert(restored.removeBackground == true);

    // 2. ComfyUI Mapping
    QJsonObject comfy = AiService::buildComfyUiPromptJson(params);
    assert(comfy.contains("3"));
    QJsonObject node3Inputs = comfy["3"].toObject()["inputs"].toObject();
    assert(node3Inputs["sampler_name"].toString() == "dpmpp_2m_sde");
    assert(node3Inputs["scheduler"].toString() == "karras");

    // 3. WebUI JSON
    QJsonObject webUi = AiService::buildWebUiTxt2ImgJson(params);
    assert(webUi["sampler_name"].toString() == "DPM++ 2M SDE");
    assert(webUi["scheduler"].toString() == "karras");

    std::cout << "  -> PASSED!" << std::endl;
}

static void testRemoveBackground() {
    std::cout << "[TEST] 11. Remove Background (Transparency & Alpha Edge)..." << std::endl;

    // 100x100 の白背景画像を作成
    QImage src(100, 100, QImage::Format_ARGB32);
    src.fill(QColor(255, 255, 255, 255));

    // 中央に赤い正方形 (30,30)-(70,70) を描画
    for (int y = 30; y < 70; ++y) {
        for (int x = 30; x < 70; ++x) {
            src.setPixelColor(x, y, QColor(255, 0, 0, 255));
        }
    }

    QImage transparent = AiService::removeBackground(src, 25);
    assert(!transparent.isNull());
    assert(transparent.size() == src.size());

    // 4隅は完全に透明 (alpha == 0)
    assert(qAlpha(transparent.pixel(0, 0)) == 0);
    assert(qAlpha(transparent.pixel(99, 0)) == 0);
    assert(qAlpha(transparent.pixel(0, 99)) == 0);
    assert(qAlpha(transparent.pixel(99, 99)) == 0);

    // 中央の赤部分は不透明 (alpha == 255) かつ 赤色
    QRgb centerPixel = transparent.pixel(50, 50);
    assert(qAlpha(centerPixel) == 255);
    assert(qRed(centerPixel) == 255);
    assert(qGreen(centerPixel) == 0);
    assert(qBlue(centerPixel) == 0);

    std::cout << "  -> PASSED!" << std::endl;
}

static void testUpscaleModelDirectoryPreference() {
    std::cout << "[TEST] 12. Upscale Model Directory Preference & Downloader..." << std::endl;

    QString origUpscaleDir = PreferencesDialog::upscaleModelDirectory();
    if (origUpscaleDir.contains("Temp") || origUpscaleDir.isEmpty() || !QDir(origUpscaleDir).exists()) {
        if (QDir("E:/ComfyUI/Data/Models/Upscaler").exists()) {
            origUpscaleDir = "E:/ComfyUI/Data/Models/Upscaler";
        }
    }

    QTemporaryDir tempUpscaleDir;
    assert(tempUpscaleDir.isValid());

    PreferencesDialog::setUpscaleModelDirectory(tempUpscaleDir.path());
    assert(PreferencesDialog::upscaleModelDirectory() == tempUpscaleDir.path());

    auto infoAnime6B = AiComponentDownloader::getComponentInfo(ComponentType::UpscaleModelAnime6B);
    assert(infoAnime6B.targetDirectory == tempUpscaleDir.path());

    auto infoRealEsrgan = AiComponentDownloader::getComponentInfo(ComponentType::UpscaleModelRealEsrgan);
    assert(infoRealEsrgan.targetDirectory == tempUpscaleDir.path());

    // 元の設定を確実に復元
    PreferencesDialog::setUpscaleModelDirectory(origUpscaleDir);

    std::cout << "  -> PASSED!" << std::endl;
}

static void testAiSettingsPersistenceAndThumbnailStyle() {
    std::cout << "[TEST] 13. AI Settings Persistence (Backend, Model, Geometry) & Thumbnail Red Border..." << std::endl;

    // 既存設定の退避
    QSettings origSettings("KABURAGI", "PaintSoft KABURAGI");
    origSettings.beginGroup("AiAssistant");
    QMap<QString, QVariant> backup;
    for (const QString& key : origSettings.childKeys()) {
        backup[key] = origSettings.value(key);
    }
    origSettings.endGroup();

    QString testModelName = "v1-5-pruned-emaonly-fp16.safetensors";

    // 1. 設定保存
    {
        AiAssistantDockWidget dock1;
        dock1.setFloating(true);
        dock1.setGeometry(150, 200, 480, 680);

        // ラジオボタンを組み込み(Embedded)に切り替え
        const auto radios = dock1.findChildren<QRadioButton*>();
        for (auto* r : radios) {
            if (r->text().contains("組み込み")) {
                r->setChecked(true);
                break;
            }
        }

        // 実在するモデル名があればそれを使用
        QStringList realModels = dock1.scanLocalModels();
        if (!realModels.isEmpty()) {
            testModelName = realModels.first();
        }

        // モデル選択コンボボックス
        QComboBox* comboModel = nullptr;
        const auto combos = dock1.findChildren<QComboBox*>();
        for (auto* c : combos) {
            if (c->count() > 0 && c->itemText(0).contains(".safetensors")) {
                comboModel = c;
                break;
            }
        }
        if (!comboModel && !combos.isEmpty()) {
            comboModel = combos[0];
        }
        if (comboModel) {
            if (comboModel->findText(testModelName) == -1) {
                comboModel->addItem(testModelName);
            }
            comboModel->setCurrentText(testModelName);
        }

        dock1.saveSettings();
    }

    // 2. 新規インスタンスでの復元検証
    {
        AiAssistantDockWidget dock2;
        // loadSettings() はコンストラクタ内で自動実行される

        // バックエンドが組み込みになっているか
        const auto radios = dock2.findChildren<QRadioButton*>();
        bool embeddedChecked = false;
        for (auto* r : radios) {
            if (r->text().contains("組み込み") && r->isChecked()) {
                embeddedChecked = true;
                break;
            }
        }
        assert(embeddedChecked);

        // モデルが復元されているか
        QComboBox* comboModel = nullptr;
        const auto combos = dock2.findChildren<QComboBox*>();
        for (auto* c : combos) {
            if (c->currentText().contains(testModelName)) {
                comboModel = c;
                break;
            }
        }
        assert(comboModel != nullptr);
        assert(comboModel->currentText() == testModelName);

        // フローティングとジオメトリが復元されているか
        assert(dock2.isFloating() == true);
        assert(dock2.geometry().width() == 480);
        assert(dock2.geometry().height() == 680);

        // サムネイルリストの赤枠強調表示スタイルシート検証
        QListWidget* listThumb = dock2.findChild<QListWidget*>();
        assert(listThumb != nullptr);
        QString ss = listThumb->styleSheet();
        assert(ss.contains("#d32f2f") || ss.contains("red") || ss.contains("#e53935"));
        assert(ss.contains("border"));
    }

    // 元の設定の復元
    QSettings restoreSettings("KABURAGI", "PaintSoft KABURAGI");
    restoreSettings.remove("AiAssistant");
    restoreSettings.beginGroup("AiAssistant");
    for (auto it = backup.begin(); it != backup.end(); ++it) {
        if (it.key() == "modelName" && it.value().toString() == "custom_saved_model_v2.safetensors") {
            // ダミー値だった場合は実在するモデル名にしてクリーンアップ
            restoreSettings.setValue("modelName", testModelName);
        } else {
            restoreSettings.setValue(it.key(), it.value());
        }
    }
    restoreSettings.endGroup();
    restoreSettings.sync();

    std::cout << "  -> PASSED!" << std::endl;
}

void testPromptHistory() {
    std::cout << "[TEST] 14. Prompt History & Pinning Persistence..." << std::endl;

    // 既存設定の退避
    auto origItems = PromptHistoryDialog::loadHistory();

    // 初期化
    PromptHistoryDialog::saveHistory({});
    assert(PromptHistoryDialog::loadHistory().empty());

    // アイテム追加
    PromptHistoryDialog::addHistoryItem("1girl, cute smile, school uniform", "worst quality, bad anatomy", "笑顔の女子高生");
    PromptHistoryDialog::addHistoryItem("landscape, sunset, mountain, beautiful sky", "lowres, text");

    auto items = PromptHistoryDialog::loadHistory();
    assert(items.size() == 2);
    // 最後に足したものが先頭
    assert(items[0].prompt == "landscape, sunset, mountain, beautiful sky");
    assert(items[1].prompt == "1girl, cute smile, school uniform");
    assert(items[1].llmPrompt == "笑顔の女子高生");
    assert(!items[0].isPinned && !items[1].isPinned);

    // ピン留めテスト (1番目をピン留め)
    items[1].isPinned = true;
    PromptHistoryDialog::saveHistory(items);

    auto itemsAfterPin = PromptHistoryDialog::loadHistory();
    assert(itemsAfterPin.size() == 2);
    // ピン留めされたアイテムが最上位に昇格していること
    assert(itemsAfterPin[0].prompt == "1girl, cute smile, school uniform");
    assert(itemsAfterPin[0].isPinned == true);
    assert(itemsAfterPin[1].prompt == "landscape, sunset, mountain, beautiful sky");
    assert(itemsAfterPin[1].isPinned == false);

    // 重複アイテム追加（同じプロンプト＋ネガティブ）
    PromptHistoryDialog::addHistoryItem("1girl, cute smile, school uniform", "worst quality, bad anatomy", "アップデートされた要望");
    auto itemsAfterDuplicate = PromptHistoryDialog::loadHistory();
    assert(itemsAfterDuplicate.size() == 2); // 重複により増えない
    assert(itemsAfterDuplicate[0].prompt == "1girl, cute smile, school uniform");
    assert(itemsAfterDuplicate[0].isPinned == true); // ピン留めが維持されていること
    assert(itemsAfterDuplicate[0].llmPrompt == "アップデートされた要望");

    // 元の設定を復元
    PromptHistoryDialog::saveHistory(origItems);

    std::cout << "  -> PASSED!" << std::endl;
}

void testLlmPromptRefinement() {
    std::cout << "[TEST] 15. LLM Prompt Iterative Refinement..." << std::endl;

    LlmConfig cfg;
    cfg.modelName = "llama3";
    QString prevPrompt = "1girl, short hair, school uniform, daytime, classroom";
    QString prevNeg = "worst quality, bad anatomy";
    QString userMod = "もっと笑顔にして、髪を銀髪ロングに変えて、背景を夜空にして";
    QString style = "アニメ・美麗イラスト調 (Anime / Masterpiece)";

    QJsonObject json = LlmPromptService::buildRefineRequestJson(cfg, prevPrompt, prevNeg, userMod, style);
    assert(json["model"].toString() == "llama3");

    QJsonArray messages = json["messages"].toArray();
    assert(messages.size() == 2);

    QJsonObject sysMsg = messages[0].toObject();
    assert(sysMsg["role"].toString() == "system");
    QString sysContent = sysMsg["content"].toString();
    assert(sysContent.contains("iterative refinement", Qt::CaseInsensitive));
    assert(sysContent.contains("positive") || sysContent.contains("prompt"));
    assert(sysContent.contains("Anime / Masterpiece"));

    QJsonObject userMsg = messages[1].toObject();
    assert(userMsg["role"].toString() == "user");
    QString userContent = userMsg["content"].toString();
    assert(userContent.contains("もっと笑顔にして、髪を銀髪ロングに変えて"));
    assert(userContent.contains(prevPrompt));
    assert(userContent.contains(prevNeg));

    // レスポンス解析テスト
    QByteArray mockResponse = R"({
        "choices": [
            {
                "message": {
                    "role": "assistant",
                    "content": "{\"prompt\": \"1girl, very long hair, silver hair, beaming smile, starry sky, night, cinematic lighting, masterpiece, best quality\", \"negative_prompt\": \"worst quality, low quality, bad anatomy\"}"
                }
            }
        ]
    })";

    QString parseErr;
    LlmPromptResult res = LlmPromptService::parseChatResponseJson(mockResponse, &parseErr);
    assert(parseErr.isEmpty());
    assert(res.prompt.contains("silver hair"));
    assert(res.prompt.contains("starry sky"));
    assert(res.negativePrompt.contains("worst quality"));

    std::cout << "  -> PASSED!" << std::endl;
}

void testThreeDOpenPoseExtraction() {
    std::cout << "[TEST] 16. ThreeD OpenPose Extraction..." << std::endl;

    ThreeDLayer layer("3D Test", 512, 512);
    auto cube = ThreeDModel::createCube(1.0f);
    assert(cube != nullptr);
    layer.addModel(cube);

    // OpenPose画像生成
    QImage openPoseImg = layer.extractOpenPoseImage(512, 512);
    assert(!openPoseImg.isNull());
    assert(openPoseImg.width() == 512);
    assert(openPoseImg.height() == 512);

    // 黒背景であること (OpenPoseの標準仕様)
    QColor bgPixel = openPoseImg.pixelColor(0, 0);
    assert(bgPixel.red() == 0 && bgPixel.green() == 0 && bgPixel.blue() == 0);

    // キューブのモデル単体関節テスト (ボーン未登録プリミティブでも安全に18関節配列を返すこと)
    auto joints = cube->getOpenPoseJoints();
    assert(joints.size() == 18);

    std::cout << "  -> PASSED!" << std::endl;
}

void testSketchToLineartAndBrightnessToAlpha() {
    std::cout << "[TEST] 17. SketchToLineart & BrightnessToAlpha..." << std::endl;

    // 1. convertBrightnessToAlpha の検証
    QImage src(10, 10, QImage::Format_RGB32);
    src.fill(Qt::white); // 全面白
    src.setPixelColor(0, 0, Qt::black); // (0,0) は黒
    src.setPixelColor(1, 1, QColor(128, 128, 128)); // (1,1) はグレー

    QImage lineartAlpha = AiService::convertBrightnessToAlpha(src);
    assert(!lineartAlpha.isNull());
    assert(lineartAlpha.format() == QImage::Format_ARGB32_Premultiplied);

    // 白ピクセル -> 完全透明 (alpha == 0)
    QColor whiteResult = lineartAlpha.pixelColor(5, 5);
    assert(whiteResult.alpha() == 0);

    // 黒ピクセル -> 完全不透明黒 (alpha == 255, rgb == 0,0,0)
    QColor blackResult = lineartAlpha.pixelColor(0, 0);
    assert(blackResult.alpha() == 255);
    assert(blackResult.red() == 0 && blackResult.green() == 0 && blackResult.blue() == 0);

    // グレーピクセル -> 半透明 (alpha ~ 127)
    QColor grayResult = lineartAlpha.pixelColor(1, 1);
    assert(grayResult.alpha() >= 120 && grayResult.alpha() <= 135);

    // 2. ComfyUI JSON Building での SketchToLineart モード検証
    AiGenerateParams comfyParams;
    comfyParams.backend = ApiBackend::LocalComfyUI;
    comfyParams.mode = AiMode::SketchToLineart;
    comfyParams.denoisingStrength = 1.0f;
    comfyParams.initImage = src;
    comfyParams.enableControlNet = true;
    comfyParams.controlNetType = ControlNetType::Lineart;
    comfyParams.controlNetWeight = 1.0f;
    comfyParams.controlNetImage = src;

    QJsonObject comfyJson = AiService::buildComfyUiPromptJson(comfyParams);
    assert(!comfyJson.isEmpty());
    // initImage があるため KSampler に VAEEncode 経由で繋がっていること
    assert(comfyJson.contains("prompt"));
    QJsonObject promptObj = comfyJson["prompt"].toObject();
    assert(promptObj.contains("3")); // KSampler
    QJsonObject ksamplerInputs = promptObj["3"].toObject()["inputs"].toObject();
    assert(ksamplerInputs["denoise"].toDouble() == 1.0);

    // 2b. batchCount > 1 の時に RepeatLatentBatch (Node 12) が挿入されることの検証
    comfyParams.batchCount = 4;
    QJsonObject batchComfy = AiService::buildComfyUiPromptJson(comfyParams, "init.png", QString(), "cn.png");
    QJsonObject batchPrompt = batchComfy["prompt"].toObject();
    assert(batchPrompt.contains("12"));
    assert(batchPrompt["12"].toObject()["class_type"].toString() == "RepeatLatentBatch");
    assert(batchPrompt["12"].toObject()["inputs"].toObject()["amount"].toInt() == 4);
    assert(batchPrompt["3"].toObject()["inputs"].toObject()["latent_image"].toArray()[0].toString() == "12");

    // 2c. Inpaint モード時に LoadImage(マスク) + ImageToMask + SetLatentNoiseMask が挿入されることの検証
    AiGenerateParams inpaintParams;
    inpaintParams.mode = AiMode::Inpaint;
    inpaintParams.initImage = src;
    inpaintParams.maskImage = src;
    QJsonObject inpaintComfy = AiService::buildComfyUiPromptJson(inpaintParams, "init_canvas.png", "mask_eyes.png");
    QJsonObject inpaintPrompt = inpaintComfy["prompt"].toObject();
    assert(inpaintPrompt.contains("13")); // LoadImage (mask)
    assert(inpaintPrompt["13"].toObject()["class_type"].toString() == "LoadImage");
    assert(inpaintPrompt["13"].toObject()["inputs"].toObject()["image"].toString() == "mask_eyes.png");
    assert(inpaintPrompt.contains("14")); // ImageToMask
    assert(inpaintPrompt["14"].toObject()["class_type"].toString() == "ImageToMask");
    assert(inpaintPrompt["14"].toObject()["inputs"].toObject()["channel"].toString() == "red");
    assert(inpaintPrompt.contains("15")); // SetLatentNoiseMask
    assert(inpaintPrompt["15"].toObject()["class_type"].toString() == "SetLatentNoiseMask");
    assert(inpaintPrompt["3"].toObject()["inputs"].toObject()["latent_image"].toArray()[0].toString() == "15");

    // 3. AiAssistantDockWidget でのモード切り替え検証
    AiAssistantDockWidget dock;
    dock.setMode(AiMode::SketchToLineart);
    QImage initImg = dock.getInitImageForCurrentMode();
    assert(!initImg.isNull());
    // 白紙画像であること (ピクセルがすべて白)
    assert(initImg.pixelColor(0, 0) == Qt::white);
    assert(initImg.pixelColor(initImg.width() / 2, initImg.height() / 2) == Qt::white);

    std::cout << "  -> PASSED!" << std::endl;
}

void testSdxlControlNetSupport() {
    std::cout << "[TEST] 18. SDXL ControlNet Support & Model Resolution..." << std::endl;

    // 1. isModelSdxl 判定
    assert(AiService::isModelSdxl("animagine-xl-3.1.safetensors") == true);
    assert(AiService::isModelSdxl("illustrious-xl-v0.1.safetensors") == true);
    assert(AiService::isModelSdxl("ilustmix_v111.safetensors") == true);
    assert(AiService::isModelSdxl("ponyDiffusionV6XL_v6StartWithThisOne.safetensors") == true);
    assert(AiService::isModelSdxl("sd_xl_base_1.0.safetensors") == true);
    assert(AiService::isModelSdxl("v1-5-pruned-emaonly.safetensors") == false);
    assert(AiService::isModelSdxl("meinamix_v11.safetensors") == false);
    assert(AiService::isModelSdxl("dreamshaper_8.safetensors") == false);
    assert(AiService::isModelSdxl("") == false);

    // 2. AiComponentDownloader::fromControlNetType
    assert(AiComponentDownloader::fromControlNetType(ControlNetType::Lineart, true) == ComponentType::ControlNetSdxlLineart);
    assert(AiComponentDownloader::fromControlNetType(ControlNetType::Lineart, false) == ComponentType::ControlNetSd15Lineart);
    assert(AiComponentDownloader::fromControlNetType(ControlNetType::Canny, true) == ComponentType::ControlNetSdxlCanny);
    assert(AiComponentDownloader::fromControlNetType(ControlNetType::Canny, false) == ComponentType::ControlNetSd15Canny);
    assert(AiComponentDownloader::fromControlNetType(ControlNetType::Depth, true) == ComponentType::ControlNetSdxlDepth);
    assert(AiComponentDownloader::fromControlNetType(ControlNetType::OpenPose, true) == ComponentType::ControlNetSdxlOpenpose);
    assert(AiComponentDownloader::fromControlNetType(ControlNetType::Scribble, true) == ComponentType::ControlNetSdxlScribble);

    // SDXL Lineart Component Info
    auto sdxlLineartInfo = AiComponentDownloader::getComponentInfo(ComponentType::ControlNetSdxlLineart);
    assert(sdxlLineartInfo.fileName == "CN-anytest_v4-marged.safetensors");
    assert(sdxlLineartInfo.downloadUrl.contains("huggingface.co"));

    // 3. ComfyUI JSON での SDXL vs SD 1.5 ControlNet 自動切り替え
    AiGenerateParams sdxlParams;
    sdxlParams.backend = ApiBackend::LocalComfyUI;
    sdxlParams.modelName = "animagine-xl-3.1.safetensors";
    sdxlParams.enableControlNet = true;
    sdxlParams.controlNetType = ControlNetType::Lineart;
    sdxlParams.controlNetImage = QImage(32, 32, QImage::Format_ARGB32_Premultiplied);

    QJsonObject comfySdxl = AiService::buildComfyUiPromptJson(sdxlParams);
    assert(comfySdxl.contains("8"));
    QString cnModelSdxl = comfySdxl["8"].toObject()["inputs"].toObject()["controlnet_name"].toString();
    assert(cnModelSdxl == "CN-anytest_v4-marged.safetensors");

    AiGenerateParams sd15Params = sdxlParams;
    sd15Params.modelName = "v1-5-pruned-emaonly.safetensors";
    QJsonObject comfySd15 = AiService::buildComfyUiPromptJson(sd15Params);
    assert(comfySd15.contains("8"));
    QString cnModelSd15 = comfySd15["8"].toObject()["inputs"].toObject()["controlnet_name"].toString();
    assert(cnModelSd15 == "control_v11p_sd15_lineart.pth");

    // 4. WebUI JSON での SDXL vs SD 1.5 ControlNet 自動切り替え
    QJsonObject webUiSdxl = AiService::buildWebUiTxt2ImgJson(sdxlParams);
    QJsonObject alwaysOnSdxl = webUiSdxl["alwayson_scripts"].toObject()["controlnet"].toObject();
    QJsonObject unitSdxl = alwaysOnSdxl["args"].toArray()[0].toObject();
    assert(unitSdxl["model"].toString().contains("anytest"));

    QJsonObject webUiSd15 = AiService::buildWebUiTxt2ImgJson(sd15Params);
    QJsonObject alwaysOnSd15 = webUiSd15["alwayson_scripts"].toObject()["controlnet"].toObject();
    QJsonObject unitSd15 = alwaysOnSd15["args"].toArray()[0].toObject();
    // 5. findControlNetModelPath の実フォルダでの検証 (チェックポイント誤検出防止)
    QString foundSdxlCn = AiService::findControlNetModelPath(ControlNetType::Lineart, true);
    // sdxl_BW_bold_Line.safetensors などのベースモデル・Checkpointが誤ってControlNetとして検出されないこと
    assert(!foundSdxlCn.contains("sdxl_BW_bold_Line", Qt::CaseInsensitive));
    assert(!foundSdxlCn.contains("sdxl_BWLine", Qt::CaseInsensitive));

    // SD 1.5 Lineart の場合は既存の control_v11p_sd15_lineart.pth が正しく検出されること
    QString foundSd15Cn = AiService::findControlNetModelPath(ControlNetType::Lineart, false);
    if (!foundSd15Cn.isEmpty()) {
        assert(foundSd15Cn.contains("control_v11p_sd15_lineart", Qt::CaseInsensitive));
    }

    std::cout << "  -> PASSED!" << std::endl;
}

static void testCanvasAspectAutoSwitch() {
    std::cout << "[TEST] 19. Canvas Aspect Auto Switch & SDXL Scaling..." << std::endl;

    AiAssistantDockWidget widget;
    Document doc(1200, 800); // 3:2 横長キャンバス (比率 1.5)
    widget.setDocument(&doc);

    // 1. 最初は別のプリセット (512x512) を選んでおく
    QComboBox* comboRes = widget.findChild<QComboBox*>();
    // find comboRes
    QList<QComboBox*> combos = widget.findChildren<QComboBox*>();
    QComboBox* resCombo = nullptr;
    QComboBox* modeCombo = nullptr;
    QComboBox* modelCombo = nullptr;
    for (auto* c : combos) {
        if (c->findData(QSize(-1, -1)) >= 0) {
            resCombo = c;
        } else if (c->findData(static_cast<int>(AiMode::RoughToClean)) >= 0) {
            modeCombo = c;
        }
    }
    assert(resCombo != nullptr);
    assert(modeCombo != nullptr);

    // 手動で 512x512 を選ぶ
    int p512 = resCombo->findData(QSize(512, 512));
    assert(p512 >= 0);
    resCombo->setCurrentIndex(p512);
    assert(resCombo->currentIndex() == p512);

    // 2. 「ラフから線画生成 (SketchToLineart)」モードに切り替える
    int sketchIdx = modeCombo->findData(static_cast<int>(AiMode::SketchToLineart));
    assert(sketchIdx >= 0);
    modeCombo->setCurrentIndex(sketchIdx);

    // 解像度プリセットが自動で「キャンバス比率追従 (Canvas Aspect)」(QSize(-1, -1)) に切り替わっていること
    assert(resCombo->currentData().toSize() == QSize(-1, -1));

    // 3. 幅・高さのスピンボックスの値を確認
    QList<QSpinBox*> spins = widget.findChildren<QSpinBox*>();
    QSpinBox* spinW = nullptr;
    QSpinBox* spinH = nullptr;
    for (auto* s : spins) {
        if (s->suffix() == " px") {
            if (!spinW) spinW = s;
            else if (!spinH) spinH = s;
        }
    }
    assert(spinW != nullptr && spinH != nullptr);

    // SD 1.5 の場合: 1200x800 (比率 1.5) -> W ≈ sqrt(262144 * 1.5) = 627 -> 640, H ≈ sqrt(262144 / 1.5) = 418 -> 448
    // 横長 (W > H) であり、比率が 1.5 に極めて近いこと
    assert(spinW->value() > spinH->value());
    float aspectCalc = static_cast<float>(spinW->value()) / spinH->value();
    assert(std::abs(aspectCalc - 1.5f) < 0.2f);

    // 4. ドキュメントを縦長キャンバスに変更 (800x1200, 比率 0.6667)
    Document docPortrait(800, 1200);
    widget.setDocument(&docPortrait);

    // 縦長 (W < H) に自動更新されていること
    assert(spinW->value() < spinH->value());
    float aspectPortrait = static_cast<float>(spinW->value()) / spinH->value();
    assert(std::abs(aspectPortrait - (800.0f / 1200.0f)) < 0.15f);

    std::cout << "  -> PASSED!" << std::endl;
}

static void testInsertLayerAboveActive() {
    std::cout << "[TEST] 20. Insert Layer Directly Above Active Layer..." << std::endl;

    Document doc(800, 600);
    // 初期状態で Layer 1 (index 0) が存在
    assert(doc.layers().size() == 1);
    assert(doc.activeLayerIndex() == 0);

    // Layer 2 を追加 -> index 1 に挿入され、activeLayerIndex は 1 になる
    auto l2 = doc.addRasterLayer("Layer 2");
    assert(doc.layers().size() == 2);
    assert(doc.activeLayerIndex() == 1);
    assert(doc.layers()[1]->name() == "Layer 2");

    // Layer 3 を追加 -> index 2 に挿入され、activeLayerIndex は 2 になる
    auto l3 = doc.addRasterLayer("Layer 3");
    assert(doc.layers().size() == 3);
    assert(doc.activeLayerIndex() == 2);
    assert(doc.layers()[2]->name() == "Layer 3");

    // ここで、中間の Layer 2 (index 1) をアクティブにする
    doc.setActiveLayerIndex(1);
    assert(doc.activeLayerIndex() == 1);

    // 新規レイヤー "Layer 2.5" を追加
    // -> アクティブな Layer 2 (index 1) の直上である index 2 に挿入されるべき！
    auto l2_5 = doc.addRasterLayer("Layer 2.5");
    assert(doc.layers().size() == 4);
    assert(doc.activeLayerIndex() == 2);
    assert(doc.layers()[2]->name() == "Layer 2.5");
    assert(doc.layers()[1]->name() == "Layer 2");
    assert(doc.layers()[3]->name() == "Layer 3");

    // AI 生成レイヤー (addRasterLayerWithImage) も同様にアクティブレイヤーの直上に挿入されること
    // 一番下の Layer 1 (index 0) をアクティブにする
    doc.setActiveLayerIndex(0);
    QImage dummyImg(100, 100, QImage::Format_ARGB32);
    dummyImg.fill(Qt::blue);
    auto aiLayer = doc.addRasterLayerWithImage(dummyImg, "AI Layer Above Layer 1");
    assert(doc.layers().size() == 5);
    assert(doc.activeLayerIndex() == 1);
    assert(doc.layers()[1]->name() == "AI Layer Above Layer 1");
    assert(doc.layers()[0]->name() == "レイヤー 1");

    std::cout << "  -> PASSED!" << std::endl;
}

static void testQualityScaleControl() {
    std::cout << "[TEST] 21. AI Generation Quality Scale (Speed vs Quality)..." << std::endl;

    AiAssistantDockWidget widget;
    Document doc(1000, 1000); // 1:1 正方形キャンバス
    widget.setDocument(&doc);

    // QComboBox から m_comboQualityScale を見つける
    QList<QComboBox*> combos = widget.findChildren<QComboBox*>();
    QComboBox* qualityCombo = nullptr;
    QComboBox* resCombo = nullptr;
    for (auto* c : combos) {
        if (c->count() > 0 && c->itemData(0).toDouble() == 0.5) {
            qualityCombo = c;
        } else if (c->count() > 0 && c->itemData(0).toSize() == QSize(-1, -1)) {
            resCombo = c;
        }
    }
    assert(qualityCombo != nullptr);
    assert(resCombo != nullptr);

    // キャンバス比率追従を選択
    int aspectIdx = resCombo->findData(QSize(-1, -1));
    assert(aspectIdx >= 0);
    resCombo->setCurrentIndex(aspectIdx);

    QList<QSpinBox*> spins = widget.findChildren<QSpinBox*>();
    QSpinBox* spinW = nullptr;
    QSpinBox* spinH = nullptr;
    for (auto* s : spins) {
        if (s->suffix() == " px") {
            if (!spinW) spinW = s;
            else if (!spinH) spinH = s;
        }
    }
    assert(spinW != nullptr && spinH != nullptr);

    // 1. 標準 (1.0x) の解像度を取得
    int standardIdx = qualityCombo->findData(1.0);
    assert(standardIdx >= 0);
    qualityCombo->setCurrentIndex(standardIdx);
    int standardW = spinW->value();
    int standardH = spinH->value();
    assert(standardW > 0 && standardH > 0);

    // 2. 高速ドラフト (0.5x 画素) に変更
    int draftIdx = qualityCombo->findData(0.5);
    assert(draftIdx >= 0);
    qualityCombo->setCurrentIndex(draftIdx);
    int draftW = spinW->value();
    int draftH = spinH->value();

    // 画素数が標準の約半分（W, H それぞれ標準の約 0.71 倍）になっていること
    assert(draftW < standardW);
    assert(draftH < standardH);
    double areaRatio = (double)(draftW * draftH) / (double)(standardW * standardH);
    assert(areaRatio >= 0.4 && areaRatio <= 0.65);

    // 3. 最高品質 (2.0x 画素) に変更
    int highIdx = qualityCombo->findData(2.0);
    assert(highIdx >= 0);
    qualityCombo->setCurrentIndex(highIdx);
    int highW = spinW->value();
    int highH = spinH->value();

    // 画素数が標準の約2倍になっていること
    assert(highW > standardW);
    assert(highH > standardH);
    double highAreaRatio = (double)(highW * highH) / (double)(standardW * standardH);
    assert(highAreaRatio >= 1.7 && highAreaRatio <= 2.3);

    std::cout << "  -> PASSED!" << std::endl;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    std::cout << "=== RUNNING AI SERVICE & ASSISTANT TESTS ===" << std::endl;

    testBase64Conversion();
    testWebUiJsonBuilding();
    testComfyUiJsonBuilding();
    testResponseParsing();
    testAiAssistantDockWidget();
    testCanvasIntegration();
    testPreferencesAiDirectories();
    testAiComponentDownloader();
    testLlmPromptService();
    testSamplerAndScheduler();
    testRemoveBackground();
    testUpscaleModelDirectoryPreference();
    testAiSettingsPersistenceAndThumbnailStyle();
    testPromptHistory();
    testLlmPromptRefinement();
    testThreeDOpenPoseExtraction();
    testSketchToLineartAndBrightnessToAlpha();
    testSdxlControlNetSupport();
    testCanvasAspectAutoSwitch();
    testInsertLayerAboveActive();
    testQualityScaleControl();

    std::cout << "[ALL AI SERVICE & ASSISTANT TESTS PASSED SUCCESSFULLY!]" << std::endl;
    return 0;
}
