/*
===========================================================================

Darklight offline lightmap and deluxemap baker.

The chart UV stream is written to mapProcFile004.  Atlas images and a small
manifest are stored in maps/path/mapname.zip, which is mounted transiently by
the renderer while that map is loaded.

===========================================================================
*/

#include "../../../idlib/precompiled.h"
#pragma hdrstop

#include "dmap.h"

static const int LM_ATLAS_SIZE = 1024;
static const int LM_PADDING = 4;
static const float LM_DEFAULT_SCALE = 4.0f;
static const float LM_DEFAULT_AMBIENT = 0.025f;
static const float LM_DEFAULT_DIRECT_SCALE = 2.0f;
static const int LM_DEFAULT_SHADOW_SAMPLES = 9;
static const float LM_DEFAULT_BOUNCE_SCALE = 0.65f;
static const int LM_DEFAULT_BOUNCE_SAMPLES = 32;
static const float LM_DEFAULT_BOUNCE_DISTANCE = 4096.0f;
static const int LM_DEFAULT_BOUNCE_SPACING = 2;
static const int LM_DEFAULT_DENOISE_PASSES = 3;
static const float LM_DEFAULT_AO_STRENGTH = 0.65f;
static const float LM_DEFAULT_AO_DISTANCE = 64.0f;

typedef struct {
	idVec3 a;
	idVec3 b;
	idVec3 c;
	idVec2 textureCoords[3];
	idBounds bounds;
	idVec3 center;
	idVec3 normal;
	idVec2 lightmapTexCoords[3];
	int lightmapAtlas;
	int alphaMask;
	bool castsShadow;
} lmOccluder_t;

typedef struct {
	idList<byte> rgba;
	int width;
	int height;
	float threshold;
	float alphaScale;
	float matrix[2][3];
	textureRepeat_t repeat;
} lmAlphaStage_t;

typedef struct {
	const idMaterial *material;
	idList<lmAlphaStage_t *> stages;
	bool opaqueFallback;
} lmAlphaMask_t;

typedef struct {
	const idImage *source;
	idList<byte> rgba;
	int width;
	int height;
	textureRepeat_t repeat;
	bool valid;
	bool constantWhite;
} lmCpuImage_t;

typedef struct {
	lmCpuImage_t *image;
	idVec3 color;
	float matrix[2][3];
} lmBakedLightStage_t;

typedef struct {
	const mapLight_t *light;
	lmCpuImage_t *falloff;
	idList<lmBakedLightStage_t *> stages;
	bool exact;
} lmBakedLight_t;

typedef struct {
	idBounds bounds;
	int first;
	int count;
	int children[2];
} lmTraceNode_t;

typedef struct {
	int atlas;
	const idMaterial *material;
	idList<idDrawVert> verts;
	idList<glIndex_t> indexes;
	idList<idVec2> lightmapTexCoords;
} lmSurface_t;

typedef struct {
	int cursorX;
	int cursorY;
	int rowHeight;
	idList<byte> lightmap;
	idList<byte> deluxemap;
	idList<byte> valid;
	idList<byte> directLightmap;
} lmAtlas_t;

typedef struct {
	idStr name;
	idList<byte> data;
	unsigned int crc;
	unsigned int localOffset;
} lmZipEntry_t;

static idStr lmMapFileBase;
static idList<lmSurface_t *> lmSurfaces;
static idList<lmAtlas_t *> lmAtlases;
static idList<lmOccluder_t> lmOccluders;
static idList<lmAlphaMask_t *> lmAlphaMasks;
static idList<lmCpuImage_t *> lmCpuImages;
static idList<lmBakedLight_t *> lmBakedLights;
static idList<int> lmOccluderOrder;
static idList<lmTraceNode_t> lmTraceNodes;
static float lmAmbient;
static float lmDirectScale;
static int lmShadowSamples;
static float lmBounceScale;
static int lmBounceSamples;
static float lmBounceDistance;
static int lmBounceSpacing;
static int lmDenoisePasses;
static float lmAOStrength;
static float lmAODistance;
static unsigned __int64 lmShadePoints;
static unsigned __int64 lmLightTests;
static unsigned __int64 lmLightsInVolume;
static unsigned __int64 lmLightsOccluded;
static unsigned __int64 lmLightsContributing;
static unsigned __int64 lmShadowRays;
static unsigned __int64 lmMixedShadowTests;
static unsigned __int64 lmBounceRays;
static unsigned __int64 lmBounceHits;
static unsigned __int64 lmBounceContributingHits;
static unsigned __int64 lmBounceAnchorTexels;
static unsigned __int64 lmBounceTexels;
static unsigned __int64 lmBounceNonzeroTexels;
static unsigned __int64 lmBounceByteSum;
static unsigned __int64 lmAOByteSum;
static unsigned __int64 lmAlphaTests;
static unsigned __int64 lmAlphaPassThroughs;
static int lmExternalOccluders;
static int lmDoorOccluders;
static int lmExactBakedLights;

static unsigned short LM_PackRGB565( const byte *rgb );
static void LM_UnpackRGB565( unsigned short color, byte *rgb );

static int LM_ClampByte( float value ) {
	return idMath::ClampInt( 0, 255, (int)( value * 255.0f + 0.5f ) );
}

static unsigned short LM_ClampUShort( float value ) {
	return (unsigned short)idMath::ClampInt( 0, 65535, (int)( value * 65535.0f + 0.5f ) );
}

static lmAtlas_t *LM_NewAtlas( void ) {
	lmAtlas_t *atlas = new lmAtlas_t;
	const int pixels = LM_ATLAS_SIZE * LM_ATLAS_SIZE;
	atlas->cursorX = 0;
	atlas->cursorY = 0;
	atlas->rowHeight = 0;
	atlas->lightmap.SetNum( pixels * 4 );
	atlas->deluxemap.SetNum( pixels * 4 );
	atlas->valid.SetNum( pixels );
	memset( atlas->lightmap.Ptr(), 0, atlas->lightmap.Num() );
	memset( atlas->deluxemap.Ptr(), 0, atlas->deluxemap.Num() );
	memset( atlas->valid.Ptr(), 0, atlas->valid.Num() );
	lmAtlases.Append( atlas );
	return atlas;
}

static int LM_AllocChart( int width, int height, int &x, int &y ) {
	lmAtlas_t *atlas;

	if ( lmAtlases.Num() == 0 ) {
		LM_NewAtlas();
	}
	atlas = lmAtlases[lmAtlases.Num() - 1];
	if ( atlas->cursorX + width > LM_ATLAS_SIZE ) {
		atlas->cursorX = 0;
		atlas->cursorY += atlas->rowHeight;
		atlas->rowHeight = 0;
	}
	if ( atlas->cursorY + height > LM_ATLAS_SIZE ) {
		atlas = LM_NewAtlas();
	}

	x = atlas->cursorX;
	y = atlas->cursorY;
	atlas->cursorX += width;
	atlas->rowHeight = Max( atlas->rowHeight, height );
	return lmAtlases.Num() - 1;
}

static void LM_ClearAlphaMasks( void ) {
	for ( int maskIndex = 0; maskIndex < lmAlphaMasks.Num(); maskIndex++ ) {
		lmAlphaMasks[maskIndex]->stages.DeleteContents( true );
		delete lmAlphaMasks[maskIndex];
	}
	lmAlphaMasks.Clear();
}

static int LM_GetAlphaMask( const idMaterial *material ) {
	if ( !material || material->Coverage() != MC_PERFORATED ) {
		return -1;
	}
	for ( int maskIndex = 0; maskIndex < lmAlphaMasks.Num(); maskIndex++ ) {
		if ( lmAlphaMasks[maskIndex]->material == material ) {
			return maskIndex;
		}
	}

	lmAlphaMask_t *mask = new lmAlphaMask_t;
	mask->material = material;
	mask->opaqueFallback = false;

	idList<float> evaluatedRegisters;
	const float *registers = material->ConstantRegisters();
	if ( !registers ) {
		evaluatedRegisters.SetNum( material->GetNumRegisters() );
		float shaderParms[MAX_ENTITY_SHADER_PARMS];
		memset( shaderParms, 0, sizeof( shaderParms ) );
		shaderParms[SHADERPARM_RED] = 1.0f;
		shaderParms[SHADERPARM_GREEN] = 1.0f;
		shaderParms[SHADERPARM_BLUE] = 1.0f;
		shaderParms[SHADERPARM_ALPHA] = 1.0f;
		viewDef_t view;
		memset( &view, 0, sizeof( view ) );
		material->EvaluateRegisters( evaluatedRegisters.Ptr(), shaderParms, &view, NULL );
		registers = evaluatedRegisters.Ptr();
	}

	bool hadActiveAlphaStage = false;
	for ( int stageIndex = 0; stageIndex < material->GetNumStages(); stageIndex++ ) {
		const shaderStage_t *stage = material->GetStage( stageIndex );
		if ( !stage->hasAlphaTest || registers[stage->conditionRegister] == 0.0f ) {
			continue;
		}
		hadActiveAlphaStage = true;
		const float alphaScale = idMath::ClampFloat( 0.0f, 1.0f,
			registers[stage->color.registers[3]] );
		if ( alphaScale <= 0.0f ) {
			continue;
		}
		if ( !stage->texture.image || stage->texture.texgen != TG_EXPLICIT ||
			stage->texture.image->generatorFunction || stage->texture.image->cubeFiles != CF_2D ) {
			mask->opaqueFallback = true;
			break;
		}

		byte *pic = NULL;
		int width = 0;
		int height = 0;
		R_LoadImageProgram( stage->texture.image->imgName, &pic, &width, &height, NULL );
		if ( !pic || width <= 0 || height <= 0 ) {
			if ( pic ) {
				R_StaticFree( pic );
			}
			mask->opaqueFallback = true;
			break;
		}

		lmAlphaStage_t *alphaStage = new lmAlphaStage_t;
		alphaStage->rgba.SetNum( width * height * 4 );
		memcpy( alphaStage->rgba.Ptr(), pic, alphaStage->rgba.Num() );
		R_StaticFree( pic );
		alphaStage->width = width;
		alphaStage->height = height;
		alphaStage->threshold = registers[stage->alphaTestRegister];
		alphaStage->alphaScale = alphaScale;
		alphaStage->repeat = stage->texture.image->repeat;
		alphaStage->matrix[0][0] = 1.0f;
		alphaStage->matrix[0][1] = 0.0f;
		alphaStage->matrix[0][2] = 0.0f;
		alphaStage->matrix[1][0] = 0.0f;
		alphaStage->matrix[1][1] = 1.0f;
		alphaStage->matrix[1][2] = 0.0f;
		if ( stage->texture.hasMatrix ) {
			for ( int row = 0; row < 2; row++ ) {
				for ( int column = 0; column < 3; column++ ) {
					alphaStage->matrix[row][column] = registers[stage->texture.matrix[row][column]];
				}
			}
		}
		mask->stages.Append( alphaStage );
	}
	if ( !hadActiveAlphaStage ) {
		// This matches the depth renderer: if all alpha-test stages are disabled,
		// a perforated material falls back to a solid depth surface.
		mask->opaqueFallback = true;
	}
	return lmAlphaMasks.Append( mask );
}

static int LM_WrapAlphaCoordinate( int coordinate, int size ) {
	coordinate %= size;
	return coordinate < 0 ? coordinate + size : coordinate;
}

static float LM_SampleAlphaTexel( const lmAlphaStage_t *stage, int x, int y ) {
	if ( stage->repeat == TR_REPEAT ) {
		x = LM_WrapAlphaCoordinate( x, stage->width );
		y = LM_WrapAlphaCoordinate( y, stage->height );
	} else if ( x < 0 || x >= stage->width || y < 0 || y >= stage->height ) {
		if ( stage->repeat == TR_CLAMP_TO_ZERO_ALPHA || stage->repeat == TR_CLAMP_TO_BORDER ) {
			return 0.0f;
		}
		x = idMath::ClampInt( 0, stage->width - 1, x );
		y = idMath::ClampInt( 0, stage->height - 1, y );
	}
	return stage->rgba[( y * stage->width + x ) * 4 + 3] * ( 1.0f / 255.0f );
}

static float LM_SampleAlpha( const lmAlphaStage_t *stage, const idVec2 &textureCoord ) {
	const float s = textureCoord[0] * stage->matrix[0][0] +
		textureCoord[1] * stage->matrix[0][1] + stage->matrix[0][2];
	const float t = textureCoord[0] * stage->matrix[1][0] +
		textureCoord[1] * stage->matrix[1][1] + stage->matrix[1][2];
	const float imageX = s * stage->width - 0.5f;
	const float imageY = t * stage->height - 0.5f;
	const int x0 = (int)floorf( imageX );
	const int y0 = (int)floorf( imageY );
	const float fractionX = imageX - x0;
	const float fractionY = imageY - y0;
	const float a00 = LM_SampleAlphaTexel( stage, x0, y0 );
	const float a10 = LM_SampleAlphaTexel( stage, x0 + 1, y0 );
	const float a01 = LM_SampleAlphaTexel( stage, x0, y0 + 1 );
	const float a11 = LM_SampleAlphaTexel( stage, x0 + 1, y0 + 1 );
	const float row0 = a00 + ( a10 - a00 ) * fractionX;
	const float row1 = a01 + ( a11 - a01 ) * fractionX;
	return row0 + ( row1 - row0 ) * fractionY;
}

