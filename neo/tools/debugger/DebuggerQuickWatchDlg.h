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
#ifndef DEBUGGERQUICKWATCHDLG_H_
#define DEBUGGERQUICKWATCHDLG_H_

class rvDebuggerWindow;

class rvDebuggerQuickWatchDlg
{
public:

	rvDebuggerQuickWatchDlg ( );

	bool	DoModal				( rvDebuggerWindow* window, int callstackDepth, const char* variable = NULL );

protected:

	HWND				mWnd;
	int					mCallstackDepth;
	idStr				mVariable;
	rvDebuggerWindow*	mDebuggerWindow;

	void				SetVariable	( const char* varname, bool force = false );

private:

	int					mEditFromRight;
	int					mButtonFromRight;
	int					mEditFromBottom;

	static INT_PTR	CALLBACK DlgProc ( HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam );
};

#endif // DEBUGGERQUICKWATCHDLG_H_