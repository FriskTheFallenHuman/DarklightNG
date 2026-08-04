// Copyright (C) 2007 Id Software, Inc.
//

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "Game_local.h"
#include "AtmosphereRenderable.h"
#include "../decllib/declAtmosphere.h"
#include "../renderer/ModelManager.h"

idCVar sdAtmosphereRenderable::a_sun( "a_sun", "85", CVAR_GAME | CVAR_FLOAT, "atmosphere sun intensity" );
idCVar sdAtmosphereRenderable::a_sunShadows( "a_sunShadows", "0", CVAR_GAME | CVAR_BOOL | CVAR_ARCHIVE, "enable realtime shadows from the atmosphere sun" );
idCVar sdAtmosphereRenderable::a_glowScale( "a_glowScale", "0.25", CVAR_GAME | CVAR_FLOAT, "blurred image contribution factor" );
idCVar sdAtmosphereRenderable::a_glowBaseScale( "a_glowBaseScale", "0.21", CVAR_GAME | CVAR_FLOAT, "original image contribution factor" );
idCVar sdAtmosphereRenderable::a_glowThresh( "a_glowThresh", "0.0", CVAR_GAME | CVAR_FLOAT, "atmosphere glow threshold" );
idCVar sdAtmosphereRenderable::a_glowLuminanceDependency( "a_glowLuminanceDependency", "1.0", CVAR_GAME | CVAR_FLOAT, "glow luminance dependency" );
idCVar sdAtmosphereRenderable::a_glowSunPower( "a_glowSunPower", "16", CVAR_GAME | CVAR_FLOAT, "sun glow exponent" );
idCVar sdAtmosphereRenderable::a_glowSunScale( "a_glowSunScale", "0.0", CVAR_GAME | CVAR_FLOAT, "sun glow scale" );
idCVar sdAtmosphereRenderable::a_glowSunBaseScale( "a_glowSunBaseScale", "0.0", CVAR_GAME | CVAR_FLOAT, "sun base glow scale" );

sdAtmosphereRenderable::sdAtmosphereRenderable( idRenderWorld *world ) :
	renderWorld( world ), skyLight( NULL ), skyLightSprite( NULL ), skyLightGlowSprite( NULL ),
	postProcessMaterial( NULL ), spriteModel( NULL ), currentScale( 0.0f ), currentAlpha( 0.0f ),
	sunFlareMaxSize( 0.0f ), sunFlareTime( 0.0f ) {
	postProcessMaterial = declManager->FindMaterial( "postprocess/glow", false );
	spriteModel = renderModelManager->FindModel( "_SPRITE" );
}

sdAtmosphereRenderable::~sdAtmosphereRenderable() {
	FreeLightDef();
	FreeModelDef();
}

void sdAtmosphereRenderable::UpdateAtmosphere( parms_t &parms ) {
	if ( !parms.atmosphere ) {
		FreeLightDef();
		FreeModelDef();
		return;
	}
	UpdateCelestialBody( parms );
	UpdateCloudLayers( parms );
}

void sdAtmosphereRenderable::DrawPostProcess( const renderView_t *, float, float, float, float ) const {
	// Darklight composites atmospheric extinction in draw_atmosphere.cpp.
}

void sdAtmosphereRenderable::FreeModelDef() {
	if ( skyLightSprite ) {
		renderWorld->FreeRenderEntity( skyLightSprite );
		skyLightSprite = NULL;
	}
	if ( skyLightGlowSprite ) {
		renderWorld->FreeRenderEntity( skyLightGlowSprite );
		skyLightGlowSprite = NULL;
	}
}

void sdAtmosphereRenderable::FreeLightDef() {
	if ( skyLight ) {
		renderWorld->FreeRenderLight( skyLight );
		skyLight = NULL;
	}
}

