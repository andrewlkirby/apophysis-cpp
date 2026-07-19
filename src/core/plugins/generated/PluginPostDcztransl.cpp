// Generated wrapper: registers post_dcztransl.c (see ../thirdparty/post_dcztransl.c) as an
// apo::Variation via PluginVariation. Paired with PluginPostDcztransl_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_post_dcztransl_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_post_dcztransl_Create();
int apo_plugin_post_dcztransl_Destroy(void** state);
int apo_plugin_post_dcztransl_Prepare(void* state);
int apo_plugin_post_dcztransl_Calc(void* state);
int apo_plugin_post_dcztransl_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_post_dcztransl_GetVariable(void* state, const char* name, double* value);
int apo_plugin_post_dcztransl_SetVariable(void* state, const char* name, double* value);
int apo_plugin_post_dcztransl_ResetVariable(void* state, const char* name);
int apo_plugin_post_dcztransl_GetNrVariables();
const char* apo_plugin_post_dcztransl_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_post_dcztransl_Create,       &apo_plugin_post_dcztransl_Destroy,       &apo_plugin_post_dcztransl_Prepare,
    &apo_plugin_post_dcztransl_Calc,         &apo_plugin_post_dcztransl_InitDC,        &apo_plugin_post_dcztransl_GetVariable,
    &apo_plugin_post_dcztransl_SetVariable,  &apo_plugin_post_dcztransl_ResetVariable, &apo_plugin_post_dcztransl_GetNrVariables,
    &apo_plugin_post_dcztransl_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("post_dcztransl", kVTable);
} // namespace
