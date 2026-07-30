// Real-widget interaction test for EditorWindow's progress readout: the
// quality-dropdown-triggered preview render (onQualityBoxCommitted) now
// tracks progress the same way MainWindow's own full-quality preview render
// does (see MainWindow::onProgressTick and
// main_window_interaction_test.cpp's testMainPreviewShowsProgressDuringAFullQualityRender,
// which this test mirrors), rather than the plain "Rendering..." with no
// percent every other EditorWindow render (triangle drags, live property
// edits, ...) still shows - those stay untracked on purpose, see
// EditorWindow::requestRender()'s doc comment.

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

    // Dense enough, against this window's own real on-screen canvas size
    // (~570x620 for EditorWindow's default 1000x700 layout - much larger
    // than main_window_interaction_test.cpp's fixed 150x150 fixture, so a
    // far smaller density multiple gets the same real point-count margin;
    // measured empirically - the default "10" preset's own render already
    // produces several million points here), that the render takes a real,
    // perceptible amount of time - see main_window_interaction_test.cpp's
    // identical-purpose comment on why a too-fast render can finish before
    // onProgressTick()'s ~150ms poll ever lands a single tick. Deliberately
    // NOT pushed much higher: EditorWindow's destructor blocks on
    // workerThread_->wait() for whatever render is still in flight, so an
    // overly heavy density here would hang this test's own cleanup, not
    // just slow down the render.
    qualityBox->setCurrentText("150");
    // Same signal a real Return keypress in the (editable) combo's line
    // edit fires - see the constructor's own
    // connect(qualityBox_->lineEdit(), &QLineEdit::editingFinished, ...).
    emit qualityBox->lineEdit()->editingFinished();

    const bool sawProgress = waitUntil(
        [editor] {
            const QString msg = editor->statusBar()->currentMessage();
            return msg.contains("Rendering...") && msg.contains("%");
        },
        5000);
    check(sawProgress,
          "committing the Editor's quality dropdown shows a live percent/elapsed/remaining status, matching "
          "MainWindow's own full-quality preview readout");

    const bool settled = waitUntil([editor] { return editor->statusBar()->currentMessage().contains("Rendered"); }, 15000);
    check(settled, "the status bar still reports the final \"Rendered...\" message once the render completes");

    delete editor;
    apo::ui::AppSettings::setPreviewSampleDensity(savedPreviewDensity);
}

} // namespace

int main(int argc, char** argv) {
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
