// Copyright (C) 2007 Id Software, Inc.
//

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "Game_local.h"
#include "Atmosphere.h"
#include "../decllib/declAtmosphere.h"
#include "../decllib/declAmbientCubeMap.h"

static const sdDeclAtmosphere *Game_FindAtmosphere( const char *name, bool makeDefault ) {
	return name && name[0] ? static_cast<const sdDeclAtmosphere *>(
		declManager->FindType( DECL_ATMOSPHERE, name, makeDefault ) ) : NULL;
}

static const sdDeclAmbientCubeMap *Game_FindAmbientCubeMap( const char *name, bool makeDefault ) {
	return name && name[0] ? static_cast<const sdDeclAmbientCubeMap *>(
		declManager->FindType( DECL_AMBIENTCUBEMAP, name, makeDefault ) ) : NULL;
}

idCVar sdAtmosphere::a_windTimeScale( "a_windTimeScale", "0.00005", CVAR_GAME | CVAR_FLOAT, "speed at which atmosphere wind changes" );
sdAtmosphere *sdAtmosphere::currentAtmosphere = NULL;

sdAtmosphereInstance::sdAtmosphereInstance( idDict &spawnArgs ) : renderable( NULL ), active( false ) {
	const char *declName = spawnArgs.GetString( "atmospheredecl" );
	if ( !declName[0] ) declName = spawnArgs.GetString( "atmosphereDecl", "default" );
	atmosphereParms.atmosphere = Game_FindAtmosphere( declName, false );
	spawnArgs.GetVector( "origin", "0 0 0", atmosphereParms.skyOrigin );
	origin = atmosphereParms.skyOrigin;
}

sdAtmosphereInstance::~sdAtmosphereInstance() {
	DeActivate();
}

void sdAtmosphereInstance::Activate() {
	if ( !atmosphereParms.atmosphere ) return;
	if ( !renderable ) renderable = new sdAtmosphereRenderable( gameRenderWorld );
	gameRenderWorld->SetAtmosphere( atmosphereParms.atmosphere );
	renderable->UpdateAtmosphere( atmosphereParms );
	active = true;
}

void sdAtmosphereInstance::DeActivate() {
	if ( !active && !renderable ) return;
	if ( gameRenderWorld->GetAtmosphere() == atmosphereParms.atmosphere ) {
		gameRenderWorld->SetAtmosphere( NULL );
	}
	delete renderable;
	renderable = NULL;
	active = false;
}

void sdAtmosphereInstance::Think() {
	if ( active && atmosphereParms.atmosphere && atmosphereParms.atmosphere->IsModified() ) {
		renderable->UpdateAtmosphere( atmosphereParms );
		const_cast<sdDeclAtmosphere *>( atmosphereParms.atmosphere )->ClearModified();
	}
}

sdAtmosphere::sdAtmosphere() : currentAtmosphereInstance( NULL ), windAngle( 0.0f ), windStrength( 0.0f ) {
	windVector.Zero();
}

sdAtmosphere::~sdAtmosphere() {
	delete currentAtmosphereInstance;
	currentAtmosphereInstance = NULL;
	if ( currentAtmosphere == this ) currentAtmosphere = NULL;
}

void sdAtmosphere::Spawn() {
	if ( currentAtmosphere ) {
		gameLocal.Warning( "ignoring additional atmosphere entity '%s'", GetName() );
		PostEventMS( &EV_Remove, 0 );
		return;
	}

	currentAtmosphereInstance = new sdAtmosphereInstance( spawnArgs );
	if ( !currentAtmosphereInstance->GetDecl() ) {
		gameLocal.Warning( "atmosphere entity '%s' references missing decl '%s'", GetName(), spawnArgs.GetString( "atmospheredecl" ) );
		return;
	}
	currentAtmosphere = this;
	currentAtmosphereInstance->SetFloodOrigin( GetPhysics()->GetOrigin() );
	currentAtmosphereInstance->Activate();
	FloodAmbientCubeMap( GetPhysics()->GetOrigin(), currentAtmosphereInstance->GetDecl()->GetAmbientCubeMap() );
	BecomeActive( TH_THINK );
	gameLocal.Printf( "Atmosphere '%s' active from %s\n", currentAtmosphereInstance->GetDecl()->GetName(), currentAtmosphereInstance->GetDecl()->GetFileName() );
}

