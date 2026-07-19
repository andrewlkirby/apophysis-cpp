// Generated wrapper: registers ex.c (see ../thirdparty/ex.c) as an
// apo::Variation via PluginVariation. Paired with PluginEx_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_ex_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_ex_Create();
int apo_plugin_ex_Destroy(void** state);
int apo_plugin_ex_Prepare(void* state);
int apo_plugin_ex_Calc(void* state);
int apo_plugin_ex_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_ex_GetVariable(void* state, const char* name, double* value);
int apo_plugin_ex_SetVariable(void* state, const char* name, double* value);
int apo_plugin_ex_ResetVariable(void* state, const char* name);
int apo_plugin_ex_GetNrVariables();
const char* apo_plugin_ex_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_ex_Create,       &apo_plugin_ex_Destroy,       &apo_plugin_ex_Prepare,
    &apo_plugin_ex_Calc,         &apo_plugin_ex_InitDC,        &apo_plugin_ex_GetVariable,
    &apo_plugin_ex_SetVariable,  &apo_plugin_ex_ResetVariable, &apo_plugin_ex_GetNrVariables,
    &apo_plugin_ex_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("ex", kVTable);
} // namespace
