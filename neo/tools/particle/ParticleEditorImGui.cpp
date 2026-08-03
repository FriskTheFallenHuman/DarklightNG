#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../../game/Game.h"
#include "../../framework/DeclParticle.h"
#include "../edit_public.h"
#include "../radiant/RadiantImGui.h"
#include "ParticleEditorImGui.h"

#include "imgui.h"

namespace {

enum particlePreview_t {
	PARTICLE_PREVIEW_TESTMODEL,
	PARTICLE_PREVIEW_IMPACT,
	PARTICLE_PREVIEW_MUZZLE,
	PARTICLE_PREVIEW_FLIGHT,
	PARTICLE_PREVIEW_SELECTED
};

struct ParticleEditorState {
	ParticleEditorState() : open( false ), particle( NULL ), stageIndex( -1 ), dirty( false ), preview( PARTICLE_PREVIEW_TESTMODEL ), mapModified( false ) {
		filter[0] = status[0] = materialName[0] = newName[0] = newFile[0] = emitterName[0] = '\0';
	}
	bool open;
	idDeclParticle *particle;
	int stageIndex;
	bool dirty;
	particlePreview_t preview;
	bool mapModified;
	char filter[256];
	char status[512];
	char materialName[256];
	char newName[256];
	char newFile[512];
	char emitterName[256];
};

static ParticleEditorState state;

static idParticleStage *CurrentStage() {
	if ( state.particle == NULL || state.stageIndex < 0 || state.stageIndex >= state.particle->stages.Num() ) return NULL;
	return state.particle->stages[state.stageIndex];
}

static void LoadStage() {
	idParticleStage *stage = CurrentStage();
	idStr::Copynz( state.materialName, stage != NULL && stage->material != NULL ? stage->material->GetName() : "", sizeof( state.materialName ) );
}

static void SelectParticle( const char *name ) {
	state.particle = name != NULL && name[0] != '\0' ?
		static_cast<idDeclParticle *>( const_cast<idDecl *>( declManager->FindType( DECL_PARTICLE, name, false ) ) ) : NULL;
	state.stageIndex = state.particle != NULL && state.particle->stages.Num() > 0 ? 0 : -1;
	state.dirty = false;
	state.status[0] = '\0';
	LoadStage();
}

static void SetSelectedModel( const char *modelName ) {
	if ( gameEdit == NULL ) return;
	idList<idEntity *> entities;
	entities.SetNum( 128 );
	const int count = gameEdit->GetSelectedEntities( entities.Ptr(), entities.Num() );
	for ( int index = 0; index < count; index++ ) {
		const idDict *dict = gameEdit->EntityGetSpawnArgs( entities[index] );
		if ( dict == NULL ) continue;
		gameEdit->EntitySetModel( entities[index], modelName );
		gameEdit->EntityUpdateVisuals( entities[index] );
		gameEdit->MapSetEntityKeyVal( dict->GetString( "name" ), "model", modelName );
	}
	state.mapModified |= count > 0;
}

static void UpdatePreview() {
	if ( state.particle == NULL ) return;
	idStr particleModel = state.particle->GetName();
	particleModel.SetFileExtension( ".prt" );
	cmdSystem->BufferCommandText( CMD_EXEC_NOW, "testmodel\n" );
	switch ( state.preview ) {
		case PARTICLE_PREVIEW_TESTMODEL:
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, va( "testmodel %s\n", particleModel.c_str() ) );
			break;
		case PARTICLE_PREVIEW_IMPACT:
			cvarSystem->SetCVarInteger( "g_testParticle", TEST_PARTICLE_IMPACT );
			cvarSystem->SetCVarString( "g_testParticleName", particleModel );
			break;
		case PARTICLE_PREVIEW_MUZZLE:
			cvarSystem->SetCVarInteger( "g_testParticle", TEST_PARTICLE_MUZZLE );
			cvarSystem->SetCVarString( "g_testParticleName", particleModel );
			break;
		case PARTICLE_PREVIEW_FLIGHT:
			cvarSystem->SetCVarInteger( "g_testParticle", TEST_PARTICLE_FLIGHT );
			cvarSystem->SetCVarString( "g_testParticleName", particleModel );
			break;
		case PARTICLE_PREVIEW_SELECTED:
			SetSelectedModel( particleModel );
			break;
	}
}

