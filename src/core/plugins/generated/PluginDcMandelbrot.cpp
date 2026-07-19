// Generated wrapper: registers dc_mandelbrot.c (see ../thirdparty/dc_mandelbrot.c) as an
// apo::Variation via PluginVariation. Paired with PluginDcMandelbrot_impl.c,
// which compiles the plugin itself as C with its symbols prefixed
// apo_plugin_dc_mandelbrot_* (declared extern "C" below to match).
#include "../PluginVariation.h"

extern "C" {
void* apo_plugin_dc_mandelbrot_Create();
int apo_plugin_dc_mandelbrot_Destroy(void** state);
int apo_plugin_dc_mandelbrot_Prepare(void* state);
int apo_plugin_dc_mandelbrot_Calc(void* state);
int apo_plugin_dc_mandelbrot_InitDC(void* state, void* pFPx, void* pFPy, void* pFPz, void* pFTx, void* pFTy, void* pFTz,
                                void* pColor, double vvar, double a, double b, double c, double d, double e,
                                double f);
int apo_plugin_dc_mandelbrot_GetVariable(void* state, const char* name, double* value);
int apo_plugin_dc_mandelbrot_SetVariable(void* state, const char* name, double* value);
int apo_plugin_dc_mandelbrot_ResetVariable(void* state, const char* name);
int apo_plugin_dc_mandelbrot_GetNrVariables();
const char* apo_plugin_dc_mandelbrot_GetVariableNameAt(int index);
}

namespace {
const apo::PluginVTable kVTable = {
    &apo_plugin_dc_mandelbrot_Create,       &apo_plugin_dc_mandelbrot_Destroy,       &apo_plugin_dc_mandelbrot_Prepare,
    &apo_plugin_dc_mandelbrot_Calc,         &apo_plugin_dc_mandelbrot_InitDC,        &apo_plugin_dc_mandelbrot_GetVariable,
    &apo_plugin_dc_mandelbrot_SetVariable,  &apo_plugin_dc_mandelbrot_ResetVariable, &apo_plugin_dc_mandelbrot_GetNrVariables,
    &apo_plugin_dc_mandelbrot_GetVariableNameAt,
};
const bool kRegistered = apo::registerPluginVariation("dc_mandelbrot", kVTable);
} // namespace
