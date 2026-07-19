// Generated wrapper: registers julia.c (see ../thirdparty/julia.c) as an
// apo::Variation via PluginVariation. Paired with PluginJulia_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_julia_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_julia_Create();
int apo_plugin_julia_Destroy(void** state);
int apo_plugin_julia_Prepare(void* state);
int apo_plugin_julia_Calc(void* state);
int apo_plugin_julia_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_julia_GetVariable(void* state, const char* name, double* value);
int apo_plugin_julia_SetVariable(void* state, const char* name, double* value);
int apo_plugin_julia_ResetVariable(void* state, const char* name);
int apo_plugin_julia_GetNrVariables();
const char* apo_plugin_julia_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_julia_Create,       &apo_plugin_julia_Destroy,       &apo_plugin_julia_Prepare,
    &apo_plugin_julia_Calc,         &apo_plugin_julia_InitDC,        &apo_plugin_julia_GetVariable,
    &apo_plugin_julia_SetVariable,  &apo_plugin_julia_ResetVariable, &apo_plugin_julia_GetNrVariables,
    &apo_plugin_julia_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("julia", kVTable);
} // namespace
