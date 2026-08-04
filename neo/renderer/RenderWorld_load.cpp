/*
===========================================================================

DarklightNG Source Code
Copyright (C) 2026 - Justin Marshall(aka IceColdDuke).

This file is part of the DarklightNG GPL source code.
This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).

DarklightNG is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

DarklightNG is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

===========================================================================
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"
#include "../decllib/declAmbientCubeMap.h"


/*
================
idRenderWorldLocal::FreeWorld
================
*/
void idRenderWorldLocal::FreeWorld() {
	int i;
	atmosphere = NULL;

	// this will free all the lightDefs and entityDefs
	FreeDefs();

	// free all the portals and check light/model references
	for ( i = 0 ; i < numPortalAreas ; i++ ) {
		portalArea_t	*area;
		portal_t		*portal, *nextPortal;

		area = &portalAreas[i];
		for ( portal = area->portals ; portal ; portal = nextPortal ) {
			nextPortal = portal->next;
			delete portal->w;
			R_StaticFree( portal );
		}

		// there shouldn't be any remaining lightRefs or entityRefs
		if ( area->lightRefs.areaNext != &area->lightRefs ) {
			common->Error( "FreeWorld: unexpected remaining lightRefs" );
		}
		if ( area->entityRefs.areaNext != &area->entityRefs ) {
			common->Error( "FreeWorld: unexpected remaining entityRefs" );
		}
	}

	if ( portalAreas ) {
		R_StaticFree( portalAreas );
		portalAreas = NULL;
		numPortalAreas = 0;
		R_StaticFree( areaScreenRect );
		areaScreenRect = NULL;
	}

	if ( doublePortals ) {
		R_StaticFree( doublePortals );
		doublePortals = NULL;
		numInterAreaPortals = 0;
	}

	if ( areaNodes ) {
		R_StaticFree( areaNodes );
		areaNodes = NULL;
	}

	// free all the inline idRenderModels 
	for ( i = 0 ; i < localModels.Num() ; i++ ) {
		renderModelManager->RemoveModel( localModels[i] );
		delete localModels[i];
	}
	localModels.Clear();
	bakedAtlasImages.Clear();
	defaultAmbientCubeMap = NULL;
	megaTextureSTGrid.Clear();
	megaTextureBounds.Clear();
	megaTextureSTGridWidth = 0;
	megaTextureSTGridHeight = 0;

	if ( hasBakedLightmaps ) {
		fileSystem->UnmountMapArchive();
		hasBakedLightmaps = false;
	}
	bakedSurfaceCount = 0;
	bakedBatchCount = 0;
	bakedLightSuppressionCount = 0;
	bakedLightCandidateCount = 0;
	bakedLightMovedCount = 0;
	bakedDrawReported = false;
	ambientCubeDrawReported = false;

	areaReferenceAllocator.Shutdown();

	mapName = "<FREED>";
}

/*
================
idRenderWorldLocal::SetMegaTextureSTGrid
================
*/
void idRenderWorldLocal::SetMegaTextureSTGrid( const idBounds &bounds, const idVec2 *grid, int width, int height ) {
	megaTextureSTGrid.Clear();
	megaTextureBounds.Clear();
	megaTextureSTGridWidth = megaTextureSTGridHeight = 0;
	if ( !grid || width < 2 || height < 2 || bounds.IsCleared() ) {
		return;
	}
	megaTextureBounds = bounds;
	megaTextureSTGridWidth = width;
	megaTextureSTGridHeight = height;
	megaTextureSTGrid.SetNum( width * height );
	memcpy( megaTextureSTGrid.Ptr(), grid, width * height * sizeof( grid[0] ) );
}

/*
================
idRenderWorldLocal::TouchWorldModels
================
*/
void idRenderWorldLocal::TouchWorldModels( void ) {
	int i;

	for ( i = 0 ; i < localModels.Num() ; i++ ) {
		renderModelManager->CheckModel( localModels[i]->Name() );
	}
}

