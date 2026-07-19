// Generated wrapper: registers gdoffs.c (see ../thirdparty/gdoffs.c) as an
// apo::Variation via PluginVariation. Paired with PluginGdoffs_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_gdoffs_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_gdoffs_Create();
int apo_plugin_gdoffs_Destroy(void** state);
int apo_plugin_gdoffs_Prepare(void* state);
int apo_plugin_gdoffs_Calc(void* state);
int apo_plugin_gdoffs_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_gdoffs_GetVariable(void* state, const char* name, double* value);
int apo_plugin_gdoffs_SetVariable(void* state, const char* name, double* value);
int apo_plugin_gdoffs_ResetVariable(void* state, const char* name);
int apo_plugin_gdoffs_GetNrVariables();
const char* apo_plugin_gdoffs_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_gdoffs_Create,       &apo_plugin_gdoffs_Destroy,       &apo_plugin_gdoffs_Prepare,
    &apo_plugin_gdoffs_Calc,         &apo_plugin_gdoffs_InitDC,        &apo_plugin_gdoffs_GetVariable,
    &apo_plugin_gdoffs_SetVariable,  &apo_plugin_gdoffs_ResetVariable, &apo_plugin_gdoffs_GetNrVariables,
    &apo_plugin_gdoffs_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("gdoffs", kVTable);
} // namespace
