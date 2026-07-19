// Generated wrapper: registers xheart.c (see ../thirdparty/xheart.c) as an
// apo::Variation via PluginVariation. Paired with PluginXheart_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_xheart_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_xheart_Create();
int apo_plugin_xheart_Destroy(void** state);
int apo_plugin_xheart_Prepare(void* state);
int apo_plugin_xheart_Calc(void* state);
int apo_plugin_xheart_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_xheart_GetVariable(void* state, const char* name, double* value);
int apo_plugin_xheart_SetVariable(void* state, const char* name, double* value);
int apo_plugin_xheart_ResetVariable(void* state, const char* name);
int apo_plugin_xheart_GetNrVariables();
const char* apo_plugin_xheart_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_xheart_Create,       &apo_plugin_xheart_Destroy,       &apo_plugin_xheart_Prepare,
    &apo_plugin_xheart_Calc,         &apo_plugin_xheart_InitDC,        &apo_plugin_xheart_GetVariable,
    &apo_plugin_xheart_SetVariable,  &apo_plugin_xheart_ResetVariable, &apo_plugin_xheart_GetNrVariables,
    &apo_plugin_xheart_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("xheart", kVTable);
} // namespace