/*
================
R_CanBatchBakedSurfaces
================
*/
static bool R_CanBatchBakedSurfaces( const modelSurface_t &first, const modelSurface_t &second ) {
	const srfTriangles_t *firstTri = first.geometry;
	const srfTriangles_t *secondTri = second.geometry;
	if ( !firstTri || !secondTri || first.shader != second.shader ) {
		return false;
	}
	if ( first.shader->Coverage() != MC_OPAQUE || !first.shader->ReceivesLighting() || first.shader->IsDiscrete() ) {
		return false;
	}
	return firstTri->lightmapAtlas >= 0 &&
		firstTri->lightmapAtlas == secondTri->lightmapAtlas &&
		firstTri->lightmapTexCoords && secondTri->lightmapTexCoords &&
		firstTri->bakedLightmap && firstTri->bakedLightmap == secondTri->bakedLightmap &&
		firstTri->bakedDeluxemap && firstTri->bakedDeluxemap == secondTri->bakedDeluxemap;
}

/*
================
R_AddBatchedModelSurfaces

Dmap lightmap charts are intentionally separate surfaces in mapProcFile004.
Consecutive charts with identical draw state remain spatially local, so merge
them into one resident VBO/IBO without broadening culling to an entire model.
================
*/
static int R_AddBatchedModelSurfaces( idRenderModel *model, idList<modelSurface_t> &surfaces ) {
	int bakedBatches = 0;
	for ( int first = 0; first < surfaces.Num(); ) {
		int end = first + 1;
		while ( end < surfaces.Num() && R_CanBatchBakedSurfaces( surfaces[first], surfaces[end] ) ) {
			end++;
		}

		modelSurface_t outputSurface = surfaces[first];
		if ( end - first > 1 ) {
			idList<const srfTriangles_t *> batch;
			batch.SetNum( end - first );
			for ( int i = first; i < end; i++ ) {
				batch[i - first] = surfaces[i].geometry;
			}
			outputSurface.geometry = R_MergeSurfaceList( batch.Ptr(), batch.Num() );
			for ( int i = first; i < end; i++ ) {
				R_FreeStaticTriSurf( surfaces[i].geometry );
			}
		}

		model->AddSurface( outputSurface );
		if ( outputSurface.geometry->bakedLightmap && outputSurface.geometry->bakedDeluxemap ) {
			bakedBatches++;
		}
		first = end;
	}
	return bakedBatches;
}

/*
================
idRenderWorldLocal::ParseModel
================
*/
idRenderModel *idRenderWorldLocal::ParseModel( idLexer *src, bool hasLightmapUVs ) {
	idRenderModel	*model;
	idToken			token;
	int				i, j;
	srfTriangles_t	*tri;
	modelSurface_t	surf;
	idList<modelSurface_t> parsedSurfaces;

	src->ExpectTokenString( "{" );

	// parse the name
	src->ExpectAnyToken( &token );

	model = renderModelManager->AllocModel();
	model->InitEmpty( token );

	int numSurfaces = src->ParseInt();
	if ( numSurfaces < 0 ) {
		src->Error( "R_ParseModel: bad numSurfaces" );
	}

	for ( i = 0 ; i < numSurfaces ; i++ ) {
		src->ExpectTokenString( "{" );

		src->ExpectAnyToken( &token );

		surf.shader = declManager->FindMaterial( token );
		surf.id = i;

		((idMaterial*)surf.shader)->AddReference();

		tri = R_AllocStaticTriSurf();
		surf.geometry = tri;
		tri->lightmapAtlas = hasLightmapUVs ? src->ParseInt() : -1;

		tri->numVerts = src->ParseInt();
		tri->numIndexes = src->ParseInt();

		R_AllocStaticTriSurfVerts( tri, tri->numVerts );
		for ( j = 0 ; j < tri->numVerts ; j++ ) {
			float	vec[10];

			src->Parse1DMatrix( hasLightmapUVs ? 10 : 8, vec );

			tri->verts[j].xyz[0] = vec[0];
			tri->verts[j].xyz[1] = vec[1];
			tri->verts[j].xyz[2] = vec[2];
			tri->verts[j].st[0] = vec[3];
			tri->verts[j].st[1] = vec[4];
			tri->verts[j].normal[0] = vec[5];
			tri->verts[j].normal[1] = vec[6];
			tri->verts[j].normal[2] = vec[7];
			if ( hasLightmapUVs ) {
				if ( !tri->lightmapTexCoords ) {
					tri->lightmapTexCoords = (idVec2 *)R_StaticAlloc( tri->numVerts * sizeof( idVec2 ) );
				}
				tri->lightmapTexCoords[j][0] = vec[8];
				tri->lightmapTexCoords[j][1] = vec[9];
			}
		}

		if ( hasBakedLightmaps && tri->lightmapAtlas >= 0 ) {
			idStr imageName;
			imageName = va( "%s/lightmap_%03i", mapName.c_str(), tri->lightmapAtlas );
			tri->bakedLightmap = globalImages->ImageFromFile( imageName, TF_LINEAR, false, TR_CLAMP,
				TD_HIGH_QUALITY, CF_2D, true );
			if ( bakedAtlasImages.FindIndex( tri->bakedLightmap ) < 0 ) {
				bakedAtlasImages.Append( tri->bakedLightmap );
				tri->bakedLightmap->Reload( true, true );
			}
			imageName = va( "%s/deluxemap_%03i", mapName.c_str(), tri->lightmapAtlas );
			tri->bakedDeluxemap = globalImages->ImageFromFile( imageName, TF_LINEAR, false, TR_CLAMP,
				TD_HIGH_QUALITY, CF_2D, true );
			if ( bakedAtlasImages.FindIndex( tri->bakedDeluxemap ) < 0 ) {
				bakedAtlasImages.Append( tri->bakedDeluxemap );
				tri->bakedDeluxemap->Reload( true, true );
			}
			bakedSurfaceCount++;
		}

		R_AllocStaticTriSurfIndexes( tri, tri->numIndexes );
		for ( j = 0 ; j < tri->numIndexes ; j++ ) {
			tri->indexes[j] = src->ParseInt();
		}
		src->ExpectTokenString( "}" );

		parsedSurfaces.Append( surf );
	}

	src->ExpectTokenString( "}" );

	if ( hasBakedLightmaps ) {
		bakedBatchCount += R_AddBatchedModelSurfaces( model, parsedSurfaces );
	} else {
		for ( i = 0; i < parsedSurfaces.Num(); i++ ) {
			model->AddSurface( parsedSurfaces[i] );
		}
	}
	model->FinishSurfaces();

	return model;
}