static bool LM_OccluderBlocksRay( const lmOccluder_t &occluder, float u, float v ) {
	if ( occluder.alphaMask < 0 ) {
		return true;
	}
	lmAlphaTests++;
	const lmAlphaMask_t *mask = lmAlphaMasks[occluder.alphaMask];
	if ( mask->opaqueFallback ) {
		return true;
	}
	const float w = 1.0f - u - v;
	const idVec2 textureCoord = occluder.textureCoords[0] * w +
		occluder.textureCoords[1] * u + occluder.textureCoords[2] * v;
	for ( int stageIndex = 0; stageIndex < mask->stages.Num(); stageIndex++ ) {
		const lmAlphaStage_t *stage = mask->stages[stageIndex];
		if ( LM_SampleAlpha( stage, textureCoord ) * stage->alphaScale > stage->threshold ) {
			return true;
		}
	}
	lmAlphaPassThroughs++;
	return false;
}

static void LM_ClearBakedLights( void ) {
	for ( int lightIndex = 0; lightIndex < lmBakedLights.Num(); lightIndex++ ) {
		lmBakedLights[lightIndex]->stages.DeleteContents( true );
		delete lmBakedLights[lightIndex];
	}
	lmBakedLights.Clear();
	lmCpuImages.DeleteContents( true );
}

static lmCpuImage_t *LM_GetCpuImage( const idImage *source ) {
	if ( !source ) {
		return NULL;
	}
	for ( int imageIndex = 0; imageIndex < lmCpuImages.Num(); imageIndex++ ) {
		if ( lmCpuImages[imageIndex]->source == source ) {
			return lmCpuImages[imageIndex];
		}
	}

	lmCpuImage_t *image = new lmCpuImage_t;
	image->source = source;
	image->width = 0;
	image->height = 0;
	image->repeat = source->repeat;
	image->valid = false;
	image->constantWhite = false;
	if ( source == globalImages->whiteImage || source == globalImages->noFalloffImage ) {
		image->valid = true;
		image->constantWhite = true;
		lmCpuImages.Append( image );
		return image;
	}
	if ( source->generatorFunction || source->cubeFiles != CF_2D ) {
		lmCpuImages.Append( image );
		return image;
	}

	byte *pic = NULL;
	R_LoadImageProgram( source->imgName, &pic, &image->width, &image->height, NULL );
	if ( pic && image->width > 0 && image->height > 0 ) {
		image->rgba.SetNum( image->width * image->height * 4 );
		memcpy( image->rgba.Ptr(), pic, image->rgba.Num() );
		image->valid = true;
	}
	if ( pic ) {
		R_StaticFree( pic );
	}
	lmCpuImages.Append( image );
	return image;
}

static void LM_BuildBakedLights( void ) {
	lmExactBakedLights = 0;
	lmBakedLights.SetNum( dmapGlobals.mapLights.Num() );
	for ( int lightIndex = 0; lightIndex < dmapGlobals.mapLights.Num(); lightIndex++ ) {
		const mapLight_t *light = dmapGlobals.mapLights[lightIndex];
		lmBakedLight_t *baked = new lmBakedLight_t;
		baked->light = light;
		baked->falloff = NULL;
		baked->exact = light && light->bake && light->def.lightShader != NULL;
		lmBakedLights[lightIndex] = baked;
		if ( !baked->exact ) {
			continue;
		}

		baked->falloff = LM_GetCpuImage( light->def.falloffImage );
		if ( !baked->falloff || !baked->falloff->valid ) {
			baked->exact = false;
			continue;
		}

		const idMaterial *shader = light->def.lightShader;
		idList<float> evaluatedRegisters;
		const float *registers = shader->ConstantRegisters();
		if ( !registers ) {
			evaluatedRegisters.SetNum( shader->GetNumRegisters() );
			viewDef_t view;
			memset( &view, 0, sizeof( view ) );
			shader->EvaluateRegisters( evaluatedRegisters.Ptr(), light->def.parms.shaderParms, &view, NULL );
			registers = evaluatedRegisters.Ptr();
		}

		for ( int stageIndex = 0; stageIndex < shader->GetNumStages(); stageIndex++ ) {
			const shaderStage_t *stage = shader->GetStage( stageIndex );
			if ( registers[stage->conditionRegister] == 0.0f ) {
				continue;
			}
			lmCpuImage_t *stageImage = LM_GetCpuImage( stage->texture.image );
			if ( !stageImage || !stageImage->valid ) {
				baked->exact = false;
				break;
			}
			lmBakedLightStage_t *bakedStage = new lmBakedLightStage_t;
			bakedStage->image = stageImage;
			bakedStage->color.Set( registers[stage->color.registers[0]],
				registers[stage->color.registers[1]], registers[stage->color.registers[2]] );
			bakedStage->matrix[0][0] = 1.0f;
			bakedStage->matrix[0][1] = 0.0f;
			bakedStage->matrix[0][2] = 0.0f;
			bakedStage->matrix[1][0] = 0.0f;
			bakedStage->matrix[1][1] = 1.0f;
			bakedStage->matrix[1][2] = 0.0f;
			if ( stage->texture.hasMatrix ) {
				for ( int row = 0; row < 2; row++ ) {
					for ( int column = 0; column < 3; column++ ) {
						bakedStage->matrix[row][column] = registers[stage->texture.matrix[row][column]];
					}
				}
			}
			baked->stages.Append( bakedStage );
		}
		if ( baked->stages.Num() == 0 ) {
			baked->exact = false;
		}
		if ( baked->exact ) {
			lmExactBakedLights++;
		}
	}
}

static idVec3 LM_SampleCpuTexel( const lmCpuImage_t *image, int x, int y ) {
	if ( image->repeat == TR_REPEAT ) {
		x = LM_WrapAlphaCoordinate( x, image->width );
		y = LM_WrapAlphaCoordinate( y, image->height );
	} else if ( x < 0 || x >= image->width || y < 0 || y >= image->height ) {
		if ( image->repeat != TR_CLAMP ) {
			return idVec3( 0.0f, 0.0f, 0.0f );
		}
		x = idMath::ClampInt( 0, image->width - 1, x );
		y = idMath::ClampInt( 0, image->height - 1, y );
	}
	const byte *pixel = &image->rgba[( y * image->width + x ) * 4];
	return idVec3( pixel[0], pixel[1], pixel[2] ) * ( 1.0f / 255.0f );
}

static idVec3 LM_SampleCpuImage( const lmCpuImage_t *image, float s, float t ) {
	if ( !image || !image->valid ) {
		return idVec3( 0.0f, 0.0f, 0.0f );
	}
	if ( image->constantWhite ) {
		if ( image->repeat != TR_REPEAT && ( s < 0.0f || s > 1.0f || t < 0.0f || t > 1.0f ) ) {
			return idVec3( 0.0f, 0.0f, 0.0f );
		}
		return idVec3( 1.0f, 1.0f, 1.0f );
	}
	const float imageX = s * image->width - 0.5f;
	const float imageY = t * image->height - 0.5f;
	const int x0 = (int)floorf( imageX );
	const int y0 = (int)floorf( imageY );
	const float fractionX = imageX - x0;
	const float fractionY = imageY - y0;
	const idVec3 c00 = LM_SampleCpuTexel( image, x0, y0 );
	const idVec3 c10 = LM_SampleCpuTexel( image, x0 + 1, y0 );
	const idVec3 c01 = LM_SampleCpuTexel( image, x0, y0 + 1 );
	const idVec3 c11 = LM_SampleCpuTexel( image, x0 + 1, y0 + 1 );
	const idVec3 row0 = c00 + ( c10 - c00 ) * fractionX;
	const idVec3 row1 = c01 + ( c11 - c01 ) * fractionX;
	return row0 + ( row1 - row0 ) * fractionY;
}

static bool LM_SampleBakedLight( int lightIndex, const idVec3 &point, idVec3 &color ) {
	color.Zero();
	if ( lightIndex < 0 || lightIndex >= lmBakedLights.Num() || !lmBakedLights[lightIndex]->exact ) {
		return false;
	}
	const lmBakedLight_t *baked = lmBakedLights[lightIndex];
	const mapLight_t *light = baked->light;
	const float projectionQ = light->def.lightProject[2].Distance( point );
	if ( idMath::Fabs( projectionQ ) < 0.000001f ) {
		return true;
	}
	const float projectionS = light->def.lightProject[0].Distance( point ) / projectionQ;
	const float projectionT = light->def.lightProject[1].Distance( point ) / projectionQ;
	const float falloffS = light->def.lightProject[3].Distance( point );
	const idVec3 falloff = LM_SampleCpuImage( baked->falloff, falloffS, 0.5f );
	for ( int stageIndex = 0; stageIndex < baked->stages.Num(); stageIndex++ ) {
		const lmBakedLightStage_t *stage = baked->stages[stageIndex];
		const float s = projectionS * stage->matrix[0][0] + projectionT * stage->matrix[0][1] + stage->matrix[0][2];
		const float t = projectionS * stage->matrix[1][0] + projectionT * stage->matrix[1][1] + stage->matrix[1][2];
		const idVec3 projection = LM_SampleCpuImage( stage->image, s, t );
		color[0] += stage->color[0] * projection[0] * falloff[0];
		color[1] += stage->color[1] * projection[1] * falloff[1];
		color[2] += stage->color[2] * projection[2] * falloff[2];
	}
	return true;
}

static void LM_AddTraceTriangles( const srfTriangles_t *tri, const idVec3 &origin, const idMat3 &axis,
		const idMaterial *material, bool castsShadow, int lightmapAtlas,
		const idList<idVec2> *lightmapTexCoords, bool doorShadow ) {
	const int alphaMask = LM_GetAlphaMask( material );
	for ( int i = 0; i + 2 < tri->numIndexes; i += 3 ) {
		lmOccluder_t occluder;
		const int indexes[3] = { tri->indexes[i + 0], tri->indexes[i + 1], tri->indexes[i + 2] };
		occluder.a = origin + tri->verts[indexes[0]].xyz * axis;
		occluder.b = origin + tri->verts[indexes[1]].xyz * axis;
		occluder.c = origin + tri->verts[indexes[2]].xyz * axis;
		occluder.bounds.Clear();
		occluder.bounds.AddPoint( occluder.a );
		occluder.bounds.AddPoint( occluder.b );
		occluder.bounds.AddPoint( occluder.c );
		occluder.bounds.ExpandSelf( 0.05f );
		occluder.center = ( occluder.a + occluder.b + occluder.c ) * ( 1.0f / 3.0f );
		// Proc triangles use clockwise front-face winding.  Start with the
		// corresponding outward geometric normal, then align it to the transformed
		// vertex normals for meshes with mixed or repaired winding.
		occluder.normal = ( occluder.c - occluder.a ).Cross( occluder.b - occluder.a );
		occluder.normal.Normalize();
		idVec3 shadingNormal = ( tri->verts[indexes[0]].normal + tri->verts[indexes[1]].normal +
			tri->verts[indexes[2]].normal ) * axis;
		if ( shadingNormal.Normalize() != 0.0f && occluder.normal * shadingNormal < 0.0f ) {
			occluder.normal = -occluder.normal;
		}
		occluder.castsShadow = castsShadow;
		occluder.lightmapAtlas = lightmapAtlas;
		occluder.alphaMask = alphaMask;
		for ( int vertex = 0; vertex < 3; vertex++ ) {
			occluder.textureCoords[vertex] = tri->verts[indexes[vertex]].st;
			occluder.lightmapTexCoords[vertex] = lightmapTexCoords ? ( *lightmapTexCoords )[indexes[vertex]] : idVec2( 0.0f, 0.0f );
		}
		lmOccluders.Append( occluder );
		if ( doorShadow ) {
			lmDoorOccluders++;
		}
	}
}

static bool LM_IsBakedStaticEntity( const idMapEntity *mapEntity, bool worldEntity ) {
	if ( worldEntity ) {
		return true;
	}
	return Dmap_IsStaticEntityImmutable( mapEntity );
}

static bool LM_IsDoorEntity( const idMapEntity *mapEntity ) {
	if ( !mapEntity ) {
		return false;
	}
	const char *classname = mapEntity->epairs.GetString( "classname", "" );
	return !idStr::Icmpn( classname, "func_door", 9 );
}

static void LM_GetEntityTransform( int entityNum, idVec3 &origin, idMat3 &axis ) {
	origin.Zero();
	axis.Identity();
	if ( entityNum <= 0 || entityNum >= dmapGlobals.num_entities ) {
		return;
	}
	const uEntity_t &entity = dmapGlobals.uEntities[entityNum];
	origin = entity.origin;
	const idDict &args = entity.mapEntity->epairs;
	if ( !args.GetMatrix( "rotation", "1 0 0 0 1 0 0 0 1", axis ) ) {
		const float angle = args.GetFloat( "angle", "0" );
		if ( angle != 0.0f ) {
			axis = idAngles( 0.0f, angle, 0.0f ).ToMat3();
		}
	}
}

