// Generated wrapper: registers handkerchief.c (see ../thirdparty/handkerchief.c) as an
// apo::Variation via PluginVariation. Paired with PluginHandkerchief_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_handkerchief_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_handkerchief_Create();
int apo_plugin_handkerchief_Destroy(void** state);
int apo_plugin_handkerchief_Prepare(void* state);
int apo_plugin_handkerchief_Calc(void* state);
int apo_plugin_handkerchief_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_handkerchief_GetVariable(void* state, const char* name, double* value);
int apo_plugin_handkerchief_SetVariable(void* state, const char* name, double* value);
int apo_plugin_handkerchief_ResetVariable(void* state, const char* name);
int apo_plugin_handkerchief_GetNrVariables();
const char* apo_plugin_handkerchief_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_handkerchief_Create,       &apo_plugin_handkerchief_Destroy,       &apo_plugin_handkerchief_Prepare,
    &apo_plugin_handkerchief_Calc,         &apo_plugin_handkerchief_InitDC,        &apo_plugin_handkerchief_GetVariable,
    &apo_plugin_handkerchief_SetVariable,  &apo_plugin_handkerchief_ResetVariable, &apo_plugin_handkerchief_GetNrVariables,
    &apo_plugin_handkerchief_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("handkerchief", kVTable);
} // namespace
