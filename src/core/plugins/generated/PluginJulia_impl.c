/* Generated wrapper: gives julia.c's globally-named C symbols a unique
   prefix so 50+ plugins can be statically linked into one binary without
   multiple-definition errors (see ../PluginVariation.h). Compiled as C
   (not C++) - see ../thirdparty/apoplugin.h's file header for why. */
#define PluginVarCreate            apo_plugin_julia_Create
#define PluginVarDestroy           apo_plugin_julia_Destroy
#define PluginVarInit              apo_plugin_julia_Init
#define PluginVarInit3D            apo_plugin_julia_Init3D
#define PluginVarInitDC            apo_plugin_julia_InitDC
#define PluginVarGetName           apo_plugin_julia_GetName
#define PluginVarGetNrVariables    apo_plugin_julia_GetNrVariables
#define PluginVarGetVariable       apo_plugin_julia_GetVariable
#define PluginVarSetVariable       apo_plugin_julia_SetVariable
#define PluginVarResetVariable     apo_plugin_julia_ResetVariable
#define PluginVarGetVariableNameAt apo_plugin_julia_GetVariableNameAt
#define PluginVarPrepare           apo_plugin_julia_Prepare
#define PluginVarCalc              apo_plugin_julia_Calc
#define VarInfo                    apo_plugin_julia_VarInfo
#define VariationName              apo_plugin_julia_VariationName
#define NumVariables               apo_plugin_julia_NumVariables
#define dummyFTz                   apo_plugin_julia_dummyFTz
#define dummyFPz                   apo_plugin_julia_dummyFPz
#define dummyColor                 apo_plugin_julia_dummyColor
#include "../thirdparty/julia.c"
