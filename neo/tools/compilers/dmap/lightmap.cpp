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

typedef struct {
	idVec3 a;
	idVec3 b;
	idVec3 c;
	idBounds bounds;
	idVec3 center;
} lmOccluder_t;

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
static idList<int> lmOccluderOrder;
static idList<lmTraceNode_t> lmTraceNodes;
static float lmAmbient;
static float lmDirectScale;
static int lmShadowSamples;
static unsigned __int64 lmShadePoints;
static unsigned __int64 lmLightTests;
static unsigned __int64 lmLightsInVolume;
static unsigned __int64 lmLightsOccluded;
static unsigned __int64 lmLightsContributing;
static unsigned __int64 lmShadowRays;
static unsigned __int64 lmMixedShadowTests;
static int lmExternalOccluders;

static int LM_ClampByte( float value ) {
	return idMath::ClampInt( 0, 255, (int)( value * 255.0f + 0.5f ) );
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

static void LM_AddOccluders( const srfTriangles_t *tri, const idVec3 &origin, const idMat3 &axis ) {
	for ( int i = 0; i + 2 < tri->numIndexes; i += 3 ) {
		lmOccluder_t occluder;
		occluder.a = origin + tri->verts[tri->indexes[i + 0]].xyz * axis;
		occluder.b = origin + tri->verts[tri->indexes[i + 1]].xyz * axis;
		occluder.c = origin + tri->verts[tri->indexes[i + 2]].xyz * axis;
		occluder.bounds.Clear();
		occluder.bounds.AddPoint( occluder.a );
		occluder.bounds.AddPoint( occluder.b );
		occluder.bounds.AddPoint( occluder.c );
		occluder.bounds.ExpandSelf( 0.05f );
		occluder.center = ( occluder.a + occluder.b + occluder.c ) * ( 1.0f / 3.0f );
		lmOccluders.Append( occluder );
	}
}

static bool LM_IsBakedStaticEntity( const idMapEntity *mapEntity, bool worldEntity ) {
	if ( worldEntity ) {
		return true;
	}
	return Dmap_IsStaticEntityImmutable( mapEntity );
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
		if ( mapEntity->GetNumPrimitives() != 0 || !LM_IsBakedStaticEntity( mapEntity, false ) ||
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
			if ( !material || material->Coverage() != MC_OPAQUE || !material->SurfaceCastsShadow() ) {
				continue;
			}
			LM_AddOccluders( surface->geometry, renderEntity.origin, renderEntity.axis );
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

static bool LM_TraceTriangle( const lmOccluder_t &tri, const idVec3 &start, const idVec3 &dir ) {
	const idVec3 edge1 = tri.b - tri.a;
	const idVec3 edge2 = tri.c - tri.a;
	const idVec3 p = dir.Cross( edge2 );
	const float det = edge1 * p;
	if ( idMath::Fabs( det ) < 0.000001f ) {
		return false;
	}
	const float invDet = 1.0f / det;
	const idVec3 tvec = start - tri.a;
	const float u = ( tvec * p ) * invDet;
	if ( u < 0.0f || u > 1.0f ) {
		return false;
	}
	const idVec3 q = tvec.Cross( edge1 );
	const float v = ( dir * q ) * invDet;
	if ( v < 0.0f || u + v > 1.0f ) {
		return false;
	}
	const float fraction = ( edge2 * q ) * invDet;
	return fraction > 0.0001f && fraction < 0.9999f;
}

static bool LM_TraceNode( int nodeIndex, const idVec3 &start, const idVec3 &end, const idVec3 &dir ) {
	const lmTraceNode_t &node = lmTraceNodes[nodeIndex];
	if ( !node.bounds.LineIntersection( start, end ) ) {
		return false;
	}
	if ( node.count > 0 ) {
		for ( int i = node.first; i < node.first + node.count; i++ ) {
			if ( LM_TraceTriangle( lmOccluders[lmOccluderOrder[i]], start, dir ) ) {
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

static bool LM_PointInLight( const mapLight_t *light, const idVec3 &point, float &attenuation ) {
	const renderLight_t &parms = light->def.parms;
	if ( parms.pointLight ) {
		idVec3 local = ( point - parms.origin ) * parms.axis.Transpose();
		float distanceSquared = 0.0f;
		for ( int i = 0; i < 3; i++ ) {
			if ( parms.lightRadius[i] <= 0.0f ) {
				return false;
			}
			const float d = local[i] / parms.lightRadius[i];
			distanceSquared += d * d;
		}
		if ( distanceSquared >= 1.0f ) {
			return false;
		}
		// Doom point lights fill an oriented light volume.  Squaring the
		// remaining radial distance made almost the entire volume black and did
		// not resemble the projection/falloff textures used by realtime lights.
		attenuation = 1.0f - distanceSquared;
		return true;
	}

	for ( int i = 0; i < 6; i++ ) {
		if ( light->def.frustum[i].Distance( point ) > 0.0f ) {
			return false;
		}
	}
	attenuation = 1.0f;
	return true;
}

static float LM_ShadowVisibility( const idVec3 &point, const idVec3 &normal,
		const idVec3 &footprintS, const idVec3 &footprintT, const idVec3 &lightDirection,
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
		if ( LM_Occluded( samplePoint + normal * 0.5f, traceEnd ) ) {
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
			if ( LM_Occluded( samplePoint + normal * 0.5f, traceEnd ) ) {
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

		float lambert = light->def.lightShader && light->def.lightShader->IsAmbientLight() ? 1.0f : normal * lightDirection;
		if ( lambert <= 0.0f ) {
			continue;
		}
		float visibility = 1.0f;
		if ( !light->def.parms.noShadows ) {
			visibility = LM_ShadowVisibility( point, normal, footprintS, footprintT, lightDirection,
				light->def.globalLightOrigin, light->def.parms.parallel );
			if ( visibility < 1.0f ) {
				lmLightsOccluded++;
			}
			if ( visibility <= 0.0f ) {
				continue;
			}
		}

		const float weight = attenuation * lambert * lmDirectScale * visibility;
		idVec3 color( light->def.parms.shaderParms[SHADERPARM_RED],
			light->def.parms.shaderParms[SHADERPARM_GREEN],
			light->def.parms.shaderParms[SHADERPARM_BLUE] );
		irradiance += color * weight;
		lmLightsContributing++;
		const float luminance = color[0] * 0.2126f + color[1] * 0.7152f + color[2] * 0.0722f;
		directionSum += lightDirection * ( weight * luminance );
		directionWeight += weight * luminance;
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

		idVec3 faceNormal = ( b.xyz - a.xyz ).Cross( c.xyz - a.xyz );
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
	lmOccluders.Clear();
	lmOccluderOrder.Clear();
	lmTraceNodes.Clear();
	lmMapFileBase = mapFileBase;
	lmAmbient = LM_DEFAULT_AMBIENT;
	lmDirectScale = LM_DEFAULT_DIRECT_SCALE;
	lmShadowSamples = LM_DEFAULT_SHADOW_SAMPLES;
	if ( dmapGlobals.uEntities && dmapGlobals.num_entities > 0 && dmapGlobals.uEntities[0].mapEntity ) {
		const idDict &worldSpawn = dmapGlobals.uEntities[0].mapEntity->epairs;
		worldSpawn.GetFloat( "lightmapAmbient", "0.025", lmAmbient );
		worldSpawn.GetFloat( "lightmapDirectScale", "2.0", lmDirectScale );
		worldSpawn.GetInt( "lightmapShadowSamples", "9", lmShadowSamples );
	}
	lmAmbient = idMath::ClampFloat( 0.0f, 1.0f, lmAmbient );
	lmDirectScale = idMath::ClampFloat( 0.0f, 16.0f, lmDirectScale );
	lmShadowSamples = lmShadowSamples <= 1 ? 1 : ( lmShadowSamples <= 5 ? 5 : 9 );
	lmShadePoints = 0;
	lmLightTests = 0;
	lmLightsInVolume = 0;
	lmLightsOccluded = 0;
	lmLightsContributing = 0;
	lmShadowRays = 0;
	lmMixedShadowTests = 0;
	lmExternalOccluders = 0;
	LM_AddExternalModelOccluders();
	common->Printf( "----- BakeLightmaps (ambient %.3f, direct scale %.3f, adaptive shadow samples %i) -----\n",
		lmAmbient, lmDirectScale, lmShadowSamples );
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
	if ( !LM_IsBakedStaticEntity( entity.mapEntity, entityNum == 0 ) ) {
		return -1;
	}
	idVec3 entityOrigin;
	idMat3 entityAxis;
	LM_GetEntityTransform( entityNum, entityOrigin, entityAxis );
	if ( !entity.mapEntity->epairs.GetBool( "noshadows", "0" ) &&
		material->Coverage() == MC_OPAQUE && material->SurfaceCastsShadow() ) {
		LM_AddOccluders( tri, entityOrigin, entityAxis );
	}
	if ( !material->IsDrawn() || !material->ReceivesLighting() || material->Coverage() != MC_OPAQUE || tri->numVerts == 0 ) {
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
	common->Printf( "%i lightmap surfaces, %i atlases, %i occluder triangles\n",
		lmSurfaces.Num(), lmAtlases.Num(), lmOccluders.Num() );

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
	common->Printf( "%llu valid luxels, average light %.1f / 255\n", validTexels, averageLightByte );

	manifest = "lightmapArchiveVersion 3\n";
	manifest += va( "procFileId %s\n", PROC_FILE_ID );
	manifest += "lightingComplete 1\n";
	manifest += "atlasFormat DDS_DXT1\n";
	manifest += va( "atlasSize %i\n", LM_ATLAS_SIZE );
	manifest += va( "sampleScale %g\n", LM_DEFAULT_SCALE );
	manifest += va( "shadowSamples %i\n", lmShadowSamples );
	manifest += va( "ambient %g\n", lmAmbient );
	manifest += va( "directScale %g\n", lmDirectScale );
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
	lmOccluders.Clear();
	lmOccluderOrder.Clear();
	lmTraceNodes.Clear();
}
