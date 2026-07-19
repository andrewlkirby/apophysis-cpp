// Generated wrapper: registers stwins.c (see ../thirdparty/stwins.c) as an
// apo::Variation via PluginVariation. Paired with PluginStwins_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_stwins_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_stwins_Create();
int apo_plugin_stwins_Destroy(void** state);
int apo_plugin_stwins_Prepare(void* state);
int apo_plugin_stwins_Calc(void* state);
int apo_plugin_stwins_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_stwins_GetVariable(void* state, const char* name, double* value);
int apo_plugin_stwins_SetVariable(void* state, const char* name, double* value);
int apo_plugin_stwins_ResetVariable(void* state, const char* name);
int apo_plugin_stwins_GetNrVariables();
const char* apo_plugin_stwins_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_stwins_Create,       &apo_plugin_stwins_Destroy,       &apo_plugin_stwins_Prepare,
    &apo_plugin_stwins_Calc,         &apo_plugin_stwins_InitDC,        &apo_plugin_stwins_GetVariable,
    &apo_plugin_stwins_SetVariable,  &apo_plugin_stwins_ResetVariable, &apo_plugin_stwins_GetNrVariables,
    &apo_plugin_stwins_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("stwin", kVTable);
} // namespace
