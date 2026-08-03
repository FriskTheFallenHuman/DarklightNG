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

#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <direct.h>
#include <io.h>
#include <conio.h>
#include <mapi.h>
#include <ShellAPI.h>

#ifndef __MRC__
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include "../sys_local.h"
#include "win_local.h"
#include "rc/CreateResourceIDs.h"
#include "../../renderer/tr_local.h"
#include "../../tools/common/EditorTheme.h"

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")

idCVar Win32Vars_t::sys_arch( "sys_arch", "", CVAR_SYSTEM | CVAR_INIT, "" );
idCVar Win32Vars_t::sys_cpustring( "sys_cpustring", "detect", CVAR_SYSTEM | CVAR_INIT, "" );
idCVar Win32Vars_t::in_mouse( "in_mouse", "1", CVAR_SYSTEM | CVAR_BOOL, "enable mouse input" );
idCVar Win32Vars_t::win_allowAltTab( "win_allowAltTab", "0", CVAR_SYSTEM | CVAR_BOOL, "allow Alt-Tab when fullscreen" );
idCVar Win32Vars_t::win_notaskkeys( "win_notaskkeys", "0", CVAR_SYSTEM | CVAR_INTEGER, "disable windows task keys" );
idCVar Win32Vars_t::win_username( "win_username", "", CVAR_SYSTEM | CVAR_INIT, "windows user name" );
idCVar Win32Vars_t::win_xpos( "win_xpos", "3", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_INTEGER, "horizontal position of window" );
idCVar Win32Vars_t::win_ypos( "win_ypos", "22", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_INTEGER, "vertical position of window" );
idCVar Win32Vars_t::win_outputDebugString( "win_outputDebugString", "0", CVAR_SYSTEM | CVAR_BOOL, "" );
idCVar Win32Vars_t::win_outputEditString( "win_outputEditString", "1", CVAR_SYSTEM | CVAR_BOOL, "" );
idCVar Win32Vars_t::win_viewlog( "win_viewlog", "0", CVAR_SYSTEM | CVAR_INTEGER, "" );
idCVar Win32Vars_t::win_timerUpdate( "win_timerUpdate", "0", CVAR_SYSTEM | CVAR_BOOL, "allows the game to be updated while dragging the window" );
idCVar Win32Vars_t::win_allowMultipleInstances( "win_allowMultipleInstances", "0", CVAR_SYSTEM | CVAR_BOOL, "allow multiple instances running concurrently" );

Win32Vars_t	win32;

static char		sys_cmdline[MAX_STRING_CHARS];

#ifdef ID_ALLOW_TOOLS
/*
==================
Dark Windows 98 editor theme

The dark control palette is provided by the modern theme API where available,
with GDI color fallbacks for legacy MFC dialogs. The title bar remains custom:
it is painted as a Windows 98-style gradient whose colors are live cvars.
==================
*/
idCVar win_editorDarkTheme( "win_editorDarkTheme", "1", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_BOOL, "use the dark editor theme" );
idCVar win_editorMainTitleStart( "win_editorMainTitleStart", "484848", CVAR_SYSTEM | CVAR_ARCHIVE, "main editor title gradient start color (RRGGBB)" );
idCVar win_editorMainTitleEnd( "win_editorMainTitleEnd", "181818", CVAR_SYSTEM | CVAR_ARCHIVE, "main editor title gradient end color (RRGGBB)" );
idCVar win_editorTitleStart( "win_editorTitleStart", "0A246A", CVAR_SYSTEM | CVAR_ARCHIVE, "active child-window title gradient start color (RRGGBB)" );
idCVar win_editorTitleEnd( "win_editorTitleEnd", "A6CAF0", CVAR_SYSTEM | CVAR_ARCHIVE, "active child-window title gradient end color (RRGGBB)" );
idCVar win_editorInactiveTitleStart( "win_editorInactiveTitleStart", "404040", CVAR_SYSTEM | CVAR_ARCHIVE, "inactive editor title gradient start color (RRGGBB)" );
idCVar win_editorInactiveTitleEnd( "win_editorInactiveTitleEnd", "202020", CVAR_SYSTEM | CVAR_ARCHIVE, "inactive editor title gradient end color (RRGGBB)" );
idCVar win_editorTitleText( "win_editorTitleText", "FFFFFF", CVAR_SYSTEM | CVAR_ARCHIVE, "editor title text color (RRGGBB)" );
idCVar win_editorBackground( "win_editorBackground", "202020", CVAR_SYSTEM | CVAR_ARCHIVE, "editor background color (RRGGBB)" );
idCVar win_editorControl( "win_editorControl", "303030", CVAR_SYSTEM | CVAR_ARCHIVE, "editor control surface color (RRGGBB)" );
idCVar win_editorField( "win_editorField", "181818", CVAR_SYSTEM | CVAR_ARCHIVE, "editor input and list color (RRGGBB)" );
idCVar win_editorText( "win_editorText", "E6E6E6", CVAR_SYSTEM | CVAR_ARCHIVE, "editor control text color (RRGGBB)" );
idCVar win_editorSelection( "win_editorSelection", "0A64AD", CVAR_SYSTEM | CVAR_ARCHIVE, "editor selection color (RRGGBB)" );
idCVar win_editorBorder( "win_editorBorder", "555555", CVAR_SYSTEM | CVAR_ARCHIVE, "editor separator and border color (RRGGBB)" );
idCVar win_editorTitleHeight( "win_editorTitleHeight", "38", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_INTEGER, "editor title bar height in pixels" );
idCVar win_editorTitleFontSize( "win_editorTitleFontSize", "17", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_INTEGER, "editor title font height in pixels" );
idCVar win_editorMenuHeight( "win_editorMenuHeight", "40", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_INTEGER, "editor menu row height in pixels" );
idCVar win_editorMenuFontSize( "win_editorMenuFontSize", "20", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_INTEGER, "editor menu font height in pixels" );

typedef void (WINAPI *setThemeAppProperties_t)( DWORD );
typedef HRESULT (WINAPI *setWindowTheme_t)( HWND, LPCWSTR, LPCWSTR );
typedef HRESULT (WINAPI *dwmSetWindowAttribute_t)( HWND, DWORD, LPCVOID, DWORD );
typedef int (WINAPI *setPreferredAppMode_t)( int );
typedef BOOL (WINAPI *allowDarkModeForWindow_t)( HWND, BOOL );
typedef void (WINAPI *flushMenuThemes_t)( void );
typedef BOOL (WINAPI *gradientFill_t)( HDC, PTRIVERTEX, ULONG, PVOID, ULONG, ULONG );

static setWindowTheme_t		win32SetWindowTheme;
static dwmSetWindowAttribute_t	win32DwmSetWindowAttribute;
static allowDarkModeForWindow_t	win32AllowDarkModeForWindow;
static gradientFill_t		win32GradientFill;
static HHOOK				win32EditorThemeHook;
static HHOOK				win32EditorThemeMessageHook;
static bool				win32EditorThemeEnabled;
static const char			*win32EditorThemeWindowProc = "DarklightEditorThemeWindowProc";
static const char			*win32EditorCaptionPressed = "DarklightEditorCaptionPressed";
static const char			*win32EditorCaptionPressedInside = "DarklightEditorCaptionPressedInside";
static HFONT				win32EditorCaptionFont;
static HFONT				win32EditorMenuFont;
static int				win32EditorCaptionFontSize;
static int				win32EditorMenuFontSize;
static HBRUSH				win32EditorBackgroundBrush;
static HBRUSH				win32EditorControlBrush;
static HBRUSH				win32EditorFieldBrush;
static HBRUSH				win32EditorSelectionBrush;
static HBRUSH				win32EditorBorderBrush;
static COLORREF			win32EditorBackgroundBrushColor = CLR_INVALID;
static COLORREF			win32EditorControlBrushColor = CLR_INVALID;
static COLORREF			win32EditorFieldBrushColor = CLR_INVALID;
static COLORREF			win32EditorSelectionBrushColor = CLR_INVALID;
static COLORREF			win32EditorBorderBrushColor = CLR_INVALID;

static const DWORD		win32EditorMenuItemMagic = 0x394D5444;	// DTM9

struct win32EditorMenuItem_t {
	DWORD	magic;
	bool	menuBar;
	bool	separator;
	bool	hasSubmenu;
	char	text[512];
};

static int Sys_EditorThemeMetric( const idCVar &metricCvar, int fallback, int minimum, int maximum ) {
	int value = metricCvar.GetInteger();
	if ( value <= 0 ) {
		value = fallback;
	}
	return max( minimum, min( maximum, value ) );
}

static int Sys_HexColorDigit( char digit ) {
	if ( digit >= '0' && digit <= '9' ) {
		return digit - '0';
	}
	if ( digit >= 'a' && digit <= 'f' ) {
		return digit - 'a' + 10;
	}
	if ( digit >= 'A' && digit <= 'F' ) {
		return digit - 'A' + 10;
	}
	return -1;
}

static COLORREF Sys_EditorThemeColor( const idCVar &colorCvar, COLORREF fallback ) {
	const char *colorText = colorCvar.GetString();
	if ( colorText[0] == '#' ) {
		colorText++;
	}

	int rgb = 0;
	for ( int i = 0; i < 6; i++ ) {
		const int digit = Sys_HexColorDigit( colorText[i] );
		if ( digit < 0 ) {
			return fallback;
		}
		rgb = ( rgb << 4 ) | digit;
	}
	if ( colorText[6] != '\0' ) {
		return fallback;
	}

	return RGB( ( rgb >> 16 ) & 255, ( rgb >> 8 ) & 255, rgb & 255 );
}

static HBRUSH Sys_EditorThemeBrush( HBRUSH &brush, COLORREF &brushColor, COLORREF color ) {
	if ( !brush || brushColor != color ) {
		if ( brush ) {
			DeleteObject( brush );
		}
		brush = CreateSolidBrush( color );
		brushColor = color;
	}
	return brush;
}

bool Sys_EditorDarkThemeEnabled() {
	return win_editorDarkTheme.GetBool();
}

COLORREF Sys_GetEditorThemeColor( editorThemeColor_t role ) {
	switch ( role ) {
		case EDITOR_THEME_BACKGROUND:
			return Sys_EditorThemeColor( win_editorBackground, RGB( 32, 32, 32 ) );
		case EDITOR_THEME_CONTROL:
			return Sys_EditorThemeColor( win_editorControl, RGB( 48, 48, 48 ) );
		case EDITOR_THEME_FIELD:
			return Sys_EditorThemeColor( win_editorField, RGB( 24, 24, 24 ) );
		case EDITOR_THEME_TEXT:
			return Sys_EditorThemeColor( win_editorText, RGB( 230, 230, 230 ) );
		case EDITOR_THEME_SELECTION:
			return Sys_EditorThemeColor( win_editorSelection, RGB( 10, 100, 173 ) );
		case EDITOR_THEME_BORDER:
			return Sys_EditorThemeColor( win_editorBorder, RGB( 85, 85, 85 ) );
	}
	return RGB( 0, 0, 0 );
}

HBRUSH Sys_GetEditorThemeBrush( editorThemeColor_t role ) {
	const COLORREF color = Sys_GetEditorThemeColor( role );
	switch ( role ) {
		case EDITOR_THEME_BACKGROUND:
			return Sys_EditorThemeBrush( win32EditorBackgroundBrush, win32EditorBackgroundBrushColor, color );
		case EDITOR_THEME_CONTROL:
			return Sys_EditorThemeBrush( win32EditorControlBrush, win32EditorControlBrushColor, color );
		case EDITOR_THEME_FIELD:
			return Sys_EditorThemeBrush( win32EditorFieldBrush, win32EditorFieldBrushColor, color );
		case EDITOR_THEME_SELECTION:
			return Sys_EditorThemeBrush( win32EditorSelectionBrush, win32EditorSelectionBrushColor, color );
		case EDITOR_THEME_BORDER:
			return Sys_EditorThemeBrush( win32EditorBorderBrush, win32EditorBorderBrushColor, color );
		default:
			return Sys_EditorThemeBrush( win32EditorControlBrush, win32EditorControlBrushColor, color );
	}
}

static void Sys_DrawEditorCaptionGradient( HDC hDC, const RECT &rect, COLORREF start, COLORREF end ) {
	if ( rect.right <= rect.left || rect.bottom <= rect.top ) {
		return;
	}

	if ( win32GradientFill ) {
		TRIVERTEX vertices[2];
		vertices[0].x = rect.left;
		vertices[0].y = rect.top;
		vertices[0].Red = GetRValue( start ) << 8;
		vertices[0].Green = GetGValue( start ) << 8;
		vertices[0].Blue = GetBValue( start ) << 8;
		vertices[0].Alpha = 0;
		vertices[1].x = rect.right;
		vertices[1].y = rect.bottom;
		vertices[1].Red = GetRValue( end ) << 8;
		vertices[1].Green = GetGValue( end ) << 8;
		vertices[1].Blue = GetBValue( end ) << 8;
		vertices[1].Alpha = 0;
		GRADIENT_RECT gradient = { 0, 1 };
		win32GradientFill( hDC, vertices, 2, &gradient, 1, GRADIENT_FILL_RECT_H );
	} else {
		HBRUSH brush = CreateSolidBrush( start );
		FillRect( hDC, &rect, brush );
		DeleteObject( brush );
	}
}

static UINT Sys_GetEditorPressedCaptionButton( HWND hWnd ) {
	const UINT_PTR storedButton = reinterpret_cast<UINT_PTR>( GetPropA( hWnd, win32EditorCaptionPressed ) );
	return storedButton ? static_cast<UINT>( storedButton - 1 ) : HTNOWHERE;
}

static bool Sys_IsEditorCaptionButton( UINT hitCode ) {
	return hitCode == HTCLOSE || hitCode == HTMINBUTTON || hitCode == HTMAXBUTTON;
}

