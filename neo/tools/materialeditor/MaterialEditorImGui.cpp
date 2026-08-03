#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../edit_public.h"
#include "../radiant/RadiantImGui.h"
#include "MaterialEditorImGui.h"

#include "imgui.h"
#include <vector>

namespace {

struct MaterialDirective {
	idStr key;
	idStr value;
};

struct MaterialStage {
	MaterialStage() : enabled( true ), specialMap( false ) {}
	bool enabled;
	bool specialMap;
	idStr name;
	idList<MaterialDirective> directives;
};

struct MaterialEditorState {
	MaterialEditorState() : open( false ), material( NULL ), selectedStage( -1 ), dirty( false ), sourceDirty( false ) {
		filter[0] = status[0] = newName[0] = newFile[0] = newStageName[0] = newDirectiveKey[0] = newDirectiveValue[0] = renameName[0] = '\0'; source.resize( 1024 * 1024, 0 );
	}
	bool open;
	idMaterial *material;
	idList<MaterialDirective> directives;
	idList<MaterialStage *> stages;
	int selectedStage;
	bool dirty, sourceDirty;
	char filter[256], status[1024], newName[512], newFile[512], newStageName[256], newDirectiveKey[256], newDirectiveValue[1024], renameName[512];
	std::vector<char> source;
	idStr console;
	idStrList undo;
	idStrList redo;
};

static MaterialEditorState state;

static void ClearModel() {
	for ( int i = 0; i < state.stages.Num(); i++ ) delete state.stages[i];
	state.stages.Clear(); state.directives.Clear(); state.selectedStage = -1;
}

static idStr CleanRest( idStr value ) {
	value.StripLeading( ' ' ); value.StripLeading( '\t' ); value.StripTrailing( ' ' ); value.StripTrailing( '\t' ); value.StripTrailing( '\r' );
	return value;
}

static void ParseStage( idLexer &lexer, MaterialStage &stage ) {
	idToken token;
	while ( lexer.ReadToken( &token ) ) {
		if ( token == "}" ) break;
		if ( token == "{" ) { lexer.SkipBracedSection( false ); continue; }
		idStr value; lexer.ReadRestOfLine( value ); value = CleanRest( value );
		if ( !token.Icmp( "name" ) ) { value.StripLeading( '"' ); value.StripTrailing( '"' ); stage.name = value; }
		else { MaterialDirective &directive = stage.directives.Alloc(); directive.key = token; directive.value = value; }
	}
	if ( stage.name.IsEmpty() ) stage.name = va( "Stage %d", state.stages.Num() );
}

static bool ParseSource( const char *source ) {
	ClearModel();
	idLexer lexer;
	lexer.LoadMemory( source, (int)strlen( source ), "Material Editor" );
	lexer.SetFlags( LEXFL_NOSTRINGCONCAT | LEXFL_NOSTRINGESCAPECHARS | LEXFL_ALLOWPATHNAMES | LEXFL_ALLOWMULTICHARLITERALS | LEXFL_ALLOWBACKSLASHSTRINGCONCAT | LEXFL_NOFATALERRORS );
	idToken token;
	if ( !lexer.ReadToken( &token ) ) return false;
	if ( !token.Icmp( "material" ) ) { if ( !lexer.ReadToken( &token ) ) return false; }
	if ( !lexer.SkipUntilString( "{" ) ) return false;
	while ( lexer.ReadToken( &token ) ) {
		if ( token == "}" ) break;
		if ( token == "{" ) {
			MaterialStage *stage = new MaterialStage; state.stages.Append( stage ); ParseStage( lexer, *stage ); continue;
		}
		idStr value; lexer.ReadRestOfLine( value ); value = CleanRest( value );
		if ( !token.Icmp( "diffusemap" ) || !token.Icmp( "bumpmap" ) || !token.Icmp( "specularmap" ) ) {
			MaterialStage *stage = new MaterialStage; stage->specialMap = true; stage->name = token; MaterialDirective &directive = stage->directives.Alloc(); directive.key = "map"; directive.value = value; state.stages.Append( stage );
		} else {
			MaterialDirective &directive = state.directives.Alloc(); directive.key = token; directive.value = value;
		}
	}
	state.selectedStage = state.stages.Num() > 0 ? 0 : -1;
	return !lexer.HadError();
}

static MaterialDirective *FindDirective( idList<MaterialDirective> &directives, const char *key ) {
	for ( int i = 0; i < directives.Num(); i++ ) if ( !directives[i].key.Icmp( key ) ) return &directives[i];
	return NULL;
}

static idStr GenerateSource() {
	idStr text;
	text = va( "\n%s\n{\n", state.material != NULL ? state.material->GetName() : state.newName );
	for ( int i = 0; i < state.directives.Num(); i++ ) {
		text += "\t"; text += state.directives[i].key;
		if ( !state.directives[i].value.IsEmpty() ) { text += "\t"; text += state.directives[i].value; }
		text += "\n";
	}
	for ( int i = 0; i < state.stages.Num(); i++ ) {
		MaterialStage *stage = state.stages[i]; if ( !stage->enabled ) continue;
		if ( stage->specialMap ) {
			MaterialDirective *map = FindDirective( stage->directives, "map" );
			text += va( "\t%s\t%s\n", stage->name.c_str(), map != NULL ? map->value.c_str() : "" );
			continue;
		}
		text += "\t{\n";
		if ( !stage->name.IsEmpty() ) text += va( "\t\tname\t\"%s\"\n", stage->name.c_str() );
		for ( int j = 0; j < stage->directives.Num(); j++ ) {
			text += "\t\t"; text += stage->directives[j].key;
			if ( !stage->directives[j].value.IsEmpty() ) { text += "\t"; text += stage->directives[j].value; }
			text += "\n";
		}
		text += "\t}\n";
	}
	text += "}\n";
	return text;
}

static void UpdateSourceFromModel() {
	idStr text = GenerateSource();
	if ( text.Length() + 1 >= (int)state.source.size() ) state.source.resize( text.Length() + 1024 * 64, 0 );
	memcpy( state.source.data(), text.c_str(), text.Length() + 1 ); state.sourceDirty = false;
}

static void PushHistory( idStrList &history, const char *source ) {
	if ( source == NULL || source[0] == '\0' ) return;
	if ( history.Num() == 0 || history[history.Num() - 1].Cmp( source ) ) history.Append( source );
	if ( history.Num() > 128 ) history.RemoveIndex( 0 );
}

static void Changed() {
	PushHistory( state.undo, state.source.data() );
	state.redo.Clear(); state.dirty = true; state.sourceDirty = false; UpdateSourceFromModel(); state.status[0] = '\0';
}

static void ApplyHistory( idStrList &from, idStrList &to ) {
	if ( from.Num() == 0 ) return;
	PushHistory( to, state.source.data() );
	idStr source = from[from.Num() - 1]; from.RemoveIndex( from.Num() - 1 );
	if ( source.Length() + 1 >= (int)state.source.size() ) state.source.resize( source.Length() + 1024 * 64, 0 );
	memcpy( state.source.data(), source.c_str(), source.Length() + 1 );
	if ( ParseSource( state.source.data() ) ) { state.dirty = true; state.sourceDirty = false; UpdateSourceFromModel(); }
}

static void SelectMaterial( const char *name ) {
	if ( state.material != NULL && name != NULL && idStr::Icmp( state.material->GetName(), name ) && ( state.dirty || state.sourceDirty ) ) {
		idStr::Copynz( state.status, "Save or reload the current material before switching", sizeof( state.status ) );
		return;
	}
	state.material = name != NULL && name[0] != '\0' ? static_cast<idMaterial *>( const_cast<idMaterial *>( declManager->FindMaterial( name, false ) ) ) : NULL;
	ClearModel(); state.undo.Clear(); state.redo.Clear(); state.dirty = state.sourceDirty = false; state.source[0] = '\0'; state.status[0] = '\0';
	if ( state.material == NULL ) return;
	const int length = state.material->GetTextLength(); if ( length + 1 >= (int)state.source.size() ) state.source.resize( length + 1024 * 64, 0 ); state.material->GetText( state.source.data() );
	if ( !ParseSource( state.source.data() ) ) idStr::Copynz( state.status, "Material source contains constructs the structured parser could not fully read", sizeof( state.status ) );
}

static void ApplyMaterial() {
	if ( state.material == NULL ) return;
	if ( state.sourceDirty ) {
		if ( !ParseSource( state.source.data() ) ) { idStr::Copynz( state.status, "Source parse failed", sizeof( state.status ) ); return; }
		state.sourceDirty = false;
	}
	idStr text = GenerateSource();
	state.material->SetText( text );
	state.material->Parse( text, text.Length() );
	state.dirty = true; UpdateSourceFromModel();
	idStr::Copynz( state.status, "Applied to renderer", sizeof( state.status ) );
}

static bool SaveMaterial() {
	if ( state.material == NULL ) return false;
	ApplyMaterial();
	if ( !state.material->Save() ) { idStr::Copynz( state.status, "Save failed (material file may be read-only)", sizeof( state.status ) ); return false; }
	state.dirty = false; idStr::Copynz( state.status, "Material saved", sizeof( state.status ) ); return true;
}

static bool EditString( const char *label, idStr &value, int capacity = 1024 ) {
	char buffer[2048]; capacity = Min( capacity, (int)sizeof( buffer ) ); idStr::Copynz( buffer, value, capacity );
	if ( ImGui::InputText( label, buffer, capacity ) ) { value = buffer; return true; } return false;
}

static void ToggleDirective( idList<MaterialDirective> &directives, const char *key, bool enabled ) {
	for ( int i = 0; i < directives.Num(); i++ ) if ( !directives[i].key.Icmp( key ) ) { if ( !enabled ) directives.RemoveIndex( i ); return; }
	if ( enabled ) { MaterialDirective &directive = directives.Alloc(); directive.key = key; directive.value.Clear(); }
}

static void RenderCommonMaterialProperties() {
	struct Toggle { const char *label; const char *key; } toggles[] = {
		{ "No shadows", "noshadows" }, { "No self shadow", "noselfshadow" }, { "Force shadows", "forceshadows" }, { "Two sided", "twosided" },
		{ "Translucent", "translucent" }, { "No overlays", "nooverlays" }, { "No fog", "nofog" }, { "No impact", "noimpact" }, { "No footsteps", "nosteps" },
		{ "Ladder", "ladder" }, { "Slick", "slick" }, { "Discrete", "discrete" }, { "No fragment", "nofragment" }
	};
	bool changed = false;
	for ( int i = 0; i < (int)( sizeof( toggles ) / sizeof( toggles[0] ) ); i++ ) { bool enabled = FindDirective( state.directives, toggles[i].key ) != NULL; if ( ImGui::Checkbox( toggles[i].label, &enabled ) ) { ToggleDirective( state.directives, toggles[i].key, enabled ); changed = true; } if ( i % 3 != 2 ) ImGui::SameLine(); }
	const char *values[] = { "description", "qer_editorimage", "surfaceType", "sort", "cull", "polygonOffset", "deform", "spectrum", "lightFalloffImage", "guisurf" };
	for ( int i = 0; i < (int)( sizeof( values ) / sizeof( values[0] ) ); i++ ) {
		MaterialDirective *directive = FindDirective( state.directives, values[i] );
		idStr current = directive != NULL ? directive->value : "";
		if ( EditString( values[i], current ) ) { if ( directive == NULL ) { directive = &state.directives.Alloc(); directive->key = values[i]; } directive->value = current; changed = true; }
	}
	if ( changed ) Changed();
}

static void RenderDirectiveGrid( idList<MaterialDirective> &directives, const char *id ) {
	bool changed = false;
	ImGui::PushID( id );
	if ( ImGui::BeginTable( "DirectiveGrid", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable ) ) {
		ImGui::TableSetupColumn( "Directive", ImGuiTableColumnFlags_WidthFixed, 190 ); ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch ); ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthFixed, 55 ); ImGui::TableHeadersRow();
		for ( int i = 0; i < directives.Num(); ) {
			ImGui::PushID( i ); ImGui::TableNextRow(); ImGui::TableNextColumn(); changed |= EditString( "##Key", directives[i].key, 256 ); ImGui::TableNextColumn(); changed |= EditString( "##Value", directives[i].value ); ImGui::TableNextColumn();
			bool removed = ImGui::SmallButton( "Delete" ); ImGui::PopID(); if ( removed ) { directives.RemoveIndex( i ); changed = true; continue; } i++;
		}
		ImGui::EndTable();
	}
	ImGui::InputTextWithHint( "##NewDirectiveKey", "directive", state.newDirectiveKey, sizeof( state.newDirectiveKey ) ); ImGui::SameLine(); ImGui::InputTextWithHint( "##NewDirectiveValue", "value", state.newDirectiveValue, sizeof( state.newDirectiveValue ) ); ImGui::SameLine();
	if ( ImGui::Button( "Add property" ) && state.newDirectiveKey[0] != '\0' ) { MaterialDirective &directive = directives.Alloc(); directive.key = state.newDirectiveKey; directive.value = state.newDirectiveValue; state.newDirectiveKey[0] = state.newDirectiveValue[0] = '\0'; changed = true; }
	ImGui::PopID();
	if ( changed ) Changed();
}

