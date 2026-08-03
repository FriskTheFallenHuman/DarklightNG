#ifndef __AF_EDITOR_IMGUI_H__
#define __AF_EDITOR_IMGUI_H__

void AFEditorImGuiShow( const char *afName = NULL );
void AFEditorImGuiHide();
bool AFEditorImGuiIsOpen();
void AFEditorImGuiRender();
void AFEditorImGuiShutdown();

#endif
