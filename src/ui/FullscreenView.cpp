#include "FullscreenView.h"

#include <algorithm>
#include <random>

#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QScreen>
#include <QShowEvent>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include "AppSettings.h"
#include "RenderWorker.h"

namespace apo::ui {

namespace {
constexpr int kProgressPollIntervalMs = 150;
} // namespace

FullscreenView::FullscreenView(std::shared_ptr<apo::Flame> flame, QWidget* parent)
    : QWidget(parent, Qt::Window), flame_(std::move(flame)) {
    setAttribute(Qt::WA_DeleteOnClose);
    setStyleSheet("background-color: black;");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    imageLabel_ = new QLabel(this);
    imageLabel_->setObjectName("fullscreenImageLabel");
    imageLabel_->setAlignment(Qt::AlignCenter);
    layout->addWidget(imageLabel_);

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName("fullscreenStatusLabel");
    statusLabel_->setStyleSheet("color: white; background-color: rgba(0,0,0,160); padding: 4px;");
    statusLabel_->setAlignment(Qt::AlignCenter);
    statusLabel_->setText("Rendering...");
    layout->addWidget(statusLabel_);

    progressTimer_ = new QTimer(this);
    progressTimer_->setInterval(kProgressPollIntervalMs);
    connect(progressTimer_, &QTimer::timeout, this, &FullscreenView::onProgressTick);

    workerThread_ = new QThread(this);
    worker_ = new RenderWorker();
    worker_->moveToThread(workerThread_);
    connect(this, &FullscreenView::renderRequested, worker_, &RenderWorker::renderFull);
    connect(worker_, &RenderWorker::fullRenderFinished, this, &FullscreenView::onRenderFinished);
    connect(workerThread_, &QThread::finished, worker_, &QObject::deleteLater);
    workerThread_->start();
}

void FullscreenView::setAutoScreenshot(const QString& path, bool exitAfter) {
    autoScreenshotPath_ = path;
    autoScreenshotExit_ = exitAfter;
}

FullscreenView::~FullscreenView() {
    // Same cancel-before-destroy ordering as RenderDialog - progress_ is
    // read by the worker thread for the duration of its blocking render()
    // call, so it must be requested to stop (and that stop observed, via
    // quit()+wait()) before this destructor's implicit progress_.reset()
    // would otherwise dangle that pointer out from under it.
    if (progress_) progress_->cancelRequested.store(true, std::memory_order_relaxed);
    workerThread_->quit();
    workerThread_->wait();
}

void FullscreenView::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!renderStarted_) {
        renderStarted_ = true;
        startRender();
    }
}

void FullscreenView::startRender() {
    const QScreen* screen = QGuiApplication::primaryScreen();
    const QSize screenSize = screen ? screen->geometry().size() : size();
    const int w = std::max(screenSize.width(), 1);
    const int h = std::max(screenSize.height(), 1);

    auto renderFlame = flame_->clone();
    renderFlame->adjustScale(w, h);

    progress_ = std::make_unique<apo::RenderProgress>();
    elapsedTimer_.start();
    progressTimer_->start();
    statusLabel_->setText("Rendering...");

    std::shared_ptr<const apo::Flame> shared(std::move(renderFlame));
    const quint64 seed = static_cast<quint64>(std::random_device{}());
    emit renderRequested(shared, seed, AppSettings::renderThreadCount(), progress_.get(), QString());
}

void FullscreenView::onProgressTick() {
    if (!progress_) return;
    const std::uint64_t done = progress_->pointsDone.load(std::memory_order_relaxed);
    const std::uint64_t target = progress_->pointsTarget.load(std::memory_order_relaxed);
    const int percent = target > 0 ? static_cast<int>(std::min<std::uint64_t>(100, done * 100 / target)) : 0;
    const double elapsedSec = elapsedTimer_.elapsed() / 1000.0;
    statusLabel_->setText(QString("Rendering... %1%% (%2s) - right-click to cancel").arg(percent).arg(elapsedSec, 0, 'f', 1));
}

void FullscreenView::onRenderFinished(QImage image, quint64 /*pointsGenerated*/, quint64 pointsAccepted,
                                       bool cancelled, bool /*saved*/) {
    progressTimer_->stop();
    progress_.reset();

    if (!image.isNull()) imageLabel_->setPixmap(QPixmap::fromImage(image));

    if (cancelled) {
        statusLabel_->setText("Cancelled - double-click or press any key to close");
    } else {
        const double elapsedSec = elapsedTimer_.elapsed() / 1000.0;
        statusLabel_->setText(
            QString("Done - %1 points in %2s - double-click or press any key to close").arg(pointsAccepted).arg(elapsedSec, 0, 'f', 1));
    }

    if (!autoScreenshotPath_.isEmpty()) {
        const QString path = autoScreenshotPath_;
        const bool exitAfter = autoScreenshotExit_;
        autoScreenshotPath_.clear();
        QTimer::singleShot(100, this, [this, path, exitAfter] {
            grab().save(path, "PNG");
            if (exitAfter) qApp->quit();
        });
    }
}

void FullscreenView::keyPressEvent(QKeyEvent* event) {
    Q_UNUSED(event);
    close();
}

void FullscreenView::mouseDoubleClickEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    close();
}

void FullscreenView::contextMenuEvent(QContextMenuEvent* event) {
    QMenu menu(this);
    QAction* cancelAction = menu.addAction("Cancel Render");
    cancelAction->setEnabled(static_cast<bool>(progress_));
    connect(cancelAction, &QAction::triggered, this, [this] {
        if (progress_) progress_->cancelRequested.store(true, std::memory_order_relaxed);
    });
    QAction* closeAction = menu.addAction("Close");
    connect(closeAction, &QAction::triggered, this, &FullscreenView::close);
    menu.exec(event->globalPos());
}

} // namespace apo::ui
