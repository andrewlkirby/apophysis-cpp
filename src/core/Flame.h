#pragma once

#include <array>
#include <memory>
#include <string>

#include "Bezier.h"
#include "ColorMap.h"
#include "XForm.h"

namespace apo {

// Sentinel xform index meaning "Flame::finalXform", for any API that
// otherwise takes an index into Flame::xform (0..numXForms()-1) - lets
// TriangleCanvas/TransformPanel/EditorWindow's xform-selection plumbing
// treat the final transform as just another selectable/editable/draggable
// triangle without a parallel "or is it the final one?" parameter
// threaded through every call. -2, not -1, so it stays distinguishable
// from the existing "nothing selected" sentinel used throughout the
// Editor's UI layer.
inline constexpr int kFinalXformIndex = -2;

// Mirrors Delphi's TControlPoint (Flame/ControlPoint.pas): one flame - its
// xforms, camera, colormap, and render parameters.
//
// Phase 1 scope only: data fields plus construction/reset, matching the
// subset of TControlPoint's public fields that describe *what a flame is*.
// Deliberately not yet ported (belongs to later phases per the approved
// migration plan, not because it's unimportant):
//   - IterateXYC / the chaos-game point iteration loop, camera projection
//     (ProjectNone/ProjectPitch/...), CalcBoundbox, RandomCP -> Phase 3
//     (Rendering engine) - these need the Rng/threading design settled first.
//   - SaveToStringlist/ParseString(List)/ToXMLString -> Phase 4 (File I/O),
//     which deliberately replaces the original's regex-based XML handling
//     with a real parser rather than porting ParameterIO.pas as-is.
//   - InterpolateX (animation frame interpolation) -> Phase 3, alongside
//     the render loop it primarily exists to feed.
class Flame {
public:
    static constexpr int kMaxXForms = XForm::kMaxXForms; // NXFORMS

    Flame();

    void clear();

    // Deep-copies every xform (via XForm::assign(), not a shallow struct
    // copy - Flame::xform holds unique_ptr<XForm>s, and each XForm owns its
    // own regVariations_ instances) plus every scalar/collection field.
    // Needed because Flame has no implicit copy constructor (unique_ptr
    // members delete it) - added for Phase 3's per-render-thread cloning
    // (see Renderer.cpp): each worker thread mutates its own XForm
    // per-point scratch state (tx_/ty_/.../rng_) during nextPoint(), so
    // concurrent threads cannot safely share one Flame's XForm objects,
    // matching the original's own BucketFillerThread.Create doing
    // `Fcp := cp.Clone` per thread.
    std::unique_ptr<Flame> clone() const;

    // Same deep-copy as clone(), but into an already-existing Flame instead
    // of a new one - matches TControlPoint.Copy. Needed wherever a shared
    // Flame instance (e.g. a shared_ptr<Flame> another window also holds)
    // must be overwritten in place - the Mutate dialog adopting a mutant is
    // the first such caller (Phase 6): it can't just rebind the caller's
    // shared_ptr to a fresh clone(), since EditorWindow's own reference
    // would then keep pointing at the stale flame.
    void copyFrom(const Flame& other);

    // Renderer-only lean clone (FOLLOWUP_PLAN.txt B1a): same deep-copy as
    // clone(), except only xform[0..activeXForms-1] (clamped to
    // [0,kMaxXForms]) get real XForm objects - every other slot in `xform`
    // is left null, instead of clone()'s full kMaxXForms+1 (Flame's default
    // constructor alone instantiates every registered variation for all 101
    // slots - ~15,000-20,000 heap allocations - regardless of how many
    // xforms a real flame ever actually uses; Renderer::render() pays that
    // cost once per worker thread on every render). Safe ONLY because
    // Renderer.cpp's per-thread render clone (a) always knows its exact
    // active xform count up front (computed from the original, un-cloned
    // flame before any thread starts) and threads every loop bound through
    // that count explicitly rather than re-deriving it by calling
    // numXForms() on the clone, and (b) never indexes xform[] past that
    // count (see buildPropTable()'s own [0,numXForms) contract - a render's
    // per-point xform selection can never draw an index outside that
    // range). Any other caller MUST NOT use this: numXForms() and every
    // other Flame method that scans the xform array assume every slot up to
    // kMaxXForms holds a real object, and will read through a null
    // unique_ptr if handed a cloneForRender() result.
    std::unique_ptr<Flame> cloneForRender(int activeXForms) const;

    // First index i in [0, kMaxXForms) with xform[i]->density == 0 marks
    // the active xform count - xforms are kept packed at the front, exactly
    // matching TControlPoint.NumXForms's contract (callers must maintain
    // this packing; it is not enforced here, same as the original).
    int numXForms() const;