static void Sys_DrawEditorCaptionButton( HDC hDC, const RECT &rect, UINT hitCode, bool restore, bool pressed ) {
	FillRect( hDC, &rect, Sys_GetEditorThemeBrush( pressed ? EDITOR_THEME_FIELD : EDITOR_THEME_CONTROL ) );

	const COLORREF highlightColor = RGB( 104, 104, 104 );
	const COLORREF shadowColor = RGB( 14, 14, 14 );
	HPEN highlightPen = CreatePen( PS_SOLID, 1, highlightColor );
	HPEN shadowPen = CreatePen( PS_SOLID, 1, shadowColor );
	HPEN oldPen = reinterpret_cast<HPEN>( SelectObject( hDC, pressed ? shadowPen : highlightPen ) );
	MoveToEx( hDC, rect.left, rect.bottom - 1, NULL );
	LineTo( hDC, rect.left, rect.top );
	LineTo( hDC, rect.right - 1, rect.top );
	SelectObject( hDC, pressed ? highlightPen : shadowPen );
	LineTo( hDC, rect.right - 1, rect.bottom - 1 );
	LineTo( hDC, rect.left, rect.bottom - 1 );

	const int pressedOffset = pressed ? 1 : 0;
	const int centerX = ( rect.left + rect.right ) / 2 + pressedOffset;
	const int centerY = ( rect.top + rect.bottom ) / 2 + pressedOffset;
	const int glyphHalfWidth = max( 4, min( 7, ( rect.right - rect.left ) / 5 ) );
	const int glyphHalfHeight = max( 3, min( 6, ( rect.bottom - rect.top ) / 5 ) );
	HPEN glyphPen = CreatePen( PS_SOLID, 2, Sys_GetEditorThemeColor( EDITOR_THEME_TEXT ) );
	SelectObject( hDC, glyphPen );

	if ( hitCode == HTCLOSE ) {
		MoveToEx( hDC, centerX - glyphHalfWidth, centerY - glyphHalfHeight, NULL );
		LineTo( hDC, centerX + glyphHalfWidth + 1, centerY + glyphHalfHeight + 1 );
		MoveToEx( hDC, centerX + glyphHalfWidth, centerY - glyphHalfHeight, NULL );
		LineTo( hDC, centerX - glyphHalfWidth - 1, centerY + glyphHalfHeight + 1 );
	} else if ( hitCode == HTMINBUTTON ) {
		MoveToEx( hDC, centerX - glyphHalfWidth, centerY + glyphHalfHeight, NULL );
		LineTo( hDC, centerX + glyphHalfWidth + 1, centerY + glyphHalfHeight );
	} else if ( restore ) {
		Rectangle( hDC, centerX - glyphHalfWidth + 3, centerY - glyphHalfHeight - 2,
			centerX + glyphHalfWidth + 2, centerY + glyphHalfHeight - 1 );
		Rectangle( hDC, centerX - glyphHalfWidth - 2, centerY - glyphHalfHeight + 2,
			centerX + glyphHalfWidth - 2, centerY + glyphHalfHeight + 3 );
	} else {
		Rectangle( hDC, centerX - glyphHalfWidth, centerY - glyphHalfHeight,
			centerX + glyphHalfWidth + 1, centerY + glyphHalfHeight + 1 );
	}

	SelectObject( hDC, oldPen );
	DeleteObject( glyphPen );
	DeleteObject( shadowPen );
	DeleteObject( highlightPen );
}

static void Sys_DrawEditorCaption( HWND hWnd, bool active ) {
	const LONG_PTR style = GetWindowLongPtr( hWnd, GWL_STYLE );
	if ( !( style & WS_CAPTION ) || IsIconic( hWnd ) ) {
		return;
	}

	RECT windowRect;
	if ( !GetWindowRect( hWnd, &windowRect ) ) {
		return;
	}
	OffsetRect( &windowRect, -windowRect.left, -windowRect.top );

	const int frameX = GetSystemMetrics( SM_CXFRAME ) + GetSystemMetrics( SM_CXPADDEDBORDER );
	const int frameY = GetSystemMetrics( SM_CYFRAME ) + GetSystemMetrics( SM_CXPADDEDBORDER );
	const int captionHeight = max( GetSystemMetrics( SM_CYCAPTION ),
		Sys_EditorThemeMetric( win_editorTitleHeight, 38, 20, 64 ) );
	RECT captionRect = { frameX, frameY, windowRect.right - frameX, frameY + captionHeight };

	HDC hDC = GetWindowDC( hWnd );
	if ( !hDC ) {
		return;
	}

	// The stock themed non-client renderer leaves a bright/blue resize frame
	// around Radiant's captioned child views. Paint the complete frame before
	// drawing the classic caption so every edge uses the editor palette.
	const HBRUSH frameBrush = Sys_GetEditorThemeBrush( EDITOR_THEME_BORDER );
	RECT frameRect = { 0, 0, windowRect.right, frameY };
	FillRect( hDC, &frameRect, frameBrush );
	frameRect.top = windowRect.bottom - frameY;
	frameRect.bottom = windowRect.bottom;
	FillRect( hDC, &frameRect, frameBrush );
	frameRect.left = 0;
	frameRect.top = frameY;
	frameRect.right = frameX;
	frameRect.bottom = windowRect.bottom - frameY;
	FillRect( hDC, &frameRect, frameBrush );
	frameRect.left = windowRect.right - frameX;
	frameRect.right = windowRect.right;
	FillRect( hDC, &frameRect, frameBrush );

	// GetMenu returns the control ID in the hMenu slot for WS_CHILD windows,
	// so it cannot identify the frame by itself. Only the top-level Radiant
	// frame owns a real menu; CAM, Z and XY panes must retain the blue caption.
	const bool imguiMainFrame = GetPropA( hWnd, "DarklightEditorMainFrame" ) != NULL;
	const bool mainFrame = !( style & WS_CHILD ) && ( GetMenu( hWnd ) != NULL || imguiMainFrame );
	const COLORREF gradientStart = mainFrame
		? ( imguiMainFrame
			? RGB( 0, 0, 0 )
			: active
			? Sys_EditorThemeColor( win_editorMainTitleStart, RGB( 72, 72, 72 ) )
			: Sys_EditorThemeColor( win_editorInactiveTitleStart, RGB( 64, 64, 64 ) ) )
		: Sys_EditorThemeColor( win_editorTitleStart, RGB( 10, 36, 106 ) );
	const COLORREF gradientEnd = mainFrame
		? ( imguiMainFrame
			? RGB( 0, 0, 0 )
			: active
			? Sys_EditorThemeColor( win_editorMainTitleEnd, RGB( 24, 24, 24 ) )
			: Sys_EditorThemeColor( win_editorInactiveTitleEnd, RGB( 32, 32, 32 ) ) )
		: Sys_EditorThemeColor( win_editorTitleEnd, RGB( 166, 202, 240 ) );
	Sys_DrawEditorCaptionGradient( hDC, captionRect, gradientStart, gradientEnd );
	if ( mainFrame ) {
		MENUBARINFO menuInfo;
		memset( &menuInfo, 0, sizeof( menuInfo ) );
		menuInfo.cbSize = sizeof( menuInfo );
		RECT screenWindowRect;
		if ( GetWindowRect( hWnd, &screenWindowRect ) && GetMenuBarInfo( hWnd, OBJID_MENU, 0, &menuInfo ) ) {
			const int menuBottom = menuInfo.rcBar.bottom - screenWindowRect.top;
			RECT menuSeam = { frameX, menuBottom - 1, windowRect.right - frameX, menuBottom + 3 };
			FillRect( hDC, &menuSeam, Sys_GetEditorThemeBrush( EDITOR_THEME_CONTROL ) );
		}
	}

	RECT textRect = captionRect;
	const int buttonWidth = GetSystemMetrics( SM_CXSIZE );
	const int buttonHeight = min( captionRect.bottom - captionRect.top, GetSystemMetrics( SM_CYSIZE ) );
	RECT buttonRect = { captionRect.right - buttonWidth, captionRect.top, captionRect.right, captionRect.top + buttonHeight };
	const UINT pressedButton = Sys_GetEditorPressedCaptionButton( hWnd );
	const bool pressedInside = GetPropA( hWnd, win32EditorCaptionPressedInside ) != NULL;

	if ( style & WS_SYSMENU ) {
		Sys_DrawEditorCaptionButton( hDC, buttonRect, HTCLOSE, false, pressedInside && pressedButton == HTCLOSE );
		textRect.right = buttonRect.left;
		OffsetRect( &buttonRect, -buttonWidth, 0 );
		if ( style & WS_MAXIMIZEBOX ) {
			Sys_DrawEditorCaptionButton( hDC, buttonRect, HTMAXBUTTON, IsZoomed( hWnd ) != FALSE,
				pressedInside && pressedButton == HTMAXBUTTON );
			textRect.right = buttonRect.left;
			OffsetRect( &buttonRect, -buttonWidth, 0 );
		}
		if ( style & WS_MINIMIZEBOX ) {
			Sys_DrawEditorCaptionButton( hDC, buttonRect, HTMINBUTTON, false,
				pressedInside && pressedButton == HTMINBUTTON );
			textRect.right = buttonRect.left;
		}
	}

	HICON icon = reinterpret_cast<HICON>( SendMessage( hWnd, WM_GETICON, ICON_SMALL2, 0 ) );
	if ( !icon ) {
		icon = reinterpret_cast<HICON>( GetClassLongPtr( hWnd, GCLP_HICONSM ) );
	}
	const int iconSize = min( GetSystemMetrics( SM_CXSMICON ), captionRect.bottom - captionRect.top - 4 );
	if ( icon && iconSize > 0 ) {
		DrawIconEx( hDC, captionRect.left + 2, captionRect.top + ( captionHeight - iconSize ) / 2, icon, iconSize, iconSize, 0, NULL, DI_NORMAL );
		textRect.left += iconSize + 5;
	} else {
		textRect.left += 4;
	}
	textRect.right -= 3;

	char title[1024];
	GetWindowTextA( hWnd, title, sizeof( title ) );
	const int captionFontSize = Sys_EditorThemeMetric( win_editorTitleFontSize, 17, 10, 32 );
	if ( !win32EditorCaptionFont || win32EditorCaptionFontSize != captionFontSize ) {
		if ( win32EditorCaptionFont ) {
			DeleteObject( win32EditorCaptionFont );
		}
		win32EditorCaptionFont = CreateFontA( -captionFontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "MS Sans Serif" );
		win32EditorCaptionFontSize = captionFontSize;
	}
	HFONT oldFont = reinterpret_cast<HFONT>( SelectObject( hDC, win32EditorCaptionFont ) );
	SetBkMode( hDC, TRANSPARENT );
	SetTextColor( hDC, Sys_EditorThemeColor( win_editorTitleText, RGB( 255, 255, 255 ) ) );
	DrawTextA( hDC, title, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX );
	SelectObject( hDC, oldFont );
	ReleaseDC( hWnd, hDC );
}

static bool Sys_IsEditorStatusBar( HWND hWnd ) {
	char windowClass[64];
	GetClassNameA( hWnd, windowClass, sizeof( windowClass ) );
	return !idStr::Icmp( windowClass, STATUSCLASSNAMEA );
}

static bool Sys_IsEditorWindowClass( HWND hWnd, const char *expectedClass ) {
	char windowClass[64];
	GetClassNameA( hWnd, windowClass, sizeof( windowClass ) );
	return !idStr::Icmp( windowClass, expectedClass );
}

static void Sys_DrawEditorStatusBar( HWND hWnd, HDC hDC ) {
	RECT clientRect;
	GetClientRect( hWnd, &clientRect );
	FillRect( hDC, &clientRect, Sys_GetEditorThemeBrush( EDITOR_THEME_CONTROL ) );

	const COLORREF borderColor = Sys_GetEditorThemeColor( EDITOR_THEME_BORDER );
	HPEN borderPen = CreatePen( PS_SOLID, 1, borderColor );
	HPEN oldPen = reinterpret_cast<HPEN>( SelectObject( hDC, borderPen ) );
	MoveToEx( hDC, clientRect.left, clientRect.top, NULL );
	LineTo( hDC, clientRect.right, clientRect.top );

	HFONT font = reinterpret_cast<HFONT>( SendMessage( hWnd, WM_GETFONT, 0, 0 ) );
	HFONT oldFont = font ? reinterpret_cast<HFONT>( SelectObject( hDC, font ) ) : NULL;
	SetBkMode( hDC, TRANSPARENT );
	SetTextColor( hDC, Sys_GetEditorThemeColor( EDITOR_THEME_TEXT ) );

	const int partCount = static_cast<int>( SendMessage( hWnd, SB_GETPARTS, 0, 0 ) );
	for ( int i = 0; i < partCount; i++ ) {
		RECT partRect;
		if ( !SendMessage( hWnd, SB_GETRECT, i, reinterpret_cast<LPARAM>( &partRect ) ) ) {
			continue;
		}
		if ( i > 0 ) {
			MoveToEx( hDC, partRect.left, partRect.top + 2, NULL );
			LineTo( hDC, partRect.left, partRect.bottom - 2 );
		}

		char text[1024];
		text[0] = '\0';
		SendMessageA( hWnd, SB_GETTEXTA, i, reinterpret_cast<LPARAM>( text ) );
		partRect.left += 4;
		partRect.right -= 3;
		DrawTextA( hDC, text, -1, &partRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX );
	}

	if ( oldFont ) {
		SelectObject( hDC, oldFont );
	}
	SelectObject( hDC, oldPen );
	DeleteObject( borderPen );

	if ( GetWindowLongPtr( hWnd, GWL_STYLE ) & SBARS_SIZEGRIP ) {
		const int right = clientRect.right - 3;
		const int bottom = clientRect.bottom - 3;
		HPEN lightPen = CreatePen( PS_SOLID, 1, Sys_GetEditorThemeColor( EDITOR_THEME_TEXT ) );
		HPEN darkPen = CreatePen( PS_SOLID, 1, borderColor );
		for ( int offset = 0; offset < 9; offset += 4 ) {
			SelectObject( hDC, darkPen );
			MoveToEx( hDC, right - offset, bottom, NULL );
			LineTo( hDC, right, bottom - offset );
			SelectObject( hDC, lightPen );
			MoveToEx( hDC, right - offset - 1, bottom, NULL );
			LineTo( hDC, right, bottom - offset - 1 );
		}
		SelectObject( hDC, oldPen );
		DeleteObject( lightPen );
		DeleteObject( darkPen );
	}
}