/*
================
idRenderWorldLocal::ParseShadowModel
================
*/
idRenderModel *idRenderWorldLocal::ParseShadowModel( idLexer *src ) {
	idRenderModel	*model;
	idToken			token;
	int				j;
	srfTriangles_t	*tri;
	modelSurface_t	surf;

	src->ExpectTokenString( "{" );

	// parse the name
	src->ExpectAnyToken( &token );

	model = renderModelManager->AllocModel();
	model->InitEmpty( token );

	surf.shader = tr.defaultMaterial;

	tri = R_AllocStaticTriSurf();
	surf.geometry = tri;

	tri->numVerts = src->ParseInt();
	tri->numShadowIndexesNoCaps = src->ParseInt();
	tri->numShadowIndexesNoFrontCaps = src->ParseInt();
	tri->numIndexes = src->ParseInt();
	tri->shadowCapPlaneBits = src->ParseInt();

	R_AllocStaticTriSurfShadowVerts( tri, tri->numVerts );
	tri->bounds.Clear();
	for ( j = 0 ; j < tri->numVerts ; j++ ) {
		float	vec[8];

		src->Parse1DMatrix( 3, vec );
		tri->shadowVertexes[j].xyz[0] = vec[0];
		tri->shadowVertexes[j].xyz[1] = vec[1];
		tri->shadowVertexes[j].xyz[2] = vec[2];
		tri->shadowVertexes[j].xyz[3] = 1;		// no homogenous value

		tri->bounds.AddPoint( tri->shadowVertexes[j].xyz.ToVec3() );
	}

	R_AllocStaticTriSurfIndexes( tri, tri->numIndexes );
	for ( j = 0 ; j < tri->numIndexes ; j++ ) {
		tri->indexes[j] = src->ParseInt();
	}

	// add the completed surface to the model
	model->AddSurface( surf );

	src->ExpectTokenString( "}" );

	// we do NOT do a model->FinishSurfaceces, because we don't need sil edges, planes, tangents, etc.
//	model->FinishSurfaces();

	return model;
}

/*
================
idRenderWorldLocal::SetupAreaRefs
================
*/
void idRenderWorldLocal::SetupAreaRefs() {
	int		i;

	connectedAreaNum = 0;
	for ( i = 0 ; i < numPortalAreas ; i++ ) {
		portalAreas[i].areaNum = i;
		portalAreas[i].ambientCubeMap = defaultAmbientCubeMap;
		portalAreas[i].lightRefs.areaNext =
		portalAreas[i].lightRefs.areaPrev =
			&portalAreas[i].lightRefs;
		portalAreas[i].entityRefs.areaNext =
		portalAreas[i].entityRefs.areaPrev =
			&portalAreas[i].entityRefs;
	}
}

