#include <cstdio>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>

#include "AdjustDialog.h"
#include "CurvesDialog.h"
#include "EditorWindow.h"
#include "ForceSymmetryDialog.h"
#include "PostProcessDialog.h"
#include "XaosDialog.h"
#include "FullscreenView.h"
#include "GradientBrowserDialog.h"
#include "MainWindow.h"
#include "MutateDialog.h"
#include "OptionsDialog.h"
#include "RenderAllDialog.h"
#include "RenderDialog.h"
#include "SmoothPaletteDialog.h"
#include "core/io/FlameIO.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Apophysis 7X");
    QApplication::setOrganizationName("Apophysis 7X");

    QCommandLineParser parser;
    parser.setApplicationDescription("Apophysis 7X - fractal flame editor");
    parser.addHelpOption();
    parser.addPositionalArgument("flame", "A .flame/.flam3 file to open on startup (optional)");

    // Verification hooks, not normal user options - see
    // MainWindow::setAutoScreenshot's comment. Undocumented on purpose
    // (addOption without addHelpOption visibility would still show it, so
    // this is just an ordinary hidden-by-convention debug flag, not
    // enforced-hidden).
    QCommandLineOption screenshotOption("auto-screenshot-exit", "Render, screenshot to <path>, then exit.", "path");
    parser.addOption(screenshotOption);
    QCommandLineOption editOption("edit", "Open the given flame directly in the Editor window instead of the "
                                           "flame-library Main window.");
    parser.addOption(editOption);
    QCommandLineOption adjustOption(
        "adjust", "Open the given flame directly in the Adjust dialog instead of the flame-library Main window.");
    parser.addOption(adjustOption);
    QCommandLineOption renderOption(
        "render", "Open the given flame directly in the Render dialog instead of the flame-library Main window.");
    parser.addOption(renderOption);
    QCommandLineOption renderAllOption(
        "render-all", "Open every flame in the given file directly in the Render All dialog instead of the "
                       "flame-library Main window.");
    parser.addOption(renderAllOption);
    QCommandLineOption optionsOption(
        "options", "Open the Options dialog instead of the flame-library Main window (no flame file needed).");
    parser.addOption(optionsOption);
    QCommandLineOption mutateOption(
        "mutate", "Open the given flame directly in the Mutate dialog instead of the flame-library Main window.");
    parser.addOption(mutateOption);
    QCommandLineOption gradientBrowserOption(
        "gradient-browser", "Open the Gradient Browser dialog instead of the flame-library Main window (the "
                             "positional argument, if given, is a .ugr/.gradient/.map file to open immediately).");
    parser.addOption(gradientBrowserOption);
    QCommandLineOption fullscreenOption(
        "fullscreen", "Open the given flame directly in the Fullscreen view instead of the flame-library Main "
                       "window.");
    parser.addOption(fullscreenOption);
    QCommandLineOption curvesOption(
        "curves", "Open the given flame directly in the Curves dialog instead of the flame-library Main window.");
    parser.addOption(curvesOption);
    QCommandLineOption postProcessOption(
        "post-process",
        "Open the given flame directly in the Post Process dialog instead of the flame-library Main window.");
    parser.addOption(postProcessOption);
    QCommandLineOption xaosOption(
        "xaos", "Open the given flame directly in the Xaos dialog instead of the flame-library Main window.");
    parser.addOption(xaosOption);
    QCommandLineOption forceSymmetryOption(
        "force-symmetry",
        "Open the given flame directly in the Force Symmetry dialog instead of the flame-library Main window.");
    parser.addOption(forceSymmetryOption);
    QCommandLineOption smoothPaletteOption(
        "smooth-palette", "Open the Smooth Palette dialog instead of the flame-library Main window (no flame file "
                           "needed - the positional argument, if given, is an image file to open immediately).");
    parser.addOption(smoothPaletteOption);

    parser.process(app);

    const QStringList positional = parser.positionalArguments();

    if (parser.isSet(editOption)) {
        if (positional.isEmpty()) {
            fprintf(stderr, "--edit requires a flame file argument\n");
            return 2;
        }
        auto flames = apo::loadFlameFile(positional.first().toStdString());
        if (flames.empty()) {
            fprintf(stderr, "could not load any <flame> from '%s'\n", qPrintable(positional.first()));
            return 1;
        }
        auto* editor = new apo::ui::EditorWindow(std::shared_ptr<apo::Flame>(std::move(flames.front())));
        if (parser.isSet(screenshotOption)) {
            editor->setAutoScreenshot(parser.value(screenshotOption), /*exitAfter=*/true);
        }
        editor->show();
        return app.exec();
    }

    if (parser.isSet(adjustOption)) {
        if (positional.isEmpty()) {
            fprintf(stderr, "--adjust requires a flame file argument\n");
            return 2;
        }
        auto flames = apo::loadFlameFile(positional.first().toStdString());
        if (flames.empty()) {
            fprintf(stderr, "could not load any <flame> from '%s'\n", qPrintable(positional.first()));
            return 1;
        }
        auto* dialog = new apo::ui::AdjustDialog(std::shared_ptr<apo::Flame>(std::move(flames.front())));
        if (parser.isSet(screenshotOption)) {
            dialog->setAutoScreenshot(parser.value(screenshotOption), /*exitAfter=*/true);
        }
        dialog->show();
        return app.exec();
    }

    if (parser.isSet(renderOption)) {
        if (positional.isEmpty()) {
            fprintf(stderr, "--render requires a flame file argument\n");
            return 2;
        }
        auto flames = apo::loadFlameFile(positional.first().toStdString());
        if (flames.empty()) {
            fprintf(stderr, "could not load any <flame> from '%s'\n", qPrintable(positional.first()));
            return 1;
        }
        auto* dialog = new apo::ui::RenderDialog(std::shared_ptr<apo::Flame>(std::move(flames.front())));
        if (parser.isSet(screenshotOption)) {
            dialog->setAutoScreenshot(parser.value(screenshotOption), /*exitAfter=*/true);
        }
        dialog->show();
        return app.exec();
    }

    if (parser.isSet(renderAllOption)) {
        if (positional.isEmpty()) {
            fprintf(stderr, "--render-all requires a flame file argument\n");
            return 2;
        }
        auto flames = apo::loadFlameFile(positional.first().toStdString());
        if (flames.empty()) {
            fprintf(stderr, "could not load any <flame> from '%s'\n", qPrintable(positional.first()));
            return 1;
        }
        std::vector<std::shared_ptr<apo::Flame>> sharedFlames;
        sharedFlames.reserve(flames.size());
        for (auto& f : flames) sharedFlames.push_back(std::shared_ptr<apo::Flame>(std::move(f)));
        auto* dialog = new apo::ui::RenderAllDialog(std::move(sharedFlames));
        if (parser.isSet(screenshotOption)) {
            dialog->setAutoScreenshot(parser.value(screenshotOption), /*exitAfter=*/true);
        }
        dialog->show();
        return app.exec();
    }

    if (parser.isSet(mutateOption)) {
        if (positional.isEmpty()) {
            fprintf(stderr, "--mutate requires a flame file argument\n");
            return 2;
        }
        auto flames = apo::loadFlameFile(positional.first().toStdString());
        if (flames.empty()) {
            fprintf(stderr, "could not load any <flame> from '%s'\n", qPrintable(positional.first()));
            return 1;
        }
        auto* dialog = new apo::ui::MutateDialog(std::shared_ptr<apo::Flame>(std::move(flames.front())));
        if (parser.isSet(screenshotOption)) {
            dialog->setAutoScreenshot(parser.value(screenshotOption), /*exitAfter=*/true);
        }
        dialog->show();
        return app.exec();
    }

    if (parser.isSet(fullscreenOption)) {
        if (positional.isEmpty()) {
            fprintf(stderr, "--fullscreen requires a flame file argument\n");
            return 2;
        }
        auto flames = apo::loadFlameFile(positional.first().toStdString());
        if (flames.empty()) {
            fprintf(stderr, "could not load any <flame> from '%s'\n", qPrintable(positional.first()));
            return 1;
        }
        auto* view = new apo::ui::FullscreenView(std::shared_ptr<apo::Flame>(std::move(flames.front())));
        if (parser.isSet(screenshotOption)) {
            view->setAutoScreenshot(parser.value(screenshotOption), /*exitAfter=*/true);
        }
        view->showFullScreen();
        return app.exec();
    }

    if (parser.isSet(curvesOption)) {
        if (positional.isEmpty()) {
            fprintf(stderr, "--curves requires a flame file argument\n");
            return 2;
        }
        auto flames = apo::loadFlameFile(positional.first().toStdString());
        if (flames.empty()) {
            fprintf(stderr, "could not load any <flame> from '%s'\n", qPrintable(positional.first()));
            return 1;
        }
        auto* dialog = new apo::ui::CurvesDialog(std::shared_ptr<apo::Flame>(std::move(flames.front())));
        if (parser.isSet(screenshotOption)) {
            dialog->setAutoScreenshot(parser.value(screenshotOption), /*exitAfter=*/true);
        }
        dialog->show();
        return app.exec();
    }

    if (parser.isSet(postProcessOption)) {
        if (positional.isEmpty()) {
            fprintf(stderr, "--post-process requires a flame file argument\n");
            return 2;
        }
        auto flames = apo::loadFlameFile(positional.first().toStdString());
        if (flames.empty()) {
            fprintf(stderr, "could not load any <flame> from '%s'\n", qPrintable(positional.first()));
            return 1;
        }
        auto* dialog = new apo::ui::PostProcessDialog(std::shared_ptr<apo::Flame>(std::move(flames.front())));
        if (parser.isSet(screenshotOption)) {
            dialog->setAutoScreenshot(parser.value(screenshotOption), /*exitAfter=*/true);
        }
        dialog->show();
        return app.exec();
    }

    if (parser.isSet(xaosOption)) {
        if (positional.isEmpty()) {
            fprintf(stderr, "--xaos requires a flame file argument\n");
            return 2;
        }
        auto flames = apo::loadFlameFile(positional.first().toStdString());
        if (flames.empty()) {
            fprintf(stderr, "could not load any <flame> from '%s'\n", qPrintable(positional.first()));
            return 1;
        }
        auto* dialog = new apo::ui::XaosDialog(std::shared_ptr<apo::Flame>(std::move(flames.front())));
        if (parser.isSet(screenshotOption)) {
            dialog->setAutoScreenshot(parser.value(screenshotOption), /*exitAfter=*/true);
        }
        dialog->show();
        return app.exec();
    }

    if (parser.isSet(forceSymmetryOption)) {
        if (positional.isEmpty()) {
            fprintf(stderr, "--force-symmetry requires a flame file argument\n");
            return 2;
        }
        auto flames = apo::loadFlameFile(positional.first().toStdString());
        if (flames.empty()) {
            fprintf(stderr, "could not load any <flame> from '%s'\n", qPrintable(positional.first()));
            return 1;
        }
        auto* dialog = new apo::ui::ForceSymmetryDialog(std::shared_ptr<apo::Flame>(std::move(flames.front())));
        if (parser.isSet(screenshotOption)) {
            dialog->setAutoScreenshot(parser.value(screenshotOption), /*exitAfter=*/true);
        }
        dialog->show();
        return app.exec();
    }

    if (parser.isSet(gradientBrowserOption)) {
        auto* dialog = new apo::ui::GradientBrowserDialog();
        if (!positional.isEmpty()) dialog->openFile(positional.first());
        if (parser.isSet(screenshotOption)) {
            dialog->setAutoScreenshot(parser.value(screenshotOption), /*exitAfter=*/true);
        }
        dialog->show();
        return app.exec();
    }

    if (parser.isSet(smoothPaletteOption)) {
        auto* dialog = new apo::ui::SmoothPaletteDialog();
        if (!positional.isEmpty()) dialog->openImageFile(positional.first());
        if (parser.isSet(screenshotOption)) {
            dialog->setAutoScreenshot(parser.value(screenshotOption), /*exitAfter=*/true);
        }
        dialog->show();
        return app.exec();
    }

    if (parser.isSet(optionsOption)) {
        // Unlike --edit/--adjust/--render, no flame file is needed - the
        // Options dialog is app-wide, not per-flame (see OptionsDialog.h).
        auto* dialog = new apo::ui::OptionsDialog();
        if (parser.isSet(screenshotOption)) {
            dialog->setAutoScreenshot(parser.value(screenshotOption), /*exitAfter=*/true);
        }
        dialog->show();
        return app.exec();
    }

    apo::ui::MainWindow window;
    if (parser.isSet(screenshotOption)) {
        window.setAutoScreenshot(parser.value(screenshotOption), /*exitAfter=*/true);
    }
    if (!positional.isEmpty()) {
        window.openFlameFile(positional.first());
    }

    window.show();
    return app.exec();
}
