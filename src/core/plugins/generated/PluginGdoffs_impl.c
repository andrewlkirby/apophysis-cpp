/* Generated wrapper: gives gdoffs.c's globally-named C symbols a unique
   prefix so 50+ plugins can be statically linked into one binary without
   multiple-definition errors (see ../PluginVariation.h). Compiled as C
   (not C++) - see ../thirdparty/apoplugin.h's file header for why. */
#define PluginVarCreate            apo_plugin_gdoffs_Create
#define PluginVarDestroy           apo_plugin_gdoffs_Destroy
#define PluginVarInit              apo_plugin_gdoffs_Init
#define PluginVarInit3D            apo_plugin_gdoffs_Init3D
#define PluginVarInitDC            apo_plugin_gdoffs_InitDC
#define PluginVarGetName           apo_plugin_gdoffs_GetName
#define PluginVarGetNrVariables    apo_plugin_gdoffs_GetNrVariables
#define PluginVarGetVariable       apo_plugin_gdoffs_GetVariable
#define PluginVarSetVariable       apo_plugin_gdoffs_SetVariable
#define PluginVarResetVariable     apo_plugin_gdoffs_ResetVariable
#define PluginVarGetVariableNameAt apo_plugin_gdoffs_GetVariableNameAt
#define PluginVarPrepare           apo_plugin_gdoffs_Prepare
#define PluginVarCalc              apo_plugin_gdoffs_Calc
#define VarInfo                    apo_plugin_gdoffs_VarInfo
#define VariationName              apo_plugin_gdoffs_VariationName
#define NumVariables               apo_plugin_gdoffs_NumVariables
#define dummyFTz                   apo_plugin_gdoffs_dummyFTz
#define dummyFPz                   apo_plugin_gdoffs_dummyFPz
#define dummyColor                 apo_plugin_gdoffs_dummyColor
#include "../thirdparty/gdoffs.c"