/*
=====================
idRenderWorldLocal::LoadAmbientCubeMaps

Loads directional ambient declarations and assigns light_ambient marker origins
to the compiled portal area that contains them.  ETQW used an ambient-specific
portal blocking bit; Doom 3 does not have that bit, so direct area assignment is
deterministic and avoids flooding an indoor cube through every open portal.
=====================
*/
void idRenderWorldLocal::LoadAmbientCubeMaps( const char *mapBaseName ) {
	struct ambientAssignment_t {
		idStr name;
		idVec3 origin;
	};

	idStr filename = mapBaseName;
	filename.SetFileExtension( "ambient" );
	fileSystem->ReadFile( filename, NULL, &ambientMapTimeStamp );
	defaultAmbientCubeMap = NULL;
	for ( int i = 0; i < numPortalAreas; i++ ) {
		portalAreas[i].ambientCubeMap = NULL;
	}

	idLexer src( filename, LEXFL_NOSTRINGCONCAT | LEXFL_NODOLLARPRECOMPILE );
	if ( !src.IsLoaded() ) {
		return;
	}

	idStr defaultName;
	idList<ambientAssignment_t> assignments;
	int parsedCubeMaps = 0;
	idToken token;
	while ( src.ReadToken( &token ) ) {
		if ( !token.Icmp( "ambientCubeMap" ) ) {
			idToken cubeName;
			if ( !src.ReadToken( &cubeName ) ) {
				src.Error( "expected ambient cube map name" );
				return;
			}
			// Older generated sidecars embedded the declaration text. The real
			// declaration now lives in atmosphere/*.atm, but consume the legacy
			// block so existing maps continue to load during migration.
			if ( src.CheckTokenString( "{" ) ) {
				src.SkipBracedSection( false );
			}
			idAmbientCubeMap *cube = R_FindAmbientCubeMap( cubeName, false );
			if ( cube ) {
				parsedCubeMaps++;
			} else {
				common->Warning( "%s references undeclared ambient cube map '%s'",
					filename.c_str(), cubeName.c_str() );
			}
			continue;
		}
		if ( !token.Icmp( "defaultAmbientCubeMap" ) ) {
			if ( !src.ReadToken( &token ) ) {
				src.Error( "expected default ambient cube map name" );
				return;
			}
			defaultName = token;
			continue;
		}
		if ( !token.Icmp( "areaAmbientCubeMap" ) ) {
			ambientAssignment_t assignment;
			assignment.origin.Zero();
			src.ExpectTokenString( "{" );
			while ( src.ReadToken( &token ) && token != "}" ) {
				if ( !token.Icmp( "name" ) ) {
					src.ReadToken( &token );
					assignment.name = token;
				} else if ( !token.Icmp( "origin" ) ) {
					src.Parse1DMatrix( 3, assignment.origin.ToFloatPtr() );
				} else {
					src.Error( "unknown areaAmbientCubeMap token '%s'", token.c_str() );
					return;
				}
			}
			assignments.Append( assignment );
			continue;
		}
		src.Error( "unknown ambient sidecar token '%s'", token.c_str() );
		return;
	}

	defaultAmbientCubeMap = R_FindAmbientCubeMap( defaultName, false );
	if ( !defaultAmbientCubeMap ) {
		common->Warning( "%s has no valid defaultAmbientCubeMap", filename.c_str() );
	}
	for ( int i = 0; i < numPortalAreas; i++ ) {
		portalAreas[i].ambientCubeMap = defaultAmbientCubeMap;
	}
	int assigned = 0;
	for ( int i = 0; i < assignments.Num(); i++ ) {
		const idAmbientCubeMap *cube = R_FindAmbientCubeMap( assignments[i].name, false );
		const int areaNum = PointInArea( assignments[i].origin );
		if ( !cube ) {
			common->Warning( "%s references unknown ambient cube map '%s'", filename.c_str(), assignments[i].name.c_str() );
			continue;
		}
		if ( areaNum < 0 || areaNum >= numPortalAreas ) {
			common->Warning( "%s ambient marker '%s' is outside the compiled world at %s",
				filename.c_str(), assignments[i].name.c_str(), assignments[i].origin.ToString() );
			continue;
		}
		portalAreas[areaNum].ambientCubeMap = cube;
		assigned++;
	}
	common->Printf( "Loaded %i ambient cube maps and assigned %i/%i area markers from %s\n",
		parsedCubeMaps, assigned, assignments.Num(), filename.c_str() );
}

