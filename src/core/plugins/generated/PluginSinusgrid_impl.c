/* Generated wrapper: gives sinusgrid.c's globally-named C symbols a unique
   prefix so 50+ plugins can be statically linked into one binary without
   multiple-definition errors (see ../PluginVariation.h). Compiled as C
   (not C++) - see ../thirdparty/apoplugin.h's file header for why. */
#define PluginVarCreate            apo_plugin_sinusgrid_Create
#define PluginVarDestroy           apo_plugin_sinusgrid_Destroy
#define PluginVarInit              apo_plugin_sinusgrid_Init
#define PluginVarInit3D            apo_plugin_sinusgrid_Init3D
#define PluginVarInitDC            apo_plugin_sinusgrid_InitDC
#define PluginVarGetName           apo_plugin_sinusgrid_GetName
#define PluginVarGetNrVariables    apo_plugin_sinusgrid_GetNrVariables
#define PluginVarGetVariable       apo_plugin_sinusgrid_GetVariable
#define PluginVarSetVariable       apo_plugin_sinusgrid_SetVariable
#define PluginVarResetVariable     apo_plugin_sinusgrid_ResetVariable
#define PluginVarGetVariableNameAt apo_plugin_sinusgrid_GetVariableNameAt
#define PluginVarPrepare           apo_plugin_sinusgrid_Prepare
#define PluginVarCalc              apo_plugin_sinusgrid_Calc
#define VarInfo                    apo_plugin_sinusgrid_VarInfo
#define VariationName              apo_plugin_sinusgrid_VariationName
#define NumVariables               apo_plugin_sinusgrid_NumVariables
#define dummyFTz                   apo_plugin_sinusgrid_dummyFTz
#define dummyFPz                   apo_plugin_sinusgrid_dummyFPz
#define dummyColor                 apo_plugin_sinusgrid_dummyColor
#include "../thirdparty/sinusgrid.c"
