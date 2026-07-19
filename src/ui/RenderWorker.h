#pragma once

#include <cstdint>
#include <exception>
#include <memory>

#include <QDebug>
#include <QImage>
#include <QObject>
#include <QString>

#include "core/Flame.h"
#include "core/io/PngWriter.h"
#include "core/render/Renderer.h"

namespace apo::ui {

// Runs apo::Renderer::render() (a blocking call that can take anywhere
// from milliseconds to several seconds depending on sample density/canvas
// size) on a dedicated QThread, so MainWindow's UI thread stays responsive
// while a render is in flight. Lives on that thread via moveToThread() in
// MainWindow's constructor - render() is invoked through renderRequested's
// queued connection, never called directly.
class RenderWorker final : public QObject {
    Q_OBJECT

public:
    explicit RenderWorker(QObject* parent = nullptr) : QObject(parent) {}

public slots:
    // `flame` is a shared_ptr (not a reference) specifically so ownership
    // safely crosses the queued-connection thread boundary - Qt copies the
    // signal's arguments into an event posted to the worker thread, and a
    // plain `const Flame&` would dangle if the caller's Flame were
    // destroyed before the worker thread gets around to processing it.
    // BucketPrecision::Float (FOLLOWUP_PLAN.txt B5) - this is exactly the
    // "low density, latency-sensitive" preview path the plan calls out by
    // name: AdjustDialog/CurvesDialog/EditorWindow/MutateDialog/
    // PostProcessDialog's live-drag previews, all wired to this slot (not
    // renderFlameWithProgress or renderFull below, both of which stay at
    // the BucketPrecision::Double default - a full-quality render keeps
    // full precision).
    void renderFlame(std::shared_ptr<const apo::Flame> flame, quint64 seed) {
        // A render can legitimately throw (most realistically std::bad_alloc,
        // if a large canvas's bucket allocation collides with whatever else
        // is using memory right now) - uncaught, that exception unwinds out
        // of this slot on the worker QThread's event loop, which has no
        // handler for it either, and the process calls std::terminate()
        // (confirmed against a real user-reported crash dump: the crashing
        // thread's stack showed exactly this - terminate/_CxxFrameHandler4
        // near a thread-entry frame, not memory corruption). Every render
        // path here is wrapped for the same reason: turn "render failed" into
        // a normal finished-with-no-image signal instead of a whole-app crash.
        try {
            const apo::RenderedImage image =
                apo::Renderer::render(*flame, seed, /*threadCount=*/0, /*progress=*/nullptr, /*timings=*/nullptr,
                                       apo::BucketPrecision::Float);
            emit renderFinished(toQImage(image), image.stats.pointsGenerated, image.stats.pointsAccepted);
        } catch (const std::exception& e) {
            qWarning() << "RenderWorker::renderFlame failed:" << e.what();
            emit renderFinished(QImage(), 0, 0);
        } catch (...) {
            qWarning() << "RenderWorker::renderFlame failed with an unrecognized exception";
            emit renderFinished(QImage(), 0, 0);
        }
    }

    // Identical to renderFlame() above, plus progress reporting - a
    // *separate* slot rather than an extra defaulted parameter on
    // renderFlame() itself, because Qt's connect() requires a slot's
    // parameter count to be <= the signal's (a default argument doesn't
    // change the slot's actual function-pointer type, so widening
    // renderFlame() itself would break every other window's own 2-argument
    // renderRequested(flame, seed) signal already connected to it -
    // confirmed the hard way, not assumed: it fails to compile). Only
    // MainWindow's own renderRequested carries a progress pointer (plan's
    // P2.5 - see MainWindow.h's own doc comment on why only its full-
    // quality preview render wants one), so only it connects here.
    //
    // `progress` is a raw pointer, not owned by this call - same
    // caller-owned contract as renderFull()'s below; nullptr is fine (no
    // progress reporting).
    void renderFlameWithProgress(std::shared_ptr<const apo::Flame> flame, quint64 seed, apo::RenderProgress* progress) {
        try {
            const apo::RenderedImage image = apo::Renderer::render(*flame, seed, /*threadCount=*/0, progress);
            emit renderFinished(toQImage(image), image.stats.pointsGenerated, image.stats.pointsAccepted);
        } catch (const std::exception& e) {
            qWarning() << "RenderWorker::renderFlameWithProgress failed:" << e.what();
            emit renderFinished(QImage(), 0, 0);
        } catch (...) {
            qWarning() << "RenderWorker::renderFlameWithProgress failed with an unrecognized exception";
            emit renderFinished(QImage(), 0, 0);
        }
    }

