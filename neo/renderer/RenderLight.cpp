/*
===========================================================================

DarklightNG Source Code
Copyright (C) 2026 - Justin Marshall(aka IceColdDuke).

This file is part of the DarklightNG GPL source code.
This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).

DarklightNG is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

DarklightNG is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

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
