// Generated wrapper: registers dc_bubble.c (see ../thirdparty/dc_bubble.c) as an
// apo::Variation via PluginVariation. Paired with PluginDcBubble_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_dc_bubble_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_dc_bubble_Create();
int apo_plugin_dc_bubble_Destroy(void** state);
int apo_plugin_dc_bubble_Prepare(void* state);
int apo_plugin_dc_bubble_Calc(void* state);
int apo_plugin_dc_bubble_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_dc_bubble_GetVariable(void* state, const char* name, double* value);
int apo_plugin_dc_bubble_SetVariable(void* state, const char* name, double* value);
int apo_plugin_dc_bubble_ResetVariable(void* state, const char* name);
int apo_plugin_dc_bubble_GetNrVariables();
const char* apo_plugin_dc_bubble_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_dc_bubble_Create,       &apo_plugin_dc_bubble_Destroy,       &apo_plugin_dc_bubble_Prepare,
    &apo_plugin_dc_bubble_Calc,         &apo_plugin_dc_bubble_InitDC,        &apo_plugin_dc_bubble_GetVariable,
    &apo_plugin_dc_bubble_SetVariable,  &apo_plugin_dc_bubble_ResetVariable, &apo_plugin_dc_bubble_GetNrVariables,
    &apo_plugin_dc_bubble_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("dc_bubble", kVTable);
} // namespace
