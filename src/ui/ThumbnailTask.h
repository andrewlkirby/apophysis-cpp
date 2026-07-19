#pragma once

#include <memory>

#include <QImage>
#include <QObject>
#include <QRunnable>

#include "core/Flame.h"

namespace apo::ui {

// One flame's thumbnail render, run on QThreadPool::globalInstance() -
// matches the migration plan's explicit direction for Main.pas's
// TThumbnailThread ("moves to a QThreadPool/QtConcurrent task"). QRunnable
// (for QThreadPool) and QObject (for the finished signal) are combined via
// ordinary multiple inheritance, a standard, supported Qt idiom for
// exactly this "background task that needs to report back" shape.
//
// Deliberately single-threaded internally (Renderer::render's
// threadCount=1) even though many ThumbnailTasks run concurrently across
// the pool: each one spawning its own full multithreaded render would
// oversubscribe the machine (N thumbnails x hardware_concurrency threads
// competing), whereas QThreadPool already caps concurrency sensibly on its
// own (defaults to the ideal thread count) - the parallelism belongs at
// the "how many thumbnails at once" level, not duplicated inside each one.
class ThumbnailTask final : public QObject, public QRunnable {
    Q_OBJECT

public:
    ThumbnailTask(int index, std::shared_ptr<const apo::Flame> flame, int size)
        : index_(index), flame_(std::move(flame)), size_(size) {
        setAutoDelete(true);
    }

    void run() override {
        // Clone rather than mutate the shared original: other code (the
        // list's full-quality selection render, another concurrent
        // ThumbnailTask for the same flame during a re-render) may be
        // reading `flame_` at the same time - see Flame::clone()'s own
        // comment on exactly this concurrent-access hazard.
        std::unique_ptr<apo::Flame> thumb = flame_->clone();
        thumb->width = size_;
        thumb->height = size_;
        thumb->spatialOversample = 1;
        thumb->sampleDensity = 5; // fast/rough - just enough to recognize the shape

        const apo::RenderedImage image = apo::Renderer::render(*thumb, /*seed=*/1, /*threadCount=*/1);

        QImage qimage;
        if (!image.pixels.empty()) {
            const QImage::Format format = image.channels == 4 ? QImage::Format_RGBA8888 : QImage::Format_RGB888;
            qimage = QImage(image.pixels.data(), image.width, image.height, image.width * image.channels, format)
                         .copy();
        }

        emit finished(index_, qimage);
    }

signals:
    void finished(int index, QImage image);

private:
    int index_;
    std::shared_ptr<const apo::Flame> flame_;
    int size_;
};

} // namespace apo::ui
