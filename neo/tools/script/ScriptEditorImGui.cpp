#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../edit_public.h"
#include "../radiant/RadiantImGui.h"
#include "ScriptEditorImGui.h"

#include "imgui.h"
#include <vector>

namespace {

struct ScriptSymbol {
	idStr name;
	int offset;
};

struct ScriptEditorState {
	ScriptEditorState() : open( false ), dirty( false ), pendingStart( -1 ), pendingEnd( -1 ), foundStart( -1 ), foundEnd( -1 ), searchOffset( 0 ), gotoLine( 1 ), matchCase( false ), wholeWord( false ) {
		fileName[0] = filter[0] = find[0] = replace[0] = status[0] = '\0'; text.resize( 1024 * 1024, 0 );
	}
	bool open, dirty;
	char fileName[512], filter[256], find[256], replace[256], status[512];
	std::vector<char> text;
	idStrList files;
	idStrList events;
	idList<ScriptSymbol> symbols;
	int pendingStart, pendingEnd, foundStart, foundEnd, searchOffset, gotoLine;
	bool matchCase, wholeWord;
};

static ScriptEditorState state;

static int TextCallback( ImGuiInputTextCallbackData *data ) {
	if ( state.pendingStart >= 0 ) {
		data->CursorPos = state.pendingEnd;
		data->SelectionStart = state.pendingStart;
		data->SelectionEnd = state.pendingEnd;
		state.pendingStart = state.pendingEnd = -1;
	}
	return 0;
}

static void RefreshFiles() {
	state.files.Clear();
	idFileList *scripts = fileSystem->ListFilesTree( "script", ".script", true );
	if ( scripts != NULL ) { for ( int i = 0; i < scripts->GetNumFiles(); i++ ) state.files.Append( scripts->GetFile( i ) ); fileSystem->FreeFileList( scripts ); }
	idFileList *guis = fileSystem->ListFilesTree( "guis", ".gui", true );
	if ( guis != NULL ) { for ( int i = 0; i < guis->GetNumFiles(); i++ ) state.files.Append( guis->GetFile( i ) ); fileSystem->FreeFileList( guis ); }
	state.files.Sort();
}

static void ParseEvents() {
	state.events.Clear();
	idParser parser;
	idToken token;
	if ( !parser.LoadFile( "script/doom_events.script" ) ) return;
	while ( parser.ReadToken( &token ) ) {
		if ( token != "scriptEvent" ) continue;
		idToken returnType, name;
		if ( !parser.ReadToken( &returnType ) || !parser.ReadToken( &name ) ) break;
		idStr signature = returnType + " " + name;
		while ( parser.ReadToken( &token ) && token != ";" ) signature += " " + token;
		state.events.Append( signature );
	}
}

static void ParseSymbols() {
	state.symbols.Clear();
	const char *source = state.text.data();
	const int length = (int)strlen( source );
	int lineStart = 0;
	for ( int pos = 0; pos <= length; pos++ ) {
		if ( pos != length && source[pos] != '\n' ) continue;
		idStr line( source + lineStart, 0, pos - lineStart );
		line.StripLeading( ' ' ); line.StripLeading( '\t' );
		if ( line.Length() > 0 && line[0] != '/' && line.Find( "(" ) > 0 && line.Find( ")" ) > line.Find( "(" ) && line.Find( ";" ) < 0 ) {
			int paren = line.Find( "(" );
			idStr prefix = line.Left( paren ); prefix.StripTrailing( ' ' ); prefix.StripTrailing( '\t' );
			int space = Max( prefix.Last( ' ' ), prefix.Last( '\t' ) );
			idStr name = prefix.Right( prefix.Length() - space - 1 );
			if ( name.Length() > 0 && name.Icmp( "if" ) && name.Icmp( "while" ) && name.Icmp( "for" ) && name.Icmp( "switch" ) ) {
				ScriptSymbol &symbol = state.symbols.Alloc(); symbol.name = name; symbol.offset = lineStart;
			}
		}
		lineStart = pos + 1;
	}
}

static bool OpenScript( const char *fileName ) {
	if ( fileName == NULL || fileName[0] == '\0' ) return false;
	if ( state.dirty && state.fileName[0] != '\0' && idStr::Icmp( state.fileName, fileName ) ) { idStr::Copynz( state.status, "Save or revert the current file before switching", sizeof( state.status ) ); return false; }
	void *buffer = NULL;
	const int length = fileSystem->ReadFile( fileName, &buffer );
	if ( length < 0 || buffer == NULL ) { idStr::Copynz( state.status, va( "Couldn't open %s", fileName ), sizeof( state.status ) ); return false; }
	if ( length + 1 >= (int)state.text.size() ) state.text.resize( length + 1024 * 64, 0 );
	memcpy( state.text.data(), buffer, length ); state.text[length] = '\0';
	fileSystem->FreeFile( buffer );
	idStr::Copynz( state.fileName, fileName, sizeof( state.fileName ) );
	state.dirty = false; state.searchOffset = 0; state.pendingStart = state.pendingEnd = state.foundStart = state.foundEnd = -1;
	ParseSymbols(); idStr::Copynz( state.status, va( "%d bytes", length ), sizeof( state.status ) ); return true;
}

static bool SaveScript() {
	if ( state.fileName[0] == '\0' ) return false;
	const int length = (int)strlen( state.text.data() );
	if ( fileSystem->WriteFile( state.fileName, state.text.data(), length, "fs_devpath" ) == -1 ) { idStr::Copynz( state.status, va( "Couldn't save %s", state.fileName ), sizeof( state.status ) ); return false; }
	state.dirty = false; ParseSymbols(); idStr::Copynz( state.status, va( "Saved %s", state.fileName ), sizeof( state.status ) ); return true;
}

static bool IsWordChar( char c ) { return idStr::CharIsAlpha( c ) || idStr::CharIsNumeric( c ) || c == '_'; }

static void FindNext() {
	if ( state.find[0] == '\0' ) return;
	const char *source = state.text.data();
	const int length = (int)strlen( source );
	int found = idStr::FindText( source + Min( state.searchOffset, length ), state.find, state.matchCase );
	if ( found >= 0 ) found += Min( state.searchOffset, length );
	if ( found < 0 && state.searchOffset > 0 ) found = idStr::FindText( source, state.find, state.matchCase );
	while ( found >= 0 && state.wholeWord ) {
		const int end = found + (int)strlen( state.find );
		if ( ( found == 0 || !IsWordChar( source[found - 1] ) ) && ( end >= length || !IsWordChar( source[end] ) ) ) break;
		const int next = found + 1; const int relative = idStr::FindText( source + next, state.find, state.matchCase ); found = relative < 0 ? -1 : next + relative;
	}
	if ( found < 0 ) { idStr::Copynz( state.status, "Text not found", sizeof( state.status ) ); return; }
	state.foundStart = found; state.foundEnd = found + (int)strlen( state.find ); state.pendingStart = state.foundStart; state.pendingEnd = state.foundEnd; state.searchOffset = state.foundEnd;
	int line = 1; for ( int i = 0; i < found; i++ ) if ( source[i] == '\n' ) line++;
	idStr::Copynz( state.status, va( "Match at line %d", line ), sizeof( state.status ) );
}

static void ReplaceSelectionAndFind() {
	if ( state.foundStart < 0 || state.foundEnd < state.foundStart ) { FindNext(); return; }
	idStr source = state.text.data();
	idStr result = source.Left( state.foundStart ); result += state.replace; result += source.Mid( state.foundEnd, source.Length() - state.foundEnd );
	if ( result.Length() + 1 >= (int)state.text.size() ) state.text.resize( result.Length() + 1024 * 64, 0 );
	memcpy( state.text.data(), result.c_str(), result.Length() + 1 ); state.dirty = true; state.searchOffset = state.foundStart + (int)strlen( state.replace ); state.pendingStart = state.pendingEnd = state.foundStart = state.foundEnd = -1; FindNext();
}

static void GoToLine() {
	const char *source = state.text.data(); int line = 1; int offset = 0;
	while ( source[offset] != '\0' && line < Max( 1, state.gotoLine ) ) if ( source[offset++] == '\n' ) line++;
	state.pendingStart = state.pendingEnd = offset; idStr::Copynz( state.status, va( "Line %d", line ), sizeof( state.status ) );
}

} // namespace

