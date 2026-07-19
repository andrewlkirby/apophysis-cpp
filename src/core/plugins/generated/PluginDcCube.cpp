// Generated wrapper: registers dc_cube.c (see ../thirdparty/dc_cube.c) as an
// apo::Variation via PluginVariation. Paired with PluginDcCube_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_dc_cube_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_dc_cube_Create();
int apo_plugin_dc_cube_Destroy(void** state);
int apo_plugin_dc_cube_Prepare(void* state);
int apo_plugin_dc_cube_Calc(void* state);
int apo_plugin_dc_cube_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_dc_cube_GetVariable(void* state, const char* name, double* value);
int apo_plugin_dc_cube_SetVariable(void* state, const char* name, double* value);
int apo_plugin_dc_cube_ResetVariable(void* state, const char* name);
int apo_plugin_dc_cube_GetNrVariables();
const char* apo_plugin_dc_cube_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_dc_cube_Create,       &apo_plugin_dc_cube_Destroy,       &apo_plugin_dc_cube_Prepare,
    &apo_plugin_dc_cube_Calc,         &apo_plugin_dc_cube_InitDC,        &apo_plugin_dc_cube_GetVariable,
    &apo_plugin_dc_cube_SetVariable,  &apo_plugin_dc_cube_ResetVariable, &apo_plugin_dc_cube_GetNrVariables,
    &apo_plugin_dc_cube_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("dc_cube", kVTable);
} // namespace
