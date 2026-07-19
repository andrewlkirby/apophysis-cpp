/* Generated wrapper: gives stwins.c's globally-named C symbols a unique
   prefix so 50+ plugins can be statically linked into one binary without
   multiple-definition errors (see ../PluginVariation.h). Compiled as C
   (not C++) - see ../thirdparty/apoplugin.h's file header for why. */
#define PluginVarCreate            apo_plugin_stwins_Create
#define PluginVarDestroy           apo_plugin_stwins_Destroy
#define PluginVarInit              apo_plugin_stwins_Init
#define PluginVarInit3D            apo_plugin_stwins_Init3D
#define PluginVarInitDC            apo_plugin_stwins_InitDC
#define PluginVarGetName           apo_plugin_stwins_GetName
#define PluginVarGetNrVariables    apo_plugin_stwins_GetNrVariables
#define PluginVarGetVariable       apo_plugin_stwins_GetVariable
#define PluginVarSetVariable       apo_plugin_stwins_SetVariable
#define PluginVarResetVariable     apo_plugin_stwins_ResetVariable
#define PluginVarGetVariableNameAt apo_plugin_stwins_GetVariableNameAt
#define PluginVarPrepare           apo_plugin_stwins_Prepare
#define PluginVarCalc              apo_plugin_stwins_Calc
#define VarInfo                    apo_plugin_stwins_VarInfo
#define VariationName              apo_plugin_stwins_VariationName
#define NumVariables               apo_plugin_stwins_NumVariables
#define dummyFTz                   apo_plugin_stwins_dummyFTz
#define dummyFPz                   apo_plugin_stwins_dummyFPz
#define dummyColor                 apo_plugin_stwins_dummyColor
#include "../thirdparty/stwins.c"