    // Structural xform-list edits - Editor.pas's mnuAddClick/mnuDupClick/
    // DeleteTriangle, restated in terms of this port's packed-array model.
    // All three maintain the numXForms() packing contract themselves.
    //
    // addXForm()/duplicateXForm() return the new xform's index, or -1 if
    // already at kMaxXForms (a no-op - callers don't need to pre-check).
    // addXForm() appends a fresh default xform (density 0.5, pure linear,
    // default xaos weights - every existing xform can already reach it,
    // matching the original's behavior of not touching other xforms'
    // modWeights on Add).
    int addXForm();
    // duplicateXForm() appends a copy of xform[index] and preserves its
    // xaos links in both directions (every other xform's weight *toward*
    // the duplicate matches its weight toward the original, and the
    // duplicate's self-weight matches the original's self-weight) -
    // matches mnuDupClick exactly.
    int duplicateXForm(int index);
    // removeXForm() deletes xform[index] and re-packs every later xform
    // down by one slot, including xaos-weight column compaction (drop the
    // deleted column, shift later columns down to match the new indices)
    // - matches DeleteTriangle's compaction exactly. No-op if index is out
    // of range or only one xform remains (an editor should never be left
    // with zero active xforms). Deliberately does not port
    // DeleteTriangle's optional RebuildXaosLinks smart-relink step (a
    // rarely-exercised edge case: automatically bridging xaos links that
    // singly passed through the deleted xform) - a user who's built a
    // custom xaos chain and deletes one of its xforms can reconnect the
    // remaining links by hand, a reasonable "good performance, not a
    // faithful copy" cut for this genuinely niche behavior.
    void removeXForm(int index);

    // Rescales pixelsPerUnit proportionally to a new render width (matching
    // ControlPoint.pas's AdjustScale) and updates width/height, so the
    // flame's visible composition stays consistent when the render target
    // changes size - see Flame.cpp for the full rationale.
    void adjustScale(int w, int h);

    std::array<std::unique_ptr<XForm>, kMaxXForms + 1> xform;
    std::unique_ptr<XForm> finalXform;
    bool finalXformEnabled = false;
    bool useFinalXform = false;
    int soloXform = -1;

    bool transparency = false;

    // Print/output rotation angle, radians (Delphi's FAngle - the field
    // Renderer.cpp's plan.projectionKind/camera setup doesn't yet consume;
    // see AddPointsToBucketsAngle in the Phase 3 design notes). Added in
    // Phase 4 for .flame round-trip fidelity - ParameterIO.pas's XML
    // loader has a confirmed bug where the `rotate` attribute (a
    // degrees-based alternate encoding of this same angle) is written into
    // `vibrancy` instead of `FAngle` (a copy-paste error - the comment on
    // that exact line documents the *intended* behavior correctly, the
    // code just assigns to the wrong field). FlameIO.cpp fixes this rather
    // than reproducing it: both `angle` and `rotate` load into this field.
    double angle = 0;

    double cameraPitch = 0, cameraYaw = 0, cameraPersp = 0, cameraDOF = 0, cameraZpos = 0;

    bool noLinearFix = false;
    ColorMap cmap;
    int cmapIndex = 0;
    double time = 0;
    double brightness = 1.0;
    double contrast = 1.0;
    double gamma = 1.0;
    int width = 100;
    int height = 100;
    int spatialOversample = 1;
    std::string name, nick, url;
    std::array<double, 2> center{0, 0};
    double vibrancy = 1.0;
    double hueRotation = 0.0;
    std::array<int, 4> background{0, 0, 0, 0};
    double zoom = 0;
    double pixelsPerUnit = 50;
    double spatialFilterRadius = 0.5;
    double sampleDensity = 50;
    double actualDensity = 0;
    int nbatches = 1;
    int whiteLevel = 200;
    int cmapInter = 0;
    int symmetry = 0;
    double pulse[2][2] = {{0, 60}, {0, 60}};
    double wiggle[2][2] = {{0, 60}, {0, 60}};

    double estimator = 9.0;
    double estimatorMin = 0.0;
    double estimatorCurve = 0.4;
    int jitters = 1;
    double gammaThreshold = 0.01;
    bool enableDE = false;

    // Bezier tone curves (the Curves dialog) - index 0 is the master/"All"
    // channel (applied first during tone-mapping), 1/2/3 are Red/Green/Blue
    // (applied after) - matches TCurvesChannel's ccAll/ccRed/ccGreen/ccBlue
    // ordering exactly. See Renderer.cpp's toneMap() for where these get
    // applied and src/ui/CurvesDialog.h for the editor. Every channel
    // defaults to BezierCurve's own default (a straight P0=(0,0)-P3=(1,1)
    // line, i.e. no-op) - see Renderer.cpp's curvesSet detection for why a
    // render skips curve application entirely while every channel is still
    // at that exact default, rather than evaluating a "no-op" table.
    std::array<BezierCurve, 4> curves;

    std::string xdata;

private:
    // Tag-dispatched constructor that skips the default constructor's
    // eager "populate all 101 xform slots" work - used only by
    // cloneForRender(), which populates just the slots it needs itself.
    // `xform`/`finalXform` are left null (std::array<unique_ptr<...>,N>'s
    // own default member-initialization); every scalar field still gets its
    // normal in-class default via NSDMI, same as the default constructor.
    struct SkipXFormInitTag {};
    explicit Flame(SkipXFormInitTag) {}

    // Every copyFrom()/cloneForRender()-shared field except the xform
    // array/finalXform themselves (each caller copies those its own way -
    // see copyFrom() vs cloneForRender()).
    void copyScalarFieldsFrom(const Flame& other);
};

} // namespace apo
