#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../decl/DeclBrowserImGui.h"
#include "../edit_public.h"
#include "../radiant/RadiantImGui.h"
#include "SoundEditorImGui.h"

#include "imgui.h"

namespace {

struct SoundEntityValues {
	SoundEntityValues() { Clear(); }
	void Clear() {
		name[0] = shader[0] = group[0] = '\0';
		volume = 0.0f; minDistance = 1.0f; maxDistance = 10.0f; leadThrough = 0.1f;
		wait = random = shakes = 0.0f;
		omni = -1; triggered = -1;
		occlusion = plain = looping = unclamped = false;
	}
	char name[256];
	char shader[256];
	char group[256];
	float volume;
	float minDistance;
	float maxDistance;
	float leadThrough;
	float wait;
	float random;
	float shakes;
	int omni;
	int triggered;
	bool occlusion;
	bool plain;
	bool looping;
	bool unclamped;
};

struct SoundEditorState {
	SoundEditorState() : open( false ), autoPlay( false ), groupOnly( false ), mapModified( false ), waveBytes( -1 ) {
		filter[0] = status[0] = playName[0] = selectedGroup[0] = selectedSpeaker[0] = newSpeakerName[0] = saveAsPath[0] = '\0';
	}
	bool open;
	bool autoPlay;
	bool groupOnly;
	bool mapModified;
	int waveBytes;
	char filter[256];
	char status[512];
	char playName[512];
	char selectedGroup[256];
	char selectedSpeaker[256];
	char newSpeakerName[256];
	char saveAsPath[512];
	idStrList groups;
	idStrList speakers;
	idStrList inUseSounds;
	idStrList waves;
	SoundEntityValues values;
};

static SoundEditorState state;

static void LoadValues( const idDict *dict ) {
	state.values.Clear();
	if ( dict == NULL ) return;
	idStr::Copynz( state.values.name, dict->GetString( "name" ), sizeof( state.values.name ) );
	idStr::Copynz( state.values.shader, dict->GetString( "s_shader" ), sizeof( state.values.shader ) );
	idStr::Copynz( state.values.group, dict->GetString( "soundgroup" ), sizeof( state.values.group ) );
	state.values.volume = dict->GetFloat( "s_volume", "0" );
	state.values.minDistance = dict->GetFloat( "s_mindistance", "1" );
	state.values.maxDistance = dict->GetFloat( "s_maxdistance", "10" );
	state.values.leadThrough = dict->GetFloat( "s_leadthrough", "0.1" );
	state.values.wait = dict->GetFloat( "wait", "0" );
	state.values.random = dict->GetFloat( "random", "0" );
	state.values.shakes = dict->GetFloat( "s_shakes", "0" );
	state.values.omni = dict->GetInt( "s_omni", "-1" );
	state.values.triggered = dict->GetInt( "s_waitfortrigger", "-1" );
	state.values.occlusion = dict->GetBool( "s_occlusion", "0" );
	state.values.plain = dict->GetBool( "s_plain", "0" );
	state.values.looping = dict->GetBool( "s_looping", "0" );
	state.values.unclamped = dict->GetBool( "s_unclamped", "0" );
}

static void ValuesToDict( idDict &dict ) {
	dict.Set( "name", state.values.name );
	dict.Set( "s_shader", state.values.shader );
	dict.Set( "soundgroup", state.values.group );
	dict.SetFloat( "s_volume", state.values.volume );
	dict.SetFloat( "s_mindistance", state.values.minDistance );
	dict.SetFloat( "s_maxdistance", state.values.maxDistance );
	dict.SetFloat( "s_leadthrough", state.values.leadThrough );
	dict.SetFloat( "wait", state.values.wait );
	dict.SetFloat( "random", state.values.random );
	dict.SetFloat( "s_shakes", state.values.shakes );
	dict.SetInt( "s_omni", state.values.omni );
	dict.SetInt( "s_waitfortrigger", state.values.triggered );
	dict.SetBool( "s_occlusion", state.values.occlusion );
	dict.SetBool( "s_plain", state.values.plain );
	dict.SetBool( "s_looping", state.values.looping );
	dict.SetBool( "s_unclamped", state.values.unclamped );
	dict.SetBool( "s_justVolume", true );
}

static void Play( const char *name ) {
	idSoundWorld *soundWorld = soundSystem->GetPlayingSoundWorld();
	if ( soundWorld != NULL ) soundWorld->PlayShaderDirectly( name != NULL ? name : "" );
}

static void RefreshMapLists() {
	state.groups.Clear();
	state.speakers.Clear();
	state.inUseSounds.Clear();
	if ( gameEdit == NULL ) return;
	idList<const char *> values;
	values.SetNum( 1024 );
	int count = gameEdit->MapGetUniqueMatchingKeyVals( "soundgroup", values.Ptr(), values.Num() );
	for ( int index = 0; index < count; index++ ) if ( values[index] != NULL && values[index][0] != '\0' ) state.groups.AddUnique( values[index] );
	values.SetNum( 1024 );
	const char *group = state.groupOnly ? state.selectedGroup : "";
	count = gameEdit->MapGetEntitiesMatchingClassWithString( "speaker", group, values.Ptr(), values.Num() );
	for ( int index = 0; index < count; index++ ) {
		state.speakers.AddUnique( values[index] );
		const idDict *dict = gameEdit->MapGetEntityDict( values[index] );
		if ( dict != NULL && dict->GetString( "s_shader" )[0] != '\0' ) state.inUseSounds.AddUnique( dict->GetString( "s_shader" ) );
	}
	state.groups.Sort();
	state.speakers.Sort();
	state.inUseSounds.Sort();
}

static void RefreshWaves() {
	state.waves.Clear();
	idFileList *files = fileSystem->ListFilesTree( "sound", ".wav|.ogg", true );
	if ( files != NULL ) {
		for ( int index = 0; index < files->GetNumFiles(); index++ ) state.waves.Append( files->GetFile( index ) );
		fileSystem->FreeFileList( files );
	}
	state.waves.Sort();
}

static void SelectSound( const char *name ) {
	idStr::Copynz( state.values.shader, name != NULL ? name : "", sizeof( state.values.shader ) );
	idStr::Copynz( state.playName, name != NULL ? name : "", sizeof( state.playName ) );
	state.waveBytes = -1;
	if ( state.autoPlay ) Play( state.playName );
}

static void SelectSpeaker( const char *name ) {
	if ( gameEdit == NULL || name == NULL ) return;
	idStr::Copynz( state.selectedSpeaker, name, sizeof( state.selectedSpeaker ) );
	gameEdit->ClearEntitySelection();
	idEntity *entity = gameEdit->FindEntity( name );
	if ( entity != NULL ) gameEdit->AddSelectedEntity( entity );
	const idDict *dict = gameEdit->MapGetEntityDict( name );
	LoadValues( dict );
}

static void SelectGroup( const char *group ) {
	if ( gameEdit == NULL || group == NULL ) return;
	idStr::Copynz( state.selectedGroup, group, sizeof( state.selectedGroup ) );
	idStr::Copynz( state.values.group, group, sizeof( state.values.group ) );
	gameEdit->ClearEntitySelection();
	idList<const char *> entities;
	entities.SetNum( 512 );
	const int count = gameEdit->MapGetEntitiesMatchingClassWithString( "speaker", group, entities.Ptr(), entities.Num() );
	for ( int index = 0; index < count; index++ ) {
		idEntity *entity = gameEdit->FindEntity( entities[index] );
		if ( entity != NULL ) {
			gameEdit->AddSelectedEntity( entity );
			LoadValues( gameEdit->EntityGetSpawnArgs( entity ) );
		}
	}
	RefreshMapLists();
}

static void ApplyValues( bool volumeOnly = false, float volumeDelta = 0.0f ) {
	if ( gameEdit == NULL ) return;
	Play( "" );
	idList<idEntity *> entities;
	entities.SetNum( 128 );
	const int count = gameEdit->GetSelectedEntities( entities.Ptr(), entities.Num() );
	for ( int index = 0; index < count; index++ ) {
		const idDict *spawnArgs = gameEdit->EntityGetSpawnArgs( entities[index] );
		if ( spawnArgs == NULL ) continue;
		const char *name = spawnArgs->GetString( "name" );
		const idDict *mapDict = gameEdit->MapGetEntityDict( name );
		if ( mapDict == NULL ) continue;
		if ( volumeOnly ) {
			state.values.volume = mapDict->GetFloat( "s_volume" ) + volumeDelta;
			gameEdit->MapSetEntityKeyVal( name, "s_volume", va( "%f", state.values.volume ) );
			gameEdit->MapSetEntityKeyVal( name, "s_justVolume", "1" );
		} else {
			idDict source;
			ValuesToDict( source );
			gameEdit->MapCopyDictToEntity( name, &source );
		}
		gameEdit->EntityUpdateChangeableSpawnArgs( entities[index], gameEdit->MapGetEntityDict( name ) );
	}
	if ( count == 0 && state.values.name[0] != '\0' && gameEdit->MapGetEntityDict( state.values.name ) != NULL ) {
		idDict source;
		ValuesToDict( source );
		gameEdit->MapCopyDictToEntity( state.values.name, &source );
	}
	state.mapModified |= count > 0;
	RefreshMapLists();
}

static void TranslateSelected( const idVec3 &translation ) {
	if ( gameEdit == NULL ) return;
	idList<idEntity *> entities;
	entities.SetNum( 128 );
	const int count = gameEdit->GetSelectedEntities( entities.Ptr(), entities.Num() );
	for ( int index = 0; index < count; index++ ) {
		const idDict *dict = gameEdit->EntityGetSpawnArgs( entities[index] );
		if ( dict == NULL ) continue;
		gameEdit->EntityTranslate( entities[index], translation );
		gameEdit->EntityUpdateVisuals( entities[index] );
		gameEdit->MapEntityTranslate( dict->GetString( "name" ), translation );
	}
	state.mapModified |= count > 0;
}

static void SpawnSpeaker() {
	if ( gameEdit == NULL || state.newSpeakerName[0] == '\0' ) return;
	idVec3 origin;
	idAngles angles;
	gameEdit->PlayerGetViewAngles( angles );
	gameEdit->PlayerGetEyePosition( origin );
	origin += idAngles( 0, angles.yaw, 0 ).ToForward() * 80.0f + idVec3( 0, 0, 1 );
	idDict args;
	args.Set( "classname", "speaker" );
	args.Set( "name", state.newSpeakerName );
	args.Set( "origin", origin.ToString() );
	args.SetFloat( "angle", angles.yaw + 180.0f );
	args.Set( "s_shader", state.values.shader );
	args.SetBool( "s_looping", true );
	idEntity *entity = NULL;
	gameEdit->SpawnEntityDef( args, &entity );
	if ( entity != NULL ) {
		gameEdit->EntityUpdateChangeableSpawnArgs( entity, NULL );
		gameEdit->ClearEntitySelection();
		gameEdit->AddSelectedEntity( entity );
	}
	gameEdit->MapAddEntity( &args );
	LoadValues( gameEdit->MapGetEntityDict( state.newSpeakerName ) );
	state.mapModified = true;
	RefreshMapLists();
	ImGui::CloseCurrentPopup();
}

static void DeleteSelectedSpeakers() {
	if ( gameEdit == NULL ) return;
	idList<idEntity *> entities;
	entities.SetNum( 128 );
	const int count = gameEdit->GetSelectedEntities( entities.Ptr(), entities.Num() );
	idStrList names;
	for ( int index = 0; index < count; index++ ) {
		const idDict *dict = gameEdit->EntityGetSpawnArgs( entities[index] );
		if ( dict != NULL ) names.Append( dict->GetString( "name" ) );
	}
	for ( int index = 0; index < names.Num(); index++ ) {
		idEntity *entity = gameEdit->FindEntity( names[index] );
		gameEdit->MapRemoveEntity( names[index] );
		if ( entity != NULL ) {
			gameEdit->EntityStopSound( entity );
			gameEdit->EntityDelete( entity );
		}
	}
	state.mapModified |= names.Num() > 0;
	state.values.Clear();
	RefreshMapLists();
}

static void RenderSoundEntityProperties() {
	ImGui::InputText( "Speaker name", state.values.name, sizeof( state.values.name ), ImGuiInputTextFlags_ReadOnly );
	ImGui::InputText( "Sound shader", state.values.shader, sizeof( state.values.shader ) );
	ImGui::InputText( "Sound group", state.values.group, sizeof( state.values.group ) );
	ImGui::DragFloat( "Volume", &state.values.volume, 0.1f, -60.0f, 60.0f );
	ImGui::DragFloat( "Minimum distance", &state.values.minDistance, 0.1f, 0.0f );
	ImGui::DragFloat( "Maximum distance", &state.values.maxDistance, 0.1f, 0.0f );
	ImGui::DragFloat( "Lead-through", &state.values.leadThrough, 0.01f, 0.0f );
	ImGui::DragFloat( "Wait", &state.values.wait, 0.01f, 0.0f );
	ImGui::DragFloat( "Random", &state.values.random, 0.01f, 0.0f );
	ImGui::DragFloat( "Shakes", &state.values.shakes, 0.05f, 0.0f );
	const char *triState[] = { "Default", "Disabled", "Enabled" };
	int omni = state.values.omni + 1;
	if ( ImGui::Combo( "Omnidirectional", &omni, triState, 3 ) ) state.values.omni = omni - 1;
	int triggered = state.values.triggered + 1;
	if ( ImGui::Combo( "Wait for trigger", &triggered, triState, 3 ) ) state.values.triggered = triggered - 1;
	ImGui::Checkbox( "Occlusion", &state.values.occlusion );
	ImGui::SameLine(); ImGui::Checkbox( "Plain", &state.values.plain );
	ImGui::SameLine(); ImGui::Checkbox( "Looping", &state.values.looping );
	ImGui::SameLine(); ImGui::Checkbox( "Unclamped", &state.values.unclamped );
	if ( ImGui::Button( "Apply to selected" ) ) ApplyValues();
	ImGui::SameLine(); if ( ImGui::Button( "Volume -1 dB" ) ) ApplyValues( true, -1.0f );
	ImGui::SameLine(); if ( ImGui::Button( "Volume +1 dB" ) ) ApplyValues( true, 1.0f );
	ImGui::SameLine(); if ( ImGui::Button( "Trigger selected" ) && gameEdit != NULL ) gameEdit->TriggerSelected();
}

} // namespace

