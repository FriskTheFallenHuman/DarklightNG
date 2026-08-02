#ifndef __RADIANT_IMGUI_H__
#define __RADIANT_IMGUI_H__

bool RadiantImGuiEnabled();
bool RadiantImGuiCreate();
void RadiantImGuiDestroy();
void RadiantImGuiFocus();
void RadiantImGuiFrame();
void RadiantImGuiPumpMessages();
HWND RadiantImGuiWindow();
void RadiantImGuiShowInspector( int mode );
void RadiantImGuiShowLightEditor();
void RadiantImGuiRefreshLightEditor();
void RadiantImGuiShowSurfaceInspector();
void RadiantImGuiRefreshSurfaceInspector();
void RadiantImGuiShowPatchInspector();
void RadiantImGuiRefreshPatchInspector();

#endif
