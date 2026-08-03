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
#include "../../renderer/tr_local.h"
#include "../posix/posix_public.h"
#include "local.h"

/*
==========
input
==========
*/

void Sys_InitInput( void ) { }

void Sys_ShutdownInput( void ) { }

void Sys_GrabMouseCursor( bool ) { }

int Sys_PollMouseInputEvents( void ) { return 0; }

void Sys_EndMouseInputEvents( void ) { }

int Sys_ReturnMouseInputEvent( const int n, int &action, int &value ) { return 0; }

int Sys_PollKeyboardInputEvents( void ) { return 0; }

void Sys_EndKeyboardInputEvents( void ) { }

int Sys_ReturnKeyboardInputEvent( const int n, int &action, bool &state ) { return 0; }

unsigned char Sys_MapCharForKey( int key ) { return (unsigned char)key; }

/*
================
Sys_GetVideoRam
returns in megabytes
================
*/
int Sys_GetVideoRam( void ) {
	return 64;
}

/*
==========
GL
==========
*/

void GLimp_EnableLogging( bool enable ) { }

bool GLimp_Init( glimpParms_t a ) { return true; }

void GLimp_SetGamma( unsigned short red[256], 
				    unsigned short green[256],
					unsigned short blue[256] ) { }

void GLimp_Shutdown( void ) { }

void GLimp_SwapBuffers( void ) { }

void GLimp_DeactivateContext( void ) { }

void GLimp_ActivateContext( void ) { }

bool GLimp_SetScreenParms( glimpParms_t parms ) { return true; }

