// Copyright (C) 2007 Id Software, Inc.
//

#ifndef __GAME_ATMOSPHERE_H__
#define __GAME_ATMOSPHERE_H__

#include "AtmosphereRenderable.h"

class sdDeclAmbientCubeMap;

class sdAtmosphereInstance {
public:
	sdAtmosphereInstance( idDict &spawnArgs );
	~sdAtmosphereInstance();

	void Activate();
	void DeActivate();
	void Think();
	void SetDecl( const sdDeclAtmosphere *decl ) { atmosphereParms.atmosphere = decl; }
	const sdDeclAtmosphere *GetDecl() const { return atmosphereParms.atmosphere; }
	void SetFloodOrigin( const idVec3 &value ) { origin = value; }
	const idVec3 &GetFloodOrigin() const { return origin; }

private:
	sdAtmosphereRenderable::parms_t atmosphereParms;
	sdAtmosphereRenderable *renderable;
	bool active;
	idVec3 origin;
};

D3_CLASS()
class sdAtmosphere : public idEntity {
public:
	CLASS_PROTOTYPE( sdAtmosphere );

	sdAtmosphere();
	virtual ~sdAtmosphere();
	void Spawn();
	virtual void Think();
	void FreeModelDef();
	void FreeLightDef();
	idVec3 GetFogColor() const;
	const idVec3 &GetWindVector() const { return windVector; }
	static sdAtmosphere *GetAtmosphereInstance() { return currentAtmosphere; }
	static void SetAtmosphere_f( const idCmdArgs &args );
	static void GetAtmosphereLightDetails_f( const idCmdArgs &args );
	static void FloodAmbientCubeMap( const idVec3 &origin, const sdDeclAmbientCubeMap *ambientCubeMap );

	static idCVar a_windTimeScale;
	static sdAtmosphere *currentAtmosphere;

private:
	void UpdateWeather();
	sdAtmosphereInstance *currentAtmosphereInstance;
	idVec3 windVector;
	float windAngle;
	float windStrength;
};

D3_CLASS()
class sdAmbientLight : public idEntity {
public:
	CLASS_PROTOTYPE( sdAmbientLight );
	void Spawn();
};

#endif /* __GAME_ATMOSPHERE_H__ */
