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

class DialogAFConstraintFixed;
class DialogAFConstraintBallAndSocket;
class DialogAFConstraintUniversal;
class DialogAFConstraintHinge;
class DialogAFConstraintSlider;
class DialogAFConstraintSpring;

// DialogAFConstraint dialog

class DialogAFConstraint : public CDialog {

	DECLARE_DYNAMIC(DialogAFConstraint)

public:
						DialogAFConstraint( CWnd* pParent = NULL );   // standard constructor
	virtual				~DialogAFConstraint();
	void				LoadFile( idDeclAF *af );
	void				SaveFile( void );
	void				LoadConstraint( const char *name );
	void				SaveConstraint( void );
	void				UpdateFile( void );

	enum				{ IDD = IDD_DIALOG_AF_CONSTRAINT };

protected:
	virtual BOOL		OnInitDialog();
	virtual void		DoDataExchange( CDataExchange* pDX );    // DDX/DDV support
	virtual int			OnToolHitTest( CPoint point, TOOLINFO* pTI ) const;
	afx_msg BOOL		OnToolTipNotify( UINT id, NMHDR *pNMHDR, LRESULT *pResult );
	afx_msg void		OnShowWindow( BOOL bShow, UINT nStatus );
	afx_msg void		OnCbnSelchangeComboConstraints();
	afx_msg void		OnBnClickedButtonNewconstraint();
	afx_msg void		OnBnClickedButtonRenameconstraint();
	afx_msg void		OnBnClickedButtonDeleteconstraint();
	afx_msg void		OnCbnSelchangeComboConstraintType();
	afx_msg void		OnCbnSelchangeComboConstraintBody1();
	afx_msg void		OnCbnSelchangeComboConstraintBody2();
	afx_msg void		OnEnChangeEditConstraintFriction();
	afx_msg void		OnDeltaposSpinConstraintFriction(NMHDR *pNMHDR, LRESULT *pResult);

	DECLARE_MESSAGE_MAP()

private:
	idDeclAF *			file;
	idDeclAF_Constraint*constraint;
	CDialog *			constraintDlg;
	DialogAFConstraintFixed *fixedDlg;
	DialogAFConstraintBallAndSocket *ballAndSocketDlg;
	DialogAFConstraintUniversal *universalDlg;
	DialogAFConstraintHinge *hingeDlg;
	DialogAFConstraintSlider *sliderDlg;
	DialogAFConstraintSpring *springDlg;

	//{{AFX_DATA(DialogAFConstraint)
	CComboBox			m_comboConstraintList;			// list with constraints
	CComboBox			m_comboConstraintType;
	CComboBox			m_comboBody1List;
	CComboBox			m_comboBody2List;
	float				m_friction;
	//}}AFX_DATA

	static toolTip_t	toolTips[];

private:
	void				InitConstraintList( void );
	void				InitConstraintTypeDlg( void );
	void				InitBodyLists( void );
	void				InitNewRenameDeleteButtons( void );
};
