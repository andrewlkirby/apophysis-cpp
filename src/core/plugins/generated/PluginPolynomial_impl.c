/* Generated wrapper: gives polynomial.c's globally-named C symbols a unique
   prefix so 50+ plugins can be statically linked into one binary without
   multiple-definition errors (see ../PluginVariation.h). Compiled as C
   (not C++) - see ../thirdparty/apoplugin.h's file header for why. */
#define PluginVarCreate            apo_plugin_polynomial_Create
#define PluginVarDestroy           apo_plugin_polynomial_Destroy
#define PluginVarInit              apo_plugin_polynomial_Init
#define PluginVarInit3D            apo_plugin_polynomial_Init3D
#define PluginVarInitDC            apo_plugin_polynomial_InitDC
#define PluginVarGetName           apo_plugin_polynomial_GetName
#define PluginVarGetNrVariables    apo_plugin_polynomial_GetNrVariables
#define PluginVarGetVariable       apo_plugin_polynomial_GetVariable
#define PluginVarSetVariable       apo_plugin_polynomial_SetVariable
#define PluginVarResetVariable     apo_plugin_polynomial_ResetVariable
#define PluginVarGetVariableNameAt apo_plugin_polynomial_GetVariableNameAt
#define PluginVarPrepare           apo_plugin_polynomial_Prepare
#define PluginVarCalc              apo_plugin_polynomial_Calc
#define VarInfo                    apo_plugin_polynomial_VarInfo
#define VariationName              apo_plugin_polynomial_VariationName
#define NumVariables               apo_plugin_polynomial_NumVariables
#define dummyFTz                   apo_plugin_polynomial_dummyFTz
#define dummyFPz                   apo_plugin_polynomial_dummyFPz
#define dummyColor                 apo_plugin_polynomial_dummyColor
#include "../thirdparty/polynomial.c"
