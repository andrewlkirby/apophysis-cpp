/* Generated wrapper: gives julian2.c's globally-named C symbols a unique
   prefix so 50+ plugins can be statically linked into one binary without
   multiple-definition errors (see ../PluginVariation.h). Compiled as C
   (not C++) - see ../thirdparty/apoplugin.h's file header for why. */
#define PluginVarCreate            apo_plugin_julian2_Create
#define PluginVarDestroy           apo_plugin_julian2_Destroy
#define PluginVarInit              apo_plugin_julian2_Init
#define PluginVarInit3D            apo_plugin_julian2_Init3D
#define PluginVarInitDC            apo_plugin_julian2_InitDC
#define PluginVarGetName           apo_plugin_julian2_GetName
#define PluginVarGetNrVariables    apo_plugin_julian2_GetNrVariables
#define PluginVarGetVariable       apo_plugin_julian2_GetVariable
#define PluginVarSetVariable       apo_plugin_julian2_SetVariable
#define PluginVarResetVariable     apo_plugin_julian2_ResetVariable
#define PluginVarGetVariableNameAt apo_plugin_julian2_GetVariableNameAt
#define PluginVarPrepare           apo_plugin_julian2_Prepare
#define PluginVarCalc              apo_plugin_julian2_Calc
#define VarInfo                    apo_plugin_julian2_VarInfo
#define VariationName              apo_plugin_julian2_VariationName
#define NumVariables               apo_plugin_julian2_NumVariables
#define dummyFTz                   apo_plugin_julian2_dummyFTz
#define dummyFPz                   apo_plugin_julian2_dummyFPz
#define dummyColor                 apo_plugin_julian2_dummyColor
#include "../thirdparty/julian2.c"
