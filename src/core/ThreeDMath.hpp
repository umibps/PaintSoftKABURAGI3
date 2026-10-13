#pragma once

#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QColor>
#include <QImage>
#include <QString>
#include <vector>
#include <algorithm>
#include <cmath>

namespace Kaburagi {

struct Vertex3D {
    QVector3D position;
    QVector3D normal;
    QVector2D texCoord;
    QVector4D color = QVector4D(1.0f, 1.0f, 1.0f, 1.0f);
};

struct Material3D {
    QString name;
    QColor diffuseColor = Qt::white;
    QImage diffuseTexture;
    QColor shadeColor = QColor(220, 205, 215);
    QImage shadeTexture;
    float shadeShift = 0.0f;
    float shadeToony = 0.9f;
    bool hasShadeColor = false;
    float roughness = 0.5f;
    float metallic = 0.0f;
    QString alphaMode = "OPAQUE";
    int blendMode = 0;      // 0: Opaque, 1: Cutout, 2: Transparent, 3: TransparentWithZWrite
    int renderQueue = 2000; // Unity/VRM render queue (2000=Opaque, 2450=Cutout, 3000=Transparent)
    int cullMode = 2;       // 0: Off, 1: Front, 2: Back
    float alphaCutoff = 0.5f;
    bool doubleSided = false;
};

struct VertexSkinWeight {
    uint16_t joints[4] = {0, 0, 0, 0};
    float weights[4] = {1.0f, 0.0f, 0.0f, 0.0f};
};

struct Mesh3D {
    QString name;
    std::vector<Vertex3D> vertices;
    std::vector<Vertex3D> bindVertices;
    std::vector<VertexSkinWeight> skinWeights;
    std::vector<uint32_t> indices;
    int materialIndex = -1;
    bool visible = true;
    int skinIndex = -1;
};

struct BoundingBox3D {
    QVector3D min = QVector3D(1e9f, 1e9f, 1e9f);
    QVector3D max = QVector3D(-1e9f, -1e9f, -1e9f);

    void expand(const QVector3D& p) {
        min.setX(std::min(min.x(), p.x()));
        min.setY(std::min(min.y(), p.y()));
        min.setZ(std::min(min.z(), p.z()));
        max.setX(std::max(max.x(), p.x()));
        max.setY(std::max(max.y(), p.y()));
        max.setZ(std::max(max.z(), p.z()));
    }

    QVector3D center() const {
        return (min + max) * 0.5f;
    }

    QVector3D size() const {
        return max - min;
    }

    float maxDimension() const {
        QVector3D s = size();
        return std::max({s.x(), s.y(), s.z()});
    }

    bool isValid() const {
        return min.x() <= max.x() && min.y() <= max.y() && min.z() <= max.z();
    }
};

} // namespace Kaburagi
