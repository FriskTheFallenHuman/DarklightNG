// Copyright (C) 2007 Id Software, Inc.
//

#ifndef __GAME_ATMOSPHERERENDERABLE_H__
#define __GAME_ATMOSPHERERENDERABLE_H__

#include "../framework/CVarSystem.h"

class idRenderWorld;
class idRenderModel;
class idRenderLight;
class idRenderEntity;
class sdDeclAtmosphere;

class sdAtmosphereRenderable {
public:
	struct parms_t {
		parms_t() : atmosphere( NULL ), boxDomeModel( NULL ), oldDomeModel( NULL ), mapId( 0 ) { skyOrigin.Zero(); }

		const sdDeclAtmosphere *atmosphere;
		idRenderModel *boxDomeModel;
		idRenderModel *oldDomeModel;
		idVec3 skyOrigin;
		int mapId;
	};

	sdAtmosphereRenderable( idRenderWorld *renderWorld );
	virtual ~sdAtmosphereRenderable();

	void UpdateAtmosphere( parms_t &parms );
	void DrawPostProcess( const renderView_t *view, float x, float y, float w, float h ) const;
	void FreeModelDef();
	void FreeLightDef();
	bool IsLightValid() const { return skyLight != NULL; }

	static idCVar a_sun;
	static idCVar a_sunShadows;
	static idCVar a_glowScale;
	static idCVar a_glowBaseScale;
	static idCVar a_glowThresh;
	static idCVar a_glowLuminanceDependency;
	static idCVar a_glowSunPower;
	static idCVar a_glowSunScale;
	static idCVar a_glowSunBaseScale;

private:
	void UpdateCelestialBody( parms_t &parms );
	void UpdateCloudLayers( parms_t &parms );

	idRenderWorld *renderWorld;
	idRenderLight *skyLight;
	idRenderEntity *skyLightSprite;
	idRenderEntity *skyLightGlowSprite;
	const idMaterial *postProcessMaterial;
	idRenderModel *spriteModel;
	float currentScale;
	float currentAlpha;
	float sunFlareMaxSize;
	float sunFlareTime;
};

#endif /* __GAME_ATMOSPHERERENDERABLE_H__ */
