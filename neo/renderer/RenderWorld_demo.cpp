/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).  

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/
#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"

//#define WRITE_GUIS

typedef struct {
	int		version;
	int		sizeofRenderEntity;
	int		sizeofRenderLight;
	char	mapname[256];
} demoHeader_t;


/*
==============
StartWritingDemo
==============
*/
void		idRenderWorldLocal::StartWritingDemo( idDemoFile *demo ) {
	int		i;

	// FIXME: we should track the idDemoFile locally, instead of snooping into session for it

	WriteLoadMap();

	// write the door portal state
	for ( i = 0 ; i < numInterAreaPortals ; i++ ) {
		if ( doublePortals[i].blockingBits ) {
			SetPortalState( i+1, doublePortals[i].blockingBits );
		}
	}

	// clear the archive counter on all defs
	for ( i = 0 ; i < lightDefs.Num() ; i++ ) {
		if ( lightDefs[i] ) {
			lightDefs[i]->archived = false;
		}
	}
	for ( i = 0 ; i < entityDefs.Num() ; i++ ) {
		if ( entityDefs[i] ) {
			entityDefs[i]->archived = false;
		}
	}
}

void idRenderWorldLocal::StopWritingDemo() {
//	writeDemo = NULL;
}

/*
==============
ProcessDemoCommand
==============
*/
bool		idRenderWorldLocal::ProcessDemoCommand( idDemoFile *readDemo, renderView_t *renderView, int *demoTimeOffset ) {
	bool	newMap = false;
	
	if ( !readDemo ) {
		return false;
	}

	demoCommand_t	dc;
	qhandle_t		h;

	if ( !readDemo->ReadInt( (int&)dc ) ) {
		// a demoShot may not have an endFrame, but it is still valid
		return false;
	}

	switch( dc ) {
	case DC_LOADMAP:
		// read the initial data
		demoHeader_t	header;

		readDemo->ReadInt( header.version );
		readDemo->ReadInt( header.sizeofRenderEntity );
		readDemo->ReadInt( header.sizeofRenderLight );
		for ( int i = 0; i < 256; i++ )
			readDemo->ReadChar( header.mapname[i] );
		// the internal version value got replaced by DS_VERSION at toplevel
		if ( header.version != 4 ) {
				common->Error( "Demo version mismatch.\n" );
		}

		if ( r_showDemo.GetBool() ) {
			common->Printf( "DC_LOADMAP: %s\n", header.mapname );
		}
		InitFromMap( header.mapname );

		newMap = true;		// we will need to set demoTimeOffset

		break;

	case DC_RENDERVIEW:
		readDemo->ReadInt( renderView->viewID );
		readDemo->ReadInt( renderView->x );
		readDemo->ReadInt( renderView->y );
		readDemo->ReadInt( renderView->width );
		readDemo->ReadInt( renderView->height );
		readDemo->ReadFloat( renderView->fov_x );
		readDemo->ReadFloat( renderView->fov_y );
		readDemo->ReadVec3( renderView->vieworg );
		readDemo->ReadMat3( renderView->viewaxis );
		readDemo->ReadBool( renderView->cramZNear );
		readDemo->ReadBool( renderView->forceUpdate );
		// binary compatibility with win32 padded structures
		char tmp;
		readDemo->ReadChar( tmp );
		readDemo->ReadChar( tmp );
		readDemo->ReadInt( renderView->time );
		for ( int i = 0; i < MAX_GLOBAL_SHADER_PARMS; i++ )
			readDemo->ReadFloat( renderView->shaderParms[i] );

		if ( !readDemo->ReadInt( (int&)renderView->globalMaterial ) ) {
			 return false;
		 }
												 
		if ( r_showDemo.GetBool() ) {
			common->Printf( "DC_RENDERVIEW: %i\n", renderView->time );
		}

		// possibly change the time offset if this is from a new map
		if ( newMap && demoTimeOffset ) {
			*demoTimeOffset = renderView->time - eventLoop->Milliseconds();
		}
		return false;

	case DC_UPDATE_ENTITYDEF:
		ReadRenderEntity();
		break;
	case DC_DELETE_ENTITYDEF:
		if ( !readDemo->ReadInt( h ) ) {
			return false;
		}
		if ( r_showDemo.GetBool() ) {
			common->Printf( "DC_DELETE_ENTITYDEF: %i\n", h );
		}
		if ( h >= 0 && h < entityDefs.Num() && entityDefs[h] != NULL ) {
			FreeRenderEntity( entityDefs[h] );
		}
		break;
	case DC_UPDATE_LIGHTDEF:
		ReadRenderLight();
		break;
	case DC_DELETE_LIGHTDEF:
		if ( !readDemo->ReadInt( h ) ) {
			return false;
		}
		if ( r_showDemo.GetBool() ) {
			common->Printf( "DC_DELETE_LIGHTDEF: %i\n", h );
		}
		if ( h >= 0 && h < lightDefs.Num() && lightDefs[h] != NULL ) {
			FreeRenderLight( lightDefs[h] );
		}
		break;

	case DC_CAPTURE_RENDER:
		if ( r_showDemo.GetBool() ) {
			common->Printf( "DC_CAPTURE_RENDER\n" );
		}
		renderSystem->CaptureRenderToImage( readDemo->ReadHashString() );
		break;

	case DC_CROP_RENDER:
		if ( r_showDemo.GetBool() ) {
			common->Printf( "DC_CROP_RENDER\n" );
		}
		int	size[3];
		readDemo->ReadInt( size[0] );
		readDemo->ReadInt( size[1] );
		readDemo->ReadInt( size[2] );
		renderSystem->CropRenderSize( size[0], size[1], size[2] != 0 );
		break;

	case DC_UNCROP_RENDER:
		if ( r_showDemo.GetBool() ) {
			common->Printf( "DC_UNCROP\n" );
		}
		renderSystem->UnCrop();
		break;

	case DC_GUI_MODEL:
		if ( r_showDemo.GetBool() ) {
			common->Printf( "DC_GUI_MODEL\n" );
		}
		tr.demoGuiModel->ReadFromDemo( readDemo );
		break;

	case DC_DEFINE_MODEL:
		{
		idRenderModel	*model = renderModelManager->AllocModel();
		model->ReadFromDemoFile( session->readDemo );
		// add to model manager, so we can find it
		renderModelManager->AddModel( model );

		// save it in the list to free when clearing this map
		localModels.Append( model );

		if ( r_showDemo.GetBool() ) {
			common->Printf( "DC_DEFINE_MODEL\n" );
		}
		break;
		}
	case DC_SET_PORTAL_STATE:
		{
			int		data[2];
			readDemo->ReadInt( data[0] );
			readDemo->ReadInt( data[1] );
			SetPortalState( data[0], data[1] );
			if ( r_showDemo.GetBool() ) {
				common->Printf( "DC_SET_PORTAL_STATE: %i %i\n", data[0], data[1] );
			}
		}
		
		break;
	case DC_END_FRAME:
		return true;

	default:
		common->Error( "Bad token in demo stream" );
	}

	return false;
}

