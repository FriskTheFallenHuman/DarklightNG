#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../../framework/DeclAF.h"
#include "../edit_public.h"
#include "../radiant/RadiantImGui.h"
#include "AFEditorImGui.h"

#include "imgui.h"

namespace {

struct AFEditorState {
	AFEditorState() : open( false ), af( NULL ), bodyIndex( -1 ), constraintIndex( -1 ), dirty( false ), savedGravity( 1066.0f ) {
		filter[0] = status[0] = newName[0] = newFile[0] = renameName[0] = '\0';
	}
	bool open;
	idDeclAF *af;
	int bodyIndex;
	int constraintIndex;
	bool dirty;
	float savedGravity;
	char filter[256];
	char status[512];
	char newName[256];
	char newFile[512];
	char renameName[256];
};

static AFEditorState state;

static bool EditString( const char *label, idStr &value, int size = 512 ) {
	char buffer[1024];
	size = Min( size, (int)sizeof( buffer ) );
	idStr::Copynz( buffer, value.c_str(), size );
	if ( ImGui::InputText( label, buffer, size ) ) {
		value = buffer;
		return true;
	}
	return false;
}

static void Changed() {
	if ( state.af != NULL ) state.af->modified = true;
	state.dirty = true;
	state.status[0] = '\0';
}

static void SelectAF( const char *name ) {
	state.af = name != NULL && name[0] != '\0' ? static_cast<idDeclAF *>( const_cast<idDecl *>( declManager->FindType( DECL_AF, name, false ) ) ) : NULL;
	state.bodyIndex = state.af != NULL && state.af->bodies.Num() > 0 ? 0 : -1;
	state.constraintIndex = state.af != NULL && state.af->constraints.Num() > 0 ? 0 : -1;
	state.dirty = state.af != NULL && state.af->modified;
	state.status[0] = '\0';
}

static idDeclAF_Body *CurrentBody() {
	return state.af != NULL && state.bodyIndex >= 0 && state.bodyIndex < state.af->bodies.Num() ? state.af->bodies[state.bodyIndex] : NULL;
}

static idDeclAF_Constraint *CurrentConstraint() {
	return state.af != NULL && state.constraintIndex >= 0 && state.constraintIndex < state.af->constraints.Num() ? state.af->constraints[state.constraintIndex] : NULL;
}

static bool RenderAFVector( const char *label, idAFVector &value ) {
	bool changed = false;
	ImGui::PushID( label );
	ImGui::SeparatorText( label );
	const char *types[] = { "Coordinates", "Joint", "Bone center", "Bone direction" };
	int type = value.type;
	if ( ImGui::Combo( "Type", &type, types, 4 ) ) {
		value.type = static_cast<decltype( value.type )>( type );
		changed = true;
	}
	if ( value.type == idAFVector::VEC_COORDS ) {
		changed |= ImGui::DragFloat3( "XYZ", value.ToVec3().ToFloatPtr(), 0.1f );
	} else {
		changed |= EditString( "Joint 1", value.joint1, 256 );
		if ( value.type == idAFVector::VEC_BONECENTER || value.type == idAFVector::VEC_BONEDIR ) changed |= EditString( "Joint 2", value.joint2, 256 );
	}
	ImGui::PopID();
	return changed;
}

static void RenderProperties() {
	if ( state.af == NULL ) {
		ImGui::TextDisabled( "Select an articulated figure." );
		return;
	}
	bool changed = false;
	changed |= EditString( "Model", state.af->model );
	changed |= EditString( "Skin", state.af->skin );
	changed |= ImGui::DragFloat( "Total mass", &state.af->totalMass, 0.1f, -1.0f, 100000.0f );
	changed |= ImGui::Checkbox( "Self collision", &state.af->selfCollision );
	changed |= ImGui::InputInt( "Contents bits", &state.af->contents );
	changed |= ImGui::InputInt( "Clip mask bits", &state.af->clipMask );

	if ( ImGui::CollapsingHeader( "Default friction", ImGuiTreeNodeFlags_DefaultOpen ) ) {
		changed |= ImGui::DragFloat( "Linear", &state.af->defaultLinearFriction, 0.01f, 0.0f, 100.0f );
		changed |= ImGui::DragFloat( "Angular", &state.af->defaultAngularFriction, 0.01f, 0.0f, 100.0f );
		changed |= ImGui::DragFloat( "Contact", &state.af->defaultContactFriction, 0.01f, 0.0f, 100.0f );
		changed |= ImGui::DragFloat( "Constraint", &state.af->defaultConstraintFriction, 0.01f, 0.0f, 100.0f );
		if ( ImGui::Button( "Apply defaults to all bodies" ) ) {
			for ( int i = 0; i < state.af->bodies.Num(); i++ ) {
				state.af->bodies[i]->linearFriction = state.af->defaultLinearFriction;
				state.af->bodies[i]->angularFriction = state.af->defaultAngularFriction;
				state.af->bodies[i]->contactFriction = state.af->defaultContactFriction;
			}
			changed = true;
		}
	}
	if ( ImGui::CollapsingHeader( "Rest and suspension", ImGuiTreeNodeFlags_DefaultOpen ) ) {
		changed |= ImGui::DragFloat2( "Suspend velocity", state.af->suspendVelocity.ToFloatPtr(), 0.01f );
		changed |= ImGui::DragFloat2( "Suspend acceleration", state.af->suspendAcceleration.ToFloatPtr(), 0.01f );
		changed |= ImGui::DragFloat( "No move time", &state.af->noMoveTime, 0.01f );
		changed |= ImGui::DragFloat( "No move translation", &state.af->noMoveTranslation, 0.01f );
		changed |= ImGui::DragFloat( "No move rotation", &state.af->noMoveRotation, 0.01f );
		changed |= ImGui::DragFloat( "Minimum move time", &state.af->minMoveTime, 0.01f );
		changed |= ImGui::DragFloat( "Maximum move time", &state.af->maxMoveTime, 0.01f );
	}
	if ( changed ) Changed();
}

static void RenderBody() {
	idDeclAF_Body *body = CurrentBody();
	if ( body == NULL ) {
		ImGui::TextDisabled( "Select or create a body." );
		return;
	}
	bool changed = false;
	idStr oldBodyName = body->name;
	if ( EditString( "Name", body->name, 256 ) ) {
		// Keep body references in constraints synchronized exactly as the old rename dialog did.
		idStr newBodyName = body->name;
		body->name = oldBodyName;
		state.af->RenameBody( oldBodyName, newBodyName );
		changed = true;
	}
	changed |= EditString( "Joint", body->jointName, 256 );
	const char *jointMods[] = { "Axis", "Origin", "Axis and origin" };
	int jointMod = body->jointMod;
	if ( ImGui::Combo( "Joint modification", &jointMod, jointMods, 3 ) ) { body->jointMod = (declAFJointMod_t)jointMod; changed = true; }
	const char *models[] = { "Box", "Octahedron", "Dodecahedron", "Cylinder", "Cone", "Bone" };
	const int modelValues[] = { TRM_BOX, TRM_OCTAHEDRON, TRM_DODECAHEDRON, TRM_CYLINDER, TRM_CONE, TRM_BONE };
	int modelChoice = 0;
	for ( int i = 0; i < 6; i++ ) if ( body->modelType == modelValues[i] ) modelChoice = i;
	if ( ImGui::Combo( "Collision model", &modelChoice, models, 6 ) ) { body->modelType = modelValues[modelChoice]; changed = true; }
	changed |= RenderAFVector( "Model point 1", body->v1 );
	changed |= RenderAFVector( "Model point 2", body->v2 );
	changed |= ImGui::InputInt( "Sides", &body->numSides );
	changed |= ImGui::DragFloat( "Width", &body->width, 0.1f );
	changed |= ImGui::DragFloat( "Density", &body->density, 0.01f, 0.0f, 100000.0f );
	changed |= RenderAFVector( "Origin", body->origin );
	changed |= ImGui::DragFloat3( "Angles", body->angles.ToFloatPtr(), 0.1f );
	changed |= ImGui::InputInt( "Body contents bits", &body->contents );
	changed |= ImGui::InputInt( "Body clip mask bits", &body->clipMask );
	changed |= ImGui::Checkbox( "Body self collision", &body->selfCollision );
	changed |= ImGui::DragFloat3( "Inertia X", body->inertiaScale[0].ToFloatPtr(), 0.01f );
	changed |= ImGui::DragFloat3( "Inertia Y", body->inertiaScale[1].ToFloatPtr(), 0.01f );
	changed |= ImGui::DragFloat3( "Inertia Z", body->inertiaScale[2].ToFloatPtr(), 0.01f );
	changed |= ImGui::DragFloat( "Linear friction", &body->linearFriction, 0.01f, 0.0f, 100.0f );
	changed |= ImGui::DragFloat( "Angular friction", &body->angularFriction, 0.01f, 0.0f, 100.0f );
	changed |= ImGui::DragFloat( "Contact friction", &body->contactFriction, 0.01f, 0.0f, 100.0f );
	changed |= EditString( "Contained joints", body->containedJoints );
	changed |= RenderAFVector( "Friction direction", body->frictionDirection );
	changed |= RenderAFVector( "Contact motor direction", body->contactMotorDirection );
	if ( changed ) Changed();
}

static void RenderConstraint() {
	idDeclAF_Constraint *constraint = CurrentConstraint();
	if ( constraint == NULL ) {
		ImGui::TextDisabled( "Select or create a constraint." );
		return;
	}
	bool changed = false;
	idStr oldConstraintName = constraint->name;
	if ( EditString( "Name", constraint->name, 256 ) ) {
		idStr newConstraintName = constraint->name;
		constraint->name = oldConstraintName;
		state.af->RenameConstraint( oldConstraintName, newConstraintName );
		changed = true;
	}
	changed |= EditString( "Body 1", constraint->body1, 256 );
	changed |= EditString( "Body 2", constraint->body2, 256 );
	const char *types[] = { "Invalid", "Fixed", "Ball and socket", "Universal", "Hinge", "Slider", "Spring" };
	int type = constraint->type;
	if ( ImGui::Combo( "Type", &type, types, 7 ) ) { constraint->type = (declAFConstraintType_t)type; changed = true; }
	changed |= ImGui::DragFloat( "Friction", &constraint->friction, 0.01f, 0.0f, 100.0f );
	if ( constraint->type != DECLAF_CONSTRAINT_FIXED ) changed |= RenderAFVector( "Anchor", constraint->anchor );
	if ( constraint->type == DECLAF_CONSTRAINT_UNIVERSALJOINT ) {
		changed |= RenderAFVector( "Shaft 1", constraint->shaft[0] );
		changed |= RenderAFVector( "Shaft 2", constraint->shaft[1] );
	}
	if ( constraint->type == DECLAF_CONSTRAINT_HINGE || constraint->type == DECLAF_CONSTRAINT_SLIDER ) changed |= RenderAFVector( "Axis", constraint->axis );
	if ( constraint->type == DECLAF_CONSTRAINT_SPRING ) {
		changed |= RenderAFVector( "Anchor 2", constraint->anchor2 );
		changed |= ImGui::DragFloat( "Stretch", &constraint->stretch, 0.01f );
		changed |= ImGui::DragFloat( "Compress", &constraint->compress, 0.01f );
		changed |= ImGui::DragFloat( "Damping", &constraint->damping, 0.01f );
		changed |= ImGui::DragFloat( "Rest length", &constraint->restLength, 0.01f );
		changed |= ImGui::DragFloat( "Minimum length", &constraint->minLength, 0.01f );
		changed |= ImGui::DragFloat( "Maximum length", &constraint->maxLength, 0.01f );
	}
	if ( constraint->type == DECLAF_CONSTRAINT_BALLANDSOCKETJOINT || constraint->type == DECLAF_CONSTRAINT_UNIVERSALJOINT || constraint->type == DECLAF_CONSTRAINT_HINGE ) {
		const char *limits[] = { "None", "Cone", "Pyramid" };
		int limit = constraint->limit + 1;
		if ( ImGui::Combo( "Limit", &limit, limits, 3 ) ) {
			if ( limit == 0 ) constraint->limit = idDeclAF_Constraint::LIMIT_NONE;
			else if ( limit == 1 ) constraint->limit = idDeclAF_Constraint::LIMIT_CONE;
			else constraint->limit = idDeclAF_Constraint::LIMIT_PYRAMID;
			changed = true;
		}
		if ( constraint->limit != idDeclAF_Constraint::LIMIT_NONE ) {
			changed |= RenderAFVector( "Limit axis", constraint->limitAxis );
			changed |= ImGui::DragFloat3( "Limit angles", constraint->limitAngles, 0.1f );
		}
	}
	if ( changed ) Changed();
}

static void RenderBodyList() {
	if ( state.af == NULL ) return;
	if ( ImGui::Button( "Add body" ) ) ImGui::OpenPopup( "Add AF body" );
	ImGui::SameLine();
	if ( ImGui::Button( "Delete body" ) && CurrentBody() != NULL ) ImGui::OpenPopup( "Delete AF body" );
	if ( ImGui::BeginPopup( "Add AF body" ) ) {
		ImGui::InputText( "Name", state.newName, sizeof( state.newName ) );
		if ( ImGui::Button( "Add" ) && state.newName[0] != '\0' ) {
			state.af->NewBody( state.newName );
			state.bodyIndex = state.af->bodies.Num() - 1;
			state.newName[0] = '\0'; Changed(); ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	if ( ImGui::BeginPopupModal( "Delete AF body", NULL, ImGuiWindowFlags_AlwaysAutoResize ) ) {
		ImGui::Text( "Delete body '%s' and constraints which reference it?", CurrentBody() != NULL ? CurrentBody()->name.c_str() : "" );
		if ( ImGui::Button( "Delete" ) && CurrentBody() != NULL ) {
			idStr name = CurrentBody()->name;
			state.af->DeleteBody( name ); state.bodyIndex = Min( state.bodyIndex, state.af->bodies.Num() - 1 ); Changed(); ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine(); if ( ImGui::Button( "Cancel" ) ) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
	ImGui::BeginChild( "AFBodies", ImVec2( 0, 190 ), ImGuiChildFlags_Borders );
	for ( int i = 0; i < state.af->bodies.Num(); i++ ) if ( ImGui::Selectable( state.af->bodies[i]->name, i == state.bodyIndex ) ) { state.bodyIndex = i; cvarSystem->SetCVarString( "af_highlightBody", state.af->bodies[i]->name ); }
	ImGui::EndChild();
}

static void RenderConstraintList() {
	if ( state.af == NULL ) return;
	if ( ImGui::Button( "Add constraint" ) ) ImGui::OpenPopup( "Add AF constraint" );
	ImGui::SameLine();
	if ( ImGui::Button( "Delete constraint" ) && CurrentConstraint() != NULL ) ImGui::OpenPopup( "Delete AF constraint" );
	if ( ImGui::BeginPopup( "Add AF constraint" ) ) {
		ImGui::InputText( "Name", state.newName, sizeof( state.newName ) );
		if ( ImGui::Button( "Add" ) && state.newName[0] != '\0' ) {
			state.af->NewConstraint( state.newName ); state.constraintIndex = state.af->constraints.Num() - 1;
			state.newName[0] = '\0'; Changed(); ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	if ( ImGui::BeginPopupModal( "Delete AF constraint", NULL, ImGuiWindowFlags_AlwaysAutoResize ) ) {
		ImGui::Text( "Delete constraint '%s'?", CurrentConstraint() != NULL ? CurrentConstraint()->name.c_str() : "" );
		if ( ImGui::Button( "Delete" ) && CurrentConstraint() != NULL ) {
			idStr name = CurrentConstraint()->name; state.af->DeleteConstraint( name ); state.constraintIndex = Min( state.constraintIndex, state.af->constraints.Num() - 1 ); Changed(); ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine(); if ( ImGui::Button( "Cancel" ) ) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
	ImGui::BeginChild( "AFConstraints", ImVec2( 0, 190 ), ImGuiChildFlags_Borders );
	for ( int i = 0; i < state.af->constraints.Num(); i++ ) if ( ImGui::Selectable( state.af->constraints[i]->name, i == state.constraintIndex ) ) { state.constraintIndex = i; cvarSystem->SetCVarString( "af_highlightConstraint", state.af->constraints[i]->name ); }
	ImGui::EndChild();
}

static void CVarCheckbox( const char *label, const char *name ) {
	bool value = cvarSystem->GetCVarBool( name );
	if ( ImGui::Checkbox( label, &value ) ) cvarSystem->SetCVarBool( name, value );
}

static void RenderViewOptions() {
	ImGui::SeparatorText( "Articulated figure display" );
	CVarCheckbox( "Bodies", "af_showBodies" ); ImGui::SameLine(); CVarCheckbox( "Body names", "af_showBodyNames" ); ImGui::SameLine(); CVarCheckbox( "Body mass", "af_showMass" );
	CVarCheckbox( "Total mass", "af_showTotalMass" ); ImGui::SameLine(); CVarCheckbox( "Inertia tensors", "af_showInertia" ); ImGui::SameLine(); CVarCheckbox( "Velocities", "af_showVelocity" );
	CVarCheckbox( "Constraints", "af_showConstraints" ); ImGui::SameLine(); CVarCheckbox( "Constraint names", "af_showConstraintNames" ); ImGui::SameLine(); CVarCheckbox( "Primary only", "af_showPrimaryOnly" );
	CVarCheckbox( "Limits", "af_showLimits" ); ImGui::SameLine(); CVarCheckbox( "Constrained bodies", "af_showConstrainedBodies" ); ImGui::SameLine(); CVarCheckbox( "Trees", "af_showTrees" );
	int skeleton = cvarSystem->GetCVarInteger( "r_showSkel" );
	const char *skeletonModes[] = { "Off", "Model and skeleton", "Skeleton only" };
	if ( ImGui::Combo( "MD5 skeleton", &skeleton, skeletonModes, 3 ) ) cvarSystem->SetCVarInteger( "r_showSkel", skeleton );
	ImGui::SeparatorText( "Debug rendering" );
	CVarCheckbox( "Depth-test debug lines", "r_debugLineDepthTest" );
	bool arrows = cvarSystem->GetCVarInteger( "r_debugArrowStep" ) != 0;
	if ( ImGui::Checkbox( "Use arrows", &arrows ) ) cvarSystem->SetCVarInteger( "r_debugArrowStep", arrows ? 120 : 0 );
	ImGui::SeparatorText( "Physics overrides" );
	CVarCheckbox( "Disable friction", "af_skipFriction" ); ImGui::SameLine(); CVarCheckbox( "Disable limits", "af_skipLimits" ); ImGui::SameLine(); CVarCheckbox( "Disable self collision", "af_skipSelfCollision" );
	float gravity = cvarSystem->GetCVarFloat( "g_gravity" );
	if ( gravity != 0.0f ) state.savedGravity = gravity;
	bool noGravity = gravity == 0.0f;
	if ( ImGui::Checkbox( "Disable gravity", &noGravity ) ) cvarSystem->SetCVarFloat( "g_gravity", noGravity ? 0.0f : state.savedGravity );
	CVarCheckbox( "Show timings", "af_showTimings" ); ImGui::SameLine(); CVarCheckbox( "Drag entities", "g_dragEntity" ); ImGui::SameLine(); CVarCheckbox( "Show drag selection", "g_dragShowSelection" );
}

} // namespace

void AFEditorImGuiShow( const char *afName ) {
	state.open = true;
	if ( afName != NULL && afName[0] != '\0' ) SelectAF( afName );
}

void AFEditorImGuiHide() { state.open = false; }
bool AFEditorImGuiIsOpen() { return state.open; }

void AFEditorImGuiRender() {
	if ( !state.open ) return;
	ImGui::SetNextWindowSize( ImVec2( 1180, 760 ), ImGuiCond_FirstUseEver );
	if ( !ImGui::Begin( state.dirty ? "Articulated Figure Editor*" : "Articulated Figure Editor", &state.open, ImGuiWindowFlags_MenuBar ) ) { ImGui::End(); return; }
	if ( ImGui::BeginMenuBar() ) {
		if ( ImGui::BeginMenu( "File" ) ) {
			if ( ImGui::MenuItem( "New..." ) ) ImGui::OpenPopup( "New articulated figure" );
			if ( ImGui::MenuItem( "Save", "Ctrl+S", false, state.af != NULL ) ) {
				if ( state.af->Save() ) { state.dirty = false; idStr::Copynz( state.status, "Saved", sizeof( state.status ) ); }
				else idStr::Copynz( state.status, "Save failed (file may be read-only)", sizeof( state.status ) );
			}
			if ( ImGui::MenuItem( "Reload", NULL, false, state.af != NULL ) ) { idStr name = state.af->GetName(); state.af->Invalidate(); SelectAF( name ); }
			ImGui::Separator();
			if ( ImGui::MenuItem( "Close" ) ) state.open = false;
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
	if ( ImGui::BeginPopup( "New articulated figure" ) ) {
		ImGui::InputText( "Name", state.newName, sizeof( state.newName ) );
		ImGui::InputText( "AF file", state.newFile, sizeof( state.newFile ) );
		if ( ImGui::Button( "Create" ) && state.newName[0] != '\0' && state.newFile[0] != '\0' ) {
			SelectAF( declManager->CreateNewDecl( DECL_AF, state.newName, state.newFile )->GetName() ); Changed(); state.newName[0] = state.newFile[0] = '\0'; ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	if ( ImGui::Button( "Save" ) && state.af != NULL ) { if ( state.af->Save() ) state.dirty = false; }
	ImGui::SameLine(); if ( ImGui::Button( "Spawn" ) && state.af != NULL && gameEdit != NULL ) gameEdit->AF_SpawnEntity( state.af->GetName() );
	ImGui::SameLine(); if ( ImGui::Button( "Update entities / T-pose" ) && state.af != NULL && gameEdit != NULL ) gameEdit->AF_UpdateEntities( state.af->GetName() );
	ImGui::SameLine(); if ( ImGui::Button( "Delete selected entity" ) ) cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "deleteSelected\n" );
	if ( state.status[0] != '\0' ) { ImGui::SameLine(); ImGui::TextUnformatted( state.status ); }

	if ( ImGui::BeginTable( "AFLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV ) ) {
		ImGui::TableSetupColumn( "Browser", ImGuiTableColumnFlags_WidthFixed, 300.0f );
		ImGui::TableSetupColumn( "Editor", ImGuiTableColumnFlags_WidthStretch );
		ImGui::TableNextColumn();
		ImGui::InputTextWithHint( "##AFFilter", "Filter articulated figures", state.filter, sizeof( state.filter ) );
		ImGui::BeginChild( "AFDeclarations", ImVec2( 0, 220 ), ImGuiChildFlags_Borders );
		for ( int i = 0; i < declManager->GetNumDecls( DECL_AF ); i++ ) {
			const idDecl *decl = declManager->DeclByIndex( DECL_AF, i, false );
			if ( state.filter[0] != '\0' && idStr::FindText( decl->GetName(), state.filter, false ) < 0 ) continue;
			if ( ImGui::Selectable( decl->GetName(), state.af != NULL && !idStr::Icmp( state.af->GetName(), decl->GetName() ) ) ) SelectAF( decl->GetName() );
		}
		ImGui::EndChild();
		if ( ImGui::BeginTabBar( "AFItems" ) ) {
			if ( ImGui::BeginTabItem( "Bodies" ) ) { RenderBodyList(); ImGui::EndTabItem(); }
			if ( ImGui::BeginTabItem( "Constraints" ) ) { RenderConstraintList(); ImGui::EndTabItem(); }
			ImGui::EndTabBar();
		}
		ImGui::TableNextColumn();
		if ( ImGui::BeginTabBar( "AFEditorTabs" ) ) {
			if ( ImGui::BeginTabItem( "Properties" ) ) { ImGui::BeginChild( "AFPropertiesScroll" ); RenderProperties(); ImGui::EndChild(); ImGui::EndTabItem(); }
			if ( ImGui::BeginTabItem( "Body" ) ) { ImGui::BeginChild( "AFBodyScroll" ); RenderBody(); ImGui::EndChild(); ImGui::EndTabItem(); }
			if ( ImGui::BeginTabItem( "Constraint" ) ) { ImGui::BeginChild( "AFConstraintScroll" ); RenderConstraint(); ImGui::EndChild(); ImGui::EndTabItem(); }
			if ( ImGui::BeginTabItem( "View / Physics" ) ) { ImGui::BeginChild( "AFViewScroll" ); RenderViewOptions(); ImGui::EndChild(); ImGui::EndTabItem(); }
			ImGui::EndTabBar();
		}
		ImGui::EndTable();
	}
	ImGui::End();
}

void AFEditorImGuiShutdown() { state.open = false; state.af = NULL; }

void AFEditorInit( const idDict *spawnArgs ) {
	if ( RadiantImGuiWindow() == NULL ) RadiantInit();
	const char *name = spawnArgs != NULL ? spawnArgs->GetString( "articulatedFigure" ) : NULL;
	if ( ( name == NULL || name[0] == '\0' ) && spawnArgs != NULL ) name = spawnArgs->GetString( "ragdoll" );
	AFEditorImGuiShow( name ); RadiantImGuiFocus(); idKeyInput::ClearStates(); com_editors &= ~EDITOR_AF;
}

void AFEditorRun() {}
void AFEditorShutdown() { AFEditorImGuiShutdown(); com_editors &= ~EDITOR_AF; }
