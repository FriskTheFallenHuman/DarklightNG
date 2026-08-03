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

idRenderEntityLocal::idRenderEntityLocal() {
	memset( modelMatrix, 0, sizeof( modelMatrix ) );

	world					= NULL;
	index					= 0;
	lastModifiedFrameNum	= 0;
	archived				= false;
	initialized				= false;
	dynamicModel			= NULL;
	dynamicModelFrameCount	= 0;
	cachedDynamicModel		= NULL;
	referenceBounds			= bounds_zero;
	viewCount				= 0;
	viewEntity				= NULL;
	visibleCount			= 0;
	decals					= NULL;
	overlay					= NULL;
	entityRefs				= NULL;
	needsPortalSky			= false;
}

void idRenderEntityLocal::FreeRenderEntity() {
	if ( world != NULL ) {
		world->FreeRenderEntity( this );
	}
}

void idRenderEntityLocal::RemoveFromRenderWorld() {
	if ( !initialized ) {
		return;
	}

	R_FreeEntityDefDerivedData( this, false, false );
	if ( session->writeDemo && archived ) {
		world->WriteFreeEntity( index );
		archived = false;
	}
	initialized = false;
}

void idRenderEntityLocal::UpdateRenderEntity( bool force ) {
	if ( r_skipUpdates.GetBool() ) {
		return;
	}

	tr.pc.c_entityUpdates++;
	if ( hModel == NULL && callback == NULL ) {
		common->Error( "idRenderEntity::UpdateRenderEntity: NULL model" );
	}

	force = force || forceUpdate != 0;
	if ( initialized ) {
		if ( !force && dirtyFlags == DIRTY_NONE && joints == NULL && callbackData == NULL && dynamicModel == NULL ) {
			return;
		}

		const unsigned int referenceChanges = DIRTY_MODEL | DIRTY_TRANSFORM | DIRTY_BOUNDS;
		if ( !force && callback != NULL && ( dirtyFlags & referenceChanges ) == 0 ) {
			c_callbackUpdate++;
			R_ClearEntityDefDynamicModel( this );
			dirtyFlags = DIRTY_NONE;
			return;
		}

		const bool keepModelData = ( dirtyFlags & DIRTY_MODEL ) == 0;
		R_FreeEntityDefDerivedData( this, keepModelData, keepModelData );
	}

	R_AxisToModelMatrix( axis, origin, modelMatrix );
	lastModifiedFrameNum = tr.frameCount;
	if ( session->writeDemo && archived ) {
		world->WriteFreeEntity( index );
		archived = false;
	}

	if ( !r_useEntityCallbacks.GetBool() && callback != NULL ) {
		R_IssueEntityDefCallback( this );
	}
	R_CreateEntityRefs( this );

	initialized = true;
	dirtyFlags = DIRTY_NONE;
}

void idRenderEntityLocal::ForceUpdate() {
	dirtyFlags = DIRTY_ALL;
	UpdateRenderEntity( true );
}

bool idRenderEntityLocal::IsInRenderWorld() const {
	return initialized;
}

int idRenderEntityLocal::GetIndex() const {
	return index;
}

void idRenderEntityLocal::ProjectOverlay( const idPlane localTextureAxis[2], const idMaterial *material ) {
	if ( !initialized || hModel == NULL || hModel->IsDynamicModel() != DM_CACHED ) {
		return;
	}
	idRenderModel *model = R_EntityDefDynamicModel( this );
	if ( overlay == NULL ) {
		overlay = idRenderModelOverlay::Alloc();
	}
	overlay->CreateOverlay( model, localTextureAxis, material );
}
void idRenderEntityLocal::RemoveDecals() {
	if ( !initialized ) {
		return;
	}
	R_FreeEntityDefDecals( this );
	R_FreeEntityDefOverlay( this );
}
