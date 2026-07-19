/* Generated wrapper: gives sigmoid.c's globally-named C symbols a unique
   prefix so 50+ plugins can be statically linked into one binary without
   multiple-definition errors (see ../PluginVariation.h). Compiled as C
   (not C++) - see ../thirdparty/apoplugin.h's file header for why. */
#define PluginVarCreate            apo_plugin_sigmoid_Create
#define PluginVarDestroy           apo_plugin_sigmoid_Destroy
#define PluginVarInit              apo_plugin_sigmoid_Init
#define PluginVarInit3D            apo_plugin_sigmoid_Init3D
#define PluginVarInitDC            apo_plugin_sigmoid_InitDC
#define PluginVarGetName           apo_plugin_sigmoid_GetName
#define PluginVarGetNrVariables    apo_plugin_sigmoid_GetNrVariables
#define PluginVarGetVariable       apo_plugin_sigmoid_GetVariable
#define PluginVarSetVariable       apo_plugin_sigmoid_SetVariable
#define PluginVarResetVariable     apo_plugin_sigmoid_ResetVariable
#define PluginVarGetVariableNameAt apo_plugin_sigmoid_GetVariableNameAt
#define PluginVarPrepare           apo_plugin_sigmoid_Prepare
#define PluginVarCalc              apo_plugin_sigmoid_Calc
#define VarInfo                    apo_plugin_sigmoid_VarInfo
#define VariationName              apo_plugin_sigmoid_VariationName
#define NumVariables               apo_plugin_sigmoid_NumVariables
#define dummyFTz                   apo_plugin_sigmoid_dummyFTz
#define dummyFPz                   apo_plugin_sigmoid_dummyFPz
#define dummyColor                 apo_plugin_sigmoid_dummyColor
#include "../thirdparty/sigmoid.c"
