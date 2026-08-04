
/*
===========================================================================

	ETQW ambient cube-map declaration, adapted to the Darklight renderer.

===========================================================================
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "declAmbientCubeMap.h"
#include "../renderer/tr_local.h"

static const int BAKEDLIGHT_SIZE = 64;
static const int GRADIENT_SIZE = 16;

static idList<sdDeclAmbientCubeMap *> ambientCubeDecls;
static sdDeclAmbientCubeMap *generatingAmbientCubeMap;
static sdDeclAmbientCubeMap *generatingLightCubeMap;
static sdDeclAmbientCubeMap *generatingSpecularCubeMap;
static sdDeclAmbientCubeMap *generatingGradientMap;

static sdDeclAmbientCubeMap *R_AmbientCubeOwnerForImage( idImage *image, int type ) {
	for ( int i = 0; i < ambientCubeDecls.Num(); i++ ) {
		sdDeclAmbientCubeMap *cube = ambientCubeDecls[i];
		idImage *candidate = type == 0 ? cube->GetAmbientCubeMap() :
			type == 1 ? cube->GetLightCubeMap() :
			type == 2 ? cube->GetSpecularCubeMap() : cube->GetGradientMap();
		if ( candidate == image ) {
			return cube;
		}
	}
	return type == 0 ? generatingAmbientCubeMap : type == 1 ? generatingLightCubeMap :
		type == 2 ? generatingSpecularCubeMap : generatingGradientMap;
}

static void R_GenerateAmbientCubeImage( idImage *image ) {
	sdDeclAmbientCubeMap *owner = R_AmbientCubeOwnerForImage( image, 0 );
	owner ? owner->GenerateAmbientImage( image ) : image->MakeDefault();
}

static void R_GenerateLightCubeImage( idImage *image ) {
	sdDeclAmbientCubeMap *owner = R_AmbientCubeOwnerForImage( image, 1 );
	owner ? owner->GenerateLightImage( image ) : image->MakeDefault();
}

static void R_GenerateSpecularCubeImage( idImage *image ) {
	sdDeclAmbientCubeMap *owner = R_AmbientCubeOwnerForImage( image, 2 );
	owner ? owner->GenerateSpecularImage( image ) : image->MakeDefault();
}

static void R_GenerateGradientImage( idImage *image ) {
	sdDeclAmbientCubeMap *owner = R_AmbientCubeOwnerForImage( image, 3 );
	owner ? owner->GenerateGradientImage( image ) : image->MakeDefault();
}

sdDeclAmbientCubeMap::sdDeclAmbientCubeMap() {
	ambientCubeDecls.Append( this );
	FreeData();
}

sdDeclAmbientCubeMap::~sdDeclAmbientCubeMap() {
	ambientCubeDecls.Remove( this );
}

const char *sdDeclAmbientCubeMap::DefaultDefinition() const {
	return "{}";
}

void sdDeclAmbientCubeMap::FreeData() {
	ambientLights.Clear();
	indoors = false;
	envMap.Clear();
	ambientColor.Set( 0.5f, 0.5f, 0.5f );
	highLightColor.Set( 0.8f, 0.8f, 0.8f );
	minSpecAmbientColor.Set( 0.6f, 0.6f, 0.6f );
	minSpecShadowColor.Set( 0.5f, 0.5f, 0.5f );
	brightness = 1.0f;
	sunDirection.Zero();
	sunColor.Zero();
	avgAmbientColor.Zero();
	ambientCubeMap = NULL;
	lightCubeMap = NULL;
	specularCubeMap = NULL;
	environmentCubeMap = NULL;
	gradientMap = NULL;
}

void sdDeclAmbientCubeMap::CacheFromDict( const idDict &dict ) {
	const idKeyValue *keyValue = NULL;
	while ( ( keyValue = dict.MatchPrefix( "ambientCubeMap", keyValue ) ) != NULL ) {
		if ( keyValue->GetValue().Length() ) {
			declManager->FindType( DECL_AMBIENTCUBEMAP, keyValue->GetValue(), false );
		}
	}
}

bool sdDeclAmbientCubeMap::ParseAmbientLight( idLexer &src ) {
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
			light.dir.x = src.ParseFloat();
			light.dir.y = src.ParseFloat();
			light.dir.z = src.ParseFloat();
			light.dir.Normalize();
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
			src.Warning( "sdDeclAmbientCubeMap::ParseAmbientLight: unknown token '%s'", token.c_str() );
			return false;
		}
	}
	return false;
}

bool sdDeclAmbientCubeMap::Parse( const char *text, const int textLength ) {
	idLexer src;
	idToken token;
	src.LoadMemory( text, textLength, GetFileName(), GetLineNum() );
	src.SetFlags( DECL_LEXER_FLAGS );
	src.SkipUntilString( "{" );
	FreeData();

	while ( src.ReadToken( &token ) ) {
		if ( token == "}" ) {
			GenerateImages();
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
			envMap = token;
		} else if ( !token.Icmp( "ambientColor" ) ) {
			ambientColor.Set( src.ParseFloat(), src.ParseFloat(), src.ParseFloat() );
		} else if ( !token.Icmp( "highLightColor" ) ) {
			highLightColor.Set( src.ParseFloat(), src.ParseFloat(), src.ParseFloat() );
		} else if ( !token.Icmp( "minSpecAmbientColor" ) ) {
			minSpecAmbientColor.Set( src.ParseFloat(), src.ParseFloat(), src.ParseFloat() );
		} else if ( !token.Icmp( "minSpecShadowColor" ) ) {
			minSpecShadowColor.Set( src.ParseFloat(), src.ParseFloat(), src.ParseFloat() );
		} else if ( !token.Icmp( "brightness" ) ) {
			brightness = src.ParseFloat();
		} else if ( !token.Icmp( "sunDirection" ) || !token.Icmp( "sunDir" ) ) {
			sunDirection.Set( src.ParseFloat(), src.ParseFloat(), src.ParseFloat() );
			sunDirection.Normalize();
		} else if ( !token.Icmp( "sunColor" ) ) {
			sunColor.Set( src.ParseFloat(), src.ParseFloat(), src.ParseFloat() );
		} else {
			src.Warning( "sdDeclAmbientCubeMap::Parse: unknown token '%s'", token.c_str() );
			return false;
		}
	}
	return false;
}

void sdDeclAmbientCubeMap::SetSunParameters( const idVec3 &direction, const idVec3 &color ) {
	if ( indoors ) {
		return;
	}
	sunDirection = direction;
	sunColor = color;
	GenerateImages();
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

void sdDeclAmbientCubeMap::GenerateCubeImage( idImage *image, generatedCube_t type ) const {
	const int facePixels = BAKEDLIGHT_SIZE * BAKEDLIGHT_SIZE;
	byte *pixels = new byte[6 * facePixels * 4];
	const byte *faces[6];
	int maximumChannel = 0;
	double channelTotal = 0.0;

	for ( int face = 0; face < 6; face++ ) {
		faces[face] = pixels + face * facePixels * 4;
		for ( int y = 0; y < BAKEDLIGHT_SIZE; y++ ) {
			for ( int x = 0; x < BAKEDLIGHT_SIZE; x++ ) {
				idVec3 direction;
				R_GetAmbientCubeVector( face, BAKEDLIGHT_SIZE, x, y, direction );
				idVec3 sample = type == GC_AMBIENT || type == GC_LIGHT ? ambientColor : vec3_zero;
				for ( int i = 0; i < ambientLights.Num(); i++ ) {
					const ambientLight_t &light = ambientLights[i];
					if ( type == GC_SPECULAR ? !light.specular : !light.ambient ) {
						continue;
					}
					float contribution = Max( light.dir * direction, 0.0f );
					if ( type == GC_SPECULAR ) {
						contribution = idMath::Pow( contribution, 16.0f );
					}
					sample += light.color * contribution;
				}
				if ( type == GC_LIGHT && !indoors ) {
					sample += sunColor * Max( sunDirection * direction, 0.0f );
					sample *= 0.25f;
				}
				byte *output = pixels + ( face * facePixels + y * BAKEDLIGHT_SIZE + x ) * 4;
				for ( int channel = 0; channel < 3; channel++ ) {
					output[channel] = static_cast<byte>( idMath::ClampInt( 0, 255,
						idMath::Ftoi( sample[channel] * 255.0f ) ) );
					maximumChannel = Max( maximumChannel, static_cast<int>( output[channel] ) );
					channelTotal += output[channel];
				}
				output[3] = 255;
			}
		}
	}

	image->GenerateCubeImage( faces, BAKEDLIGHT_SIZE, TF_LINEAR, false, TD_HIGH_QUALITY );
	common->DPrintf( "Generated ambient declaration image %s: average %.1f/255, maximum %i/255\n",
		GetName(), channelTotal / ( 6.0 * facePixels * 3.0 ), maximumChannel );
	delete[] pixels;
}

void sdDeclAmbientCubeMap::GenerateAmbientImage( idImage *image ) const {
	GenerateCubeImage( image, GC_AMBIENT );
}

void sdDeclAmbientCubeMap::GenerateLightImage( idImage *image ) const {
	GenerateCubeImage( image, GC_LIGHT );
}

void sdDeclAmbientCubeMap::GenerateSpecularImage( idImage *image ) const {
	GenerateCubeImage( image, GC_SPECULAR );
}

void sdDeclAmbientCubeMap::GenerateGradientImage( idImage *image ) const {
	byte pixels[GRADIENT_SIZE * 4];
	for ( int i = 0; i < GRADIENT_SIZE; i++ ) {
		const float fraction = static_cast<float>( i ) / ( GRADIENT_SIZE - 1 );
		const idVec3 color = ambientColor * ( 1.0f - fraction ) + highLightColor * fraction;
		for ( int channel = 0; channel < 3; channel++ ) {
			pixels[i * 4 + channel] = static_cast<byte>( idMath::ClampInt( 0, 255,
				idMath::Ftoi( color[channel] * 255.0f ) ) );
		}
		pixels[i * 4 + 3] = 255;
	}
	image->GenerateImage( pixels, GRADIENT_SIZE, 1, TF_LINEAR, false, TR_CLAMP, TD_HIGH_QUALITY );
}

void sdDeclAmbientCubeMap::GenerateImages() {
	idVec3 total = ambientColor;
	for ( int i = 0; i < ambientLights.Num(); i++ ) {
		if ( ambientLights[i].ambient ) {
			total += ambientLights[i].color;
		}
	}
	avgAmbientColor.Set( total.x, total.y, total.z, 1.0f );
	if ( !globalImages || !base ) {
		return;
	}

	generatingAmbientCubeMap = this;
	ambientCubeMap = globalImages->ImageFromFunction( va( "_ambientCubeMap_%s", GetName() ), R_GenerateAmbientCubeImage );
	generatingAmbientCubeMap = NULL;
	generatingLightCubeMap = this;
	lightCubeMap = globalImages->ImageFromFunction( va( "_lightCubeMap_%s", GetName() ), R_GenerateLightCubeImage );
	generatingLightCubeMap = NULL;
	generatingSpecularCubeMap = this;
	specularCubeMap = globalImages->ImageFromFunction( va( "_specularCubeMap_%s", GetName() ), R_GenerateSpecularCubeImage );
	generatingSpecularCubeMap = NULL;
	generatingGradientMap = this;
	gradientMap = globalImages->ImageFromFunction( va( "_gradientMap_%s", GetName() ), R_GenerateGradientImage );
	generatingGradientMap = NULL;

	if ( !envMap.IsEmpty() ) {
		environmentCubeMap = globalImages->ImageFromFile( envMap, TF_DEFAULT, true, TR_CLAMP,
			TD_HIGH_QUALITY, CF_NATIVE );
	}
}

bool sdDeclAmbientCubeMap::RebuildTextSource() {
	idFile_Memory file( va( "ambientCubeMap %s", GetName() ) );
	file.WriteFloatString( "ambientCubeMap %s {\n", GetName() );
	if ( indoors ) {
		file.WriteFloatString( "\tindoors\n" );
	}
	if ( !envMap.IsEmpty() ) {
		file.WriteFloatString( "\tenvMap \"%s\"\n", envMap.c_str() );
	}
	file.WriteFloatString( "\tambientColor %f %f %f\n", ambientColor.x, ambientColor.y, ambientColor.z );
	file.WriteFloatString( "\thighLightColor %f %f %f\n", highLightColor.x, highLightColor.y, highLightColor.z );
	file.WriteFloatString( "\tminSpecAmbientColor %f %f %f\n", minSpecAmbientColor.x, minSpecAmbientColor.y, minSpecAmbientColor.z );
	file.WriteFloatString( "\tminSpecShadowColor %f %f %f\n", minSpecShadowColor.x, minSpecShadowColor.y, minSpecShadowColor.z );
	file.WriteFloatString( "\tbrightness %f\n", brightness );
	for ( int i = 0; i < ambientLights.Num(); i++ ) {
		const ambientLight_t &light = ambientLights[i];
		file.WriteFloatString( "\tambientLight {\n\t\tname \"%s\"\n", light.name.c_str() );
		file.WriteFloatString( "\t\tdirection %f %f %f\n", light.dir.x, light.dir.y, light.dir.z );
		file.WriteFloatString( "\t\tcolor %f %f %f\n", light.color.x, light.color.y, light.color.z );
		file.WriteFloatString( "\t\tambient %i\n\t\tspecular %i\n\t}\n", light.ambient, light.specular );
	}
	file.WriteFloatString( "}\n" );
	SetText( file.GetDataPtr() );
	return true;
}

bool sdDeclAmbientCubeMap::Save() {
	return RebuildTextSource() && ReplaceSourceFileText();
}

sdDeclAmbientCubeMap *R_FindAmbientCubeMap( const char *name, bool makeDefault ) {
	if ( !name || !name[0] ) {
		return NULL;
	}
	return const_cast<sdDeclAmbientCubeMap *>( static_cast<const sdDeclAmbientCubeMap *>(
		declManager->FindType( DECL_AMBIENTCUBEMAP, name, makeDefault ) ) );
}

void R_ShutdownAmbientCubeMaps() {
	generatingAmbientCubeMap = NULL;
	generatingLightCubeMap = NULL;
	generatingSpecularCubeMap = NULL;
	generatingGradientMap = NULL;
}
