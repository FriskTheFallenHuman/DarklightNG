#ifndef __SCRIPT_EDITOR_IMGUI_H__
#define __SCRIPT_EDITOR_IMGUI_H__

void ScriptEditorImGuiShow( const char *scriptName = NULL );
void ScriptEditorImGuiHide();
bool ScriptEditorImGuiIsOpen();
void ScriptEditorImGuiRender();
void ScriptEditorImGuiShutdown();

#endif
