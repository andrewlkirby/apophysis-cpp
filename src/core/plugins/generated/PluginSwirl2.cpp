// Generated wrapper: registers swirl2.c (see ../thirdparty/swirl2.c) as an
// apo::Variation via PluginVariation. Paired with PluginSwirl2_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_swirl2_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_swirl2_Create();
int apo_plugin_swirl2_Destroy(void** state);
int apo_plugin_swirl2_Prepare(void* state);
int apo_plugin_swirl2_Calc(void* state);
int apo_plugin_swirl2_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_swirl2_GetVariable(void* state, const char* name, double* value);
int apo_plugin_swirl2_SetVariable(void* state, const char* name, double* value);
int apo_plugin_swirl2_ResetVariable(void* state, const char* name);
int apo_plugin_swirl2_GetNrVariables();
const char* apo_plugin_swirl2_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_swirl2_Create,       &apo_plugin_swirl2_Destroy,       &apo_plugin_swirl2_Prepare,
    &apo_plugin_swirl2_Calc,         &apo_plugin_swirl2_InitDC,        &apo_plugin_swirl2_GetVariable,
    &apo_plugin_swirl2_SetVariable,  &apo_plugin_swirl2_ResetVariable, &apo_plugin_swirl2_GetNrVariables,
    &apo_plugin_swirl2_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("psphere", kVTable);
} // namespace
