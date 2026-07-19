// Generated wrapper: registers octapol.c (see ../thirdparty/octapol.c) as an
// apo::Variation via PluginVariation. Paired with PluginOctapol_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_octapol_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_octapol_Create();
int apo_plugin_octapol_Destroy(void** state);
int apo_plugin_octapol_Prepare(void* state);
int apo_plugin_octapol_Calc(void* state);
int apo_plugin_octapol_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_octapol_GetVariable(void* state, const char* name, double* value);
int apo_plugin_octapol_SetVariable(void* state, const char* name, double* value);
int apo_plugin_octapol_ResetVariable(void* state, const char* name);
int apo_plugin_octapol_GetNrVariables();
const char* apo_plugin_octapol_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_octapol_Create,       &apo_plugin_octapol_Destroy,       &apo_plugin_octapol_Prepare,
    &apo_plugin_octapol_Calc,         &apo_plugin_octapol_InitDC,        &apo_plugin_octapol_GetVariable,
    &apo_plugin_octapol_SetVariable,  &apo_plugin_octapol_ResetVariable, &apo_plugin_octapol_GetNrVariables,
    &apo_plugin_octapol_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("octapol", kVTable);
} // namespace
