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
#ifndef GESTATUSBAR_H_
#define GESTATUSBAR_H_

class rvGEStatusBar
{
public:

	rvGEStatusBar ( );

	bool	Create			( HWND parent, UINT id, bool visible = true );
	void	Resize			( int width, int height );	
	
	HWND	GetWindow		( void );

	void	SetZoom			( int zoom );
	void	SetTriangles	( int tris );
	void	SetSimple		( bool simple );
	
	void	Show			( bool state );
	void	Update			( void );
		
protected:

	HWND	mWnd;
	bool	mSimple;
	int		mZoom;
	int		mTriangles;
};

ID_INLINE HWND rvGEStatusBar::GetWindow ( void )
{
	return mWnd;
}

ID_INLINE void rvGEStatusBar::SetZoom ( int zoom )
{
	if ( mZoom != zoom )
	{
		mZoom = zoom;
		Update ( );
	}
}

ID_INLINE void rvGEStatusBar::SetTriangles ( int triangles )
{
	if ( triangles != mTriangles )
	{
		mTriangles = triangles;
		Update ( );
	}
}

ID_INLINE void rvGEStatusBar::SetSimple ( bool simple )
{
	if ( mSimple != simple )
	{
		mSimple = simple;
		Update ( );
	}
}

#endif // GESTATUSBAR_H_