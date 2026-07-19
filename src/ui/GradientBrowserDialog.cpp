#include "GradientBrowserDialog.h"

#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

#include "core/BuiltinGradients.h"
#include "core/io/GradientIO.h"

namespace apo::ui {

namespace {
QImage colorMapToImage(const apo::ColorMap& cmap) {
    QImage image(256, 1, QImage::Format_RGB888);
    for (int i = 0; i < 256; ++i) {
        image.setPixelColor(i, 0, QColor(cmap.entries[i][0], cmap.entries[i][1], cmap.entries[i][2]));
    }
    return image;
}
} // namespace

GradientBrowserDialog::GradientBrowserDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Gradient Browser");
    resize(420, 480);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* rootLayout = new QVBoxLayout(this);

    auto* pathRow = new QHBoxLayout();
    pathLabel_ = new QLabel(this);
    pathLabel_->setObjectName("pathLabel");
    pathLabel_->setWordWrap(true);
    builtinButton_ = new QPushButton("Built-in Library", this);
    builtinButton_->setObjectName("builtinButton");
    connect(builtinButton_, &QPushButton::clicked, this, &GradientBrowserDialog::openBuiltinLibrary);
    browseButton_ = new QPushButton("Browse...", this);
    browseButton_->setObjectName("browseButton");
    connect(browseButton_, &QPushButton::clicked, this, &GradientBrowserDialog::browseFile);
    pathRow->addWidget(pathLabel_, /*stretch=*/1);
    pathRow->addWidget(builtinButton_);
    pathRow->addWidget(browseButton_);
    rootLayout->addLayout(pathRow);

    list_ = new QListWidget(this);
    list_->setObjectName("gradientList");
    connect(list_, &QListWidget::currentRowChanged, this, &GradientBrowserDialog::onSelectionChanged);
    connect(list_, &QListWidget::itemActivated, this, [this](QListWidgetItem*) { applySelected(); });
    rootLayout->addWidget(list_, /*stretch=*/1);

    previewStrip_ = new QLabel(this);
    previewStrip_->setObjectName("previewStrip");
    previewStrip_->setMinimumHeight(32);
    previewStrip_->setScaledContents(true);
    previewStrip_->setStyleSheet("background-color: #202020;");
    rootLayout->addWidget(previewStrip_);

    applyButton_ = new QPushButton("Apply", this);
    applyButton_->setObjectName("applyButton");
    applyButton_->setEnabled(false);
    connect(applyButton_, &QPushButton::clicked, this, &GradientBrowserDialog::applySelected);
    rootLayout->addWidget(applyButton_);

    openBuiltinLibrary();
}

void GradientBrowserDialog::setAutoScreenshot(const QString& path, bool exitAfter) {
    autoScreenshotPath_ = path;
    autoScreenshotExit_ = exitAfter;
}

void GradientBrowserDialog::browseFile() {
    const QString path = QFileDialog::getOpenFileName(this, "Open Gradient File", currentPath_,
                                                        "Gradient files (*.ugr *.gradient *.map);;All files (*.*)");
    if (!path.isEmpty()) openFile(path);
}

void GradientBrowserDialog::openFile(const QString& path) {
    source_ = Source::File;
    currentPath_ = path;
    list_->clear();
    hasSelection_ = false;
    applyButton_->setEnabled(false);
    previewStrip_->clear();

    const QFileInfo info(path);
    if (info.suffix().compare("map", Qt::CaseInsensitive) == 0) {
        // A .map file holds exactly one anonymous gradient (the whole
        // file) - matches ListFileContents's .map branch, showing the
        // file itself as the single selectable entry.
        list_->addItem(info.fileName());
    } else {
        const auto names = apo::listUgrGradientNames(path.toStdString());
        for (const auto& name : names) list_->addItem(QString::fromStdString(name));
    }

    pathLabel_->setText(list_->count() > 0 ? path : path + " (no gradients found)");
    if (list_->count() > 0) list_->setCurrentRow(0);
}

void GradientBrowserDialog::openBuiltinLibrary() {
    source_ = Source::Builtin;
    currentPath_.clear();
    list_->clear();
    hasSelection_ = false;
    applyButton_->setEnabled(false);
    previewStrip_->clear();

    for (int i = 0; i < apo::builtinGradientCount(); ++i) {
        list_->addItem(QString::fromUtf8(apo::builtinGradientName(i)));
    }
    pathLabel_->setText(QString("Built-in Library (%1 gradients)").arg(apo::builtinGradientCount()));
    if (list_->count() > 0) list_->setCurrentRow(0);
}

void GradientBrowserDialog::onSelectionChanged(int row) {
    if (row < 0) {
        hasSelection_ = false;
        applyButton_->setEnabled(false);
        previewStrip_->clear();
        return;
    }

    if (source_ == Source::Builtin) {
        currentPreview_ = apo::builtinGradient(row);
        hasSelection_ = true;
    } else {
        const QFileInfo info(currentPath_);
        const bool isMap = info.suffix().compare("map", Qt::CaseInsensitive) == 0;
        const std::string name = isMap ? "" : list_->item(row)->text().toStdString();
        hasSelection_ = apo::loadGradientFile(currentPath_.toStdString(), currentPreview_, name);
    }

    applyButton_->setEnabled(hasSelection_);
    refreshPreview();
}

void GradientBrowserDialog::refreshPreview() {
    if (hasSelection_) {
        previewStrip_->setPixmap(QPixmap::fromImage(colorMapToImage(currentPreview_)));
    } else {
        previewStrip_->clear();
    }
}

void GradientBrowserDialog::applySelected() {
    if (!hasSelection_) return;
    emit gradientApplied(currentPreview_);
}

void GradientBrowserDialog::showEvent(QShowEvent* event) {
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
