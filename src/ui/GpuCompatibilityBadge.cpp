#include "GpuCompatibilityBadge.h"

#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPixmap>

#include "core/render/gpu/VariationKinds.h"

namespace apo::ui {

const QIcon& cpuOnlyBadgeIcon() {
    // Drawn once and cached (QListWidgetItem/QTableWidgetItem::setIcon()
    // copies the QIcon handle, not the pixmap data) rather than loaded from
    // a resource file, since this project has no icon asset for it and the
    // shape is trivial.
    static const QIcon icon = [] {
        QPixmap pixmap(10, 10);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(140, 140, 140));
        painter.drawEllipse(1, 1, 8, 8);
        painter.end();
        return QIcon(pixmap);
    }();
    return icon;
}

const QString& cpuOnlyTooltipSuffix() {
    static const QString suffix =
        QStringLiteral("\nCPU-only: legacy plugin variation, not GPU-accelerated (renders correctly, just slower "
                        "on a GPU-enabled build).");
    return suffix;
}

bool isCpuOnlyVariation(const std::string& variationName) { return !apo::gpu::isVariationNameGpuEligible(variationName); }

} // namespace apo::ui
