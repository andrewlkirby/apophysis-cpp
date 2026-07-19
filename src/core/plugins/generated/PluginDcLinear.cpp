// Generated wrapper: registers dc_linear.c (see ../thirdparty/dc_linear.c) as an
// apo::Variation via PluginVariation. Paired with PluginDcLinear_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_dc_linear_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_dc_linear_Create();
int apo_plugin_dc_linear_Destroy(void** state);
int apo_plugin_dc_linear_Prepare(void* state);
int apo_plugin_dc_linear_Calc(void* state);
int apo_plugin_dc_linear_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_dc_linear_GetVariable(void* state, const char* name, double* value);
int apo_plugin_dc_linear_SetVariable(void* state, const char* name, double* value);
int apo_plugin_dc_linear_ResetVariable(void* state, const char* name);
int apo_plugin_dc_linear_GetNrVariables();
const char* apo_plugin_dc_linear_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_dc_linear_Create,       &apo_plugin_dc_linear_Destroy,       &apo_plugin_dc_linear_Prepare,
    &apo_plugin_dc_linear_Calc,         &apo_plugin_dc_linear_InitDC,        &apo_plugin_dc_linear_GetVariable,
    &apo_plugin_dc_linear_SetVariable,  &apo_plugin_dc_linear_ResetVariable, &apo_plugin_dc_linear_GetNrVariables,
    &apo_plugin_dc_linear_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("dc_linear", kVTable);
} // namespace