static void Sys_DrawEditorComboBox( HWND hWnd, HDC hDC ) {
	RECT clientRect;
	GetClientRect( hWnd, &clientRect );
	FillRect( hDC, &clientRect, Sys_GetEditorThemeBrush( EDITOR_THEME_FIELD ) );

	const bool enabled = IsWindowEnabled( hWnd ) != FALSE;
	const LONG_PTR style = GetWindowLongPtr( hWnd, GWL_STYLE );
	const int comboType = style & CBS_DROPDOWNLIST;
	RECT arrowRect = clientRect;
	if ( comboType != CBS_SIMPLE ) {
		const int arrowWidth = min( clientRect.right - clientRect.left - 2, max( 20, GetSystemMetrics( SM_CXVSCROLL ) ) );
		arrowRect.left = arrowRect.right - arrowWidth;
		InflateRect( &arrowRect, -1, -1 );
		FillRect( hDC, &arrowRect, Sys_GetEditorThemeBrush( EDITOR_THEME_CONTROL ) );
	}

	HBRUSH borderBrush = CreateSolidBrush( Sys_GetEditorThemeColor( EDITOR_THEME_BORDER ) );
	FrameRect( hDC, &clientRect, borderBrush );
	if ( comboType != CBS_SIMPLE ) {
		FrameRect( hDC, &arrowRect, borderBrush );
	}
	DeleteObject( borderBrush );

	char text[1024];
	GetWindowTextA( hWnd, text, sizeof( text ) );
	RECT textRect = clientRect;
	textRect.left += 5;
	textRect.right = comboType == CBS_SIMPLE ? textRect.right - 4 : arrowRect.left - 4;
	HFONT font = reinterpret_cast<HFONT>( SendMessage( hWnd, WM_GETFONT, 0, 0 ) );
	HFONT oldFont = font ? reinterpret_cast<HFONT>( SelectObject( hDC, font ) ) : NULL;
	SetBkMode( hDC, TRANSPARENT );
	SetTextColor( hDC, enabled ? Sys_GetEditorThemeColor( EDITOR_THEME_TEXT ) : RGB( 128, 128, 128 ) );
	DrawTextA( hDC, text, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX );

	if ( comboType != CBS_SIMPLE ) {
		const int centerX = ( arrowRect.left + arrowRect.right ) / 2;
		const int centerY = ( arrowRect.top + arrowRect.bottom ) / 2 + 1;
		POINT arrow[3] = {
			{ centerX - 4, centerY - 2 },
			{ centerX + 4, centerY - 2 },
			{ centerX, centerY + 2 }
		};
		HBRUSH arrowBrush = CreateSolidBrush( enabled ? Sys_GetEditorThemeColor( EDITOR_THEME_TEXT ) : RGB( 128, 128, 128 ) );
		HPEN arrowPen = CreatePen( PS_SOLID, 1, enabled ? Sys_GetEditorThemeColor( EDITOR_THEME_TEXT ) : RGB( 128, 128, 128 ) );
		HBRUSH oldBrush = reinterpret_cast<HBRUSH>( SelectObject( hDC, arrowBrush ) );
		HPEN oldPen = reinterpret_cast<HPEN>( SelectObject( hDC, arrowPen ) );
		Polygon( hDC, arrow, 3 );
		SelectObject( hDC, oldPen );
		SelectObject( hDC, oldBrush );
		DeleteObject( arrowPen );
		DeleteObject( arrowBrush );
	}
	if ( oldFont ) {
		SelectObject( hDC, oldFont );
	}
}

static void Sys_DrawEditorTabControl( HWND hWnd, HDC hDC ) {
	RECT clientRect;
	GetClientRect( hWnd, &clientRect );
	FillRect( hDC, &clientRect, Sys_GetEditorThemeBrush( EDITOR_THEME_BACKGROUND ) );

	HFONT font = reinterpret_cast<HFONT>( SendMessage( hWnd, WM_GETFONT, 0, 0 ) );
	HFONT oldFont = font ? reinterpret_cast<HFONT>( SelectObject( hDC, font ) ) : NULL;
	SetBkMode( hDC, TRANSPARENT );
	SetTextColor( hDC, Sys_GetEditorThemeColor( EDITOR_THEME_TEXT ) );
	const int itemCount = static_cast<int>( SendMessage( hWnd, TCM_GETITEMCOUNT, 0, 0 ) );
	const int selectedIndex = static_cast<int>( SendMessage( hWnd, TCM_GETCURSEL, 0, 0 ) );

	for ( int pass = 0; pass < 2; pass++ ) {
		for ( int itemIndex = 0; itemIndex < itemCount; itemIndex++ ) {
			const bool selected = itemIndex == selectedIndex;
			if ( selected != ( pass == 1 ) ) {
				continue;
			}
			RECT itemRect;
			if ( !SendMessage( hWnd, TCM_GETITEMRECT, itemIndex, reinterpret_cast<LPARAM>( &itemRect ) ) ) {
				continue;
			}
			FillRect( hDC, &itemRect,
				Sys_GetEditorThemeBrush( selected ? EDITOR_THEME_CONTROL : EDITOR_THEME_FIELD ) );
			HBRUSH borderBrush = CreateSolidBrush( Sys_GetEditorThemeColor( EDITOR_THEME_BORDER ) );
			FrameRect( hDC, &itemRect, borderBrush );
			DeleteObject( borderBrush );

			char text[256];
			text[0] = '\0';
			TCITEMA item;
			memset( &item, 0, sizeof( item ) );
			item.mask = TCIF_TEXT;
			item.pszText = text;
			item.cchTextMax = sizeof( text );
			SendMessageA( hWnd, TCM_GETITEMA, itemIndex, reinterpret_cast<LPARAM>( &item ) );
			DrawTextA( hDC, text, -1, &itemRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX );
		}
	}
	if ( oldFont ) {
		SelectObject( hDC, oldFont );
	}
}

static void Sys_DrawEditorTrackBar( HWND hWnd, HDC hDC ) {
	RECT clientRect;
	GetClientRect( hWnd, &clientRect );
	FillRect( hDC, &clientRect, Sys_GetEditorThemeBrush( EDITOR_THEME_CONTROL ) );

	RECT channelRect;
	if ( SendMessage( hWnd, TBM_GETCHANNELRECT, 0, reinterpret_cast<LPARAM>( &channelRect ) ) ) {
		FillRect( hDC, &channelRect, Sys_GetEditorThemeBrush( EDITOR_THEME_FIELD ) );
		HBRUSH borderBrush = CreateSolidBrush( Sys_GetEditorThemeColor( EDITOR_THEME_BORDER ) );
		FrameRect( hDC, &channelRect, borderBrush );
		DeleteObject( borderBrush );
	}

	RECT thumbRect;
	if ( SendMessage( hWnd, TBM_GETTHUMBRECT, 0, reinterpret_cast<LPARAM>( &thumbRect ) ) ) {
		FillRect( hDC, &thumbRect, Sys_GetEditorThemeBrush( EDITOR_THEME_SELECTION ) );
		HBRUSH borderBrush = CreateSolidBrush( Sys_GetEditorThemeColor( EDITOR_THEME_BORDER ) );
		FrameRect( hDC, &thumbRect, borderBrush );
		DeleteObject( borderBrush );
	}
}

static LRESULT Sys_DrawEditorToolbar( LPNMTBCUSTOMDRAW customDraw ) {
	if ( customDraw->nmcd.dwDrawStage == CDDS_PREPAINT ) {
		// MFC and the themed toolbar control both add a pale top edge. Paint
		// the complete toolbar edge from a window DC before its client area.
		HWND toolBar = customDraw->nmcd.hdr.hwndFrom;
		HDC windowDC = GetWindowDC( toolBar );
		if ( windowDC ) {
			RECT toolBarRect;
			GetWindowRect( toolBar, &toolBarRect );
			OffsetRect( &toolBarRect, -toolBarRect.left, -toolBarRect.top );
			RECT topEdge = { 0, 0, toolBarRect.right, min( 3, toolBarRect.bottom ) };
			FillRect( windowDC, &topEdge, Sys_GetEditorThemeBrush( EDITOR_THEME_CONTROL ) );
			RECT bottomEdge = { 0, max( 0, toolBarRect.bottom - 3 ), toolBarRect.right, toolBarRect.bottom };
			FillRect( windowDC, &bottomEdge, Sys_GetEditorThemeBrush( EDITOR_THEME_CONTROL ) );
			ReleaseDC( toolBar, windowDC );
		}
		FillRect( customDraw->nmcd.hdc, &customDraw->nmcd.rc, Sys_GetEditorThemeBrush( EDITOR_THEME_CONTROL ) );
		return CDRF_NOTIFYITEMDRAW;
	}
	if ( customDraw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT ) {
		return TBCDRF_USECDCOLORS | TBCDRF_HILITEHOTTRACK;
	}
	return CDRF_DODEFAULT;
}

static void Sys_ApplyEditorMenu( HMENU menu, bool menuBar ) {
	if ( !menu ) {
		return;
	}

	MENUINFO menuInfo;
	memset( &menuInfo, 0, sizeof( menuInfo ) );
	menuInfo.cbSize = sizeof( menuInfo );
	menuInfo.fMask = MIM_BACKGROUND;
	menuInfo.hbrBack = Sys_GetEditorThemeBrush( EDITOR_THEME_CONTROL );
	SetMenuInfo( menu, &menuInfo );

	const int itemCount = GetMenuItemCount( menu );
	for ( int i = 0; i < itemCount; i++ ) {
		MENUITEMINFOA itemInfo;
		memset( &itemInfo, 0, sizeof( itemInfo ) );
		itemInfo.cbSize = sizeof( itemInfo );
		itemInfo.fMask = MIIM_FTYPE | MIIM_DATA | MIIM_SUBMENU | MIIM_ID;
		if ( !GetMenuItemInfoA( menu, i, TRUE, &itemInfo ) ) {
			continue;
		}

		win32EditorMenuItem_t *themeItem = reinterpret_cast<win32EditorMenuItem_t *>( itemInfo.dwItemData );
		if ( !( itemInfo.fType & MFT_OWNERDRAW ) || !themeItem || themeItem->magic != win32EditorMenuItemMagic ) {
			char itemText[512];
			itemText[0] = '\0';
			GetMenuStringA( menu, i, itemText, sizeof( itemText ), MF_BYPOSITION );

			themeItem = new win32EditorMenuItem_t;
			themeItem->magic = win32EditorMenuItemMagic;
			themeItem->menuBar = menuBar;
			themeItem->separator = ( itemInfo.fType & MFT_SEPARATOR ) != 0;
			themeItem->hasSubmenu = itemInfo.hSubMenu != NULL;
			idStr::Copynz( themeItem->text, itemText, sizeof( themeItem->text ) );

			MENUITEMINFOA ownerDrawInfo;
			memset( &ownerDrawInfo, 0, sizeof( ownerDrawInfo ) );
			ownerDrawInfo.cbSize = sizeof( ownerDrawInfo );
			ownerDrawInfo.fMask = MIIM_FTYPE | MIIM_DATA;
			ownerDrawInfo.fType = itemInfo.fType | MFT_OWNERDRAW;
			ownerDrawInfo.dwItemData = reinterpret_cast<ULONG_PTR>( themeItem );
			SetMenuItemInfoA( menu, i, TRUE, &ownerDrawInfo );
		}

		if ( itemInfo.hSubMenu ) {
			Sys_ApplyEditorMenu( itemInfo.hSubMenu, false );
		}
	}
}

static bool Sys_MeasureEditorMenuItem( MEASUREITEMSTRUCT *measureItem ) {
	if ( !measureItem || measureItem->CtlType != ODT_MENU ) {
		return false;
	}
	win32EditorMenuItem_t *themeItem = reinterpret_cast<win32EditorMenuItem_t *>( measureItem->itemData );
	if ( !themeItem || themeItem->magic != win32EditorMenuItemMagic ) {
		return false;
	}

	if ( themeItem->separator ) {
		measureItem->itemWidth = 8;
		measureItem->itemHeight = 7;
		return true;
	}

	HDC hDC = GetDC( NULL );
	const int menuFontSize = Sys_EditorThemeMetric( win_editorMenuFontSize, 20, 10, 32 );
	if ( !win32EditorMenuFont || win32EditorMenuFontSize != menuFontSize ) {
		if ( win32EditorMenuFont ) {
			DeleteObject( win32EditorMenuFont );
		}
		win32EditorMenuFont = CreateFontA( -menuFontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "MS Sans Serif" );
		win32EditorMenuFontSize = menuFontSize;
	}
	HFONT oldFont = reinterpret_cast<HFONT>( SelectObject( hDC, win32EditorMenuFont ) );
	SIZE textSize = { 0, 0 };
	GetTextExtentPoint32A( hDC, themeItem->text, strlen( themeItem->text ), &textSize );
	SelectObject( hDC, oldFont );
	ReleaseDC( NULL, hDC );

	measureItem->itemWidth = textSize.cx + ( themeItem->menuBar ? 14 : 42 );
	measureItem->itemHeight = max( Sys_EditorThemeMetric( win_editorMenuHeight, 40, 20, 64 ), textSize.cy + 8 );
	return true;
}

