// Generated wrapper: registers dc_gridout.c (see ../thirdparty/dc_gridout.c) as an
// apo::Variation via PluginVariation. Paired with PluginDcGridout_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_dc_gridout_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_dc_gridout_Create();
int apo_plugin_dc_gridout_Destroy(void** state);
int apo_plugin_dc_gridout_Prepare(void* state);
int apo_plugin_dc_gridout_Calc(void* state);
int apo_plugin_dc_gridout_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_dc_gridout_GetVariable(void* state, const char* name, double* value);
int apo_plugin_dc_gridout_SetVariable(void* state, const char* name, double* value);
int apo_plugin_dc_gridout_ResetVariable(void* state, const char* name);
int apo_plugin_dc_gridout_GetNrVariables();
const char* apo_plugin_dc_gridout_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_dc_gridout_Create,       &apo_plugin_dc_gridout_Destroy,       &apo_plugin_dc_gridout_Prepare,
    &apo_plugin_dc_gridout_Calc,         &apo_plugin_dc_gridout_InitDC,        &apo_plugin_dc_gridout_GetVariable,
    &apo_plugin_dc_gridout_SetVariable,  &apo_plugin_dc_gridout_ResetVariable, &apo_plugin_dc_gridout_GetNrVariables,
    &apo_plugin_dc_gridout_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("dc_gridout", kVTable);
} // namespace
