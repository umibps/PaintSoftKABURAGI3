#include <QApplication>
#include <QFile>
#include <iostream>
#include "ThreeDModel.hpp"

using namespace Kaburagi;

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    ThreeDModel model;
    if (!model.loadVrm("E:/Works/3D Model/Model/”Ü‹{ç‘/”Ü‹{ç‘5.vrm")) {
        return 1;
    }
    for (size_t i = 0; i < model.materials().size(); ++i) {
        const auto& m = model.materials()[i];
        if (!m.diffuseTexture.isNull()) {
            QString fn = QString("E:/Works/Programming/PaintSoftKABURAGI3_0_1/scratch_mat_%1_%2.png").arg(i).arg(m.name);
            m.diffuseTexture.save(fn);
            std::cout << "Saved " << fn.toStdString() << " format=" << m.diffuseTexture.format() << std::endl;
        }
    }
    return 0;
}
