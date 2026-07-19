// Generated wrapper: registers extrude.c (see ../thirdparty/extrude.c) as an
// apo::Variation via PluginVariation. Paired with PluginExtrude_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_extrude_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_extrude_Create();
int apo_plugin_extrude_Destroy(void** state);
int apo_plugin_extrude_Prepare(void* state);
int apo_plugin_extrude_Calc(void* state);
int apo_plugin_extrude_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_extrude_GetVariable(void* state, const char* name, double* value);
int apo_plugin_extrude_SetVariable(void* state, const char* name, double* value);
int apo_plugin_extrude_ResetVariable(void* state, const char* name);
int apo_plugin_extrude_GetNrVariables();
const char* apo_plugin_extrude_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_extrude_Create,       &apo_plugin_extrude_Destroy,       &apo_plugin_extrude_Prepare,
    &apo_plugin_extrude_Calc,         &apo_plugin_extrude_InitDC,        &apo_plugin_extrude_GetVariable,
    &apo_plugin_extrude_SetVariable,  &apo_plugin_extrude_ResetVariable, &apo_plugin_extrude_GetNrVariables,
    &apo_plugin_extrude_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("extrude", kVTable);
} // namespace
