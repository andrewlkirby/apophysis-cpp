// Generated wrapper: registers npolar.c (see ../thirdparty/npolar.c) as an
// apo::Variation via PluginVariation. Paired with PluginNpolar_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_npolar_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_npolar_Create();
int apo_plugin_npolar_Destroy(void** state);
int apo_plugin_npolar_Prepare(void* state);
int apo_plugin_npolar_Calc(void* state);
int apo_plugin_npolar_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_npolar_GetVariable(void* state, const char* name, double* value);
int apo_plugin_npolar_SetVariable(void* state, const char* name, double* value);
int apo_plugin_npolar_ResetVariable(void* state, const char* name);
int apo_plugin_npolar_GetNrVariables();
const char* apo_plugin_npolar_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_npolar_Create,       &apo_plugin_npolar_Destroy,       &apo_plugin_npolar_Prepare,
    &apo_plugin_npolar_Calc,         &apo_plugin_npolar_InitDC,        &apo_plugin_npolar_GetVariable,
    &apo_plugin_npolar_SetVariable,  &apo_plugin_npolar_ResetVariable, &apo_plugin_npolar_GetNrVariables,
    &apo_plugin_npolar_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("npolar", kVTable);
} // namespace
