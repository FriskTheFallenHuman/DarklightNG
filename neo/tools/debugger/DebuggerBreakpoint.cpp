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

#include "DebuggerApp.h"
#include "DebuggerBreakpoint.h"

int rvDebuggerBreakpoint::mNextID = 1;

rvDebuggerBreakpoint::rvDebuggerBreakpoint ( const char* filename, int linenumber, int id )
{
	mFilename = filename;
	mLineNumber = linenumber;
	mEnabled = true;
	
	if ( id == -1 )
	{	
		mID = mNextID++;
	}	
	else 
	{
		mID = id;
	}
}

rvDebuggerBreakpoint::rvDebuggerBreakpoint ( rvDebuggerBreakpoint& bp )
{
	mFilename = bp.mFilename;
	mEnabled = bp.mEnabled;
	mLineNumber = bp.mLineNumber;
}

rvDebuggerBreakpoint::~rvDebuggerBreakpoint ( void )
{
}
