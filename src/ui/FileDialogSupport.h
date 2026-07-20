#pragma once

#include <QFileDialog>
#include <QGuiApplication>

namespace apo::ui {

// Every QFileDialog::getSaveFileName/getOpenFileName call site in this
// codebase should pass this as its trailing `options` argument.
//
// Under the real desktop app this is always QFileDialog::Options() (no
// change to the native picker Linux/macOS users get, which is genuinely
// nicer than Qt's own dialog, and no change to the normal "overwrite this
// file?" confirmation a real user should still see) - but under the
// "offscreen" QPA platform (every UI test in tests/ui/*.cpp, via
// qputenv("QT_QPA_PLATFORM", "offscreen") in each test's main()) it forces
// two things:
//
//   - DontUseNativeDialog: ensures getSaveFileName/getOpenFileName actually
//     constructs Qt's own QFileDialog widget instance, which every test's
//     modal-dialog interception helper (see e.g.
//     tests/ui/main_window_interaction_test.cpp's acceptNextSaveDialogWith)
//     depends on being able to find via QApplication::activeModalWidget().
//   - DontConfirmOverwrite: THE fix for a real, reproduced deadlock, root-
//     caused via gdb (`thread apply all bt` on the hung process) rather
//     than guessed - Thread 1's backtrace showed QFileDialog::accept()
//     itself opening a *second*, nested QDialog::exec() (Qt's built-in
//     "A file named X already exists. Do you want to replace it?" prompt,
//     shown whenever the selected save path already exists on disk) that
//     nothing ever answers, since it's a different widget class than the
//     QFileDialog every test's interception helper polls for - the process
//     then blocks in poll() forever. This is a real risk in a test suite:
//     many tests reuse similar output filenames run over run, and a
//     dialog's own suggested-from-flame-name default can coincide with a
//     leftover file from a previous run. Confirmed fixed by disabling the
//     prompt outright for the offscreen/test environment, where "may this
//     overwrite an existing file" is never a real question - every test
//     already treats its own output path as disposable.
inline QFileDialog::Options testFriendlyFileDialogOptions() {
    return QGuiApplication::platformName() == QLatin1String("offscreen")
               ? (QFileDialog::DontUseNativeDialog | QFileDialog::DontConfirmOverwrite)
               : QFileDialog::Options();
}

} // namespace apo::ui