static void LM_AddExternalModelOccluders( void ) {
	if ( !dmapGlobals.dmapFile ) {
		return;
	}
	for ( int entityNum = 1; entityNum < dmapGlobals.dmapFile->GetNumEntities(); entityNum++ ) {
		const idMapEntity *mapEntity = dmapGlobals.dmapFile->GetEntity( entityNum );
		const bool doorShadow = LM_IsDoorEntity( mapEntity );
		if ( mapEntity->GetNumPrimitives() != 0 ||
			( !LM_IsBakedStaticEntity( mapEntity, false ) && !doorShadow ) ||
			Dmap_ShouldInlineStatic( mapEntity ) || mapEntity->epairs.GetBool( "noshadows", "0" ) ) {
			continue;
		}

		renderEntity_t renderEntity;
		gameEdit->ParseSpawnArgsToRenderEntity( &mapEntity->epairs, &renderEntity );
		if ( !renderEntity.hModel || renderEntity.hModel->IsDefaultModel() ||
			renderEntity.hModel->IsDynamicModel() != DM_STATIC ) {
			continue;
		}
		for ( int surfaceNum = 0; surfaceNum < renderEntity.hModel->NumSurfaces(); surfaceNum++ ) {
			const modelSurface_t *surface = renderEntity.hModel->Surface( surfaceNum );
			if ( !surface || !surface->geometry ) {
				continue;
			}
			const idMaterial *material = R_RemapShaderBySkin( surface->shader,
				renderEntity.customSkin, renderEntity.customShader );
			if ( !material || material->Coverage() == MC_TRANSLUCENT ||
				( material->Coverage() != MC_PERFORATED && !material->SurfaceCastsShadow() ) ) {
				continue;
			}
			LM_AddTraceTriangles( surface->geometry, renderEntity.origin, renderEntity.axis,
				material, true, -1, NULL, doorShadow );
			lmExternalOccluders += surface->geometry->numIndexes / 3;
		}
	}
}

static int LM_BuildTraceNode( int first, int count ) {
	lmTraceNode_t node;
	idBounds centerBounds;
	int nodeIndex;

	node.bounds.Clear();
	centerBounds.Clear();
	for ( int i = first; i < first + count; i++ ) {
		const lmOccluder_t &occluder = lmOccluders[lmOccluderOrder[i]];
		node.bounds.AddBounds( occluder.bounds );
		centerBounds.AddPoint( occluder.center );
	}
	node.first = first;
	node.count = count;
	node.children[0] = node.children[1] = -1;
	nodeIndex = lmTraceNodes.Append( node );

	if ( count <= 8 ) {
		return nodeIndex;
	}

	idVec3 size = centerBounds[1] - centerBounds[0];
	int axis = 0;
	if ( size[1] > size[axis] ) {
		axis = 1;
	}
	if ( size[2] > size[axis] ) {
		axis = 2;
	}
	const float split = ( centerBounds[0][axis] + centerBounds[1][axis] ) * 0.5f;
	int left = first;
	int right = first + count - 1;
	while ( left <= right ) {
		if ( lmOccluders[lmOccluderOrder[left]].center[axis] < split ) {
			left++;
		} else {
			idSwap( lmOccluderOrder[left], lmOccluderOrder[right] );
			right--;
		}
	}
	int leftCount = left - first;
	if ( leftCount == 0 || leftCount == count ) {
		leftCount = count / 2;
	}

	lmTraceNodes[nodeIndex].count = 0;
	lmTraceNodes[nodeIndex].children[0] = LM_BuildTraceNode( first, leftCount );
	lmTraceNodes[nodeIndex].children[1] = LM_BuildTraceNode( first + leftCount, count - leftCount );
	return nodeIndex;
}

static bool LM_IntersectTriangle( const lmOccluder_t &tri, const idVec3 &start, const idVec3 &dir,
		float &fraction, float &u, float &v ) {
	const idVec3 edge1 = tri.b - tri.a;
	const idVec3 edge2 = tri.c - tri.a;
	const idVec3 p = dir.Cross( edge2 );
	const float det = edge1 * p;
	// Deliberately use the absolute determinant: all baked visibility rays
	// intersect both front and back faces, independent of material cull mode.
	if ( idMath::Fabs( det ) < 0.000001f ) {
		return false;
	}
	const float invDet = 1.0f / det;
	const idVec3 tvec = start - tri.a;
	u = ( tvec * p ) * invDet;
	if ( u < 0.0f || u > 1.0f ) {
		return false;
	}
	const idVec3 q = tvec.Cross( edge1 );
	v = ( dir * q ) * invDet;
	if ( v < 0.0f || u + v > 1.0f ) {
		return false;
	}
	fraction = ( edge2 * q ) * invDet;
	return fraction > 0.0001f && fraction < 0.9999f;
}

static bool LM_TraceNode( int nodeIndex, const idVec3 &start, const idVec3 &end, const idVec3 &dir ) {
	const lmTraceNode_t &node = lmTraceNodes[nodeIndex];
	if ( !node.bounds.LineIntersection( start, end ) ) {
		return false;
	}
	if ( node.count > 0 ) {
		for ( int i = node.first; i < node.first + node.count; i++ ) {
			const lmOccluder_t &occluder = lmOccluders[lmOccluderOrder[i]];
			float fraction;
			float u;
			float v;
			if ( occluder.castsShadow && LM_IntersectTriangle( occluder, start, dir, fraction, u, v ) &&
				LM_OccluderBlocksRay( occluder, u, v ) ) {
				return true;
			}
		}
		return false;
	}
	return LM_TraceNode( node.children[0], start, end, dir ) || LM_TraceNode( node.children[1], start, end, dir );
}

static bool LM_Occluded( const idVec3 &start, const idVec3 &end ) {
	if ( lmTraceNodes.Num() == 0 ) {
		return false;
	}
	return LM_TraceNode( 0, start, end, end - start );
}

static void LM_TraceNearestNode( int nodeIndex, const idVec3 &start, const idVec3 &dir,
		float &nearestFraction, int &nearestTriangle, float &nearestU, float &nearestV ) {
	const lmTraceNode_t &node = lmTraceNodes[nodeIndex];
	const idVec3 nearestEnd = start + dir * nearestFraction;
	if ( !node.bounds.LineIntersection( start, nearestEnd ) ) {
		return;
	}
	if ( node.count > 0 ) {
		for ( int i = node.first; i < node.first + node.count; i++ ) {
			const int triangleIndex = lmOccluderOrder[i];
			float fraction;
			float u;
			float v;
			if ( LM_IntersectTriangle( lmOccluders[triangleIndex], start, dir, fraction, u, v ) &&
				fraction < nearestFraction && LM_OccluderBlocksRay( lmOccluders[triangleIndex], u, v ) ) {
				nearestFraction = fraction;
				nearestTriangle = triangleIndex;
				nearestU = u;
				nearestV = v;
			}
		}
		return;
	}
	LM_TraceNearestNode( node.children[0], start, dir, nearestFraction, nearestTriangle, nearestU, nearestV );
	LM_TraceNearestNode( node.children[1], start, dir, nearestFraction, nearestTriangle, nearestU, nearestV );
}

static bool LM_TraceNearest( const idVec3 &start, const idVec3 &end, int &triangleIndex, float &fraction,
		float &u, float &v ) {
	if ( lmTraceNodes.Num() == 0 ) {
		return false;
	}
	fraction = 1.0f;
	triangleIndex = -1;
	LM_TraceNearestNode( 0, start, end - start, fraction, triangleIndex, u, v );
	return triangleIndex >= 0;
}

static bool LM_PointInLight( const mapLight_t *light, const idVec3 &point, float &attenuation ) {
	for ( int i = 0; i < 6; i++ ) {
		if ( light->def.frustum[i].Distance( point ) > 0.0f ) {
			return false;
		}
	}
	attenuation = 1.0f;
	return true;
}

static float LM_FallbackLightAttenuation( const mapLight_t *light, const idVec3 &point ) {
	if ( !light->def.parms.pointLight ) {
		return 1.0f;
	}
	idVec3 local = ( point - light->def.parms.origin ) * light->def.parms.axis.Transpose();
	float distanceSquared = 0.0f;
	for ( int axis = 0; axis < 3; axis++ ) {
		if ( light->def.parms.lightRadius[axis] <= 0.0f ) {
			return 0.0f;
		}
		const float distance = local[axis] / light->def.parms.lightRadius[axis];
		distanceSquared += distance * distance;
	}
	return Max( 0.0f, 1.0f - distanceSquared );
}

static float LM_ShadowVisibility( const idVec3 &point, const idVec3 &footprintS,
		const idVec3 &footprintT, const idVec3 &lightDirection,
		const idVec3 &lightOrigin, bool parallel ) {
	// The first five samples cover the texel with a rotated quincunx.  Mixed
	// results get four corner samples, concentrating the extra rays on shadow
	// silhouettes while still catching shadows thinner than one luxel.
	static const idVec2 sampleOffsets[9] = {
		idVec2( 0.0f, 0.0f ),
		idVec2( -0.4f, -0.1f ), idVec2( -0.1f, 0.4f ),
		idVec2( 0.4f, 0.1f ), idVec2( 0.1f, -0.4f ),
		idVec2( -0.35f, -0.35f ), idVec2( -0.35f, 0.35f ),
		idVec2( 0.35f, -0.35f ), idVec2( 0.35f, 0.35f )
	};

	const int primarySamples = lmShadowSamples == 1 ? 1 : 5;
	int occludedSamples = 0;
	for ( int sample = 0; sample < primarySamples; sample++ ) {
		const idVec3 samplePoint = point + footprintS * sampleOffsets[sample][0] + footprintT * sampleOffsets[sample][1];
		const idVec3 traceEnd = parallel ? samplePoint + lightDirection * 65536.0f : lightOrigin;
		lmShadowRays++;
		// Bias toward the light instead of along the receiver normal.  Combined
		// with the two-sided triangle intersection this makes visibility
		// independent of front/back winding and avoids starting inside a wall.
		if ( LM_Occluded( samplePoint + lightDirection * 0.5f, traceEnd ) ) {
			occludedSamples++;
		}
	}

	int totalSamples = primarySamples;
	if ( lmShadowSamples == 9 && occludedSamples > 0 && occludedSamples < primarySamples ) {
		lmMixedShadowTests++;
		for ( int sample = primarySamples; sample < 9; sample++ ) {
			const idVec3 samplePoint = point + footprintS * sampleOffsets[sample][0] + footprintT * sampleOffsets[sample][1];
			const idVec3 traceEnd = parallel ? samplePoint + lightDirection * 65536.0f : lightOrigin;
			lmShadowRays++;
			if ( LM_Occluded( samplePoint + lightDirection * 0.5f, traceEnd ) ) {
				occludedSamples++;
			}
		}
		totalSamples = 9;
	}

	return 1.0f - (float)occludedSamples / totalSamples;
}

static void LM_ShadePoint( const idVec3 &point, const idVec3 &normal, const idVec3 &tangent,
		const idVec3 &bitangent, const idVec3 &footprintS, const idVec3 &footprintT,
		idVec3 &irradiance, idVec3 &deluxeDirection ) {
	irradiance.Set( lmAmbient, lmAmbient, lmAmbient );
	idVec3 directionSum( 0.0f, 0.0f, 0.0f );
	float directionWeight = 0.0f;
	lmShadePoints++;

	for ( int i = 0; i < dmapGlobals.mapLights.Num(); i++ ) {
		const mapLight_t *light = dmapGlobals.mapLights[i];
		if ( !light->bake ) {
			continue;
		}
		lmLightTests++;
		float attenuation;
		if ( !LM_PointInLight( light, point, attenuation ) ) {
			continue;
		}
		lmLightsInVolume++;
		idVec3 color;
		if ( !LM_SampleBakedLight( i, point, color ) ) {
			attenuation = LM_FallbackLightAttenuation( light, point );
			color.Set( light->def.parms.shaderParms[SHADERPARM_RED] * attenuation,
				light->def.parms.shaderParms[SHADERPARM_GREEN] * attenuation,
				light->def.parms.shaderParms[SHADERPARM_BLUE] * attenuation );
		}
		if ( color[0] <= 0.0001f && color[1] <= 0.0001f && color[2] <= 0.0001f ) {
			continue;
		}

		idVec3 lightDirection;
		if ( light->def.parms.parallel ) {
			lightDirection = -light->def.parms.axis[2];
			lightDirection.Normalize();
		} else {
			lightDirection = light->def.globalLightOrigin - point;
			if ( lightDirection.Normalize() == 0.0f ) {
				continue;
			}
		}

		const bool ambientLight = light->def.lightShader && light->def.lightShader->IsAmbientLight();
		const float signedLambert = normal * lightDirection;
		float lambert = ambientLight ? 1.0f : idMath::Fabs( signedLambert );
		if ( lambert <= 0.0f ) {
			continue;
		}
		idVec3 deluxeLightDirection = lightDirection;
		if ( signedLambert < 0.0f ) {
			// Treat receivers as two-sided, mirroring a back-side light into the
			// front tangent hemisphere used by the deluxemap shader.
			deluxeLightDirection -= normal * ( 2.0f * signedLambert );
			deluxeLightDirection.Normalize();
		}
		float visibility = 1.0f;
		if ( !light->def.parms.noShadows ) {
			visibility = LM_ShadowVisibility( point, footprintS, footprintT, lightDirection,
				light->def.globalLightOrigin, light->def.parms.parallel );
			if ( visibility < 1.0f ) {
				lmLightsOccluded++;
			}
			if ( visibility <= 0.0f ) {
				continue;
			}
		}

		const float weight = lambert * lmDirectScale * visibility;
		irradiance += color * weight;
		lmLightsContributing++;
		const float luminance = color[0] * 0.2126f + color[1] * 0.7152f + color[2] * 0.0722f;
		if ( !ambientLight ) {
			directionSum += deluxeLightDirection * ( weight * luminance );
			directionWeight += weight * luminance;
		}
	}

	if ( directionWeight > 0.0001f ) {
		idVec3 worldDirection = directionSum / directionWeight;
		worldDirection.Normalize();
		deluxeDirection.Set( tangent * worldDirection, bitangent * worldDirection, normal * worldDirection );
		deluxeDirection.Normalize();
	} else {
		deluxeDirection.Set( 0.0f, 0.0f, 1.0f );
	}
}

