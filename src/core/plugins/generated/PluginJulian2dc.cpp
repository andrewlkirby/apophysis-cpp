// Generated wrapper: registers julian2dc.c (see ../thirdparty/julian2dc.c) as an
// apo::Variation via PluginVariation. Paired with PluginJulian2dc_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_julian2dc_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_julian2dc_Create();
int apo_plugin_julian2dc_Destroy(void** state);
int apo_plugin_julian2dc_Prepare(void* state);
int apo_plugin_julian2dc_Calc(void* state);
int apo_plugin_julian2dc_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_julian2dc_GetVariable(void* state, const char* name, double* value);
int apo_plugin_julian2dc_SetVariable(void* state, const char* name, double* value);
int apo_plugin_julian2dc_ResetVariable(void* state, const char* name);
int apo_plugin_julian2dc_GetNrVariables();
const char* apo_plugin_julian2dc_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_julian2dc_Create,       &apo_plugin_julian2dc_Destroy,       &apo_plugin_julian2dc_Prepare,
    &apo_plugin_julian2dc_Calc,         &apo_plugin_julian2dc_InitDC,        &apo_plugin_julian2dc_GetVariable,
    &apo_plugin_julian2dc_SetVariable,  &apo_plugin_julian2dc_ResetVariable, &apo_plugin_julian2dc_GetNrVariables,
    &apo_plugin_julian2dc_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("julian2dc", kVTable);
} // namespace
