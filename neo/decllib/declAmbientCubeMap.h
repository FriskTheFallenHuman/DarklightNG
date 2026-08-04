/*
===========================================================================

	ETQW ambient cube-map declaration, adapted to the Darklight renderer.

===========================================================================
*/

#ifndef __DECLAMBIENTCUBEMAP_H__
#define __DECLAMBIENTCUBEMAP_H__

#include "../framework/DeclManager.h"

class idImage;
class idLexer;

class sdDeclAmbientCubeMap : public idDecl {
public:
	struct ambientLight_t {
		ambientLight_t() : dir( 1.0f, 0.0f, 0.0f ), color( 1.0f, 1.0f, 1.0f ),
			specular( true ), ambient( true ) {}

		void SetAngles( const idVec2 &angles ) {
			dir.x = idMath::Cos( DEG2RAD( angles.x ) ) * idMath::Sin( DEG2RAD( angles.y ) );
			dir.y = idMath::Sin( DEG2RAD( angles.x ) ) * idMath::Sin( DEG2RAD( angles.y ) );
			dir.z = idMath::Cos( DEG2RAD( angles.y ) );
		}
		idVec2 GetAngles() const {
			return idVec2( RAD2DEG( idMath::ATan( dir.y, dir.x ) ), RAD2DEG( idMath::ACos( dir.z ) ) );
		}

		idVec3 dir;
		idVec3 color;
		idStr name;
		bool specular;
		bool ambient;
	};

					sdDeclAmbientCubeMap();
	virtual			~sdDeclAmbientCubeMap();

	virtual const char *DefaultDefinition() const;
	virtual bool		Parse( const char *text, const int textLength );
	virtual size_t		Size() const { return sizeof( sdDeclAmbientCubeMap ); }
	virtual void		FreeData();

	static void		CacheFromDict( const idDict &dict );

	const idList<ambientLight_t> &GetAmbientLights() const { return ambientLights; }
	bool				IsIndoors() const { return indoors; }
	const char *		GetEnvironmentMap() const { return envMap.c_str(); }
	idVec3				GetAmbientColor() const { return ambientColor; }
	idVec3				GetHighLightColor() const { return highLightColor; }
	idVec4				GetAvgAmbientColor() const { return avgAmbientColor; }
	float				GetBrightness() const { return brightness; }
	idVec3				GetMinSpecAmbientColor() const { return minSpecAmbientColor; }
	idVec3				GetMinSpecShadowColor() const { return minSpecShadowColor; }

	idImage *			GetAmbientCubeMap() const { return ambientCubeMap; }
	idImage *			GetLightCubeMap() const { return lightCubeMap; }
	idImage *			GetSpecularCubeMap() const { return specularCubeMap; }
	idImage *			GetEnvironmentCubeMap() const { return environmentCubeMap; }
	idImage *			GetGradientMap() const { return gradientMap; }

	void				SetSunParameters( const idVec3 &direction, const idVec3 &color );
	void				GenerateImages();
	void				GenerateAmbientImage( idImage *image ) const;
	void				GenerateLightImage( idImage *image ) const;
	void				GenerateSpecularImage( idImage *image ) const;
	void				GenerateGradientImage( idImage *image ) const;

	int					AddAmbientLight( const ambientLight_t &light ) { return ambientLights.Append( light ); }
	void				UpdateAmbientLight( int index, const ambientLight_t &value ) { ambientLights[index] = value; }
	void				RemoveAmbientLight( int index ) { ambientLights.RemoveIndex( index ); }
	void				SetIndoors( bool value ) { indoors = value; }
	void				SetEnvironmentMap( const char *value ) { envMap = value; }
	void				SetAmbientColor( const idVec3 &value ) { ambientColor = value; }
	void				SetHighLightColor( const idVec3 &value ) { highLightColor = value; }
	void				SetBrightness( float value ) { brightness = value; }
	void				SetMinSpecAmbientColor( const idVec3 &value ) { minSpecAmbientColor = value; }
	void				SetMinSpecShadowColor( const idVec3 &value ) { minSpecShadowColor = value; }
	bool				Save();

private:
	enum generatedCube_t {
		GC_AMBIENT,
		GC_LIGHT,
		GC_SPECULAR
	};

	bool				ParseAmbientLight( idLexer &src );
	bool				RebuildTextSource();
	void				GenerateCubeImage( idImage *image, generatedCube_t type ) const;

	idList<ambientLight_t> ambientLights;
	bool				indoors;
	idStr				envMap;
	idVec3				ambientColor;
	idVec3				highLightColor;
	float				brightness;
	idVec3				sunDirection;
	idVec3				sunColor;
	idVec4				avgAmbientColor;
	idVec3				minSpecAmbientColor;
	idVec3				minSpecShadowColor;
	idImage *			ambientCubeMap;
	idImage *			lightCubeMap;
	idImage *			specularCubeMap;
	idImage *			environmentCubeMap;
	idImage *			gradientMap;
};

// Compatibility name used by the first Darklight integration. New code should
// use the original ETQW declaration class name above.
typedef sdDeclAmbientCubeMap idAmbientCubeMap;

sdDeclAmbientCubeMap *R_FindAmbientCubeMap( const char *name, bool makeDefault = true );
void R_ShutdownAmbientCubeMaps();

#endif /* !__DECLAMBIENTCUBEMAP_H__ */
