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
#include "GEDeleteModifier.h"

rvGEDeleteModifier::rvGEDeleteModifier ( const char* name, idWindow* window ) :
	rvGEModifier ( name, window )
{
}

/*
================
rvGEDeleteModifier::Apply

Apply the delete modifier by setting the deleted flag in the wrapper
================
*/
bool rvGEDeleteModifier::Apply ( void )
{
	mWrapper->SetDeleted ( true );
	
	return true;
}

/*
================
rvGEDeleteModifier::Undo

Undo the delete modifier by unsetting the deleted flag in the wrapper
================
*/
bool rvGEDeleteModifier::Undo ( void )
{
	mWrapper->SetDeleted ( false );
	
	return true;
}

