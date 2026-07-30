#pragma once

#include <QByteArray>
#include <QString>
#include <QWidget>

#include "AppSettings.h"

namespace apo::ui {

// Restores a top-level window's last-saved size/position (see
// AppSettings::windowGeometry) if one exists. Call once, at the END of the
// constructor - after every child widget/layout has been built, not right
// after the constructor's own resize(w, h) call - so a window with nothing
// saved yet still gets that hardcoded default size rather than Qt's generic
// layout-sizeHint fallback, while a window with a saved geometry overrides
// it. Calling this before the layout is fully built is a real bug, not
// just a stylistic preference: Qt's layout system can silently grow a
// window back past a smaller restored size once the rest of the
// constructor populates a layout whose true minimum size wasn't known yet
// at restore time (observed directly: AdjustDialog's splitter+tabs layout
// did exactly this before this comment's own fix moved the call to the
// end of its constructor). Pair with saveWindowGeometry() in a
// closeEvent() override using the same key.
inline void restoreWindowGeometry(QWidget* window, const QString& key) {
    const QByteArray geometry = AppSettings::windowGeometry(key);
    if (!geometry.isEmpty()) window->restoreGeometry(geometry);
}

inline void saveWindowGeometry(QWidget* window, const QString& key) {
    AppSettings::setWindowGeometry(key, window->saveGeometry());
}

} // namespace apo::ui
