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

#ifndef SPINBUTTON_H_
#define SPINBUTTON_H_

void SpinButton_SetIncrement ( HWND hWnd, float inc );
void SpinButton_HandleNotify ( NMHDR* hdr );
void SpinButton_SetRange	 ( HWND hWnd, float min, float max );

#endif // SPINBUTOTN_H_
