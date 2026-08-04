#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "ToolEditorsImGui.h"
#include "../af/AFEditorImGui.h"
#include "../decl/DeclBrowserImGui.h"
#include "../materialeditor/MaterialEditorImGui.h"
#include "../radiant/MegaTextureEditorImGui.h"
#include "../radiant/RadiantImGui.h"
#include "../particle/ParticleEditorImGui.h"
#include "../pda/PDAEditorImGui.h"
#include "../script/ScriptEditorImGui.h"
#include "../sound/SoundEditorImGui.h"

void ToolEditorsImGuiShow( toolEditorImGui_t editor, const char *selection ) {
	switch ( editor ) {
		case TOOL_IMGUI_DECL_BROWSER: DeclBrowserImGuiShow( selection ); break;
		case TOOL_IMGUI_MATERIAL_EDITOR: MaterialEditorImGuiShow( selection ); break;
		case TOOL_IMGUI_PARTICLE_EDITOR: ParticleEditorImGuiShow( selection ); break;
		case TOOL_IMGUI_SOUND_EDITOR: SoundEditorImGuiShow( selection ); break;
		case TOOL_IMGUI_AF_EDITOR: AFEditorImGuiShow( selection ); break;
		case TOOL_IMGUI_PDA_EDITOR: PDAEditorImGuiShow( selection ); break;
		case TOOL_IMGUI_SCRIPT_EDITOR: ScriptEditorImGuiShow( selection ); break;
		case TOOL_IMGUI_MEGA_TEXTURE_EDITOR:
			MegaTextureEditorImGuiShow( selection );
			RadiantImGuiShowMegaTextureInspector();
			break;
		default: break;
	}
}

void ToolEditorsImGuiHide( toolEditorImGui_t editor ) {
	switch ( editor ) {
		case TOOL_IMGUI_DECL_BROWSER: DeclBrowserImGuiHide(); break;
		case TOOL_IMGUI_MATERIAL_EDITOR: MaterialEditorImGuiHide(); break;
		case TOOL_IMGUI_PARTICLE_EDITOR: ParticleEditorImGuiHide(); break;
		case TOOL_IMGUI_SOUND_EDITOR: SoundEditorImGuiHide(); break;
		case TOOL_IMGUI_AF_EDITOR: AFEditorImGuiHide(); break;
		case TOOL_IMGUI_PDA_EDITOR: PDAEditorImGuiHide(); break;
		case TOOL_IMGUI_SCRIPT_EDITOR: ScriptEditorImGuiHide(); break;
		case TOOL_IMGUI_MEGA_TEXTURE_EDITOR: MegaTextureEditorImGuiHide(); break;
		default: break;
	}
}

bool ToolEditorsImGuiIsOpen( toolEditorImGui_t editor ) {
	switch ( editor ) {
		case TOOL_IMGUI_DECL_BROWSER: return DeclBrowserImGuiIsOpen();
		case TOOL_IMGUI_MATERIAL_EDITOR: return MaterialEditorImGuiIsOpen();
		case TOOL_IMGUI_PARTICLE_EDITOR: return ParticleEditorImGuiIsOpen();
		case TOOL_IMGUI_SOUND_EDITOR: return SoundEditorImGuiIsOpen();
		case TOOL_IMGUI_AF_EDITOR: return AFEditorImGuiIsOpen();
		case TOOL_IMGUI_PDA_EDITOR: return PDAEditorImGuiIsOpen();
		case TOOL_IMGUI_SCRIPT_EDITOR: return ScriptEditorImGuiIsOpen();
		case TOOL_IMGUI_MEGA_TEXTURE_EDITOR: return MegaTextureEditorImGuiIsOpen();
		default: return false;
	}
}

void ToolEditorsImGuiRender() {
	DeclBrowserImGuiRender();
	MaterialEditorImGuiRender();
	ParticleEditorImGuiRender();
	SoundEditorImGuiRender();
	AFEditorImGuiRender();
	PDAEditorImGuiRender();
	ScriptEditorImGuiRender();
}

void ToolEditorsImGuiShutdown() {
	DeclBrowserImGuiShutdown();
	MaterialEditorImGuiShutdown();
	ParticleEditorImGuiShutdown();
	SoundEditorImGuiShutdown();
	AFEditorImGuiShutdown();
	PDAEditorImGuiShutdown();
	ScriptEditorImGuiShutdown();
	MegaTextureEditorImGuiShutdown();
}
