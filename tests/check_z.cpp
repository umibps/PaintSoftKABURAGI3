#include <QApplication>
#include <iostream>
#include "ThreeDModel.hpp"

using namespace Kaburagi;

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    ThreeDModel model;
    if (!model.loadVrm("E:/Works/3D Model/Model/î‹ã{êÁëÅ/î‹ã{êÁëÅ5.vrm")) return 1;

    const auto& mBody = model.meshes()[10];
    const auto& mCloth = model.meshes()[14];

    std::cout << "Checking stomach area (Y in [0.8, 1.1], X in [-0.1, 0.1])..." << std::endl;
    float maxZ_Body = -1e9f, minZ_Body = 1e9f;
    for (uint32_t idx : mBody.indices) {
        const auto& v = mBody.vertices[idx];
        if (v.position.y() >= 0.8f && v.position.y() <= 1.1f && std::abs(v.position.x()) <= 0.1f) {
            maxZ_Body = std::max(maxZ_Body, v.position.z());
            minZ_Body = std::min(minZ_Body, v.position.z());
        }
    }
    float maxZ_Cloth = -1e9f, minZ_Cloth = 1e9f;
    for (uint32_t idx : mCloth.indices) {
        const auto& v = mCloth.vertices[idx];
        if (v.position.y() >= 0.8f && v.position.y() <= 1.1f && std::abs(v.position.x()) <= 0.1f) {
            maxZ_Cloth = std::max(maxZ_Cloth, v.position.z());
            minZ_Cloth = std::min(minZ_Cloth, v.position.z());
        }
    }
    std::cout << "Body Z: [" << minZ_Body << ", " << maxZ_Body << "]" << std::endl;
    std::cout << "Cloth Z: [" << minZ_Cloth << ", " << maxZ_Cloth << "]" << std::endl;
    return 0;
}
