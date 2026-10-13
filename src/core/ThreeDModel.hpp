#pragma once

#include "ThreeDMath.hpp"
#include "ThreeDPose.hpp"
#include <QString>
#include <memory>
#include <vector>

namespace Kaburagi {

class ThreeDModel {
public:
    ThreeDModel();
    ~ThreeDModel() = default;

    const QString& name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    bool isVisible() const { return m_visible; }
    void setVisible(bool v) { m_visible = v; }

    const QString& filePath() const { return m_filePath; }

    const std::vector<Mesh3D>& meshes() const { return m_meshes; }
    std::vector<Mesh3D>& meshes() { return m_meshes; }

    const std::vector<Material3D>& materials() const { return m_materials; }
    std::vector<Material3D>& materials() { return m_materials; }

    const BoundingBox3D& boundingBox() const { return m_boundingBox; }

    // モデル姿勢
    const QVector3D& position() const { return m_position; }
    void setPosition(const QVector3D& pos) { m_position = pos; }

    const QQuaternion& rotation() const { return m_rotation; }
    void setRotation(const QQuaternion& rot) {
        m_rotation = rot;
        m_rotationEuler = rot.toEulerAngles();
    }

    const QVector3D& rotationEuler() const { return m_rotationEuler; }
    void setRotationEuler(const QVector3D& eulerDeg);

    const QVector3D& scale() const { return m_scale; }
    void setScale(const QVector3D& s) { m_scale = s; }

    QMatrix4x4 modelMatrix() const;

    // ファイル読み込み
    bool loadFromFile(const QString& filePath);
    bool loadObj(const QString& filePath);
    bool loadGltf(const QString& filePath);
    bool loadPmd(const QString& filePath);
    bool loadPmx(const QString& filePath);
    bool loadVrm(const QString& filePath);

    // スケルトン＆ポーズ適用
    Skeleton3D& skeleton() { return m_skeleton; }
    const Skeleton3D& skeleton() const { return m_skeleton; }
    void applyPose(const ThreeDPose& pose, bool updateCpuSkinning = true);

    // ノード＆スキン管理
    struct ModelNode {
        QString name;
        int parent = -1;
        std::vector<int> children;
        QVector3D translation;
        QVector3D initialTranslation;
        QQuaternion rotation;
        QQuaternion initialRotation;
        QVector3D scale = QVector3D(1.0f, 1.0f, 1.0f);
        QMatrix4x4 worldMatrix;
    };

    struct ModelSkin {
        std::vector<int> joints;
        std::vector<QMatrix4x4> inverseBindMatrices;
    };

    const std::vector<ModelNode>& nodes() const { return m_nodes; }
    std::vector<ModelNode>& nodes() { return m_nodes; }
    const std::vector<ModelSkin>& skins() const { return m_skins; }
    std::vector<ModelSkin>& skins() { return m_skins; }
    std::unordered_map<QString, int>& humanoidBoneToNode() { return m_humanoidBoneToNode; }
    const std::unordered_map<QString, int>& humanoidBoneToNode() const { return m_humanoidBoneToNode; }

    void updateNodeMatrices();
    void updateSkinning();
    void applySkeletonSkinning();
    std::vector<QMatrix4x4> getMeshSkinMatrices(int meshIdx) const;

    // バウンディングボックス計算＆自動センタリング/スケール正規化
    void recalculateBoundingBox();
    void normalizeScale(float targetSize = 2.0f);

    // SpringBone (二次物理シミュレーション: 髪の毛・服・胸等の揺れもの)
    struct SpringBoneCollider {
        QVector3D offset;
        float radius = 0.0f;
    };

    struct SpringBoneColliderGroup {
        int nodeIdx = -1;
        std::vector<SpringBoneCollider> colliders;
    };

    struct SpringBoneJoint {
        int nodeIdx = -1;
        int childNodeIdx = -1; // 末端の場合は -1
        QVector3D localTail;   // このノードのローカル空間における先端(Tail)の座標
        float boneLength = 0.07f;
        QVector3D currentTail; // ワールド空間の現在先端位置
        QVector3D prevTail;    // ワールド空間の前回先端位置
        bool initialized = false;
    };

    struct SpringBoneGroup {
        QString comment;
        float stiffness = 0.5f;
        float dragForce = 0.4f;
        QVector3D gravityDir = QVector3D(0.0f, -1.0f, 0.0f);
        float gravityPower = 0.0f;
        float hitRadius = 0.02f;
        std::vector<int> rootNodes;
        std::vector<int> colliderGroupIndices;
        std::vector<SpringBoneJoint> joints;
    };

    std::vector<SpringBoneColliderGroup>& springColliderGroups() { return m_springColliderGroups; }
    const std::vector<SpringBoneColliderGroup>& springColliderGroups() const { return m_springColliderGroups; }

    std::vector<SpringBoneGroup>& springBoneGroups() { return m_springBoneGroups; }
    const std::vector<SpringBoneGroup>& springBoneGroups() const { return m_springBoneGroups; }

    void initSpringBones();
    void resetSpringBones();
    void updateSpringBones(float dt, bool updateCpuSkinning = true);

    // テスト・プリミティブ生成
    static std::shared_ptr<ThreeDModel> createCube(float size = 1.0f);
    static std::shared_ptr<ThreeDModel> createGrid(float size = 10.0f, int divisions = 10);

private:
    QString m_name;
    QString m_filePath;
    std::vector<Mesh3D> m_meshes;
    std::vector<Material3D> m_materials;
    BoundingBox3D m_boundingBox;
    Skeleton3D m_skeleton;

    std::vector<ModelNode> m_nodes;
    std::vector<ModelSkin> m_skins;
    std::unordered_map<QString, int> m_humanoidBoneToNode;

    std::vector<SpringBoneColliderGroup> m_springColliderGroups;
    std::vector<SpringBoneGroup> m_springBoneGroups;

    QVector3D m_position = QVector3D(0.0f, 0.0f, 0.0f);
    QQuaternion m_rotation = QQuaternion();
    QVector3D m_rotationEuler = QVector3D(0.0f, 0.0f, 0.0f);
    QVector3D m_scale = QVector3D(1.0f, 1.0f, 1.0f);
    bool m_visible = true;
};

} // namespace Kaburagi