/*
================
WriteLoadMap
================
*/
void	idRenderWorldLocal::WriteLoadMap() {

	// only the main renderWorld writes stuff to demos, not the wipes or
	// menu renders
	if ( this != session->rw ) {
		return;
	}

	session->writeDemo->WriteInt( DS_RENDER );
	session->writeDemo->WriteInt( DC_LOADMAP );

	demoHeader_t	header;
	strncpy( header.mapname, mapName.c_str(), sizeof( header.mapname ) - 1 );
	header.version = 4;
	header.sizeofRenderEntity = sizeof( idRenderEntity );
	header.sizeofRenderLight = sizeof( idRenderLight );
	session->writeDemo->WriteInt( header.version );
	session->writeDemo->WriteInt( header.sizeofRenderEntity );
	session->writeDemo->WriteInt( header.sizeofRenderLight );
	for ( int i = 0; i < 256; i++ )
		session->writeDemo->WriteChar( header.mapname[i] );
	
	if ( r_showDemo.GetBool() ) {
		common->Printf( "write DC_DELETE_LIGHTDEF: %s\n", mapName.c_str() );
	}
}

/*
================
WriteVisibleDefs

================
*/
void	idRenderWorldLocal::WriteVisibleDefs( const viewDef_t *viewDef ) {
	// only the main renderWorld writes stuff to demos, not the wipes or
	// menu renders
	if ( this != session->rw ) {
		return;
	}

	// make sure all necessary entities and lights are updated
	for ( viewEntity_t *viewEnt = viewDef->viewEntitys ; viewEnt ; viewEnt = viewEnt->next ) {
		idRenderEntityLocal *ent = viewEnt->entityDef;

		if ( ent->archived ) {
			// still up to date
			continue;
		}

		// write it out
		WriteRenderEntity( ent );
		ent->archived = true;
	}

	for ( viewLight_t *viewLight = viewDef->viewLights ; viewLight ; viewLight = viewLight->next ) {
		idRenderLightLocal *light = viewLight->lightDef;

		if ( light->archived ) {
			// still up to date
			continue;
		}
		// write it out
		WriteRenderLight( light );
		light->archived = true;
	}
}


