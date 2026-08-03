#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../af/AFEditorImGui.h"
#include "../edit_public.h"
#include "../materialeditor/MaterialEditorImGui.h"
#include "../particle/ParticleEditorImGui.h"
#include "../pda/PDAEditorImGui.h"
#include "../radiant/RadiantImGui.h"
#include "../script/ScriptEditorImGui.h"
#include "../sound/SoundEditorImGui.h"
#include "DeclBrowserImGui.h"

#include "imgui.h"
#include <vector>

namespace {

struct DeclBrowserState {
	DeclBrowserState() : open( false ), selectedType( DECL_MATERIAL ), decl( NULL ), dirty( false ), pendingSelection( false ), newType( DECL_MATERIAL ) {
		nameFilter[0] = textFilter[0] = status[0] = newName[0] = newFile[0] = '\0'; source.resize( 1024 * 1024, 0 );
	}
	bool open;
	declType_t selectedType;
	idDecl *decl;
	bool dirty, pendingSelection;
	char nameFilter[256], textFilter[256], status[512], newName[256], newFile[512];
	std::vector<char> source;
	declType_t newType;
	idStrList looseFiles;
};

static DeclBrowserState state;

static void RefreshLooseFiles() {
	state.looseFiles.Clear();
	idFileList *scripts = fileSystem->ListFilesTree( "script", ".script", true );
	if ( scripts != NULL ) { for ( int i = 0; i < scripts->GetNumFiles(); i++ ) state.looseFiles.Append( scripts->GetFile( i ) ); fileSystem->FreeFileList( scripts ); }
	idFileList *guis = fileSystem->ListFilesTree( "guis", ".gui", true );
	if ( guis != NULL ) { for ( int i = 0; i < guis->GetNumFiles(); i++ ) state.looseFiles.Append( guis->GetFile( i ) ); fileSystem->FreeFileList( guis ); }
	state.looseFiles.Sort();
}

static bool TextMatches( const idDecl *decl ) {
	if ( decl == NULL ) return false;
	if ( state.nameFilter[0] != '\0' && !idStr::Filter( state.nameFilter, decl->GetName(), false ) && idStr::FindText( decl->GetName(), state.nameFilter, false ) < 0 ) return false;
	if ( state.textFilter[0] != '\0' ) {
		const int length = decl->GetTextLength();
		std::vector<char> text( length + 1, 0 ); decl->GetText( text.data() );
		if ( idStr::FindText( text.data(), state.textFilter, false ) < 0 ) return false;
	}
	return true;
}

static void SelectDecl( declType_t type, const char *name ) {
	if ( state.decl != NULL && name != NULL && ( state.decl->GetType() != type || idStr::Icmp( state.decl->GetName(), name ) ) && state.dirty ) {
		idStr::Copynz( state.status, "Save or reload the current declaration before switching", sizeof( state.status ) );
		return;
	}
	state.selectedType = type;
	state.decl = name != NULL ? const_cast<idDecl *>( declManager->FindType( type, name, false ) ) : NULL;
	state.dirty = false; state.source[0] = '\0'; state.status[0] = '\0';
	if ( state.decl == NULL ) return;
	const int length = state.decl->GetTextLength();
	if ( length + 1 >= (int)state.source.size() ) state.source.resize( length + 1024 * 64, 0 );
	state.decl->GetText( state.source.data() );
	idStr::Copynz( state.status, va( "%s:%d", state.decl->GetFileName(), state.decl->GetLineNum() ), sizeof( state.status ) );
}

static bool SaveDecl() {
	if ( state.decl == NULL ) return false;
	idStr name = state.decl->GetName(); declType_t type = state.decl->GetType();
	state.decl->SetText( state.source.data() );
	if ( !state.decl->ReplaceSourceFileText() ) { idStr::Copynz( state.status, "Save failed (source may be read-only)", sizeof( state.status ) ); return false; }
	state.decl->Invalidate();
	state.decl = const_cast<idDecl *>( declManager->FindType( type, name, false ) );
	state.dirty = false;
	idStr::Copynz( state.status, state.decl != NULL && state.decl->GetState() != DS_DEFAULTED ? "Saved and parsed" : "Saved, but declaration defaulted while parsing", sizeof( state.status ) );
	return true;
}

static void OpenSpecialized() {
	if ( state.decl == NULL ) return;
	const char *name = state.decl->GetName();
	switch ( state.decl->GetType() ) {
		case DECL_MATERIAL: MaterialEditorImGuiShow( name ); break;
		case DECL_SOUND: SoundEditorImGuiShow( name ); break;
		case DECL_PARTICLE: ParticleEditorImGuiShow( name ); break;
		case DECL_AF: AFEditorImGuiShow( name ); break;
		case DECL_PDA: PDAEditorImGuiShow( name ); break;
		default: break;
	}
}

static bool HasSpecializedEditor() {
	if ( state.decl == NULL ) return false;
	declType_t type = state.decl->GetType();
	return type == DECL_MATERIAL || type == DECL_SOUND || type == DECL_PARTICLE || type == DECL_AF || type == DECL_PDA;
}

static void SelectByName( const char *name ) {
	if ( name == NULL || name[0] == '\0' ) return;
	for ( int type = 0; type < declManager->GetNumDeclTypes(); type++ ) {
		const idDecl *decl = declManager->FindType( (declType_t)type, name, false );
		if ( decl != NULL ) { SelectDecl( (declType_t)type, name ); return; }
	}
}

} // namespace

