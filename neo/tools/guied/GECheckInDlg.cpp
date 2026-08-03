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
#pragma hdrstop

#include "../../sys/rc/guied_resource.h"

#include "GEApp.h"

typedef struct
{
	const char*		mFilename;
	idStr*			mComment;
	
} GECHECKINDLG;

/*
================
GECheckInDlg_GeneralProc

Dialog procedure for the check in dialog
================
*/
static INT_PTR CALLBACK GECheckInDlg_GeneralProc ( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	GECHECKINDLG* dlg = (GECHECKINDLG*) GetWindowLong ( hwnd, GWL_USERDATA );
	
	switch ( msg )
	{
		case WM_INITDIALOG:		
			SetWindowLong ( hwnd, GWL_USERDATA, lParam );
			dlg = (GECHECKINDLG*) lParam;
			
			SetWindowText ( GetDlgItem ( hwnd, IDC_GUIED_FILENAME ), dlg->mFilename );
			break;
			
		case WM_COMMAND:
			switch ( LOWORD ( wParam ) )
			{
				case IDOK:
				{
					char* temp;
					int	  tempsize;
					
					tempsize = GetWindowTextLength ( GetDlgItem ( hwnd, IDC_GUIED_COMMENT ) );
					temp = new char [ tempsize + 2 ];
					GetWindowText ( GetDlgItem ( hwnd, IDC_GUIED_COMMENT ), temp, tempsize + 1 );
					
					*dlg->mComment = temp;
					
					delete[] temp;
					
					EndDialog ( hwnd, 1 );
					break;
				}
					
				case IDCANCEL:
					EndDialog ( hwnd, 0 );
					break;
			}
			break;
	}
	
	return FALSE;
}

/*
================
GECheckInDlg_DoModal

Starts the check in dialog
================
*/
bool GECheckInDlg_DoModal ( HWND parent, const char* filename, idStr* comment )
{
	GECHECKINDLG	dlg;
	
	dlg.mComment = comment;
	dlg.mFilename = filename;
	
	if ( !DialogBoxParam ( gApp.GetInstance(), MAKEINTRESOURCE(IDD_GUIED_CHECKIN), parent, GECheckInDlg_GeneralProc, (LPARAM) &dlg ) )
	{
		return false;
	}
	
	return true;
}