static bool Sys_DrawEditorMenuItem( DRAWITEMSTRUCT *drawItem ) {
	if ( !drawItem || drawItem->CtlType != ODT_MENU ) {
		return false;
	}
	win32EditorMenuItem_t *themeItem = reinterpret_cast<win32EditorMenuItem_t *>( drawItem->itemData );
	if ( !themeItem || themeItem->magic != win32EditorMenuItemMagic ) {
		return false;
	}

	const bool selected = ( drawItem->itemState & ODS_SELECTED ) != 0;
	const bool disabled = ( drawItem->itemState & ( ODS_DISABLED | ODS_GRAYED ) ) != 0;
	FillRect( drawItem->hDC, &drawItem->rcItem, Sys_GetEditorThemeBrush( selected ? EDITOR_THEME_SELECTION : EDITOR_THEME_CONTROL ) );

	if ( themeItem->separator ) {
		const int middle = ( drawItem->rcItem.top + drawItem->rcItem.bottom ) / 2;
		HPEN separator = CreatePen( PS_SOLID, 1, Sys_GetEditorThemeColor( EDITOR_THEME_BORDER ) );
		HPEN oldPen = reinterpret_cast<HPEN>( SelectObject( drawItem->hDC, separator ) );
		MoveToEx( drawItem->hDC, drawItem->rcItem.left + 6, middle, NULL );
		LineTo( drawItem->hDC, drawItem->rcItem.right - 6, middle );
		SelectObject( drawItem->hDC, oldPen );
		DeleteObject( separator );
		return true;
	}

	RECT textRect = drawItem->rcItem;
	textRect.left += themeItem->menuBar ? 7 : 24;
	textRect.right -= themeItem->menuBar ? 7 : 20;
	SetBkMode( drawItem->hDC, TRANSPARENT );
	SetTextColor( drawItem->hDC, disabled ? RGB( 128, 128, 128 ) : Sys_GetEditorThemeColor( EDITOR_THEME_TEXT ) );
	HFONT oldFont = reinterpret_cast<HFONT>( SelectObject( drawItem->hDC, win32EditorMenuFont ) );

	char label[512];
	idStr::Copynz( label, themeItem->text, sizeof( label ) );
	char *shortcut = strchr( label, '\t' );
	if ( shortcut ) {
		*shortcut++ = '\0';
	}
	DrawTextA( drawItem->hDC, label, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE );
	if ( shortcut ) {
		DrawTextA( drawItem->hDC, shortcut, -1, &textRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE );
	}
	SelectObject( drawItem->hDC, oldFont );

	if ( drawItem->itemState & ODS_CHECKED ) {
		RECT checkRect = drawItem->rcItem;
		checkRect.left += 4;
		checkRect.right = checkRect.left + 14;
		checkRect.top += 3;
		checkRect.bottom -= 3;
		DrawFrameControl( drawItem->hDC, &checkRect, DFC_MENU, DFCS_MENUCHECK );
	}
	if ( themeItem->hasSubmenu && !themeItem->menuBar ) {
		RECT arrowRect = drawItem->rcItem;
		arrowRect.left = arrowRect.right - 16;
		DrawFrameControl( drawItem->hDC, &arrowRect, DFC_MENU, DFCS_MENUARROWRIGHT );
	}
	return true;
}

static bool Sys_IsEditorWindowActive( HWND hWnd ) {
	HWND focus = GetFocus();
	return GetForegroundWindow() == hWnd || focus == hWnd || ( focus && IsChild( hWnd, focus ) );
}

static LRESULT CALLBACK Sys_EditorThemeWindowProcedure( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam ) {
	WNDPROC originalProcedure = reinterpret_cast<WNDPROC>( GetPropA( hWnd, win32EditorThemeWindowProc ) );
	if ( !originalProcedure ) {
		return DefWindowProc( hWnd, message, wParam, lParam );
	}

	switch ( message ) {
		case WM_NCCALCSIZE: {
			LRESULT result = CallWindowProc( originalProcedure, hWnd, message, wParam, lParam );
			const LONG_PTR style = GetWindowLongPtr( hWnd, GWL_STYLE );
			if ( style & WS_CAPTION ) {
				const int captionHeight = max( GetSystemMetrics( SM_CYCAPTION ),
					Sys_EditorThemeMetric( win_editorTitleHeight, 38, 20, 64 ) );
				const int extraHeight = captionHeight - GetSystemMetrics( SM_CYCAPTION );
				RECT *clientRect = wParam
					? &reinterpret_cast<NCCALCSIZE_PARAMS *>( lParam )->rgrc[0]
					: reinterpret_cast<RECT *>( lParam );
				if ( clientRect && extraHeight > 0 ) {
					clientRect->top = min( clientRect->bottom, clientRect->top + extraHeight );
				}
			}
			return result;
		}

		case WM_NCHITTEST: {
			LRESULT result = CallWindowProc( originalProcedure, hWnd, message, wParam, lParam );
			const LONG_PTR style = GetWindowLongPtr( hWnd, GWL_STYLE );
			if ( !( style & WS_CAPTION ) ) {
				return result;
			}
			RECT windowRect;
			GetWindowRect( hWnd, &windowRect );
			const int x = static_cast<short>( LOWORD( lParam ) ) - windowRect.left;
			const int y = static_cast<short>( HIWORD( lParam ) ) - windowRect.top;
			const int frameX = GetSystemMetrics( SM_CXFRAME ) + GetSystemMetrics( SM_CXPADDEDBORDER );
			const int frameY = GetSystemMetrics( SM_CYFRAME ) + GetSystemMetrics( SM_CXPADDEDBORDER );
			const int captionHeight = max( GetSystemMetrics( SM_CYCAPTION ),
				Sys_EditorThemeMetric( win_editorTitleHeight, 38, 20, 64 ) );
			if ( y < frameY || y >= frameY + captionHeight || x < frameX || x >= windowRect.right - windowRect.left - frameX ) {
				return result;
			}

			if ( style & WS_SYSMENU ) {
				const int buttonWidth = GetSystemMetrics( SM_CXSIZE );
				int buttonRight = windowRect.right - windowRect.left - frameX;
				if ( x >= buttonRight - buttonWidth ) {
					return HTCLOSE;
				}
				buttonRight -= buttonWidth;
				if ( style & WS_MAXIMIZEBOX ) {
					if ( x >= buttonRight - buttonWidth ) {
						return HTMAXBUTTON;
					}
					buttonRight -= buttonWidth;
				}
				if ( style & WS_MINIMIZEBOX ) {
					if ( x >= buttonRight - buttonWidth ) {
						return HTMINBUTTON;
					}
				}
				if ( x < frameX + captionHeight ) {
					return HTSYSMENU;
				}
			}
			return HTCAPTION;
		}

		case WM_ERASEBKGND: {
			RECT clientRect;
			GetClientRect( hWnd, &clientRect );
			FillRect( reinterpret_cast<HDC>( wParam ), &clientRect, Sys_GetEditorThemeBrush( EDITOR_THEME_BACKGROUND ) );
			return TRUE;
		}

		case WM_PAINT:
			if ( Sys_IsEditorWindowClass( hWnd, "ComboBox" ) ) {
				PAINTSTRUCT paint;
				HDC hDC = BeginPaint( hWnd, &paint );
				Sys_DrawEditorComboBox( hWnd, hDC );
				EndPaint( hWnd, &paint );
				return 0;
			} else if ( Sys_IsEditorWindowClass( hWnd, WC_TABCONTROLA ) ) {
				PAINTSTRUCT paint;
				HDC hDC = BeginPaint( hWnd, &paint );
				Sys_DrawEditorTabControl( hWnd, hDC );
				EndPaint( hWnd, &paint );
				return 0;
			} else if ( Sys_IsEditorWindowClass( hWnd, TRACKBAR_CLASSA ) ) {
				PAINTSTRUCT paint;
				HDC hDC = BeginPaint( hWnd, &paint );
				Sys_DrawEditorTrackBar( hWnd, hDC );
				EndPaint( hWnd, &paint );
				return 0;
			} else if ( Sys_IsEditorStatusBar( hWnd ) ) {
				PAINTSTRUCT paint;
				HDC hDC = BeginPaint( hWnd, &paint );
				Sys_DrawEditorStatusBar( hWnd, hDC );
				EndPaint( hWnd, &paint );
				return 0;
			}
			break;

		case WM_PRINT:
		case WM_PRINTCLIENT:
			if ( Sys_IsEditorWindowClass( hWnd, "ComboBox" ) ) {
				Sys_DrawEditorComboBox( hWnd, reinterpret_cast<HDC>( wParam ) );
				return 0;
			} else if ( Sys_IsEditorWindowClass( hWnd, WC_TABCONTROLA ) ) {
				Sys_DrawEditorTabControl( hWnd, reinterpret_cast<HDC>( wParam ) );
				return 0;
			} else if ( Sys_IsEditorWindowClass( hWnd, TRACKBAR_CLASSA ) ) {
				Sys_DrawEditorTrackBar( hWnd, reinterpret_cast<HDC>( wParam ) );
				return 0;
			} else if ( Sys_IsEditorStatusBar( hWnd ) ) {
				Sys_DrawEditorStatusBar( hWnd, reinterpret_cast<HDC>( wParam ) );
				return 0;
			}
			break;

		case WM_INITMENUPOPUP:
			Sys_ApplyEditorMenu( reinterpret_cast<HMENU>( wParam ), false );
			break;

		case WM_MEASUREITEM:
			if ( Sys_MeasureEditorMenuItem( reinterpret_cast<MEASUREITEMSTRUCT *>( lParam ) ) ) {
				return TRUE;
			}
			break;

		case WM_DRAWITEM:
			if ( Sys_DrawEditorMenuItem( reinterpret_cast<DRAWITEMSTRUCT *>( lParam ) ) ) {
				return TRUE;
			}
			break;

		case WM_NOTIFY: {
			NMHDR *notification = reinterpret_cast<NMHDR *>( lParam );
			if ( notification && notification->code == NM_CUSTOMDRAW ) {
				char controlClass[64];
				GetClassNameA( notification->hwndFrom, controlClass, sizeof( controlClass ) );
				if ( !idStr::Icmp( controlClass, TOOLBARCLASSNAMEA ) ) {
					LPNMTBCUSTOMDRAW customDraw = reinterpret_cast<LPNMTBCUSTOMDRAW>( lParam );
					customDraw->clrText = Sys_GetEditorThemeColor( EDITOR_THEME_TEXT );
					customDraw->clrTextHighlight = Sys_GetEditorThemeColor( EDITOR_THEME_TEXT );
					customDraw->clrMark = Sys_GetEditorThemeColor( EDITOR_THEME_SELECTION );
					customDraw->clrBtnFace = Sys_GetEditorThemeColor( EDITOR_THEME_CONTROL );
					customDraw->clrBtnHighlight = Sys_GetEditorThemeColor( EDITOR_THEME_BORDER );
					customDraw->clrHighlightHotTrack = Sys_GetEditorThemeColor( EDITOR_THEME_SELECTION );
					return Sys_DrawEditorToolbar( customDraw );
				}
			}
			break;
		}

		case WM_CTLCOLORDLG:
		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORBTN: {
			HDC hDC = reinterpret_cast<HDC>( wParam );
			const COLORREF face = Sys_EditorThemeColor( win_editorControl, RGB( 48, 48, 48 ) );
			SetTextColor( hDC, Sys_EditorThemeColor( win_editorText, RGB( 230, 230, 230 ) ) );
			SetBkColor( hDC, face );
			SetBkMode( hDC, TRANSPARENT );
			return reinterpret_cast<LRESULT>( Sys_EditorThemeBrush( win32EditorControlBrush, win32EditorControlBrushColor, face ) );
		}

		case WM_CTLCOLOREDIT:
		case WM_CTLCOLORLISTBOX: {
			HDC hDC = reinterpret_cast<HDC>( wParam );
			const COLORREF field = Sys_EditorThemeColor( win_editorField, RGB( 24, 24, 24 ) );
			SetTextColor( hDC, Sys_EditorThemeColor( win_editorText, RGB( 230, 230, 230 ) ) );
			SetBkColor( hDC, field );
			return reinterpret_cast<LRESULT>( Sys_EditorThemeBrush( win32EditorFieldBrush, win32EditorFieldBrushColor, field ) );
		}

		case WM_CTLCOLORSCROLLBAR: {
			const COLORREF background = Sys_EditorThemeColor( win_editorBackground, RGB( 32, 32, 32 ) );
			return reinterpret_cast<LRESULT>( Sys_EditorThemeBrush( win32EditorBackgroundBrush, win32EditorBackgroundBrushColor, background ) );
		}

		case WM_NCPAINT: {
			LRESULT result = CallWindowProc( originalProcedure, hWnd, message, wParam, lParam );
			Sys_DrawEditorCaption( hWnd, Sys_IsEditorWindowActive( hWnd ) );
			return result;
		}

		case WM_NCACTIVATE: {
			LRESULT result = CallWindowProc( originalProcedure, hWnd, message, wParam, lParam );
			Sys_DrawEditorCaption( hWnd, wParam != FALSE );
			return result;
		}

		case WM_NCLBUTTONDOWN:
		case WM_NCLBUTTONDBLCLK: {
			const UINT hitCode = static_cast<UINT>( wParam );
			if ( Sys_IsEditorCaptionButton( hitCode ) ) {
				SetPropA( hWnd, win32EditorCaptionPressed,
					reinterpret_cast<HANDLE>( static_cast<UINT_PTR>( hitCode + 1 ) ) );
				SetPropA( hWnd, win32EditorCaptionPressedInside, reinterpret_cast<HANDLE>( 1 ) );
				SetCapture( hWnd );
				Sys_DrawEditorCaption( hWnd, Sys_IsEditorWindowActive( hWnd ) );
				return 0;
			}
			LRESULT result = CallWindowProc( originalProcedure, hWnd, message, wParam, lParam );
			if ( IsWindow( hWnd ) ) {
				Sys_DrawEditorCaption( hWnd, Sys_IsEditorWindowActive( hWnd ) );
			}
			return result;
		}

		case WM_MOUSEMOVE: {
			const UINT pressedButton = Sys_GetEditorPressedCaptionButton( hWnd );
			if ( Sys_IsEditorCaptionButton( pressedButton ) ) {
				POINT cursor;
				GetCursorPos( &cursor );
				const LRESULT cursorHit = SendMessage( hWnd, WM_NCHITTEST, 0, MAKELPARAM( cursor.x, cursor.y ) );
				if ( static_cast<UINT>( cursorHit ) == pressedButton ) {
					SetPropA( hWnd, win32EditorCaptionPressedInside, reinterpret_cast<HANDLE>( 1 ) );
				} else {
					RemovePropA( hWnd, win32EditorCaptionPressedInside );
				}
				Sys_DrawEditorCaption( hWnd, Sys_IsEditorWindowActive( hWnd ) );
				return 0;
			}
			break;
		}

		case WM_LBUTTONUP: {
			const UINT pressedButton = Sys_GetEditorPressedCaptionButton( hWnd );
			if ( Sys_IsEditorCaptionButton( pressedButton ) ) {
				POINT cursor;
				GetCursorPos( &cursor );
				const LRESULT cursorHit = SendMessage( hWnd, WM_NCHITTEST, 0, MAKELPARAM( cursor.x, cursor.y ) );
				const bool activateButton = static_cast<UINT>( cursorHit ) == pressedButton;
				RemovePropA( hWnd, win32EditorCaptionPressed );
				RemovePropA( hWnd, win32EditorCaptionPressedInside );
				ReleaseCapture();
				Sys_DrawEditorCaption( hWnd, Sys_IsEditorWindowActive( hWnd ) );
				if ( activateButton ) {
					const WPARAM command = pressedButton == HTCLOSE ? SC_CLOSE :
						( pressedButton == HTMINBUTTON ? SC_MINIMIZE : ( IsZoomed( hWnd ) ? SC_RESTORE : SC_MAXIMIZE ) );
					PostMessage( hWnd, WM_SYSCOMMAND, command, MAKELPARAM( cursor.x, cursor.y ) );
				}
				return 0;
			}
			break;
		}

		case WM_CANCELMODE:
		case WM_CAPTURECHANGED:
			if ( Sys_IsEditorCaptionButton( Sys_GetEditorPressedCaptionButton( hWnd ) ) ) {
				RemovePropA( hWnd, win32EditorCaptionPressed );
				RemovePropA( hWnd, win32EditorCaptionPressedInside );
				Sys_DrawEditorCaption( hWnd, Sys_IsEditorWindowActive( hWnd ) );
			}
			break;

		case WM_NCMOUSEMOVE:
		case WM_NCMOUSEHOVER:
		case WM_NCMOUSELEAVE:
		case WM_NCLBUTTONUP:
		case WM_NCRBUTTONDOWN:
		case WM_NCRBUTTONUP: {
			// DefWindowProc paints the operating-system caption glyphs while a
			// non-client button is hot or pressed. Paint our full caption again
			// after it completes so native Vista-era button art cannot bleed
			// through the Win98-style dark buttons.
			LRESULT result = CallWindowProc( originalProcedure, hWnd, message, wParam, lParam );
			if ( IsWindow( hWnd ) ) {
				Sys_DrawEditorCaption( hWnd, Sys_IsEditorWindowActive( hWnd ) );
			}
			return result;
		}

		case WM_SETTEXT:
		case WM_SETICON: {
			LRESULT result = CallWindowProc( originalProcedure, hWnd, message, wParam, lParam );
			Sys_DrawEditorCaption( hWnd, Sys_IsEditorWindowActive( hWnd ) );
			return result;
		}

		case WM_NCDESTROY: {
			LRESULT result = CallWindowProc( originalProcedure, hWnd, message, wParam, lParam );
			RemovePropA( hWnd, win32EditorThemeWindowProc );
			RemovePropA( hWnd, win32EditorCaptionPressed );
			RemovePropA( hWnd, win32EditorCaptionPressedInside );
			return result;
		}
	}

	return CallWindowProc( originalProcedure, hWnd, message, wParam, lParam );
}

