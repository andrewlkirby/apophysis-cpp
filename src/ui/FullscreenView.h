#pragma once

#include <memory>

#include <QElapsedTimer>
#include <QImage>
#include <QWidget>

#include "core/Flame.h"
#include "core/render/Renderer.h"

class QThread;
class QLabel;
class QShowEvent;
class QKeyEvent;
class QMouseEvent;
class QContextMenuEvent;

namespace apo::ui {

class RenderWorker;

// Port of Fullscreen.pas (343 lines) - shows the current flame rendered
// full-screen, borderless. Deliberately scoped down per the "good
// performance, not a faithful copy" project direction:
//   - No "Render More" (the original's RenderStop/RenderMore pair resumes
//     a paused render, accumulating more samples into the same buckets -
//     this port's Renderer has no incremental-accumulation support, the
//     same gap RenderDialog already accepted; Cancel here is a one-shot
//     stop, not a pause). The right-click menu offers Cancel (while
//     rendering) and Close, not Stop/Resume/Exit.
//   - No taskbar hide/restore (FindWindow(Shell_TrayWnd)-based Win32
//     calls) - showFullScreen() already covers the "no window chrome"
//     goal without reaching around Qt into raw Win32 APIs.
//   - Any key closes (not just non-space keys) - simpler than replicating
//     space's original pause/resume-specific meaning, which doesn't apply
//     without a resumable render.
// Reuses the exact RenderWorker::renderFull/RenderProgress machinery
// RenderDialog already established, rendered at the primary screen's
// resolution rather than a user-chosen size.
class FullscreenView final : public QWidget {
    Q_OBJECT

public:
    explicit FullscreenView(std::shared_ptr<apo::Flame> flame, QWidget* parent = nullptr);
    ~FullscreenView() override;

    // Testing/verification hook, same idea as every other dialog's
    // setAutoScreenshot - fires once the initial render completes (this
    // view has nothing else to "settle").
    void setAutoScreenshot(const QString& path, bool exitAfter);

signals:
    void renderRequested(std::shared_ptr<const apo::Flame> flame, quint64 seed, int threadCount,
                          apo::RenderProgress* progress, QString outputPath);

protected:
    void showEvent(QShowEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void onRenderFinished(QImage image, quint64 pointsGenerated, quint64 pointsAccepted, bool cancelled, bool saved);
    void onProgressTick();

private:
    void startRender();

    std::shared_ptr<apo::Flame> flame_;
    bool renderStarted_ = false;

    QLabel* imageLabel_ = nullptr;
    QLabel* statusLabel_ = nullptr;

    QThread* workerThread_ = nullptr;
    RenderWorker* worker_ = nullptr;
    std::unique_ptr<apo::RenderProgress> progress_;
    class QTimer* progressTimer_ = nullptr;
    QElapsedTimer elapsedTimer_;

    QString autoScreenshotPath_;
    bool autoScreenshotExit_ = false;
};

} // namespace apo::ui