/*
================
WriteRenderView
================
*/
void	idRenderWorldLocal::WriteRenderView( const renderView_t *renderView ) {
	int i;

	// only the main renderWorld writes stuff to demos, not the wipes or
	// menu renders
	if ( this != session->rw ) {
		return;
	}
	
	// write the actual view command
	session->writeDemo->WriteInt( DS_RENDER );
	session->writeDemo->WriteInt( DC_RENDERVIEW );
	session->writeDemo->WriteInt( renderView->viewID );
	session->writeDemo->WriteInt( renderView->x );
	session->writeDemo->WriteInt( renderView->y );
	session->writeDemo->WriteInt( renderView->width );
	session->writeDemo->WriteInt( renderView->height );
	session->writeDemo->WriteFloat( renderView->fov_x );
	session->writeDemo->WriteFloat( renderView->fov_y );
	session->writeDemo->WriteVec3( renderView->vieworg );
	session->writeDemo->WriteMat3( renderView->viewaxis );
	session->writeDemo->WriteBool( renderView->cramZNear );
	session->writeDemo->WriteBool( renderView->forceUpdate );
	// binary compatibility with old win32 version writing padded structures directly to disk
	session->writeDemo->WriteUnsignedChar( 0 );
	session->writeDemo->WriteUnsignedChar( 0 );
	session->writeDemo->WriteInt( renderView->time );
	for ( i = 0; i < MAX_GLOBAL_SHADER_PARMS; i++ )
		session->writeDemo->WriteFloat( renderView->shaderParms[i] );
	session->writeDemo->WriteInt( (int&)renderView->globalMaterial );
	
	if ( r_showDemo.GetBool() ) {
		common->Printf( "write DC_RENDERVIEW: %i\n", renderView->time );
	}
}

/*
================
WriteFreeEntity
================
*/
void	idRenderWorldLocal::WriteFreeEntity( qhandle_t handle ) {

	// only the main renderWorld writes stuff to demos, not the wipes or
	// menu renders
	if ( this != session->rw ) {
		return;
	}

	session->writeDemo->WriteInt( DS_RENDER );
	session->writeDemo->WriteInt( DC_DELETE_ENTITYDEF );
	session->writeDemo->WriteInt( handle );

	if ( r_showDemo.GetBool() ) {
		common->Printf( "write DC_DELETE_ENTITYDEF: %i\n", handle );
	}
}

/*
================
WriteFreeLightEntity
================
*/
void	idRenderWorldLocal::WriteFreeLight( qhandle_t handle ) {

	// only the main renderWorld writes stuff to demos, not the wipes or
	// menu renders
	if ( this != session->rw ) {
		return;
	}

	session->writeDemo->WriteInt( DS_RENDER );
	session->writeDemo->WriteInt( DC_DELETE_LIGHTDEF );
	session->writeDemo->WriteInt( handle );

	if ( r_showDemo.GetBool() ) {
		common->Printf( "write DC_DELETE_LIGHTDEF: %i\n", handle );
	}
}

