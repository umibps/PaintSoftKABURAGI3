#pragma once

#include <QPointF>
#include <vector>
#include <deque>

namespace Kaburagi {

struct StrokeInputPoint {
    QPointF pos;
    float pressure = 1.0f;
};

/**
 * @brief SAI風 手ブレ補正エンジン
 * 補正レベル 0 (OFF) 〜 15 (S-7相当) に応じて指数移動平均と履歴補間を行う。
 */
class StrokeSmoother {
public:
    StrokeSmoother(int smoothLevel = 3) : m_smoothLevel(smoothLevel) {}

    void setSmoothLevel(int level) {
        m_smoothLevel = std::max(0, std::min(15, level));
    }
    int smoothLevel() const { return m_smoothLevel; }

    void reset() {
        m_history.clear();
        m_hasLastFiltered = false;
    }

    StrokeInputPoint pushPoint(const QPointF& rawPos, float rawPressure) {
        if (m_smoothLevel == 0) {
            return { rawPos, rawPressure };
        }

        m_history.push_back({ rawPos, rawPressure });
        int windowSize = std::min(static_cast<size_t>(m_smoothLevel + 2), static_cast<size_t>(16));
        if (m_history.size() > static_cast<size_t>(windowSize)) {
            m_history.pop_front();
        }

        // 重み付け移動平均
        float totalWeight = 0.0f;
        QPointF weightedPos(0, 0);
        float weightedPressure = 0.0f;

        float weight = 1.0f;
        for (auto it = m_history.rbegin(); it != m_history.rend(); ++it) {
            weightedPos += it->pos * weight;
            weightedPressure += it->pressure * weight;
            totalWeight += weight;
            weight *= 0.75f;
        }

        QPointF avgPos = weightedPos / totalWeight;
        float avgPressure = weightedPressure / totalWeight;

        if (!m_hasLastFiltered) {
            m_lastFiltered = { avgPos, avgPressure };
            m_hasLastFiltered = true;
            return m_lastFiltered;
        }

        // 指数平滑化
        float alpha = 1.0f / (1.0f + m_smoothLevel * 0.4f);
        m_lastFiltered.pos = m_lastFiltered.pos * (1.0f - alpha) + avgPos * alpha;
        m_lastFiltered.pressure = m_lastFiltered.pressure * (1.0f - alpha) + avgPressure * alpha;

        return m_lastFiltered;
    }

private:
    int m_smoothLevel = 3;
    std::deque<StrokeInputPoint> m_history;
    StrokeInputPoint m_lastFiltered;
    bool m_hasLastFiltered = false;
};

} // namespace Kaburagi
