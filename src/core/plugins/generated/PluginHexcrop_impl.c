/* Generated wrapper: gives hexcrop.c's globally-named C symbols a unique
   prefix so 50+ plugins can be statically linked into one binary without
   multiple-definition errors (see ../PluginVariation.h). Compiled as C
   (not C++) - see ../thirdparty/apoplugin.h's file header for why. */
#define PluginVarCreate            apo_plugin_hexcrop_Create
#define PluginVarDestroy           apo_plugin_hexcrop_Destroy
#define PluginVarInit              apo_plugin_hexcrop_Init
#define PluginVarInit3D            apo_plugin_hexcrop_Init3D
#define PluginVarInitDC            apo_plugin_hexcrop_InitDC
#define PluginVarGetName           apo_plugin_hexcrop_GetName
#define PluginVarGetNrVariables    apo_plugin_hexcrop_GetNrVariables
#define PluginVarGetVariable       apo_plugin_hexcrop_GetVariable
#define PluginVarSetVariable       apo_plugin_hexcrop_SetVariable
#define PluginVarResetVariable     apo_plugin_hexcrop_ResetVariable
#define PluginVarGetVariableNameAt apo_plugin_hexcrop_GetVariableNameAt
#define PluginVarPrepare           apo_plugin_hexcrop_Prepare
#define PluginVarCalc              apo_plugin_hexcrop_Calc
#define VarInfo                    apo_plugin_hexcrop_VarInfo
#define VariationName              apo_plugin_hexcrop_VariationName
#define NumVariables               apo_plugin_hexcrop_NumVariables
#define dummyFTz                   apo_plugin_hexcrop_dummyFTz
#define dummyFPz                   apo_plugin_hexcrop_dummyFPz
#define dummyColor                 apo_plugin_hexcrop_dummyColor
#include "../thirdparty/hexcrop.c"
