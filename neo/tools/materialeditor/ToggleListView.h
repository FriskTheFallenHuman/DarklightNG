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
#pragma once

#include <afxcview.h>

/**
* A simple list view that supports a toggle button. ToggleListView is a simple extension
* to the CListView class that support a toggle button. It is limited to a single column
* and always uses full row select. The toggle state is stored in the data for each item 
* so users of this class should not attempt to use the data field for storage. lparam can
* be used instead.
*/
class ToggleListView : public CListView {

public:
	/**
	* Enumeration that defines the possible states of the toggle button.
	*/
	enum {
		TOGGLE_STATE_DISABLED = 0,
		TOGGLE_STATE_ON,
		TOGGLE_STATE_OFF
	};

public:
	void				SetToggleIcons(LPCSTR disabled = NULL, LPCSTR on = NULL, LPCSTR off = NULL);
	void				SetToggleState(int index, int toggleState, bool notify = false);
	int					GetToggleState(int index);
	
	//Windows messages
	afx_msg int			OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void 		OnSize(UINT nType, int cx, int cy);
	afx_msg void 		MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
	afx_msg void 		OnNMClick(NMHDR *pNMHDR, LRESULT *pResult);
	
	DECLARE_MESSAGE_MAP()


protected:
	ToggleListView();
	virtual ~ToggleListView();

	DECLARE_DYNCREATE(ToggleListView)

	//Overrides
	virtual BOOL		PreCreateWindow(CREATESTRUCT& cs);
	virtual void		DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);

	//Virtual Event Methods for sub-classes
	virtual void		OnStateChanged(int index, int toggleState) {};

	void				Draw3dRect(HDC hDC, RECT* rect, HBRUSH topLeft, HBRUSH bottomRight);

protected:
	HICON				onIcon;
	HICON				offIcon;
	HICON				disabledIcon;
	
};


