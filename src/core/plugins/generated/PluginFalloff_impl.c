/* Generated wrapper: gives falloff.c's globally-named C symbols a unique
   prefix so 50+ plugins can be statically linked into one binary without
   multiple-definition errors (see ../PluginVariation.h). Compiled as C
   (not C++) - see ../thirdparty/apoplugin.h's file header for why. */
#define PluginVarCreate            apo_plugin_falloff_Create
#define PluginVarDestroy           apo_plugin_falloff_Destroy
#define PluginVarInit              apo_plugin_falloff_Init
#define PluginVarInit3D            apo_plugin_falloff_Init3D
#define PluginVarInitDC            apo_plugin_falloff_InitDC
#define PluginVarGetName           apo_plugin_falloff_GetName
#define PluginVarGetNrVariables    apo_plugin_falloff_GetNrVariables
#define PluginVarGetVariable       apo_plugin_falloff_GetVariable
#define PluginVarSetVariable       apo_plugin_falloff_SetVariable
#define PluginVarResetVariable     apo_plugin_falloff_ResetVariable
#define PluginVarGetVariableNameAt apo_plugin_falloff_GetVariableNameAt
#define PluginVarPrepare           apo_plugin_falloff_Prepare
#define PluginVarCalc              apo_plugin_falloff_Calc
#define VarInfo                    apo_plugin_falloff_VarInfo
#define VariationName              apo_plugin_falloff_VariationName
#define NumVariables               apo_plugin_falloff_NumVariables
#define dummyFTz                   apo_plugin_falloff_dummyFTz
#define dummyFPz                   apo_plugin_falloff_dummyFPz
#define dummyColor                 apo_plugin_falloff_dummyColor
#include "../thirdparty/falloff.c"
