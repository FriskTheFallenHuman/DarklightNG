#ifndef __EDITOR_THEME_H__
#define __EDITOR_THEME_H__

#include <windows.h>

enum editorThemeColor_t {
	EDITOR_THEME_BACKGROUND,
	EDITOR_THEME_CONTROL,
	EDITOR_THEME_FIELD,
	EDITOR_THEME_TEXT,
	EDITOR_THEME_SELECTION,
	EDITOR_THEME_BORDER
};

bool Sys_EditorDarkThemeEnabled();
COLORREF Sys_GetEditorThemeColor( editorThemeColor_t role );
HBRUSH Sys_GetEditorThemeBrush( editorThemeColor_t role );

#endif
