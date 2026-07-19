// Generated wrapper: registers sigmoid.c (see ../thirdparty/sigmoid.c) as an
// apo::Variation via PluginVariation. Paired with PluginSigmoid_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_sigmoid_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_sigmoid_Create();
int apo_plugin_sigmoid_Destroy(void** state);
int apo_plugin_sigmoid_Prepare(void* state);
int apo_plugin_sigmoid_Calc(void* state);
int apo_plugin_sigmoid_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_sigmoid_GetVariable(void* state, const char* name, double* value);
int apo_plugin_sigmoid_SetVariable(void* state, const char* name, double* value);
int apo_plugin_sigmoid_ResetVariable(void* state, const char* name);
int apo_plugin_sigmoid_GetNrVariables();
const char* apo_plugin_sigmoid_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_sigmoid_Create,       &apo_plugin_sigmoid_Destroy,       &apo_plugin_sigmoid_Prepare,
    &apo_plugin_sigmoid_Calc,         &apo_plugin_sigmoid_InitDC,        &apo_plugin_sigmoid_GetVariable,
    &apo_plugin_sigmoid_SetVariable,  &apo_plugin_sigmoid_ResetVariable, &apo_plugin_sigmoid_GetNrVariables,
    &apo_plugin_sigmoid_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("sigmoid", kVTable);
} // namespace