const idAmbientCubeMap *idRenderWorldLocal::AmbientCubeMapForEntity( const idRenderEntityLocal *def ) const {
	if ( !def || !defaultAmbientCubeMap ) {
		return NULL;
	}
	if ( def->GetModel() && def->GetModel()->IsStaticWorldModel() && def->entityRefs && def->entityRefs->area ) {
		return def->entityRefs->area->ambientCubeMap;
	}
	const int areaNum = PointInArea( def->GetOrigin() );
	if ( areaNum >= 0 && areaNum < numPortalAreas ) {
		return portalAreas[areaNum].ambientCubeMap;
	}
	if ( def->entityRefs && def->entityRefs->area ) {
		return def->entityRefs->area->ambientCubeMap;
	}
	return defaultAmbientCubeMap;
}

/*
================
idRenderWorldLocal::ParseInterAreaPortals
================
*/
void idRenderWorldLocal::ParseInterAreaPortals( idLexer *src ) {
	int i, j;

	src->ExpectTokenString( "{" );

	numPortalAreas = src->ParseInt();
	if ( numPortalAreas < 0 ) {
		src->Error( "R_ParseInterAreaPortals: bad numPortalAreas" );
		return;
	}
	portalAreas = (portalArea_t *)R_ClearedStaticAlloc( numPortalAreas * sizeof( portalAreas[0] ) );
	areaScreenRect = (idScreenRect *) R_ClearedStaticAlloc( numPortalAreas * sizeof( idScreenRect ) );

	// set the doubly linked lists
	SetupAreaRefs();

	numInterAreaPortals = src->ParseInt();
	if ( numInterAreaPortals < 0 ) {
		src->Error(  "R_ParseInterAreaPortals: bad numInterAreaPortals" );
		return;
	}

	doublePortals = (doublePortal_t *)R_ClearedStaticAlloc( numInterAreaPortals * 
		sizeof( doublePortals [0] ) );

	for ( i = 0 ; i < numInterAreaPortals ; i++ ) {
		int		numPoints, a1, a2;
		idWinding	*w;
		portal_t	*p;

		numPoints = src->ParseInt();
		a1 = src->ParseInt();
		a2 = src->ParseInt();

		w = new idWinding( numPoints );
		w->SetNumPoints( numPoints );
		for ( j = 0 ; j < numPoints ; j++ ) {
			src->Parse1DMatrix( 3, (*w)[j].ToFloatPtr() );
			// no texture coordinates
			(*w)[j][3] = 0;
			(*w)[j][4] = 0;
		}

		// add the portal to a1
		p = (portal_t *)R_ClearedStaticAlloc( sizeof( *p ) );
		p->intoArea = a2;
		p->doublePortal = &doublePortals[i];
		p->w = w;
		p->w->GetPlane( p->plane );

		p->next = portalAreas[a1].portals;
		portalAreas[a1].portals = p;

		doublePortals[i].portals[0] = p;

		// reverse it for a2
		p = (portal_t *)R_ClearedStaticAlloc( sizeof( *p ) );
		p->intoArea = a1;
		p->doublePortal = &doublePortals[i];
		p->w = w->Reverse();
		p->w->GetPlane( p->plane );

		p->next = portalAreas[a2].portals;
		portalAreas[a2].portals = p;

		doublePortals[i].portals[1] = p;
	}

	src->ExpectTokenString( "}" );
}

/*
================
idRenderWorldLocal::ParseNodes
================
*/
void idRenderWorldLocal::ParseNodes( idLexer *src ) {
	int			i;

	src->ExpectTokenString( "{" );

	numAreaNodes = src->ParseInt();
	if ( numAreaNodes < 0 ) {
		src->Error( "R_ParseNodes: bad numAreaNodes" );
	}
	areaNodes = (areaNode_t *)R_ClearedStaticAlloc( numAreaNodes * sizeof( areaNodes[0] ) );

	for ( i = 0 ; i < numAreaNodes ; i++ ) {
		areaNode_t	*node;

		node = &areaNodes[i];

		src->Parse1DMatrix( 4, node->plane.ToFloatPtr() );
		node->children[0] = src->ParseInt();
		node->children[1] = src->ParseInt();
	}

	src->ExpectTokenString( "}" );
}

