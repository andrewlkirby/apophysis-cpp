// Generated wrapper: registers dc_triangle.c (see ../thirdparty/dc_triangle.c) as an
// apo::Variation via PluginVariation. Paired with PluginDcTriangle_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_dc_triangle_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_dc_triangle_Create();
int apo_plugin_dc_triangle_Destroy(void** state);
int apo_plugin_dc_triangle_Prepare(void* state);
int apo_plugin_dc_triangle_Calc(void* state);
int apo_plugin_dc_triangle_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_dc_triangle_GetVariable(void* state, const char* name, double* value);
int apo_plugin_dc_triangle_SetVariable(void* state, const char* name, double* value);
int apo_plugin_dc_triangle_ResetVariable(void* state, const char* name);
int apo_plugin_dc_triangle_GetNrVariables();
const char* apo_plugin_dc_triangle_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_dc_triangle_Create,       &apo_plugin_dc_triangle_Destroy,       &apo_plugin_dc_triangle_Prepare,
    &apo_plugin_dc_triangle_Calc,         &apo_plugin_dc_triangle_InitDC,        &apo_plugin_dc_triangle_GetVariable,
    &apo_plugin_dc_triangle_SetVariable,  &apo_plugin_dc_triangle_ResetVariable, &apo_plugin_dc_triangle_GetNrVariables,
    &apo_plugin_dc_triangle_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("dc_triangle", kVTable);
} // namespace
