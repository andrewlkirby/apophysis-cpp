#pragma once

#include <vector>

#include <QString>
#include <QStringList>

// Small QSettings-backed wrapper for the persisted, app-wide preferences
// OptionsDialog exposes (src/ui/OptionsDialog.h). Deliberately not a port
// of Options.pas's full TIniFile-backed preference set (sound, language,
// editor guideline colors, gradient-file paths, Electric Sheep/Chaotica/
// flam3 interop, ...) - most of that either configures a feature this port
// hasn't built (guideline overlays, a plugin-enable UI) or a third-party
// integration out of scope entirely, and per the "good performance, not a
// faithful copy" project direction, a preference with no feature consuming
// it is dead weight, not useful scaffolding (see Renderer.h's Adaptive DE
// class comment for a case where the *original itself* shipped exactly
// that kind of dead setting). Only settings an existing dialog/render path
// actually reads are here; extend this alongside whatever new feature
// needs the next one.
namespace apo::ui::AppSettings {

// Render threads for a full-quality render (RenderDialog's Render button) -
// 0 means "Auto: use every available hardware thread", matching
// apo::Renderer::render()'s own threadCount=0 default. Lets a user cap CPU
// usage during a long final render (e.g. to keep the rest of the machine
// responsive), mirroring Options.pas's cbNrThreads. Not applied to preview
// renders (RenderWorker::renderFlame) or thumbnails (ThumbnailTask) - both
// are already fast/low-priority enough that a user-facing cap isn't a
// concern there, and ThumbnailTask deliberately forces threadCount=1 for
// its own unrelated oversubscription reason (see its own comment).
int renderThreadCount();
void setRenderThreadCount(int threads);

// Sample density used for live-updating interactive previews (AdjustDialog
// dragging a slider, MutateDialog's 3x3 mutant grid) - lower is more
// responsive, higher is closer to what the final render actually looks
// like. Mirrors Options.pas's High/Medium/Low preview quality presets
// (there, a separate setting per dialog - MutatePrevQual vs the Adjust
// preview's own - unified into one shared knob here, since both are the
// same kind of thing and a user is unlikely to want them independently
// configured). Simplified to one fixed set of density values (4/8/16)
// rather than user-editable numbers per level. Not used by EditorWindow's
// preview (that one renders at the flame's own real sampleDensity, see
// EditorWindow::requestRender) or by MainWindow/ThumbnailTask (fixed,
// non-interactive contexts where responsiveness during a *drag* isn't a
// concern).
double previewSampleDensity();
void setPreviewSampleDensity(double density);

// Random flame generation (Options.pas's Random tab) - consumed by
// MainWindow::onNewRandomBatchTriggered (New Random Batch) and
// MutateDialog (its own mutation-candidate transform-count range).
// Defaults match this port's prior hardcoded constants exactly (see
// RandomFlame.h/MutateDialog.cpp), so a fresh install behaves identically
// to before these settings existed.
int randomMinXforms();
void setRandomMinXforms(int count);
int randomMaxXforms();
void setRandomMaxXforms(int count);
int randomBatchSize();
void setRandomBatchSize(int count);
QString randomBatchTitlePrefix();
void setRandomBatchTitlePrefix(const QString& prefix);
// Matches Main.pas's KeepBackground: when set, a freshly generated random
// batch keeps whatever background color the library already had instead
// of resetting to black.
bool randomKeepBackground();
void setRandomKeepBackground(bool keep);
int mutationMinXforms();
void setMutationMinXforms(int count);
int mutationMaxXforms();
void setMutationMaxXforms(int count);
// Forced symmetry automatically applied to every randomly generated flame
// - matches Main.pas's SymmetryType (0=None/1=Bilateral/2=Rotational/
// 3=Dihedral) and SymmetryOrder, consumed via core/edit/Symmetry.h's
// addSymmetry() at the MainWindow call site (see its own doc comment on
// the sym-sign convention: Bilateral -> sym=-1, Rotational -> sym=+Order,
// Dihedral -> sym=-Order).
int randomSymmetryType();
void setRandomSymmetryType(int type);
int randomSymmetryOrder();
void setRandomSymmetryOrder(int order);

// Variation blending for New Random Batch (MutationOps::randomizeXforms's
// own trailing parameters, extended into a settings-backed feature - see
// its doc comment for exactly what each of these controls). Unlike the
// xform-count/symmetry settings above, there's no prior hardcoded constant
// to preserve here (this feature didn't exist before), so these default to
// values that actually exercise blending/parameter randomization out of the
// box - a fresh install's "New Random Batch" is meant to look different
// (and more varied) from before this feature existed, not identical to it;
// a user who prefers the old single-variation-only behavior can set both
// variation-count fields to 1 and untick "Randomize variation parameters".
int randomMinVariationsPerXform();
void setRandomMinVariationsPerXform(int count);
int randomMaxVariationsPerXform();
void setRandomMaxVariationsPerXform(int count);
double randomVariationWeightMin();
void setRandomVariationWeightMin(double weight);
double randomVariationWeightMax();
void setRandomVariationWeightMax(double weight);
bool randomizeVariationParameters();
void setRandomizeVariationParameters(bool randomize);
double randomVariationParameterStrength();
void setRandomVariationParameterStrength(double strength);

// Options.pas's Variations tab checklist - names of every variation
// EXCLUDED from random topology generation (an empty list, the default,
// means every variation is eligible - this port's original, unrestricted
// behavior). Stored as an exclusion set rather than an inclusion set so a
// variation added to the registry later (a new plugin, say) defaults to
// enabled without needing this setting migrated. Consumed via
// enabledVariationIndices() below, not directly - MutationOps.h (Qt-free
// core code) takes a plain index list, not this Qt-ish name-based form.
QStringList disabledVariationNames();
void setDisabledVariationNames(const QStringList& names);
// VariationRegistry indices of every variation NOT in
// disabledVariationNames() - the ready-to-use form
// MutationOps::randomizeXforms/setXformsVariation's eligibleVariations
// parameter wants. Falls back to every registered index if the exclusion
// set would otherwise leave nothing eligible (a user who unchecked every
// box shouldn't end up with generation broken outright - same "clamp to
// something sane" spirit as randomMinXforms/randomMaxXforms above).
std::vector<int> enabledVariationIndices();

// Options.pas's "On random flame use..." gradient-source choice
// (grpGradient/randGradient - confirmed live at RndFlame.pas:185 and
// Main.pas:932/1073; an earlier pass at this port's own research had
// wrongly marked it dead, corrected in MISSING_FEATURES_PLAN.txt).
// Matches RandomFlame.h's RandomGradientSource enum ordinal-for-ordinal
// (0=RandomPreset/1=CurrentGradient/2=Calculated); stored as a plain int
// here rather than the enum itself so this header doesn't need to include
// RandomFlame.h just for a QSettings key. Default 2 (Calculated) matches
// this port's original, pre-Options-tab behavior exactly.
int randomGradientSource();
void setRandomGradientSource(int source);

// Options.pas's "Variation" menu (forced variation for New Random Batch -
// NOT for the Mutate dialog, which already has its own, richer per-session
// Trend combo serving the same purpose there). Empty string (the default)
// means "Random", matching randomizeXforms's own variationIndex<0
// convention; consumed by resolving the name back to a VariationRegistry
// index at the MainWindow call site (stored by name, not raw index, so it
// survives the registry's variation order changing between versions).
QString randomForcedVariationName();
void setRandomForcedVariationName(const QString& name);

// MainWindow's library list/thumbnail view toggle (Main.pas's
// btnViewIcons/btnViewList, backed by the global ClassicListMode - true
// means a compact name-only list, matching Regstry.pas/Settings.pas's own
// default when nothing is stored yet). This port's own long-standing
// default (before this toggle existed at all) was always thumbnails, so
// - unlike ClassicListMode's own true default - this setting defaults to
// true meaning *thumbnails*, to match this port's own established
// behavior for every existing installation rather than silently flipping
// it on upgrade. Skipping thumbnail generation entirely while this is
// false is a real performance win for large batches (matches the
// original's own reason for the feature - see Main.pas:689's `if (not
// ClassicListMode) then item.ImageIndex := i`), not just a cosmetic
// layout change.
bool libraryThumbnailView();
void setLibraryThumbnailView(bool thumbnails);

// New-flame render defaults (Options.pas's Display tab "Rendering" group)
// - applied to a brand-new flame (MainWindow's New Flame action and every
// randomly generated flame) instead of Flame's own raw class defaults.
// Gamma/brightness in particular default to 4 here (not 1, Flame.h's own
// class default) - matches Settings.pas/Regstry.pas's defGamma/
// defBrightness fallback, the original's actual "New Flame" values.
double defaultGamma();
void setDefaultGamma(double gamma);
double defaultBrightness();
void setDefaultBrightness(double brightness);
double defaultVibrancy();
void setDefaultVibrancy(double vibrancy);
double defaultGammaThreshold();
void setDefaultGammaThreshold(double threshold);
double defaultSampleDensity();
void setDefaultSampleDensity(double density);
int defaultOversample();
void setDefaultOversample(int oversample);
double defaultFilterRadius();
void setDefaultFilterRadius(double radius);

} // namespace apo::ui::AppSettings