static void AddStage( const char *kind ) {
	MaterialStage *stage = new MaterialStage;
	if ( !idStr::Icmp( kind, "stage" ) ) { stage->name = state.newStageName[0] != '\0' ? state.newStageName : va( "Stage %d", state.stages.Num() + 1 ); }
	else { stage->specialMap = true; stage->name = kind; MaterialDirective &map = stage->directives.Alloc(); map.key = "map"; map.value = "textures/"; }
	state.selectedStage = state.stages.Append( stage ); state.newStageName[0] = '\0'; Changed();
}

static void RenderStageEditor() {
	if ( state.selectedStage < 0 || state.selectedStage >= state.stages.Num() ) { ImGui::TextDisabled( "Select or create a stage." ); return; }
	MaterialStage *stage = state.stages[state.selectedStage]; bool changed = false;
	changed |= ImGui::Checkbox( "Enabled", &stage->enabled );
	if ( stage->specialMap ) {
		ImGui::SameLine(); ImGui::Text( "Special map: %s", stage->name.c_str() );
		MaterialDirective *map = FindDirective( stage->directives, "map" ); if ( map != NULL ) changed |= EditString( "Map expression", map->value );
	} else {
		changed |= EditString( "Stage name", stage->name, 256 );
		const char *common[] = { "map", "blend", "if", "rgb", "red", "green", "blue", "alpha", "alphaTest", "texgen", "translate", "scale", "rotate", "scroll", "vertexColor", "inverseVertexColor", "program", "vertexProgram", "fragmentProgram" };
		if ( ImGui::BeginCombo( "Add common directive", "Select..." ) ) { for ( int i = 0; i < (int)( sizeof( common ) / sizeof( common[0] ) ); i++ ) if ( FindDirective( stage->directives, common[i] ) == NULL && ImGui::Selectable( common[i] ) ) { MaterialDirective &directive = stage->directives.Alloc(); directive.key = common[i]; changed = true; } ImGui::EndCombo(); }
	}
	if ( changed ) Changed();
	RenderDirectiveGrid( stage->directives, "StageDirectives" );
}

} // namespace