void ScriptEditorImGuiShow( const char *scriptName ) {
	state.open = true; if ( state.files.Num() == 0 ) RefreshFiles(); if ( state.events.Num() == 0 ) ParseEvents();
	if ( scriptName != NULL && scriptName[0] != '\0' ) OpenScript( scriptName );
}
void ScriptEditorImGuiHide() { state.open = false; }
bool ScriptEditorImGuiIsOpen() { return state.open; }

void ScriptEditorImGuiRender() {
	if ( !state.open ) return;
	ImGui::SetNextWindowSize( ImVec2( 1250, 800 ), ImGuiCond_FirstUseEver );
	if ( !ImGui::Begin( state.dirty ? "Script Editor*" : "Script Editor", &state.open, ImGuiWindowFlags_MenuBar ) ) { ImGui::End(); return; }
	if ( ImGui::BeginMenuBar() ) {
		if ( ImGui::BeginMenu( "File" ) ) { if ( ImGui::MenuItem( "Save", "Ctrl+S", false, state.fileName[0] != '\0' ) ) SaveScript(); if ( ImGui::MenuItem( "Revert", NULL, false, state.fileName[0] != '\0' ) ) OpenScript( state.fileName ); if ( ImGui::MenuItem( "Refresh file list" ) ) RefreshFiles(); if ( ImGui::MenuItem( "Close" ) ) state.open = false; ImGui::EndMenu(); }
		if ( ImGui::BeginMenu( "Script" ) ) { if ( ImGui::MenuItem( "Reload game scripts", NULL, false, state.fileName[0] != '\0' ) ) { if ( state.dirty ) SaveScript(); cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "reloadScript\n" ); } ImGui::EndMenu(); }
		ImGui::EndMenuBar();
	}
	if ( ImGui::Button( "Save" ) ) SaveScript(); ImGui::SameLine();
	ImGui::SetNextItemWidth( 210 ); ImGui::InputTextWithHint( "##FindScript", "Find", state.find, sizeof( state.find ) ); ImGui::SameLine(); if ( ImGui::Button( "Find next" ) ) FindNext(); ImGui::SameLine();
	ImGui::SetNextItemWidth( 180 ); ImGui::InputTextWithHint( "##ReplaceScript", "Replace", state.replace, sizeof( state.replace ) ); ImGui::SameLine(); if ( ImGui::Button( "Replace / next" ) ) ReplaceSelectionAndFind(); ImGui::SameLine();
	ImGui::Checkbox( "Case", &state.matchCase ); ImGui::SameLine(); ImGui::Checkbox( "Whole word", &state.wholeWord ); ImGui::SameLine();
	ImGui::SetNextItemWidth( 80 ); ImGui::InputInt( "Line", &state.gotoLine, 0 ); ImGui::SameLine(); if ( ImGui::Button( "Go" ) ) GoToLine();
	if ( ImGui::BeginTable( "ScriptLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV ) ) {
		ImGui::TableSetupColumn( "Files", ImGuiTableColumnFlags_WidthFixed, 260 ); ImGui::TableSetupColumn( "Editor", ImGuiTableColumnFlags_WidthStretch ); ImGui::TableSetupColumn( "Symbols", ImGuiTableColumnFlags_WidthFixed, 230 );
		ImGui::TableNextColumn(); ImGui::InputTextWithHint( "##ScriptFilter", "Filter files", state.filter, sizeof( state.filter ) ); ImGui::BeginChild( "ScriptFiles", ImVec2( 0, 0 ), ImGuiChildFlags_Borders );
		for ( int i = 0; i < state.files.Num(); i++ ) { if ( state.filter[0] != '\0' && idStr::FindText( state.files[i], state.filter, false ) < 0 ) continue; if ( ImGui::Selectable( state.files[i], !idStr::Icmp( state.fileName, state.files[i] ) ) ) OpenScript( state.files[i] ); }
		ImGui::EndChild();
		ImGui::TableNextColumn(); ImGui::TextUnformatted( state.fileName[0] != '\0' ? state.fileName : "No script selected" );
		const ImVec2 editorSize( -1, -ImGui::GetFrameHeightWithSpacing() );
		if ( ImGui::InputTextMultiline( "##ScriptSource", state.text.data(), state.text.size(), editorSize, ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackAlways, TextCallback ) ) { state.dirty = true; }
		ImGui::TextUnformatted( state.status );
		ImGui::TableNextColumn();
		if ( ImGui::BeginTabBar( "ScriptSymbolsTabs" ) ) {
			if ( ImGui::BeginTabItem( "Functions" ) ) { for ( int i = 0; i < state.symbols.Num(); i++ ) if ( ImGui::Selectable( state.symbols[i].name ) ) { state.pendingStart = state.pendingEnd = state.symbols[i].offset; } ImGui::EndTabItem(); }
			if ( ImGui::BeginTabItem( "Events" ) ) { ImGui::BeginChild( "ScriptEvents" ); for ( int i = 0; i < state.events.Num(); i++ ) { ImGui::Selectable( state.events[i] ); if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", state.events[i].c_str() ); } ImGui::EndChild(); ImGui::EndTabItem(); }
			ImGui::EndTabBar();
		}
		ImGui::EndTable();
	}
	ImGui::End();
}

void ScriptEditorImGuiShutdown() { state.open = false; state.files.Clear(); state.events.Clear(); state.symbols.Clear(); }

void ScriptEditorInit( const idDict *spawnArgs ) { if ( RadiantImGuiWindow() == NULL ) RadiantInit(); const char *file = spawnArgs != NULL ? spawnArgs->GetString( "script" ) : NULL; ScriptEditorImGuiShow( file ); RadiantImGuiFocus(); idKeyInput::ClearStates(); com_editors &= ~EDITOR_SCRIPT; }
void ScriptEditorRun() {}
void ScriptEditorShutdown() { ScriptEditorImGuiShutdown(); com_editors &= ~EDITOR_SCRIPT; }
