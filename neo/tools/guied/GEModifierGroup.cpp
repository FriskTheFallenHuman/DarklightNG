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

#include "GEModifierGroup.h"

rvGEModifierGroup::rvGEModifierGroup ( ) :
	rvGEModifier ( "Group", NULL )
{	
}

rvGEModifierGroup::~rvGEModifierGroup ( )
{
	int i;
	
	for ( i = 0; i < mModifiers.Num(); i ++ )
	{
		delete mModifiers[i];
	}
	
	mModifiers.Clear ( );
}

bool rvGEModifierGroup::Append ( rvGEModifier* mod )
{
	// All modifiers must be the same type
	assert ( !mModifiers.Num() || !idStr::Icmp ( mod->GetName ( ), mModifiers[0]->GetName ( ) ) );

	if ( !mModifiers.Num ( ) )
	{
		mName = mod->GetName ( );
	}

	mModifiers.Append ( mod );
	return true;
}

bool rvGEModifierGroup::IsValid ( void )
{
	int i;
	
	for ( i = 0; i < mModifiers.Num(); i ++ )
	{
		if ( !mModifiers[i]->IsValid ( ) )
		{
			return false;
		}
	}
	
	return true;
}

bool rvGEModifierGroup::Apply ( void )
{
	int i;
	
	for ( i = 0; i < mModifiers.Num(); i ++ )
	{
		mModifiers[i]->Apply ( );
	}
	
	return true;
}

bool rvGEModifierGroup::Undo ( void )
{
	int i;
	
	for ( i = 0; i < mModifiers.Num(); i ++ )
	{
		mModifiers[i]->Undo ( );
	}
	
	return true;
}

bool rvGEModifierGroup::CanMerge ( rvGEModifier* mergebase )
{
	rvGEModifierGroup*	merge = (rvGEModifierGroup*) mergebase;
	int					i;
			
	if ( mModifiers.Num() != merge->mModifiers.Num ( ) )
	{
		return false;
	}
			
	// Double check the merge is possible
	for ( i = 0; i < mModifiers.Num(); i ++ )
	{
		if ( mModifiers[i]->GetWindow() != merge->mModifiers[i]->GetWindow() )
		{
			return false;
		}
		
		if ( idStr::Icmp ( mModifiers[i]->GetName ( ), merge->mModifiers[i]->GetName ( ) ) )
		{
			return false;
		}

		if ( !mModifiers[i]->CanMerge ( merge->mModifiers[i] ) )
		{
			return false;
		}
	}

	return true;
}

bool rvGEModifierGroup::Merge ( rvGEModifier* mergebase )
{
	rvGEModifierGroup*	merge = (rvGEModifierGroup*) mergebase;
	int					i;
	
	// Double check the merge is possible
	for ( i = 0; i < mModifiers.Num(); i ++ )
	{
		mModifiers[i]->Merge ( merge->mModifiers[i] );
	}
	
	return true;
}