void MaterialEditorImGuiShow( const char *materialName ) { state.open = true; if ( materialName != NULL && materialName[0] != '\0' ) SelectMaterial( materialName ); }
void MaterialEditorImGuiHide() { state.open = false; }
bool MaterialEditorImGuiIsOpen() { return state.open; }

void MaterialEditorImGuiRender() {
	if ( !state.open ) return;
	ImGui::SetNextWindowSize( ImVec2( 1320, 820 ), ImGuiCond_FirstUseEver );
	if ( !ImGui::Begin( state.dirty || state.sourceDirty ? "Material Editor*" : "Material Editor", &state.open, ImGuiWindowFlags_MenuBar ) ) { ImGui::End(); return; }
	if ( ImGui::BeginMenuBar() ) {
		if ( ImGui::BeginMenu( "File" ) ) { if ( ImGui::MenuItem( "New material..." ) ) ImGui::OpenPopup( "New material" ); if ( ImGui::MenuItem( "Save", "Ctrl+S", false, state.material != NULL ) ) SaveMaterial(); if ( ImGui::MenuItem( "Reload", NULL, false, state.material != NULL ) ) { idStr name = state.material->GetName(); state.material->Invalidate(); SelectMaterial( name ); } if ( ImGui::MenuItem( "Rename...", NULL, false, state.material != NULL ) ) { idStr::Copynz( state.renameName, state.material->GetName(), sizeof( state.renameName ) ); ImGui::OpenPopup( "Rename material" ); } if ( ImGui::MenuItem( "Close" ) ) state.open = false; ImGui::EndMenu(); }
		if ( ImGui::BeginMenu( "Edit" ) ) { if ( ImGui::MenuItem( "Undo", "Ctrl+Z", false, state.undo.Num() > 0 ) ) ApplyHistory( state.undo, state.redo ); if ( ImGui::MenuItem( "Redo", "Ctrl+Y", false, state.redo.Num() > 0 ) ) ApplyHistory( state.redo, state.undo ); ImGui::EndMenu(); }
		ImGui::EndMenuBar();
	}
	if ( ImGui::BeginPopup( "New material" ) ) { ImGui::InputText( "Name", state.newName, sizeof( state.newName ) ); ImGui::InputText( "Material file", state.newFile, sizeof( state.newFile ) ); if ( ImGui::Button( "Create" ) && state.newName[0] != '\0' && state.newFile[0] != '\0' ) { idMaterial *material = static_cast<idMaterial *>( declManager->CreateNewDecl( DECL_MATERIAL, state.newName, state.newFile ) ); material->ReplaceSourceFileText(); SelectMaterial( material->GetName() ); state.newName[0] = state.newFile[0] = '\0'; ImGui::CloseCurrentPopup(); } ImGui::EndPopup(); }
	if ( ImGui::BeginPopup( "Rename material" ) ) { ImGui::InputText( "New name", state.renameName, sizeof( state.renameName ) ); if ( ImGui::Button( "Rename" ) && state.material != NULL && state.renameName[0] != '\0' ) { idStr old = state.material->GetName(); if ( declManager->RenameDecl( DECL_MATERIAL, old, state.renameName ) ) SelectMaterial( state.renameName ); else idStr::Copynz( state.status, "Rename failed", sizeof( state.status ) ); ImGui::CloseCurrentPopup(); } ImGui::EndPopup(); }
	if ( ImGui::Button( "Save" ) && state.material != NULL ) SaveMaterial(); ImGui::SameLine(); if ( ImGui::Button( "Undo" ) && state.undo.Num() > 0 ) ApplyHistory( state.undo, state.redo ); ImGui::SameLine(); if ( ImGui::Button( "Redo" ) && state.redo.Num() > 0 ) ApplyHistory( state.redo, state.undo ); ImGui::SameLine(); if ( ImGui::Button( "Apply" ) && state.material != NULL ) ApplyMaterial(); ImGui::SameLine(); if ( ImGui::Button( "Apply to selected faces" ) && state.material != NULL ) RadiantImGuiApplyMaterial( state.material->GetName() ); ImGui::SameLine(); ImGui::TextUnformatted( state.status );
	if ( ImGui::BeginTable( "MaterialLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV ) ) {
		ImGui::TableSetupColumn( "Materials", ImGuiTableColumnFlags_WidthFixed, 275 ); ImGui::TableSetupColumn( "Stages", ImGuiTableColumnFlags_WidthFixed, 260 ); ImGui::TableSetupColumn( "Properties", ImGuiTableColumnFlags_WidthStretch );
		ImGui::TableNextColumn(); ImGui::InputTextWithHint( "##MaterialFilter", "Filter materials", state.filter, sizeof( state.filter ) ); ImGui::BeginChild( "MaterialList", ImVec2( 0, 0 ), ImGuiChildFlags_Borders );
		for ( int i = 0; i < declManager->GetNumDecls( DECL_MATERIAL ); i++ ) { const idDecl *decl = declManager->DeclByIndex( DECL_MATERIAL, i, false ); if ( state.filter[0] != '\0' && idStr::FindText( decl->GetName(), state.filter, false ) < 0 ) continue; if ( ImGui::Selectable( decl->GetName(), state.material != NULL && !idStr::Icmp( state.material->GetName(), decl->GetName() ) ) ) SelectMaterial( decl->GetName() ); }
		ImGui::EndChild(); ImGui::TableNextColumn();
		ImGui::InputTextWithHint( "##NewStageName", "New stage name", state.newStageName, sizeof( state.newStageName ) );
		if ( ImGui::Button( "Add stage" ) ) AddStage( "stage" ); ImGui::SameLine(); if ( ImGui::Button( "Bump" ) ) AddStage( "bumpmap" ); ImGui::SameLine(); if ( ImGui::Button( "Diffuse" ) ) AddStage( "diffusemap" ); ImGui::SameLine(); if ( ImGui::Button( "Specular" ) ) AddStage( "specularmap" );
		if ( ImGui::Button( "Up" ) && state.selectedStage > 0 ) { idSwap( state.stages[state.selectedStage], state.stages[state.selectedStage - 1] ); state.selectedStage--; Changed(); } ImGui::SameLine(); if ( ImGui::Button( "Down" ) && state.selectedStage >= 0 && state.selectedStage + 1 < state.stages.Num() ) { idSwap( state.stages[state.selectedStage], state.stages[state.selectedStage + 1] ); state.selectedStage++; Changed(); } ImGui::SameLine(); if ( ImGui::Button( "Delete" ) && state.selectedStage >= 0 ) { delete state.stages[state.selectedStage]; state.stages.RemoveIndex( state.selectedStage ); state.selectedStage = Min( state.selectedStage, state.stages.Num() - 1 ); Changed(); }
		ImGui::BeginChild( "MaterialStages", ImVec2( 0, 0 ), ImGuiChildFlags_Borders ); for ( int i = 0; i < state.stages.Num(); i++ ) { MaterialStage *stage = state.stages[i]; if ( ImGui::Selectable( va( "%s%s", stage->enabled ? "" : "[disabled] ", stage->name.c_str() ), i == state.selectedStage ) ) state.selectedStage = i; } ImGui::EndChild();
		ImGui::TableNextColumn();
		if ( ImGui::BeginTabBar( "MaterialPropertyTabs" ) ) {
			if ( ImGui::BeginTabItem( "Material" ) ) { ImGui::BeginChild( "MaterialProperties" ); RenderCommonMaterialProperties(); ImGui::SeparatorText( "All material directives" ); RenderDirectiveGrid( state.directives, "MaterialDirectives" ); ImGui::EndChild(); ImGui::EndTabItem(); }
			if ( ImGui::BeginTabItem( "Stage" ) ) { ImGui::BeginChild( "MaterialStageProperties" ); RenderStageEditor(); ImGui::EndChild(); ImGui::EndTabItem(); }
			if ( ImGui::BeginTabItem( "Source" ) ) { if ( ImGui::Button( "Apply source to structured editor" ) ) { if ( ParseSource( state.source.data() ) ) { state.sourceDirty = false; state.dirty = true; idStr::Copynz( state.status, "Source parsed", sizeof( state.status ) ); } else idStr::Copynz( state.status, "Source parse failed", sizeof( state.status ) ); } if ( ImGui::InputTextMultiline( "##MaterialSource", state.source.data(), state.source.size(), ImVec2( -1, -1 ), ImGuiInputTextFlags_AllowTabInput ) ) state.sourceDirty = true; ImGui::EndTabItem(); }
			if ( ImGui::BeginTabItem( "Console" ) ) { ImGui::BeginChild( "MaterialConsole", ImVec2( 0, 0 ), ImGuiChildFlags_Borders ); ImGui::TextUnformatted( state.console ); ImGui::EndChild(); ImGui::EndTabItem(); }
			ImGui::EndTabBar();
		}
		ImGui::EndTable();
	}
	ImGui::End();
}

void MaterialEditorImGuiShutdown() { state.open = false; state.material = NULL; ClearModel(); state.console.Clear(); state.undo.Clear(); state.redo.Clear(); }

void MaterialEditorInit() { if ( RadiantImGuiWindow() == NULL ) RadiantInit(); MaterialEditorImGuiShow(); RadiantImGuiFocus(); idKeyInput::ClearStates(); com_editors &= ~EDITOR_MATERIAL; }
void MaterialEditorRun() {}
void MaterialEditorShutdown() { MaterialEditorImGuiShutdown(); com_editors &= ~EDITOR_MATERIAL; }
void MaterialEditorPrintConsole( const char *msg ) { if ( msg == NULL ) return; state.console += msg; if ( state.console.Length() > 128 * 1024 ) state.console = state.console.Right( 96 * 1024 ); }