/*
================
idRenderWorldLocal::CommonChildrenArea_r
================
*/
int idRenderWorldLocal::CommonChildrenArea_r( areaNode_t *node ) {
	int	nums[2];

	for ( int i = 0 ; i < 2 ; i++ ) {
		if ( node->children[i] <= 0 ) {
			nums[i] = -1 - node->children[i];
		} else {
			nums[i] = CommonChildrenArea_r( &areaNodes[ node->children[i] ] );
		}
	}

	// solid nodes will match any area
	if ( nums[0] == AREANUM_SOLID ) {
		nums[0] = nums[1];
	}
	if ( nums[1] == AREANUM_SOLID ) {
		nums[1] = nums[0];
	}

	int	common;
	if ( nums[0] == nums[1] ) {
		common = nums[0];
	} else {
		common = CHILDREN_HAVE_MULTIPLE_AREAS;
	}

	node->commonChildrenArea = common;

	return common;
}

/*
=================
idRenderWorldLocal::ClearWorld

Sets up for a single area world
=================
*/
void idRenderWorldLocal::ClearWorld() {
	numPortalAreas = 1;
	portalAreas = (portalArea_t *)R_ClearedStaticAlloc( sizeof( portalAreas[0] ) );
	areaScreenRect = (idScreenRect *) R_ClearedStaticAlloc( sizeof( idScreenRect ) );

	SetupAreaRefs();

	// even though we only have a single area, create a node
	// that has both children pointing at it so we don't need to
	//
	areaNodes = (areaNode_t *)R_ClearedStaticAlloc( sizeof( areaNodes[0] ) );
	areaNodes[0].plane[3] = 1;
	areaNodes[0].children[0] = -1;
	areaNodes[0].children[1] = -1;
}

/*
=================
idRenderWorldLocal::FreeDefs
=================
*/
void idRenderWorldLocal::FreeDefs() {
	int		i;

	// free all lightDefs
	for ( i = 0 ; i < lightDefs.Num() ; i++ ) {
		idRenderLightLocal	*light;

		light = lightDefs[i];
		if ( light && light->world == this ) {
			FreeRenderLight( light );
			lightDefs[i] = NULL;
		}
	}

	// free all entityDefs
	for ( i = 0 ; i < entityDefs.Num() ; i++ ) {
		idRenderEntityLocal	*mod;

		mod = entityDefs[i];
		if ( mod && mod->world == this ) {
			FreeRenderEntity( mod );
			entityDefs[i] = NULL;
		}
	}
}