/*
================
WriteRenderLight
================
*/
void	idRenderWorldLocal::WriteRenderLight( const idRenderLightLocal *light ) {

	// only the main renderWorld writes stuff to demos, not the wipes or
	// menu renders
	if ( this != session->rw ) {
		return;
	}

	session->writeDemo->WriteInt( DS_RENDER );
	session->writeDemo->WriteInt( DC_UPDATE_LIGHTDEF );
	session->writeDemo->WriteInt( light->GetIndex() );

	session->writeDemo->WriteMat3( light->GetAxis() );
	session->writeDemo->WriteVec3( light->GetOrigin() );
	session->writeDemo->WriteInt( light->GetSuppressLightInViewID() );
	session->writeDemo->WriteInt( light->GetAllowLightInViewID() );
	session->writeDemo->WriteBool( light->GetNoShadows() );
	session->writeDemo->WriteBool( light->GetNoSpecular() );
	session->writeDemo->WriteBool( light->GetPointLight() );
	session->writeDemo->WriteBool( light->GetParallel() );
	session->writeDemo->WriteVec3( light->GetLightRadius() );
	session->writeDemo->WriteVec3( light->GetLightCenter() );
	session->writeDemo->WriteVec3( light->GetTarget() );
	session->writeDemo->WriteVec3( light->GetRight() );
	session->writeDemo->WriteVec3( light->GetUp() );
	session->writeDemo->WriteVec3( light->GetStart() );
	session->writeDemo->WriteVec3( light->GetEnd() );
	session->writeDemo->WriteInt( light->GetPrelightModel() != NULL );
	session->writeDemo->WriteInt( light->GetLightId() );
	session->writeDemo->WriteInt( light->GetShader() != NULL );
	for ( int i = 0; i < MAX_ENTITY_SHADER_PARMS; i++)
		session->writeDemo->WriteFloat( light->GetShaderParm( i ) );
	session->writeDemo->WriteInt( light->GetReferenceSound() != NULL );

	if ( light->GetPrelightModel() ) {
		session->writeDemo->WriteHashString( light->GetPrelightModel()->Name() );
	}
	if ( light->GetShader() ) {
		session->writeDemo->WriteHashString( light->GetShader()->GetName() );
	}
	if ( light->GetReferenceSound() ) {
		int	index = light->GetReferenceSound()->Index();
		session->writeDemo->WriteInt( index );
	}

	if ( r_showDemo.GetBool() ) {
		common->Printf( "write DC_UPDATE_LIGHTDEF: %i\n", light->GetIndex() );
	}
}

/*
================
ReadRenderLight
================
*/
void	idRenderWorldLocal::ReadRenderLight( ) {
	idRenderLightLocal parameters;
	int index;

	session->readDemo->ReadInt( index );
	if ( index < 0 ) {
		common->Error( "ReadRenderLight: index < 0 " );
	}

	idMat3 axis;
	idVec3 vector;
	int integerValue;
	bool boolValue;
	float floatValue;

	session->readDemo->ReadMat3( axis );
	parameters.SetAxis( axis );
	session->readDemo->ReadVec3( vector );
	parameters.SetOrigin( vector );
	session->readDemo->ReadInt( integerValue );
	parameters.SetSuppressLightInViewID( integerValue );
	session->readDemo->ReadInt( integerValue );
	parameters.SetAllowLightInViewID( integerValue );
	session->readDemo->ReadBool( boolValue );
	parameters.SetNoShadows( boolValue );
	session->readDemo->ReadBool( boolValue );
	parameters.SetNoSpecular( boolValue );
	session->readDemo->ReadBool( boolValue );
	parameters.SetPointLight( boolValue );
	session->readDemo->ReadBool( boolValue );
	parameters.SetParallel( boolValue );
	session->readDemo->ReadVec3( vector );
	parameters.SetLightRadius( vector );
	session->readDemo->ReadVec3( vector );
	parameters.SetLightCenter( vector );
	session->readDemo->ReadVec3( vector );
	parameters.SetTarget( vector );
	session->readDemo->ReadVec3( vector );
	parameters.SetRight( vector );
	session->readDemo->ReadVec3( vector );
	parameters.SetUp( vector );
	session->readDemo->ReadVec3( vector );
	parameters.SetStart( vector );
	session->readDemo->ReadVec3( vector );
	parameters.SetEnd( vector );
	int hasPrelightModel;
	session->readDemo->ReadInt( hasPrelightModel );
	session->readDemo->ReadInt( integerValue );
	parameters.SetLightId( integerValue );
	int hasShader;
	session->readDemo->ReadInt( hasShader );
	for ( int i = 0; i < MAX_ENTITY_SHADER_PARMS; i++ ) {
		session->readDemo->ReadFloat( floatValue );
		parameters.SetShaderParm( i, floatValue );
	}
	int hasReferenceSound;
	session->readDemo->ReadInt( hasReferenceSound );
	if ( hasPrelightModel ) {
		parameters.SetPrelightModel( renderModelManager->FindModel( session->readDemo->ReadHashString() ) );
	}
	if ( hasShader ) {
		parameters.SetShader( declManager->FindMaterial( session->readDemo->ReadHashString() ) );
	}
	if ( hasReferenceSound ) {
		int soundIndex;
		session->readDemo->ReadInt( soundIndex );
		parameters.SetReferenceSound( session->sw->EmitterForIndex( soundIndex ) );
	}

	while ( lightDefs.Num() <= index ) {
		lightDefs.Append( NULL );
	}
	idRenderLightLocal *light = lightDefs[index];
	if ( light == NULL ) {
		light = new idRenderLightLocal;
		light->world = this;
		light->index = index;
		lightDefs[index] = light;
	} else {
		R_FreeLightDefDerivedData( light );
		light->initialized = false;
		light->lightHasMoved = true;
	}
	light->CopyFrom( parameters );
	light->UpdateRenderLight();

	if ( r_showDemo.GetBool() ) {
		common->Printf( "DC_UPDATE_LIGHTDEF: %i\n", index );
	}
}

