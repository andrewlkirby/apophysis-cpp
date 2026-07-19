// Generated wrapper: registers spherical2.c (see ../thirdparty/spherical2.c) as an
// apo::Variation via PluginVariation. Paired with PluginSpherical2_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_spherical2_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_spherical2_Create();
int apo_plugin_spherical2_Destroy(void** state);
int apo_plugin_spherical2_Prepare(void* state);
int apo_plugin_spherical2_Calc(void* state);
int apo_plugin_spherical2_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_spherical2_GetVariable(void* state, const char* name, double* value);
int apo_plugin_spherical2_SetVariable(void* state, const char* name, double* value);
int apo_plugin_spherical2_ResetVariable(void* state, const char* name);
int apo_plugin_spherical2_GetNrVariables();
const char* apo_plugin_spherical2_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_spherical2_Create,       &apo_plugin_spherical2_Destroy,       &apo_plugin_spherical2_Prepare,
    &apo_plugin_spherical2_Calc,         &apo_plugin_spherical2_InitDC,        &apo_plugin_spherical2_GetVariable,
    &apo_plugin_spherical2_SetVariable,  &apo_plugin_spherical2_ResetVariable, &apo_plugin_spherical2_GetNrVariables,
    &apo_plugin_spherical2_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("spherical2", kVTable);
} // namespace
