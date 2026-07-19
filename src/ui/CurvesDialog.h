#pragma once

#include <array>
#include <memory>

#include <QDialog>
#include <QImage>
#include <QString>

#include "core/Bezier.h"
#include "core/Flame.h"

class QThread;
class QComboBox;
class QLabel;
class QPushButton;
class QResizeEvent;

namespace apo::ui {

class RenderWorker;
class SliderSpin;
class CurvesWidget;

// Port of Forms/Curves.pas (+ System/CurvesControl.pas for the draggable
// graph itself - see CurvesWidget.h) - a Bezier tone-curve editor for the
// All/Red/Green/Blue channels. Scoped down per the "good performance, not a
// faithful copy" project direction: the original doesn't have its own
// undo/redo stack for this dialog either (its OK/Cancel-less edits apply
// straight to MainCp, undoable only via MainForm's own separate global
// Undo), so this port replaces that with simple per-channel Reset / Reset
// All buttons rather than porting a second dialog-local undo stack.
class CurvesDialog final : public QDialog {
    Q_OBJECT

public:
    explicit CurvesDialog(std::shared_ptr<apo::Flame> flame, QWidget* parent = nullptr);
    ~CurvesDialog() override;

    void setAutoScreenshot(const QString& path, bool exitAfter);

protected:
    void resizeEvent(QResizeEvent* event) override;

signals:
    // Emitted after every edit so an owning window (EditorWindow) can
    // refresh its own view of the same shared Flame.
    void flameChanged();
    void renderRequested(std::shared_ptr<const apo::Flame> flame, quint64 seed);

private slots:
    void onRenderFinished(QImage image, quint64 pointsGenerated, quint64 pointsAccepted);

private:
    void onChannelChanged(int index);
    void onCurveChanged();
    void onWeightChanged(int pointIndex, double value);
    void onReset();
    void onResetAll();
    void refreshWeightControls();
    void requestPreviewRender();

    std::shared_ptr<apo::Flame> flame_;
    std::array<apo::BezierCurve, 4> initialCurves_; // for Reset/Reset All

    QComboBox* channelCombo_ = nullptr;
    CurvesWidget* curvesWidget_ = nullptr;
    SliderSpin* weight1Ctrl_ = nullptr;
    SliderSpin* weight2Ctrl_ = nullptr;
    QPushButton* resetButton_ = nullptr;
    QPushButton* resetAllButton_ = nullptr;
    QLabel* previewLabel_ = nullptr;

    QThread* workerThread_ = nullptr;
    RenderWorker* worker_ = nullptr;
    bool renderInFlight_ = false;
    bool renderDirty_ = false;

    QString autoScreenshotPath_;
    bool autoScreenshotExit_ = false;
};

} // namespace apo::ui
