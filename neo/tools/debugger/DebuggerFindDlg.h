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
#ifndef DEBUGGERFINDDLG_H_
#define DEBUGGERFINDDLG_H_

class rvDebuggerWindow;

class rvDebuggerFindDlg
{
public:

	rvDebuggerFindDlg ( );

	bool	DoModal				( rvDebuggerWindow* window );

	const char*		GetFindText	( void );

protected:

	HWND	mWnd;

private:

	static char		mFindText[ 256 ];

	static INT_PTR	CALLBACK DlgProc ( HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam );
};

ID_INLINE const char* rvDebuggerFindDlg::GetFindText ( void )
{
	return mFindText;
}

#endif // DEBUGGERFINDDLG_H_