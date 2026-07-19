// Generated wrapper: registers wavesn.c (see ../thirdparty/wavesn.c) as an
// apo::Variation via PluginVariation. Paired with PluginWavesn_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_wavesn_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_wavesn_Create();
int apo_plugin_wavesn_Destroy(void** state);
int apo_plugin_wavesn_Prepare(void* state);
int apo_plugin_wavesn_Calc(void* state);
int apo_plugin_wavesn_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_wavesn_GetVariable(void* state, const char* name, double* value);
int apo_plugin_wavesn_SetVariable(void* state, const char* name, double* value);
int apo_plugin_wavesn_ResetVariable(void* state, const char* name);
int apo_plugin_wavesn_GetNrVariables();
const char* apo_plugin_wavesn_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_wavesn_Create,       &apo_plugin_wavesn_Destroy,       &apo_plugin_wavesn_Prepare,
    &apo_plugin_wavesn_Calc,         &apo_plugin_wavesn_InitDC,        &apo_plugin_wavesn_GetVariable,
    &apo_plugin_wavesn_SetVariable,  &apo_plugin_wavesn_ResetVariable, &apo_plugin_wavesn_GetNrVariables,
    &apo_plugin_wavesn_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("wavesn", kVTable);
} // namespace