static void Sys_ApplyEditorWindowTheme( HWND hWnd ) {
	if ( !hWnd ) {
		return;
	}

	char windowClass[64];
	GetClassNameA( hWnd, windowClass, sizeof( windowClass ) );
	const bool customControl = !idStr::Icmp( windowClass, "ComboBox" ) ||
		!idStr::Icmp( windowClass, WC_TABCONTROLA ) || !idStr::Icmp( windowClass, TRACKBAR_CLASSA );

	if ( win32AllowDarkModeForWindow ) {
		win32AllowDarkModeForWindow( hWnd, TRUE );
	}
	if ( win32SetWindowTheme ) {
		win32SetWindowTheme( hWnd, customControl ? L"" : L"DarkMode_Explorer", NULL );
	}

	const LONG_PTR style = GetWindowLongPtr( hWnd, GWL_STYLE );
	bool installedWindowProcedure = false;
	if ( win32DwmSetWindowAttribute && ( style & WS_CAPTION ) && !( style & WS_CHILD ) ) {
		const BOOL darkMode = TRUE;
		const DWORD disableDwmCaption = 1;	// DWMNCRP_DISABLED
		const DWORD squareCorners = 1;		// DWMWCP_DONOTROUND
		const COLORREF border = Sys_EditorThemeColor( win_editorControl, RGB( 48, 48, 48 ) );
		win32DwmSetWindowAttribute( hWnd, 2, &disableDwmCaption, sizeof( disableDwmCaption ) );	// DWMWA_NCRENDERING_POLICY
		win32DwmSetWindowAttribute( hWnd, 20, &darkMode, sizeof( darkMode ) );			// DWMWA_USE_IMMERSIVE_DARK_MODE
		win32DwmSetWindowAttribute( hWnd, 33, &squareCorners, sizeof( squareCorners ) );		// DWMWA_WINDOW_CORNER_PREFERENCE
		win32DwmSetWindowAttribute( hWnd, 34, &border, sizeof( border ) );				// DWMWA_BORDER_COLOR
	}

	if ( !GetPropA( hWnd, win32EditorThemeWindowProc ) ) {
		if ( !( style & WS_CHILD ) || ( style & WS_CAPTION ) || !idStr::Icmp( windowClass, "#32770" ) ||
				!idStr::Icmp( windowClass, STATUSCLASSNAMEA ) || customControl || !idStr::Icmpn( windowClass, "Afx", 3 ) ) {
			WNDPROC originalProcedure = reinterpret_cast<WNDPROC>( GetWindowLongPtr( hWnd, GWLP_WNDPROC ) );
			if ( originalProcedure ) {
				SetPropA( hWnd, win32EditorThemeWindowProc, reinterpret_cast<HANDLE>( originalProcedure ) );
				SetWindowLongPtr( hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( Sys_EditorThemeWindowProcedure ) );
				installedWindowProcedure = true;
			}
		}
	}

	if ( !idStr::Icmp( windowClass, STATUSCLASSNAMEA ) ) {
		SendMessage( hWnd, SB_SETBKCOLOR, 0, Sys_GetEditorThemeColor( EDITOR_THEME_CONTROL ) );
		InvalidateRect( hWnd, NULL, TRUE );
	}
	if ( customControl ) {
		InvalidateRect( hWnd, NULL, TRUE );
	}
	if ( installedWindowProcedure && ( style & WS_CAPTION ) ) {
		SetWindowPos( hWnd, NULL, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED );
	}

	if ( installedWindowProcedure && !( style & WS_CHILD ) ) {
		HMENU menu = GetMenu( hWnd );
		if ( menu ) {
			Sys_ApplyEditorMenu( menu, true );
			SetMenu( hWnd, NULL );
			SetMenu( hWnd, menu );
			DrawMenuBar( hWnd );
		}
	}
}

static BOOL CALLBACK Sys_ApplyEditorChildTheme( HWND hWnd, LPARAM ) {
	Sys_ApplyEditorWindowTheme( hWnd );
	return TRUE;
}

static BOOL CALLBACK Sys_ApplyEditorTopLevelTheme( HWND hWnd, LPARAM ) {
	Sys_ApplyEditorWindowTheme( hWnd );
	EnumChildWindows( hWnd, Sys_ApplyEditorChildTheme, 0 );
	return TRUE;
}

static LRESULT CALLBACK Sys_EditorThemeHook( int code, WPARAM wParam, LPARAM lParam ) {
	if ( code == HCBT_CREATEWND ) {
		HWND hWnd = reinterpret_cast<HWND>( wParam );
		if ( win32AllowDarkModeForWindow ) {
			win32AllowDarkModeForWindow( hWnd, TRUE );
		}
		if ( win32SetWindowTheme ) {
			win32SetWindowTheme( hWnd, L"DarkMode_Explorer", NULL );
		}
	} else if ( code == HCBT_ACTIVATE ) {
		HWND hWnd = reinterpret_cast<HWND>( wParam );
		Sys_ApplyEditorWindowTheme( hWnd );
		EnumChildWindows( hWnd, Sys_ApplyEditorChildTheme, 0 );
		RedrawWindow( hWnd, NULL, NULL, RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN );
	}

	return CallNextHookEx( win32EditorThemeHook, code, wParam, lParam );
}

static LRESULT CALLBACK Sys_EditorThemeMessageHook( int code, WPARAM wParam, LPARAM lParam ) {
	if ( code >= 0 ) {
		CWPSTRUCT *message = reinterpret_cast<CWPSTRUCT *>( lParam );
		if ( message && message->message == WM_SHOWWINDOW && message->wParam ) {
			Sys_ApplyEditorWindowTheme( message->hwnd );
			EnumChildWindows( message->hwnd, Sys_ApplyEditorChildTheme, 0 );
		}
	}
	return CallNextHookEx( win32EditorThemeMessageHook, code, wParam, lParam );
}

static void Sys_EnableEditorTheme() {
	if ( win32EditorThemeEnabled || !win_editorDarkTheme.GetBool() ) {
		return;
	}
	win32EditorThemeEnabled = true;

	HMODULE themeLibrary = LoadLibraryA( "uxtheme.dll" );
	if ( themeLibrary ) {
		setThemeAppProperties_t setThemeAppProperties = reinterpret_cast<setThemeAppProperties_t>( GetProcAddress( themeLibrary, "SetThemeAppProperties" ) );
		setPreferredAppMode_t setPreferredAppMode = reinterpret_cast<setPreferredAppMode_t>( GetProcAddress( themeLibrary, MAKEINTRESOURCEA( 135 ) ) );
		flushMenuThemes_t flushMenuThemes = reinterpret_cast<flushMenuThemes_t>( GetProcAddress( themeLibrary, MAKEINTRESOURCEA( 136 ) ) );
		win32AllowDarkModeForWindow = reinterpret_cast<allowDarkModeForWindow_t>( GetProcAddress( themeLibrary, MAKEINTRESOURCEA( 133 ) ) );
		win32SetWindowTheme = reinterpret_cast<setWindowTheme_t>( GetProcAddress( themeLibrary, "SetWindowTheme" ) );
		if ( setThemeAppProperties ) {
			setThemeAppProperties( 3 );	// STAP_ALLOW_NONCLIENT | STAP_ALLOW_CONTROLS
		}
		if ( setPreferredAppMode ) {
			setPreferredAppMode( 2 );	// ForceDark
		}
		if ( flushMenuThemes ) {
			flushMenuThemes();
		}
	}

	HMODULE dwmLibrary = LoadLibraryA( "dwmapi.dll" );
	if ( dwmLibrary ) {
		win32DwmSetWindowAttribute = reinterpret_cast<dwmSetWindowAttribute_t>( GetProcAddress( dwmLibrary, "DwmSetWindowAttribute" ) );
	}
	HMODULE msimgLibrary = LoadLibraryA( "msimg32.dll" );
	if ( msimgLibrary ) {
		win32GradientFill = reinterpret_cast<gradientFill_t>( GetProcAddress( msimgLibrary, "GradientFill" ) );
	}

	EnumThreadWindows( GetCurrentThreadId(), Sys_ApplyEditorTopLevelTheme, 0 );
	win32EditorThemeHook = SetWindowsHookEx( WH_CBT, Sys_EditorThemeHook, NULL, GetCurrentThreadId() );
	win32EditorThemeMessageHook = SetWindowsHookEx( WH_CALLWNDPROC, Sys_EditorThemeMessageHook, NULL, GetCurrentThreadId() );
}
#endif

// not a hard limit, just what we keep track of for debugging
xthreadInfo *g_threads[MAX_THREADS];

int g_thread_count = 0;

static sysMemoryStats_t exeLaunchMemoryStats;

static	xthreadInfo	soundThreadInfo;
static	HANDLE		soundTimer;

/*
================
Sys_GetExeLaunchMemoryStatus
================
*/
void Sys_GetExeLaunchMemoryStatus( sysMemoryStats_t &stats ) {
	stats = exeLaunchMemoryStats;
}

/*
==================
Sys_Createthread
==================
*/
void Sys_CreateThread(  xthread_t function, void *parms, xthreadPriority priority, xthreadInfo &info, const char *name, xthreadInfo *threads[MAX_THREADS], int *thread_count ) {
	HANDLE temp = CreateThread(	NULL,	// LPSECURITY_ATTRIBUTES lpsa,
									0,		// DWORD cbStack,
									(LPTHREAD_START_ROUTINE)function,	// LPTHREAD_START_ROUTINE lpStartAddr,
									parms,	// LPVOID lpvThreadParm,
									0,		//   DWORD fdwCreate,
									&info.threadId);
	info.threadHandle = (int) temp;
	if (priority == THREAD_HIGHEST) {
		SetThreadPriority( (HANDLE)info.threadHandle, THREAD_PRIORITY_HIGHEST );		//  we better sleep enough to do this
	} else if (priority == THREAD_ABOVE_NORMAL ) {
		SetThreadPriority( (HANDLE)info.threadHandle, THREAD_PRIORITY_ABOVE_NORMAL );
	}
	info.name = name;
	if ( *thread_count < MAX_THREADS ) {
		threads[(*thread_count)++] = &info;
	} else {
		common->DPrintf("WARNING: MAX_THREADS reached\n");
	}
}

/*
==================
Sys_DestroyThread
==================
*/
void Sys_DestroyThread( xthreadInfo& info ) {
	WaitForSingleObject( (HANDLE)info.threadHandle, INFINITE);
	CloseHandle( (HANDLE)info.threadHandle );
	info.threadHandle = 0;
}

/*
==================
Sys_Sentry
==================
*/
void Sys_Sentry() {
	int j = 0;
}

/*
==================
Sys_GetThreadName
==================
*/
const char* Sys_GetThreadName(int *index) {
	int id = GetCurrentThreadId();
	for( int i = 0; i < g_thread_count; i++ ) {
		if ( id == g_threads[i]->threadId ) {
			if ( index ) {
				*index = i;
			}
			return g_threads[i]->name;
		}
	}
	if ( index ) {
		*index = -1;
	}
	return "main";
}


