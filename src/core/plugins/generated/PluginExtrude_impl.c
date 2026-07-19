/* Generated wrapper: gives extrude.c's globally-named C symbols a unique
   prefix so 50+ plugins can be statically linked into one binary without
   multiple-definition errors (see ../PluginVariation.h). Compiled as C
   (not C++) - see ../thirdparty/apoplugin.h's file header for why. */
#define PluginVarCreate            apo_plugin_extrude_Create
#define PluginVarDestroy           apo_plugin_extrude_Destroy
#define PluginVarInit              apo_plugin_extrude_Init
#define PluginVarInit3D            apo_plugin_extrude_Init3D
#define PluginVarInitDC            apo_plugin_extrude_InitDC
#define PluginVarGetName           apo_plugin_extrude_GetName
#define PluginVarGetNrVariables    apo_plugin_extrude_GetNrVariables
#define PluginVarGetVariable       apo_plugin_extrude_GetVariable
#define PluginVarSetVariable       apo_plugin_extrude_SetVariable
#define PluginVarResetVariable     apo_plugin_extrude_ResetVariable
#define PluginVarGetVariableNameAt apo_plugin_extrude_GetVariableNameAt
#define PluginVarPrepare           apo_plugin_extrude_Prepare
#define PluginVarCalc              apo_plugin_extrude_Calc
#define VarInfo                    apo_plugin_extrude_VarInfo
#define VariationName              apo_plugin_extrude_VariationName
#define NumVariables               apo_plugin_extrude_NumVariables
#define dummyFTz                   apo_plugin_extrude_dummyFTz
#define dummyFPz                   apo_plugin_extrude_dummyFPz
#define dummyColor                 apo_plugin_extrude_dummyColor
#include "../thirdparty/extrude.c"
