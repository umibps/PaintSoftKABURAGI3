#pragma once

#include "Layer.hpp"
#include <vector>
#include <QPointF>

namespace Kaburagi {

struct VectorPoint {
    float x = 0.0f;
    float y = 0.0f;
    float pressure = 1.0f; // 0.0f ~ 2.0f
    float opacity = 1.0f;  // 0.0f ~ 1.0f
    bool isCorner = false; // 角/曲線フラグ
    bool isSelected = false; // 選択フラグ
};

struct VectorStroke {
    std::vector<VectorPoint> points;
    float baseWidth = 4.0f;
    uint32_t color = 0xFF000000; // ARGB
    float density = 1.0f;        // 濃度 (0.0f ~ 1.0f)
    int hardness = 100;          // 輪郭の硬さ (0 ~ 100)
    int falloff = 0;             // ボケ足 (0 ~ 100)
    bool isClosed = false;       // パスを閉じるか
    bool isFilled = false;       // 内部を塗りつぶすか
    bool isSelected = false;

    // 指定座標に最も近い制御点を探す
    int findNearestPoint(const QPointF& pt, float maxDist = 20.0f) const;

    // 指定座標付近の制御点の筆圧を変更 (SAI筆圧ツール)
    bool adjustPressureNear(const QPointF& pt, float deltaPressure, float radius = 30.0f);

    // 楕円ストロークを生成するヘルパー関数
    static VectorStroke createEllipse(const QPointF& center, float rx, float ry,
                                     float width, uint32_t color, float density,
                                     int hardness, int falloff, bool isFilled);

    // 多角形ストロークを生成するヘルパー関数
    static VectorStroke createPolygon(const QPointF& center, float rx, float ry,
                                      int sides, bool isRegular, float rotAngle,
                                      float width, uint32_t color, float density,
                                      int hardness, int falloff, bool isFilled);
};

enum class VectorEraserMode {
    Intersection = 0,   // 交点まで消去 (デフォルト)
    WholeLine = 1,      // 線全体を消去
    TouchedArea = 2     // 触れた部分を消去
};

enum class VectorControlPointMode {
    MovePoint = 0,          // 制御点移動
    AddPoint = 1,           // 制御点追加
    DeletePoint = 2,        // 制御点削除
    MoveStroke = 3,         // ストローク移動
    DuplicateMoveStroke = 4,// ストローク複製移動
    ConnectStrokes = 5,     // ストローク連結
    DeleteStroke = 6,       // ストローク削除
    RotateStroke = 7,       // ストローク回転
    MoveOrAddPoint = 8      // 制御点 移動/追加
};

class VectorLayer : public Layer {
public:
    VectorLayer(const QString& name, int width, int height);
    ~VectorLayer() override = default;

    std::vector<VectorStroke>& strokes() { return m_strokes; }
    const std::vector<VectorStroke>& strokes() const { return m_strokes; }
    void setStrokes(const std::vector<VectorStroke>& strokes);

    void addStroke(const VectorStroke& stroke);
    bool removeStroke(size_t index);

    // 制御点／ストローク選択管理
    void clearSelection();
    void selectStroke(int strokeIndex, bool select = true);
    void selectPoint(int strokeIndex, int pointIndex, bool select = true);
    int selectedStrokeIndex() const;
    std::pair<int, int> selectedPointIndex() const;

    // ヒットテスト
    bool findNearestControlPoint(const QPointF& pt, float hitRadius, int& outStrokeIdx, int& outPtIdx, float* outDist = nullptr) const;
    int findNearestStroke(const QPointF& pt, float hitRadius, float* outDist = nullptr, int* outSegIndex = nullptr, float* outSegT = nullptr) const;

    // 制御点・ストロークの編集操作
    bool movePoint(int strokeIdx, int ptIdx, const QPointF& newPos);
    bool addPointToStroke(int strokeIdx, const QPointF& pt, int* outAddedPtIdx = nullptr);
    bool removePointFromStroke(int strokeIdx, int ptIdx);
    bool moveStroke(int strokeIdx, float dx, float dy);
    int duplicateStroke(int strokeIdx);
    QPointF strokeCenter(int strokeIdx) const;
    bool rotateStroke(int strokeIdx, float angleDegrees, const QPointF& center);
    bool connectStrokes(int strokeIdx1, int ptIdx1, int strokeIdx2, int ptIdx2);

    // ベクター消しゴム処理 (交点まで消去 / 線全体消去 / 触れた部分消去)
    // 変更があった場合は true を返し、更新されたストローク群を反映
    bool eraseAt(const QPointF& pt, float radius, VectorEraserMode mode);
    bool eraseByTrajectory(const std::vector<QPointF>& trajectory, float radius, VectorEraserMode mode);

    // 制御点やストロークが編集されたときにタイルバッファへ再描画
    void rasterizeAll();
    void rasterizeStroke(const VectorStroke& stroke);
    void translate(int dx, int dy) override;

    // ヘルパー: ストロークのCatmull-Rom補間されたサンプリング点列を取得
    static std::vector<QPointF> sampleStrokePoints(const VectorStroke& stroke, std::vector<float>* outParams = nullptr, int subdiv = 8);

    // ヘルパー: ストローク制御点の自動簡約化 (Ramer-Douglas-Peucker 拡張: 幾何形状・筆圧・不透明度を維持しながら制御点を最小化)
    static std::vector<VectorPoint> simplifyPoints(const std::vector<VectorPoint>& points, float tolerance = 2.0f, float pressureTol = 0.2f);

private:
    std::vector<VectorStroke> m_strokes;
    void drawSegment(const QPointF& p0, const QPointF& p1, float w0, float w1, uint32_t color);

    bool eraseWholeLine(const QPointF& pt, float radius);
    bool eraseWholeLineTrajectory(const std::vector<QPointF>& trajectory, float radius);
    bool eraseIntersection(const QPointF& pt, float radius, bool doRasterize = true);
    bool eraseTouchedAreaTrajectory(const std::vector<QPointF>& trajectory, float radius);
};

} // namespace Kaburagi
