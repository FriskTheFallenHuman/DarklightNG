#ifndef __PDA_EDITOR_IMGUI_H__
#define __PDA_EDITOR_IMGUI_H__

void PDAEditorImGuiShow( const char *pdaName = NULL );
void PDAEditorImGuiHide();
bool PDAEditorImGuiIsOpen();
void PDAEditorImGuiRender();
void PDAEditorImGuiShutdown();

#endif
