// Generated wrapper: registers post_rblur.c (see ../thirdparty/post_rblur.c) as an
// apo::Variation via PluginVariation. Paired with PluginPostRblur_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_post_rblur_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_post_rblur_Create();
int apo_plugin_post_rblur_Destroy(void** state);
int apo_plugin_post_rblur_Prepare(void* state);
int apo_plugin_post_rblur_Calc(void* state);
int apo_plugin_post_rblur_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_post_rblur_GetVariable(void* state, const char* name, double* value);
int apo_plugin_post_rblur_SetVariable(void* state, const char* name, double* value);
int apo_plugin_post_rblur_ResetVariable(void* state, const char* name);
int apo_plugin_post_rblur_GetNrVariables();
const char* apo_plugin_post_rblur_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_post_rblur_Create,       &apo_plugin_post_rblur_Destroy,       &apo_plugin_post_rblur_Prepare,
    &apo_plugin_post_rblur_Calc,         &apo_plugin_post_rblur_InitDC,        &apo_plugin_post_rblur_GetVariable,
    &apo_plugin_post_rblur_SetVariable,  &apo_plugin_post_rblur_ResetVariable, &apo_plugin_post_rblur_GetNrVariables,
    &apo_plugin_post_rblur_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("post_rblur", kVTable);
} // namespace
