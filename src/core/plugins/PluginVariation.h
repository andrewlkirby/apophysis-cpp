#pragma once

#include <memory>
#include <string>

#include "../Variation.h"

namespace apo {

// Adapter that lets the ~55 legacy Apophysis Plugin/*.c files (originally
// built as separate DLLs, one variation type each, per Plugin/apoplugin.h's
// C ABI) register as ordinary apo::Variation types, statically linked in.
//
// Each plugin .c file is compiled as-is (see plugins/thirdparty/) inside its
// own uniquely-named C++ namespace by a small per-plugin wrapper .cpp (see
// plugins/generated/) - that's what keeps ~55 copies of identically-named C
// functions (PluginVarCreate, PluginVarCalc, ...) from colliding at link
// time, since each TU's copy gets internal linkage from its namespace rather
// than being one global symbol shared (and redefined) by every plugin.
//
// Rather than a bespoke Variation subclass per plugin (which would mean
// hardcoding each plugin's parameter names into C++ code - unnecessary,
// since the plugin ABI already exposes get/set/reset by name and
// enumerates its own parameter count/names at runtime), this is one
// generic adapter driven by a table of function pointers into that
// plugin's namespaced symbols. Each plugin's wrapper .cpp just builds a
// PluginVTable and calls registerPluginVariation() once.
struct PluginVTable {
    void* (*create)();
    int (*destroy)(void** state);
    int (*prepare)(void* state);
    int (*calc)(void* state);
    // Always bound via PluginVarInitDC (the most general of apoplugin.h's
    // three Init variants - Init/Init3D just fill in dummy z/color/matrix
    // values that InitDC lets us pass for real), regardless of whether a
    // given plugin actually reads z/color/matrix: a plugin that ignores
    // them simply never touches the extra fields, so this is behaviorally
    // equivalent to the original's more selective Init/Init3D/InitDC
    // dispatch (which picked based on which symbols a DLL happened to
    // export) while needing only one code path here.
    int (*initDC)(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                  void* pColor, double vvar, double a, double b, double c, double d, double e, double f);
    int (*getVariable)(void* state, const char* name, double* value);
    int (*setVariable)(void* state, const char* name, double* value);
    int (*resetVariable)(void* state, const char* name);
    int (*getNrVariables)();
    const char* (*getVariableNameAt)(int index);
};

class PluginVariation final : public Variation {
public:
    explicit PluginVariation(const PluginVTable& vt);
    ~PluginVariation() override;

    void prepare() override;
    void calc() override;

    int numVariables() const override { return vt_.getNrVariables(); }
    std::string variableNameAt(int index) const override { return vt_.getVariableNameAt(index); }
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    const PluginVTable& vt_;
    void* state_ = nullptr;
};

class PluginVariationFactory final : public VariationFactory {
public:
    PluginVariationFactory(std::string name, const PluginVTable& vt);

    std::string name() const override { return name_; }
    std::unique_ptr<Variation> create() const override { return std::make_unique<PluginVariation>(vt_); }
    int numVariables() const override { return vt_.getNrVariables(); }
    std::string variableNameAt(int index) const override { return vt_.getVariableNameAt(index); }

private:
    std::string name_;
    const PluginVTable& vt_;
};

// Called once per plugin, from a file-scope static initializer in each
// plugin's wrapper .cpp - matches registerVariation<T>()'s role for native
// variations (see VariationRegistration.h). `vt` must outlive the registry
// (a plain file-scope `const PluginVTable` in the wrapper .cpp, never
// freed, satisfies this trivially).
bool registerPluginVariation(std::string name, const PluginVTable& vt);

} // namespace apo
