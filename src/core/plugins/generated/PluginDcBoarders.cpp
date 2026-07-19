// Generated wrapper: registers dc_boarders.c (see ../thirdparty/dc_boarders.c) as an
// apo::Variation via PluginVariation. Paired with PluginDcBoarders_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_dc_boarders_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_dc_boarders_Create();
int apo_plugin_dc_boarders_Destroy(void** state);
int apo_plugin_dc_boarders_Prepare(void* state);
int apo_plugin_dc_boarders_Calc(void* state);
int apo_plugin_dc_boarders_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_dc_boarders_GetVariable(void* state, const char* name, double* value);
int apo_plugin_dc_boarders_SetVariable(void* state, const char* name, double* value);
int apo_plugin_dc_boarders_ResetVariable(void* state, const char* name);
int apo_plugin_dc_boarders_GetNrVariables();
const char* apo_plugin_dc_boarders_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_dc_boarders_Create,       &apo_plugin_dc_boarders_Destroy,       &apo_plugin_dc_boarders_Prepare,
    &apo_plugin_dc_boarders_Calc,         &apo_plugin_dc_boarders_InitDC,        &apo_plugin_dc_boarders_GetVariable,
    &apo_plugin_dc_boarders_SetVariable,  &apo_plugin_dc_boarders_ResetVariable, &apo_plugin_dc_boarders_GetNrVariables,
    &apo_plugin_dc_boarders_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("dc_boarders", kVTable);
} // namespace
