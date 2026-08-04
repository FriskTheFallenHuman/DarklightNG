/*
===========================================================================

DarklightNG Source Code
Copyright (C) 2026 - Justin Marshall(aka IceColdDuke).

This file is part of the DarklightNG GPL source code.

===========================================================================
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"
#include "AmbientCubeMap.h"

static const int AMBIENT_CUBE_SIZE = 64;
static idList<idAmbientCubeMap *> ambientCubeMaps;
static idAmbientCubeMap *generatingAmbientCubeMap;
static idAmbientCubeMap *generatingSpecularCubeMap;

static idAmbientCubeMap *R_AmbientCubeOwnerForImage( idImage *image, bool specular ) {
	for ( int i = 0; i < ambientCubeMaps.Num(); i++ ) {
		idAmbientCubeMap *cube = ambientCubeMaps[i];
		if ( ( specular ? cube->GetSpecularCubeMap() : cube->GetAmbientCubeMap() ) == image ) {
			return cube;
		}
	}
	return specular ? generatingSpecularCubeMap : generatingAmbientCubeMap;
}

static void R_GenerateAmbientCubeImage( idImage *image ) {
	idAmbientCubeMap *owner = R_AmbientCubeOwnerForImage( image, false );
	if ( owner ) {
		owner->GenerateAmbientImage( image );
	} else {
		image->MakeDefault();
	}
}

static void R_GenerateSpecularCubeImage( idImage *image ) {
	idAmbientCubeMap *owner = R_AmbientCubeOwnerForImage( image, true );
	if ( owner ) {
		owner->GenerateSpecularImage( image );
	} else {
		image->MakeDefault();
	}
}

idAmbientCubeMap::idAmbientCubeMap( const char *mapName ) : name( mapName ) {
	FreeData();
}

void idAmbientCubeMap::FreeData() {
	ambientLights.Clear();
	indoors = false;
	environmentMap.Clear();
	ambientColor.Set( 0.5f, 0.5f, 0.5f );
	highLightColor.Set( 0.8f, 0.8f, 0.8f );
	minSpecAmbientColor.Set( 0.6f, 0.6f, 0.6f );
	minSpecShadowColor.Set( 0.5f, 0.5f, 0.5f );
	sunDirection.Zero();
	sunColor.Zero();
	brightness = 1.0f;
	ambientCubeMap = NULL;
	specularCubeMap = NULL;
	environmentCubeMap = NULL;
}

bool idAmbientCubeMap::ParseAmbientLight( idLexer &src ) {
	ambientLight_t light;
	idToken token;

	if ( !src.ExpectTokenString( "{" ) ) {
		return false;
	}
	while ( src.ReadToken( &token ) ) {
		if ( token == "}" ) {
			if ( light.name.IsEmpty() ) {
				light.name = va( "Light%i", ambientLights.Num() );
			}
			ambientLights.Append( light );
			return true;
		}
		if ( !token.Icmp( "color" ) ) {
			light.color.x = src.ParseFloat();
			light.color.y = src.ParseFloat();
			light.color.z = src.ParseFloat();
		} else if ( !token.Icmp( "direction" ) ) {
			light.direction.x = src.ParseFloat();
			light.direction.y = src.ParseFloat();
			light.direction.z = src.ParseFloat();
			light.direction.Normalize();
		} else if ( !token.Icmp( "brightness" ) ) {
			light.color *= src.ParseFloat();
		} else if ( !token.Icmp( "ambient" ) ) {
			light.ambient = src.ParseBool();
		} else if ( !token.Icmp( "specular" ) ) {
			light.specular = src.ParseBool();
		} else if ( !token.Icmp( "name" ) ) {
			if ( !src.ReadToken( &token ) ) {
				return false;
			}
			light.name = token;
		} else {
			src.Warning( "ambientCubeMap %s: unknown ambientLight token '%s'", name.c_str(), token.c_str() );
			return false;
		}
	}
	return false;
}

bool idAmbientCubeMap::Parse( idLexer &src ) {
	idToken token;
	FreeData();

	if ( !src.ExpectTokenString( "{" ) ) {
		return false;
	}
	while ( src.ReadToken( &token ) ) {
		if ( token == "}" ) {
			return true;
		}
		if ( !token.Icmp( "ambientLight" ) ) {
			if ( !ParseAmbientLight( src ) ) {
				return false;
			}
		} else if ( !token.Icmp( "indoors" ) ) {
			indoors = true;
		} else if ( !token.Icmp( "envMap" ) ) {
			if ( !src.ReadToken( &token ) ) {
				return false;
			}
			environmentMap = token;
		} else if ( !token.Icmp( "ambientColor" ) ) {
			ambientColor.x = src.ParseFloat();
			ambientColor.y = src.ParseFloat();
			ambientColor.z = src.ParseFloat();
		} else if ( !token.Icmp( "highLightColor" ) ) {
			highLightColor.x = src.ParseFloat();
			highLightColor.y = src.ParseFloat();
			highLightColor.z = src.ParseFloat();
		} else if ( !token.Icmp( "minSpecAmbientColor" ) ) {
			minSpecAmbientColor.x = src.ParseFloat();
			minSpecAmbientColor.y = src.ParseFloat();
			minSpecAmbientColor.z = src.ParseFloat();
		} else if ( !token.Icmp( "minSpecShadowColor" ) ) {
			minSpecShadowColor.x = src.ParseFloat();
			minSpecShadowColor.y = src.ParseFloat();
			minSpecShadowColor.z = src.ParseFloat();
		} else if ( !token.Icmp( "sunDirection" ) ) {
			sunDirection.x = src.ParseFloat();
			sunDirection.y = src.ParseFloat();
			sunDirection.z = src.ParseFloat();
			sunDirection.Normalize();
		} else if ( !token.Icmp( "sunColor" ) ) {
			sunColor.x = src.ParseFloat();
			sunColor.y = src.ParseFloat();
			sunColor.z = src.ParseFloat();
		} else if ( !token.Icmp( "brightness" ) ) {
			brightness = src.ParseFloat();
		} else {
			src.Warning( "ambientCubeMap %s: unknown token '%s'", name.c_str(), token.c_str() );
			return false;
		}
	}
	return false;
}

static void R_GetAmbientCubeVector( int face, int size, int x, int y, idVec3 &direction ) {
	const float sc = ( ( x + 0.5f ) / size ) * 2.0f - 1.0f;
	const float tc = ( ( y + 0.5f ) / size ) * 2.0f - 1.0f;
	switch ( face ) {
	case 0: direction.Set( 1.0f, -tc, -sc ); break;
	case 1: direction.Set( -1.0f, -tc, sc ); break;
	case 2: direction.Set( sc, 1.0f, tc ); break;
	case 3: direction.Set( sc, -1.0f, -tc ); break;
	case 4: direction.Set( sc, -tc, 1.0f ); break;
	default: direction.Set( -sc, -tc, -1.0f ); break;
	}
	direction.Normalize();
}

void idAmbientCubeMap::GenerateCubeImage( idImage *image, bool makeSpecular ) const {
	const int facePixels = AMBIENT_CUBE_SIZE * AMBIENT_CUBE_SIZE;
	float *floating = new float[6 * facePixels * 4];
	byte *pixels = new byte[6 * facePixels * 4];
	const byte *faces[6];
	int maximumChannel = 0;
	double channelTotal = 0.0;
	memset( floating, 0, 6 * facePixels * 4 * sizeof( float ) );

	for ( int face = 0; face < 6; face++ ) {
		faces[face] = pixels + face * facePixels * 4;
		for ( int y = 0; y < AMBIENT_CUBE_SIZE; y++ ) {
			for ( int x = 0; x < AMBIENT_CUBE_SIZE; x++ ) {
				idVec3 direction;
				R_GetAmbientCubeVector( face, AMBIENT_CUBE_SIZE, x, y, direction );
				float *sample = floating + ( face * facePixels + y * AMBIENT_CUBE_SIZE + x ) * 4;
				for ( int i = 0; i < ambientLights.Num(); i++ ) {
					const ambientLight_t &light = ambientLights[i];
					if ( makeSpecular ? !light.specular : !light.ambient ) {
						continue;
					}
					float contribution = Max( light.direction * direction, 0.0f );
					if ( makeSpecular ) {
						contribution = idMath::Pow( contribution, 16.0f );
					}
					sample[0] = Min( sample[0] + light.color.x * contribution, 1.0f );
					sample[1] = Min( sample[1] + light.color.y * contribution, 1.0f );
					sample[2] = Min( sample[2] + light.color.z * contribution, 1.0f );
				}
				sample[3] = 1.0f;
				byte *output = pixels + ( face * facePixels + y * AMBIENT_CUBE_SIZE + x ) * 4;
				for ( int channel = 0; channel < 4; channel++ ) {
					output[channel] = static_cast<byte>( idMath::ClampInt( 0, 255, idMath::Ftoi( sample[channel] * 255.0f ) ) );
				}
				for ( int channel = 0; channel < 3; channel++ ) {
					maximumChannel = Max( maximumChannel, static_cast<int>( output[channel] ) );
					channelTotal += output[channel];
				}
			}
		}
	}

	image->GenerateCubeImage( faces, AMBIENT_CUBE_SIZE, TF_LINEAR, false, TD_HIGH_QUALITY );
	common->DPrintf( "Generated %s cube %s: average %.1f/255, maximum %i/255\n",
		makeSpecular ? "specular" : "ambient", name.c_str(),
		channelTotal / ( 6.0 * facePixels * 3.0 ), maximumChannel );
	delete[] pixels;
	delete[] floating;
}

void idAmbientCubeMap::GenerateAmbientImage( idImage *image ) const {
	GenerateCubeImage( image, false );
}

void idAmbientCubeMap::GenerateSpecularImage( idImage *image ) const {
	GenerateCubeImage( image, true );
}

void idAmbientCubeMap::GenerateImages() {
	if ( !globalImages ) {
		return;
	}

	generatingAmbientCubeMap = this;
	ambientCubeMap = globalImages->ImageFromFunction( va( "_ambientCubeMap_%s", name.c_str() ), R_GenerateAmbientCubeImage );
	generatingAmbientCubeMap = NULL;

	generatingSpecularCubeMap = this;
	specularCubeMap = globalImages->ImageFromFunction( va( "_specularCubeMap_%s", name.c_str() ), R_GenerateSpecularCubeImage );
	generatingSpecularCubeMap = NULL;

	if ( !environmentMap.IsEmpty() ) {
		environmentCubeMap = globalImages->ImageFromFile( environmentMap, TF_DEFAULT, true, TR_CLAMP,
			TD_HIGH_QUALITY, CF_NATIVE );
	}
}

idAmbientCubeMap *R_FindAmbientCubeMap( const char *name, bool create ) {
	if ( !name || !name[0] ) {
		return NULL;
	}
	for ( int i = 0; i < ambientCubeMaps.Num(); i++ ) {
		if ( !idStr::Icmp( ambientCubeMaps[i]->GetName(), name ) ) {
			return ambientCubeMaps[i];
		}
	}
	if ( !create ) {
		return NULL;
	}
	idAmbientCubeMap *cube = new idAmbientCubeMap( name );
	ambientCubeMaps.Append( cube );
	return cube;
}

void R_ShutdownAmbientCubeMaps() {
	ambientCubeMaps.DeleteContents( true );
	generatingAmbientCubeMap = NULL;
	generatingSpecularCubeMap = NULL;
}
