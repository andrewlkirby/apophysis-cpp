// Generated wrapper: registers hexcrop.c (see ../thirdparty/hexcrop.c) as an
// apo::Variation via PluginVariation. Paired with PluginHexcrop_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_hexcrop_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_hexcrop_Create();
int apo_plugin_hexcrop_Destroy(void** state);
int apo_plugin_hexcrop_Prepare(void* state);
int apo_plugin_hexcrop_Calc(void* state);
int apo_plugin_hexcrop_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_hexcrop_GetVariable(void* state, const char* name, double* value);
int apo_plugin_hexcrop_SetVariable(void* state, const char* name, double* value);
int apo_plugin_hexcrop_ResetVariable(void* state, const char* name);
int apo_plugin_hexcrop_GetNrVariables();
const char* apo_plugin_hexcrop_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_hexcrop_Create,       &apo_plugin_hexcrop_Destroy,       &apo_plugin_hexcrop_Prepare,
    &apo_plugin_hexcrop_Calc,         &apo_plugin_hexcrop_InitDC,        &apo_plugin_hexcrop_GetVariable,
    &apo_plugin_hexcrop_SetVariable,  &apo_plugin_hexcrop_ResetVariable, &apo_plugin_hexcrop_GetNrVariables,
    &apo_plugin_hexcrop_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("hexcrop", kVTable);
} // namespace
