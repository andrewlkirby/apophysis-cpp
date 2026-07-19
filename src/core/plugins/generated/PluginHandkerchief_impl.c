/* Generated wrapper: gives handkerchief.c's globally-named C symbols a unique
   prefix so 50+ plugins can be statically linked into one binary without
   multiple-definition errors (see ../PluginVariation.h). Compiled as C
   (not C++) - see ../thirdparty/apoplugin.h's file header for why. */
#define PluginVarCreate            apo_plugin_handkerchief_Create
#define PluginVarDestroy           apo_plugin_handkerchief_Destroy
#define PluginVarInit              apo_plugin_handkerchief_Init
#define PluginVarInit3D            apo_plugin_handkerchief_Init3D
#define PluginVarInitDC            apo_plugin_handkerchief_InitDC
#define PluginVarGetName           apo_plugin_handkerchief_GetName
#define PluginVarGetNrVariables    apo_plugin_handkerchief_GetNrVariables
#define PluginVarGetVariable       apo_plugin_handkerchief_GetVariable
#define PluginVarSetVariable       apo_plugin_handkerchief_SetVariable
#define PluginVarResetVariable     apo_plugin_handkerchief_ResetVariable
#define PluginVarGetVariableNameAt apo_plugin_handkerchief_GetVariableNameAt
#define PluginVarPrepare           apo_plugin_handkerchief_Prepare
#define PluginVarCalc              apo_plugin_handkerchief_Calc
#define VarInfo                    apo_plugin_handkerchief_VarInfo
#define VariationName              apo_plugin_handkerchief_VariationName
#define NumVariables               apo_plugin_handkerchief_NumVariables
#define dummyFTz                   apo_plugin_handkerchief_dummyFTz
#define dummyFPz                   apo_plugin_handkerchief_dummyFPz
#define dummyColor                 apo_plugin_handkerchief_dummyColor
#include "../thirdparty/handkerchief.c"