static void SetAtmosphereSprite( idRenderEntity *entity, idRenderModel *model,
		const idMaterial *material, const idVec3 &origin, const idVec3 &color, float size ) {
	entity->SetOrigin( origin );
	entity->SetAxis( mat3_identity );
	entity->SetModel( model );
	entity->SetCustomShader( material );
	entity->SetNoShadow( true );
	entity->SetNoSelfShadow( true );
	entity->SetNoDynamicInteractions( true );
	entity->SetShaderParm( SHADERPARM_RED, color.x );
	entity->SetShaderParm( SHADERPARM_GREEN, color.y );
	entity->SetShaderParm( SHADERPARM_BLUE, color.z );
	entity->SetShaderParm( SHADERPARM_ALPHA, 1.0f );
	entity->SetShaderParm( SHADERPARM_SPRITE_WIDTH, size );
	entity->SetShaderParm( SHADERPARM_SPRITE_HEIGHT, size );
	entity->SetBounds( model->Bounds( entity ) );
	entity->UpdateRenderEntity( true );
}

void sdAtmosphereRenderable::UpdateCelestialBody( parms_t &parms ) {
	const sdDeclAtmosphere *atmosphere = parms.atmosphere;
	idVec3 sunDirection = atmosphere->GetSunDirection();
	if ( sunDirection.Normalize() == 0.0f ) {
		sunDirection.Set( 0.0f, 0.0f, 1.0f );
	}
	const float sunIntensity = Min( 1.0f, 0.3f + a_sun.GetFloat() / 80.0f );
	const idVec3 sunColor = atmosphere->GetSunColor() * sunIntensity;

	if ( !skyLight ) {
		skyLight = renderWorld->AllocRenderLight();
	}
	skyLight->SetOrigin( parms.skyOrigin );
	skyLight->SetAxis( mat3_identity );
	skyLight->SetPointLight( true );
	skyLight->SetParallel( true );
	skyLight->SetLightCenter( sunDirection );
	skyLight->SetLightRadius( idVec3( 50000.0f, 50000.0f, 50000.0f ) );
	skyLight->SetShader( atmosphere->GetSunMaterial() );
	skyLight->SetShaderParm( SHADERPARM_RED, sunColor.x );
	skyLight->SetShaderParm( SHADERPARM_GREEN, sunColor.y );
	skyLight->SetShaderParm( SHADERPARM_BLUE, sunColor.z );
	skyLight->SetShaderParm( SHADERPARM_ALPHA, 1.0f );
	skyLight->SetNoShadows( !a_sunShadows.GetBool() || sunColor.LengthSqr() <= Square( 2.0f / 255.0f ) );
	skyLight->SetBakedLight( false );
	skyLight->UpdateRenderLight( true );

	// ETQW offsets sprites in the model shader.  Doom 3's sprite model has no
	// offset parm, so place it explicitly along the same sun direction.
	const idVec3 spriteOrigin = parms.skyOrigin + sunDirection * 3000.0f;
	if ( atmosphere->GetSunSpriteMaterial() && spriteModel ) {
		if ( !skyLightSprite ) skyLightSprite = renderWorld->AllocRenderEntity();
		const float size = idMath::ClampFloat( 32.0f, 2048.0f, atmosphere->GetSunSpriteSize() * 0.08f );
		SetAtmosphereSprite( skyLightSprite, spriteModel, atmosphere->GetSunSpriteMaterial(), spriteOrigin, sunColor, size );
	}

	if ( atmosphere->GetSunFlareMaterial() && atmosphere->GetSunFlareSize() > 0.0f && spriteModel ) {
		if ( !skyLightGlowSprite ) skyLightGlowSprite = renderWorld->AllocRenderEntity();
		sunFlareMaxSize = atmosphere->GetSunFlareSize();
		sunFlareTime = atmosphere->GetSunFlareTime();
		const float size = idMath::ClampFloat( 32.0f, 3072.0f, sunFlareMaxSize * 0.08f );
		SetAtmosphereSprite( skyLightGlowSprite, spriteModel, atmosphere->GetSunFlareMaterial(), spriteOrigin, sunColor, size );
	}
}

void sdAtmosphereRenderable::UpdateCloudLayers( parms_t & ) {
	// The Valley conversion puts the native cloud/sky imagery on the enclosing
	// sky shell.  The original method remains the single update point so a
	// dedicated ETQW dome model can be attached later without entity changes.
}
