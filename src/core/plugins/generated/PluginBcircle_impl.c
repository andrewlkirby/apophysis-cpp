/* Generated wrapper: gives bcircle.c's globally-named C symbols a unique
   prefix so 50+ plugins can be statically linked into one binary without
   multiple-definition errors (see ../PluginVariation.h). Compiled as C
   (not C++) - see ../thirdparty/apoplugin.h's file header for why. */
#define PluginVarCreate            apo_plugin_bcircle_Create
#define PluginVarDestroy           apo_plugin_bcircle_Destroy
#define PluginVarInit              apo_plugin_bcircle_Init
#define PluginVarInit3D            apo_plugin_bcircle_Init3D
#define PluginVarInitDC            apo_plugin_bcircle_InitDC
#define PluginVarGetName           apo_plugin_bcircle_GetName
#define PluginVarGetNrVariables    apo_plugin_bcircle_GetNrVariables
#define PluginVarGetVariable       apo_plugin_bcircle_GetVariable
#define PluginVarSetVariable       apo_plugin_bcircle_SetVariable
#define PluginVarResetVariable     apo_plugin_bcircle_ResetVariable
#define PluginVarGetVariableNameAt apo_plugin_bcircle_GetVariableNameAt
#define PluginVarPrepare           apo_plugin_bcircle_Prepare
#define PluginVarCalc              apo_plugin_bcircle_Calc
#define VarInfo                    apo_plugin_bcircle_VarInfo
#define VariationName              apo_plugin_bcircle_VariationName
#define NumVariables               apo_plugin_bcircle_NumVariables
#define dummyFTz                   apo_plugin_bcircle_dummyFTz
#define dummyFPz                   apo_plugin_bcircle_dummyFPz
#define dummyColor                 apo_plugin_bcircle_dummyColor

#include "../thirdparty/bcircle.c"
