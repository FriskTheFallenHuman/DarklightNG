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

#include "../../sys/rc/common_resource.h"
#include "DialogName.h"

/////////////////////////////////////////////////////////////////////////////
// DialogName dialog


DialogName::DialogName(const char *pName, CWnd* pParent /*=NULL*/)
	: CDialog(DialogName::IDD, pParent)
{
	//{{AFX_DATA_INIT(DialogName)
	m_strName = _T("");
	//}}AFX_DATA_INIT
	m_strCaption = pName;
}


void DialogName::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(DialogName)
	DDX_Text(pDX, IDC_TOOLS_EDITNAME, m_strName);
	//}}AFX_DATA_MAP
}

BOOL DialogName::OnInitDialog() 
{
	CDialog::OnInitDialog();

	SetWindowText(m_strCaption);
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

BEGIN_MESSAGE_MAP(DialogName, CDialog)
	//{{AFX_MSG_MAP(DialogName)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// DialogName message handlers

void DialogName::OnOK() 
{
	CDialog::OnOK();
}
