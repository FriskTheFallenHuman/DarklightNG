/*
===========================================================================

DarklightNG Source Code
Copyright (C) 2026 - Justin Marshall(aka IceColdDuke).

This file is part of the DarklightNG GPL source code.

===========================================================================
*/

#ifndef __AMBIENTCUBEMAP_H__
#define __AMBIENTCUBEMAP_H__

class idLexer;
class idImage;

/*
===============================================================================

	ETQW-style directional ambient lighting declaration.

	The source data is six (or more) directional light samples.  The renderer
	bakes those samples into diffuse and specular cube maps on demand, just as
	the original renderer did, instead of treating the marker as a finite light.

===============================================================================
*/
class idAmbientCubeMap {
public:
	struct ambientLight_t {
		idVec3			direction;
		idVec3			color;
		idStr			name;
		bool			ambient;
		bool			specular;

		ambientLight_t() : direction( 1.0f, 0.0f, 0.0f ), color( 1.0f, 1.0f, 1.0f ),
			ambient( true ), specular( true ) {}
	};

	explicit			idAmbientCubeMap( const char *name );

	bool				Parse( idLexer &src );
	void				GenerateImages();

	const char *		GetName() const { return name.c_str(); }
	idImage *			GetAmbientCubeMap() const { return ambientCubeMap; }
	idImage *			GetSpecularCubeMap() const { return specularCubeMap; }
	idImage *			GetEnvironmentCubeMap() const { return environmentCubeMap; }
	float				GetBrightness() const { return brightness; }
	bool				IsIndoors() const { return indoors; }

	void				GenerateAmbientImage( idImage *image ) const;
	void				GenerateSpecularImage( idImage *image ) const;

private:
	bool				ParseAmbientLight( idLexer &src );
	void				FreeData();
	void				GenerateCubeImage( idImage *image, bool specular ) const;

	idStr				name;
	idList<ambientLight_t> ambientLights;
	bool				indoors;
	idStr				environmentMap;
	idVec3				ambientColor;
	idVec3				highLightColor;
	idVec3				minSpecAmbientColor;
	idVec3				minSpecShadowColor;
	idVec3				sunDirection;
	idVec3				sunColor;
	float				brightness;

	idImage *			ambientCubeMap;
	idImage *			specularCubeMap;
	idImage *			environmentCubeMap;
};

idAmbientCubeMap *R_FindAmbientCubeMap( const char *name, bool create = true );
void R_ShutdownAmbientCubeMaps();

#endif /* !__AMBIENTCUBEMAP_H__ */
