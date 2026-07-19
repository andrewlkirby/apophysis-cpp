// Generated wrapper: registers post_mirror_x.c (see ../thirdparty/post_mirror_x.c) as an
// apo::Variation via PluginVariation. Paired with PluginPostMirrorX_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_post_mirror_x_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_post_mirror_x_Create();
int apo_plugin_post_mirror_x_Destroy(void** state);
int apo_plugin_post_mirror_x_Prepare(void* state);
int apo_plugin_post_mirror_x_Calc(void* state);
int apo_plugin_post_mirror_x_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_post_mirror_x_GetVariable(void* state, const char* name, double* value);
int apo_plugin_post_mirror_x_SetVariable(void* state, const char* name, double* value);
int apo_plugin_post_mirror_x_ResetVariable(void* state, const char* name);
int apo_plugin_post_mirror_x_GetNrVariables();
const char* apo_plugin_post_mirror_x_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_post_mirror_x_Create,       &apo_plugin_post_mirror_x_Destroy,       &apo_plugin_post_mirror_x_Prepare,
    &apo_plugin_post_mirror_x_Calc,         &apo_plugin_post_mirror_x_InitDC,        &apo_plugin_post_mirror_x_GetVariable,
    &apo_plugin_post_mirror_x_SetVariable,  &apo_plugin_post_mirror_x_ResetVariable, &apo_plugin_post_mirror_x_GetNrVariables,
    &apo_plugin_post_mirror_x_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("post_mirror_x", kVTable);
} // namespace
