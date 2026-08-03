#ifndef __TOOL_EDITORS_IMGUI_H__
#define __TOOL_EDITORS_IMGUI_H__

enum toolEditorImGui_t {
	TOOL_IMGUI_DECL_BROWSER = 0,
	TOOL_IMGUI_MATERIAL_EDITOR,
	TOOL_IMGUI_PARTICLE_EDITOR,
	TOOL_IMGUI_SOUND_EDITOR,
	TOOL_IMGUI_AF_EDITOR,
	TOOL_IMGUI_PDA_EDITOR,
	TOOL_IMGUI_SCRIPT_EDITOR,
	TOOL_IMGUI_EDITOR_COUNT
};

void ToolEditorsImGuiShow( toolEditorImGui_t editor, const char *selection = NULL );
void ToolEditorsImGuiHide( toolEditorImGui_t editor );
bool ToolEditorsImGuiIsOpen( toolEditorImGui_t editor );
void ToolEditorsImGuiRender();
void ToolEditorsImGuiShutdown();

#endif
