/*
===========================================================================

DarklightNG Source Code
Copyright (C) 2026 - Justin Marshall (aka IceColdDuke).

SDL2 owns keyboard focus and grabbing. The old global low-level Windows hook
is intentionally retired; SDL_SetWindowGrab and relative mouse mode provide
scoped capture without modifying system-wide task keys.

===========================================================================
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "sys_platform.h"

void DisableTaskKeys( BOOL disable, BOOL beep, BOOL taskManager ) {
	if ( disable ) {
		common->DPrintf( "SDL2 input capture leaves operating-system task keys enabled\n" );
	}
}