void DeclBrowserImGuiShow( const char *declName ) { state.open = true; if ( state.looseFiles.Num() == 0 ) RefreshLooseFiles(); SelectByName( declName ); }
void DeclBrowserImGuiHide() { state.open = false; }
bool DeclBrowserImGuiIsOpen() { return state.open; }

void DeclBrowserImGuiRender() {
	if ( !state.open ) return;
	ImGui::SetNextWindowSize( ImVec2( 1280, 800 ), ImGuiCond_FirstUseEver );
	if ( !ImGui::Begin( state.dirty ? "Declaration Browser*" : "Declaration Browser", &state.open, ImGuiWindowFlags_MenuBar ) ) { ImGui::End(); return; }
	if ( ImGui::BeginMenuBar() ) {
		if ( ImGui::BeginMenu( "File" ) ) { if ( ImGui::MenuItem( "New declaration..." ) ) ImGui::OpenPopup( "New declaration" ); if ( ImGui::MenuItem( "Save", "Ctrl+S", false, state.decl != NULL ) ) SaveDecl(); if ( ImGui::MenuItem( "Reload declarations" ) ) DeclBrowserReloadDeclarations(); if ( ImGui::MenuItem( "Close" ) ) state.open = false; ImGui::EndMenu(); }
		ImGui::EndMenuBar();
	}
	if ( ImGui::BeginPopup( "New declaration" ) ) {
		const char *preview = declManager->GetDeclNameFromType( state.newType );
		if ( ImGui::BeginCombo( "Type", preview != NULL ? preview : "Unknown" ) ) { for ( int i = 0; i < declManager->GetNumDeclTypes(); i++ ) if ( ImGui::Selectable( declManager->GetDeclNameFromType( (declType_t)i ), state.newType == i ) ) state.newType = (declType_t)i; ImGui::EndCombo(); }
		ImGui::InputText( "Name", state.newName, sizeof( state.newName ) ); ImGui::InputText( "Source file", state.newFile, sizeof( state.newFile ) );
		if ( ImGui::Button( "Create" ) && state.newName[0] != '\0' && state.newFile[0] != '\0' ) { idDecl *decl = declManager->CreateNewDecl( state.newType, state.newName, state.newFile ); decl->ReplaceSourceFileText(); SelectDecl( state.newType, decl->GetName() ); state.newName[0] = state.newFile[0] = '\0'; ImGui::CloseCurrentPopup(); }
		ImGui::EndPopup();
	}
	ImGui::SetNextItemWidth( 220 ); ImGui::InputTextWithHint( "##DeclNameFilter", "Name filter (* and ? supported)", state.nameFilter, sizeof( state.nameFilter ) ); ImGui::SameLine();
	ImGui::SetNextItemWidth( 220 ); ImGui::InputTextWithHint( "##DeclTextFilter", "Contains source text", state.textFilter, sizeof( state.textFilter ) ); ImGui::SameLine();
	if ( ImGui::Button( "Save" ) && state.decl != NULL ) SaveDecl(); ImGui::SameLine();
	if ( ImGui::Button( "Open specialized editor" ) && HasSpecializedEditor() ) OpenSpecialized(); ImGui::SameLine(); ImGui::TextUnformatted( state.status );

	if ( ImGui::BeginTable( "DeclBrowserLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV ) ) {
		ImGui::TableSetupColumn( "Browser", ImGuiTableColumnFlags_WidthFixed, 340 ); ImGui::TableSetupColumn( "Source", ImGuiTableColumnFlags_WidthStretch ); ImGui::TableNextColumn();
		if ( ImGui::BeginTabBar( "DeclBrowserTabs" ) ) {
			if ( ImGui::BeginTabItem( "Declarations" ) ) {
				const char *typeName = declManager->GetDeclNameFromType( state.selectedType );
				if ( ImGui::BeginCombo( "Type", typeName != NULL ? typeName : "Unknown" ) ) { for ( int i = 0; i < declManager->GetNumDeclTypes(); i++ ) if ( ImGui::Selectable( declManager->GetDeclNameFromType( (declType_t)i ), state.selectedType == i ) ) { state.selectedType = (declType_t)i; state.decl = NULL; state.source[0] = '\0'; } ImGui::EndCombo(); }
				ImGui::BeginChild( "DeclList", ImVec2( 0, 0 ), ImGuiChildFlags_Borders );
				int shown = 0; for ( int i = 0; i < declManager->GetNumDecls( state.selectedType ); i++ ) { const idDecl *decl = declManager->DeclByIndex( state.selectedType, i, false ); if ( !TextMatches( decl ) ) continue; shown++; if ( ImGui::Selectable( decl->GetName(), state.decl == decl ) ) SelectDecl( state.selectedType, decl->GetName() ); if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s:%d", decl->GetFileName(), decl->GetLineNum() ); }
				ImGui::EndChild(); ImGui::EndTabItem();
			}
			if ( ImGui::BeginTabItem( "Scripts / GUIs" ) ) { if ( ImGui::Button( "Refresh" ) ) RefreshLooseFiles(); ImGui::BeginChild( "LooseFileList", ImVec2( 0, 0 ), ImGuiChildFlags_Borders ); for ( int i = 0; i < state.looseFiles.Num(); i++ ) { if ( state.nameFilter[0] != '\0' && idStr::FindText( state.looseFiles[i], state.nameFilter, false ) < 0 ) continue; if ( ImGui::Selectable( state.looseFiles[i] ) ) ScriptEditorImGuiShow( state.looseFiles[i] ); } ImGui::EndChild(); ImGui::EndTabItem(); }
			ImGui::EndTabBar();
		}
		ImGui::TableNextColumn();
		if ( state.decl != NULL ) { ImGui::Text( "%s %s", declManager->GetDeclNameFromType( state.decl->GetType() ), state.decl->GetName() ); ImGui::TextDisabled( "%s:%d", state.decl->GetFileName(), state.decl->GetLineNum() ); if ( ImGui::InputTextMultiline( "##DeclSource", state.source.data(), state.source.size(), ImVec2( -1, -1 ), ImGuiInputTextFlags_AllowTabInput ) ) state.dirty = true; }
		else ImGui::TextDisabled( "Select a declaration to inspect or edit its source." );
		ImGui::EndTable();
	}
	ImGui::End();
}

void DeclBrowserImGuiShutdown() { state.open = false; state.decl = NULL; state.looseFiles.Clear(); }

void DeclBrowserInit( const idDict *spawnArgs ) { if ( RadiantImGuiWindow() == NULL ) RadiantInit(); DeclBrowserImGuiShow( spawnArgs != NULL ? spawnArgs->GetString( "decl" ) : NULL ); RadiantImGuiFocus(); idKeyInput::ClearStates(); com_editors &= ~EDITOR_DECL; }
void DeclBrowserRun() {}
void DeclBrowserShutdown() { DeclBrowserImGuiShutdown(); com_editors &= ~EDITOR_DECL; }
void DeclBrowserReloadDeclarations() { declManager->Reload( true ); if ( state.decl != NULL ) { idStr name = state.decl->GetName(); declType_t type = state.decl->GetType(); SelectDecl( type, name ); } idStr::Copynz( state.status, "Declarations reloaded", sizeof( state.status ) ); }
