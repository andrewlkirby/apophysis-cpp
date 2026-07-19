// Generated wrapper: registers falloff.c (see ../thirdparty/falloff.c) as an
// apo::Variation via PluginVariation. Paired with PluginFalloff_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_falloff_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_falloff_Create();
int apo_plugin_falloff_Destroy(void** state);
int apo_plugin_falloff_Prepare(void* state);
int apo_plugin_falloff_Calc(void* state);
int apo_plugin_falloff_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_falloff_GetVariable(void* state, const char* name, double* value);
int apo_plugin_falloff_SetVariable(void* state, const char* name, double* value);
int apo_plugin_falloff_ResetVariable(void* state, const char* name);
int apo_plugin_falloff_GetNrVariables();
const char* apo_plugin_falloff_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_falloff_Create,       &apo_plugin_falloff_Destroy,       &apo_plugin_falloff_Prepare,
    &apo_plugin_falloff_Calc,         &apo_plugin_falloff_InitDC,        &apo_plugin_falloff_GetVariable,
    &apo_plugin_falloff_SetVariable,  &apo_plugin_falloff_ResetVariable, &apo_plugin_falloff_GetNrVariables,
    &apo_plugin_falloff_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("falloff", kVTable);
} // namespace