/*
================
WriteRenderEntity
================
*/
void	idRenderWorldLocal::WriteRenderEntity( const idRenderEntityLocal *ent ) {

	// only the main renderWorld writes stuff to demos, not the wipes or
	// menu renders
	if ( this != session->rw ) {
		return;
	}

	session->writeDemo->WriteInt( DS_RENDER );
	session->writeDemo->WriteInt( DC_UPDATE_ENTITYDEF );
	session->writeDemo->WriteInt( ent->GetIndex() );
	
	session->writeDemo->WriteInt( ent->GetModel() != NULL );
	session->writeDemo->WriteInt( ent->GetEntityNum() );
	session->writeDemo->WriteInt( ent->GetBodyId() );
	session->writeDemo->WriteVec3( ent->GetBounds()[0] );
	session->writeDemo->WriteVec3( ent->GetBounds()[1] );
	session->writeDemo->WriteInt( ent->GetCallback() != NULL );
	session->writeDemo->WriteInt( ent->GetCallbackData() != NULL );
	session->writeDemo->WriteInt( ent->GetSuppressSurfaceInViewID() );
	session->writeDemo->WriteInt( ent->GetSuppressShadowInViewID() );
	session->writeDemo->WriteInt( ent->GetSuppressShadowInLightID() );
	session->writeDemo->WriteInt( ent->GetAllowSurfaceInViewID() );
	session->writeDemo->WriteVec3( ent->GetOrigin() );
	session->writeDemo->WriteMat3( ent->GetAxis() );
	session->writeDemo->WriteInt( ent->GetCustomShader() != NULL );
	session->writeDemo->WriteInt( ent->GetReferenceShader() != NULL );
	session->writeDemo->WriteInt( ent->GetCustomSkin() != NULL );
	session->writeDemo->WriteInt( ent->GetReferenceSound() != NULL );
	for ( int i = 0; i < MAX_ENTITY_SHADER_PARMS; i++ )
		session->writeDemo->WriteFloat( ent->GetShaderParm( i ) );
	for ( int i = 0; i < MAX_RENDERENTITY_GUI; i++ )
		session->writeDemo->WriteInt( ent->GetGui( i ) != NULL );
	session->writeDemo->WriteInt( ent->GetRemoteRenderView() != NULL );
	session->writeDemo->WriteInt( ent->GetNumJoints() );
	session->writeDemo->WriteInt( ent->GetJoints() != NULL );
	session->writeDemo->WriteFloat( ent->GetModelDepthHack() );
	session->writeDemo->WriteBool( ent->GetNoSelfShadow() );
	session->writeDemo->WriteBool( ent->GetNoShadow() );
	session->writeDemo->WriteBool( ent->GetNoDynamicInteractions() );
	session->writeDemo->WriteBool( ent->GetWeaponDepthHack() );
	session->writeDemo->WriteInt( ent->GetForceUpdate() );

	if ( ent->GetCustomShader() ) {
		session->writeDemo->WriteHashString( ent->GetCustomShader()->GetName() );
	}
	if ( ent->GetCustomSkin() ) {
		session->writeDemo->WriteHashString( ent->GetCustomSkin()->GetName() );
	}
	if ( ent->GetModel() ) {
		session->writeDemo->WriteHashString( ent->GetModel()->Name() );
	}
	if ( ent->GetReferenceShader() ) {
		session->writeDemo->WriteHashString( ent->GetReferenceShader()->GetName() );
	}
	if ( ent->GetReferenceSound() ) {
		int	index = ent->GetReferenceSound()->Index();
		session->writeDemo->WriteInt( index );
	}
	if ( ent->GetNumJoints() ) {
		for ( int i = 0; i < ent->GetNumJoints(); i++) {
			float *data = ent->GetJoints()[i].ToFloatPtr();
			for ( int j = 0; j < 12; ++j)
				session->writeDemo->WriteFloat( data[j] );
		}
	}

	/*
	if ( ent->decals ) {
		ent->decals->WriteToDemoFile( session->readDemo );
	}
	if ( ent->overlay ) {
		ent->overlay->WriteToDemoFile( session->writeDemo );
	}
	*/

#ifdef WRITE_GUIS
	if ( ent->gui ) {
		ent->gui->WriteToDemoFile( session->writeDemo );
	}
	if ( ent->gui2 ) {
		ent->gui2->WriteToDemoFile( session->writeDemo );
	}
	if ( ent->gui3 ) {
		ent->gui3->WriteToDemoFile( session->writeDemo );
	}
#endif

	// RENDERDEMO_VERSION >= 2 ( Doom3 1.2 )
	session->writeDemo->WriteInt( ent->GetTimeGroup() );
	session->writeDemo->WriteInt( ent->GetXrayIndex() );

	if ( r_showDemo.GetBool() ) {
		common->Printf( "write DC_UPDATE_ENTITYDEF: %i = %s\n", ent->GetIndex(), ent->GetModel() ? ent->GetModel()->Name() : "NULL" );
	}
}

