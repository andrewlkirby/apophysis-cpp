// Real-widget interaction test for EditorWindow's progress readout: the
// quality-dropdown-triggered preview render (onQualityBoxCommitted) now
// tracks progress the same way MainWindow's own full-quality preview render
// does (see MainWindow::onProgressTick and
// main_window_interaction_test.cpp's testMainPreviewShowsProgressDuringAFullQualityRender,
// which this test originally mirrored), rather than the plain "Rendering..."
// with no percent every other EditorWindow render (triangle drags, live
// property edits, ...) still shows - those stay untracked on purpose, see
// EditorWindow::requestRender()'s doc comment.
//
// Deliberately does NOT tune a flame/density/canvas size to reliably take
// some specific amount of wall-clock time the way
// testMainPreviewShowsProgressDuringAFullQualityRender does - that approach
// isn't portable across machines: measured directly, the same render took
// ~1.3s on a CUDA-capable dev machine and still hadn't finished after 15s on
// a CI runner's CPU-only fallback, so no fixed density is simultaneously
// "slow enough to observe a progress tick" and "fast enough not to time
// out" across every machine this suite runs on. Instead this pauses the
// render deterministically via EditorWindow::currentRenderProgressForTesting()
// - the same idea as RenderDialog's *real* Pause button letting its own
// tests sidestep this exact problem (see render_dialog_interaction_test.cpp's
// testPauseFreezesProgressUntilResumed) - so this test's own runtime is
// bounded and independent of how fast the render itself actually is.

#include <functional>
#include <memory>

#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QStatusBar>
#include <QTest>

#include "../TestHelpers.h"
#include "AppSettings.h"
#include "EditorWindow.h"
#include "core/Flame.h"

using apo_test::check;

namespace {

bool waitUntil(const std::function<bool()>& pred, int timeoutMs = 10000) {
    QElapsedTimer timer;
    timer.start();
    while (!pred()) {
        if (timer.elapsed() > timeoutMs) return false;
        QTest::qWait(20);
    }
    return true;
}

std::shared_ptr<apo::Flame> makeTestFlame() {
    auto flame = std::make_shared<apo::Flame>();
    flame->name = "editor progress test flame";
    flame->width = 150;
    flame->height = 150;
    apo::XForm& xf = *flame->xform[0];
    xf.clear();
    xf.density = 1.0;
    return flame;
}

void testQualityDropdownRenderShowsProgress() {
    const double savedPreviewDensity = apo::ui::AppSettings::previewSampleDensity();

    auto* editor = new apo::ui::EditorWindow(makeTestFlame());
    editor->show();

    // Let the constructor's own initial preview render (untracked - see
    // requestRender()'s doc comment) finish first, so it can't be mistaken
    // for the tracked one below.
    const bool initialSettled =
        waitUntil([editor] { return !editor->statusBar()->currentMessage().contains("Rendering"); });
    check(initialSettled, "test setup: the Editor's initial preview render completes");

    auto* qualityBox = editor->findChild<QComboBox*>("qualityBox");
    if (!check(qualityBox != nullptr, "EditorWindow exposes its quality-dropdown combo box as 'qualityBox'")) {
        delete editor;
        apo::ui::AppSettings::setPreviewSampleDensity(savedPreviewDensity);
        return;
    }

    // Retries the whole commit-and-pause cycle rather than a single
    // grab-and-pause: EditorWindow can still have an unrelated, untracked
    // render winding down from the window's own initial layout settling
    // (resizeEvent() re-requests as the widget's geometry catches up to its
    // final size, same as every other EditorWindow render source - see
    // requestRender()'s doc comment). If this attempt's commit lands while
    // that's still in flight, requestRender() takes its renderDirty_ branch
    // instead of starting immediately (see its own doc comment) - the real
    // tracked render then only actually starts later, from inside
    // onRenderFinished(), during this attempt's own QTest::qWait() below,
    // and can finish entirely within that same wait on a fast machine,
    // before ever getting caught. onQualityBoxCommitted() is idempotent (it
    // just re-parses the box's current text and re-requests with tracking
    // on - no need to pick/tune a specific density value here), so simply
    // re-issuing the commit on the next attempt is safe, and by then any
    // earlier churn has long settled, making the "no event-loop turn
    // between triggering and pausing" trick below land reliably - the same
    // trick render_dialog_interaction_test.cpp's
    // testPauseFreezesProgressUntilResumed relies on to pause before the
    // worker thread's first per-sub-batch check, deterministically,
    // regardless of how fast the render itself would otherwise be.
    bool sawProgress = false;
    for (int attempt = 0; attempt < 10 && !sawProgress; ++attempt) {
        // Same signal a real Return keypress in the (editable) combo's line
        // edit fires - see the constructor's own
        // connect(qualityBox_->lineEdit(), &QLineEdit::editingFinished, ...).
        emit qualityBox->lineEdit()->editingFinished();

        if (auto* progress = editor->currentRenderProgressForTesting()) {
            progress->pauseRequested.store(true, std::memory_order_relaxed);
        }

        QTest::qWait(200); // > progressTimer_'s 150ms interval, so a tick lands while paused
        const QString msg = editor->statusBar()->currentMessage();
        sawProgress = msg.contains("Rendering...") && msg.contains("%");

        if (!sawProgress) {
            // Didn't land this attempt - make sure nothing is left paused
            // (see currentRenderProgressForTesting()'s own doc comment on
            // why this is re-fetched rather than reusing the pointer above)
            // before the next attempt's fresh commit.
            if (auto* progress = editor->currentRenderProgressForTesting()) {
                progress->pauseRequested.store(false, std::memory_order_relaxed);
            }
        }
    }
    check(sawProgress,
          "committing the Editor's quality dropdown shows a live percent/elapsed/remaining status, matching "
          "MainWindow's own full-quality preview readout");

    // Resume whatever's currently paused so the render can actually finish -
    // re-fetched fresh rather than reusing a pointer from the loop above:
    // never hold a RenderProgress pointer across a wait, since
    // onRenderFinished() destroys the old one and requestRender() can
    // allocate a new one at any point on the UI thread (see
    // currentRenderProgressForTesting()'s own doc comment).
    if (auto* progress = editor->currentRenderProgressForTesting()) {
        progress->pauseRequested.store(false, std::memory_order_relaxed);
    }

    const bool settled = waitUntil([editor] { return editor->statusBar()->currentMessage().contains("Rendered"); }, 15000);
    check(settled, "the status bar still reports the final \"Rendered...\" message once the render completes");

    delete editor;
    apo::ui::AppSettings::setPreviewSampleDensity(savedPreviewDensity);
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    // Required for AppSettings (QSettings-backed) to resolve a real storage
    // location - without these, QSettings silently no-ops every read/write
    // (see AppSettings.h) - same setup main_window_interaction_test.cpp's
    // own main() uses.
    QApplication::setApplicationName("Apophysis 7X");
    QApplication::setOrganizationName("Apophysis 7X");

    testQualityDropdownRenderShowsProgress();

    return apo_test::reportAndExit();
}