void SoundEditorImGuiShow( const char *soundShader, const idDict *spawnArgs ) {
	state.open = true;
	if ( spawnArgs != NULL ) LoadValues( spawnArgs );
	if ( soundShader != NULL && soundShader[0] != '\0' ) SelectSound( soundShader );
	RefreshMapLists();
	if ( state.waves.Num() == 0 ) RefreshWaves();
}

void SoundEditorImGuiHide() {
	state.open = false;
	Play( "" );
}

bool SoundEditorImGuiIsOpen() {
	return state.open;
}

void SoundEditorImGuiRender() {
	if ( !state.open ) return;
	ImGui::SetNextWindowSize( ImVec2( 1200, 780 ), ImGuiCond_FirstUseEver );
	if ( !ImGui::Begin( "Sound Editor", &state.open, ImGuiWindowFlags_NoCollapse | ( state.mapModified ? ImGuiWindowFlags_UnsavedDocument : 0 ) ) ) {
		ImGui::End();
		return;
	}
	if ( ImGui::Button( "Refresh" ) ) {
		declManager->Reload( false );
		RefreshMapLists();
		RefreshWaves();
	}
	ImGui::SameLine(); if ( ImGui::Button( "Play" ) ) Play( state.playName[0] != '\0' ? state.playName : state.values.shader );
	ImGui::SameLine(); if ( ImGui::Button( "Stop" ) ) Play( "" );
	ImGui::SameLine(); ImGui::Checkbox( "Auto-play", &state.autoPlay );
	ImGui::SameLine(); if ( ImGui::Button( "Edit shader declaration" ) && state.values.shader[0] != '\0' ) DeclBrowserImGuiShow( state.values.shader );
	ImGui::SameLine(); if ( ImGui::Button( "Save map" ) && gameEdit != NULL ) { ApplyValues(); gameEdit->MapSave(); state.mapModified = false; }
	ImGui::SameLine(); if ( ImGui::Button( "Save map as" ) ) { state.saveAsPath[0] = '\0'; ImGui::OpenPopup( "Save sound map as" ); }
	if ( state.mapModified ) { ImGui::SameLine(); ImGui::TextColored( ImVec4( 1, 0.7f, 0.2f, 1 ), "map modified" ); }

	if ( ImGui::BeginPopupModal( "Save sound map as", NULL, ImGuiWindowFlags_AlwaysAutoResize ) ) {
		ImGui::InputText( "Map path", state.saveAsPath, sizeof( state.saveAsPath ) );
		if ( ImGui::Button( "Save", ImVec2( 100, 0 ) ) && gameEdit != NULL && state.saveAsPath[0] != '\0' ) {
			ApplyValues(); gameEdit->MapSave( state.saveAsPath ); state.mapModified = false; ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine(); if ( ImGui::Button( "Cancel", ImVec2( 100, 0 ) ) ) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	if ( ImGui::Button( "Drop speaker" ) && gameEdit != NULL ) {
		idStr::Copynz( state.newSpeakerName, gameEdit->GetUniqueEntityName( "speaker" ), sizeof( state.newSpeakerName ) );
		ImGui::OpenPopup( "Drop speaker entity" );
	}
	ImGui::SameLine(); if ( ImGui::Button( "Delete selected speakers" ) ) ImGui::OpenPopup( "Delete speaker entities" );
	if ( ImGui::BeginPopupModal( "Drop speaker entity", NULL, ImGuiWindowFlags_AlwaysAutoResize ) ) {
		ImGui::InputText( "Entity name", state.newSpeakerName, sizeof( state.newSpeakerName ) );
		if ( ImGui::Button( "Spawn", ImVec2( 100, 0 ) ) ) SpawnSpeaker();
		ImGui::SameLine(); if ( ImGui::Button( "Cancel", ImVec2( 100, 0 ) ) ) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
	if ( ImGui::BeginPopupModal( "Delete speaker entities", NULL, ImGuiWindowFlags_AlwaysAutoResize ) ) {
		ImGui::TextUnformatted( "Delete all selected speaker entities?" );
		if ( ImGui::Button( "Delete", ImVec2( 100, 0 ) ) ) { DeleteSelectedSpeakers(); ImGui::CloseCurrentPopup(); }
		ImGui::SameLine(); if ( ImGui::Button( "Cancel", ImVec2( 100, 0 ) ) ) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	if ( ImGui::BeginTable( "SoundEditorLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV ) ) {
		ImGui::TableSetupColumn( "Sounds", ImGuiTableColumnFlags_WidthFixed, 350 );
		ImGui::TableSetupColumn( "Speakers", ImGuiTableColumnFlags_WidthFixed, 260 );
		ImGui::TableSetupColumn( "Properties", ImGuiTableColumnFlags_WidthStretch );
		ImGui::TableNextColumn();
		ImGui::InputTextWithHint( "##SoundFilter", "filter shaders and waves", state.filter, sizeof( state.filter ) );
		if ( ImGui::BeginTabBar( "SoundSources" ) ) {
			if ( ImGui::BeginTabItem( "Shaders" ) ) {
				ImGui::BeginChild( "SoundShaders", ImVec2( 0, -1 ), ImGuiChildFlags_Borders );
				for ( int index = 0; index < declManager->GetNumDecls( DECL_SOUND ); index++ ) {
					const idSoundShader *shader = declManager->SoundByIndex( index, false );
					if ( shader == NULL || ( state.filter[0] != '\0' && idStr::FindText( shader->GetName(), state.filter, false ) < 0 ) ) continue;
					if ( ImGui::Selectable( shader->GetName(), !idStr::Icmp( state.values.shader, shader->GetName() ) ) ) SelectSound( shader->GetName() );
					if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", shader->GetFileName() );
				}
				ImGui::EndChild();
				ImGui::EndTabItem();
			}
			if ( ImGui::BeginTabItem( "In use" ) ) {
				for ( int index = 0; index < state.inUseSounds.Num(); index++ ) if ( ImGui::Selectable( state.inUseSounds[index] ) ) SelectSound( state.inUseSounds[index] );
				ImGui::EndTabItem();
			}
			if ( ImGui::BeginTabItem( "Wave files" ) ) {
				ImGui::BeginChild( "SoundWaves", ImVec2( 0, -30 ), ImGuiChildFlags_Borders );
				for ( int index = 0; index < state.waves.Num(); index++ ) {
					const char *wave = state.waves[index];
					if ( state.filter[0] != '\0' && idStr::FindText( wave, state.filter, false ) < 0 ) continue;
					if ( ImGui::Selectable( wave, !idStr::Icmp( state.playName, wave ) ) ) {
						idStr::Copynz( state.playName, wave, sizeof( state.playName ) );
						idStr::Copynz( state.values.shader, wave, sizeof( state.values.shader ) );
						state.waveBytes = fileSystem->ReadFile( wave, NULL );
						if ( state.autoPlay ) Play( wave );
					}
				}
				ImGui::EndChild();
				if ( state.waveBytes >= 0 ) ImGui::Text( "Wave size: %.2f MB", state.waveBytes / ( 1024.0f * 1024.0f ) );
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

		ImGui::TableNextColumn();
		if ( ImGui::Checkbox( "Only selected group", &state.groupOnly ) ) RefreshMapLists();
		ImGui::SameLine(); if ( ImGui::Button( "Refresh##Speakers" ) ) RefreshMapLists();
		ImGui::SeparatorText( "Groups" );
		ImGui::BeginChild( "SoundGroups", ImVec2( 0, 150 ), ImGuiChildFlags_Borders );
		for ( int index = 0; index < state.groups.Num(); index++ ) if ( ImGui::Selectable( state.groups[index], !idStr::Icmp( state.selectedGroup, state.groups[index] ) ) ) SelectGroup( state.groups[index] );
		ImGui::EndChild();
		ImGui::SeparatorText( "Speakers" );
		ImGui::BeginChild( "SoundSpeakers", ImVec2( 0, 250 ), ImGuiChildFlags_Borders );
		for ( int index = 0; index < state.speakers.Num(); index++ ) if ( ImGui::Selectable( state.speakers[index], !idStr::Icmp( state.selectedSpeaker, state.speakers[index] ) ) ) SelectSpeaker( state.speakers[index] );
		ImGui::EndChild();
		ImGui::SeparatorText( "Move selected" );
		if ( ImGui::Button( "X-" ) ) TranslateSelected( idVec3( -8, 0, 0 ) ); ImGui::SameLine();
		if ( ImGui::Button( "X+" ) ) TranslateSelected( idVec3( 8, 0, 0 ) ); ImGui::SameLine();
		if ( ImGui::Button( "Y-" ) ) TranslateSelected( idVec3( 0, -8, 0 ) ); ImGui::SameLine();
		if ( ImGui::Button( "Y+" ) ) TranslateSelected( idVec3( 0, 8, 0 ) );
		if ( ImGui::Button( "Z-" ) ) TranslateSelected( idVec3( 0, 0, -8 ) ); ImGui::SameLine();
		if ( ImGui::Button( "Z+" ) ) TranslateSelected( idVec3( 0, 0, 8 ) );

		ImGui::TableNextColumn();
		RenderSoundEntityProperties();
		ImGui::EndTable();
	}
	ImGui::End();
}

void SoundEditorImGuiShutdown() {
	SoundEditorImGuiHide();
	state.groups.Clear(); state.speakers.Clear(); state.inUseSounds.Clear(); state.waves.Clear();
}

void SoundEditorInit( const idDict *spawnArgs ) {
	if ( RadiantImGuiWindow() == NULL ) RadiantInit();
	const idDict *dict = spawnArgs;
	if ( spawnArgs != NULL && gameEdit != NULL && spawnArgs->GetString( "name" )[0] != '\0' ) {
		const idDict *mapDict = gameEdit->MapGetEntityDict( spawnArgs->GetString( "name" ) );
		if ( mapDict != NULL ) dict = mapDict;
	}
	SoundEditorImGuiShow( dict != NULL ? dict->GetString( "s_shader" ) : NULL, dict );
	RadiantImGuiFocus();
	idKeyInput::ClearStates();
	com_editors &= ~EDITOR_SOUND;
}

void SoundEditorRun() {
}

void SoundEditorShutdown() {
	SoundEditorImGuiShutdown();
	com_editors &= ~EDITOR_SOUND;
}
