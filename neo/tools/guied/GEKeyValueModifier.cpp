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
#include "GEKeyValueModifier.h"

rvGEKeyValueModifier::rvGEKeyValueModifier ( const char* name, idWindow* window, const char* key, const char* value ) : 	
	rvGEModifier ( name, window ),
	mKey ( key ),
	mValue ( value )
{
	mUndoValue = mWrapper->GetStateDict().GetString ( mKey );
}

bool rvGEKeyValueModifier::Apply ( void )
{
	if ( mValue.Length ( ) )
	{
		mWrapper->SetStateKey ( mKey, mValue );
	}
	else
	{
		mWrapper->DeleteStateKey ( mKey );
	}

	return true;
}

bool rvGEKeyValueModifier::Undo ( void )
{
	mWrapper->SetStateKey ( mKey, mValue );

	return true;
}

bool rvGEKeyValueModifier::Merge ( rvGEModifier* mergebase )
{
	rvGEKeyValueModifier* merge = (rvGEKeyValueModifier*) mergebase;

	mValue = merge->mValue;	
	
	return true;
} 
