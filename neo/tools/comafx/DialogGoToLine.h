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

#ifndef __DIALOGGOTOLINE_H__
#define __DIALOGGOTOLINE_H__

// DialogGoToLine dialog

class DialogGoToLine : public CDialog {

	DECLARE_DYNAMIC(DialogGoToLine)

public:

						DialogGoToLine( CWnd* pParent = NULL );   // standard constructor
	virtual				~DialogGoToLine();

	enum				{ IDD = IDD_DIALOG_GOTOLINE };

	void				SetRange( int firstLine, int lastLine );
	int					GetLine( void ) const;

protected:
	virtual BOOL		OnInitDialog();
	virtual void		DoDataExchange( CDataExchange* pDX );    // DDX/DDV support
	afx_msg void		OnBnClickedOk();

	DECLARE_MESSAGE_MAP()

private:

	CEdit				numberEdit;
	int					firstLine;
	int					lastLine;
	int					line;
};

#endif /* !__DIALOGGOTOLINE_H__ */
