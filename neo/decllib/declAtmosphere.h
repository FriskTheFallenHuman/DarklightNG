/*
===========================================================================

	ETQW atmosphere declaration, adapted to Darklight.

===========================================================================
*/

#ifndef __DECLATMOSPHERE_H__
#define __DECLATMOSPHERE_H__

#include "../framework/DeclManager.h"
#include "declAmbientCubeMap.h"

class idDeclFX;
class idImage;
class idRenderModel;
class idLexer;

const int NUM_CLOUD_LAYER_PARAMETERS = 12;
const int MAX_CLOUD_LAYERS = 10;

struct sdCloudLayer {
	sdCloudLayer() : material( NULL ), style( 0 ) { memset( parms, 0, sizeof( parms ) ); }
	const idMaterial *material;
	int style;
	float parms[NUM_CLOUD_LAYER_PARAMETERS];
};

struct sdPrecipitationParameters {
	enum precipitationType_e {
		PT_NONE,
		PT_RAIN,
		PT_SNOW,
		PT_SPLASH,
		PT_MODELRAIN,
		PT_MODELSNOW
	};

	precipitationType_e preType;
	int maxParticles;
	float heightMin, heightMax;
	float weightMin, weightMax;
	float windScale, gustWindScale;
	float fallMin, fallMax;
	float timeMin, timeMax;
	float tumbleStrength;
	float precipitationDistance;
	const idMaterial *material;
	idRenderModel *model;
	const idDeclFX *effect;

	void Default();
	bool Parse( idLexer &src );
	void Save( idFile_Memory &file ) const;
};

class sdDeclAtmosphere : public idDecl {
public:
	struct postProcessParms_t {
		idVec3 tint;
		float saturation;
		float contrast;
		idVec4 glareParms;
		idVec4 glareBases;
	};

	static const int NUM_PRECIP_LAYERS = 2;

					sdDeclAtmosphere();
	virtual			~sdDeclAtmosphere() {}
	virtual const char *DefaultDefinition() const;
	virtual bool		Parse( const char *text, const int textLength );
	virtual size_t		Size() const { return sizeof( sdDeclAtmosphere ); }
	virtual void		FreeData();
	static void		CacheFromDict( const idDict &dict );

	const idMaterial *GetSunMaterial() const { return sunMaterial; }
	const idVec3 &GetSunDirection() const { return sunDir; }
	float GetSunAzimuth() const { return sunAzimuth; }
	float GetSunZenith() const { return sunZenith; }
	const idVec3 &GetSunColor() const { return sunColor; }
	float GetSunHaloScale() const { return sunHaloScale; }
	float GetSunHaloBias() const { return sunHaloBias; }
	const idMaterial *GetSunSpriteMaterial() const { return sunSpriteMaterial; }
	float GetSunSpriteSize() const { return sunSpriteSize; }
	const idMaterial *GetSunFlareMaterial() const { return sunFlareMaterial; }
	float GetSunFlareSize() const { return sunFlareSize; }
	float GetSunFlareTime() const { return sunFlareTime; }
	bool EnableSunFlareAziZen() const { return enableSunFlareAziZen; }
	float GetSunFlareAzi() const { return sunFlareAzi; }
	float GetSunFlareZen() const { return sunFlareZen; }
	const postProcessParms_t &GetDefaultPostProcessParms() const { return defaultPostProcessParms; }
	postProcessParms_t &GetPostProcessParms() const { return postProcessParms; }
	float GetFogDistHalf() const { return fogDistHalf; }
	float GetFogHeightHalf() const { return fogHeightHalf; }
	float GetFogHeightOffset() const { return fogHeightOffset; }
	const idVec3 &GetFogColor() const { return fogColor; }
	float GetFogStart() const { return fogStart; }
	float GetFogEnd() const { return fogEnd; }
	const idMaterial *GetAtmosphereMaterial() const { return atmosphereMaterial; }
	const sdDeclAmbientCubeMap *GetAmbientCubeMap() const { return ambientCubeMap; }
	idImage *GetSkyGradientImage() const { return skyGradientImage; }
	float GetFarClip() const { return farClip; }
	bool IsNight() const { return isNight; }
	bool DrawAtmosphereLast() const { return drawAtmosphereLast; }
	const idVec3 &GetMinSpecShadowColor() const { return minSpecShadowColor; }
	const idList<sdCloudLayer> &GetCloudLayers() const { return cloudLayers; }
	const sdPrecipitationParameters &GetPrecipitation( int layer ) const { return precipitation[layer]; }
	float GetWindAngle() const { return windAngle; }
	float GetWindAngleDev() const { return windAngleDev; }
	float GetWindStrength() const { return windStrength; }
	float GetWindStrengthDev() const { return windStrengthDev; }
	bool IsModified() const { return modified; }
	void ClearModified() { modified = false; }

