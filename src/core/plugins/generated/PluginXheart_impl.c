/* Generated wrapper: gives xheart.c's globally-named C symbols a unique
   prefix so 50+ plugins can be statically linked into one binary without
   multiple-definition errors (see ../PluginVariation.h). Compiled as C
   (not C++) - see ../thirdparty/apoplugin.h's file header for why. */
#define PluginVarCreate            apo_plugin_xheart_Create
#define PluginVarDestroy           apo_plugin_xheart_Destroy
#define PluginVarInit              apo_plugin_xheart_Init
#define PluginVarInit3D            apo_plugin_xheart_Init3D
#define PluginVarInitDC            apo_plugin_xheart_InitDC
#define PluginVarGetName           apo_plugin_xheart_GetName
#define PluginVarGetNrVariables    apo_plugin_xheart_GetNrVariables
#define PluginVarGetVariable       apo_plugin_xheart_GetVariable
#define PluginVarSetVariable       apo_plugin_xheart_SetVariable
#define PluginVarResetVariable     apo_plugin_xheart_ResetVariable
#define PluginVarGetVariableNameAt apo_plugin_xheart_GetVariableNameAt
#define PluginVarPrepare           apo_plugin_xheart_Prepare
#define PluginVarCalc              apo_plugin_xheart_Calc
#define VarInfo                    apo_plugin_xheart_VarInfo
#define VariationName              apo_plugin_xheart_VariationName
#define NumVariables               apo_plugin_xheart_NumVariables
#define dummyFTz                   apo_plugin_xheart_dummyFTz
#define dummyFPz                   apo_plugin_xheart_dummyFPz
#define dummyColor                 apo_plugin_xheart_dummyColor
#include "../thirdparty/xheart.c"
