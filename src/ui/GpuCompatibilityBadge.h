#pragma once

#include <string>

#include <QString>

class QIcon;

namespace apo::ui {

// Shared lightweight visual marker for "this variation name isn't ported to
// the GPU backend" (docs/GPU_RENDERING_PLAN.md), used by both
// TransformPanel's Variations table and OptionsDialog's Variations tab
// checklist - factored out here (rather than duplicated per file) so the
// marker can't visually drift between the two: same icon, same tooltip
// wording, everywhere a variation name is listed for selection.
//
// Deliberately a plain dot rather than a warning/error icon: a GPU-
// ineligible variation (today, only the 47 legacy C plugins) isn't broken
// or discouraged, it just always renders on the CPU regardless of
// AppSettings::useGpuRendering().
const QIcon& cpuOnlyBadgeIcon();

// Appended to a variation name's own tooltip (not prepended - callers
// already show the plain name first) when cpuOnlyBadgeIcon() applies.
const QString& cpuOnlyTooltipSuffix();

// True iff `variationName` should get the marker above - i.e. iff
// apo::gpu::isVariationNameGpuEligible(variationName) is false. Thin
// wrapper so callers don't need their own core/render/gpu/VariationKinds.h
// include just for this one check.
bool isCpuOnlyVariation(const std::string& variationName);

} // namespace apo::ui
