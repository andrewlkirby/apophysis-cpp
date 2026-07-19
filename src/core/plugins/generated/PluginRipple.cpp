// Generated wrapper: registers ripple.c (see ../thirdparty/ripple.c) as an
// apo::Variation via PluginVariation. Paired with PluginRipple_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_ripple_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_ripple_Create();
int apo_plugin_ripple_Destroy(void** state);
int apo_plugin_ripple_Prepare(void* state);
int apo_plugin_ripple_Calc(void* state);
int apo_plugin_ripple_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_ripple_GetVariable(void* state, const char* name, double* value);
int apo_plugin_ripple_SetVariable(void* state, const char* name, double* value);
int apo_plugin_ripple_ResetVariable(void* state, const char* name);
int apo_plugin_ripple_GetNrVariables();
const char* apo_plugin_ripple_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_ripple_Create,       &apo_plugin_ripple_Destroy,       &apo_plugin_ripple_Prepare,
    &apo_plugin_ripple_Calc,         &apo_plugin_ripple_InitDC,        &apo_plugin_ripple_GetVariable,
    &apo_plugin_ripple_SetVariable,  &apo_plugin_ripple_ResetVariable, &apo_plugin_ripple_GetNrVariables,
    &apo_plugin_ripple_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("ripple", kVTable);
} // namespace
