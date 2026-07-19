/* Generated wrapper: gives dc_linear.c's globally-named C symbols a unique
   prefix so 50+ plugins can be statically linked into one binary without
   multiple-definition errors (see ../PluginVariation.h). Compiled as C
   (not C++) - see ../thirdparty/apoplugin.h's file header for why. */
#define PluginVarCreate            apo_plugin_dc_linear_Create
#define PluginVarDestroy           apo_plugin_dc_linear_Destroy
#define PluginVarInit              apo_plugin_dc_linear_Init
#define PluginVarInit3D            apo_plugin_dc_linear_Init3D
#define PluginVarInitDC            apo_plugin_dc_linear_InitDC
#define PluginVarGetName           apo_plugin_dc_linear_GetName
#define PluginVarGetNrVariables    apo_plugin_dc_linear_GetNrVariables
#define PluginVarGetVariable       apo_plugin_dc_linear_GetVariable
#define PluginVarSetVariable       apo_plugin_dc_linear_SetVariable
#define PluginVarResetVariable     apo_plugin_dc_linear_ResetVariable
#define PluginVarGetVariableNameAt apo_plugin_dc_linear_GetVariableNameAt
#define PluginVarPrepare           apo_plugin_dc_linear_Prepare
#define PluginVarCalc              apo_plugin_dc_linear_Calc
#define VarInfo                    apo_plugin_dc_linear_VarInfo
#define VariationName              apo_plugin_dc_linear_VariationName
#define NumVariables               apo_plugin_dc_linear_NumVariables
#define dummyFTz                   apo_plugin_dc_linear_dummyFTz
#define dummyFPz                   apo_plugin_dc_linear_dummyFPz
#define dummyColor                 apo_plugin_dc_linear_dummyColor
#include "../thirdparty/dc_linear.c"
