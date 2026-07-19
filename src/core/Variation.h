#pragma once

#include <memory>
#include <string>

#include "Rng.h"

namespace apo {

// A minimal, allocation-free type-erased "call this no-arg member function
// on this object" pair (FOLLOWUP_PLAN.txt B2b) - replaces
// std::function<void()> in XForm's per-point calc dispatch list
// (XForm::calcFunctionList_/functionList_) and in selectCalcFunction()'s
// return type below. std::function's own dispatch is an extra layer of
// indirection stacked on top of whatever the target itself already does
// (Variation::calc() is itself virtual - see below), and its type erasure
// makes that extra layer opaque to the inliner. Every call site here is
// always "call this exact method on this exact object", never a general
// closure over arbitrary captured state, so a raw function-pointer +
// context-pointer pair (built via bindCalc(), below) is exactly what's
// needed - costs nothing to construct (no heap allocation, no vtable of
// its own) and is trivially copyable.
struct CalcFn {
    void* obj = nullptr;
    void (*trampoline)(void*) = nullptr;
    void operator()() const { trampoline(obj); }
};

// Binds a no-arg member function of T to `obj` as a CalcFn - the
// zero-overhead replacement for `[obj]{ obj->Method(); }`. Method may
// itself be virtual (e.g. &Variation::calc, used by this file's own default
// selectCalcFunction() below): the trampoline's call through Method still
// dispatches virtually as normal - only the *outer* std::function-style
// indirection this replaces is removed, not Method's own dispatch.
template <class T, void (T::*Method)()>
CalcFn bindCalc(T* obj) {
    return CalcFn{obj, [](void* o) { (static_cast<T*>(o)->*Method)(); }};
}

// Mirrors Delphi's TBaseVariation (Core/BaseVariation.pas). One instance of
// a Variation subclass is created per registered variation type, per XForm
// (see XForm::addRegVariations(), matching TXForm.AddRegVariations).
//
// The Delphi version threads its per-point state through raw pointer fields
// (FTx/FTy/FTz in, FPx/FPy/FPz out) that XForm::prepare() rebinds every call.
// Kept as pointers here too (not references) for the same reason the
// original used them: XForm owns the actual doubles and rebinds these once
// per XForm::prepare(), not once per point.
class Variation {
public:
    virtual ~Variation() = default;

    // Bound by XForm::prepare() before any calc() call.
    double* tx = nullptr;
    double* ty = nullptr;
    double* tz = nullptr;
    double* px = nullptr;
    double* py = nullptr;
    double* pz = nullptr;
    double* color = nullptr;

    // Affine coefficients of the owning XForm (a..f == c[0][0]..c[2][1]).
    double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

    // Interpolated weight for this variation on the owning XForm.
    double vvar = 0;

    // Bound by XForm::prepare() to the same Rng the owning XForm's local
    // stochastic variations use (see Rng.h) - for variations that need
    // per-point randomness (e.g. julian's xform-branch pick via
    // rng->uniformInt()).
    Rng* rng = nullptr;

    // Called once per XForm::prepare(), after tx/ty/.../a../vvar/rng are
    // bound. Matches TBaseVariation.Prepare.
    virtual void prepare() {}

    // Called once per point, in variation-list order (the common case).
    // Reads *tx/*ty/*tz, accumulates into *px/*py/*pz - matches
    // TBaseVariation.CalcFunction.
    virtual void calc() = 0;

    // Called once per XForm::prepare(), immediately after prepare() above -
    // matches TBaseVariation.GetCalcFunction. Most variations never override
    // this (the default just wraps calc()), but a few (e.g. julian) pick a
    // cheaper specialized per-point function based on the variation's
    // *current parameter values*, re-evaluated on every prepare() call, not
    // just once at construction - the original calls GetCalcFunction from
    // inside TXForm.Prepare, not TXForm.Create, specifically so a parameter
    // change takes effect on the next prepare().
    virtual CalcFn selectCalcFunction() { return bindCalc<Variation, &Variation::calc>(this); }

    // Named-parameter access for variation-specific settings (e.g. a
    // "julia_power" field on the julia variation). Default: no parameters.
    virtual int numVariables() const { return 0; }
    virtual std::string variableNameAt(int /*index*/) const { return {}; }
    virtual bool getVariable(const std::string& /*name*/, double& /*value*/) const { return false; }
    // `value` is in/out, matching Delphi's `var Value: double` - a few
    // variations normalize what's stored (e.g. julian_power rounds to a
    // nonzero integer) and report the actual stored value back through
    // `value`, which callers (the editor's parameter spinners, in Phase 6)
    // depend on to reflect the normalized value rather than the raw input.
    virtual bool setVariable(const std::string& /*name*/, double& /*value*/) { return false; }
    // Default resets to 0 (matches TBaseVariation.ResetVariable's default),
    // but many variations override this with their own non-zero defaults
    // (e.g. auger_freq resets to 5, not 0) - virtual for that reason.
    virtual bool resetVariable(const std::string& name) {
        double zero = 0.0;
        return setVariable(name, zero);
    }
};

// Mirrors Delphi's TVariationLoader/TVariationClassLoader (Core/BaseVariation.pas):
// a lightweight, allocation-free-until-needed descriptor the registry holds,
// separate from actual Variation instances (which are only created per XForm).
class VariationFactory {
public:
    virtual ~VariationFactory() = default;

    virtual std::string name() const = 0;
    virtual std::unique_ptr<Variation> create() const = 0;
    virtual int numVariables() const = 0;
    virtual std::string variableNameAt(int index) const = 0;

    bool supports3D = false;
    bool supportsDC = false;
};

} // namespace apo
