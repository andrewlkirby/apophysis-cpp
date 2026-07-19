/* Generated wrapper: gives fisheye.c's globally-named C symbols a unique
   prefix so 50+ plugins can be statically linked into one binary without
   multiple-definition errors (see ../PluginVariation.h). Compiled as C
   (not C++) - see ../thirdparty/apoplugin.h's file header for why. */
#define PluginVarCreate            apo_plugin_fisheye_Create
#define PluginVarDestroy           apo_plugin_fisheye_Destroy
#define PluginVarInit              apo_plugin_fisheye_Init
#define PluginVarInit3D            apo_plugin_fisheye_Init3D
#define PluginVarInitDC            apo_plugin_fisheye_InitDC
#define PluginVarGetName           apo_plugin_fisheye_GetName
#define PluginVarGetNrVariables    apo_plugin_fisheye_GetNrVariables
#define PluginVarGetVariable       apo_plugin_fisheye_GetVariable
#define PluginVarSetVariable       apo_plugin_fisheye_SetVariable
#define PluginVarResetVariable     apo_plugin_fisheye_ResetVariable
#define PluginVarGetVariableNameAt apo_plugin_fisheye_GetVariableNameAt
#define PluginVarPrepare           apo_plugin_fisheye_Prepare
#define PluginVarCalc              apo_plugin_fisheye_Calc
#define VarInfo                    apo_plugin_fisheye_VarInfo
#define VariationName              apo_plugin_fisheye_VariationName
#define NumVariables               apo_plugin_fisheye_NumVariables
#define dummyFTz                   apo_plugin_fisheye_dummyFTz
#define dummyFPz                   apo_plugin_fisheye_dummyFPz
#define dummyColor                 apo_plugin_fisheye_dummyColor
#include "../thirdparty/fisheye.c"
