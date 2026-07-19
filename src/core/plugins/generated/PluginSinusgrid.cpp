// Generated wrapper: registers sinusgrid.c (see ../thirdparty/sinusgrid.c) as an
// apo::Variation via PluginVariation. Paired with PluginSinusgrid_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_sinusgrid_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_sinusgrid_Create();
int apo_plugin_sinusgrid_Destroy(void** state);
int apo_plugin_sinusgrid_Prepare(void* state);
int apo_plugin_sinusgrid_Calc(void* state);
int apo_plugin_sinusgrid_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_sinusgrid_GetVariable(void* state, const char* name, double* value);
int apo_plugin_sinusgrid_SetVariable(void* state, const char* name, double* value);
int apo_plugin_sinusgrid_ResetVariable(void* state, const char* name);
int apo_plugin_sinusgrid_GetNrVariables();
const char* apo_plugin_sinusgrid_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_sinusgrid_Create,       &apo_plugin_sinusgrid_Destroy,       &apo_plugin_sinusgrid_Prepare,
    &apo_plugin_sinusgrid_Calc,         &apo_plugin_sinusgrid_InitDC,        &apo_plugin_sinusgrid_GetVariable,
    &apo_plugin_sinusgrid_SetVariable,  &apo_plugin_sinusgrid_ResetVariable, &apo_plugin_sinusgrid_GetNrVariables,
    &apo_plugin_sinusgrid_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("sinusgrid", kVTable);
} // namespace
