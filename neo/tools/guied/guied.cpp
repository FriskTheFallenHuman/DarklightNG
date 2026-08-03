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

#include "../../renderer/tr_local.h"
#include "../../sys/win32/win_local.h"
#include <io.h>

#include "../../ui/DeviceContext.h"
#include "../../sys/win32/rc/guied_resource.h"

#include "GEApp.h"
#include "GEImGui.h"

rvGEApp		gApp;
	
/*
================
GUIEditorInit

Start the gui editor
================
*/
void GUIEditorInit( void ) 
{
	if ( !gApp.IsActive() && !gApp.Initialize() )
	{
		common->Warning ( "Could not initialize the GUI editor backend.\n" );
		return;
	}
	if ( !GEImGuiCreate() )
	{
		common->Warning ( "Could not create the ImGui GUI editor window.\n" );
		return;
	}
	com_editors |= EDITOR_GUI;
	GEImGuiShow();
}

/*
================
GUIEditorShutdown
================
*/
void GUIEditorShutdown( void ) {
	GEImGuiDestroy();
}

void GUIEditorToggle( void ) {
	if ( GEImGuiIsVisible() ) {
		GUIEditorHide();
	} else {
		GUIEditorInit();
	}
}

void GUIEditorHide( void ) {
	GEImGuiHide();
	com_editors &= ~EDITOR_GUI;
}

bool GUIEditorIsVisible( void ) {
	return GEImGuiIsVisible();
}

/*
================
GUIEditorHandleMessage

Handle translator messages
================
*/
bool GUIEditorHandleMessage ( void *msg )
{
	if ( !gApp.IsActive ( ) || !GEImGuiIsVisible() )
	{
		return false;
	}

	return gApp.TranslateAccelerator( reinterpret_cast<LPMSG>(msg) );
}

/*
================
GUIEditorRun

Run a frame 
================
*/
void GUIEditorRun() 
{
	// Win_Frame/Sys_PumpEvents (and MFC while Radiant is active) already owns
	// the process message queue. Pumping it a second time here can consume the
	// other editor's close/quit messages.
	GEImGuiFrame ( );
	
	// The GUI editor runs too hot so we need to slow it down a bit.
	Sleep ( 1 );
}

/*
================
StringFromVec4

Returns a clean string version of the given vec4
================
*/
const char *StringFromVec4 ( idVec4& v )
{
	return va( "%s,%s,%s,%s",
		idStr::FloatArrayToString( &v[0], 1, 8 ),
		idStr::FloatArrayToString( &v[1], 1, 8 ),
		idStr::FloatArrayToString( &v[2], 1, 8 ),
		idStr::FloatArrayToString( &v[3], 1, 8 ) );
}

/*
================
IsExpression

Returns true if the given string is an expression
================
*/
bool IsExpression ( const char* s )
{
	idParser src( s, strlen ( s ), "", 
				  LEXFL_ALLOWMULTICHARLITERALS		| 
				  LEXFL_NOSTRINGCONCAT				| 
				  LEXFL_ALLOWBACKSLASHSTRINGCONCAT	|
				  LEXFL_NOFATALERRORS );

	idToken token;
	bool	needComma = false;
	bool	needNumber = false;
	while ( src.ReadToken ( &token ) )
	{
		switch ( token.type )
		{
			case TT_NUMBER:
				needComma = true;
				needNumber = false;
				break;
			
			case TT_PUNCTUATION:
				if ( needNumber )
				{
					return true;
				}				
				if ( token[0] == ',' )
				{
					if ( !needComma )
					{
						return true;
					}
					
					needComma = false;
					break;
				}

				if ( needComma )
				{
					return true;
				}

				if ( token[0] == '-' )
				{
					needNumber = true;
				}
				break;
				
			default:
				return true;
		}
	}
					
	return false;
}