static void LM_RasterizeSurface( const lmSurface_t *surface ) {
	lmAtlas_t *atlas = lmAtlases[surface->atlas];

	for ( int index = 0; index + 2 < surface->indexes.Num(); index += 3 ) {
		const int ia = surface->indexes[index + 0];
		const int ib = surface->indexes[index + 1];
		const int ic = surface->indexes[index + 2];
		const idDrawVert &a = surface->verts[ia];
		const idDrawVert &b = surface->verts[ib];
		const idDrawVert &c = surface->verts[ic];
		const idVec2 ta = surface->lightmapTexCoords[ia] * LM_ATLAS_SIZE - idVec2( 0.5f, 0.5f );
		const idVec2 tb = surface->lightmapTexCoords[ib] * LM_ATLAS_SIZE - idVec2( 0.5f, 0.5f );
		const idVec2 tc = surface->lightmapTexCoords[ic] * LM_ATLAS_SIZE - idVec2( 0.5f, 0.5f );

		const float denominator = ( tb[1] - tc[1] ) * ( ta[0] - tc[0] ) + ( tc[0] - tb[0] ) * ( ta[1] - tc[1] );
		if ( idMath::Fabs( denominator ) < 0.000001f ) {
			continue;
		}
		const idVec3 footprintS = ( a.xyz - c.xyz ) * ( ( tb[1] - tc[1] ) / denominator ) +
			( b.xyz - c.xyz ) * ( ( tc[1] - ta[1] ) / denominator );
		const idVec3 footprintT = ( a.xyz - c.xyz ) * ( ( tc[0] - tb[0] ) / denominator ) +
			( b.xyz - c.xyz ) * ( ( ta[0] - tc[0] ) / denominator );

		int minX = idMath::ClampInt( 0, LM_ATLAS_SIZE - 1, (int)floorf( Min( ta[0], Min( tb[0], tc[0] ) ) ) );
		int maxX = idMath::ClampInt( 0, LM_ATLAS_SIZE - 1, (int)ceilf( Max( ta[0], Max( tb[0], tc[0] ) ) ) );
		int minY = idMath::ClampInt( 0, LM_ATLAS_SIZE - 1, (int)floorf( Min( ta[1], Min( tb[1], tc[1] ) ) ) );
		int maxY = idMath::ClampInt( 0, LM_ATLAS_SIZE - 1, (int)ceilf( Max( ta[1], Max( tb[1], tc[1] ) ) ) );

		idVec3 faceNormal = ( c.xyz - a.xyz ).Cross( b.xyz - a.xyz );
		faceNormal.Normalize();
		idVec3 tangent;
		idVec3 bitangent;
		const idVec3 edge1 = b.xyz - a.xyz;
		const idVec3 edge2 = c.xyz - a.xyz;
		const float du1 = b.st[0] - a.st[0];
		const float dv1 = b.st[1] - a.st[1];
		const float du2 = c.st[0] - a.st[0];
		const float dv2 = c.st[1] - a.st[1];
		const float textureDet = du1 * dv2 - du2 * dv1;
		if ( idMath::Fabs( textureDet ) > 0.000001f ) {
			const float invTextureDet = 1.0f / textureDet;
			tangent = ( edge1 * dv2 - edge2 * dv1 ) * invTextureDet;
			bitangent = ( edge2 * du1 - edge1 * du2 ) * invTextureDet;
			tangent.Normalize();
			bitangent.Normalize();
		} else {
			faceNormal.NormalVectors( tangent, bitangent );
		}

		for ( int y = minY; y <= maxY; y++ ) {
			for ( int x = minX; x <= maxX; x++ ) {
				const float w0 = ( ( tb[1] - tc[1] ) * ( x - tc[0] ) + ( tc[0] - tb[0] ) * ( y - tc[1] ) ) / denominator;
				const float w1 = ( ( tc[1] - ta[1] ) * ( x - tc[0] ) + ( ta[0] - tc[0] ) * ( y - tc[1] ) ) / denominator;
				const float w2 = 1.0f - w0 - w1;
				if ( w0 < -0.001f || w1 < -0.001f || w2 < -0.001f ) {
					continue;
				}

				idVec3 point = a.xyz * w0 + b.xyz * w1 + c.xyz * w2;
				idVec3 normal = a.normal * w0 + b.normal * w1 + c.normal * w2;
				if ( normal.Normalize() == 0.0f ) {
					normal = faceNormal;
				}
				if ( normal * faceNormal < 0.0f ) {
					normal = -normal;
				}
				idVec3 localTangent = tangent - normal * ( tangent * normal );
				if ( localTangent.Normalize() == 0.0f ) {
					normal.NormalVectors( localTangent, bitangent );
				}
				idVec3 localBitangent = bitangent - normal * ( bitangent * normal );
				localBitangent -= localTangent * ( localBitangent * localTangent );
				if ( localBitangent.Normalize() == 0.0f ) {
					localBitangent = normal.Cross( localTangent );
					localBitangent.Normalize();
				}

				idVec3 irradiance;
				idVec3 deluxeDirection;
				LM_ShadePoint( point, normal, localTangent, localBitangent, footprintS, footprintT,
					irradiance, deluxeDirection );

				const int pixel = y * LM_ATLAS_SIZE + x;
				atlas->lightmap[pixel * 4 + 0] = (byte)LM_ClampByte( irradiance[0] );
				atlas->lightmap[pixel * 4 + 1] = (byte)LM_ClampByte( irradiance[1] );
				atlas->lightmap[pixel * 4 + 2] = (byte)LM_ClampByte( irradiance[2] );
				atlas->lightmap[pixel * 4 + 3] = 255;
				atlas->deluxemap[pixel * 4 + 0] = (byte)LM_ClampByte( deluxeDirection[0] * 0.5f + 0.5f );
				atlas->deluxemap[pixel * 4 + 1] = (byte)LM_ClampByte( deluxeDirection[1] * 0.5f + 0.5f );
				atlas->deluxemap[pixel * 4 + 2] = (byte)LM_ClampByte( deluxeDirection[2] * 0.5f + 0.5f );
				atlas->deluxemap[pixel * 4 + 3] = 255;
				atlas->valid[pixel] = 1;
			}
		}
	}
}

static bool LM_SampleDirectLight( const lmOccluder_t &triangle, float u, float v, idVec3 &irradiance ) {
	if ( triangle.lightmapAtlas < 0 || triangle.lightmapAtlas >= lmAtlases.Num() ) {
		return false;
	}
	const lmAtlas_t *atlas = lmAtlases[triangle.lightmapAtlas];
	if ( atlas->directLightmap.Num() != LM_ATLAS_SIZE * LM_ATLAS_SIZE * 3 ) {
		return false;
	}
	const float w = 1.0f - u - v;
	const idVec2 texCoord = triangle.lightmapTexCoords[0] * w +
		triangle.lightmapTexCoords[1] * u + triangle.lightmapTexCoords[2] * v;
	const int centerX = idMath::ClampInt( 0, LM_ATLAS_SIZE - 1, (int)( texCoord[0] * LM_ATLAS_SIZE ) );
	const int centerY = idMath::ClampInt( 0, LM_ATLAS_SIZE - 1, (int)( texCoord[1] * LM_ATLAS_SIZE ) );
	int samplePixel = -1;
	for ( int radius = 0; radius <= 2 && samplePixel < 0; radius++ ) {
		for ( int y = centerY - radius; y <= centerY + radius && samplePixel < 0; y++ ) {
			for ( int x = centerX - radius; x <= centerX + radius; x++ ) {
				if ( x < 0 || x >= LM_ATLAS_SIZE || y < 0 || y >= LM_ATLAS_SIZE ) {
					continue;
				}
				const int pixel = y * LM_ATLAS_SIZE + x;
				if ( atlas->valid[pixel] ) {
					samplePixel = pixel;
					break;
				}
			}
		}
	}
	if ( samplePixel < 0 ) {
		return false;
	}

	irradiance[0] = Max( 0.0f, atlas->directLightmap[samplePixel * 3 + 0] * ( 1.0f / 255.0f ) - lmAmbient );
	irradiance[1] = Max( 0.0f, atlas->directLightmap[samplePixel * 3 + 1] * ( 1.0f / 255.0f ) - lmAmbient );
	irradiance[2] = Max( 0.0f, atlas->directLightmap[samplePixel * 3 + 2] * ( 1.0f / 255.0f ) - lmAmbient );
	return irradiance[0] > 0.0f || irradiance[1] > 0.0f || irradiance[2] > 0.0f;
}

static float LM_RadicalInverse( unsigned int bits ) {
	bits = ( bits << 16 ) | ( bits >> 16 );
	bits = ( ( bits & 0x55555555u ) << 1 ) | ( ( bits & 0xAAAAAAAAu ) >> 1 );
	bits = ( ( bits & 0x33333333u ) << 2 ) | ( ( bits & 0xCCCCCCCCu ) >> 2 );
	bits = ( ( bits & 0x0F0F0F0Fu ) << 4 ) | ( ( bits & 0xF0F0F0F0u ) >> 4 );
	bits = ( ( bits & 0x00FF00FFu ) << 8 ) | ( ( bits & 0xFF00FF00u ) >> 8 );
	return bits * 2.3283064365386963e-10f;
}

static void LM_ShadeBouncePoint( const idVec3 &point, const idVec3 &normal, const idVec3 &tangent,
		const idVec3 &bitangent, int atlasIndex, int pixelX, int pixelY, idVec3 &bounced,
		float &ambientVisibility ) {
	bounced.Zero();
	float aoOcclusion = 0.0f;
	unsigned int hash = (unsigned int)atlasIndex * 0x9E3779B9u;
	hash ^= (unsigned int)pixelX * 0x85EBCA6Bu;
	hash ^= (unsigned int)pixelY * 0xC2B2AE35u;
	const float rotation = ( hash & 0xFFFFu ) * ( 1.0f / 65536.0f );

	for ( int sample = 0; sample < lmBounceSamples; sample++ ) {
		const float radialSample = ( sample + 0.5f ) / lmBounceSamples;
		float angularSample = LM_RadicalInverse( (unsigned int)sample ) + rotation;
		angularSample -= floorf( angularSample );
		const float radius = sqrtf( radialSample );
		const float z = sqrtf( Max( 0.0f, 1.0f - radialSample ) );
		float sine;
		float cosine;
		idMath::SinCos( angularSample * idMath::TWO_PI, sine, cosine );
		idVec3 direction = tangent * ( cosine * radius ) + bitangent * ( sine * radius ) + normal * z;
		direction.Normalize();

		const idVec3 traceStart = point + normal * 0.5f;
		const float traceDistance = Max( lmBounceDistance, lmAODistance );
		const idVec3 traceEnd = traceStart + direction * traceDistance;
		int triangleIndex;
		float hitFraction;
		float u;
		float v;
		lmBounceRays++;
		if ( !LM_TraceNearest( traceStart, traceEnd, triangleIndex, hitFraction, u, v ) ) {
			continue;
		}
		lmBounceHits++;
		const float hitDistance = hitFraction * traceDistance;
		if ( hitDistance < lmAODistance ) {
			aoOcclusion += 1.0f - hitDistance / lmAODistance;
		}
		if ( lmBounceScale <= 0.0f || hitDistance > lmBounceDistance ) {
			continue;
		}
		const lmOccluder_t &triangle = lmOccluders[triangleIndex];
		if ( triangle.normal * -direction <= 0.001f ) {
			continue;
		}
		idVec3 incoming;
		if ( LM_SampleDirectLight( triangle, u, v, incoming ) ) {
			bounced += incoming;
			lmBounceContributingHits++;
		}
	}
	bounced *= lmBounceScale / lmBounceSamples;
	ambientVisibility = idMath::ClampFloat( 0.0f, 1.0f,
		1.0f - lmAOStrength * aoOcclusion / lmBounceSamples );
}