static void MarkChanged() {
	state.dirty = true;
	if ( state.particle != NULL ) state.particle->bounds.Clear();
	UpdatePreview();
}

static bool EditParticleParm( const char *label, idParticleParm &parm ) {
	float range[2] = { parm.from, parm.to };
	if ( !ImGui::DragFloat2( label, range, 0.01f ) ) return false;
	parm.from = range[0];
	parm.to = range[1];
	return true;
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

static void SpawnEmitter() {
	if ( gameEdit == NULL || state.particle == NULL || !gameEdit->PlayerIsValid() || state.emitterName[0] == '\0' ) return;
	idVec3 origin;
	idAngles viewAngles;
	gameEdit->PlayerGetViewAngles( viewAngles );
	gameEdit->PlayerGetEyePosition( origin );
	origin += idAngles( 0, viewAngles.yaw, 0 ).ToForward() * 80.0f + idVec3( 0, 0, 1 );
	idStr model = state.particle->GetName();
	model.SetFileExtension( ".prt" );
	idDict args;
	args.Set( "classname", "func_emitter" );
	args.Set( "name", state.emitterName );
	args.Set( "origin", origin.ToString() );
	args.SetFloat( "angle", viewAngles.yaw + 180.0f );
	args.Set( "model", model );
	idEntity *entity = NULL;
	gameEdit->SpawnEntityDef( args, &entity );
	if ( entity != NULL ) {
		gameEdit->EntityUpdateChangeableSpawnArgs( entity, NULL );
		gameEdit->ClearEntitySelection();
		gameEdit->AddSelectedEntity( entity );
	}
	gameEdit->MapAddEntity( &args );
	state.mapModified = true;
	ImGui::CloseCurrentPopup();
}

static void RenderNewParticlePopup() {
	if ( !ImGui::BeginPopupModal( "New particle", NULL, ImGuiWindowFlags_AlwaysAutoResize ) ) return;
	ImGui::InputText( "Name", state.newName, sizeof( state.newName ) );
	ImGui::InputText( "Particle file", state.newFile, sizeof( state.newFile ) );
	if ( ImGui::Button( "Create", ImVec2( 110, 0 ) ) ) {
		if ( state.newName[0] == '\0' || state.newFile[0] == '\0' ) {
			idStr::Copynz( state.status, "Name and particle file are required.", sizeof( state.status ) );
		} else if ( declManager->FindType( DECL_PARTICLE, state.newName, false ) != NULL ) {
			idStr::Copynz( state.status, "That particle already exists.", sizeof( state.status ) );
		} else {
			idDeclParticle *particle = static_cast<idDeclParticle *>( declManager->CreateNewDecl( DECL_PARTICLE, state.newName, state.newFile ) );
			if ( particle != NULL ) {
				if ( particle->stages.Num() == 0 ) {
					idParticleStage *stage = new idParticleStage;
					stage->Default();
					particle->stages.Append( stage );
				}
				SelectParticle( particle->GetName() );
				state.dirty = true;
				ImGui::CloseCurrentPopup();
			}
		}
	}
	ImGui::SameLine();
	if ( ImGui::Button( "Cancel", ImVec2( 110, 0 ) ) ) ImGui::CloseCurrentPopup();
	ImGui::EndPopup();
}

static void RenderStageEditor() {
	idParticleStage *stage = CurrentStage();
	if ( state.particle == NULL ) {
		ImGui::TextDisabled( "Select a particle declaration." );
		return;
	}

	bool changed = false;
	changed |= ImGui::DragFloat( "Depth hack", &state.particle->depthHack, 0.001f, 0.0f, 1.0f );
	if ( stage == NULL ) {
		ImGui::TextDisabled( "This particle has no stages." );
		return;
	}

	if ( ImGui::BeginTabBar( "ParticleStageTabs" ) ) {
		if ( ImGui::BeginTabItem( "Emission" ) ) {
			changed |= ImGui::Checkbox( "Hidden", &stage->hidden );
			changed |= ImGui::DragInt( "Particle count", &stage->totalParticles, 1.0f, 0, 4096 );
			changed |= ImGui::DragFloat( "Life (seconds)", &stage->particleLife, 0.01f, 0.0f, 60.0f );
			changed |= ImGui::DragFloat( "Cycles", &stage->cycles, 0.05f, 0.0f, 1000.0f );
			changed |= ImGui::SliderFloat( "Spawn bunching", &stage->spawnBunching, 0.0f, 1.0f );
			changed |= ImGui::DragFloat( "Time offset", &stage->timeOffset, 0.01f );
			changed |= ImGui::DragFloat( "Dead time", &stage->deadTime, 0.01f, 0.0f, 60.0f );
			changed |= ImGui::DragFloat( "Gravity", &stage->gravity, 0.01f );
			changed |= ImGui::Checkbox( "World-space gravity", &stage->worldGravity );
			changed |= ImGui::Checkbox( "Random distribution", &stage->randomDistribution );
			changed |= ImGui::Checkbox( "Use entity color", &stage->entityColor );
			changed |= ImGui::DragFloat3( "Origin offset", stage->offset.ToFloatPtr(), 0.1f );
			stage->cycleMsec = (int)( ( stage->particleLife + stage->deadTime ) * 1000.0f );
			ImGui::EndTabItem();
		}
		if ( ImGui::BeginTabItem( "Path" ) ) {
			const char *distributionNames[] = { "Box", "Cylinder", "Sphere" };
			int distribution = (int)stage->distributionType;
			if ( ImGui::Combo( "Distribution", &distribution, distributionNames, 3 ) ) {
				stage->distributionType = (prtDistribution_t)distribution;
				changed = true;
			}
			changed |= ImGui::DragFloat4( "Distribution parameters", stage->distributionParms, 0.05f );
			const char *directionNames[] = { "Cone", "Outward" };
			int direction = (int)stage->directionType;
			if ( ImGui::Combo( "Direction", &direction, directionNames, 2 ) ) {
				stage->directionType = (prtDirection_t)direction;
				changed = true;
			}
			changed |= ImGui::DragFloat4( "Direction parameters", stage->directionParms, 0.05f );
			changed |= EditParticleParm( "Speed (from/to)", stage->speed );
			const char *pathNames[] = { "Standard", "Helix", "Flies", "Orbit", "Drip" };
			int path = (int)stage->customPathType;
			if ( ImGui::Combo( "Custom path", &path, pathNames, 5 ) ) {
				stage->customPathType = (prtCustomPth_t)path;
				changed = true;
			}
			changed |= ImGui::DragFloat4( "Path parameters 1-4", stage->customPathParms, 0.05f );
			changed |= ImGui::DragFloat4( "Path parameters 5-8", stage->customPathParms + 4, 0.05f );
			ImGui::TextWrapped( "%s", stage->GetCustomPathDesc() );
			ImGui::EndTabItem();
		}
		if ( ImGui::BeginTabItem( "Appearance" ) ) {
			if ( ImGui::InputText( "Material", state.materialName, sizeof( state.materialName ), ImGuiInputTextFlags_EnterReturnsTrue ) ) {
				stage->material = declManager->FindMaterial( state.materialName );
				changed = true;
			}
			changed |= ImGui::ColorEdit4( "Color", stage->color.ToFloatPtr() );
			changed |= ImGui::ColorEdit4( "Fade color", stage->fadeColor.ToFloatPtr() );
			changed |= ImGui::SliderFloat( "Fade in", &stage->fadeInFraction, 0.0f, 1.0f );
			changed |= ImGui::SliderFloat( "Fade out", &stage->fadeOutFraction, 0.0f, 1.0f );
			changed |= ImGui::SliderFloat( "Index fade", &stage->fadeIndexFraction, 0.0f, 1.0f );
			changed |= ImGui::DragInt( "Animation frames", &stage->animationFrames, 1.0f, 0, 256 );
			changed |= ImGui::DragFloat( "Animation rate", &stage->animationRate, 0.1f, 0.0f, 240.0f );
			changed |= ImGui::DragFloat( "Initial angle", &stage->initialAngle, 0.25f );
			changed |= EditParticleParm( "Rotation speed", stage->rotationSpeed );
			changed |= EditParticleParm( "Size", stage->size );
			changed |= EditParticleParm( "Aspect", stage->aspect );
			const char *orientationNames[] = { "View", "Aimed", "X axis", "Y axis", "Z axis" };
			int orientation = (int)stage->orientation;
			if ( ImGui::Combo( "Orientation", &orientation, orientationNames, 5 ) ) {
				stage->orientation = (prtOrientation_t)orientation;
				changed = true;
			}
			changed |= ImGui::DragFloat4( "Orientation parameters", stage->orientationParms, 0.05f );
			changed |= ImGui::DragFloat( "Bounds expansion", &stage->boundsExpansion, 0.1f, 0.0f );
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	if ( changed ) MarkChanged();
}

} // namespace

void ParticleEditorImGuiShow( const char *particleName ) {
	state.open = true;
	if ( particleName != NULL && particleName[0] != '\0' ) SelectParticle( particleName );
}

void ParticleEditorImGuiHide() {
	state.open = false;
}

bool ParticleEditorImGuiIsOpen() {
	return state.open;
}

void ParticleEditorImGuiRender() {
	if ( !state.open ) return;
	ImGui::SetNextWindowSize( ImVec2( 1180, 780 ), ImGuiCond_FirstUseEver );
	if ( !ImGui::Begin( "Particle Editor", &state.open, ImGuiWindowFlags_NoCollapse | ( state.dirty ? ImGuiWindowFlags_UnsavedDocument : 0 ) ) ) {
		ImGui::End();
		return;
	}

	if ( ImGui::Button( "New" ) ) {
		state.newName[0] = '\0';
		idStr::Copynz( state.newFile, "particles/generated.prt", sizeof( state.newFile ) );
		ImGui::OpenPopup( "New particle" );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "Save" ) && state.particle != NULL ) {
		if ( state.particle->Save() ) {
			state.dirty = false;
			idStr::snPrintf( state.status, sizeof( state.status ), "Saved %s.", state.particle->GetFileName() );
		} else idStr::Copynz( state.status, "Particle save failed.", sizeof( state.status ) );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "Reload" ) ) {
		idStr name = state.particle != NULL ? state.particle->GetName() : "";
		declManager->Reload( false );
		SelectParticle( name );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "Save particle entities" ) && gameEdit != NULL ) {
		gameEdit->MapSave();
		state.mapModified = false;
	}
	ImGui::SameLine();
	if ( ImGui::Button( "Drop emitter" ) && state.particle != NULL && gameEdit != NULL ) {
		idStr::Copynz( state.emitterName, gameEdit->GetUniqueEntityName( "func_emitter" ), sizeof( state.emitterName ) );
		ImGui::OpenPopup( "Drop particle emitter" );
	}
	if ( state.dirty ) { ImGui::SameLine(); ImGui::TextColored( ImVec4( 1, 0.7f, 0.2f, 1 ), "particle modified" ); }
	if ( state.mapModified ) { ImGui::SameLine(); ImGui::TextColored( ImVec4( 1, 0.7f, 0.2f, 1 ), "map modified" ); }
	if ( state.status[0] != '\0' ) ImGui::TextUnformatted( state.status );

	if ( ImGui::BeginPopupModal( "Drop particle emitter", NULL, ImGuiWindowFlags_AlwaysAutoResize ) ) {
		ImGui::InputText( "Entity name", state.emitterName, sizeof( state.emitterName ) );
		if ( ImGui::Button( "Spawn", ImVec2( 100, 0 ) ) ) SpawnEmitter();
		ImGui::SameLine();
		if ( ImGui::Button( "Cancel", ImVec2( 100, 0 ) ) ) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
	RenderNewParticlePopup();

	if ( ImGui::BeginTable( "ParticleEditorLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV ) ) {
		ImGui::TableSetupColumn( "Particles", ImGuiTableColumnFlags_WidthFixed, 260 );
		ImGui::TableSetupColumn( "Stages", ImGuiTableColumnFlags_WidthFixed, 175 );
		ImGui::TableSetupColumn( "Properties", ImGuiTableColumnFlags_WidthStretch );
		ImGui::TableNextColumn();
		ImGui::InputTextWithHint( "##ParticleFilter", "filter particles", state.filter, sizeof( state.filter ) );
		ImGui::BeginChild( "ParticleList", ImVec2( 0, -1 ), ImGuiChildFlags_Borders );
		for ( int index = 0; index < declManager->GetNumDecls( DECL_PARTICLE ); index++ ) {
			const idDecl *decl = declManager->DeclByIndex( DECL_PARTICLE, index, false );
			if ( decl == NULL || ( state.filter[0] != '\0' && idStr::FindText( decl->GetName(), state.filter, false ) < 0 ) ) continue;
			if ( ImGui::Selectable( decl->GetName(), state.particle == decl ) ) {
				SelectParticle( decl->GetName() );
				UpdatePreview();
			}
		}
		ImGui::EndChild();

		ImGui::TableNextColumn();
		if ( ImGui::Button( "Add stage" ) && state.particle != NULL ) {
			idParticleStage *stage = new idParticleStage;
			stage->Default();
			state.stageIndex = state.particle->stages.Append( stage );
			LoadStage();
			MarkChanged();
		}
		ImGui::SameLine();
		if ( ImGui::Button( "Copy" ) && CurrentStage() != NULL ) {
			idParticleStage *copy = new idParticleStage;
			*copy = *CurrentStage();
			state.stageIndex = state.particle->stages.Append( copy );
			LoadStage();
			MarkChanged();
		}
		if ( ImGui::Button( "Remove stage" ) && CurrentStage() != NULL ) ImGui::OpenPopup( "Remove particle stage" );
		if ( ImGui::BeginPopupModal( "Remove particle stage", NULL, ImGuiWindowFlags_AlwaysAutoResize ) ) {
			ImGui::TextUnformatted( "Remove the selected particle stage?" );
			if ( ImGui::Button( "Remove", ImVec2( 100, 0 ) ) ) {
				delete state.particle->stages[state.stageIndex];
				state.particle->stages.RemoveIndex( state.stageIndex );
				state.stageIndex = Min( state.stageIndex, state.particle->stages.Num() - 1 );
				LoadStage();
				MarkChanged();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if ( ImGui::Button( "Cancel", ImVec2( 100, 0 ) ) ) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
		ImGui::BeginChild( "ParticleStages", ImVec2( 0, 260 ), ImGuiChildFlags_Borders );
		if ( state.particle != NULL ) {
			for ( int index = 0; index < state.particle->stages.Num(); index++ ) {
				idParticleStage *stage = state.particle->stages[index];
				if ( ImGui::Selectable( va( "Stage %d%s", index, stage->hidden ? " (hidden)" : "" ), index == state.stageIndex ) ) {
					state.stageIndex = index;
					LoadStage();
				}
			}
		}
		ImGui::EndChild();
		ImGui::SeparatorText( "Preview" );
		const char *previewNames[] = { "Test model", "Impact", "Muzzle", "Flight", "Selected entities" };
		int preview = (int)state.preview;
		if ( ImGui::Combo( "Mode", &preview, previewNames, 5 ) ) {
			state.preview = (particlePreview_t)preview;
			UpdatePreview();
		}
		if ( ImGui::Button( "Refresh preview" ) ) UpdatePreview();
		ImGui::SeparatorText( "Move selected" );
		if ( ImGui::Button( "X-" ) ) TranslateSelected( idVec3( -8, 0, 0 ) ); ImGui::SameLine();
		if ( ImGui::Button( "X+" ) ) TranslateSelected( idVec3( 8, 0, 0 ) ); ImGui::SameLine();
		if ( ImGui::Button( "Y-" ) ) TranslateSelected( idVec3( 0, -8, 0 ) ); ImGui::SameLine();
		if ( ImGui::Button( "Y+" ) ) TranslateSelected( idVec3( 0, 8, 0 ) );
		if ( ImGui::Button( "Z-" ) ) TranslateSelected( idVec3( 0, 0, -8 ) ); ImGui::SameLine();
		if ( ImGui::Button( "Z+" ) ) TranslateSelected( idVec3( 0, 0, 8 ) );

		ImGui::TableNextColumn();
		RenderStageEditor();
		ImGui::EndTable();
	}
	ImGui::End();
}

void ParticleEditorImGuiShutdown() {
	state.open = false;
	state.particle = NULL;
	state.stageIndex = -1;
}

void ParticleEditorInit( const idDict *spawnArgs ) {
	if ( RadiantImGuiWindow() == NULL ) RadiantInit();
	idStr particleName = spawnArgs != NULL ? spawnArgs->GetString( "model" ) : "";
	particleName.StripFileExtension();
	ParticleEditorImGuiShow( particleName );
	RadiantImGuiFocus();
	cvarSystem->SetCVarBool( "r_useCachedDynamicModels", false );
	com_editors &= ~EDITOR_PARTICLE;
}

void ParticleEditorRun() {
}

void ParticleEditorShutdown() {
	ParticleEditorImGuiShutdown();
	com_editors &= ~EDITOR_PARTICLE;
}
