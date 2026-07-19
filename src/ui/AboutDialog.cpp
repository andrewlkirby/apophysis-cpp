#include "AboutDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace apo::ui {

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("About Apophysis 7X");
    setFixedWidth(420);

    auto* layout = new QVBoxLayout(this);

    auto* title = new QLabel("<h2>Apophysis 7X</h2>", this);
    layout->addWidget(title);

    auto* subtitle = new QLabel("C++/Qt6 port &#8212; version 1.0.0", this);
    layout->addWidget(subtitle);

    // QLabel's openExternalLinks handles link clicks itself (hands off to
    // QDesktopServices::openUrl) - simpler than the original's per-label
    // OnClick + ShellExecute wiring, one handler per credit line.
    auto* credits = new QLabel(this);
    credits->setTextFormat(Qt::RichText);
    credits->setOpenExternalLinks(true);
    credits->setWordWrap(true);
    credits->setText(
        "<p>A from-scratch C++/Qt6 rewrite of Apophysis 7X, prioritizing "
        "render performance - see the project's migration notes for what "
        "changed along the way.</p>"
        "<p><b>Rendering algorithm and variation library</b> descend from "
        "flam3 by Scott Draves and the Apophysis project:<br>"
        "flame - cosmic recursive fractal flames<br>"
        "Copyright &#169; 1992-2012 Scott Draves &mdash; "
        "<a href=\"http://flam3.com\">flam3.com</a></p>"
        "<p><b>Original Apophysis / Apophysis 7X team:</b><br>"
        "Mark Townsend, Ronald Hordijk, Piotr Borys, Peter Sdobnov, Georg Kiehne "
        "(<a href=\"http://sourceforge.net/projects/apophysis7x/\">"
        "sourceforge.net/projects/apophysis7x</a>)</p>"
        "<p><b>Built with:</b><br>"
        "<a href=\"https://www.qt.io\">Qt 6</a>, "
        "<a href=\"https://pugixml.org\">pugixml</a>, "
        "<a href=\"http://www.libpng.org\">libpng</a></p>"
        "<p>Licensed under the GNU General Public License v3 - see LICENSE.</p>");
    layout->addWidget(credits);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::clicked, this, &QDialog::accept);
    layout->addWidget(buttons);
}

} // namespace apo::ui