/*
=================
idRenderWorldLocal::InitFromMap

A NULL or empty name will make a world without a map model, which
is still useful for displaying a bare model
=================
*/
bool idRenderWorldLocal::InitFromMap( const char *name ) {
	idLexer *		src;
	idToken			token;
	idStr			filename;
	idStr			archiveName;
	idStr			ambientName;
	idRenderModel *	lastModel;

	// if this is an empty world, initialize manually
	if ( !name || !name[0] ) {
		FreeWorld();
		mapName.Clear();
		ClearWorld();
		return true;
	}


	// load it
	filename = name;
	filename.SetFileExtension( PROC_FILE_EXT );
	archiveName = name;
	archiveName.SetFileExtension( "zip" );
	ambientName = name;
	ambientName.SetFileExtension( "ambient" );

	// if we are reloading the same map, check the timestamp
	// and try to skip all the work
	ID_TIME_T currentTimeStamp;
	ID_TIME_T currentArchiveTimeStamp;
	ID_TIME_T currentAmbientTimeStamp;
	fileSystem->ReadFile( filename, NULL, &currentTimeStamp );
	fileSystem->ReadFile( archiveName, NULL, &currentArchiveTimeStamp );
	fileSystem->ReadFile( ambientName, NULL, &currentAmbientTimeStamp );

	if ( name == mapName ) {
		if ( currentTimeStamp != FILE_NOT_FOUND_TIMESTAMP && currentTimeStamp == mapTimeStamp &&
			currentArchiveTimeStamp == mapArchiveTimeStamp && currentAmbientTimeStamp == ambientMapTimeStamp ) {
			common->Printf( "idRenderWorldLocal::InitFromMap: retaining existing map\n" );
			FreeDefs();
			TouchWorldModels();
			AddWorldModelEntities();
			ClearPortalStates();
			return true;
		}
		common->Printf( "idRenderWorldLocal::InitFromMap: timestamp has changed, reloading.\n" );
	}

	FreeWorld();
	hasBakedLightmaps = fileSystem->MountMapArchive( archiveName );
	if ( hasBakedLightmaps ) {
		idStr manifestName = name;
		manifestName += "/lightmaps.manifest";
		void *manifestBuffer = NULL;
		const int manifestLength = fileSystem->ReadFile( manifestName, &manifestBuffer, NULL );
		bool validManifest = false;
		if ( manifestLength > 0 && manifestBuffer ) {
			const idStr manifestText( (const char *)manifestBuffer );
			validManifest = manifestText.Find( "lightmapArchiveVersion 3" ) >= 0 &&
				manifestText.Find( "atlasFormat DDS_DXT1" ) >= 0 &&
				manifestText.Find( "lightingComplete 1" ) >= 0 &&
				manifestText.Find( va( "procFileId %s", PROC_FILE_ID ) ) >= 0;
			// A zero-light archive only contains the low dmap sampling floor. It
			// is not a useful lighting result and makes custom ambient receivers,
			// such as MegaTexture terrain, appear black. Leave it unmounted so the
			// material uses its normal unbaked/realtime fallback until the map has
			// at least one light marked for baking.
			if ( validManifest && manifestText.Find( "numBakedLights 0" ) >= 0 ) {
				common->Warning( "Map lightmap archive %s contains no baked lights; using realtime lighting", archiveName.c_str() );
				validManifest = false;
			}
			if ( validManifest && !glConfig.textureCompressionAvailable ) {
				common->Warning( "Map lightmap archive %s requires DXT1 texture support; using realtime lights", archiveName.c_str() );
				validManifest = false;
			}
			fileSystem->FreeFile( manifestBuffer );
		}
		if ( !validManifest ) {
			common->Warning( "Map lightmap archive %s is missing a complete version 3 DDS/DXT1 manifest; using realtime lights", archiveName.c_str() );
			fileSystem->UnmountMapArchive();
			hasBakedLightmaps = false;
		}
	}

	src = new idLexer( filename, LEXFL_NOSTRINGCONCAT | LEXFL_NODOLLARPRECOMPILE );
	if ( !src->IsLoaded() ) {
		common->Printf( "idRenderWorldLocal::InitFromMap: %s not found\n", filename.c_str() );
		if ( hasBakedLightmaps ) {
			fileSystem->UnmountMapArchive();
			hasBakedLightmaps = false;
		}
		ClearWorld();
		return false;
	}


	mapName = name;
	mapTimeStamp = currentTimeStamp;
	mapArchiveTimeStamp = currentArchiveTimeStamp;
	ambientMapTimeStamp = currentAmbientTimeStamp;

	// if we are writing a demo, archive the load command
	if ( session->writeDemo ) {
		WriteLoadMap();
	}

	if ( !src->ReadToken( &token ) || ( token.Icmp( PROC_FILE_ID ) && token.Icmp( PROC_FILE_ID_LEGACY ) ) ) {
		common->Printf( "idRenderWorldLocal::InitFromMap: bad id '%s' instead of '%s'\n", token.c_str(), PROC_FILE_ID );
		delete src;
		if ( hasBakedLightmaps ) {
			fileSystem->UnmountMapArchive();
			hasBakedLightmaps = false;
		}
		return false;
	}
	const bool procHasLightmapUVs = ( token.Icmp( PROC_FILE_ID ) == 0 );
	if ( !procHasLightmapUVs && hasBakedLightmaps ) {
		fileSystem->UnmountMapArchive();
		hasBakedLightmaps = false;
	}
	if ( procHasLightmapUVs ) {
		common->Printf( "Loading %s with mapProcFile004 secondary lightmap UVs\n", name );
	}

	// parse the file
	while ( 1 ) {
		if ( !src->ReadToken( &token ) ) {
			break;
		}

		if ( token == "model" ) {
			lastModel = ParseModel( src, procHasLightmapUVs );

			// add it to the model manager list
			renderModelManager->AddModel( lastModel );

			// save it in the list to free when clearing this map
			localModels.Append( lastModel );
			continue;
		}

		if ( token == "shadowModel" ) {
			lastModel = ParseShadowModel( src );

			// add it to the model manager list
			renderModelManager->AddModel( lastModel );

			// save it in the list to free when clearing this map
			localModels.Append( lastModel );
			continue;
		}

		if ( token == "interAreaPortals" ) {
			ParseInterAreaPortals( src );
			continue;
		}

		if ( token == "nodes" ) {
			ParseNodes( src );
			continue;
		}

		src->Error( "idRenderWorldLocal::InitFromMap: bad token \"%s\"", token.c_str() );
	}

	delete src;

	// if it was a trivial map without any areas, create a single area
	if ( !numPortalAreas ) {
		ClearWorld();
	}

	// find the points where we can early-our of reference pushing into the BSP tree
	CommonChildrenArea_r( &areaNodes[0] );
	LoadAmbientCubeMaps( name );

	AddWorldModelEntities();
	ClearPortalStates();
	if ( procHasLightmapUVs ) {
		if ( hasBakedLightmaps && bakedBatchCount > 0 ) {
			common->Printf( "Baked map load: archive=mounted, atlas surfaces=%i, runtime batches=%i (%.1fx fewer surfaces)\n",
				bakedSurfaceCount, bakedBatchCount, (float)bakedSurfaceCount / bakedBatchCount );
		} else {
			common->Printf( "Baked map load: archive=%s, atlas surfaces=%i\n",
				hasBakedLightmaps ? "mounted" : "missing (realtime fallback)", bakedSurfaceCount );
		}
	}

	// done!
	return true;
}

