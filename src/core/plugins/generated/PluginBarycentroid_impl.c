/* Generated wrapper: gives barycentroid.c's globally-named C symbols a
   unique prefix so 50+ plugins can be statically linked into one binary
   without multiple-definition errors (see ../PluginVariation.h). Compiled
   as C (not C++) - see ../thirdparty/apoplugin.h's file header for why. */
#define PluginVarCreate            apo_plugin_barycentroid_Create
#define PluginVarDestroy           apo_plugin_barycentroid_Destroy
#define PluginVarInit              apo_plugin_barycentroid_Init
#define PluginVarInit3D            apo_plugin_barycentroid_Init3D
#define PluginVarInitDC            apo_plugin_barycentroid_InitDC
#define PluginVarGetName           apo_plugin_barycentroid_GetName
#define PluginVarGetNrVariables    apo_plugin_barycentroid_GetNrVariables
#define PluginVarGetVariable       apo_plugin_barycentroid_GetVariable
#define PluginVarSetVariable       apo_plugin_barycentroid_SetVariable
#define PluginVarResetVariable     apo_plugin_barycentroid_ResetVariable
#define PluginVarGetVariableNameAt apo_plugin_barycentroid_GetVariableNameAt
#define PluginVarPrepare           apo_plugin_barycentroid_Prepare
#define PluginVarCalc              apo_plugin_barycentroid_Calc
#define VarInfo                    apo_plugin_barycentroid_VarInfo
#define VariationName              apo_plugin_barycentroid_VariationName
#define NumVariables               apo_plugin_barycentroid_NumVariables
#define dummyFTz                   apo_plugin_barycentroid_dummyFTz
#define dummyFPz                   apo_plugin_barycentroid_dummyFPz
#define dummyColor                 apo_plugin_barycentroid_dummyColor
/* sgn() is `static` (hoisted file-scope helper - see thirdparty/barycentroid.c),
   so it already has internal linkage and needs no rename. */

#include "../thirdparty/barycentroid.c"
