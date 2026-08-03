#ifndef __GE_IMGUI_H__
#define __GE_IMGUI_H__

bool GEImGuiCreate();
void GEImGuiDestroy();
void GEImGuiShow();
void GEImGuiHide();
void GEImGuiToggle();
void GEImGuiFrame();
void GEImGuiExecuteCommand( UINT command );
bool GEImGuiIsVisible();
HWND GEImGuiWindow();
bool GEImGuiOwnsWindow( HWND window );
void GEImGuiShowProperties();
void GEImGuiShowScripts();
void GEImGuiShowOptions();
void GEImGuiShowViewer();
void GEImGuiShowAbout();

#endif