void sdAtmosphere::Think() {
	if ( !currentAtmosphereInstance ) return;
	UpdateWeather();
	currentAtmosphereInstance->Think();
}

void sdAtmosphere::FreeModelDef() {
	idEntity::FreeModelDef();
	if ( currentAtmosphereInstance ) currentAtmosphereInstance->DeActivate();
}

void sdAtmosphere::FreeLightDef() {
	if ( currentAtmosphereInstance ) currentAtmosphereInstance->DeActivate();
}

void sdAtmosphere::UpdateWeather() {
	const sdDeclAtmosphere *atmosphere = currentAtmosphereInstance->GetDecl();
	const float lerp = idMath::Sin( gameLocal.time * a_windTimeScale.GetFloat() );
	windAngle = atmosphere->GetWindAngle() + lerp * atmosphere->GetWindAngleDev();
	float sine, cosine;
	idMath::SinCos( DEG2RAD( windAngle ), sine, cosine );
	windStrength = atmosphere->GetWindStrength() +
		idMath::Sin( gameLocal.time * a_windTimeScale.GetFloat() + 1.23f ) * atmosphere->GetWindStrengthDev();
	windVector.Set( cosine * windStrength, sine * windStrength, 0.0f );
}

idVec3 sdAtmosphere::GetFogColor() const {
	return currentAtmosphereInstance && currentAtmosphereInstance->GetDecl() ?
		currentAtmosphereInstance->GetDecl()->GetFogColor() : vec3_zero;
}

void sdAtmosphere::FloodAmbientCubeMap( const idVec3 &origin, const sdDeclAmbientCubeMap *ambientCubeMap ) {
	if ( !ambientCubeMap ) return;
	const int sourceArea = gameRenderWorld->PointInArea( origin );
	for ( int area = 0; area < gameRenderWorld->NumAreas(); area++ ) {
		if ( area == sourceArea || ( sourceArea >= 0 && gameRenderWorld->AreasAreConnected( sourceArea, area, PS_BLOCK_VIEW ) ) ) {
			gameRenderWorld->SetAreaAmbientCubeMap( area, ambientCubeMap );
		}
	}
}

void sdAtmosphere::SetAtmosphere_f( const idCmdArgs &args ) {
	if ( args.Argc() != 2 || !currentAtmosphere || !currentAtmosphere->currentAtmosphereInstance ) {
		common->Printf( "usage: setAtmosphere <atmosphereDecl>\n" );
		return;
	}
	const sdDeclAtmosphere *atmosphere = Game_FindAtmosphere( args.Argv( 1 ), false );
	if ( !atmosphere ) return;
	currentAtmosphere->currentAtmosphereInstance->DeActivate();
	currentAtmosphere->currentAtmosphereInstance->SetDecl( atmosphere );
	currentAtmosphere->currentAtmosphereInstance->Activate();
	FloodAmbientCubeMap( currentAtmosphere->currentAtmosphereInstance->GetFloodOrigin(), atmosphere->GetAmbientCubeMap() );
}

void sdAtmosphere::GetAtmosphereLightDetails_f( const idCmdArgs & ) {
	const sdDeclAtmosphere *atmosphere = gameRenderWorld->GetAtmosphere();
	if ( !atmosphere ) {
		common->Printf( "No atmosphere present.\n" );
		return;
	}
	common->Printf( "Sun Direction: %s\nSun Color: %s\n", atmosphere->GetSunDirection().ToString(), atmosphere->GetSunColor().ToString() );
}

void sdAmbientLight::Spawn() {
	const char *name = spawnArgs.GetString( "ambientCubeMap" );
	const sdDeclAmbientCubeMap *ambientCubeMap = Game_FindAmbientCubeMap( name, false );
	if ( ambientCubeMap ) sdAtmosphere::FloodAmbientCubeMap( GetPhysics()->GetOrigin(), ambientCubeMap );
	else gameLocal.Warning( "sdAmbientLight '%s' has no valid ambientCubeMap", GetName() );
	PostEventMS( &EV_Remove, 0 );
}
