// Generated wrapper: registers post_mirror_z.c (see ../thirdparty/post_mirror_z.c) as an
// apo::Variation via PluginVariation. Paired with PluginPostMirrorZ_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_post_mirror_z_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_post_mirror_z_Create();
int apo_plugin_post_mirror_z_Destroy(void** state);
int apo_plugin_post_mirror_z_Prepare(void* state);
int apo_plugin_post_mirror_z_Calc(void* state);
int apo_plugin_post_mirror_z_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_post_mirror_z_GetVariable(void* state, const char* name, double* value);
int apo_plugin_post_mirror_z_SetVariable(void* state, const char* name, double* value);
int apo_plugin_post_mirror_z_ResetVariable(void* state, const char* name);
int apo_plugin_post_mirror_z_GetNrVariables();
const char* apo_plugin_post_mirror_z_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_post_mirror_z_Create,       &apo_plugin_post_mirror_z_Destroy,       &apo_plugin_post_mirror_z_Prepare,
    &apo_plugin_post_mirror_z_Calc,         &apo_plugin_post_mirror_z_InitDC,        &apo_plugin_post_mirror_z_GetVariable,
    &apo_plugin_post_mirror_z_SetVariable,  &apo_plugin_post_mirror_z_ResetVariable, &apo_plugin_post_mirror_z_GetNrVariables,
    &apo_plugin_post_mirror_z_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("post_mirror_z", kVTable);
} // namespace
