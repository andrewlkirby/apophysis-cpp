#include "ForceSymmetryDialog.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace apo::ui {

ForceSymmetryDialog::ForceSymmetryDialog(std::shared_ptr<apo::Flame> flame, QWidget* parent)
    : QDialog(parent), flame_(std::move(flame)) {
    setWindowTitle(
        QString("Force Symmetry - %1").arg(flame_->name.empty() ? "Untitled" : QString::fromStdString(flame_->name)));
    setAttribute(Qt::WA_DeleteOnClose);

    auto* rootLayout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    kindCombo_ = new QComboBox(this);
    kindCombo_->setObjectName("kindCombo");
    kindCombo_->addItems({"Rotational", "Rotational + Reflective"});
    form->addRow("Kind", kindCombo_);

    orderSpin_ = new QSpinBox(this);
    orderSpin_->setObjectName("orderSpin");
    // 2..Flame::kMaxXForms - order 1 is a no-op (see Symmetry.h), and
    // beyond kMaxXForms addSymmetry can't append anything more anyway.
    orderSpin_->setRange(2, apo::Flame::kMaxXForms);
    orderSpin_->setValue(4);
    form->addRow("Order", orderSpin_);

    rootLayout->addLayout(form);

    auto* buttonRow = new QHBoxLayout();
    auto* applyButton = new QPushButton("Apply", this);
    applyButton->setObjectName("applyButton");
    auto* closeButton = new QPushButton("Close", this);
    closeButton->setObjectName("closeButton");
    buttonRow->addStretch(1);
    buttonRow->addWidget(applyButton);
    buttonRow->addWidget(closeButton);
    rootLayout->addLayout(buttonRow);

    connect(applyButton, &QPushButton::clicked, this, &ForceSymmetryDialog::onApply);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);
}

void ForceSymmetryDialog::onApply() {
    // Kind index 0 (Rotational) -> +order; index 1 (Rotational +
    // Reflective) -> -order, matching Symmetry.h's sign convention.
    const int order = orderSpin_->value();
    const int sym = kindCombo_->currentIndex() == 1 ? -order : order;
    emit applyRequested(sym);
}

void ForceSymmetryDialog::setAutoScreenshot(const QString& path, bool exitAfter) {
    autoScreenshotPath_ = path;
    autoScreenshotExit_ = exitAfter;
}

void ForceSymmetryDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    if (autoScreenshotFired_ || autoScreenshotPath_.isEmpty()) return;
    autoScreenshotFired_ = true;
    const QString path = autoScreenshotPath_;
    const bool exitAfter = autoScreenshotExit_;
    QTimer::singleShot(100, this, [this, path, exitAfter] {
        grab().save(path, "PNG");
        if (exitAfter) qApp->quit();
    });
}

} // namespace apo::ui
