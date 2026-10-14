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
#include "Document.hpp"
#include "Layer.hpp"
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

    QTemporaryDir tempUpscaleDir;
    assert(tempUpscaleDir.isValid());

    PreferencesDialog::setUpscaleModelDirectory(tempUpscaleDir.path());
    assert(PreferencesDialog::upscaleModelDirectory() == tempUpscaleDir.path());

    auto infoAnime6B = AiComponentDownloader::getComponentInfo(ComponentType::UpscaleModelAnime6B);
    assert(infoAnime6B.targetDirectory == tempUpscaleDir.path());

    auto infoRealEsrgan = AiComponentDownloader::getComponentInfo(ComponentType::UpscaleModelRealEsrgan);
    assert(infoRealEsrgan.targetDirectory == tempUpscaleDir.path());

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

    std::cout << "[ALL AI SERVICE & ASSISTANT TESTS PASSED SUCCESSFULLY!]" << std::endl;
    return 0;
}