static void LM_MarkBounceSurfaceOwners( const lmSurface_t *surface, int surfaceIndex,
		idList<int> &bounceOwners, idList<idVec3> &positions, idList<idVec3> &normals ) {
	for ( int index = 0; index + 2 < surface->indexes.Num(); index += 3 ) {
		const idDrawVert &a = surface->verts[surface->indexes[index + 0]];
		const idDrawVert &b = surface->verts[surface->indexes[index + 1]];
		const idDrawVert &c = surface->verts[surface->indexes[index + 2]];
		const idVec2 ta = surface->lightmapTexCoords[surface->indexes[index + 0]] * LM_ATLAS_SIZE - idVec2( 0.5f, 0.5f );
		const idVec2 tb = surface->lightmapTexCoords[surface->indexes[index + 1]] * LM_ATLAS_SIZE - idVec2( 0.5f, 0.5f );
		const idVec2 tc = surface->lightmapTexCoords[surface->indexes[index + 2]] * LM_ATLAS_SIZE - idVec2( 0.5f, 0.5f );
		const float denominator = ( tb[1] - tc[1] ) * ( ta[0] - tc[0] ) + ( tc[0] - tb[0] ) * ( ta[1] - tc[1] );
		if ( idMath::Fabs( denominator ) < 0.000001f ) {
			continue;
		}

		const int minX = idMath::ClampInt( 0, LM_ATLAS_SIZE - 1, (int)floorf( Min( ta[0], Min( tb[0], tc[0] ) ) ) );
		const int maxX = idMath::ClampInt( 0, LM_ATLAS_SIZE - 1, (int)ceilf( Max( ta[0], Max( tb[0], tc[0] ) ) ) );
		const int minY = idMath::ClampInt( 0, LM_ATLAS_SIZE - 1, (int)floorf( Min( ta[1], Min( tb[1], tc[1] ) ) ) );
		const int maxY = idMath::ClampInt( 0, LM_ATLAS_SIZE - 1, (int)ceilf( Max( ta[1], Max( tb[1], tc[1] ) ) ) );
		idVec3 faceNormal = ( c.xyz - a.xyz ).Cross( b.xyz - a.xyz );
		faceNormal.Normalize();

		for ( int y = minY; y <= maxY; y++ ) {
			for ( int x = minX; x <= maxX; x++ ) {
				const float w0 = ( ( tb[1] - tc[1] ) * ( x - tc[0] ) + ( tc[0] - tb[0] ) * ( y - tc[1] ) ) / denominator;
				const float w1 = ( ( tc[1] - ta[1] ) * ( x - tc[0] ) + ( ta[0] - tc[0] ) * ( y - tc[1] ) ) / denominator;
				const float w2 = 1.0f - w0 - w1;
				if ( w0 < -0.001f || w1 < -0.001f || w2 < -0.001f ) {
					continue;
				}
				const int pixel = y * LM_ATLAS_SIZE + x;
				bounceOwners[pixel] = surfaceIndex;
				positions[pixel] = a.xyz * w0 + b.xyz * w1 + c.xyz * w2;
				normals[pixel] = a.normal * w0 + b.normal * w1 + c.normal * w2;
				if ( normals[pixel].Normalize() == 0.0f ) {
					normals[pixel] = faceNormal;
				}
				if ( normals[pixel] * faceNormal < 0.0f ) {
					normals[pixel] = -normals[pixel];
				}
			}
		}
	}
}

static bool LM_IsBounceAnchor( int x, int y, int surfaceIndex, const idList<int> &bounceOwners,
		const idList<idVec3> &positions, const idList<idVec3> &normals ) {
	if ( ( x % lmBounceSpacing ) == 0 && ( y % lmBounceSpacing ) == 0 ) {
		return true;
	}

	// Thin and disconnected chart fragments may not contain a regular grid
	// point.  Promote a luxel only when it has no usable grid point nearby, so
	// every receiver can still be reconstructed without leaking across charts.
	const int radius = lmBounceSpacing - 1;
	const int pixel = y * LM_ATLAS_SIZE + x;
	for ( int sampleY = Max( 0, y - radius ); sampleY <= Min( LM_ATLAS_SIZE - 1, y + radius ); sampleY++ ) {
		for ( int sampleX = Max( 0, x - radius ); sampleX <= Min( LM_ATLAS_SIZE - 1, x + radius ); sampleX++ ) {
			const int samplePixel = sampleY * LM_ATLAS_SIZE + sampleX;
			if ( ( sampleX % lmBounceSpacing ) == 0 && ( sampleY % lmBounceSpacing ) == 0 &&
				bounceOwners[samplePixel] == surfaceIndex ) {
				const idVec3 separation = positions[samplePixel] - positions[pixel];
				if ( normals[pixel] * normals[samplePixel] >= 0.8f &&
					idMath::Fabs( separation * normals[pixel] ) <= 2.0f &&
					idMath::Fabs( separation * normals[samplePixel] ) <= 2.0f ) {
					return false;
				}
			}
		}
	}
	return true;
}

static void LM_RasterizeBounceSurface( const lmSurface_t *surface, int surfaceIndex,
		idList<unsigned short> &bounce, idList<unsigned short> &ambientVisibility,
		idList<byte> &bounceAnchors, const idList<int> &bounceOwners,
		const idList<idVec3> &positions, const idList<idVec3> &normals ) {
	for ( int index = 0; index + 2 < surface->indexes.Num(); index += 3 ) {
		const idDrawVert &a = surface->verts[surface->indexes[index + 0]];
		const idDrawVert &b = surface->verts[surface->indexes[index + 1]];
		const idDrawVert &c = surface->verts[surface->indexes[index + 2]];
		const idVec2 ta = surface->lightmapTexCoords[surface->indexes[index + 0]] * LM_ATLAS_SIZE - idVec2( 0.5f, 0.5f );
		const idVec2 tb = surface->lightmapTexCoords[surface->indexes[index + 1]] * LM_ATLAS_SIZE - idVec2( 0.5f, 0.5f );
		const idVec2 tc = surface->lightmapTexCoords[surface->indexes[index + 2]] * LM_ATLAS_SIZE - idVec2( 0.5f, 0.5f );
		const float denominator = ( tb[1] - tc[1] ) * ( ta[0] - tc[0] ) + ( tc[0] - tb[0] ) * ( ta[1] - tc[1] );
		if ( idMath::Fabs( denominator ) < 0.000001f ) {
			continue;
		}

		const int minX = idMath::ClampInt( 0, LM_ATLAS_SIZE - 1, (int)floorf( Min( ta[0], Min( tb[0], tc[0] ) ) ) );
		const int maxX = idMath::ClampInt( 0, LM_ATLAS_SIZE - 1, (int)ceilf( Max( ta[0], Max( tb[0], tc[0] ) ) ) );
		const int minY = idMath::ClampInt( 0, LM_ATLAS_SIZE - 1, (int)floorf( Min( ta[1], Min( tb[1], tc[1] ) ) ) );
		const int maxY = idMath::ClampInt( 0, LM_ATLAS_SIZE - 1, (int)ceilf( Max( ta[1], Max( tb[1], tc[1] ) ) ) );
		idVec3 faceNormal = ( c.xyz - a.xyz ).Cross( b.xyz - a.xyz );
		faceNormal.Normalize();

		for ( int y = minY; y <= maxY; y++ ) {
			for ( int x = minX; x <= maxX; x++ ) {
				const float w0 = ( ( tb[1] - tc[1] ) * ( x - tc[0] ) + ( tc[0] - tb[0] ) * ( y - tc[1] ) ) / denominator;
				const float w1 = ( ( tc[1] - ta[1] ) * ( x - tc[0] ) + ( ta[0] - tc[0] ) * ( y - tc[1] ) ) / denominator;
				const float w2 = 1.0f - w0 - w1;
				if ( w0 < -0.001f || w1 < -0.001f || w2 < -0.001f ) {
					continue;
				}
				const int pixel = y * LM_ATLAS_SIZE + x;
				if ( bounceAnchors[pixel] || bounceOwners[pixel] != surfaceIndex ||
					!LM_IsBounceAnchor( x, y, surfaceIndex, bounceOwners, positions, normals ) ) {
					continue;
				}

				idVec3 point = a.xyz * w0 + b.xyz * w1 + c.xyz * w2;
				idVec3 normal = a.normal * w0 + b.normal * w1 + c.normal * w2;
				if ( normal.Normalize() == 0.0f ) {
					normal = faceNormal;
				}
				if ( normal * faceNormal < 0.0f ) {
					normal = -normal;
				}
				idVec3 tangent = a.tangents[0] * w0 + b.tangents[0] * w1 + c.tangents[0] * w2;
				tangent -= normal * ( tangent * normal );
				idVec3 bitangent;
				if ( tangent.Normalize() == 0.0f ) {
					normal.NormalVectors( tangent, bitangent );
				} else {
					bitangent = a.tangents[1] * w0 + b.tangents[1] * w1 + c.tangents[1] * w2;
					bitangent -= normal * ( bitangent * normal );
					bitangent -= tangent * ( bitangent * tangent );
					if ( bitangent.Normalize() == 0.0f ) {
						bitangent = normal.Cross( tangent );
						bitangent.Normalize();
					}
				}

				idVec3 indirect;
				float ao;
				LM_ShadeBouncePoint( point, normal, tangent, bitangent, surface->atlas, x, y, indirect, ao );
				bounce[pixel * 3 + 0] = LM_ClampUShort( indirect[0] );
				bounce[pixel * 3 + 1] = LM_ClampUShort( indirect[1] );
				bounce[pixel * 3 + 2] = LM_ClampUShort( indirect[2] );
				ambientVisibility[pixel] = LM_ClampUShort( ao );
				bounceAnchors[pixel] = 1;
				lmBounceAnchorTexels++;
			}
		}
	}
}

static void LM_DenoiseIndirect( const lmAtlas_t *atlas, const idList<int> &bounceOwners,
		const idList<idVec3> &positions, const idList<idVec3> &normals,
		idList<unsigned short> &bounce, idList<unsigned short> &ambientVisibility ) {
	if ( lmDenoisePasses <= 0 ) {
		return;
	}
	const int pixels = LM_ATLAS_SIZE * LM_ATLAS_SIZE;
	const int kernel[3] = { 1, 2, 1 };
	idList<unsigned short> nextBounce;
	idList<unsigned short> nextAO;
	nextBounce.SetNum( pixels * 3 );
	nextAO.SetNum( pixels );

	// Three edge-aware a-trous passes cover a fifteen-luxel footprint without
	// repeatedly blurring immediate neighbors.  Surface ownership, normals and
	// tangent-plane distance prevent the filter from crossing geometry edges.
	for ( int pass = 0; pass < lmDenoisePasses; pass++ ) {
		const int step = 1 << pass;
		for ( int y = 0; y < LM_ATLAS_SIZE; y++ ) {
			for ( int x = 0; x < LM_ATLAS_SIZE; x++ ) {
				const int pixel = y * LM_ATLAS_SIZE + x;
				if ( !atlas->valid[pixel] || bounceOwners[pixel] < 0 ) {
					continue;
				}
				unsigned __int64 total[3] = { 0, 0, 0 };
				unsigned __int64 aoTotal = 0;
				unsigned int totalWeight = 0;
				for ( int offsetY = -1; offsetY <= 1; offsetY++ ) {
					for ( int offsetX = -1; offsetX <= 1; offsetX++ ) {
						const int sampleX = x + offsetX * step;
						const int sampleY = y + offsetY * step;
						if ( sampleX < 0 || sampleX >= LM_ATLAS_SIZE || sampleY < 0 || sampleY >= LM_ATLAS_SIZE ) {
							continue;
						}
						const int samplePixel = sampleY * LM_ATLAS_SIZE + sampleX;
						if ( !atlas->valid[samplePixel] || bounceOwners[samplePixel] != bounceOwners[pixel] ) {
							continue;
						}
						const float normalDot = normals[pixel] * normals[samplePixel];
						if ( normalDot < 0.8f ) {
							continue;
						}
						const idVec3 separation = positions[samplePixel] - positions[pixel];
						if ( idMath::Fabs( separation * normals[pixel] ) > 2.0f ||
							idMath::Fabs( separation * normals[samplePixel] ) > 2.0f ) {
							continue;
						}
						const int normalWeight = idMath::ClampInt( 1, 16,
							(int)( ( normalDot - 0.8f ) * 80.0f + 0.5f ) );
						const int weight = kernel[offsetX + 1] * kernel[offsetY + 1] * normalWeight;
						for ( int component = 0; component < 3; component++ ) {
							total[component] += (unsigned __int64)bounce[samplePixel * 3 + component] * weight;
						}
						aoTotal += (unsigned __int64)ambientVisibility[samplePixel] * weight;
						totalWeight += weight;
					}
				}
				for ( int component = 0; component < 3; component++ ) {
					nextBounce[pixel * 3 + component] = (unsigned short)(
						( total[component] + totalWeight / 2 ) / totalWeight );
				}
				nextAO[pixel] = (unsigned short)( ( aoTotal + totalWeight / 2 ) / totalWeight );
			}
		}
		bounce.Swap( nextBounce );
		ambientVisibility.Swap( nextAO );
	}
}

