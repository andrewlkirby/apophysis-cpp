// Generated wrapper: registers coswrap.c (see ../thirdparty/coswrap.c) as an
// apo::Variation via PluginVariation. Paired with PluginCoswrap_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_coswrap_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_coswrap_Create();
int apo_plugin_coswrap_Destroy(void** state);
int apo_plugin_coswrap_Prepare(void* state);
int apo_plugin_coswrap_Calc(void* state);
int apo_plugin_coswrap_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_coswrap_GetVariable(void* state, const char* name, double* value);
int apo_plugin_coswrap_SetVariable(void* state, const char* name, double* value);
int apo_plugin_coswrap_ResetVariable(void* state, const char* name);
int apo_plugin_coswrap_GetNrVariables();
const char* apo_plugin_coswrap_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_coswrap_Create,       &apo_plugin_coswrap_Destroy,       &apo_plugin_coswrap_Prepare,
    &apo_plugin_coswrap_Calc,         &apo_plugin_coswrap_InitDC,        &apo_plugin_coswrap_GetVariable,
    &apo_plugin_coswrap_SetVariable,  &apo_plugin_coswrap_ResetVariable, &apo_plugin_coswrap_GetNrVariables,
    &apo_plugin_coswrap_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("coswrap", kVTable);
} // namespace
