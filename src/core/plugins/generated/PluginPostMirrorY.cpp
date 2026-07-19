// Generated wrapper: registers post_mirror_y.c (see ../thirdparty/post_mirror_y.c) as an
// apo::Variation via PluginVariation. Paired with PluginPostMirrorY_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_post_mirror_y_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_post_mirror_y_Create();
int apo_plugin_post_mirror_y_Destroy(void** state);
int apo_plugin_post_mirror_y_Prepare(void* state);
int apo_plugin_post_mirror_y_Calc(void* state);
int apo_plugin_post_mirror_y_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_post_mirror_y_GetVariable(void* state, const char* name, double* value);
int apo_plugin_post_mirror_y_SetVariable(void* state, const char* name, double* value);
int apo_plugin_post_mirror_y_ResetVariable(void* state, const char* name);
int apo_plugin_post_mirror_y_GetNrVariables();
const char* apo_plugin_post_mirror_y_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_post_mirror_y_Create,       &apo_plugin_post_mirror_y_Destroy,       &apo_plugin_post_mirror_y_Prepare,
    &apo_plugin_post_mirror_y_Calc,         &apo_plugin_post_mirror_y_InitDC,        &apo_plugin_post_mirror_y_GetVariable,
    &apo_plugin_post_mirror_y_SetVariable,  &apo_plugin_post_mirror_y_ResetVariable, &apo_plugin_post_mirror_y_GetNrVariables,
    &apo_plugin_post_mirror_y_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("post_mirror_y", kVTable);
} // namespace
