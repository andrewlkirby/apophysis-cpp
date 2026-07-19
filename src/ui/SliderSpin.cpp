#include "SliderSpin.h"

#include <cmath>

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>

namespace apo::ui {

namespace {
int scaleFor(int decimals) {
    int s = 1;
    for (int i = 0; i < decimals; ++i) s *= 10;
    return s;
}
} // namespace

SliderSpin::SliderSpin(const QString& label, double min, double max, double value, int decimals, QWidget* parent)
    : QWidget(parent), scale_(scaleFor(decimals)) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* labelWidget = new QLabel(label, this);
    labelWidget->setMinimumWidth(90);
    layout->addWidget(labelWidget);

    slider_ = new QSlider(Qt::Horizontal, this);
    slider_->setRange(static_cast<int>(std::lround(min * scale_)), static_cast<int>(std::lround(max * scale_)));
    slider_->setValue(static_cast<int>(std::lround(value * scale_)));
    layout->addWidget(slider_, /*stretch=*/1);

    spin_ = new QDoubleSpinBox(this);
    spin_->setDecimals(decimals);
    spin_->setRange(min, max);
    spin_->setSingleStep(1.0 / scale_);
    spin_->setValue(value);
    spin_->setMinimumWidth(80);
    layout->addWidget(spin_);

    connect(slider_, &QSlider::valueChanged, this, [this](int v) {
        if (syncing_) return;
        syncing_ = true;
        const double dv = static_cast<double>(v) / scale_;
        spin_->setValue(dv);
        syncing_ = false;
        emit valueChanged(dv);
    });
    connect(slider_, &QSlider::sliderReleased, this, [this] { emit editingFinished(this->value()); });

    connect(spin_, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        if (syncing_) return;
        syncing_ = true;
        slider_->setValue(static_cast<int>(std::lround(v * scale_)));
        syncing_ = false;
        emit valueChanged(v);
    });
    connect(spin_, &QDoubleSpinBox::editingFinished, this, [this] { emit editingFinished(this->value()); });
}

double SliderSpin::value() const { return spin_->value(); }

void SliderSpin::setRange(double min, double max, double value) {
    syncing_ = true;
    spin_->setRange(min, max);
    slider_->setRange(static_cast<int>(std::lround(min * scale_)), static_cast<int>(std::lround(max * scale_)));
    spin_->setValue(value);
    slider_->setValue(static_cast<int>(std::lround(value * scale_)));
    syncing_ = false;
}

void SliderSpin::setValue(double v) {
    syncing_ = true;
    spin_->setValue(v);
    slider_->setValue(static_cast<int>(std::lround(v * scale_)));
    syncing_ = false;
}

} // namespace apo::ui
