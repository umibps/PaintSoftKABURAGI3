#pragma once

#include <QWidget>
#include <QCheckBox>
#include "SpinScaleWidget.hpp"

namespace Kaburagi {

class ScatterSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit ScatterSettingsWidget(QWidget* parent = nullptr);
    ~ScatterSettingsWidget() override = default;

    bool isScatterEnabled() const;
    int scatterAmount() const;
    int scatterRange() const;
    int scatterSizeRandom() const;
    bool isScatterScaleUpEnabled() const;

    void setScatterEnabled(bool enabled);
    void setScatterAmount(int amount);
    void setScatterRange(int range);
    void setScatterSizeRandom(int random);
    void setScatterScaleUpEnabled(bool enabled);
    void setScatterScaleUp(bool enabled) { setScatterScaleUpEnabled(enabled); }

    QWidget* subContainer() const { return m_subContainer; }
    QWidget* containerWidget() const { return m_subContainer; }

signals:
    void scatterChanged(bool enabled);
    void scatterAmountChanged(int amount);
    void scatterRangeChanged(int range);
    void scatterSizeRandomChanged(int random);
    void scatterScaleUpChanged(bool enabled);

private:
    QCheckBox* m_scatterCheck = nullptr;
    QWidget* m_subContainer = nullptr;
    SpinScale* m_amountScale = nullptr;
    SpinScale* m_rangeScale = nullptr;
    SpinScale* m_sizeRandomScale = nullptr;
    QCheckBox* m_scaleUpCheck = nullptr;
};

} // namespace Kaburagi
