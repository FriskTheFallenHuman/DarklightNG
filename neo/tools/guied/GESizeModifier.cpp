/*
===========================================================================

DarklightNG Source Code
Copyright (C) 2026 - Justin Marshall(aka IceColdDuke).

This file is part of the DarklightNG GPL source code.
This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).

DarklightNG is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

DarklightNG is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

===========================================================================
*/

#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "GEApp.h"
#include "GESizeModifier.h"

rvGESizeModifier::rvGESizeModifier ( const char* name, idWindow* window, float l, float t, float r, float b ) :
	rvGEModifier ( name, window  )
{
	mOldRect = mWrapper->GetClientRect ( );

	mNewRect[0] = mOldRect[0] + l;
	mNewRect[1] = mOldRect[1] + t;
	mNewRect[2] = mOldRect[2] + r - l;
	mNewRect[3] = mOldRect[3] + b - t;
}

bool rvGESizeModifier::Merge ( rvGEModifier* mergebase )
{
	rvGESizeModifier* merge = (rvGESizeModifier*) mergebase;
	
	mNewRect = merge->mNewRect;
			
	return true;
} 

bool rvGESizeModifier::Apply ( void )
{
	mWrapper->SetRect ( mNewRect );

	return true;
}

bool rvGESizeModifier::Undo ( void )
{
	mWrapper->SetRect ( mOldRect );
	
	return true;
}

bool rvGESizeModifier::IsValid ( void )
{
	if ( !mWindow->GetParent ( ) )
	{
		return false;
	}
	
	return true;
}
