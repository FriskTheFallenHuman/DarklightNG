#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../../framework/DeclPDA.h"
#include "../edit_public.h"
#include "../radiant/RadiantImGui.h"
#include "PDAEditorImGui.h"

#include "imgui.h"

namespace {

enum pdaRecordType_t { PDA_RECORD_EMAIL, PDA_RECORD_AUDIO, PDA_RECORD_VIDEO };

struct PDAFields {
	char shortName[256], fullName[256], icon[512], id[128], post[256], title[256], security[256];
	void Clear() { shortName[0] = fullName[0] = icon[0] = id[0] = post[0] = title[0] = security[0] = '\0'; }
};

struct RecordFields {
	char name[256], file[512], to[256], from[256], date[128], subject[512], body[8192], image[512];
	char displayName[512], media[512], audio[512], info[2048], preview[512];
	void Clear() {
		name[0] = file[0] = to[0] = from[0] = date[0] = subject[0] = body[0] = image[0] = '\0';
		displayName[0] = media[0] = audio[0] = info[0] = preview[0] = '\0';
	}
};

struct PDAEditorState {
	PDAEditorState() : open( false ), pda( NULL ), selectedEmail( -1 ), selectedAudio( -1 ), selectedVideo( -1 ), dirty( false ), recordDirty( false ), recordType( PDA_RECORD_EMAIL ), editDecl( NULL ) {
		filter[0] = status[0] = newPDAName[0] = newPDAFile[0] = '\0'; fields.Clear(); record.Clear();
	}
	bool open;
	idDeclPDA *pda;
	int selectedEmail, selectedAudio, selectedVideo;
	bool dirty;
	bool recordDirty;
	char filter[256], status[512], newPDAName[256], newPDAFile[512];
	PDAFields fields;
	idStrList emails, audios, videos;
	pdaRecordType_t recordType;
	idDecl *editDecl;
	RecordFields record;
};

static PDAEditorState state;

static void Copy( char *dest, int size, const char *source ) { idStr::Copynz( dest, source != NULL ? source : "", size ); }

static idStr Quote( const char *value ) {
	idStr result = value != NULL ? value : "";
	result.Replace( "\\", "\\\\" );
	result.Replace( "\"", "\\\"" );
	result.Replace( "\r", "" );
	result.Replace( "\n", "\\n" );
	return result;
}

static void MarkChanged() { state.dirty = true; state.status[0] = '\0'; }

static void LoadPDA( const char *name ) {
	if ( state.pda != NULL && name != NULL && idStr::Icmp( state.pda->GetName(), name ) && ( state.dirty || state.recordDirty ) ) { Copy( state.status, sizeof( state.status ), "Save the record and PDA, or reload before switching" ); return; }
	state.pda = name != NULL && name[0] != '\0' ? static_cast<idDeclPDA *>( const_cast<idDecl *>( declManager->FindType( DECL_PDA, name, false ) ) ) : NULL;
	state.emails.Clear(); state.audios.Clear(); state.videos.Clear();
	state.selectedEmail = state.selectedAudio = state.selectedVideo = -1;
	state.fields.Clear(); state.editDecl = NULL; state.record.Clear(); state.dirty = false; state.recordDirty = false; state.status[0] = '\0';
	if ( state.pda == NULL ) return;
	Copy( state.fields.shortName, sizeof( state.fields.shortName ), state.pda->GetPdaName() );
	Copy( state.fields.fullName, sizeof( state.fields.fullName ), state.pda->GetFullName() );
	Copy( state.fields.icon, sizeof( state.fields.icon ), state.pda->GetIcon() );
	Copy( state.fields.id, sizeof( state.fields.id ), state.pda->GetID() );
	Copy( state.fields.post, sizeof( state.fields.post ), state.pda->GetPost() );
	Copy( state.fields.title, sizeof( state.fields.title ), state.pda->GetTitle() );
	Copy( state.fields.security, sizeof( state.fields.security ), state.pda->GetSecurity() );
	for ( int i = 0; i < state.pda->GetNumEmails(); i++ ) { const idDeclEmail *d = state.pda->GetEmailByIndex( i ); if ( d != NULL ) state.emails.Append( d->GetName() ); }
	for ( int i = 0; i < state.pda->GetNumAudios(); i++ ) { const idDeclAudio *d = state.pda->GetAudioByIndex( i ); if ( d != NULL ) state.audios.Append( d->GetName() ); }
	for ( int i = 0; i < state.pda->GetNumVideos(); i++ ) { const idDeclVideo *d = state.pda->GetVideoByIndex( i ); if ( d != NULL ) state.videos.Append( d->GetName() ); }
}

static bool SaveText( idDecl *decl, const idStr &text ) {
	if ( decl == NULL ) return false;
	decl->SetText( text );
	if ( !decl->ReplaceSourceFileText() ) return false;
	decl->Invalidate();
	return true;
}

static bool SavePDA() {
	if ( state.pda == NULL ) return false;
	if ( state.recordDirty ) { Copy( state.status, sizeof( state.status ), "Save or discard the edited record before saving the PDA" ); return false; }
	idStr text;
	text = va( "\npda %s {\n", state.pda->GetName() );
	text += va( "\tname\t\t\"%s\"\n", Quote( state.fields.shortName ).c_str() );
	text += va( "\tfullname\t\"%s\"\n", Quote( state.fields.fullName ).c_str() );
	text += va( "\ticon\t\t\"%s\"\n", Quote( state.fields.icon ).c_str() );
	text += va( "\tid\t\t\"%s\"\n", Quote( state.fields.id ).c_str() );
	text += va( "\tpost\t\t\"%s\"\n", Quote( state.fields.post ).c_str() );
	text += va( "\ttitle\t\t\"%s\"\n", Quote( state.fields.title ).c_str() );
	text += va( "\tsecurity\t\"%s\"\n", Quote( state.fields.security ).c_str() );
	for ( int i = 0; i < state.emails.Num(); i++ ) text += va( "\tpda_email\t\"%s\"\n", Quote( state.emails[i] ).c_str() );
	for ( int i = 0; i < state.audios.Num(); i++ ) text += va( "\tpda_audio\t\"%s\"\n", Quote( state.audios[i] ).c_str() );
	for ( int i = 0; i < state.videos.Num(); i++ ) text += va( "\tpda_video\t\"%s\"\n", Quote( state.videos[i] ).c_str() );
	text += "}\n";
	idStr name = state.pda->GetName();
	if ( !SaveText( state.pda, text ) ) { Copy( state.status, sizeof( state.status ), "Save failed (PDA file may be read-only)" ); return false; }
	LoadPDA( name ); Copy( state.status, sizeof( state.status ), "PDA saved" ); return true;
}

static void LoadRecord( pdaRecordType_t type, const char *name ) {
	if ( state.recordDirty && name != NULL && idStr::Icmp( state.record.name, name ) ) { Copy( state.status, sizeof( state.status ), "Save the current record before switching" ); return; }
	state.recordType = type; state.record.Clear(); state.editDecl = NULL;
	state.recordDirty = false;
	if ( name == NULL || name[0] == '\0' ) return;
	Copy( state.record.name, sizeof( state.record.name ), name );
	if ( type == PDA_RECORD_EMAIL ) {
		idDeclEmail *decl = static_cast<idDeclEmail *>( const_cast<idDecl *>( declManager->FindType( DECL_EMAIL, name, false ) ) ); state.editDecl = decl;
		if ( decl != NULL ) { Copy( state.record.file, sizeof( state.record.file ), decl->GetFileName() ); Copy( state.record.to, sizeof( state.record.to ), decl->GetTo() ); Copy( state.record.from, sizeof( state.record.from ), decl->GetFrom() ); Copy( state.record.date, sizeof( state.record.date ), decl->GetDate() ); Copy( state.record.subject, sizeof( state.record.subject ), decl->GetSubject() ); Copy( state.record.body, sizeof( state.record.body ), decl->GetBody() ); Copy( state.record.image, sizeof( state.record.image ), decl->GetImage() ); }
	} else if ( type == PDA_RECORD_AUDIO ) {
		idDeclAudio *decl = static_cast<idDeclAudio *>( const_cast<idDecl *>( declManager->FindType( DECL_AUDIO, name, false ) ) ); state.editDecl = decl;
		if ( decl != NULL ) { Copy( state.record.file, sizeof( state.record.file ), decl->GetFileName() ); Copy( state.record.displayName, sizeof( state.record.displayName ), decl->GetAudioName() ); Copy( state.record.media, sizeof( state.record.media ), decl->GetWave() ); Copy( state.record.info, sizeof( state.record.info ), decl->GetInfo() ); Copy( state.record.preview, sizeof( state.record.preview ), decl->GetPreview() ); }
	} else {
		idDeclVideo *decl = static_cast<idDeclVideo *>( const_cast<idDecl *>( declManager->FindType( DECL_VIDEO, name, false ) ) ); state.editDecl = decl;
		if ( decl != NULL ) { Copy( state.record.file, sizeof( state.record.file ), decl->GetFileName() ); Copy( state.record.displayName, sizeof( state.record.displayName ), decl->GetVideoName() ); Copy( state.record.media, sizeof( state.record.media ), decl->GetRoq() ); Copy( state.record.audio, sizeof( state.record.audio ), decl->GetWave() ); Copy( state.record.info, sizeof( state.record.info ), decl->GetInfo() ); Copy( state.record.preview, sizeof( state.record.preview ), decl->GetPreview() ); }
	}
}

static bool SaveRecord() {
	if ( state.record.name[0] == '\0' ) return false;
	declType_t type = state.recordType == PDA_RECORD_EMAIL ? DECL_EMAIL : state.recordType == PDA_RECORD_AUDIO ? DECL_AUDIO : DECL_VIDEO;
	const char *typeName = state.recordType == PDA_RECORD_EMAIL ? "email" : state.recordType == PDA_RECORD_AUDIO ? "audio" : "video";
	if ( state.editDecl == NULL ) {
		const char *file = state.record.file[0] != '\0' ? state.record.file : state.pda != NULL ? state.pda->GetFileName() : "newpdas/generated.pda";
		state.editDecl = declManager->CreateNewDecl( type, state.record.name, file );
	}
	idStr text;
	text = va( "\n%s %s {\n", typeName, state.record.name );
	if ( state.recordType == PDA_RECORD_EMAIL ) {
		text += va( "\tto\t\t\"%s\"\n", Quote( state.record.to ).c_str() );
		text += va( "\tfrom\t\t\"%s\"\n", Quote( state.record.from ).c_str() );
		text += va( "\tdate\t\t\"%s\"\n", Quote( state.record.date ).c_str() );
		text += va( "\tsubject\t\"%s\"\n", Quote( state.record.subject ).c_str() );
		text += va( "\timage\t\t\"%s\"\n", Quote( state.record.image ).c_str() );
		text += va( "\ttext { \"%s\" }\n", Quote( state.record.body ).c_str() );
	} else if ( state.recordType == PDA_RECORD_AUDIO ) {
		text += va( "\tname\t\t\"%s\"\n", Quote( state.record.displayName ).c_str() );
		text += va( "\taudio\t\t\"%s\"\n", Quote( state.record.media ).c_str() );
		text += va( "\tinfo\t\t\"%s\"\n", Quote( state.record.info ).c_str() );
		text += va( "\tpreview\t\t\"%s\"\n", Quote( state.record.preview ).c_str() );
	} else {
		text += va( "\tname\t\t\"%s\"\n", Quote( state.record.displayName ).c_str() );
		text += va( "\tvideo\t\t\"%s\"\n", Quote( state.record.media ).c_str() );
		text += va( "\taudio\t\t\"%s\"\n", Quote( state.record.audio ).c_str() );
		text += va( "\tinfo\t\t\"%s\"\n", Quote( state.record.info ).c_str() );
		text += va( "\tpreview\t\t\"%s\"\n", Quote( state.record.preview ).c_str() );
	}
	text += "}\n";
	if ( !SaveText( state.editDecl, text ) ) { Copy( state.status, sizeof( state.status ), "Record save failed" ); return false; }
	idStr name = state.record.name;
	if ( state.recordType == PDA_RECORD_EMAIL && state.emails.Find( name ) == NULL ) state.emails.Append( name );
	if ( state.recordType == PDA_RECORD_AUDIO && state.audios.Find( name ) == NULL ) state.audios.Append( name );
	if ( state.recordType == PDA_RECORD_VIDEO && state.videos.Find( name ) == NULL ) state.videos.Append( name );
	LoadRecord( state.recordType, name ); MarkChanged(); Copy( state.status, sizeof( state.status ), "Record saved; save the PDA to persist its membership" ); return true;
}

static void RenderRecordForm() {
	if ( state.record.name[0] == '\0' ) { ImGui::TextDisabled( "Select a record or press New." ); return; }
	bool changed = false;
	changed |= ImGui::InputText( "Declaration", state.record.name, sizeof( state.record.name ), state.editDecl != NULL ? ImGuiInputTextFlags_ReadOnly : 0 );
	changed |= ImGui::InputText( "Source file", state.record.file, sizeof( state.record.file ), state.editDecl != NULL ? ImGuiInputTextFlags_ReadOnly : 0 );
	if ( state.recordType == PDA_RECORD_EMAIL ) {
		changed |= ImGui::InputText( "To", state.record.to, sizeof( state.record.to ) ); changed |= ImGui::InputText( "From", state.record.from, sizeof( state.record.from ) );
		changed |= ImGui::InputText( "Date", state.record.date, sizeof( state.record.date ) ); changed |= ImGui::InputText( "Subject", state.record.subject, sizeof( state.record.subject ) );
		changed |= ImGui::InputText( "Image", state.record.image, sizeof( state.record.image ) );
		changed |= ImGui::InputTextMultiline( "Body", state.record.body, sizeof( state.record.body ), ImVec2( -1, 220 ) );
	} else {
		changed |= ImGui::InputText( "Display name", state.record.displayName, sizeof( state.record.displayName ) );
		changed |= ImGui::InputText( state.recordType == PDA_RECORD_AUDIO ? "Sound shader" : "Video material", state.record.media, sizeof( state.record.media ) );
		if ( state.recordType == PDA_RECORD_VIDEO ) changed |= ImGui::InputText( "Audio shader", state.record.audio, sizeof( state.record.audio ) );
		changed |= ImGui::InputTextMultiline( "Info", state.record.info, sizeof( state.record.info ), ImVec2( -1, 130 ) );
		changed |= ImGui::InputText( "Preview material", state.record.preview, sizeof( state.record.preview ) );
		const char *sound = state.recordType == PDA_RECORD_AUDIO ? state.record.media : state.record.audio;
		if ( ImGui::Button( "Play audio" ) ) { idSoundWorld *world = soundSystem->GetPlayingSoundWorld(); if ( world != NULL ) world->PlayShaderDirectly( sound ); }
		ImGui::SameLine(); if ( ImGui::Button( "Stop audio" ) ) { idSoundWorld *world = soundSystem->GetPlayingSoundWorld(); if ( world != NULL ) world->PlayShaderDirectly( "" ); }
	}
	if ( changed ) state.recordDirty = true;
	if ( ImGui::Button( "Save record" ) ) SaveRecord();
}

static void NewRecord( pdaRecordType_t type ) {
	if ( state.recordDirty ) { Copy( state.status, sizeof( state.status ), "Save the current record before creating another" ); return; }
	state.recordType = type; state.editDecl = NULL; state.record.Clear();
	const char *suffix = type == PDA_RECORD_EMAIL ? "email" : type == PDA_RECORD_AUDIO ? "audio" : "video";
	int index = type == PDA_RECORD_EMAIL ? state.emails.Num() : type == PDA_RECORD_AUDIO ? state.audios.Num() : state.videos.Num();
	idStr name;
	do { name = va( "%s_%s_%d", state.pda != NULL ? state.pda->GetName() : "new_pda", suffix, index++ ); } while ( declManager->FindType( type == PDA_RECORD_EMAIL ? DECL_EMAIL : type == PDA_RECORD_AUDIO ? DECL_AUDIO : DECL_VIDEO, name, false ) != NULL );
	Copy( state.record.name, sizeof( state.record.name ), name );
	Copy( state.record.file, sizeof( state.record.file ), state.pda != NULL ? state.pda->GetFileName() : "newpdas/generated.pda" );
}

static void RenderAssociationTab( pdaRecordType_t type ) {
	idStrList &items = type == PDA_RECORD_EMAIL ? state.emails : type == PDA_RECORD_AUDIO ? state.audios : state.videos;
	int &selected = type == PDA_RECORD_EMAIL ? state.selectedEmail : type == PDA_RECORD_AUDIO ? state.selectedAudio : state.selectedVideo;
	if ( ImGui::Button( "New" ) ) NewRecord( type );
	ImGui::SameLine();
	if ( ImGui::Button( "Add existing..." ) ) ImGui::OpenPopup( "Add existing PDA record" );
	ImGui::SameLine();
	if ( ImGui::Button( "Remove from PDA" ) && selected >= 0 && selected < items.Num() ) { items.RemoveIndex( selected ); selected = Min( selected, items.Num() - 1 ); state.editDecl = NULL; state.record.Clear(); MarkChanged(); }
	if ( ImGui::BeginPopup( "Add existing PDA record" ) ) {
		declType_t declType = type == PDA_RECORD_EMAIL ? DECL_EMAIL : type == PDA_RECORD_AUDIO ? DECL_AUDIO : DECL_VIDEO;
		ImGui::BeginChild( "ExistingPDARecords", ImVec2( 360, 280 ), ImGuiChildFlags_Borders );
		for ( int i = 0; i < declManager->GetNumDecls( declType ); i++ ) {
			const idDecl *decl = declManager->DeclByIndex( declType, i, false );
			if ( items.Find( decl->GetName() ) == NULL && ImGui::Selectable( decl->GetName() ) ) { items.Append( decl->GetName() ); selected = items.Num() - 1; LoadRecord( type, decl->GetName() ); MarkChanged(); ImGui::CloseCurrentPopup(); }
		}
		ImGui::EndChild(); ImGui::EndPopup();
	}
	if ( ImGui::BeginTable( "PDARecordLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV ) ) {
		ImGui::TableSetupColumn( "Records", ImGuiTableColumnFlags_WidthFixed, 260 ); ImGui::TableSetupColumn( "Record", ImGuiTableColumnFlags_WidthStretch );
		ImGui::TableNextColumn(); ImGui::BeginChild( "PDARecordList", ImVec2( 0, 0 ), ImGuiChildFlags_Borders );
		for ( int i = 0; i < items.Num(); i++ ) if ( ImGui::Selectable( items[i], i == selected ) ) { selected = i; LoadRecord( type, items[i] ); }
		ImGui::EndChild(); ImGui::TableNextColumn(); ImGui::BeginChild( "PDARecordForm" ); RenderRecordForm(); ImGui::EndChild(); ImGui::EndTable();
	}
}

} // namespace

void PDAEditorImGuiShow( const char *pdaName ) { state.open = true; if ( pdaName != NULL && pdaName[0] != '\0' ) LoadPDA( pdaName ); }
void PDAEditorImGuiHide() { state.open = false; }
bool PDAEditorImGuiIsOpen() { return state.open; }

void PDAEditorImGuiRender() {
	if ( !state.open ) return;
	ImGui::SetNextWindowSize( ImVec2( 1180, 760 ), ImGuiCond_FirstUseEver );
	if ( !ImGui::Begin( state.dirty ? "PDA Editor*" : "PDA Editor", &state.open, ImGuiWindowFlags_MenuBar ) ) { ImGui::End(); return; }
	if ( ImGui::BeginMenuBar() ) {
		if ( ImGui::BeginMenu( "File" ) ) { if ( ImGui::MenuItem( "New PDA..." ) ) ImGui::OpenPopup( "New PDA" ); if ( ImGui::MenuItem( "Save", "Ctrl+S", false, state.pda != NULL ) ) SavePDA(); if ( ImGui::MenuItem( "Reload", NULL, false, state.pda != NULL ) ) { idStr n = state.pda->GetName(); state.pda->Invalidate(); LoadPDA( n ); } if ( ImGui::MenuItem( "Close" ) ) state.open = false; ImGui::EndMenu(); }
		ImGui::EndMenuBar();
	}
	if ( ImGui::BeginPopup( "New PDA" ) ) {
		ImGui::InputText( "Name", state.newPDAName, sizeof( state.newPDAName ) ); ImGui::InputText( "PDA file", state.newPDAFile, sizeof( state.newPDAFile ) );
		if ( ImGui::Button( "Create" ) && state.newPDAName[0] != '\0' && state.newPDAFile[0] != '\0' ) { idDecl *decl = declManager->CreateNewDecl( DECL_PDA, state.newPDAName, state.newPDAFile ); decl->ReplaceSourceFileText(); LoadPDA( decl->GetName() ); state.newPDAName[0] = state.newPDAFile[0] = '\0'; ImGui::CloseCurrentPopup(); }
		ImGui::EndPopup();
	}
	if ( ImGui::Button( "Save PDA" ) && state.pda != NULL ) SavePDA();
	ImGui::SameLine(); if ( ImGui::Button( "Random ID" ) ) { idStr value = va( "%d-%02X", 1000 + ( rand() % 8999 ), rand() % 255 ); Copy( state.fields.id, sizeof( state.fields.id ), value ); MarkChanged(); }
	if ( state.status[0] != '\0' ) { ImGui::SameLine(); ImGui::TextUnformatted( state.status ); }
	if ( ImGui::BeginTable( "PDALayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV ) ) {
		ImGui::TableSetupColumn( "PDAs", ImGuiTableColumnFlags_WidthFixed, 280 ); ImGui::TableSetupColumn( "PDA", ImGuiTableColumnFlags_WidthStretch );
		ImGui::TableNextColumn(); ImGui::InputTextWithHint( "##PDAFilter", "Filter PDAs", state.filter, sizeof( state.filter ) ); ImGui::BeginChild( "PDAList", ImVec2( 0, 0 ), ImGuiChildFlags_Borders );
		for ( int i = 0; i < declManager->GetNumDecls( DECL_PDA ); i++ ) { const idDecl *decl = declManager->DeclByIndex( DECL_PDA, i, false ); if ( state.filter[0] != '\0' && idStr::FindText( decl->GetName(), state.filter, false ) < 0 ) continue; if ( ImGui::Selectable( decl->GetName(), state.pda != NULL && !idStr::Icmp( state.pda->GetName(), decl->GetName() ) ) ) LoadPDA( decl->GetName() ); }
		ImGui::EndChild(); ImGui::TableNextColumn();
		if ( ImGui::BeginTabBar( "PDATabs" ) ) {
			if ( ImGui::BeginTabItem( "PDA" ) ) { bool changed = false; changed |= ImGui::InputText( "Short name", state.fields.shortName, sizeof( state.fields.shortName ) ); changed |= ImGui::InputText( "Full name", state.fields.fullName, sizeof( state.fields.fullName ) ); changed |= ImGui::InputText( "Title", state.fields.title, sizeof( state.fields.title ) ); changed |= ImGui::InputText( "Post", state.fields.post, sizeof( state.fields.post ) ); changed |= ImGui::InputText( "Security", state.fields.security, sizeof( state.fields.security ) ); changed |= ImGui::InputText( "ID", state.fields.id, sizeof( state.fields.id ) ); changed |= ImGui::InputText( "Icon", state.fields.icon, sizeof( state.fields.icon ) ); if ( changed ) MarkChanged(); ImGui::EndTabItem(); }
			if ( ImGui::BeginTabItem( "Email" ) ) { RenderAssociationTab( PDA_RECORD_EMAIL ); ImGui::EndTabItem(); }
			if ( ImGui::BeginTabItem( "Audio" ) ) { RenderAssociationTab( PDA_RECORD_AUDIO ); ImGui::EndTabItem(); }
			if ( ImGui::BeginTabItem( "Video" ) ) { RenderAssociationTab( PDA_RECORD_VIDEO ); ImGui::EndTabItem(); }
			ImGui::EndTabBar();
		}
		ImGui::EndTable();
	}
	ImGui::End();
}

void PDAEditorImGuiShutdown() { state.open = false; state.pda = NULL; state.editDecl = NULL; state.emails.Clear(); state.audios.Clear(); state.videos.Clear(); }

void PDAEditorInit( const idDict *spawnArgs ) { if ( RadiantImGuiWindow() == NULL ) RadiantInit(); PDAEditorImGuiShow( spawnArgs != NULL ? spawnArgs->GetString( "pda" ) : NULL ); RadiantImGuiFocus(); idKeyInput::ClearStates(); com_editors &= ~EDITOR_PDA; }
void PDAEditorRun() {}
void PDAEditorShutdown() { PDAEditorImGuiShutdown(); com_editors &= ~EDITOR_PDA; }
