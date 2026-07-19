// Generated wrapper: registers pre_boarders2.c (see ../thirdparty/pre_boarders2.c) as an
// apo::Variation via PluginVariation. Paired with PluginPreBoarders2_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_pre_boarders2_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_pre_boarders2_Create();
int apo_plugin_pre_boarders2_Destroy(void** state);
int apo_plugin_pre_boarders2_Prepare(void* state);
int apo_plugin_pre_boarders2_Calc(void* state);
int apo_plugin_pre_boarders2_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_pre_boarders2_GetVariable(void* state, const char* name, double* value);
int apo_plugin_pre_boarders2_SetVariable(void* state, const char* name, double* value);
int apo_plugin_pre_boarders2_ResetVariable(void* state, const char* name);
int apo_plugin_pre_boarders2_GetNrVariables();
const char* apo_plugin_pre_boarders2_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_pre_boarders2_Create,       &apo_plugin_pre_boarders2_Destroy,       &apo_plugin_pre_boarders2_Prepare,
    &apo_plugin_pre_boarders2_Calc,         &apo_plugin_pre_boarders2_InitDC,        &apo_plugin_pre_boarders2_GetVariable,
    &apo_plugin_pre_boarders2_SetVariable,  &apo_plugin_pre_boarders2_ResetVariable, &apo_plugin_pre_boarders2_GetNrVariables,
    &apo_plugin_pre_boarders2_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("pre_boarders2", kVTable);
} // namespace
