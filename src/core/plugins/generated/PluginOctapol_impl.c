/* Generated wrapper: gives octapol.c's globally-named C symbols a unique
   prefix so 50+ plugins can be statically linked into one binary without
   multiple-definition errors (see ../PluginVariation.h). Compiled as C
   (not C++) - see ../thirdparty/apoplugin.h's file header for why. */
#define PluginVarCreate            apo_plugin_octapol_Create
#define PluginVarDestroy           apo_plugin_octapol_Destroy
#define PluginVarInit              apo_plugin_octapol_Init
#define PluginVarInit3D            apo_plugin_octapol_Init3D
#define PluginVarInitDC            apo_plugin_octapol_InitDC
#define PluginVarGetName           apo_plugin_octapol_GetName
#define PluginVarGetNrVariables    apo_plugin_octapol_GetNrVariables
#define PluginVarGetVariable       apo_plugin_octapol_GetVariable
#define PluginVarSetVariable       apo_plugin_octapol_SetVariable
#define PluginVarResetVariable     apo_plugin_octapol_ResetVariable
#define PluginVarGetVariableNameAt apo_plugin_octapol_GetVariableNameAt
#define PluginVarPrepare           apo_plugin_octapol_Prepare
#define PluginVarCalc              apo_plugin_octapol_Calc
#define VarInfo                    apo_plugin_octapol_VarInfo
#define VariationName              apo_plugin_octapol_VariationName
#define NumVariables               apo_plugin_octapol_NumVariables
#define dummyFTz                   apo_plugin_octapol_dummyFTz
#define dummyFPz                   apo_plugin_octapol_dummyFPz
#define dummyColor                 apo_plugin_octapol_dummyColor
#include "../thirdparty/octapol.c"