/*
================
ReadRenderEntity
================
*/
void	idRenderWorldLocal::ReadRenderEntity() {
	idRenderEntityLocal parameters;
	int index;

	session->readDemo->ReadInt( index );
	if ( index < 0 ) {
		common->Error( "ReadRenderEntity: index < 0" );
	}

	int integerValue;
	float floatValue;
	bool boolValue;
	idVec3 vector;
	idMat3 axis;

	int hasModel;
	session->readDemo->ReadInt( hasModel );
	session->readDemo->ReadInt( integerValue );
	parameters.SetEntityNum( integerValue );
	session->readDemo->ReadInt( integerValue );
	parameters.SetBodyId( integerValue );
	idBounds bounds;
	session->readDemo->ReadVec3( bounds[0] );
	session->readDemo->ReadVec3( bounds[1] );
	parameters.SetBounds( bounds );
	int ignoredPointer;
	session->readDemo->ReadInt( ignoredPointer ); // callback
	session->readDemo->ReadInt( ignoredPointer ); // callback data
	session->readDemo->ReadInt( integerValue );
	parameters.SetSuppressSurfaceInViewID( integerValue );
	session->readDemo->ReadInt( integerValue );
	parameters.SetSuppressShadowInViewID( integerValue );
	session->readDemo->ReadInt( integerValue );
	parameters.SetSuppressShadowInLightID( integerValue );
	session->readDemo->ReadInt( integerValue );
	parameters.SetAllowSurfaceInViewID( integerValue );
	session->readDemo->ReadVec3( vector );
	parameters.SetOrigin( vector );
	session->readDemo->ReadMat3( axis );
	parameters.SetAxis( axis );
	int hasCustomShader;
	int hasReferenceShader;
	int hasCustomSkin;
	int hasReferenceSound;
	session->readDemo->ReadInt( hasCustomShader );
	session->readDemo->ReadInt( hasReferenceShader );
	session->readDemo->ReadInt( hasCustomSkin );
	session->readDemo->ReadInt( hasReferenceSound );
	for ( int i = 0; i < MAX_ENTITY_SHADER_PARMS; i++ ) {
		session->readDemo->ReadFloat( floatValue );
		parameters.SetShaderParm( i, floatValue );
	}
	int guiPresent[MAX_RENDERENTITY_GUI];
	for ( int i = 0; i < MAX_RENDERENTITY_GUI; i++ ) {
		session->readDemo->ReadInt( guiPresent[i] );
	}
	session->readDemo->ReadInt( ignoredPointer ); // remote render view
	int numJoints;
	session->readDemo->ReadInt( numJoints );
	session->readDemo->ReadInt( ignoredPointer ); // joints pointer
	session->readDemo->ReadFloat( floatValue );
	parameters.SetModelDepthHack( floatValue );
	session->readDemo->ReadBool( boolValue );
	parameters.SetNoSelfShadow( boolValue );
	session->readDemo->ReadBool( boolValue );
	parameters.SetNoShadow( boolValue );
	session->readDemo->ReadBool( boolValue );
	parameters.SetNoDynamicInteractions( boolValue );
	session->readDemo->ReadBool( boolValue );
	parameters.SetWeaponDepthHack( boolValue );
	session->readDemo->ReadInt( integerValue );
	parameters.SetForceUpdate( integerValue );
	if ( hasCustomShader ) {
		parameters.SetCustomShader( declManager->FindMaterial( session->readDemo->ReadHashString() ) );
	}
	if ( hasCustomSkin ) {
		parameters.SetCustomSkin( declManager->FindSkin( session->readDemo->ReadHashString() ) );
	}
	if ( hasModel ) {
		parameters.SetModel( renderModelManager->FindModel( session->readDemo->ReadHashString() ) );
	}
	if ( hasReferenceShader ) {
		parameters.SetReferenceShader( declManager->FindMaterial( session->readDemo->ReadHashString() ) );
	}
	if ( hasReferenceSound ) {
		int soundIndex;
		session->readDemo->ReadInt( soundIndex );
		parameters.SetReferenceSound( session->sw->EmitterForIndex( soundIndex ) );
	}
	if ( numJoints > 0 ) {
		idJointMat *joints = (idJointMat *)Mem_Alloc16( numJoints * sizeof( idJointMat ) );
		for ( int i = 0; i < numJoints; i++ ) {
			float *data = joints[i].ToFloatPtr();
			for ( int j = 0; j < 12; ++j ) {
				session->readDemo->ReadFloat( data[j] );
			}
		}
		parameters.SetJoints( numJoints, joints );
	}

	/*
	if ( ent.decals ) {
		ent.decals = idRenderModelDecal::Alloc();
		ent.decals->ReadFromDemoFile( session->readDemo );
	}
	if ( ent.overlay ) {
		ent.overlay = idRenderModelOverlay::Alloc();
		ent.overlay->ReadFromDemoFile( session->readDemo );
	}
	*/

	for ( int i = 0; i < MAX_RENDERENTITY_GUI; i++ ) {
		if ( guiPresent[i] ) {
			parameters.SetGui( i, uiManager->Alloc() );
#ifdef WRITE_GUIS
			parameters.GetGui( i )->ReadFromDemoFile( session->readDemo );
#endif
		}
	}

	// >= Doom3 v1.2 only
	if ( session->renderdemoVersion >= 2 ) {
		session->readDemo->ReadInt( integerValue );
		parameters.SetTimeGroup( integerValue );
		session->readDemo->ReadInt( integerValue );
		parameters.SetXrayIndex( integerValue );
	} else {
		parameters.SetTimeGroup( 0 );
		parameters.SetXrayIndex( 0 );
	}

	while ( entityDefs.Num() <= index ) {
		entityDefs.Append( NULL );
	}
	idRenderEntityLocal *entity = entityDefs[index];
	if ( entity == NULL ) {
		entity = new idRenderEntityLocal;
		entity->world = this;
		entity->index = index;
		entityDefs[index] = entity;
	} else {
		R_FreeEntityDefDerivedData( entity, false, false );
		entity->initialized = false;
	}
	entity->CopyFrom( parameters );
	entity->UpdateRenderEntity();

	if ( r_showDemo.GetBool() ) {
		common->Printf( "DC_UPDATE_ENTITYDEF: %i = %s\n", index, entity->GetModel() ? entity->GetModel()->Name() : "NULL" );
	}
}
