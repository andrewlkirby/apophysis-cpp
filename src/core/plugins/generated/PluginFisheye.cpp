// Generated wrapper: registers fisheye.c (see ../thirdparty/fisheye.c) as an
// apo::Variation via PluginVariation. Paired with PluginFisheye_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_fisheye_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_fisheye_Create();
int apo_plugin_fisheye_Destroy(void** state);
int apo_plugin_fisheye_Prepare(void* state);
int apo_plugin_fisheye_Calc(void* state);
int apo_plugin_fisheye_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_fisheye_GetVariable(void* state, const char* name, double* value);
int apo_plugin_fisheye_SetVariable(void* state, const char* name, double* value);
int apo_plugin_fisheye_ResetVariable(void* state, const char* name);
int apo_plugin_fisheye_GetNrVariables();
const char* apo_plugin_fisheye_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_fisheye_Create,       &apo_plugin_fisheye_Destroy,       &apo_plugin_fisheye_Prepare,
    &apo_plugin_fisheye_Calc,         &apo_plugin_fisheye_InitDC,        &apo_plugin_fisheye_GetVariable,
    &apo_plugin_fisheye_SetVariable,  &apo_plugin_fisheye_ResetVariable, &apo_plugin_fisheye_GetNrVariables,
    &apo_plugin_fisheye_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("fisheye", kVTable);
} // namespace