    // A full-quality render for the Render dialog, as opposed to
    // renderFlame()'s fixed-low-density preview: a caller-chosen
    // threadCount (0 = use all hardware threads - see AppSettings::
    // renderThreadCount(), OptionsDialog's Performance group) and an
    // optional RenderProgress token the caller polls from the UI thread
    // for a progress bar / Cancel button, plus an optional save-to-PNG
    // step run here (not back on the UI thread) so a large final image's
    // disk write doesn't block the UI either.
    //
    // `progress` is a raw pointer, not owned by this call - the caller
    // (RenderDialog) owns it and must keep it alive until fullRenderFinished
    // is delivered back; nullptr is fine (no progress reporting).
    void renderFull(std::shared_ptr<const apo::Flame> flame, quint64 seed, int threadCount,
                     apo::RenderProgress* progress, QString outputPath) {
        // Note: a cancelled render already reaches this point normally (the
        // early-break in Renderer::render() just stops feeding more points
        // into the buckets already accumulated) and gets tone-mapped and
        // saved exactly like a completed one - `saved` below isn't gated on
        // `cancelled` at all, so Cancel already produces a real partial PNG
        // on disk, not a discarded render. This try/catch is a separate
        // concern: a genuinely thrown exception (see renderFlame()'s comment
        // on why - same reasoning applies here) skips straight past the
        // save step, and without a handler would crash the whole app instead
        // of just failing this one render.
        try {
            const apo::RenderedImage image = apo::Renderer::render(*flame, seed, threadCount, progress);

            bool saved = true;
            if (!outputPath.isEmpty() && !image.pixels.empty()) {
                saved = apo::writePng(outputPath.toStdString(), image.width, image.height, image.channels,
                                       image.pixels.data());
            }

            const bool cancelled = progress && progress->cancelRequested.load(std::memory_order_relaxed);
            emit fullRenderFinished(toQImage(image), image.stats.pointsGenerated, image.stats.pointsAccepted,
                                     cancelled, saved);
        } catch (const std::exception& e) {
            qWarning() << "RenderWorker::renderFull failed:" << e.what();
            const bool cancelled = progress && progress->cancelRequested.load(std::memory_order_relaxed);
            emit fullRenderFinished(QImage(), 0, 0, cancelled, /*saved=*/false);
        } catch (...) {
            qWarning() << "RenderWorker::renderFull failed with an unrecognized exception";
            const bool cancelled = progress && progress->cancelRequested.load(std::memory_order_relaxed);
            emit fullRenderFinished(QImage(), 0, 0, cancelled, /*saved=*/false);
        }
    }

signals:
    void renderFinished(QImage image, quint64 pointsGenerated, quint64 pointsAccepted);
    void fullRenderFinished(QImage image, quint64 pointsGenerated, quint64 pointsAccepted, bool cancelled, bool saved);

private:
    // QImage::Format_RGB888/RGBA8888 both expect exactly the row-major,
    // top-to-bottom, tightly-packed byte layout RenderedImage::pixels
    // already is (see Renderer.h) - constructed with a deep copy (.copy())
    // since `image.pixels` is a local about to go out of scope in every
    // caller, and QImage's own raw-data constructor doesn't take ownership
    // of the buffer it's given. Shared by all three render slots above.
    static QImage toQImage(const apo::RenderedImage& image) {
        if (image.pixels.empty()) return {};
        const QImage::Format format = image.channels == 4 ? QImage::Format_RGBA8888 : QImage::Format_RGB888;
        return QImage(image.pixels.data(), image.width, image.height, image.width * image.channels, format).copy();
    }
};

} // namespace apo::ui
