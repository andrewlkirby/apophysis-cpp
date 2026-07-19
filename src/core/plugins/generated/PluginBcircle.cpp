// Generated wrapper: registers bcircle.c (see ../thirdparty/bcircle.c) as an
// apo::Variation via PluginVariation. Paired with PluginBcircle_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_bcircle_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_bcircle_Create();
int apo_plugin_bcircle_Destroy(void** state);
int apo_plugin_bcircle_Prepare(void* state);
int apo_plugin_bcircle_Calc(void* state);
int apo_plugin_bcircle_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                               void* pColor, double vvar, double a, double b, double c, double d, double e,
                               double f);
int apo_plugin_bcircle_GetVariable(void* state, const char* name, double* value);
int apo_plugin_bcircle_SetVariable(void* state, const char* name, double* value);
int apo_plugin_bcircle_ResetVariable(void* state, const char* name);
int apo_plugin_bcircle_GetNrVariables();
const char* apo_plugin_bcircle_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_bcircle_Create,       &apo_plugin_bcircle_Destroy,     &apo_plugin_bcircle_Prepare,
    &apo_plugin_bcircle_Calc,         &apo_plugin_bcircle_InitDC,      &apo_plugin_bcircle_GetVariable,
    &apo_plugin_bcircle_SetVariable,  &apo_plugin_bcircle_ResetVariable, &apo_plugin_bcircle_GetNrVariables,
    &apo_plugin_bcircle_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("bcircle", kVTable);
} // namespace
