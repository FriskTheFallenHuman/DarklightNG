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

#include "DeviceContext.h"
#include "Window.h"
#include "UserInterfaceLocal.h"
#include "RenderWindow.h"

idRenderWindow::idRenderWindow(idDeviceContext *d, idUserInterfaceLocal *g) : idWindow(d, g) {
	dc = d;
	gui = g;
	CommonInit();
}

idRenderWindow::idRenderWindow(idUserInterfaceLocal *g) : idWindow(g) {
	gui = g;
	CommonInit();
}

idRenderWindow::~idRenderWindow() {
	if ( worldEntity != NULL && worldEntity->GetJoints() != NULL ) {
		Mem_Free16( worldEntity->GetJoints() );
		worldEntity->SetJoints( 0, NULL );
	}
	renderSystem->FreeRenderWorld( world ); 
}

void idRenderWindow::CommonInit() {
	world = renderSystem->AllocRenderWorld();
	needsRender = true;
	lightOrigin = idVec4(-128.0f, 0.0f, 0.0f, 1.0f);
	lightColor = idVec4(1.0f, 1.0f, 1.0f, 1.0f);
	modelOrigin.Zero();
	viewOffset = idVec4(-128.0f, 0.0f, 0.0f, 1.0f);
	modelAnim = NULL;
	animLength = 0;
	animEndTime = -1;
	worldEntity = NULL;
	rLight = NULL;
	updateAnimation = true;
}


void idRenderWindow::BuildAnimation(int time) {
	
	if (!updateAnimation) {
		return;
	}

	if (animName.Length() && animClass.Length()) {
		if ( worldEntity->GetJoints() != NULL ) {
			Mem_Free16( worldEntity->GetJoints() );
			worldEntity->SetJoints( 0, NULL );
		}
		const int numJoints = worldEntity->GetModel()->NumJoints();
		worldEntity->SetJoints( numJoints, ( idJointMat * )Mem_Alloc16( numJoints * sizeof( idJointMat ) ) );
		modelAnim = gameEdit->ANIM_GetAnimFromEntityDef(animClass, animName);
		if (modelAnim) {
			animLength = gameEdit->ANIM_GetLength(modelAnim);
			animEndTime = time + animLength;
		}
	}
	updateAnimation = false;

}

void idRenderWindow::PreRender() {
	if (needsRender) {
		world->InitFromMap( NULL );
		idDict spawnArgs;
		spawnArgs.Set("classname", "light");
		spawnArgs.Set("name", "light_1");
		spawnArgs.Set("origin", lightOrigin.ToVec3().ToString());
		spawnArgs.Set("_color", lightColor.ToVec3().ToString());
		rLight = world->AllocRenderLight();
		gameEdit->ParseSpawnArgsToRenderLight( &spawnArgs, rLight );
		rLight->UpdateRenderLight();
		if ( !modelName[0] ) {
			common->Warning( "Window '%s' in gui '%s': no model set", GetName(), GetGui()->GetSourceFile() );
		}
		worldEntity = world->AllocRenderEntity();
		spawnArgs.Clear();
		spawnArgs.Set("classname", "func_static");
		spawnArgs.Set("model", modelName);
		spawnArgs.Set("origin", modelOrigin.c_str());
		gameEdit->ParseSpawnArgsToRenderEntity( &spawnArgs, worldEntity );
		if ( worldEntity->GetModel() ) {
			idVec3 v = modelRotate.ToVec3();
			worldEntity->SetAxis( v.ToMat3() );
			worldEntity->SetShaderParm( 0, 1.0f );
			worldEntity->SetShaderParm( 1, 1.0f );
			worldEntity->SetShaderParm( 2, 1.0f );
			worldEntity->SetShaderParm( 3, 1.0f );
			worldEntity->UpdateRenderEntity();
		}
		needsRender = false;
	}
}

