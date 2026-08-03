/*
===========================================================================

Animation editor services that depend on game entity definitions.

===========================================================================
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "Game_local.h"

static const idDeclModelDef *AnimModelDefFromEntityDef( const idDict *args ) {
	const idDeclModelDef *modelDef = static_cast<const idDeclModelDef *>( declManager->FindType( DECL_MODELDEF, args->GetString( "model" ), false ) );
	return modelDef && modelDef->ModelHandle() ? modelDef : NULL;
}

idRenderModel *idGameEdit::ANIM_GetModelFromEntityDef( const idDict *args ) {
	idRenderModel *model = NULL;
	const idDeclModelDef *modelDef = static_cast<const idDeclModelDef *>( declManager->FindType( DECL_MODELDEF, args->GetString( "model" ), false ) );
	if ( modelDef ) {
		model = modelDef->ModelHandle();
	}
	if ( !model ) {
		model = renderModelManager->FindModel( args->GetString( "model" ) );
	}
	return model && !model->IsDefaultModel() ? model : NULL;
}

idRenderModel *idGameEdit::ANIM_GetModelFromEntityDef( const char *classname ) {
	const idDict *args = gameLocal.FindEntityDefDict( classname, false );
	return args ? ANIM_GetModelFromEntityDef( args ) : NULL;
}

const idVec3 &idGameEdit::ANIM_GetModelOffsetFromEntityDef( const char *classname ) {
	const idDict *args = gameLocal.FindEntityDefDict( classname, false );
	const idDeclModelDef *modelDef = args ? AnimModelDefFromEntityDef( args ) : NULL;
	return modelDef ? modelDef->GetVisualOffset() : vec3_origin;
}

idRenderModel *idGameEdit::ANIM_GetModelFromName( const char *modelName ) {
	const idDeclModelDef *modelDef = static_cast<const idDeclModelDef *>( declManager->FindType( DECL_MODELDEF, modelName, false ) );
	idRenderModel *model = modelDef ? modelDef->ModelHandle() : NULL;
	return model ? model : renderModelManager->FindModel( modelName );
}

const idMD5Anim *idGameEdit::ANIM_GetAnimFromEntityDef( const char *classname, const char *animname ) {
	const idDict *args = gameLocal.FindEntityDefDict( classname, false );
	if ( !args ) {
		return NULL;
	}
	const idDeclModelDef *modelDef = static_cast<const idDeclModelDef *>( declManager->FindType( DECL_MODELDEF, args->GetString( "model" ), false ) );
	if ( !modelDef ) {
		return NULL;
	}
	const idAnim *anim = modelDef->GetAnim( modelDef->GetAnim( animname ) );
	return anim ? anim->MD5Anim( 0 ) : NULL;
}

int idGameEdit::ANIM_GetNumAnimsFromEntityDef( const idDict *args ) {
	const idDeclModelDef *modelDef = static_cast<const idDeclModelDef *>( declManager->FindType( DECL_MODELDEF, args->GetString( "model" ), false ) );
	return modelDef ? modelDef->NumAnims() : 0;
}

const char *idGameEdit::ANIM_GetAnimNameFromEntityDef( const idDict *args, int animNum ) {
	const idDeclModelDef *modelDef = static_cast<const idDeclModelDef *>( declManager->FindType( DECL_MODELDEF, args->GetString( "model" ), false ) );
	const idAnim *anim = modelDef ? modelDef->GetAnim( animNum ) : NULL;
	return anim ? anim->FullName() : "";
}

const idMD5Anim *idGameEdit::ANIM_GetAnim( const char *fileName ) {
	return animationLib->GetAnim( fileName );
}

int idGameEdit::ANIM_GetLength( const idMD5Anim *anim ) {
	return anim ? anim->Length() : 0;
}

int idGameEdit::ANIM_GetNumFrames( const idMD5Anim *anim ) {
	return anim ? anim->NumFrames() : 0;
}

void idGameEdit::ANIM_CreateAnimFrame( const idRenderModel *model, const idMD5Anim *anim, int numJoints, idJointMat *joints, int time, const idVec3 &offset, bool removeOriginOffset ) {
	if ( !model || model->IsDefaultModel() || !anim ) {
		return;
	}
	if ( numJoints != model->NumJoints() ) {
		gameLocal.Error( "ANIM_CreateAnimFrame: different # of joints in idRenderEntity than in model (%s)", model->Name() );
	}
	if ( !numJoints ) {
		return;
	}
	if ( !joints ) {
		gameLocal.Error( "ANIM_CreateAnimFrame: NULL joint frame pointer on model (%s)", model->Name() );
	}
	if ( numJoints != anim->NumJoints() ) {
		gameLocal.Warning( "Model '%s' has different # of joints than anim '%s'", model->Name(), anim->Name() );
		for ( int i = 0; i < numJoints; i++ ) {
			joints[ i ].SetRotation( mat3_identity );
			joints[ i ].SetTranslation( offset );
		}
		return;
	}

	int *indexes = static_cast<int *>( _alloca16( numJoints * sizeof( int ) ) );
	for ( int i = 0; i < numJoints; i++ ) {
		indexes[ i ] = i;
	}
	frameBlend_t frame;
	anim->ConvertTimeToFrame( time, 1, frame );
	idJointQuat *jointFrame = static_cast<idJointQuat *>( _alloca16( numJoints * sizeof( idJointQuat ) ) );
	anim->GetInterpolatedFrame( frame, jointFrame, indexes, numJoints );
	SIMDProcessor->ConvertJointQuatsToJointMats( joints, jointFrame, numJoints );
	joints[ 0 ].SetTranslation( removeOriginOffset ? offset : joints[ 0 ].ToVec3() + offset );
	const idMD5Joint *modelJoints = model->GetJoints();
	for ( int i = 1; i < numJoints; i++ ) {
		joints[ i ] *= joints[ modelJoints[ i ].parent - modelJoints ];
	}
}

idRenderModel *idGameEdit::ANIM_CreateMeshForAnim( idRenderModel *model, const char *classname, const char *animname, int frame, bool removeOriginOffset ) {
	if ( !model || model->IsDefaultModel() ) {
		return NULL;
	}
	const idDict *args = gameLocal.FindEntityDefDict( classname, false );
	if ( !args ) {
		return NULL;
	}

	idRenderEntity *entity = gameRenderWorld->AllocRenderEntity();
	idBounds emptyBounds;
	emptyBounds.Clear();
	entity->SetBounds( emptyBounds );
	entity->SetSuppressSurfaceInViewID( 0 );

	const idMD5Anim *md5anim = NULL;
	idVec3 offset;
	const idDeclModelDef *modelDef = AnimModelDefFromEntityDef( args );
	if ( modelDef ) {
		const idAnim *anim = modelDef->GetAnim( modelDef->GetAnim( animname ) );
		if ( anim ) {
			md5anim = anim->MD5Anim( 0 );
			entity->SetCustomSkin( modelDef->GetDefaultSkin() );
			offset = modelDef->GetVisualOffset();
		}
	} else {
		idStr filename = animname;
		idStr extension;
		filename.ExtractFileExtension( extension );
		const char *resolvedAnim = extension.Length() ? animname : args->GetString( va( "anim %s", animname ) );
		md5anim = animationLib->GetAnim( resolvedAnim );
		offset.Zero();
	}

	if ( !md5anim ) {
		entity->FreeRenderEntity();
		return NULL;
	}
	const char *skin = args->GetString( "skin", "" );
	if ( skin[ 0 ] ) {
		entity->SetCustomSkin( declManager->FindSkin( skin ) );
	}
	const int numJoints = model->NumJoints();
	idJointMat *joints = static_cast<idJointMat *>( Mem_Alloc16( numJoints * sizeof( idJointMat ) ) );
	entity->SetJoints( numJoints, joints );
	ANIM_CreateAnimFrame( model, md5anim, numJoints, joints, FRAME2MS( frame ), offset, removeOriginOffset );
	idRenderModel *newModel = model->InstantiateDynamicModel( entity, NULL, NULL );
	Mem_Free16( joints );
	entity->SetJoints( 0, NULL );
	entity->FreeRenderEntity();
	return newModel;
}