static void LM_FilterAndApplyBounce( lmAtlas_t *atlas, const idList<unsigned short> &bounce,
		const idList<unsigned short> &ambientVisibility, const idList<byte> &bounceAnchors,
		const idList<int> &bounceOwners, const idList<idVec3> &positions,
		const idList<idVec3> &normals ) {
	const int pixels = LM_ATLAS_SIZE * LM_ATLAS_SIZE;
	idList<unsigned short> filtered;
	idList<unsigned short> filteredAO;
	filtered.SetNum( pixels * 3 );
	filteredAO.SetNum( pixels );
	memset( filtered.Ptr(), 0, filtered.Num() * sizeof( filtered[0] ) );
	memset( filteredAO.Ptr(), 0xFF, filteredAO.Num() * sizeof( filteredAO[0] ) );
	const int kernel[5] = { 1, 4, 6, 4, 1 };

	for ( int y = 0; y < LM_ATLAS_SIZE; y++ ) {
		for ( int x = 0; x < LM_ATLAS_SIZE; x++ ) {
			const int pixel = y * LM_ATLAS_SIZE + x;
			if ( !atlas->valid[pixel] || bounceOwners[pixel] < 0 ) {
				continue;
			}
			unsigned int total[3] = { 0, 0, 0 };
			unsigned int aoTotal = 0;
			int totalWeight = 0;
			for ( int offsetY = -2; offsetY <= 2; offsetY++ ) {
				for ( int offsetX = -2; offsetX <= 2; offsetX++ ) {
					const int sampleX = x + offsetX;
					const int sampleY = y + offsetY;
					if ( sampleX < 0 || sampleX >= LM_ATLAS_SIZE || sampleY < 0 || sampleY >= LM_ATLAS_SIZE ) {
						continue;
					}
					const int samplePixel = sampleY * LM_ATLAS_SIZE + sampleX;
					if ( !atlas->valid[samplePixel] || !bounceAnchors[samplePixel] ||
						bounceOwners[samplePixel] != bounceOwners[pixel] ) {
						continue;
					}
					const float normalDot = normals[pixel] * normals[samplePixel];
					const idVec3 separation = positions[samplePixel] - positions[pixel];
					if ( normalDot < 0.8f || idMath::Fabs( separation * normals[pixel] ) > 2.0f ||
						idMath::Fabs( separation * normals[samplePixel] ) > 2.0f ) {
						continue;
					}
					const int normalWeight = idMath::ClampInt( 1, 16,
						(int)( ( normalDot - 0.8f ) * 80.0f + 0.5f ) );
					const int weight = kernel[offsetX + 2] * kernel[offsetY + 2] * normalWeight;
					for ( int component = 0; component < 3; component++ ) {
						total[component] += bounce[samplePixel * 3 + component] * weight;
					}
					aoTotal += ambientVisibility[samplePixel] * weight;
					totalWeight += weight;
				}
			}
			if ( totalWeight == 0 ) {
				continue;
			}
			for ( int component = 0; component < 3; component++ ) {
				filtered[pixel * 3 + component] = (unsigned short)( ( total[component] + totalWeight / 2 ) / totalWeight );
			}
			filteredAO[pixel] = (unsigned short)( ( aoTotal + totalWeight / 2 ) / totalWeight );
		}
	}
	LM_DenoiseIndirect( atlas, bounceOwners, positions, normals, filtered, filteredAO );

	const int ambientByte = LM_ClampByte( lmAmbient );
	for ( int pixel = 0; pixel < pixels; pixel++ ) {
		if ( !atlas->valid[pixel] || bounceOwners[pixel] < 0 ) {
			continue;
		}
		lmBounceTexels++;
		const int aoByte = ( filteredAO[pixel] * 255 + 32767 ) / 65535;
		lmAOByteSum += aoByte;
		bool nonzero = false;
		for ( int component = 0; component < 3; component++ ) {
			const int unoccludedBounce = ( filtered[pixel * 3 + component] * 255 + 32767 ) / 65535;
			const int bounceByte = ( unoccludedBounce * filteredAO[pixel] + 32767 ) / 65535;
			const int occludedAmbient = ( ambientByte * filteredAO[pixel] + 32767 ) / 65535;
			const int directWithoutAmbient = Max( 0,
				(int)atlas->lightmap[pixel * 4 + component] - ambientByte );
			lmBounceByteSum += bounceByte;
			nonzero |= bounceByte > 0;
			atlas->lightmap[pixel * 4 + component] = (byte)Min( 255,
				directWithoutAmbient + occludedAmbient + bounceByte );
		}
		if ( nonzero ) {
			lmBounceNonzeroTexels++;
		}
	}
}

static void LM_BakeSecondaryBounce( void ) {
	if ( ( lmBounceScale <= 0.0f && lmAOStrength <= 0.0f ) || lmBounceSamples <= 0 || lmAtlases.Num() == 0 ) {
		return;
	}
	const int pixels = LM_ATLAS_SIZE * LM_ATLAS_SIZE;
	common->Printf( "\n----- BakeIndirectLighting (bounce %.3f, %i rays per %ix%i grid, distance %.0f, AO %.2f/%.0f, denoise %i) -----\n",
		lmBounceScale, lmBounceSamples, lmBounceSpacing, lmBounceSpacing, lmBounceDistance,
		lmAOStrength, lmAODistance, lmDenoisePasses );
	if ( lmBounceScale > 0.0f ) {
		for ( int atlasIndex = 0; atlasIndex < lmAtlases.Num(); atlasIndex++ ) {
			lmAtlas_t *atlas = lmAtlases[atlasIndex];
			atlas->directLightmap.SetNum( pixels * 3 );
			for ( int pixel = 0; pixel < pixels; pixel++ ) {
				for ( int component = 0; component < 3; component++ ) {
					atlas->directLightmap[pixel * 3 + component] = atlas->lightmap[pixel * 4 + component];
				}
			}
		}
	}

	for ( int atlasIndex = 0; atlasIndex < lmAtlases.Num(); atlasIndex++ ) {
		idList<unsigned short> bounce;
		idList<unsigned short> ambientVisibility;
		idList<byte> bounceAnchors;
		idList<int> bounceOwners;
		idList<idVec3> positions;
		idList<idVec3> normals;
		bounce.SetNum( pixels * 3 );
		ambientVisibility.SetNum( pixels );
		bounceAnchors.SetNum( pixels );
		bounceOwners.SetNum( pixels );
		positions.SetNum( pixels );
		normals.SetNum( pixels );
		memset( bounce.Ptr(), 0, bounce.Num() * sizeof( bounce[0] ) );
		memset( ambientVisibility.Ptr(), 0xFF, ambientVisibility.Num() * sizeof( ambientVisibility[0] ) );
		memset( bounceAnchors.Ptr(), 0, bounceAnchors.Num() * sizeof( bounceAnchors[0] ) );
		memset( bounceOwners.Ptr(), 0xFF, bounceOwners.Num() * sizeof( bounceOwners[0] ) );
		for ( int surfaceIndex = 0; surfaceIndex < lmSurfaces.Num(); surfaceIndex++ ) {
			if ( lmSurfaces[surfaceIndex]->atlas == atlasIndex ) {
				LM_MarkBounceSurfaceOwners( lmSurfaces[surfaceIndex], surfaceIndex,
					bounceOwners, positions, normals );
			}
		}
		for ( int surfaceIndex = 0; surfaceIndex < lmSurfaces.Num(); surfaceIndex++ ) {
			if ( lmSurfaces[surfaceIndex]->atlas == atlasIndex ) {
				LM_RasterizeBounceSurface( lmSurfaces[surfaceIndex], surfaceIndex, bounce,
					ambientVisibility, bounceAnchors, bounceOwners, positions, normals );
			}
		}
		LM_FilterAndApplyBounce( lmAtlases[atlasIndex], bounce, ambientVisibility,
			bounceAnchors, bounceOwners, positions, normals );
		common->Printf( "." );
	}
	const float averageBounceByte = lmBounceTexels ?
		(float)( (double)lmBounceByteSum / ( lmBounceTexels * 3.0 ) ) : 0.0f;
	const float bounceCoverage = lmBounceTexels ? 100.0f * lmBounceNonzeroTexels / lmBounceTexels : 0.0f;
	const float averageAO = lmBounceTexels ? (float)( (double)lmAOByteSum / ( lmBounceTexels * 255.0 ) ) : 1.0f;
	common->Printf( "\n%llu gather points, %llu bounce/AO rays, %llu geometry hits, %llu lit-surface hits, average bounce %.2f / 255, coverage %.1f%%, average AO visibility %.3f\n",
		lmBounceAnchorTexels, lmBounceRays, lmBounceHits, lmBounceContributingHits,
		averageBounceByte, bounceCoverage, averageAO );
	for ( int atlasIndex = 0; atlasIndex < lmAtlases.Num(); atlasIndex++ ) {
		lmAtlases[atlasIndex]->directLightmap.Clear();
	}
}

