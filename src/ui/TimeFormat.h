#pragma once

#include <QString>

namespace apo::ui {

// Renders a duration as H:MM:SS (once it reaches an hour - a real
// possibility for a large final render) or M:SS otherwise, always
// zero-padded past the leading field. Every render-progress status label
// (RenderDialog, RenderAllDialog, PostProcessDialog) should use this instead
// of a raw "%1s" seconds count once a render can run long enough that
// minutes/hours are the more natural unit to read at a glance.
inline QString formatDuration(double seconds) {
    if (seconds < 0.0) seconds = 0.0;
    const qint64 totalSeconds = static_cast<qint64>(seconds + 0.5);
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 secs = totalSeconds % 60;
    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(secs, 2, 10, QLatin1Char('0'));
    }
    return QString("%1:%2").arg(minutes).arg(secs, 2, 10, QLatin1Char('0'));
}

} // namespace apo::ui
