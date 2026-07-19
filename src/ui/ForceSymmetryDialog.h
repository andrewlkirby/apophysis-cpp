#pragma once

#include <memory>

#include <QDialog>
#include <QString>

#include "core/Flame.h"

class QComboBox;
class QSpinBox;
class QShowEvent;

namespace apo::ui {

// A small on-demand front-end for core/edit/Symmetry.h's addSymmetry() -
// see that header's doc comment for why this is a direct Editor action in
// this port rather than only a random-flame-generation option like the
// original (Main.pas/RndFlame.pas's SymmetryType/SymmetryOrder). Kept
// deliberately tiny: a Kind combo (Rotational / Rotational + Reflective)
// and an Order spinner, collapsing to the single signed `sym` integer
// addSymmetry() takes, plus Apply/Close - no live preview of its own
// (matches XaosDialog/CurvesDialog's own reasoning: the owning
// EditorWindow already shows one, refreshed via flameChanged).
class ForceSymmetryDialog final : public QDialog {
    Q_OBJECT

public:
    explicit ForceSymmetryDialog(std::shared_ptr<apo::Flame> flame, QWidget* parent = nullptr);

    void setAutoScreenshot(const QString& path, bool exitAfter);

protected:
    void showEvent(QShowEvent* event) override;

signals:
    // Emitted when Apply is clicked, carrying the single signed `sym`
    // value the Kind/Order controls collapse to (see Symmetry.h's doc
    // comment on the sign convention) - deliberately does NOT mutate
    // flame_ itself. EditorWindow owns the actual apo::addSymmetry() call
    // and structural-undo bookkeeping (the same pattern
    // onAddXform/onDuplicateXform/onDeleteXform already use), so a before-
    // snapshot can be taken *before* the mutation happens - something this
    // dialog can't do retroactively after the fact.
    void applyRequested(int sym);

private:
    void onApply();

    std::shared_ptr<apo::Flame> flame_; // held only for the window title; never mutated here
    QComboBox* kindCombo_ = nullptr;
    QSpinBox* orderSpin_ = nullptr;

    QString autoScreenshotPath_;
    bool autoScreenshotExit_ = false;
    bool autoScreenshotFired_ = false;
};

} // namespace apo::ui
