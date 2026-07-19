// Generated wrapper: registers polynomial.c (see ../thirdparty/polynomial.c) as an
// apo::Variation via PluginVariation. Paired with PluginPolynomial_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_polynomial_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_polynomial_Create();
int apo_plugin_polynomial_Destroy(void** state);
int apo_plugin_polynomial_Prepare(void* state);
int apo_plugin_polynomial_Calc(void* state);
int apo_plugin_polynomial_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_polynomial_GetVariable(void* state, const char* name, double* value);
int apo_plugin_polynomial_SetVariable(void* state, const char* name, double* value);
int apo_plugin_polynomial_ResetVariable(void* state, const char* name);
int apo_plugin_polynomial_GetNrVariables();
const char* apo_plugin_polynomial_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_polynomial_Create,       &apo_plugin_polynomial_Destroy,       &apo_plugin_polynomial_Prepare,
    &apo_plugin_polynomial_Calc,         &apo_plugin_polynomial_InitDC,        &apo_plugin_polynomial_GetVariable,
    &apo_plugin_polynomial_SetVariable,  &apo_plugin_polynomial_ResetVariable, &apo_plugin_polynomial_GetNrVariables,
    &apo_plugin_polynomial_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("polynomial", kVTable);
} // namespace
