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
#include "GEStateModifier.h"

rvGEStateModifier::rvGEStateModifier ( const char* name, idWindow* window, idDict& dict ) :
	rvGEModifier ( name, window ),
	mDict ( dict )
{
	mDict.Copy ( dict );

	// Make a copy of the current dictionary
	mUndoDict.Copy ( mWrapper->GetStateDict() );
}

/*
================
rvGEStateModifier::Apply

Applys the new state dictionary to the window
================
*/
bool rvGEStateModifier::Apply ( void )
{	
	return SetState ( mDict );
}

/*
================
rvGEStateModifier::Undo

Applies the undo dictionary to the window
================
*/
bool rvGEStateModifier::Undo ( void )
{
	return SetState ( mUndoDict );
}

/*
================
rvGEStateModifier::Apply

Applys the given dictionary to the window
================
*/
bool rvGEStateModifier::SetState ( idDict& dict )
{
	const idKeyValue*	key;
	int					i;
	
	// Delete any key thats gone in the new dict
	for ( i = 0; i < mWrapper->GetStateDict().GetNumKeyVals(); i ++ )
	{
		key = mWrapper->GetStateDict().GetKeyVal ( i );
		if ( !key )
		{
			continue;
		}
	}
	
	mWrapper->SetState ( dict );
	
	return true;
}

