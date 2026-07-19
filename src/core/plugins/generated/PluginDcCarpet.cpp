// Generated wrapper: registers dc_carpet.c (see ../thirdparty/dc_carpet.c) as an
// apo::Variation via PluginVariation. Paired with PluginDcCarpet_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_dc_carpet_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_dc_carpet_Create();
int apo_plugin_dc_carpet_Destroy(void** state);
int apo_plugin_dc_carpet_Prepare(void* state);
int apo_plugin_dc_carpet_Calc(void* state);
int apo_plugin_dc_carpet_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_dc_carpet_GetVariable(void* state, const char* name, double* value);
int apo_plugin_dc_carpet_SetVariable(void* state, const char* name, double* value);
int apo_plugin_dc_carpet_ResetVariable(void* state, const char* name);
int apo_plugin_dc_carpet_GetNrVariables();
const char* apo_plugin_dc_carpet_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_dc_carpet_Create,       &apo_plugin_dc_carpet_Destroy,       &apo_plugin_dc_carpet_Prepare,
    &apo_plugin_dc_carpet_Calc,         &apo_plugin_dc_carpet_InitDC,        &apo_plugin_dc_carpet_GetVariable,
    &apo_plugin_dc_carpet_SetVariable,  &apo_plugin_dc_carpet_ResetVariable, &apo_plugin_dc_carpet_GetNrVariables,
    &apo_plugin_dc_carpet_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("dc_carpet", kVTable);
} // namespace
