/*
    Apophysis Plugin

    Copyright (C) 2007-2009 Joel Faber
    Copyright (C) 2007-2009 Michael Faber

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

// Must define this structure before we include apoplugin.h
typedef struct
{
	double pre_polar2_vvar;
	double pre_polar2_vvar_2;
} Variables;

#include "apoplugin.h"

// Set the name of this plugin
APO_PLUGIN("pre_polar2");
// This plugin has no user-settable parameters (pre_polar2_vvar/_2 above
// are internal cached values computed in Prepare(), not exposed ones).
// The original's empty APO_VARIABLES() expanded to
// `VariableInfo VarInfo[] = {};` - a zero-size array with an empty
// initializer, a GNU extension MSVC's compiler rejects. Spelled out
// manually to the same effect: NumVariables=0 means every
// VarInfo[]-indexed loop in apoplugin.h never executes, so the array's
// declared size doesn't matter functionally.
VariableInfo VarInfo[1];
const int NumVariables = 0;

int PluginVarPrepare(Variation* vp)
{
	VAR(pre_polar2_vvar) = VVAR / M_PI;
	VAR(pre_polar2_vvar_2) = VAR(pre_polar2_vvar) * 0.5;
    return TRUE;
}
/* Fixed bug: the original operated on FPx/FPy (the output accumulator) -
   byte-identical to post_polar2.c, just renamed - despite being registered
   under a "pre_"-prefixed name. XForm dispatches "pre_"-named variations in
   the first phase of each point, before anything has written to the
   accumulator (see XForm::prepare()'s phase-ordering, which builds the
   pre_/normal/post_ call lists by name prefix) - so FPx/FPy are always
   exactly 0 here, making the original's log(FPx*FPx+FPy*FPy) = log(0) on
   every single point, unconditionally. Not an edge case: this variation was
   completely non-functional (always NaN) as shipped. Fixed to operate on
   FTx/FTy (the input point) instead, matching every other genuine "pre_"
   variation in this codebase (pre_crop, pre_falloff2, pre_boarders2,
   pre_dcztransl all perturb the input, not the accumulator) - this is what
   makes it an actual, usable pre-transform. */
int PluginVarCalc(Variation* vp)
{
	FTy = VAR(pre_polar2_vvar_2) * log(FTx*FTx + FTy*FTy);
	FTx = VAR(pre_polar2_vvar) * atan2(FTx, FTy);

    return TRUE;
}
