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

#include "MaterialEditor.h"
#include "../common/registryoptions.h"

class MEMainFrame;

/**
* Dialog that provides an input box and several checkboxes to define
* the parameters of a search. These parameters include: text string, search
* scope and search only name flag.
*/
class FindDialog : public CDialog
{

public:
	enum { IDD = IDD_FIND };
	
public:
	FindDialog(CWnd* pParent = NULL);
	virtual ~FindDialog();

	BOOL					Create();

protected:
	DECLARE_DYNAMIC(FindDialog)

	//Overrides
	virtual void			DoDataExchange(CDataExchange* pDX);
	virtual BOOL			OnInitDialog();

	//Messages
	afx_msg void			OnBnClickedFindNext();
	virtual void			OnCancel();
	DECLARE_MESSAGE_MAP()

	//Protected Operations
	void					LoadFindSettings();
	void					SaveFindSettings();

protected:
	MEMainFrame*			parent;
	MaterialSearchData_t	searchData;
	rvRegistryOptions		registry;
};
