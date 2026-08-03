#ifndef __MATERIAL_EDITOR_IMGUI_H__
#define __MATERIAL_EDITOR_IMGUI_H__

void MaterialEditorImGuiShow( const char *materialName = NULL );
void MaterialEditorImGuiHide();
bool MaterialEditorImGuiIsOpen();
void MaterialEditorImGuiRender();
void MaterialEditorImGuiShutdown();

#endif
