/* Generated wrapper: gives coswrap.c's globally-named C symbols a unique
   prefix so 50+ plugins can be statically linked into one binary without
   multiple-definition errors (see ../PluginVariation.h). Compiled as C
   (not C++) - see ../thirdparty/apoplugin.h's file header for why. */
#define PluginVarCreate            apo_plugin_coswrap_Create
#define PluginVarDestroy           apo_plugin_coswrap_Destroy
#define PluginVarInit              apo_plugin_coswrap_Init
#define PluginVarInit3D            apo_plugin_coswrap_Init3D
#define PluginVarInitDC            apo_plugin_coswrap_InitDC
#define PluginVarGetName           apo_plugin_coswrap_GetName
#define PluginVarGetNrVariables    apo_plugin_coswrap_GetNrVariables
#define PluginVarGetVariable       apo_plugin_coswrap_GetVariable
#define PluginVarSetVariable       apo_plugin_coswrap_SetVariable
#define PluginVarResetVariable     apo_plugin_coswrap_ResetVariable
#define PluginVarGetVariableNameAt apo_plugin_coswrap_GetVariableNameAt
#define PluginVarPrepare           apo_plugin_coswrap_Prepare
#define PluginVarCalc              apo_plugin_coswrap_Calc
#define VarInfo                    apo_plugin_coswrap_VarInfo
#define VariationName              apo_plugin_coswrap_VariationName
#define NumVariables               apo_plugin_coswrap_NumVariables
#define dummyFTz                   apo_plugin_coswrap_dummyFTz
#define dummyFPz                   apo_plugin_coswrap_dummyFPz
#define dummyColor                 apo_plugin_coswrap_dummyColor
#include "../thirdparty/coswrap.c"