void idRenderWindow::Render( int time ) {
	rLight->SetOrigin( lightOrigin.ToVec3() );
	rLight->SetShaderParm( SHADERPARM_RED, lightColor.x() );
	rLight->SetShaderParm( SHADERPARM_GREEN, lightColor.y() );
	rLight->SetShaderParm( SHADERPARM_BLUE, lightColor.z() );
	rLight->UpdateRenderLight();
	if ( worldEntity->GetModel() ) {
		if (updateAnimation) {
			BuildAnimation(time);
		}
		if (modelAnim) {
			if (time > animEndTime) {
				animEndTime = time + animLength;
			}
			gameEdit->ANIM_CreateAnimFrame( worldEntity->GetModel(), modelAnim, worldEntity->GetNumJoints(), worldEntity->GetJoints(), animLength - (animEndTime - time), vec3_origin, false );
		}
		worldEntity->SetAxis( idAngles(modelRotate.x(), modelRotate.y(), modelRotate.z()).ToMat3() );
		worldEntity->UpdateRenderEntity();
	}
}




void idRenderWindow::Draw(int time, float x, float y) {
	PreRender();
	Render(time);

	memset( &refdef, 0, sizeof( refdef ) );
	refdef.vieworg = viewOffset.ToVec3();;
	//refdef.vieworg.Set(-128, 0, 0);

	refdef.viewaxis.Identity();
	refdef.shaderParms[0] = 1;
	refdef.shaderParms[1] = 1;
	refdef.shaderParms[2] = 1;
	refdef.shaderParms[3] = 1;

	// Fullscreen GUIs occupy a centered 4:3 canvas inside the 16:9 display.
	// renderDef windows create independent 3D views, so map their horizontal
	// viewport into that same canvas to avoid stretching models such as Mars.
	const float guiScaleX = static_cast<float>( SCREEN_WIDTH * DISPLAY_ASPECT_HEIGHT ) /
		( SCREEN_HEIGHT * DISPLAY_ASPECT_WIDTH );
	const float guiOffsetX = SCREEN_WIDTH * ( 1.0f - guiScaleX ) * 0.5f;
	refdef.x = guiOffsetX + drawRect.x * guiScaleX;
	refdef.y = drawRect.y;
	refdef.width = drawRect.w * guiScaleX;
	refdef.height = drawRect.h;
	refdef.fov_x = 90;
	refdef.fov_y = 2 * atan((float)drawRect.h / drawRect.w) * idMath::M_RAD2DEG;

	refdef.time = time;
	world->RenderScene(&refdef);
}

void idRenderWindow::PostParse() {
	idWindow::PostParse();
}

// 
//  
idWinVar *idRenderWindow::GetWinVarByName(const char *_name, bool fixup, drawWin_t** owner ) {
// 
	if (idStr::Icmp(_name, "model") == 0) {
		return &modelName;
	}
	if (idStr::Icmp(_name, "anim") == 0) {
		return &animName;
	}
	if (idStr::Icmp(_name, "lightOrigin") == 0) {
		return &lightOrigin;
	}
	if (idStr::Icmp(_name, "lightColor") == 0) {
		return &lightColor;
	}
	if (idStr::Icmp(_name, "modelOrigin") == 0) {
		return &modelOrigin;
	}
	if (idStr::Icmp(_name, "modelRotate") == 0) {
		return &modelRotate;
	}
	if (idStr::Icmp(_name, "viewOffset") == 0) {
		return &viewOffset;
	}
	if (idStr::Icmp(_name, "needsRender") == 0) {
		return &needsRender;
	}

// 
//  
	return idWindow::GetWinVarByName(_name, fixup, owner);
// 
}

bool idRenderWindow::ParseInternalVar(const char *_name, idParser *src) {
	if (idStr::Icmp(_name, "animClass") == 0) {
		ParseString(src, animClass);
		return true;
	}
	return idWindow::ParseInternalVar(_name, src);
}
