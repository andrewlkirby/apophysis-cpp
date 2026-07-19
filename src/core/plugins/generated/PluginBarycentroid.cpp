// Generated wrapper: registers barycentroid.c (see
// ../thirdparty/barycentroid.c) as an apo::Variation via PluginVariation.
// Paired with PluginBarycentroid_impl.c, which compiles the plugin itself
// as C with its symbols prefixed apo_plugin_barycentroid_* (declared
// extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_barycentroid_Create();
int apo_plugin_barycentroid_Destroy(void** state);
int apo_plugin_barycentroid_Prepare(void* state);
int apo_plugin_barycentroid_Calc(void* state);
int apo_plugin_barycentroid_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy,
                                    void* pFTz, void* pColor, double vvar, double a, double b, double c, double d,
                                    double e, double f);
int apo_plugin_barycentroid_GetVariable(void* state, const char* name, double* value);
int apo_plugin_barycentroid_SetVariable(void* state, const char* name, double* value);
int apo_plugin_barycentroid_ResetVariable(void* state, const char* name);
int apo_plugin_barycentroid_GetNrVariables();
const char* apo_plugin_barycentroid_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_barycentroid_Create,
    &apo_plugin_barycentroid_Destroy,
    &apo_plugin_barycentroid_Prepare,
    &apo_plugin_barycentroid_Calc,
    &apo_plugin_barycentroid_InitDC,
    &apo_plugin_barycentroid_GetVariable,
    &apo_plugin_barycentroid_SetVariable,
    &apo_plugin_barycentroid_ResetVariable,
    &apo_plugin_barycentroid_GetNrVariables,
    &apo_plugin_barycentroid_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("barycentroid", kVTable);
} // namespace