/*
==================
Sys_EnterCriticalSection
==================
*/
void Sys_EnterCriticalSection( int index ) {
	assert( index >= 0 && index < MAX_CRITICAL_SECTIONS );
	if ( TryEnterCriticalSection( &win32.criticalSections[index] ) == 0 ) {
		EnterCriticalSection( &win32.criticalSections[index] );
//		Sys_DebugPrintf( "busy lock '%s' in thread '%s'\n", lock->name, Sys_GetThreadName() );
	}
}

/*
==================
Sys_LeaveCriticalSection
==================
*/
void Sys_LeaveCriticalSection( int index ) {
	assert( index >= 0 && index < MAX_CRITICAL_SECTIONS );
	LeaveCriticalSection( &win32.criticalSections[index] );
}

/*
==================
Sys_WaitForEvent
==================
*/
void Sys_WaitForEvent( int index ) {
	assert( index == 0 );
	if ( !win32.backgroundDownloadSemaphore ) {
		win32.backgroundDownloadSemaphore = CreateEvent( NULL, TRUE, FALSE, NULL );
	}
	WaitForSingleObject( win32.backgroundDownloadSemaphore, INFINITE );
	ResetEvent( win32.backgroundDownloadSemaphore );
}

/*
==================
Sys_TriggerEvent
==================
*/
void Sys_TriggerEvent( int index ) {
	assert( index == 0 );
	SetEvent( win32.backgroundDownloadSemaphore );
}



#pragma optimize( "", on )

#ifdef DEBUG


static unsigned int debug_total_alloc = 0;
static unsigned int debug_total_alloc_count = 0;
static unsigned int debug_current_alloc = 0;
static unsigned int debug_current_alloc_count = 0;
static unsigned int debug_frame_alloc = 0;
static unsigned int debug_frame_alloc_count = 0;

idCVar sys_showMallocs( "sys_showMallocs", "0", CVAR_SYSTEM, "" );

// _HOOK_ALLOC, _HOOK_REALLOC, _HOOK_FREE

typedef struct CrtMemBlockHeader
{
	struct _CrtMemBlockHeader *pBlockHeaderNext;	// Pointer to the block allocated just before this one:
	struct _CrtMemBlockHeader *pBlockHeaderPrev;	// Pointer to the block allocated just after this one
   char *szFileName;    // File name
   int nLine;           // Line number
   size_t nDataSize;    // Size of user block
   int nBlockUse;       // Type of block
   long lRequest;       // Allocation number
	byte		gap[4];								// Buffer just before (lower than) the user's memory:
} CrtMemBlockHeader;

#include <crtdbg.h>

/*
==================
Sys_AllocHook

	called for every malloc/new/free/delete
==================
*/
int Sys_AllocHook( int nAllocType, void *pvData, size_t nSize, int nBlockUse, long lRequest, const unsigned char * szFileName, int nLine ) 
{
	CrtMemBlockHeader	*pHead;
	byte				*temp;

	if ( nBlockUse == _CRT_BLOCK )
	{
      return( TRUE );
	}

	// get a pointer to memory block header
	temp = ( byte * )pvData;
	temp -= 32;
	pHead = ( CrtMemBlockHeader * )temp;

	switch( nAllocType ) {
		case	_HOOK_ALLOC:
			debug_total_alloc += nSize;
			debug_current_alloc += nSize;
			debug_frame_alloc += nSize;
			debug_total_alloc_count++;
			debug_current_alloc_count++;
			debug_frame_alloc_count++;
			break;

		case	_HOOK_FREE:
			assert( pHead->gap[0] == 0xfd && pHead->gap[1] == 0xfd && pHead->gap[2] == 0xfd && pHead->gap[3] == 0xfd );

			debug_current_alloc -= pHead->nDataSize;
			debug_current_alloc_count--;
			debug_total_alloc_count++;
			debug_frame_alloc_count++;
			break;

		case	_HOOK_REALLOC:
			assert( pHead->gap[0] == 0xfd && pHead->gap[1] == 0xfd && pHead->gap[2] == 0xfd && pHead->gap[3] == 0xfd );

			debug_current_alloc -= pHead->nDataSize;
			debug_total_alloc += nSize;
			debug_current_alloc += nSize;
			debug_frame_alloc += nSize;
			debug_total_alloc_count++;
			debug_current_alloc_count--;
			debug_frame_alloc_count++;
			break;
	}
	return( TRUE );
}

/*
==================
Sys_DebugMemory_f
==================
*/
void Sys_DebugMemory_f( void ) {
  	common->Printf( "Total allocation %8dk in %d blocks\n", debug_total_alloc / 1024, debug_total_alloc_count );
  	common->Printf( "Current allocation %8dk in %d blocks\n", debug_current_alloc / 1024, debug_current_alloc_count );
}

/*
==================
Sys_MemFrame
==================
*/
void Sys_MemFrame( void ) {
	if( sys_showMallocs.GetInteger() ) {
		common->Printf("Frame: %8dk in %5d blocks\n", debug_frame_alloc / 1024, debug_frame_alloc_count );
	}

	debug_frame_alloc = 0;
	debug_frame_alloc_count = 0;
}

#endif

/*
==================
Sys_FlushCacheMemory

On windows, the vertex buffers are write combined, so they
don't need to be flushed from the cache
==================
*/
void Sys_FlushCacheMemory( void *base, int bytes ) {
}

/*
=============
Sys_Error

Show the early console as an error dialog
=============
*/
void Sys_Error( const char *error, ... ) {
	va_list		argptr;
	char		text[4096];
    MSG        msg;

	va_start( argptr, error );
	vsprintf( text, error, argptr );
	va_end( argptr);

	Conbuf_AppendText( text );
	Conbuf_AppendText( "\n" );

	Win_SetErrorText( text );
	Sys_ShowConsole( 1, true );

	timeEndPeriod( 1 );

	Sys_ShutdownInput();

	GLimp_Shutdown();

	// wait for the user to quit
	while ( 1 ) {
		if ( !GetMessage( &msg, NULL, 0, 0 ) ) {
			common->Quit();
		}
		TranslateMessage( &msg );
      	DispatchMessage( &msg );
	}

	Sys_DestroyConsole();

	exit (1);
}

/*
==============
Sys_Quit
==============
*/
void Sys_Quit( void ) {
	timeEndPeriod( 1 );
	Sys_ShutdownInput();
	Sys_DestroyConsole();
	ExitProcess( 0 );
}


/*
==============
Sys_Printf
==============
*/
#define MAXPRINTMSG 4096
void Sys_Printf( const char *fmt, ... ) {
	char		msg[MAXPRINTMSG];

	va_list argptr;
	va_start(argptr, fmt);
	idStr::vsnPrintf( msg, MAXPRINTMSG-1, fmt, argptr );
	va_end(argptr);
	msg[sizeof(msg)-1] = '\0';

	if ( win32.win_outputDebugString.GetBool() ) {
		OutputDebugString( msg );
	}
	if ( win32.win_outputEditString.GetBool() ) {
		Conbuf_AppendText( msg );
	}
}

/*
==============
Sys_DebugPrintf
==============
*/
#define MAXPRINTMSG 4096
void Sys_DebugPrintf( const char *fmt, ... ) {
	char msg[MAXPRINTMSG];

	va_list argptr;
	va_start( argptr, fmt );
	idStr::vsnPrintf( msg, MAXPRINTMSG-1, fmt, argptr );
	msg[ sizeof(msg)-1 ] = '\0';
	va_end( argptr );

	OutputDebugString( msg );
}

/*
==============
Sys_DebugVPrintf
==============
*/
void Sys_DebugVPrintf( const char *fmt, va_list arg ) {
	char msg[MAXPRINTMSG];

	idStr::vsnPrintf( msg, MAXPRINTMSG-1, fmt, arg );
	msg[ sizeof(msg)-1 ] = '\0';

	OutputDebugString( msg );
}

/*
==============
Sys_Sleep
==============
*/
void Sys_Sleep( int msec ) {
	Sleep( msec );
}

/*
==============
Sys_ShowWindow
==============
*/
void Sys_ShowWindow( bool show ) {
	::ShowWindow( win32.hWnd, show ? SW_SHOW : SW_HIDE );
}

/*
==============
Sys_IsWindowVisible
==============
*/
bool Sys_IsWindowVisible( void ) {
	return ( ::IsWindowVisible( win32.hWnd ) != 0 );
}

/*
==============
Sys_Mkdir
==============
*/
void Sys_Mkdir( const char *path ) {
	_mkdir (path);
}

/*
=================
Sys_FileTimeStamp
=================
*/
ID_TIME_T Sys_FileTimeStamp( FILE *fp ) {
	struct _stat st;
	_fstat( _fileno( fp ), &st );
	return (long) st.st_mtime;
}

/*
==============
Sys_Cwd
==============
*/
const char *Sys_Cwd( void ) {
	static char cwd[MAX_OSPATH];

	_getcwd( cwd, sizeof( cwd ) - 1 );
	cwd[MAX_OSPATH-1] = 0;

	return cwd;
}

/*
==============
Sys_DefaultCDPath
==============
*/
const char *Sys_DefaultCDPath( void ) {
	return "";
}

/*
==============
Sys_DefaultBasePath
==============
*/
const char *Sys_DefaultBasePath( void ) {
	return Sys_Cwd();
}

/*
==============
Sys_DefaultSavePath
==============
*/
const char *Sys_DefaultSavePath( void ) {
	return cvarSystem->GetCVarString( "fs_basepath" );
}

/*
==============
Sys_EXEPath
==============
*/
const char *Sys_EXEPath( void ) {
	static char exe[ MAX_OSPATH ];
	GetModuleFileName( NULL, exe, sizeof( exe ) - 1 );
	return exe;
}

/*
==============
Sys_ListFiles
==============
*/
int Sys_ListFiles( const char *directory, const char *extension, idStrList &list ) {
	idStr		search;
	struct _finddata_t findinfo;
	int			findhandle;
	int			flag;

	if ( !extension) {
		extension = "";
	}

	// passing a slash as extension will find directories
	if ( extension[0] == '/' && extension[1] == 0 ) {
		extension = "";
		flag = 0;
	} else {
		flag = _A_SUBDIR;
	}

	sprintf( search, "%s\\*%s", directory, extension );

	// search
	list.Clear();

	findhandle = _findfirst( search, &findinfo );
	if ( findhandle == -1 ) {
		return -1;
	}

	do {
		if ( flag ^ ( findinfo.attrib & _A_SUBDIR ) ) {
			list.Append( findinfo.name );
		}
	} while ( _findnext( findhandle, &findinfo ) != -1 );

	_findclose( findhandle );

	return list.Num();
}


/*
================
Sys_GetClipboardData
================
*/
char *Sys_GetClipboardData( void ) {
	char *data = NULL;
	char *cliptext;

	if ( OpenClipboard( NULL ) != 0 ) {
		HANDLE hClipboardData;

		if ( ( hClipboardData = GetClipboardData( CF_TEXT ) ) != 0 ) {
			if ( ( cliptext = (char *)GlobalLock( hClipboardData ) ) != 0 ) {
				data = (char *)Mem_Alloc( GlobalSize( hClipboardData ) + 1 );
				strcpy( data, cliptext );
				GlobalUnlock( hClipboardData );
				
				strtok( data, "\n\r\b" );
			}
		}
		CloseClipboard();
	}
	return data;
}

/*
================
Sys_SetClipboardData
================
*/
void Sys_SetClipboardData( const char *string ) {
	HGLOBAL HMem;
	char *PMem;

	// allocate memory block
	HMem = (char *)::GlobalAlloc( GMEM_MOVEABLE | GMEM_DDESHARE, strlen( string ) + 1 );
	if ( HMem == NULL ) {
		return;
	}
	// lock allocated memory and obtain a pointer
	PMem = (char *)::GlobalLock( HMem );
	if ( PMem == NULL ) {
		return;
	}
	// copy text into allocated memory block
	lstrcpy( PMem, string );
	// unlock allocated memory
	::GlobalUnlock( HMem );
	// open Clipboard
	if ( !OpenClipboard( 0 ) ) {
		::GlobalFree( HMem );
		return;
	}
	// remove current Clipboard contents
	EmptyClipboard();
	// supply the memory handle to the Clipboard
	SetClipboardData( CF_TEXT, HMem );
	HMem = 0;
	// close Clipboard
	CloseClipboard();
}

/*
========================================================================

DLL Loading

========================================================================
*/

/*
=====================
Sys_DLL_Load
=====================
*/
int Sys_DLL_Load( const char *dllName ) {
	HINSTANCE	libHandle;
	libHandle = LoadLibrary( dllName );
	if ( libHandle ) {
		// since we can't have LoadLibrary load only from the specified path, check it did the right thing
		char loadedPath[ MAX_OSPATH ];
		GetModuleFileName( libHandle, loadedPath, sizeof( loadedPath ) - 1 );
		if ( idStr::IcmpPath( dllName, loadedPath ) ) {
			Sys_Printf( "ERROR: LoadLibrary '%s' wants to load '%s'\n", dllName, loadedPath );
			Sys_DLL_Unload( (int)libHandle );
			return 0;
		}
	}
	return (int)libHandle;
}

/*
=====================
Sys_DLL_GetProcAddress
=====================
*/
void *Sys_DLL_GetProcAddress( int dllHandle, const char *procName ) {
	return GetProcAddress( (HINSTANCE)dllHandle, procName ); 
}

/*
=====================
Sys_DLL_Unload
=====================
*/
void Sys_DLL_Unload( int dllHandle ) {
	if ( !dllHandle ) {
		return;
	}
	if ( FreeLibrary( (HINSTANCE)dllHandle ) == 0 ) {
		int lastError = GetLastError();
		LPVOID lpMsgBuf;
		FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER,
		    NULL,
			lastError,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
			(LPTSTR) &lpMsgBuf,
			0,
			NULL 
		);
		Sys_Error( "Sys_DLL_Unload: FreeLibrary failed - %s (%d)", lpMsgBuf, lastError );
	}
}

/*
========================================================================

EVENT LOOP

========================================================================
*/

#define	MAX_QUED_EVENTS		256
#define	MASK_QUED_EVENTS	( MAX_QUED_EVENTS - 1 )