	void SetSunMaterial( const idMaterial *value ) { sunMaterial = value; modified = true; }
	void SetSunDirection( const idVec3 &value ) { sunDir = value; modified = true; }
	void SetSunAzimuth( float value ) { sunAzimuth = value; UpdateSunDirFromAziZen(); modified = true; }
	void SetSunZenith( float value ) { sunZenith = value; UpdateSunDirFromAziZen(); modified = true; }
	void SetSunColor( const idVec3 &value ) { sunColor = value; modified = true; }
	void SetSunHaloScale( float value ) { sunHaloScale = value; modified = true; }
	void SetSunHaloBias( float value ) { sunHaloBias = value; modified = true; }
	void SetSunSpriteMaterial( const idMaterial *value ) { sunSpriteMaterial = value; modified = true; }
	void SetSunSpriteSize( float value ) { sunSpriteSize = value; modified = true; }
	void SetSunFlareMaterial( const idMaterial *value ) { sunFlareMaterial = value; modified = true; }
	void SetSunFlareSize( float value ) { sunFlareSize = value; modified = true; }
	void SetSunFlareTime( float value ) { sunFlareTime = value; modified = true; }
	void SetEnableSunFlareAziZen( bool value ) { enableSunFlareAziZen = value; modified = true; }
	void SetSunFlareAzi( float value ) { sunFlareAzi = value; modified = true; }
	void SetSunFlareZen( float value ) { sunFlareZen = value; modified = true; }
	void SetPostProcessParms( const postProcessParms_t &value ) { postProcessParms = defaultPostProcessParms = value; modified = true; }
	void SetFogDistHalf( float value ) { fogDistHalf = value; modified = true; }
	void SetFogHeightHalf( float value ) { fogHeightHalf = value; modified = true; }
	void SetFogHeightOffset( float value ) { fogHeightOffset = value; modified = true; }
	void SetFogColor( const idVec3 &value ) { fogColor = value; modified = true; }
	void SetFogStart( float value ) { fogStart = value; modified = true; }
	void SetFogEnd( float value ) { fogEnd = value; modified = true; }
	void SetAtmosphereMaterial( const idMaterial *value ) { atmosphereMaterial = value; modified = true; }
	void SetAmbientCubeMap( const sdDeclAmbientCubeMap *value ) { ambientCubeMap = value; modified = true; }
	bool SetSkyGradientImage( const char *imageName );
	void SetFarClip( float value ) { farClip = value; modified = true; }
	void SetIsNight( bool value ) { isNight = value; modified = true; }
	void SetDrawAtmosphereLast( bool value ) { drawAtmosphereLast = value; modified = true; }
	void SetMinSpecShadowColor( const idVec3 &value ) { minSpecShadowColor = value; modified = true; }
	int AddCloudLayer( const sdCloudLayer &value ) { modified = true; return cloudLayers.Append( value ); }
	void UpdateCloudLayer( int layer, const sdCloudLayer &value ) { cloudLayers[layer] = value; modified = true; }
	void RemoveCloudLayer( int layer ) { cloudLayers.RemoveIndex( layer ); modified = true; }
	void SetPrecipitation( int layer, const sdPrecipitationParameters &value ) { precipitation[layer] = value; modified = true; }
	void SetWindAngle( float value ) { windAngle = value; modified = true; }
	void SetWindAngleDev( float value ) { windAngleDev = value; modified = true; }
	void SetWindStrength( float value ) { windStrength = value; modified = true; }
	void SetWindStrengthDev( float value ) { windStrengthDev = value; modified = true; }
	void Save( idFile_Memory &file ) const;
	void Save();

private:
	bool ParsePostProcessParms( idLexer &src );
	bool ParseCloudLayer( idLexer &src );
	bool ParsePrecipitationLayer( idLexer &src );
	void RebuildTextSource( idFile_Memory &file ) const;
	void UpdateSunDirFromAziZen();

	bool modified;
	const idMaterial *sunMaterial;
	idVec3 sunDir;
	float sunAzimuth, sunZenith;
	idVec3 sunColor;
	float sunHaloScale, sunHaloBias;
	const idMaterial *sunSpriteMaterial;
	float sunSpriteSize;
	const idMaterial *sunFlareMaterial;
	float sunFlareSize, sunFlareTime;
	bool enableSunFlareAziZen;
	float sunFlareAzi, sunFlareZen;
	postProcessParms_t defaultPostProcessParms;
	mutable postProcessParms_t postProcessParms;
	float fogDistHalf, fogHeightHalf, fogHeightOffset;
	idVec3 fogColor;
	float fogStart, fogEnd;
	const idMaterial *atmosphereMaterial;
	const sdDeclAmbientCubeMap *ambientCubeMap;
	idImage *skyGradientImage;
	float farClip;
	bool isNight, drawAtmosphereLast;
	idVec3 minSpecShadowColor;
	float windAngle, windAngleDev, windStrength, windStrengthDev;
	idList<sdCloudLayer> cloudLayers;
	int numPrecipLayers;
	sdPrecipitationParameters precipitation[NUM_PRECIP_LAYERS];
};

const sdDeclAtmosphere *R_FindAtmosphere( const char *name, bool makeDefault = true );

#endif /* !__DECLATMOSPHERE_H__ */
