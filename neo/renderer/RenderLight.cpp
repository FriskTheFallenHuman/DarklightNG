/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

===========================================================================
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"

idRenderLightLocal::idRenderLightLocal() {
	memset( modelMatrix, 0, sizeof( modelMatrix ) );
	memset( lightProject, 0, sizeof( lightProject ) );
	memset( frustum, 0, sizeof( frustum ) );
	memset( frustumWindings, 0, sizeof( frustumWindings ) );

	lightHasMoved = false;
	world = NULL;
	index = 0;
	lastModifiedFrameNum = 0;
	archived = false;
	initialized = false;
	lightShader = NULL;
	falloffImage = NULL;
	globalLightOrigin = vec3_zero;
	frustumTris = NULL;
	viewCount = 0;
	viewLight = NULL;
	references = NULL;
	foggedPortals = NULL;
}

void idRenderLightLocal::FreeRenderLight() {
	if ( world != NULL ) {
		world->FreeRenderLight( this );
	}
}

void idRenderLightLocal::RemoveFromRenderWorld() {
	if ( !initialized ) {
		return;
	}

	R_FreeLightDefDerivedData( this );
	if ( session->writeDemo && archived ) {
		world->WriteFreeLight( index );
		archived = false;
	}
	initialized = false;
}

void idRenderLightLocal::UpdateRenderLight( bool force ) {
	if ( r_skipUpdates.GetBool() ) {
		return;
	}

	tr.pc.c_lightUpdates++;
	force = force || !initialized;
	if ( !force && dirtyFlags == DIRTY_NONE ) {
		return;
	}

	const bool shapeChanged = force || ( dirtyFlags & DIRTY_SHAPE ) != 0;
	if ( initialized && shapeChanged ) {
		lightHasMoved = true;
		R_FreeLightDefDerivedData( this );
	}

	if ( !initialized && bakedLight ) {
		world->bakedLightCandidateCount++;
	}

	lastModifiedFrameNum = tr.frameCount;
	if ( session->writeDemo && archived ) {
		world->WriteFreeLight( index );
		archived = false;
	}

	if ( lightHasMoved ) {
		prelightModel = NULL;
	}

	if ( shapeChanged ) {
		R_DeriveLightData( this );
		const bool useBakedSurfaceLighting = world->hasBakedLightmaps && !r_skipBakedLightmaps.GetBool() && bakedLight && !lightHasMoved;
		if ( bakedLight && lightHasMoved ) {
			world->bakedLightMovedCount++;
		}
		R_CreateLightRefs( this );
		R_CreateLightDefFogPortals( this );
		if ( useBakedSurfaceLighting ) {
			world->bakedLightSuppressionCount++;
		}
	}

	initialized = true;
	dirtyFlags = DIRTY_NONE;
}

void idRenderLightLocal::ForceUpdate() {
	dirtyFlags = DIRTY_ALL;
	UpdateRenderLight( true );
}

bool idRenderLightLocal::IsInRenderWorld() const {
	return initialized;
}

int idRenderLightLocal::GetIndex() const {
	return index;
}
