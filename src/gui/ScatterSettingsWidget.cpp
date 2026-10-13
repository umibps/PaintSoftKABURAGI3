#include "ScatterSettingsWidget.hpp"
#include <QVBoxLayout>
#include <algorithm>

namespace Kaburagi {

ScatterSettingsWidget::ScatterSettingsWidget(QWidget* parent)
    : QWidget(parent) {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(4);

    // 1. 散布をする／しない チェックボックス
    m_scatterCheck = new QCheckBox("散布", this);
    m_scatterCheck->setStyleSheet("QCheckBox { font-size: 11px; font-weight: bold; }");
    m_scatterCheck->setToolTip("散布描画を有効にする");
    mainLayout->addWidget(m_scatterCheck);

    // 2. 詳細設定コンテナ（散布ON時のみ表示）
    m_subContainer = new QWidget(this);
    auto subLayout = new QVBoxLayout(m_subContainer);
    subLayout->setContentsMargins(0, 0, 0, 0);
    subLayout->setSpacing(4);

    // 散布量: 1〜20
    m_amountScale = new SpinScale("散布量", 1.0, 20.0, 1.0, 0, "", m_subContainer);
    m_amountScale->setValue(5.0);
    m_amountScale->setToolTip("一度に散布として描画する回数 (1〜20)");
    subLayout->addWidget(m_amountScale);

    // 散布範囲: 1〜500
    m_rangeScale = new SpinScale("散布範囲", 1.0, 500.0, 1.0, 0, "", m_subContainer);
    m_rangeScale->setValue(100.0);
    m_rangeScale->setToolTip("散布描画がマウスカーソルから離れる距離の最大値 (1〜500, 100でブラシ一つ分離れる)");
    subLayout->addWidget(m_rangeScale);

    // 散布サイズ ランダム: 0〜100%
    m_sizeRandomScale = new SpinScale("散布サイズ ランダム", 0.0, 100.0, 1.0, 0, "%", m_subContainer);
    m_sizeRandomScale->setValue(0.0);
    m_sizeRandomScale->setToolTip("散布される描画サイズをランダムに変更する (0〜100%, 0で変更なし)");
    subLayout->addWidget(m_sizeRandomScale);

    // 散布サイズ ランダム拡大有無
    m_scaleUpCheck = new QCheckBox("散布サイズ ランダム拡大", m_subContainer);
    m_scaleUpCheck->setChecked(false);
    m_scaleUpCheck->setStyleSheet("QCheckBox { font-size: 11px; margin-left: 2px; }");
    m_scaleUpCheck->setToolTip("ONの時は現在のブラシサイズの+/-設定値、OFFの時は-設定値とする");
    subLayout->addWidget(m_scaleUpCheck);

    mainLayout->addWidget(m_subContainer);

    // 初期状態は非表示（散布OFFのため）
    m_subContainer->setVisible(false);

    // シグナル接続
    connect(m_scatterCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_subContainer->setVisible(checked);
        emit scatterChanged(checked);
    });

    connect(m_amountScale, &SpinScale::valueChanged, this, [this](double val) {
        emit scatterAmountChanged(static_cast<int>(val));
    });

    connect(m_rangeScale, &SpinScale::valueChanged, this, [this](double val) {
        emit scatterRangeChanged(static_cast<int>(val));
    });

    connect(m_sizeRandomScale, &SpinScale::valueChanged, this, [this](double val) {
        emit scatterSizeRandomChanged(static_cast<int>(val));
    });

    connect(m_scaleUpCheck, &QCheckBox::toggled, this, &ScatterSettingsWidget::scatterScaleUpChanged);
}

bool ScatterSettingsWidget::isScatterEnabled() const {
    return m_scatterCheck ? m_scatterCheck->isChecked() : false;
}

int ScatterSettingsWidget::scatterAmount() const {
    return m_amountScale ? static_cast<int>(m_amountScale->value()) : 5;
}

int ScatterSettingsWidget::scatterRange() const {
    return m_rangeScale ? static_cast<int>(m_rangeScale->value()) : 100;
}

int ScatterSettingsWidget::scatterSizeRandom() const {
    return m_sizeRandomScale ? static_cast<int>(m_sizeRandomScale->value()) : 0;
}

bool ScatterSettingsWidget::isScatterScaleUpEnabled() const {
    return m_scaleUpCheck ? m_scaleUpCheck->isChecked() : false;
}

void ScatterSettingsWidget::setScatterEnabled(bool enabled) {
    if (m_scatterCheck) {
        m_scatterCheck->setChecked(enabled);
        if (m_subContainer) {
            m_subContainer->setVisible(enabled);
        }
    }
}

void ScatterSettingsWidget::setScatterAmount(int amount) {
    if (m_amountScale) {
        m_amountScale->setValue(std::clamp(amount, 1, 20));
    }
}

void ScatterSettingsWidget::setScatterRange(int range) {
    if (m_rangeScale) {
        m_rangeScale->setValue(std::clamp(range, 1, 500));
    }
}

void ScatterSettingsWidget::setScatterSizeRandom(int random) {
    if (m_sizeRandomScale) {
        m_sizeRandomScale->setValue(std::clamp(random, 0, 100));
    }
}

void ScatterSettingsWidget::setScatterScaleUpEnabled(bool enabled) {
    if (m_scaleUpCheck) {
        m_scaleUpCheck->setChecked(enabled);
    }
}

} // namespace Kaburagi
