/* Generated wrapper: gives ex.c's globally-named C symbols a unique
   prefix so 50+ plugins can be statically linked into one binary without
   multiple-definition errors (see ../PluginVariation.h). Compiled as C
   (not C++) - see ../thirdparty/apoplugin.h's file header for why. */
#define PluginVarCreate            apo_plugin_ex_Create
#define PluginVarDestroy           apo_plugin_ex_Destroy
#define PluginVarInit              apo_plugin_ex_Init
#define PluginVarInit3D            apo_plugin_ex_Init3D
#define PluginVarInitDC            apo_plugin_ex_InitDC
#define PluginVarGetName           apo_plugin_ex_GetName
#define PluginVarGetNrVariables    apo_plugin_ex_GetNrVariables
#define PluginVarGetVariable       apo_plugin_ex_GetVariable
#define PluginVarSetVariable       apo_plugin_ex_SetVariable
#define PluginVarResetVariable     apo_plugin_ex_ResetVariable
#define PluginVarGetVariableNameAt apo_plugin_ex_GetVariableNameAt
#define PluginVarPrepare           apo_plugin_ex_Prepare
#define PluginVarCalc              apo_plugin_ex_Calc
#define VarInfo                    apo_plugin_ex_VarInfo
#define VariationName              apo_plugin_ex_VariationName
#define NumVariables               apo_plugin_ex_NumVariables
#define dummyFTz                   apo_plugin_ex_dummyFTz
#define dummyFPz                   apo_plugin_ex_dummyFPz
#define dummyColor                 apo_plugin_ex_dummyColor
#include "../thirdparty/ex.c"
