// Generated wrapper: registers xtrb.c (see ../thirdparty/xtrb.c) as an
// apo::Variation via PluginVariation. Paired with PluginXtrb_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_xtrb_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_xtrb_Create();
int apo_plugin_xtrb_Destroy(void** state);
int apo_plugin_xtrb_Prepare(void* state);
int apo_plugin_xtrb_Calc(void* state);
int apo_plugin_xtrb_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_xtrb_GetVariable(void* state, const char* name, double* value);
int apo_plugin_xtrb_SetVariable(void* state, const char* name, double* value);
int apo_plugin_xtrb_ResetVariable(void* state, const char* name);
int apo_plugin_xtrb_GetNrVariables();
const char* apo_plugin_xtrb_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_xtrb_Create,       &apo_plugin_xtrb_Destroy,       &apo_plugin_xtrb_Prepare,
    &apo_plugin_xtrb_Calc,         &apo_plugin_xtrb_InitDC,        &apo_plugin_xtrb_GetVariable,
    &apo_plugin_xtrb_SetVariable,  &apo_plugin_xtrb_ResetVariable, &apo_plugin_xtrb_GetNrVariables,
    &apo_plugin_xtrb_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("xtrb", kVTable);
} // namespace
