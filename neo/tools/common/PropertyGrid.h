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

#ifndef PROPERTYGRID_H_
#define PROPERTYGRID_H_

#define PGN_ITEMCHANGED		100

#define PGS_HEADERS			0x00000001
#define PGS_ALLOWINSERT		0x00000002

typedef struct
{
	NMHDR			hdr;
	int				mItem;
	const char*		mName;
	const char*		mValue;	

} NMPROPGRID;

class rvPropertyGrid
{
public:

	enum EItemType
	{
		PGIT_STRING,
		PGIT_HEADER,
		PGIT_MAX
	};

	rvPropertyGrid ( );
	
	bool	Create			( HWND parent, int id, int style = 0 );

	void	Move			( int x, int y, int w, int h, BOOL redraw = FALSE );

	bool	ReflectMessage	( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

	int		AddItem			( const char* name, const char* value, EItemType type = PGIT_STRING );
	
	void	RemoveItem		( int index );
	void	RemoveAllItems	( void );
	
	void	SetCurSel		( int index );
	int		GetCurSel		( void );

	HWND			GetWindow		( void );
	const char*		GetItemName		( int index );
	const char*		GetItemValue	( int index );

protected:

	enum EState
	{
		STATE_FINISHEDIT,
		STATE_EDIT,
		STATE_NORMAL,
	};

	void			StartEdit		( int item, bool label );
	void			FinishEdit		( void );
	void			CancelEdit		( void );

	int				HandleDrawItem	( WPARAM wParam, LPARAM lParam );

	HWND		mWindow;
	HWND		mEdit;
	int			mEditItem;
	bool		mEditLabel;
	int			mSelectedItem;
	WNDPROC		mListWndProc;
	int			mSplitter;
	int			mStyle;
	EState		mState;
	
private:

	static LRESULT CALLBACK WndProc ( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
};

inline HWND rvPropertyGrid::GetWindow ( void )
{
	return mWindow;
}

inline int rvPropertyGrid::GetCurSel ( void )
{
	return SendMessage ( mWindow, LB_GETCURSEL, 0, 0 );
}

inline void rvPropertyGrid::SetCurSel ( int index )
{
	SendMessage ( mWindow, LB_SETCURSEL, index, 0 );
	mSelectedItem = index;
}

#endif // PROPERTYGRID_H_
