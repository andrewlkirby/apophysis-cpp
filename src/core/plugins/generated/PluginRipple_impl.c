/* Generated wrapper: gives ripple.c's globally-named C symbols a unique
   prefix so 50+ plugins can be statically linked into one binary without
   multiple-definition errors (see ../PluginVariation.h). Compiled as C
   (not C++) - see ../thirdparty/apoplugin.h's file header for why. */
#define PluginVarCreate            apo_plugin_ripple_Create
#define PluginVarDestroy           apo_plugin_ripple_Destroy
#define PluginVarInit              apo_plugin_ripple_Init
#define PluginVarInit3D            apo_plugin_ripple_Init3D
#define PluginVarInitDC            apo_plugin_ripple_InitDC
#define PluginVarGetName           apo_plugin_ripple_GetName
#define PluginVarGetNrVariables    apo_plugin_ripple_GetNrVariables
#define PluginVarGetVariable       apo_plugin_ripple_GetVariable
#define PluginVarSetVariable       apo_plugin_ripple_SetVariable
#define PluginVarResetVariable     apo_plugin_ripple_ResetVariable
#define PluginVarGetVariableNameAt apo_plugin_ripple_GetVariableNameAt
#define PluginVarPrepare           apo_plugin_ripple_Prepare
#define PluginVarCalc              apo_plugin_ripple_Calc
#define VarInfo                    apo_plugin_ripple_VarInfo
#define VariationName              apo_plugin_ripple_VariationName
#define NumVariables               apo_plugin_ripple_NumVariables
#define dummyFTz                   apo_plugin_ripple_dummyFTz
#define dummyFPz                   apo_plugin_ripple_dummyFPz
#define dummyColor                 apo_plugin_ripple_dummyColor
#include "../thirdparty/ripple.c"