sysEvent_t	eventQue[MAX_QUED_EVENTS];
int			eventHead = 0;
int			eventTail = 0;

/*
================
Sys_QueEvent

Ptr should either be null, or point to a block of data that can
be freed by the game later.
================
*/
void Sys_QueEvent( int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr ) {
	sysEvent_t	*ev;

	ev = &eventQue[ eventHead & MASK_QUED_EVENTS ];

	if ( eventHead - eventTail >= MAX_QUED_EVENTS ) {
		common->Printf("Sys_QueEvent: overflow\n");
		// we are discarding an event, but don't leak memory
		if ( ev->evPtr ) {
			Mem_Free( ev->evPtr );
		}
		eventTail++;
	}

	eventHead++;

	ev->evType = type;
	ev->evValue = value;
	ev->evValue2 = value2;
	ev->evPtrLength = ptrLength;
	ev->evPtr = ptr;
}

/*
=============
Sys_PumpEvents

This allows windows to be moved during renderbump
=============
*/
void Sys_PumpEvents( void ) {
    MSG msg;

	// pump the message loop
	while( PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE ) ) {
		if ( !GetMessage( &msg, NULL, 0, 0 ) ) {
			common->Quit();
		}

		// save the msg time, because wndprocs don't have access to the timestamp
		if ( win32.sysMsgTime && win32.sysMsgTime > (int)msg.time ) {
			// don't ever let the event times run backwards	
//			common->Printf( "Sys_PumpEvents: win32.sysMsgTime (%i) > msg.time (%i)\n", win32.sysMsgTime, msg.time );
		} else {
			win32.sysMsgTime = msg.time;
		}

#ifdef ID_ALLOW_TOOLS
		if ( GUIEditorHandleMessage ( &msg ) ) {	
			continue;
		}
#endif
 
		TranslateMessage (&msg);
      	DispatchMessage (&msg);
	}
}

/*
================
Sys_GenerateEvents
================
*/
void Sys_GenerateEvents( void ) {
	static int entered = false;
	char *s;

	if ( entered ) {
		return;
	}
	entered = true;

	// pump the message loop
	Sys_PumpEvents();

	// make sure mouse and joystick are only called once a frame
	IN_Frame();

	// check for console commands
	s = Sys_ConsoleInput();
	if ( s ) {
		char	*b;
		int		len;

		len = strlen( s ) + 1;
		b = (char *)Mem_Alloc( len );
		strcpy( b, s );
		Sys_QueEvent( 0, SE_CONSOLE, 0, 0, len, b );
	}

	entered = false;
}

/*
================
Sys_ClearEvents
================
*/
void Sys_ClearEvents( void ) {
	eventHead = eventTail = 0;
}

/*
================
Sys_GetEvent
================
*/
sysEvent_t Sys_GetEvent( void ) {
	sysEvent_t	ev;

	// return if we have data
	if ( eventHead > eventTail ) {
		eventTail++;
		return eventQue[ ( eventTail - 1 ) & MASK_QUED_EVENTS ];
	}

	// return the empty event 
	memset( &ev, 0, sizeof( ev ) );

	return ev;
}

//================================================================

/*
=================
Sys_In_Restart_f

Restart the input subsystem
=================
*/
void Sys_In_Restart_f( const idCmdArgs &args ) {
	Sys_ShutdownInput();
	Sys_InitInput();
}


/*
==================
Sys_SoundThread
==================
*/
static void Sys_SoundThread( void *parm ) {
	while ( 1 ) {
#ifdef WIN32	
		// Service the sound backend at the usercmd rate.
		int r = WaitForSingleObject( soundTimer, 100 );
		if ( r != WAIT_OBJECT_0 ) {
			OutputDebugString( "Sys_SoundThread: bad wait return" );
		}
#endif
		common->SoundAsync();
	}
}

/*
==============
Sys_StartSoundThread

Start the thread that services only the sound backend.
==============
*/
void Sys_StartSoundThread( void ) {
	// create an auto-reset event that happens 60 times a second
	soundTimer = CreateWaitableTimer( NULL, false, NULL );
	if ( !soundTimer ) {
		common->Error( "Sys_StartSoundThread: CreateWaitableTimer failed" );
	}

	LARGE_INTEGER	t;
	t.HighPart = t.LowPart = 0;
	SetWaitableTimer( soundTimer, &t, USERCMD_MSEC, NULL, NULL, TRUE );

	Sys_CreateThread( (xthread_t)Sys_SoundThread, NULL, THREAD_ABOVE_NORMAL, soundThreadInfo, "Sound", g_threads,  &g_thread_count );

#ifdef SET_THREAD_AFFINITY 
	// give the sound thread an affinity for the second cpu
	SetThreadAffinityMask( (HANDLE)soundThreadInfo.threadHandle, 2 );
#endif

	if ( !soundThreadInfo.threadHandle ) {
		common->Error( "Sys_StartSoundThread: failed" );
	}
}

/*
================
Sys_AlreadyRunning

returns true if there is a copy of D3 running already
================
*/
bool Sys_AlreadyRunning( void ) {
#ifndef DEBUG
	if ( !win32.win_allowMultipleInstances.GetBool() ) {
		HANDLE hMutexOneInstance = ::CreateMutex( NULL, FALSE, "DOOM3" );
		if ( ::GetLastError() == ERROR_ALREADY_EXISTS || ::GetLastError() == ERROR_ACCESS_DENIED ) {
			return true;
		}
	}
#endif
	return false;
}

/*
================
Sys_Init

The cvar system must already be setup
================
*/
#define OSR2_BUILD_NUMBER 1111
#define WIN98_BUILD_NUMBER 1998

void Sys_Init( void ) {

	CoInitialize( NULL );

	// make sure the timer is high precision, otherwise
	// NT gets 18ms resolution
	timeBeginPeriod( 1 );

	// get WM_TIMER messages pumped every millisecond
//	SetTimer( NULL, 0, 100, NULL );

	cmdSystem->AddCommand( "in_restart", Sys_In_Restart_f, CMD_FL_SYSTEM, "restarts the input system" );
#ifdef DEBUG
	cmdSystem->AddCommand( "createResourceIDs", CreateResourceIDs_f, CMD_FL_TOOL, "assigns resource IDs in _resouce.h files" );
#endif
#if 0
	cmdSystem->AddCommand( "setAsyncSound", Sys_SetAsyncSound_f, CMD_FL_SYSTEM, "set the async sound option" );
#endif

	//
	// Windows user name
	//
	win32.win_username.SetString( Sys_GetCurrentUser() );

	//
	// Windows version
	//
	win32.osversion.dwOSVersionInfoSize = sizeof( win32.osversion );

	if ( !GetVersionEx( (LPOSVERSIONINFO)&win32.osversion ) )
		Sys_Error( "Couldn't get OS info" );

	if ( win32.osversion.dwMajorVersion < 4 ) {
		Sys_Error( GAME_NAME " requires Windows version 4 (NT) or greater" );
	}
	if ( win32.osversion.dwPlatformId == VER_PLATFORM_WIN32s ) {
		Sys_Error( GAME_NAME " doesn't run on Win32s" );
	}

	if( win32.osversion.dwPlatformId == VER_PLATFORM_WIN32_NT ) {
		if( win32.osversion.dwMajorVersion <= 4 ) {
			win32.sys_arch.SetString( "WinNT (NT)" );
		} else if( win32.osversion.dwMajorVersion == 5 && win32.osversion.dwMinorVersion == 0 ) {
			win32.sys_arch.SetString( "Win2K (NT)" );
		} else if( win32.osversion.dwMajorVersion == 5 && win32.osversion.dwMinorVersion == 1 ) {
			win32.sys_arch.SetString( "WinXP (NT)" );
		} else if ( win32.osversion.dwMajorVersion == 6 ) {
			win32.sys_arch.SetString( "Vista" );
		} else {
			win32.sys_arch.SetString( "Unknown NT variant" );
		}
	} else if( win32.osversion.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS ) {
		if( win32.osversion.dwMajorVersion == 4 && win32.osversion.dwMinorVersion == 0 ) {
			// Win95
			if( win32.osversion.szCSDVersion[1] == 'C' ) {
				win32.sys_arch.SetString( "Win95 OSR2 (95)" );
			} else {
				win32.sys_arch.SetString( "Win95 (95)" );
			}
		} else if( win32.osversion.dwMajorVersion == 4 && win32.osversion.dwMinorVersion == 10 ) {
			// Win98
			if( win32.osversion.szCSDVersion[1] == 'A' ) {
				win32.sys_arch.SetString( "Win98SE (95)" );
			} else {
				win32.sys_arch.SetString( "Win98 (95)" );
			}
		} else if( win32.osversion.dwMajorVersion == 4 && win32.osversion.dwMinorVersion == 90 ) {
			// WinMe
		  	win32.sys_arch.SetString( "WinMe (95)" );
		} else {
		  	win32.sys_arch.SetString( "Unknown 95 variant" );
		}
	} else {
		win32.sys_arch.SetString( "unknown Windows variant" );
	}

	//
	// CPU type
	//
	if ( !idStr::Icmp( win32.sys_cpustring.GetString(), "detect" ) ) {
		idStr string;

		common->Printf( "%1.0f MHz ", Sys_ClockTicksPerSecond() / 1000000.0f );

		win32.cpuid = Sys_GetCPUId();

		string.Clear();

		if ( win32.cpuid & CPUID_AMD ) {
			string += "AMD CPU";
		} else if ( win32.cpuid & CPUID_INTEL ) {
			string += "Intel CPU";
		} else if ( win32.cpuid & CPUID_UNSUPPORTED ) {
			string += "unsupported CPU";
		} else {
			string += "generic CPU";
		}

		string += " with ";
		if ( win32.cpuid & CPUID_MMX ) {
			string += "MMX & ";
		}
		if ( win32.cpuid & CPUID_3DNOW ) {
			string += "3DNow! & ";
		}
		if ( win32.cpuid & CPUID_SSE ) {
			string += "SSE & ";
		}
		if ( win32.cpuid & CPUID_SSE2 ) {
            string += "SSE2 & ";
		}
		if ( win32.cpuid & CPUID_SSE3 ) {
			string += "SSE3 & ";
		}
		if ( win32.cpuid & CPUID_HTT ) {
			string += "HTT & ";
		}
		string.StripTrailing( " & " );
		string.StripTrailing( " with " );
		win32.sys_cpustring.SetString( string );
	} else {
		common->Printf( "forcing CPU type to " );
		idLexer src( win32.sys_cpustring.GetString(), idStr::Length( win32.sys_cpustring.GetString() ), "sys_cpustring" );
		idToken token;

		int id = CPUID_NONE;
		while( src.ReadToken( &token ) ) {
			if ( token.Icmp( "generic" ) == 0 ) {
				id |= CPUID_GENERIC;
			} else if ( token.Icmp( "intel" ) == 0 ) {
				id |= CPUID_INTEL;
			} else if ( token.Icmp( "amd" ) == 0 ) {
				id |= CPUID_AMD;
			} else if ( token.Icmp( "mmx" ) == 0 ) {
				id |= CPUID_MMX;
			} else if ( token.Icmp( "3dnow" ) == 0 ) {
				id |= CPUID_3DNOW;
			} else if ( token.Icmp( "sse" ) == 0 ) {
				id |= CPUID_SSE;
			} else if ( token.Icmp( "sse2" ) == 0 ) {
				id |= CPUID_SSE2;
			} else if ( token.Icmp( "sse3" ) == 0 ) {
				id |= CPUID_SSE3;
			} else if ( token.Icmp( "htt" ) == 0 ) {
				id |= CPUID_HTT;
			}
		}
		if ( id == CPUID_NONE ) {
			common->Printf( "WARNING: unknown sys_cpustring '%s'\n", win32.sys_cpustring.GetString() );
			id = CPUID_GENERIC;
		}
		win32.cpuid = (cpuid_t) id;
	}

	common->Printf( "%s\n", win32.sys_cpustring.GetString() );
	common->Printf( "%d MB System Memory\n", Sys_GetSystemRam() );
	common->Printf( "%d MB Video Memory\n", Sys_GetVideoRam() );
}

/*
================
Sys_Shutdown
================
*/
void Sys_Shutdown( void ) {
	CoUninitialize();
}

/*
================
Sys_GetProcessorId
================
*/
cpuid_t Sys_GetProcessorId( void ) {
    return win32.cpuid;
}

/*
================
Sys_GetProcessorString
================
*/
const char *Sys_GetProcessorString( void ) {
	return win32.sys_cpustring.GetString();
}

//=======================================================================

//#define SET_THREAD_AFFINITY


/*
====================
Win_Frame
====================
*/
void Win_Frame( void ) {
	// if "viewlog" has been modified, show or hide the log console
	if ( win32.win_viewlog.IsModified() ) {
		if ( !com_skipRenderer.GetBool() && idAsyncNetwork::serverDedicated.GetInteger() != 1 ) {
			Sys_ShowConsole( win32.win_viewlog.GetInteger(), false );
		}
		win32.win_viewlog.ClearModified();
	}
}

extern "C" { void _chkstk( int size ); };
void clrstk( void );

/*
====================
TestChkStk
====================
*/
void TestChkStk( void ) {
	int		buffer[0x1000];

	buffer[0] = 1;
}

/*
====================
HackChkStk
====================
*/
void HackChkStk( void ) {
	DWORD	old;
	VirtualProtect( _chkstk, 6, PAGE_EXECUTE_READWRITE, &old );
	*(byte *)_chkstk = 0xe9;
	*(int *)((int)_chkstk+1) = (int)clrstk - (int)_chkstk - 5;

	TestChkStk();
}

