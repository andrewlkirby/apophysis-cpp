#include "XaosDialog.h"

#include <algorithm>

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QShowEvent>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "TriangleCanvas.h"

namespace apo::ui {

namespace {
QString formatWeight(double v) { return QString::number(v, 'g', 6); }
} // namespace

XaosDialog::XaosDialog(std::shared_ptr<apo::Flame> flame, QWidget* parent)
    : QDialog(parent), flame_(std::move(flame)) {
    setWindowTitle(
        QString("Xaos - %1").arg(flame_->name.empty() ? "Untitled" : QString::fromStdString(flame_->name)));
    resize(520, 420);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* rootLayout = new QVBoxLayout(this);

    table_ = new QTableWidget(this);
    table_->setObjectName("xaosTable");
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    rootLayout->addWidget(table_, /*stretch=*/1);

    auto* buttonRow = new QHBoxLayout();
    auto* clearAllButton = new QPushButton("Clear All", this);
    clearAllButton->setObjectName("clearAllButton");
    clearAllButton->setToolTip("Set every weight to 0 (no transitions)");
    auto* setAllButton = new QPushButton("Set All", this);
    setAllButton->setObjectName("setAllButton");
    setAllButton->setToolTip("Set every weight to 1 (unrestricted transitions)");
    buttonRow->addWidget(clearAllButton);
    buttonRow->addWidget(setAllButton);
    buttonRow->addStretch(1);
    rootLayout->addLayout(buttonRow);

    connect(clearAllButton, &QPushButton::clicked, this, &XaosDialog::onClearAll);
    connect(setAllButton, &QPushButton::clicked, this, &XaosDialog::onSetAll);
    connect(table_, &QTableWidget::itemChanged, this, &XaosDialog::onCellChanged);

    rebuildTable();
}

void XaosDialog::rebuildTable() {
    const int n = flame_->numXForms();
    table_->blockSignals(true);
    table_->setRowCount(n);
    table_->setColumnCount(n);

    for (int i = 0; i < n; ++i) {
        auto* vHeader = table_->verticalHeaderItem(i);
        if (!vHeader) {
            vHeader = new QTableWidgetItem();
            table_->setVerticalHeaderItem(i, vHeader);
        }
        vHeader->setText(QString::number(i + 1));
        vHeader->setBackground(TriangleCanvas::xformColor(i));

        auto* hHeader = table_->horizontalHeaderItem(i);
        if (!hHeader) {
            hHeader = new QTableWidgetItem();
            table_->setHorizontalHeaderItem(i, hHeader);
        }
        hHeader->setText(QString::number(i + 1));
        hHeader->setBackground(TriangleCanvas::xformColor(i));

        for (int j = 0; j < n; ++j) {
            auto* item = table_->item(i, j);
            if (!item) {
                item = new QTableWidgetItem();
                table_->setItem(i, j, item);
            }
            item->setText(formatWeight(flame_->xform[i]->modWeights[j]));
        }
    }

    table_->blockSignals(false);
}

void XaosDialog::onCellChanged(QTableWidgetItem* item) {
    const int row = item->row();
    const int col = item->column();
    const int n = flame_->numXForms();
    if (row < 0 || col < 0 || row >= n || col >= n) return;

    bool ok = false;
    double value = item->text().toDouble(&ok);
    if (!ok) {
        value = flame_->xform[row]->modWeights[col]; // invalid input: revert display, no-op edit
    } else {
        // Weights are transition likelihoods (see Renderer.cpp's
        // buildPropTable, whose cumulative-sum selection assumes every
        // weight is non-negative) - clamp rather than let a stray negative
        // silently degrade that xform's whole row to a fallback
        // always-picks-xform-0 distribution.
        value = std::max(0.0, value);
        flame_->xform[row]->modWeights[col] = value;
    }

    table_->blockSignals(true);
    item->setText(formatWeight(value));
    table_->blockSignals(false);

    if (ok) emit flameChanged();
}

void XaosDialog::onClearAll() {
    const int n = flame_->numXForms();
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) flame_->xform[i]->modWeights[j] = 0.0;
    }
    rebuildTable();
    emit flameChanged();
}

void XaosDialog::onSetAll() {
    const int n = flame_->numXForms();
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) flame_->xform[i]->modWeights[j] = 1.0;
    }
    rebuildTable();
    emit flameChanged();
}

void XaosDialog::refreshFromFlame() { rebuildTable(); }

void XaosDialog::setAutoScreenshot(const QString& path, bool exitAfter) {
    autoScreenshotPath_ = path;
    autoScreenshotExit_ = exitAfter;
}

void XaosDialog::showEvent(QShowEvent* event) {
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
