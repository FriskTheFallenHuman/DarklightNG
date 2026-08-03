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
#include "GEModifierStack.h"

rvGEModifierStack::rvGEModifierStack ( )
{
	mCurrentModifier = -1;
}

rvGEModifierStack::~rvGEModifierStack ( )
{
	Reset ( );
}

void rvGEModifierStack::Reset ( void )
{
	int i;
	
	for ( i = 0; i < mModifiers.Num ( ); i ++ )
	{
		delete mModifiers[i];
	}
	
	mModifiers.Clear ( );
}

bool rvGEModifierStack::Append ( rvGEModifier* modifier )
{
	// TODO: Add the modifier and clear all redo modifiers
	if ( !modifier->IsValid ( ) )
	{
		delete modifier;
		return false;
	}

	while ( mCurrentModifier < mModifiers.Num ( ) - 1 )
	{
		delete mModifiers[mModifiers.Num()-1];
		mModifiers.RemoveIndex ( mModifiers.Num()-1 );
	}
	
	if ( !mMergeBlock && mModifiers.Num ( ) )
	{
		rvGEModifier* top = mModifiers[mModifiers.Num()-1];
		
		// See if the two modifiers can merge
		if ( top->GetWindow() == modifier->GetWindow() &&
			 !idStr::Icmp ( top->GetName ( ), modifier->GetName ( ) ) &&
			 top->CanMerge ( modifier ) )
		{
			// Merge the two modifiers
			if ( top->Merge ( modifier ) )
			{
				top->Apply ( );
				
				gApp.GetProperties().Update ( );
				gApp.GetTransformer().Update ( );
			
				delete modifier;
				return true;
			}		
		}
	}
	
	mModifiers.Append ( modifier );
	mCurrentModifier = mModifiers.Num ( ) - 1;
	
	modifier->Apply ( );
	
	mMergeBlock = false;

	gApp.GetProperties().Update ( );
	gApp.GetTransformer().Update ( );

	return true;
}

bool rvGEModifierStack::Undo ( void )
{
	if ( mCurrentModifier < 0 )
	{
		return false;
	}
	
	mModifiers[mCurrentModifier]->Undo ( );
	mCurrentModifier--;

	gApp.GetProperties().Update ( );
	gApp.GetTransformer().Update ( );

	return true;
}

bool rvGEModifierStack::Redo ( void )
{
	if ( mCurrentModifier + 1 < mModifiers.Num ( ) )
	{
		mCurrentModifier++;
		mModifiers[mCurrentModifier]->Apply ( );
	}

	gApp.GetProperties().Update ( );
	gApp.GetTransformer().Update ( );

	return true;
}