/*
====================
GetExceptionCodeInfo
====================
*/
const char *GetExceptionCodeInfo( UINT code ) {
	switch( code ) {
		case EXCEPTION_ACCESS_VIOLATION: return "The thread tried to read from or write to a virtual address for which it does not have the appropriate access.";
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "The thread tried to access an array element that is out of bounds and the underlying hardware supports bounds checking.";
		case EXCEPTION_BREAKPOINT: return "A breakpoint was encountered.";
		case EXCEPTION_DATATYPE_MISALIGNMENT: return "The thread tried to read or write data that is misaligned on hardware that does not provide alignment. For example, 16-bit values must be aligned on 2-byte boundaries; 32-bit values on 4-byte boundaries, and so on.";
		case EXCEPTION_FLT_DENORMAL_OPERAND: return "One of the operands in a floating-point operation is denormal. A denormal value is one that is too small to represent as a standard floating-point value.";
		case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "The thread tried to divide a floating-point value by a floating-point divisor of zero.";
		case EXCEPTION_FLT_INEXACT_RESULT: return "The result of a floating-point operation cannot be represented exactly as a decimal fraction.";
		case EXCEPTION_FLT_INVALID_OPERATION: return "This exception represents any floating-point exception not included in this list.";
		case EXCEPTION_FLT_OVERFLOW: return "The exponent of a floating-point operation is greater than the magnitude allowed by the corresponding type.";
		case EXCEPTION_FLT_STACK_CHECK: return "The stack overflowed or underflowed as the result of a floating-point operation.";
		case EXCEPTION_FLT_UNDERFLOW: return "The exponent of a floating-point operation is less than the magnitude allowed by the corresponding type.";
		case EXCEPTION_ILLEGAL_INSTRUCTION: return "The thread tried to execute an invalid instruction.";
		case EXCEPTION_IN_PAGE_ERROR: return "The thread tried to access a page that was not present, and the system was unable to load the page. For example, this exception might occur if a network connection is lost while running a program over the network.";
		case EXCEPTION_INT_DIVIDE_BY_ZERO: return "The thread tried to divide an integer value by an integer divisor of zero.";
		case EXCEPTION_INT_OVERFLOW: return "The result of an integer operation caused a carry out of the most significant bit of the result.";
		case EXCEPTION_INVALID_DISPOSITION: return "An exception handler returned an invalid disposition to the exception dispatcher. Programmers using a high-level language such as C should never encounter this exception.";
		case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "The thread tried to continue execution after a noncontinuable exception occurred.";
		case EXCEPTION_PRIV_INSTRUCTION: return "The thread tried to execute an instruction whose operation is not allowed in the current machine mode.";
		case EXCEPTION_SINGLE_STEP: return "A trace trap or other single-instruction mechanism signaled that one instruction has been executed.";
		case EXCEPTION_STACK_OVERFLOW: return "The thread used up its stack.";
		default: return "Unknown exception";
	}
}

/*
====================
EmailCrashReport

  emailer originally from Raven/Quake 4
====================
*/
void EmailCrashReport( LPSTR messageText ) {
	LPMAPISENDMAIL	MAPISendMail;
	MapiMessage		message;
	static int lastEmailTime = 0;

	if ( Sys_Milliseconds() < lastEmailTime + 10000 ) {
		return;
	}

	lastEmailTime = Sys_Milliseconds();

	HINSTANCE mapi = LoadLibrary( "MAPI32.DLL" ); 
	if( mapi ) {
		MAPISendMail = ( LPMAPISENDMAIL )GetProcAddress( mapi, "MAPISendMail" );
		if( MAPISendMail ) {
			MapiRecipDesc toProgrammers =
			{
				0,										// ulReserved
					MAPI_TO,							// ulRecipClass
					"DOOM 3 Crash",						// lpszName
					"SMTP:programmers@idsoftware.com",	// lpszAddress
					0,									// ulEIDSize
					0									// lpEntry
			};

			memset( &message, 0, sizeof( message ) );
			message.lpszSubject = "DOOM 3 Fatal Error";
			message.lpszNoteText = messageText;
			message.nRecipCount = 1;
			message.lpRecips = &toProgrammers;

			MAPISendMail(
				0,									// LHANDLE lhSession
				0,									// ULONG ulUIParam
				&message,							// lpMapiMessage lpMessage
				MAPI_DIALOG,						// FLAGS flFlags
				0									// ULONG ulReserved
				);
		}
		FreeLibrary( mapi );
	}
}

int Sys_FPU_PrintStateFlags( char *ptr, int ctrl, int stat, int tags, int inof, int inse, int opof, int opse );

/*
====================
_except_handler
====================
*/
EXCEPTION_DISPOSITION __cdecl _except_handler( struct _EXCEPTION_RECORD *ExceptionRecord, void * EstablisherFrame,
												struct _CONTEXT *ContextRecord, void * DispatcherContext ) {

	static char msg[ 8192 ];
	char FPUFlags[2048];

	Sys_FPU_PrintStateFlags( FPUFlags, ContextRecord->FloatSave.ControlWord,
										ContextRecord->FloatSave.StatusWord,
										ContextRecord->FloatSave.TagWord,
										ContextRecord->FloatSave.ErrorOffset,
										ContextRecord->FloatSave.ErrorSelector,
										ContextRecord->FloatSave.DataOffset,
										ContextRecord->FloatSave.DataSelector );


	sprintf( msg, 
		"Please describe what you were doing when DOOM 3 crashed!\n"
		"If this text did not pop into your email client please copy and email it to programmers@idsoftware.com\n"
			"\n"
			"-= FATAL EXCEPTION =-\n"
			"\n"
			"%s\n"
			"\n"
			"0x%x at address 0x%08x\n"
			"\n"
			"%s\n"
			"\n"
			"EAX = 0x%08x EBX = 0x%08x\n"
			"ECX = 0x%08x EDX = 0x%08x\n"
			"ESI = 0x%08x EDI = 0x%08x\n"
			"EIP = 0x%08x ESP = 0x%08x\n"
			"EBP = 0x%08x EFL = 0x%08x\n"
			"\n"
			"CS = 0x%04x\n"
			"SS = 0x%04x\n"
			"DS = 0x%04x\n"
			"ES = 0x%04x\n"
			"FS = 0x%04x\n"
			"GS = 0x%04x\n"
			"\n"
			"%s\n",
			com_version.GetString(),
			ExceptionRecord->ExceptionCode,
			ExceptionRecord->ExceptionAddress,
			GetExceptionCodeInfo( ExceptionRecord->ExceptionCode ),
			ContextRecord->Eax, ContextRecord->Ebx,
			ContextRecord->Ecx, ContextRecord->Edx,
			ContextRecord->Esi, ContextRecord->Edi,
			ContextRecord->Eip, ContextRecord->Esp,
			ContextRecord->Ebp, ContextRecord->EFlags,
			ContextRecord->SegCs,
			ContextRecord->SegSs,
			ContextRecord->SegDs,
			ContextRecord->SegEs,
			ContextRecord->SegFs,
			ContextRecord->SegGs,
			FPUFlags
		);

	EmailCrashReport( msg );
	common->FatalError( msg );

    // Tell the OS to restart the faulting instruction
    return ExceptionContinueExecution;
}

#define TEST_FPU_EXCEPTIONS	/*	FPU_EXCEPTION_INVALID_OPERATION |		*/	\
							/*	FPU_EXCEPTION_DENORMALIZED_OPERAND |	*/	\
							/*	FPU_EXCEPTION_DIVIDE_BY_ZERO |			*/	\
							/*	FPU_EXCEPTION_NUMERIC_OVERFLOW |		*/	\
							/*	FPU_EXCEPTION_NUMERIC_UNDERFLOW |		*/	\
							/*	FPU_EXCEPTION_INEXACT_RESULT |			*/	\
								0

/*
==================
WinMain
==================
*/
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow ) {

	const HCURSOR hcurSave = ::SetCursor( LoadCursor( 0, IDC_WAIT ) );

	Sys_SetPhysicalWorkMemory( 192 << 20, 1024 << 20 );

	Sys_GetCurrentMemoryStatus( exeLaunchMemoryStats );

#if 0
    DWORD handler = (DWORD)_except_handler;
    __asm
    {                           // Build EXCEPTION_REGISTRATION record:
        push    handler         // Address of handler function
        push    FS:[0]          // Address of previous handler
        mov     FS:[0],ESP      // Install new EXECEPTION_REGISTRATION
    }
#endif

	win32.hInstance = hInstance;
	idStr::Copynz( sys_cmdline, lpCmdLine, sizeof( sys_cmdline ) );

	// done before Com/Sys_Init since we need this for error output
	Sys_CreateConsole();

	// no abort/retry/fail errors
	SetErrorMode( SEM_FAILCRITICALERRORS );

	for ( int i = 0; i < MAX_CRITICAL_SECTIONS; i++ ) {
		InitializeCriticalSection( &win32.criticalSections[i] );
	}

	// get the initial time base
	Sys_Milliseconds();

#ifdef DEBUG
	// disable the painfully slow MS heap check every 1024 allocs
	_CrtSetDbgFlag( 0 );
#endif

//	Sys_FPU_EnableExceptions( TEST_FPU_EXCEPTIONS );
	Sys_FPU_SetPrecision( FPU_PRECISION_DOUBLE_EXTENDED );

	common->Init( 0, NULL, lpCmdLine );

#ifdef ID_ALLOW_TOOLS
	if ( com_editors ) {
		Sys_EnableEditorTheme();
	}
#endif

#if TEST_FPU_EXCEPTIONS != 0
	common->Printf( Sys_FPU_GetState() );
#endif

#ifndef	ID_DEDICATED
	if ( win32.win_notaskkeys.GetInteger() ) {
		DisableTaskKeys( TRUE, FALSE, /*( win32.win_notaskkeys.GetInteger() == 2 )*/ FALSE );
	}
#endif

	Sys_StartSoundThread();

	// hide or show the early console as necessary
	if ( win32.win_viewlog.GetInteger() || com_skipRenderer.GetBool() || idAsyncNetwork::serverDedicated.GetInteger() ) {
		Sys_ShowConsole( 1, true );
	} else {
		Sys_ShowConsole( 0, false );
	}

#ifdef SET_THREAD_AFFINITY 
	// give the main thread an affinity for the first cpu
	SetThreadAffinityMask( GetCurrentThread(), 1 );
#endif

	::SetCursor( hcurSave );

	// Launch the script debugger
	if ( strstr( lpCmdLine, "+debugger" ) ) {
		// DebuggerClientInit( lpCmdLine );
		return 0;
	}

	::SetFocus( win32.hWnd );

    // main game loop
	while( 1 ) {

		Win_Frame();

#ifdef DEBUG
		Sys_MemFrame();
#endif

		// set exceptions, even if some crappy syscall changes them!
		Sys_FPU_EnableExceptions( TEST_FPU_EXCEPTIONS );

#ifdef ID_ALLOW_TOOLS
		if ( com_editors ) {
			Sys_EnableEditorTheme();
			if ( com_editors & EDITOR_RADIANT ) {
				// Level Editor
				RadiantRun();
			}
			if ( com_editors & EDITOR_GUI ) {
				// GUI editor companion window
				GUIEditorRun();
			}
			if (com_editors & EDITOR_MATERIAL ) {
				//BSM Nerve: Add support for the material editor
				MaterialEditorRun();
			}
			else {
				if ( com_editors & EDITOR_LIGHT ) {
					// in-game Light Editor
					LightEditorRun();
				}
				if ( com_editors & EDITOR_SOUND ) {
					// in-game Sound Editor
					SoundEditorRun();
				}
				if ( com_editors & EDITOR_DECL ) {
					// in-game Declaration Browser
					DeclBrowserRun();
				}
				if ( com_editors & EDITOR_AF ) {
					// in-game Articulated Figure Editor
					AFEditorRun();
				}
				if ( com_editors & EDITOR_PARTICLE ) {
					// in-game Particle Editor
					ParticleEditorRun();
				}
				if ( com_editors & EDITOR_SCRIPT ) {
					// in-game Script Editor
					ScriptEditorRun();
				}
				if ( com_editors & EDITOR_PDA ) {
					// in-game PDA Editor
					PDAEditorRun();
				}
			}
		}
#endif
		// run the game
		common->Frame();
	}

	// never gets here
	return 0;
}

/*
====================
clrstk

I tried to get the run time to call this at every function entry, but
====================
*/
static int	parmBytes;
__declspec( naked ) void clrstk( void ) {
	// eax = bytes to add to stack
	__asm {
		mov		[parmBytes],eax
        neg     eax                     ; compute new stack pointer in eax
        add     eax,esp
        add     eax,4
        xchg    eax,esp
        mov     eax,dword ptr [eax]		; copy the return address
        push    eax
        
        ; clear to zero
        push	edi
        push	ecx
        mov		edi,esp
        add		edi,12
        mov		ecx,[parmBytes]
		shr		ecx,2
        xor		eax,eax
		cld
        rep	stosd
        pop		ecx
        pop		edi
        
        ret
	}
}

/*
==================
idSysLocal::OpenURL
==================
*/
void idSysLocal::OpenURL( const char *url, bool doexit ) {
	static bool doexit_spamguard = false;
	HWND wnd;

	if (doexit_spamguard) {
		common->DPrintf( "OpenURL: already in an exit sequence, ignoring %s\n", url );
		return;
	}

	common->Printf("Open URL: %s\n", url);

	if ( !ShellExecute( NULL, "open", url, NULL, NULL, SW_RESTORE ) ) {
		common->Error( "Could not open url: '%s' ", url );
		return;
	}

	wnd = GetForegroundWindow();
	if ( wnd ) {
		ShowWindow( wnd, SW_MAXIMIZE );
	}

	if ( doexit ) {
		doexit_spamguard = true;
		cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "quit\n" );
	}
}

/*
==================
idSysLocal::StartProcess
==================
*/
void idSysLocal::StartProcess( const char *exePath, bool doexit ) {
	TCHAR				szPathOrig[_MAX_PATH];
	STARTUPINFO			si;
	PROCESS_INFORMATION	pi;

	ZeroMemory( &si, sizeof(si) );
	si.cb = sizeof(si);

	strncpy( szPathOrig, exePath, _MAX_PATH );

	if( !CreateProcess( NULL, szPathOrig, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ) ) {
        common->Error( "Could not start process: '%s' ", szPathOrig );
	    return;
	}

	if ( doexit ) {
		cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "quit\n" );
	}
}

/*
==================
Sys_SetFatalError
==================
*/
void Sys_SetFatalError( const char *error ) {
}

/*
==================
Sys_DoPreferences
==================
*/
void Sys_DoPreferences( void ) {
}
