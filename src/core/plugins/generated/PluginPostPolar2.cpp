// Generated wrapper: registers post_polar2.c (see ../thirdparty/post_polar2.c) as an
// apo::Variation via PluginVariation. Paired with PluginPostPolar2_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_post_polar2_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_post_polar2_Create();
int apo_plugin_post_polar2_Destroy(void** state);
int apo_plugin_post_polar2_Prepare(void* state);
int apo_plugin_post_polar2_Calc(void* state);
int apo_plugin_post_polar2_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_post_polar2_GetVariable(void* state, const char* name, double* value);
int apo_plugin_post_polar2_SetVariable(void* state, const char* name, double* value);
int apo_plugin_post_polar2_ResetVariable(void* state, const char* name);
int apo_plugin_post_polar2_GetNrVariables();
const char* apo_plugin_post_polar2_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_post_polar2_Create,       &apo_plugin_post_polar2_Destroy,       &apo_plugin_post_polar2_Prepare,
    &apo_plugin_post_polar2_Calc,         &apo_plugin_post_polar2_InitDC,        &apo_plugin_post_polar2_GetVariable,
    &apo_plugin_post_polar2_SetVariable,  &apo_plugin_post_polar2_ResetVariable, &apo_plugin_post_polar2_GetNrVariables,
    &apo_plugin_post_polar2_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("post_polar2", kVTable);
} // namespace