/*
=====================
idRenderWorldLocal::ClearPortalStates
=====================
*/
void idRenderWorldLocal::ClearPortalStates() {
	int		i, j;

	// all portals start off open
	for ( i = 0 ; i < numInterAreaPortals ; i++ ) {
		doublePortals[i].blockingBits = PS_BLOCK_NONE;
	}

	// flood fill all area connections
	for ( i = 0 ; i < numPortalAreas ; i++ ) {
		for ( j = 0 ; j < NUM_PORTAL_ATTRIBUTES ; j++ ) {
			connectedAreaNum++;
			FloodConnectedAreas( &portalAreas[i], j );
		}
	}
}

/*
=====================
idRenderWorldLocal::AddWorldModelEntities
=====================
*/
void idRenderWorldLocal::AddWorldModelEntities() {
	int		i;

	// add the world model for each portal area
		// We create these directly because a regular renderer allocation would place the references
	// based on the bounding box, rather than explicitly into the correct area
	for ( i = 0 ; i < numPortalAreas ; i++ ) {
		idRenderEntityLocal	*def;
		int			index;

		def = new idRenderEntityLocal;

		// try and reuse a free spot
		index = entityDefs.FindNull();
		if ( index == -1 ) {
			index = entityDefs.Append(def);
		} else {
			entityDefs[index] = def;
		}

		def->index = index;
		def->world = this;

		def->SetModel( renderModelManager->FindModel( va("_area%i", i ) ) );
		if ( def->GetModel()->IsDefaultModel() || !def->GetModel()->IsStaticWorldModel() ) {
			common->Error( "idRenderWorldLocal::InitFromMap: bad area model lookup" );
		}

		idRenderModel *hModel = def->GetModel();

		for ( int j = 0; j < hModel->NumSurfaces(); j++ ) {
			const modelSurface_t *surf = hModel->Surface( j );

			if ( surf->shader->GetName() == idStr( "textures/smf/portal_sky" ) ) {
				def->needsPortalSky = true;
			}
		}

		def->referenceBounds = def->GetModel()->Bounds();

		def->SetAxis( mat3_identity );

		R_AxisToModelMatrix( def->GetAxis(), def->GetOrigin(), def->modelMatrix );

		// in case an explicit shader is used on the world, we don't
		// want it to have a 0 alpha or color
		for ( int j = 0; j < 4; j++ ) {
			def->SetShaderParm( j, 1.0f );
		}
		def->initialized = true;

		AddEntityRefToArea( def, &portalAreas[i] );
	}
}

/*
=====================
CheckAreaForPortalSky
=====================
*/
bool idRenderWorldLocal::CheckAreaForPortalSky( int areaNum ) {
	areaReference_t	*ref;

	assert( areaNum >= 0 && areaNum < numPortalAreas );

	for ( ref = portalAreas[areaNum].entityRefs.areaNext; ref->entity; ref = ref->areaNext ) {
		assert( ref->area == &portalAreas[areaNum] );

		if ( ref->entity && ref->entity->needsPortalSky ) {
			return true;
		}
	}

	return false;
}
