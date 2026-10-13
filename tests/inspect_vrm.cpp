#include <QApplication>
#include <QFile>
#include <iostream>
#include "ThreeDModel.hpp"

using namespace Kaburagi;

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    ThreeDModel model;
    if (!model.loadVrm("E:/Works/3D Model/Model/”Ü‹{ç‘/”Ü‹{ç‘5.vrm")) {
        std::cerr << "Failed to load VRM" << std::endl;
        return 1;
    }
    std::cout << "Materials count: " << model.materials().size() << std::endl;
    for (size_t i = 0; i < model.materials().size(); ++i) {
        const auto& m = model.materials()[i];
        std::cout << "Mat " << i << ": name=" << m.name.toStdString()
                  << " hasTex=" << (!m.diffuseTexture.isNull())
                  << " hasAlpha=" << (m.diffuseTexture.hasAlphaChannel()) << std::endl;
    }
    std::cout << "Meshes count: " << model.meshes().size() << std::endl;
    for (size_t i = 0; i < model.meshes().size(); ++i) {
        const auto& mesh = model.meshes()[i];
        std::cout << "Mesh " << i << ": name=" << mesh.name.toStdString()
                  << " matIdx=" << mesh.materialIndex
                  << " vertCount=" << mesh.vertices.size() << std::endl;
    }
    return 0;
}