static void LM_DilateAtlas( lmAtlas_t *atlas ) {
	const int pixels = LM_ATLAS_SIZE * LM_ATLAS_SIZE;
	idList<byte> nextLight;
	idList<byte> nextDeluxe;
	idList<byte> nextValid;
	nextLight.SetNum( pixels * 4 );
	nextDeluxe.SetNum( pixels * 4 );
	nextValid.SetNum( pixels );

	for ( int pass = 0; pass < LM_PADDING; pass++ ) {
		memcpy( nextLight.Ptr(), atlas->lightmap.Ptr(), nextLight.Num() );
		memcpy( nextDeluxe.Ptr(), atlas->deluxemap.Ptr(), nextDeluxe.Num() );
		memcpy( nextValid.Ptr(), atlas->valid.Ptr(), nextValid.Num() );
		for ( int y = 0; y < LM_ATLAS_SIZE; y++ ) {
			for ( int x = 0; x < LM_ATLAS_SIZE; x++ ) {
				const int pixel = y * LM_ATLAS_SIZE + x;
				if ( atlas->valid[pixel] ) {
					continue;
				}
				const int neighbors[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
				for ( int n = 0; n < 4; n++ ) {
					const int nx = x + neighbors[n][0];
					const int ny = y + neighbors[n][1];
					if ( nx < 0 || nx >= LM_ATLAS_SIZE || ny < 0 || ny >= LM_ATLAS_SIZE ) {
						continue;
					}
					const int source = ny * LM_ATLAS_SIZE + nx;
					if ( !atlas->valid[source] ) {
						continue;
					}
					memcpy( &nextLight[pixel * 4], &atlas->lightmap[source * 4], 4 );
					memcpy( &nextDeluxe[pixel * 4], &atlas->deluxemap[source * 4], 4 );
					nextValid[pixel] = 1;
					break;
				}
			}
		}
		memcpy( atlas->lightmap.Ptr(), nextLight.Ptr(), nextLight.Num() );
		memcpy( atlas->deluxemap.Ptr(), nextDeluxe.Ptr(), nextDeluxe.Num() );
		memcpy( atlas->valid.Ptr(), nextValid.Ptr(), nextValid.Num() );
	}
}

static unsigned short LM_PackRGB565( const byte *rgb ) {
	return (unsigned short)( ( ( rgb[0] * 31 + 127 ) / 255 ) << 11 |
		( ( rgb[1] * 63 + 127 ) / 255 ) << 5 |
		( ( rgb[2] * 31 + 127 ) / 255 ) );
}

static void LM_UnpackRGB565( unsigned short color, byte *rgb ) {
	const int r = ( color >> 11 ) & 31;
	const int g = ( color >> 5 ) & 63;
	const int b = color & 31;
	rgb[0] = (byte)( ( r << 3 ) | ( r >> 2 ) );
	rgb[1] = (byte)( ( g << 2 ) | ( g >> 4 ) );
	rgb[2] = (byte)( ( b << 3 ) | ( b >> 2 ) );
}

static void LM_CompressDXT1Block( const byte pixels[16][4], byte output[8] ) {
	idVec3 mean( 0.0f, 0.0f, 0.0f );
	for ( int i = 0; i < 16; i++ ) {
		mean[0] += pixels[i][0];
		mean[1] += pixels[i][1];
		mean[2] += pixels[i][2];
	}
	mean *= 1.0f / 16.0f;

	float covariance[3][3];
	memset( covariance, 0, sizeof( covariance ) );
	for ( int i = 0; i < 16; i++ ) {
		const idVec3 delta( pixels[i][0] - mean[0], pixels[i][1] - mean[1], pixels[i][2] - mean[2] );
		for ( int row = 0; row < 3; row++ ) {
			for ( int column = 0; column < 3; column++ ) {
				covariance[row][column] += delta[row] * delta[column];
			}
		}
	}

	idVec3 axis( 1.0f, 1.0f, 1.0f );
	axis.Normalize();
	for ( int iteration = 0; iteration < 4; iteration++ ) {
		idVec3 next;
		for ( int row = 0; row < 3; row++ ) {
			next[row] = covariance[row][0] * axis[0] + covariance[row][1] * axis[1] + covariance[row][2] * axis[2];
		}
		if ( next.Normalize() == 0.0f ) {
			break;
		}
		axis = next;
	}

	int minimumPixel = 0;
	int maximumPixel = 0;
	float minimumProjection = idMath::INFINITY;
	float maximumProjection = -idMath::INFINITY;
	for ( int i = 0; i < 16; i++ ) {
		const float projection = pixels[i][0] * axis[0] + pixels[i][1] * axis[1] + pixels[i][2] * axis[2];
		if ( projection < minimumProjection ) {
			minimumProjection = projection;
			minimumPixel = i;
		}
		if ( projection > maximumProjection ) {
			maximumProjection = projection;
			maximumPixel = i;
		}
	}

	unsigned short color0 = LM_PackRGB565( pixels[maximumPixel] );
	unsigned short color1 = LM_PackRGB565( pixels[minimumPixel] );
	if ( color0 < color1 ) {
		idSwap( color0, color1 );
	}

	byte palette[4][3];
	LM_UnpackRGB565( color0, palette[0] );
	LM_UnpackRGB565( color1, palette[1] );
	for ( int component = 0; component < 3; component++ ) {
		palette[2][component] = (byte)( ( 2 * palette[0][component] + palette[1][component] + 1 ) / 3 );
		palette[3][component] = (byte)( ( palette[0][component] + 2 * palette[1][component] + 1 ) / 3 );
	}

	unsigned int indices = 0;
	const int paletteCount = color0 == color1 ? 2 : 4;
	for ( int i = 0; i < 16; i++ ) {
		int bestIndex = 0;
		int bestError = INT_MAX;
		for ( int paletteIndex = 0; paletteIndex < paletteCount; paletteIndex++ ) {
			const int dr = (int)pixels[i][0] - palette[paletteIndex][0];
			const int dg = (int)pixels[i][1] - palette[paletteIndex][1];
			const int db = (int)pixels[i][2] - palette[paletteIndex][2];
			const int error = dr * dr + dg * dg + db * db;
			if ( error < bestError ) {
				bestError = error;
				bestIndex = paletteIndex;
			}
		}
		indices |= bestIndex << ( i * 2 );
	}

	output[0] = (byte)( color0 & 255 );
	output[1] = (byte)( color0 >> 8 );
	output[2] = (byte)( color1 & 255 );
	output[3] = (byte)( color1 >> 8 );
	output[4] = (byte)( indices & 255 );
	output[5] = (byte)( ( indices >> 8 ) & 255 );
	output[6] = (byte)( ( indices >> 16 ) & 255 );
	output[7] = (byte)( ( indices >> 24 ) & 255 );
}

static void LM_MakeDDS( const idList<byte> &rgba, idList<byte> &dds ) {
	const int blockWidth = ( LM_ATLAS_SIZE + 3 ) / 4;
	const int blockHeight = ( LM_ATLAS_SIZE + 3 ) / 4;
	const int compressedBytes = blockWidth * blockHeight * 8;
	ddsFileHeader_t header;
	memset( &header, 0, sizeof( header ) );
	header.dwSize = sizeof( header );
	header.dwFlags = DDSF_CAPS | DDSF_HEIGHT | DDSF_WIDTH | DDSF_PIXELFORMAT | DDSF_LINEARSIZE;
	header.dwHeight = LM_ATLAS_SIZE;
	header.dwWidth = LM_ATLAS_SIZE;
	header.dwPitchOrLinearSize = compressedBytes;
	header.ddspf.dwSize = sizeof( header.ddspf );
	header.ddspf.dwFlags = DDSF_FOURCC;
	header.ddspf.dwFourCC = DDS_MAKEFOURCC( 'D', 'X', 'T', '1' );
	header.dwCaps1 = DDSF_TEXTURE;

	dds.SetNum( 4 + sizeof( header ) + compressedBytes );
	memcpy( dds.Ptr(), "DDS ", 4 );
	memcpy( dds.Ptr() + 4, &header, sizeof( header ) );
	byte *compressed = dds.Ptr() + 4 + sizeof( header );
	for ( int blockY = 0; blockY < blockHeight; blockY++ ) {
		for ( int blockX = 0; blockX < blockWidth; blockX++ ) {
			byte block[16][4];
			for ( int y = 0; y < 4; y++ ) {
				// Compressed rows are uploaded directly, and already match the proc lightmap T coordinates.
				const int sourceY = idMath::ClampInt( 0, LM_ATLAS_SIZE - 1, blockY * 4 + y );
				for ( int x = 0; x < 4; x++ ) {
					const int sourceX = idMath::ClampInt( 0, LM_ATLAS_SIZE - 1, blockX * 4 + x );
					memcpy( block[y * 4 + x], &rgba[( sourceY * LM_ATLAS_SIZE + sourceX ) * 4], 4 );
				}
			}
			LM_CompressDXT1Block( block, compressed );
			compressed += 8;
		}
	}
}

static void LM_ZipAdd( idList<lmZipEntry_t *> &entries, const char *name, const void *data, int length ) {
	lmZipEntry_t *entry = new lmZipEntry_t;
	entry->name = name;
	entry->name.BackSlashesToSlashes();
	entry->data.SetNum( length );
	if ( length > 0 ) {
		memcpy( entry->data.Ptr(), data, length );
	}
	entry->crc = (unsigned int)CRC32_BlockChecksum( data, length );
	entry->localOffset = 0;
	entries.Append( entry );
}

static void LM_WriteZip( const char *qpath, idList<lmZipEntry_t *> &entries ) {
	idFile *file = fileSystem->OpenFileWrite( qpath, "fs_devpath" );
	if ( !file ) {
		common->Error( "Lightmap baker could not create %s", qpath );
	}

	for ( int i = 0; i < entries.Num(); i++ ) {
		lmZipEntry_t *entry = entries[i];
		entry->localOffset = (unsigned int)file->Tell();
		file->WriteUnsignedInt( 0x04034b50 );
		file->WriteUnsignedShort( 20 );
		file->WriteUnsignedShort( 0 );
		file->WriteUnsignedShort( 0 );
		file->WriteUnsignedShort( 0 );
		file->WriteUnsignedShort( 0 );
		file->WriteUnsignedInt( entry->crc );
		file->WriteUnsignedInt( entry->data.Num() );
		file->WriteUnsignedInt( entry->data.Num() );
		file->WriteUnsignedShort( (unsigned short)entry->name.Length() );
		file->WriteUnsignedShort( 0 );
		file->Write( entry->name.c_str(), entry->name.Length() );
		file->Write( entry->data.Ptr(), entry->data.Num() );
	}

	const unsigned int centralOffset = (unsigned int)file->Tell();
	for ( int i = 0; i < entries.Num(); i++ ) {
		const lmZipEntry_t *entry = entries[i];
		file->WriteUnsignedInt( 0x02014b50 );
		file->WriteUnsignedShort( 20 );
		file->WriteUnsignedShort( 20 );
		file->WriteUnsignedShort( 0 );
		file->WriteUnsignedShort( 0 );
		file->WriteUnsignedShort( 0 );
		file->WriteUnsignedShort( 0 );
		file->WriteUnsignedInt( entry->crc );
		file->WriteUnsignedInt( entry->data.Num() );
		file->WriteUnsignedInt( entry->data.Num() );
		file->WriteUnsignedShort( (unsigned short)entry->name.Length() );
		file->WriteUnsignedShort( 0 );
		file->WriteUnsignedShort( 0 );
		file->WriteUnsignedShort( 0 );
		file->WriteUnsignedShort( 0 );
		file->WriteUnsignedInt( 0 );
		file->WriteUnsignedInt( entry->localOffset );
		file->Write( entry->name.c_str(), entry->name.Length() );
	}
	const unsigned int centralSize = (unsigned int)file->Tell() - centralOffset;
	file->WriteUnsignedInt( 0x06054b50 );
	file->WriteUnsignedShort( 0 );
	file->WriteUnsignedShort( 0 );
	file->WriteUnsignedShort( (unsigned short)entries.Num() );
	file->WriteUnsignedShort( (unsigned short)entries.Num() );
	file->WriteUnsignedInt( centralSize );
	file->WriteUnsignedInt( centralOffset );
	file->WriteUnsignedShort( 0 );
	fileSystem->CloseFile( file );
}

void Lightmap_Begin( const char *mapFileBase ) {
	lmSurfaces.DeleteContents( true );
	lmAtlases.DeleteContents( true );
	LM_ClearAlphaMasks();
	LM_ClearBakedLights();
	lmOccluders.Clear();
	lmOccluderOrder.Clear();
	lmTraceNodes.Clear();
	lmMapFileBase = mapFileBase;
	lmAmbient = LM_DEFAULT_AMBIENT;
	lmDirectScale = LM_DEFAULT_DIRECT_SCALE;
	lmShadowSamples = LM_DEFAULT_SHADOW_SAMPLES;
	lmBounceScale = LM_DEFAULT_BOUNCE_SCALE;
	lmBounceSamples = LM_DEFAULT_BOUNCE_SAMPLES;
	lmBounceDistance = LM_DEFAULT_BOUNCE_DISTANCE;
	lmBounceSpacing = LM_DEFAULT_BOUNCE_SPACING;
	lmDenoisePasses = LM_DEFAULT_DENOISE_PASSES;
	lmAOStrength = LM_DEFAULT_AO_STRENGTH;
	lmAODistance = LM_DEFAULT_AO_DISTANCE;
	if ( dmapGlobals.uEntities && dmapGlobals.num_entities > 0 && dmapGlobals.uEntities[0].mapEntity ) {
		const idDict &worldSpawn = dmapGlobals.uEntities[0].mapEntity->epairs;
		worldSpawn.GetFloat( "lightmapAmbient", "0.025", lmAmbient );
		worldSpawn.GetFloat( "lightmapDirectScale", "2.0", lmDirectScale );
		worldSpawn.GetInt( "lightmapShadowSamples", "9", lmShadowSamples );
		worldSpawn.GetFloat( "lightmapBounceScale", "0.65", lmBounceScale );
		worldSpawn.GetInt( "lightmapBounceSamples", "32", lmBounceSamples );
		worldSpawn.GetFloat( "lightmapBounceDistance", "4096", lmBounceDistance );
		worldSpawn.GetInt( "lightmapBounceSpacing", "2", lmBounceSpacing );
		worldSpawn.GetInt( "lightmapDenoisePasses", "3", lmDenoisePasses );
		worldSpawn.GetFloat( "lightmapAOStrength", "0.65", lmAOStrength );
		worldSpawn.GetFloat( "lightmapAODistance", "64", lmAODistance );
	}
	lmAmbient = idMath::ClampFloat( 0.0f, 1.0f, lmAmbient );
	lmDirectScale = idMath::ClampFloat( 0.0f, 16.0f, lmDirectScale );
	lmShadowSamples = lmShadowSamples <= 1 ? 1 : ( lmShadowSamples <= 5 ? 5 : 9 );
	lmBounceScale = idMath::ClampFloat( 0.0f, 2.0f, lmBounceScale );
	lmBounceSamples = idMath::ClampInt( 1, 64, lmBounceSamples );
	lmBounceDistance = idMath::ClampFloat( 64.0f, 16384.0f, lmBounceDistance );
	lmBounceSpacing = idMath::ClampInt( 1, 2, lmBounceSpacing );
	lmDenoisePasses = idMath::ClampInt( 0, 3, lmDenoisePasses );
	lmAOStrength = idMath::ClampFloat( 0.0f, 1.0f, lmAOStrength );
	lmAODistance = idMath::ClampFloat( 1.0f, 1024.0f, lmAODistance );
	lmShadePoints = 0;
	lmLightTests = 0;
	lmLightsInVolume = 0;
	lmLightsOccluded = 0;
	lmLightsContributing = 0;
	lmShadowRays = 0;
	lmMixedShadowTests = 0;
	lmBounceRays = 0;
	lmBounceHits = 0;
	lmBounceContributingHits = 0;
	lmBounceAnchorTexels = 0;
	lmBounceTexels = 0;
	lmBounceNonzeroTexels = 0;
	lmBounceByteSum = 0;
	lmAOByteSum = 0;
	lmAlphaTests = 0;
	lmAlphaPassThroughs = 0;
	lmExternalOccluders = 0;
	lmDoorOccluders = 0;
	LM_BuildBakedLights();
	LM_AddExternalModelOccluders();
	common->Printf( "----- BakeLightmaps (ambient %.3f, direct scale %.3f, adaptive shadow samples %i) -----\n",
		lmAmbient, lmDirectScale, lmShadowSamples );
	common->Printf( "%i baked lights using exact shader projection/falloff sampling\n", lmExactBakedLights );
	common->Printf( "%i external static-model occluder triangles\n", lmExternalOccluders );
}

int Lightmap_AddSurface( int entityNum, const idMaterial *material, const srfTriangles_t *tri,
		idList<idVec2> &lightmapTexCoords ) {
	lightmapTexCoords.SetNum( tri->numVerts );
	for ( int i = 0; i < lightmapTexCoords.Num(); i++ ) {
		lightmapTexCoords[i].Zero();
	}

	if ( entityNum < 0 || entityNum >= dmapGlobals.num_entities ) {
		return -1;
	}
	const uEntity_t &entity = dmapGlobals.uEntities[entityNum];
	const bool bakedReceiver = LM_IsBakedStaticEntity( entity.mapEntity, entityNum == 0 );
	const bool doorShadow = LM_IsDoorEntity( entity.mapEntity );
	if ( !bakedReceiver && !doorShadow ) {
		return -1;
	}
	idVec3 entityOrigin;
	idMat3 entityAxis;
	LM_GetEntityTransform( entityNum, entityOrigin, entityAxis );
	const bool castsShadow = !entity.mapEntity->epairs.GetBool( "noshadows", "0" ) &&
		material->Coverage() != MC_TRANSLUCENT &&
		( material->Coverage() == MC_PERFORATED || material->SurfaceCastsShadow() );
	const bool receivesLightmap = bakedReceiver && material->IsDrawn() && material->ReceivesLighting() &&
		material->Coverage() == MC_OPAQUE && tri->numVerts > 0;
	if ( !receivesLightmap ) {
		if ( castsShadow ) {
			LM_AddTraceTriangles( tri, entityOrigin, entityAxis, material, true, -1, NULL, doorShadow );
		}
		return -1;
	}

	idVec3 averageNormal( 0.0f, 0.0f, 0.0f );
	for ( int i = 0; i < tri->numVerts; i++ ) {
		averageNormal += tri->verts[i].normal;
	}
	if ( averageNormal.Normalize() == 0.0f && tri->numIndexes >= 3 ) {
		averageNormal = ( tri->verts[tri->indexes[1]].xyz - tri->verts[tri->indexes[0]].xyz ).Cross(
			tri->verts[tri->indexes[2]].xyz - tri->verts[tri->indexes[0]].xyz );
		averageNormal.Normalize();
	}
	int dominantAxis = 0;
	if ( idMath::Fabs( averageNormal[1] ) > idMath::Fabs( averageNormal[dominantAxis] ) ) {
		dominantAxis = 1;
	}
	if ( idMath::Fabs( averageNormal[2] ) > idMath::Fabs( averageNormal[dominantAxis] ) ) {
		dominantAxis = 2;
	}
	const int uAxis = dominantAxis == 0 ? 1 : 0;
	const int vAxis = dominantAxis == 2 ? 1 : 2;
	float minU = idMath::INFINITY;
	float minV = idMath::INFINITY;
	float maxU = -idMath::INFINITY;
	float maxV = -idMath::INFINITY;
	for ( int i = 0; i < tri->numVerts; i++ ) {
		minU = Min( minU, tri->verts[i].xyz[uAxis] );
		minV = Min( minV, tri->verts[i].xyz[vAxis] );
		maxU = Max( maxU, tri->verts[i].xyz[uAxis] );
		maxV = Max( maxV, tri->verts[i].xyz[vAxis] );
	}

	const int maxInner = LM_ATLAS_SIZE - LM_PADDING * 2 - 1;
	float sampleScale = LM_DEFAULT_SCALE;
	sampleScale = Max( sampleScale, ( maxU - minU ) / maxInner );
	sampleScale = Max( sampleScale, ( maxV - minV ) / maxInner );
	const int innerWidth = Max( 1, (int)ceilf( ( maxU - minU ) / sampleScale ) + 1 );
	const int innerHeight = Max( 1, (int)ceilf( ( maxV - minV ) / sampleScale ) + 1 );
	int chartX;
	int chartY;
	const int atlasIndex = LM_AllocChart( innerWidth + LM_PADDING * 2, innerHeight + LM_PADDING * 2, chartX, chartY );
	for ( int i = 0; i < tri->numVerts; i++ ) {
		lightmapTexCoords[i][0] = ( chartX + LM_PADDING + ( tri->verts[i].xyz[uAxis] - minU ) / sampleScale + 0.5f ) / LM_ATLAS_SIZE;
		lightmapTexCoords[i][1] = ( chartY + LM_PADDING + ( tri->verts[i].xyz[vAxis] - minV ) / sampleScale + 0.5f ) / LM_ATLAS_SIZE;
	}
	LM_AddTraceTriangles( tri, entityOrigin, entityAxis, material, castsShadow,
		atlasIndex, &lightmapTexCoords, false );

	lmSurface_t *surface = new lmSurface_t;
	surface->atlas = atlasIndex;
	surface->material = material;
	surface->verts.SetNum( tri->numVerts );
	surface->indexes.SetNum( tri->numIndexes );
	surface->lightmapTexCoords = lightmapTexCoords;
	memcpy( surface->verts.Ptr(), tri->verts, tri->numVerts * sizeof( idDrawVert ) );
	memcpy( surface->indexes.Ptr(), tri->indexes, tri->numIndexes * sizeof( glIndex_t ) );
	for ( int i = 0; i < surface->verts.Num(); i++ ) {
		surface->verts[i].xyz = entityOrigin + surface->verts[i].xyz * entityAxis;
		surface->verts[i].normal = surface->verts[i].normal * entityAxis;
		surface->verts[i].tangents[0] = surface->verts[i].tangents[0] * entityAxis;
		surface->verts[i].tangents[1] = surface->verts[i].tangents[1] * entityAxis;
	}
	lmSurfaces.Append( surface );
	return atlasIndex;
}

void Lightmap_End( void ) {
	common->Printf( "%i lightmap surfaces, %i atlases, %i trace triangles (%i door shadow-only)\n",
		lmSurfaces.Num(), lmAtlases.Num(), lmOccluders.Num(), lmDoorOccluders );

	lmOccluderOrder.SetNum( lmOccluders.Num() );
	for ( int i = 0; i < lmOccluderOrder.Num(); i++ ) {
		lmOccluderOrder[i] = i;
	}
	if ( lmOccluderOrder.Num() > 0 ) {
		LM_BuildTraceNode( 0, lmOccluderOrder.Num() );
	}
	for ( int i = 0; i < lmSurfaces.Num(); i++ ) {
		LM_RasterizeSurface( lmSurfaces[i] );
		if ( ( i & 31 ) == 31 ) {
			common->Printf( "." );
		}
	}
	LM_BakeSecondaryBounce();

	idList<lmZipEntry_t *> entries;
	idStr manifest;
	unsigned __int64 validTexels = 0;
	unsigned __int64 lightByteSum = 0;
	for ( int atlasIndex = 0; atlasIndex < lmAtlases.Num(); atlasIndex++ ) {
		const lmAtlas_t *atlas = lmAtlases[atlasIndex];
		for ( int pixel = 0; pixel < LM_ATLAS_SIZE * LM_ATLAS_SIZE; pixel++ ) {
			if ( !atlas->valid[pixel] ) {
				continue;
			}
			validTexels++;
			lightByteSum += atlas->lightmap[pixel * 4 + 0];
			lightByteSum += atlas->lightmap[pixel * 4 + 1];
			lightByteSum += atlas->lightmap[pixel * 4 + 2];
		}
	}
	const float averageLightByte = validTexels ? (float)( (double)lightByteSum / ( validTexels * 3.0 ) ) : 0.0f;
	common->Printf( "lightmap samples: %llu points, %llu light tests, %llu in volume, %llu shadowed, %llu contributing\n",
		lmShadePoints, lmLightTests, lmLightsInVolume, lmLightsOccluded, lmLightsContributing );
	common->Printf( "%llu shadow rays, %llu mixed-coverage shadow tests refined\n",
		lmShadowRays, lmMixedShadowTests );
	common->Printf( "%i alpha-test materials, %llu alpha intersections, %llu passed through holes\n",
		lmAlphaMasks.Num(), lmAlphaTests, lmAlphaPassThroughs );
	common->Printf( "%llu valid luxels, average light %.1f / 255\n", validTexels, averageLightByte );

	manifest = "lightmapArchiveVersion 3\n";
	manifest += va( "procFileId %s\n", PROC_FILE_ID );
	manifest += "lightingComplete 1\n";
	manifest += "atlasFormat DDS_DXT1\n";
	manifest += va( "atlasSize %i\n", LM_ATLAS_SIZE );
	manifest += va( "sampleScale %g\n", LM_DEFAULT_SCALE );
	manifest += va( "shadowSamples %i\n", lmShadowSamples );
	manifest += va( "bouncePasses %i\n", lmBounceScale > 0.0f ? 1 : 0 );
	manifest += va( "bounceScale %g\n", lmBounceScale );
	manifest += va( "bounceSamples %i\n", lmBounceSamples );
	manifest += va( "bounceDistance %g\n", lmBounceDistance );
	manifest += va( "bounceSpacing %i\n", lmBounceSpacing );
	manifest += va( "denoisePasses %i\n", lmDenoisePasses );
	manifest += va( "bounceGatherPoints %llu\n", lmBounceAnchorTexels );
	manifest += va( "bounceRays %llu\n", lmBounceRays );
	manifest += va( "bounceGeometryHits %llu\n", lmBounceHits );
	manifest += va( "bounceLitSurfaceHits %llu\n", lmBounceContributingHits );
	manifest += va( "bounceNonzeroTexels %llu\n", lmBounceNonzeroTexels );
	manifest += va( "averageBounceByte %g\n", lmBounceTexels ?
		(double)lmBounceByteSum / ( lmBounceTexels * 3.0 ) : 0.0 );
	manifest += va( "aoStrength %g\n", lmAOStrength );
	manifest += va( "aoDistance %g\n", lmAODistance );
	manifest += va( "averageAOVisibility %g\n", lmBounceTexels ?
		(double)lmAOByteSum / ( lmBounceTexels * 255.0 ) : 1.0 );
	manifest += va( "alphaTestMaterials %i\n", lmAlphaMasks.Num() );
	manifest += va( "alphaTestIntersections %llu\n", lmAlphaTests );
	manifest += va( "alphaPassThroughs %llu\n", lmAlphaPassThroughs );
	manifest += va( "doorShadowTriangles %i\n", lmDoorOccluders );
	manifest += va( "ambient %g\n", lmAmbient );
	manifest += va( "directScale %g\n", lmDirectScale );
	manifest += va( "exactLightShaderLights %i\n", lmExactBakedLights );
	manifest += va( "averageLightByte %g\n", averageLightByte );
	manifest += va( "numAtlases %i\n", lmAtlases.Num() );
	int numBakedLights = 0;
	for ( int i = 0; i < dmapGlobals.mapLights.Num(); i++ ) {
		if ( dmapGlobals.mapLights[i]->bake ) {
			numBakedLights++;
		}
	}
	manifest += va( "numBakedLights %i\n", numBakedLights );
	for ( int i = 0; i < dmapGlobals.mapLights.Num(); i++ ) {
		if ( dmapGlobals.mapLights[i]->bake ) {
			manifest += va( "bakedLight \"%s\"\n", dmapGlobals.mapLights[i]->name );
		}
	}
	idStr manifestName = lmMapFileBase + "/lightmaps.manifest";
	LM_ZipAdd( entries, manifestName, manifest.c_str(), manifest.Length() );

	for ( int i = 0; i < lmAtlases.Num(); i++ ) {
		LM_DilateAtlas( lmAtlases[i] );
		idList<byte> dds;
		idStr imageName;
		LM_MakeDDS( lmAtlases[i]->lightmap, dds );
		imageName = va( "dds/%s/lightmap_%03i.dds", lmMapFileBase.c_str(), i );
		LM_ZipAdd( entries, imageName, dds.Ptr(), dds.Num() );
		LM_MakeDDS( lmAtlases[i]->deluxemap, dds );
		imageName = va( "dds/%s/deluxemap_%03i.dds", lmMapFileBase.c_str(), i );
		LM_ZipAdd( entries, imageName, dds.Ptr(), dds.Num() );
	}

	idStr archiveName = lmMapFileBase;
	archiveName.SetFileExtension( "zip" );
	LM_WriteZip( archiveName, entries );
	common->Printf( "wrote %s\n", archiveName.c_str() );

	entries.DeleteContents( true );
	lmSurfaces.DeleteContents( true );
	lmAtlases.DeleteContents( true );
	LM_ClearAlphaMasks();
	LM_ClearBakedLights();
	lmOccluders.Clear();
	lmOccluderOrder.Clear();
	lmTraceNodes.Clear();
}
