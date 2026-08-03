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

#include "Game_local.h"
#include "PlayerIcon.h"

static const char * iconKeys[ ICON_NONE ] = {
	"mtr_icon_lag",
	"mtr_icon_chat"
#ifdef CTF
	,"mtr_icon_redteam",
	"mtr_icon_blueteam"
#endif
};

/*
===============
idPlayerIcon::idPlayerIcon
===============
*/
idPlayerIcon::idPlayerIcon() {
	renderEnt	= NULL;
	iconType	= ICON_NONE;
}

/*
===============
idPlayerIcon::~idPlayerIcon
===============
*/
idPlayerIcon::~idPlayerIcon() {
	FreeIcon();
}

/*
===============
idPlayerIcon::Draw
===============
*/
void idPlayerIcon::Draw( idPlayer *player, jointHandle_t joint ) {
	idVec3 origin;
	idMat3 axis;

	if ( joint == INVALID_JOINT ) {
		FreeIcon();
		return;
	}

	player->GetJointWorldTransform( joint, gameLocal.time, origin, axis );
	origin.z += 16.0f;

	Draw( player, origin );
}

/*
===============
idPlayerIcon::Draw
===============
*/
void idPlayerIcon::Draw( idPlayer *player, const idVec3 &origin ) {
	idPlayer *localPlayer = gameLocal.GetLocalPlayer();
	if ( !localPlayer || !localPlayer->GetRenderView() ) {
		FreeIcon();
		return;
	}

	idMat3 axis = localPlayer->GetRenderView()->viewaxis;

	if ( player->isLagged && !player->spectating ) {
		// create the icon if necessary, or update if already created
		if ( !CreateIcon( player, ICON_LAG, origin, axis ) ) {
			UpdateIcon( player, origin, axis );
		}
	} else if ( player->isChatting && !player->spectating ) {
		if ( !CreateIcon( player, ICON_CHAT, origin, axis ) ) {
			UpdateIcon( player, origin, axis );
		}
#ifdef CTF
	} else if ( g_CTFArrows.GetBool() && gameLocal.mpGame.IsGametypeFlagBased() && gameLocal.GetLocalPlayer() && player->team == gameLocal.GetLocalPlayer()->team && !player->IsHidden() && !player->AI_DEAD ) {
		int icon = ICON_TEAM_RED + player->team;

		if ( icon != ICON_TEAM_RED && icon != ICON_TEAM_BLUE )
			return;

		if ( !CreateIcon( player, ( playerIconType_t )icon, origin, axis ) ) {
			UpdateIcon( player, origin, axis );
		}
#endif
	} else {
		FreeIcon();
	}
}

/*
===============
idPlayerIcon::FreeIcon
===============
*/
void idPlayerIcon::FreeIcon( void ) {
	if ( renderEnt != NULL ) {
		renderEnt->FreeRenderEntity();
		renderEnt = NULL;
	}
	iconType = ICON_NONE;
}

/*
===============
idPlayerIcon::CreateIcon
===============
*/
bool idPlayerIcon::CreateIcon( idPlayer *player, playerIconType_t type, const idVec3 &origin, const idMat3 &axis ) {
	assert( type != ICON_NONE );
	const char *mtr = player->spawnArgs.GetString( iconKeys[ type ], "_default" );
	return CreateIcon( player, type, mtr, origin, axis );
}

/*
===============
idPlayerIcon::CreateIcon
===============
*/
bool idPlayerIcon::CreateIcon( idPlayer *player, playerIconType_t type, const char *mtr, const idVec3 &origin, const idMat3 &axis ) {
	assert( type != ICON_NONE );

	if ( type == iconType ) {
		return false;
	}

	FreeIcon();

	renderEnt = gameRenderWorld->AllocRenderEntity();
	renderEnt->SetOrigin( origin );
	renderEnt->SetAxis( axis );
	for ( int i = 0; i < 4; i++ ) {
		renderEnt->SetShaderParm( i, 1.0f );
	}
	renderEnt->SetShaderParm( SHADERPARM_SPRITE_WIDTH, 16.0f );
	renderEnt->SetShaderParm( SHADERPARM_SPRITE_HEIGHT, 16.0f );
	renderEnt->SetModel( renderModelManager->FindModel( "_sprite" ) );
	renderEnt->SetNoShadow( true );
	renderEnt->SetNoSelfShadow( true );
	renderEnt->SetCustomShader( declManager->FindMaterial( mtr ) );
	renderEnt->SetBounds( renderEnt->GetModel()->Bounds( renderEnt ) );
	renderEnt->UpdateRenderEntity();
	iconType = type;

	return true;
}

/*
===============
idPlayerIcon::UpdateIcon
===============
*/
void idPlayerIcon::UpdateIcon( idPlayer *player, const idVec3 &origin, const idMat3 &axis ) {
	assert( renderEnt != NULL );

	renderEnt->SetOrigin( origin );
	renderEnt->SetAxis( axis );
	renderEnt->UpdateRenderEntity();
}
