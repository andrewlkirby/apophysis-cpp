// Generated wrapper: registers dc_ztransl.c (see ../thirdparty/dc_ztransl.c) as an
// apo::Variation via PluginVariation. Paired with PluginDcZtransl_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_dc_ztransl_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_dc_ztransl_Create();
int apo_plugin_dc_ztransl_Destroy(void** state);
int apo_plugin_dc_ztransl_Prepare(void* state);
int apo_plugin_dc_ztransl_Calc(void* state);
int apo_plugin_dc_ztransl_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_dc_ztransl_GetVariable(void* state, const char* name, double* value);
int apo_plugin_dc_ztransl_SetVariable(void* state, const char* name, double* value);
int apo_plugin_dc_ztransl_ResetVariable(void* state, const char* name);
int apo_plugin_dc_ztransl_GetNrVariables();
const char* apo_plugin_dc_ztransl_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_dc_ztransl_Create,       &apo_plugin_dc_ztransl_Destroy,       &apo_plugin_dc_ztransl_Prepare,
    &apo_plugin_dc_ztransl_Calc,         &apo_plugin_dc_ztransl_InitDC,        &apo_plugin_dc_ztransl_GetVariable,
    &apo_plugin_dc_ztransl_SetVariable,  &apo_plugin_dc_ztransl_ResetVariable, &apo_plugin_dc_ztransl_GetNrVariables,
    &apo_plugin_dc_ztransl_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("dc_ztransl", kVTable);
} // namespace
