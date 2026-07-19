/* Generated wrapper: gives wavesn.c's globally-named C symbols a unique
   prefix so 50+ plugins can be statically linked into one binary without
   multiple-definition errors (see ../PluginVariation.h). Compiled as C
   (not C++) - see ../thirdparty/apoplugin.h's file header for why. */
#define PluginVarCreate            apo_plugin_wavesn_Create
#define PluginVarDestroy           apo_plugin_wavesn_Destroy
#define PluginVarInit              apo_plugin_wavesn_Init
#define PluginVarInit3D            apo_plugin_wavesn_Init3D
#define PluginVarInitDC            apo_plugin_wavesn_InitDC
#define PluginVarGetName           apo_plugin_wavesn_GetName
#define PluginVarGetNrVariables    apo_plugin_wavesn_GetNrVariables
#define PluginVarGetVariable       apo_plugin_wavesn_GetVariable
#define PluginVarSetVariable       apo_plugin_wavesn_SetVariable
#define PluginVarResetVariable     apo_plugin_wavesn_ResetVariable
#define PluginVarGetVariableNameAt apo_plugin_wavesn_GetVariableNameAt
#define PluginVarPrepare           apo_plugin_wavesn_Prepare
#define PluginVarCalc              apo_plugin_wavesn_Calc
#define VarInfo                    apo_plugin_wavesn_VarInfo
#define VariationName              apo_plugin_wavesn_VariationName
#define NumVariables               apo_plugin_wavesn_NumVariables
#define dummyFTz                   apo_plugin_wavesn_dummyFTz
#define dummyFPz                   apo_plugin_wavesn_dummyFPz
#define dummyColor                 apo_plugin_wavesn_dummyColor
#include "../thirdparty/wavesn.c"
