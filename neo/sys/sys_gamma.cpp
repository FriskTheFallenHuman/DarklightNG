/*
===========================================================================

DarklightNG Source Code
Copyright (C) 2026 - Justin Marshall (aka IceColdDuke).

SDL2 display gamma implementation.

===========================================================================
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "sys_platform.h"

void GLimp_SetGamma( unsigned short red[256], unsigned short green[256], unsigned short blue[256] ) {
	if ( win32.sdlWindow == NULL ) {
		return;
	}
	if ( SDL_SetWindowGammaRamp( win32.sdlWindow, red, green, blue ) != 0 ) {
		common->DPrintf( "SDL_SetWindowGammaRamp failed: %s\n", SDL_GetError() );
	}
}
