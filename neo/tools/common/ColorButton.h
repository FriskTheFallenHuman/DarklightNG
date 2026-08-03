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
#ifndef COLORBUTTON_H_
#define COLORBUTTON_H_

void		ColorButton_DrawItem	( HWND hWnd, LPDRAWITEMSTRUCT dis );
void		ColorButton_SetColor	( HWND hWnd, COLORREF color );
void		ColorButton_SetColor	( HWND hWnd, const char* color );
COLORREF	ColorButton_GetColor	( HWND hWnd );

void		AlphaButton_SetColor	( HWND hWnd, const char* color );

void		AlphaButton_OpenPopup	( HWND button );

#endif // COLORBUTTON_H_