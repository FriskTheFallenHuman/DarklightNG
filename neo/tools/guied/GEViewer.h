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

#ifndef GEVIEWER_H_
#define GEVIEWER_H_

class rvGEViewer
{
public:

	rvGEViewer ( );

	bool				Create		( HWND parent );
	bool				Destroy		( void );
	bool				OpenFile	( const char* filename );
	
	void				RunFrame	( void );
	
	HWND				GetWindow	( void );
		
protected:

	void				Render		( HDC dc );
	void				Play		( void );
	void				Pause		( void );

	HWND					mWnd;
	int						mWindowWidth;
	int						mWindowHeight;
	int						mToolbarHeight;
	idUserInterfaceLocal*	mInterface;
	bool					mPaused;
	HWND					mToolbar;
	int						mLastTime;
	int						mTime;
	
	LRESULT		HandlePaint	( WPARAM wParam, LPARAM lParam );
	
private:

	bool	SetupPixelFormat ( void );

	static LRESULT CALLBACK WndProc ( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam );
};

ID_INLINE HWND rvGEViewer::GetWindow ( void )
{
	return mWnd;
}

#endif // GEVIEWER_H_