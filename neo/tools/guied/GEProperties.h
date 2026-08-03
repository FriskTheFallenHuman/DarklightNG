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

#ifndef GEPROPERTIES_H_
#define GEPROPERTIES_H_

#ifndef PROPERTYGRID_H_
#include "../common/PropertyGrid.h"
#endif

class rvGEWorkspace;
class rvGEWindowWrapper;

class rvGEProperties
{
public:

	rvGEProperties ( );
	
	bool	Create				( HWND parent, bool visible );
	void	Show				( bool visibile );

	void	SetWorkspace		( rvGEWorkspace* workspace );

	void	Update				( void );	
	bool	SetProperty			( const char* name, const char* value );

	HWND	GetWindow			( void );
	
protected:

	bool	AddModifier			( const char* name, const char* value );

	static LRESULT CALLBACK WndProc ( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

	HWND				mWnd;
	rvPropertyGrid		mGrid;
	rvGEWindowWrapper*	mWrapper;
	rvGEWorkspace*		mWorkspace;	
};

ID_INLINE HWND rvGEProperties::GetWindow ( void )
{
	return mWnd;
}

ID_INLINE void rvGEProperties::SetWorkspace ( rvGEWorkspace* workspace )
{
	mWorkspace = workspace;
	Update ( );
}

#endif // GEPROPERTIES_H_
