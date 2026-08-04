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
#include "AmbientCubeMap.h"

/*
=========================================================================================

GENERAL INTERACTION RENDERING

=========================================================================================
*/

/*
====================
GL_SelectTextureNoClient
====================
*/
static void GL_SelectTextureNoClient( int unit ) {
	backEnd.glState.currenttmu = unit;
	qglActiveTextureARB( GL_TEXTURE0_ARB + unit );
	RB_LogComment( "glActiveTextureARB( %i )\n", unit );
}

/*
==================
RB_GLSL_DrawInteraction
==================
*/
void	RB_GLSL_DrawInteraction( const drawInteraction_t *din ) {
	// load all the vertex program parameters
	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, PP_LIGHT_ORIGIN, din->localLightOrigin.ToFloatPtr() );
	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, PP_VIEW_ORIGIN, din->localViewOrigin.ToFloatPtr() );
	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, PP_LIGHT_PROJECT_S, din->lightProjection[0].ToFloatPtr() );
	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, PP_LIGHT_PROJECT_T, din->lightProjection[1].ToFloatPtr() );
	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, PP_LIGHT_PROJECT_Q, din->lightProjection[2].ToFloatPtr() );
	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, PP_LIGHT_FALLOFF_S, din->lightProjection[3].ToFloatPtr() );
	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, PP_BUMP_MATRIX_S, din->bumpMatrix[0].ToFloatPtr() );
	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, PP_BUMP_MATRIX_T, din->bumpMatrix[1].ToFloatPtr() );
	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, PP_DIFFUSE_MATRIX_S, din->diffuseMatrix[0].ToFloatPtr() );
	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, PP_DIFFUSE_MATRIX_T, din->diffuseMatrix[1].ToFloatPtr() );
	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, PP_SPECULAR_MATRIX_S, din->specularMatrix[0].ToFloatPtr() );
	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, PP_SPECULAR_MATRIX_T, din->specularMatrix[1].ToFloatPtr() );

	// testing fragment based normal mapping
	if ( r_testGLSLProgram.GetBool() ) {
		R_SetGLSLProgramEnvParameter( GL_FRAGMENT_SHADER, 2, din->localLightOrigin.ToFloatPtr() );
		R_SetGLSLProgramEnvParameter( GL_FRAGMENT_SHADER, 3, din->localViewOrigin.ToFloatPtr() );
	}

	static const float zero[4] = { 0, 0, 0, 0 };
	static const float one[4] = { 1, 1, 1, 1 };
	static const float negOne[4] = { -1, -1, -1, -1 };

	switch ( din->vertexColor ) {
	case SVC_IGNORE:
		R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, PP_COLOR_MODULATE, zero );
		R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, PP_COLOR_ADD, one );
		break;
	case SVC_MODULATE:
		R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, PP_COLOR_MODULATE, one );
		R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, PP_COLOR_ADD, zero );
		break;
	case SVC_INVERSE_MODULATE:
		R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, PP_COLOR_MODULATE, negOne );
		R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, PP_COLOR_ADD, one );
		break;
	}

	// set the constant colors
	R_SetGLSLProgramEnvParameter( GL_FRAGMENT_SHADER, 0, din->diffuseColor.ToFloatPtr() );
	R_SetGLSLProgramEnvParameter( GL_FRAGMENT_SHADER, 1, din->specularColor.ToFloatPtr() );

	// set the textures

	// texture 1 will be the per-surface bump map
	GL_SelectTextureNoClient( 1 );
	din->bumpImage->Bind();

	// texture 2 will be the light falloff texture
	GL_SelectTextureNoClient( 2 );
	din->lightFalloffImage->Bind();

	// texture 3 will be the light projection texture
	GL_SelectTextureNoClient( 3 );
	din->lightImage->Bind();

	// texture 4 is the per-surface diffuse map
	GL_SelectTextureNoClient( 4 );
	din->diffuseImage->Bind();

	// texture 5 is the per-surface specular map
	GL_SelectTextureNoClient( 5 );
	din->specularImage->Bind();

	// draw it
	RB_DrawElementsWithCounters( din->surf->geo );
}


/*
=============
RB_GLSL_CreateDrawInteractions

=============
*/
void RB_GLSL_CreateDrawInteractions( const drawSurf_t *surf ) {
	if ( !surf ) {
		return;
	}

	// perform setup here that will be constant for all interactions
	GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHMASK | backEnd.depthFunc );

	if ( !R_BindGLSLProgram( r_testGLSLProgram.GetBool() ? GLSLPROG_TEST : GLSLPROG_INTERACTION ) ) {
		return;
	}

	// enable the vertex arrays
	qglEnableVertexAttribArray( 8 );
	qglEnableVertexAttribArray( 9 );
	qglEnableVertexAttribArray( 10 );
	qglEnableVertexAttribArray( 11 );
	qglEnableClientState( GL_COLOR_ARRAY );

	// texture 0 is the normalization cube map for the vector towards the light
	GL_SelectTextureNoClient( 0 );
	if ( backEnd.vLight->lightShader->IsAmbientLight() ) {
		globalImages->ambientNormalMap->Bind();
	} else {
		globalImages->normalCubeMapImage->Bind();
	}

	// texture 6 is the specular lookup table
	GL_SelectTextureNoClient( 6 );
	if ( r_testGLSLProgram.GetBool() ) {
		globalImages->specular2DTableImage->Bind();	// variable specularity in alpha channel
	} else {
		globalImages->specularTableImage->Bind();
	}


	for ( ; surf ; surf=surf->nextOnLight ) {
		// perform setup here that will not change over multiple interaction passes

		// set the vertex pointers
		const idDrawVert *ac = RB_BindDrawVertBuffer( surf->geo );
		qglColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( idDrawVert ), ac->color );
		qglVertexAttribPointer( 11, 3, GL_FLOAT, false, sizeof( idDrawVert ), ac->normal.ToFloatPtr() );
		qglVertexAttribPointer( 10, 3, GL_FLOAT, false, sizeof( idDrawVert ), ac->tangents[1].ToFloatPtr() );
		qglVertexAttribPointer( 9, 3, GL_FLOAT, false, sizeof( idDrawVert ), ac->tangents[0].ToFloatPtr() );
		qglVertexAttribPointer( 8, 2, GL_FLOAT, false, sizeof( idDrawVert ), ac->st.ToFloatPtr() );
		qglVertexPointer( 3, GL_FLOAT, sizeof( idDrawVert ), ac->xyz.ToFloatPtr() );

		// this may cause RB_GLSL_DrawInteraction to be executed multiple
		// times with different colors and images if the surface or light have multiple layers
		RB_CreateSingleDrawInteractions( surf, RB_GLSL_DrawInteraction );
	}

	qglDisableVertexAttribArray( 8 );
	qglDisableVertexAttribArray( 9 );
	qglDisableVertexAttribArray( 10 );
	qglDisableVertexAttribArray( 11 );
	qglDisableClientState( GL_COLOR_ARRAY );

	// disable features
	GL_SelectTextureNoClient( 6 );
	globalImages->BindNull();

	GL_SelectTextureNoClient( 5 );
	globalImages->BindNull();

	GL_SelectTextureNoClient( 4 );
	globalImages->BindNull();

	GL_SelectTextureNoClient( 3 );
	globalImages->BindNull();

	GL_SelectTextureNoClient( 2 );
	globalImages->BindNull();

	GL_SelectTextureNoClient( 1 );
	globalImages->BindNull();

	backEnd.glState.currenttmu = -1;
	GL_SelectTexture( 0 );

	R_UnbindGLSLProgram();
}


/*
==================
RB_GLSL_DrawInteractions
==================
*/
void RB_GLSL_DrawInteractions( void ) {
	viewLight_t		*vLight;

	GL_SelectTexture( 0 );
	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );

	// Add the realtime lights after the z-prepass and baked lighting pass.
	for ( vLight = backEnd.viewDef->viewLights ; vLight ; vLight = vLight->next ) {
		backEnd.vLight = vLight;

		// do fogging later
		if ( vLight->lightShader->IsFogLight() ) {
			continue;
		}
		if ( vLight->lightShader->IsBlendLight() ) {
			continue;
		}

		if ( !vLight->interactions && !vLight->translucentInteractions ) {
			continue;
		}

		RB_GLSL_CreateDrawInteractions( vLight->interactions );

		if ( r_skipTranslucent.GetBool() ) {
			continue;
		}

		backEnd.depthFunc = GLS_DEPTHFUNC_LESS;
		RB_GLSL_CreateDrawInteractions( vLight->translucentInteractions );

		backEnd.depthFunc = GLS_DEPTHFUNC_EQUAL;
	}

	GL_SelectTexture( 0 );
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
}

//===================================================================================

/*
==================
RB_GLSL_DrawBakedStage
==================
*/
static void RB_GLSL_DrawBakedStage( const drawSurf_t *surf, idImage *bumpImage, const idVec4 bumpMatrix[2],
		const shaderStage_t *stage ) {
	const srfTriangles_t *tri = surf->geo;
	idImage *stageImage;
	idVec4 stageMatrix[2];
	float stageColor[4];
	static const float zero[4] = { 0, 0, 0, 0 };
	static const float one[4] = { 1, 1, 1, 1 };
	static const float negativeOne[4] = { -1, -1, -1, -1 };
	static const float diffuseMode[4] = { 1, 0, 0, 0 };
	static const float specularMode[4] = { 0, 1, 0, 0 };

	R_SetDrawInteraction( stage, surf->shaderRegisters, &stageImage, stageMatrix, stageColor );
	if ( !stageImage || stageColor[0] + stageColor[1] + stageColor[2] <= 0.0f ) {
		return;
	}
	// Dmap stores baked irradiance using Doom 3's conventional r_lightScale=2
	// interaction range.  Unlike a realtime interaction, the atlas is already
	// in that expanded range when sampled here, so normalize it once before the
	// material diffuse/specular stages consume it.  Keep r_bakedLightmapScale as
	// a user-facing adjustment around the correctly decoded value of 1.0.
	const float bakedScale = 0.75f * r_bakedLightmapScale.GetFloat();
	stageColor[0] *= bakedScale;
	stageColor[1] *= bakedScale;
	stageColor[2] *= bakedScale;

	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 0, bumpMatrix[0].ToFloatPtr() );
	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 1, bumpMatrix[1].ToFloatPtr() );
	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 2, stageMatrix[0].ToFloatPtr() );
	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 3, stageMatrix[1].ToFloatPtr() );
	switch ( stage->vertexColor ) {
	case SVC_MODULATE:
		R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 4, one );
		R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 5, zero );
		break;
	case SVC_INVERSE_MODULATE:
		R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 4, negativeOne );
		R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 5, one );
		break;
	default:
		R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 4, zero );
		R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 5, one );
		break;
	}
	R_SetGLSLProgramEnvParameter( GL_FRAGMENT_SHADER, 0, stageColor );
	R_SetGLSLProgramEnvParameter( GL_FRAGMENT_SHADER, 1,
		stage->lighting == SL_SPECULAR ? specularMode : diffuseMode );

	GL_SelectTextureNoClient( 0 );
	( r_skipBump.GetBool() ? globalImages->flatNormalMap : bumpImage )->Bind();
	GL_SelectTextureNoClient( 1 );
	stageImage->Bind();
	GL_SelectTextureNoClient( 2 );
	tri->bakedLightmap->Bind();
	GL_SelectTextureNoClient( 3 );
	tri->bakedDeluxemap->Bind();
	GL_SelectTextureNoClient( 4 );
	globalImages->specularTableImage->Bind();

	RB_DrawElementsWithCounters( tri );
}

/*
==================
RB_GLSL_DrawBakedSurface
==================
*/
static void RB_GLSL_DrawBakedSurface( const drawSurf_t *surf ) {
	const srfTriangles_t *tri = surf->geo;
	const idMaterial *shader = surf->material;
	const float *regs = surf->shaderRegisters;
	idImage *bumpImage = globalImages->flatNormalMap;
	idVec4 bumpMatrix[2];

	if ( !tri || !tri->vertexBuffer || !tri->lightmapBuffer || !tri->bakedLightmap || !tri->bakedDeluxemap ) {
		return;
	}
	if ( shader->Coverage() != MC_OPAQUE || !shader->ReceivesLighting() || !tri->numIndexes ) {
		return;
	}

	GL_Cull( shader->GetCullType() );
	// Bind the VBO that owns each stream. Set
	// every idDrawVert pointer while the ambient VBO is bound, then switch to
	// the separate lightmap VBO and set only attribute 12.  Binding both caches
	// before defining the pointers made OpenGL interpret positions, normals and
	// base UVs as offsets into the two-float lightmap stream.
	const idDrawVert *ambient = RB_BindDrawVertBuffer( tri );
	qglColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( idDrawVert ), ambient->color );
	qglVertexAttribPointer( 11, 3, GL_FLOAT, false, sizeof( idDrawVert ), ambient->normal.ToFloatPtr() );
	qglVertexAttribPointer( 10, 3, GL_FLOAT, false, sizeof( idDrawVert ), ambient->tangents[1].ToFloatPtr() );
	qglVertexAttribPointer( 9, 3, GL_FLOAT, false, sizeof( idDrawVert ), ambient->tangents[0].ToFloatPtr() );
	qglVertexAttribPointer( 8, 2, GL_FLOAT, false, sizeof( idDrawVert ), ambient->st.ToFloatPtr() );
	qglVertexPointer( 3, GL_FLOAT, sizeof( idDrawVert ), ambient->xyz.ToFloatPtr() );
	const idVec2 *lightmap = RB_BindLightmapBuffer( tri );
	qglVertexAttribPointer( 12, 2, GL_FLOAT, false, sizeof( idVec2 ), lightmap->ToFloatPtr() );
	idVec3 localViewOrigin;
	R_GlobalPointToLocal( surf->space->modelMatrix, backEnd.viewDef->renderView.vieworg, localViewOrigin );
	idVec4 viewOrigin( localViewOrigin[0], localViewOrigin[1], localViewOrigin[2], 1.0f );
	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 6, viewOrigin.ToFloatPtr() );

	bumpMatrix[0].Set( 1, 0, 0, 0 );
	bumpMatrix[1].Set( 0, 1, 0, 0 );
	for ( int stageNum = 0; stageNum < shader->GetNumStages(); stageNum++ ) {
		const shaderStage_t *stage = shader->GetStage( stageNum );
		if ( regs[stage->conditionRegister] == 0.0f ) {
			continue;
		}
		if ( stage->lighting == SL_BUMP ) {
			R_SetDrawInteraction( stage, regs, &bumpImage, bumpMatrix, NULL );
			continue;
		}
		if ( stage->lighting == SL_DIFFUSE && !r_skipDiffuse.GetBool() ) {
			RB_GLSL_DrawBakedStage( surf, bumpImage, bumpMatrix, stage );
			continue;
		}
		if ( stage->lighting == SL_SPECULAR && !r_skipSpecular.GetBool() ) {
			RB_GLSL_DrawBakedStage( surf, bumpImage, bumpMatrix, stage );
		}
	}
}

/*
==================
RB_GLSL_DrawBakedLightmaps
==================
*/
void RB_GLSL_DrawBakedLightmaps( drawSurf_t **drawSurfs, int numDrawSurfs ) {
	if ( r_skipBakedLightmaps.GetBool() || !backEnd.viewDef->viewEntitys ) {
		return;
	}

	RB_LogComment( "---------- RB_GLSL_DrawBakedLightmaps ----------\n" );
	GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHMASK | GLS_DEPTHFUNC_EQUAL );
	if ( !R_BindGLSLProgram( GLSLPROG_BAKED_LIGHT ) ) {
		return;
	}
	qglEnableVertexAttribArray( 8 );
	qglEnableVertexAttribArray( 9 );
	qglEnableVertexAttribArray( 10 );
	qglEnableVertexAttribArray( 11 );
	qglEnableVertexAttribArray( 12 );
	qglEnableClientState( GL_COLOR_ARRAY );

	idRenderWorldLocal *renderWorld = backEnd.viewDef->renderWorld;
	if ( renderWorld && !renderWorld->bakedDrawReported ) {
		int visibleBakedSurfaces = 0;
		for ( int i = 0; i < numDrawSurfs; i++ ) {
			const srfTriangles_t *tri = drawSurfs[i]->geo;
			if ( tri && tri->bakedLightmap && tri->bakedDeluxemap && tri->lightmapBuffer ) {
				visibleBakedSurfaces++;
			}
		}
		common->Printf( "Baked map draw active: %i/%i visible batches, %i runtime batches from %i atlas surfaces, %i/%i static lights using baked surfaces (%i moved)\n",
			visibleBakedSurfaces, numDrawSurfs, renderWorld->bakedBatchCount, renderWorld->bakedSurfaceCount,
			renderWorld->bakedLightSuppressionCount, renderWorld->bakedLightCandidateCount, renderWorld->bakedLightMovedCount );
		renderWorld->bakedDrawReported = true;
	}

	RB_RenderDrawSurfListWithFunction( drawSurfs, numDrawSurfs, RB_GLSL_DrawBakedSurface );

	qglDisableClientState( GL_COLOR_ARRAY );
	qglDisableVertexAttribArray( 12 );
	qglDisableVertexAttribArray( 11 );
	qglDisableVertexAttribArray( 10 );
	qglDisableVertexAttribArray( 9 );
	qglDisableVertexAttribArray( 8 );
	for ( int unit = 4; unit >= 0; unit-- ) {
		GL_SelectTextureNoClient( unit );
		globalImages->BindNull();
	}
	backEnd.glState.currenttmu = -1;
	GL_SelectTexture( 0 );
	R_UnbindGLSLProgram();
}

//===================================================================================

static int ambientCubeStagesDrawn;
static int ambientCubeStagesAttempted;
static int ambientCubeStagesMissingImage;
static int ambientCubeStagesBlackColor;
static int ambientCubeLightingStageCounts[4];

/*
==================
RB_GLSL_DrawAmbientCubeStage
==================
*/
static void RB_GLSL_DrawAmbientCubeStage( const drawSurf_t *surf, idImage *bumpImage,
		const idVec4 bumpMatrix[2], const shaderStage_t *stage, const idAmbientCubeMap *cube ) {
	idImage *stageImage;
	idVec4 stageMatrix[2];
	float stageColor[4];
	static const float zero[4] = { 0, 0, 0, 0 };
	static const float one[4] = { 1, 1, 1, 1 };
	static const float negativeOne[4] = { -1, -1, -1, -1 };
	static const float diffuseMode[4] = { 1, 0, 0, 0 };
	static const float specularMode[4] = { 0, 1, 0, 0 };

	R_SetDrawInteraction( stage, surf->shaderRegisters, &stageImage, stageMatrix, stageColor );
	ambientCubeStagesAttempted++;
	if ( !stageImage ) {
		ambientCubeStagesMissingImage++;
		return;
	}
	if ( stageColor[0] + stageColor[1] + stageColor[2] <= 0.0f ) {
		ambientCubeStagesBlackColor++;
		return;
	}

	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 0, bumpMatrix[0].ToFloatPtr() );
	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 1, bumpMatrix[1].ToFloatPtr() );
	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 2, stageMatrix[0].ToFloatPtr() );
	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 3, stageMatrix[1].ToFloatPtr() );
	switch ( stage->vertexColor ) {
	case SVC_MODULATE:
		R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 4, one );
		R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 5, zero );
		break;
	case SVC_INVERSE_MODULATE:
		R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 4, negativeOne );
		R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 5, one );
		break;
	default:
		R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 4, zero );
		R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 5, one );
		break;
	}
	R_SetGLSLProgramEnvParameter( GL_FRAGMENT_SHADER, 0, stageColor );
	R_SetGLSLProgramEnvParameter( GL_FRAGMENT_SHADER, 1,
		stage->lighting == SL_SPECULAR ? specularMode : diffuseMode );
	idVec4 lightingScale( cube->GetBrightness(), r_ambientCubeMapScale.GetFloat(), 0.0f, 0.0f );
	R_SetGLSLProgramEnvParameter( GL_FRAGMENT_SHADER, 2, lightingScale.ToFloatPtr() );

	GL_SelectTextureNoClient( 0 );
	( stage->lighting == SL_SPECULAR ? cube->GetSpecularCubeMap() : cube->GetAmbientCubeMap() )->Bind();
	GL_SelectTextureNoClient( 1 );
	( r_skipBump.GetBool() ? globalImages->flatNormalMap : bumpImage )->Bind();
	GL_SelectTextureNoClient( 2 );
	stageImage->Bind();

	RB_DrawElementsWithCounters( surf->geo );
	ambientCubeStagesDrawn++;
}

/*
==================
RB_GLSL_DrawAmbientCubeSurface
==================
*/
static void RB_GLSL_DrawAmbientCubeSurface( const drawSurf_t *surf ) {
	const srfTriangles_t *tri = surf->geo;
	const idMaterial *shader = surf->material;
	const idRenderWorldLocal *renderWorld = backEnd.viewDef->renderWorld;
	const idAmbientCubeMap *cube = surf->space->ambientCubeMap;
	// View weapons and other depth-hacked models can be created without a
	// stable portal reference.  They still belong to the active atmosphere.
	if ( !cube && renderWorld ) {
		cube = renderWorld->defaultAmbientCubeMap;
	}
	if ( !tri || !tri->numIndexes || !tri->verts && !tri->vertexBuffer &&
		( !tri->ambientSurface || !tri->ambientSurface->vertexBuffer ) ) {
		return;
	}
	if ( !cube || !cube->GetAmbientCubeMap() || !cube->GetSpecularCubeMap() ) {
		return;
	}
	// World BSP already contains directional baked lighting.  Ambient cubes
	// fill imported meshes, characters and other non-atlased objects only.
	if ( tri->bakedLightmap && tri->bakedDeluxemap ) {
		return;
	}
	if ( ( shader->Coverage() != MC_OPAQUE && shader->Coverage() != MC_PERFORATED ) ||
		!shader->ReceivesLighting() || shader->IsPortalSky() ) {
		return;
	}

	GL_Cull( shader->GetCullType() );
	const idDrawVert *ambient = RB_BindDrawVertBuffer( tri );
	// A resident VBO deliberately returns offset zero here.  It is a valid
	// attribute pointer while GL_ARRAY_BUFFER is bound, not a missing stream.
	qglColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( idDrawVert ), ambient->color );
	qglVertexAttribPointer( 11, 3, GL_FLOAT, false, sizeof( idDrawVert ), ambient->normal.ToFloatPtr() );
	qglVertexAttribPointer( 10, 3, GL_FLOAT, false, sizeof( idDrawVert ), ambient->tangents[1].ToFloatPtr() );
	qglVertexAttribPointer( 9, 3, GL_FLOAT, false, sizeof( idDrawVert ), ambient->tangents[0].ToFloatPtr() );
	qglVertexAttribPointer( 8, 2, GL_FLOAT, false, sizeof( idDrawVert ), ambient->st.ToFloatPtr() );
	qglVertexPointer( 3, GL_FLOAT, sizeof( idDrawVert ), ambient->xyz.ToFloatPtr() );

	idVec3 localViewOrigin;
	R_GlobalPointToLocal( surf->space->modelMatrix, backEnd.viewDef->renderView.vieworg, localViewOrigin );
	idVec4 parm( localViewOrigin.x, localViewOrigin.y, localViewOrigin.z, 1.0f );
	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 6, parm.ToFloatPtr() );
	parm.Set( surf->space->modelMatrix[0], surf->space->modelMatrix[4], surf->space->modelMatrix[8], 0.0f );
	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 7, parm.ToFloatPtr() );
	parm.Set( surf->space->modelMatrix[1], surf->space->modelMatrix[5], surf->space->modelMatrix[9], 0.0f );
	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 8, parm.ToFloatPtr() );
	parm.Set( surf->space->modelMatrix[2], surf->space->modelMatrix[6], surf->space->modelMatrix[10], 0.0f );
	R_SetGLSLProgramEnvParameter( GL_VERTEX_SHADER, 9, parm.ToFloatPtr() );

	idImage *bumpImage = globalImages->flatNormalMap;
	idVec4 bumpMatrix[2];
	bumpMatrix[0].Set( 1, 0, 0, 0 );
	bumpMatrix[1].Set( 0, 1, 0, 0 );
	for ( int stageNum = 0; stageNum < shader->GetNumStages(); stageNum++ ) {
		const shaderStage_t *stage = shader->GetStage( stageNum );
		if ( stage->lighting >= SL_AMBIENT && stage->lighting <= SL_SPECULAR ) {
			ambientCubeLightingStageCounts[stage->lighting]++;
		}
		if ( surf->shaderRegisters[stage->conditionRegister] == 0.0f ) {
			continue;
		}
		if ( stage->lighting == SL_BUMP ) {
			R_SetDrawInteraction( stage, surf->shaderRegisters, &bumpImage, bumpMatrix, NULL );
			continue;
		}
		if ( stage->lighting == SL_DIFFUSE && !r_skipDiffuse.GetBool() ) {
			RB_GLSL_DrawAmbientCubeStage( surf, bumpImage, bumpMatrix, stage, cube );
		} else if ( stage->lighting == SL_SPECULAR && !r_skipSpecular.GetBool() ) {
			RB_GLSL_DrawAmbientCubeStage( surf, bumpImage, bumpMatrix, stage, cube );
		}
	}
}

/*
==================
RB_GLSL_DrawAmbientCubeMaps
==================
*/
void RB_GLSL_DrawAmbientCubeMaps( drawSurf_t **drawSurfs, int numDrawSurfs ) {
	if ( r_skipAmbientCubeMaps.GetBool() || !backEnd.viewDef->viewEntitys ) {
		return;
	}
	GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHMASK | GLS_DEPTHFUNC_EQUAL );
	if ( !R_BindGLSLProgram( GLSLPROG_AMBIENT_CUBE ) ) {
		return;
	}
	RB_LogComment( "---------- RB_GLSL_DrawAmbientCubeMaps ----------\n" );
	qglEnableVertexAttribArray( 8 );
	qglEnableVertexAttribArray( 9 );
	qglEnableVertexAttribArray( 10 );
	qglEnableVertexAttribArray( 11 );
	qglEnableClientState( GL_COLOR_ARRAY );
	ambientCubeStagesDrawn = 0;
	ambientCubeStagesAttempted = 0;
	ambientCubeStagesMissingImage = 0;
	ambientCubeStagesBlackColor = 0;
	memset( ambientCubeLightingStageCounts, 0, sizeof( ambientCubeLightingStageCounts ) );

	idRenderWorldLocal *renderWorld = backEnd.viewDef->renderWorld;
	const bool reportThisDraw = renderWorld && !renderWorld->ambientCubeDrawReported;
	if ( reportThisDraw ) {
		int eligible = 0;
		int assigned = 0;
		int perforated = 0;
		for ( int i = 0; i < numDrawSurfs; i++ ) {
			const drawSurf_t *surf = drawSurfs[i];
			const srfTriangles_t *tri = surf->geo;
			const idMaterial *shader = surf->material;
			if ( !tri || ( tri->bakedLightmap && tri->bakedDeluxemap ) || !shader->ReceivesLighting() ||
				shader->IsPortalSky() || ( shader->Coverage() != MC_OPAQUE && shader->Coverage() != MC_PERFORATED ) ) {
				continue;
			}
			eligible++;
			perforated += shader->Coverage() == MC_PERFORATED ? 1 : 0;
			if ( surf->space->ambientCubeMap || renderWorld->defaultAmbientCubeMap ) {
				assigned++;
			}
		}
		common->Printf( "Ambient cube draw active: %i/%i eligible surfaces assigned (%i perforated), default=%s\n",
			assigned, eligible, perforated, renderWorld->defaultAmbientCubeMap ?
			renderWorld->defaultAmbientCubeMap->GetName() : "<none>" );
	}

	RB_RenderDrawSurfListWithFunction( drawSurfs, numDrawSurfs, RB_GLSL_DrawAmbientCubeSurface );
	if ( reportThisDraw ) {
		common->Printf( "Ambient cube stages: draw=%i attempted=%i missingImage=%i blackColor=%i, lighting A/B/D/S=%i/%i/%i/%i, scale=%.2f\n",
			ambientCubeStagesDrawn, ambientCubeStagesAttempted, ambientCubeStagesMissingImage,
			ambientCubeStagesBlackColor, ambientCubeLightingStageCounts[SL_AMBIENT],
			ambientCubeLightingStageCounts[SL_BUMP], ambientCubeLightingStageCounts[SL_DIFFUSE],
			ambientCubeLightingStageCounts[SL_SPECULAR], r_ambientCubeMapScale.GetFloat() );
		renderWorld->ambientCubeDrawReported = true;
	}

	qglDisableClientState( GL_COLOR_ARRAY );
	qglDisableVertexAttribArray( 11 );
	qglDisableVertexAttribArray( 10 );
	qglDisableVertexAttribArray( 9 );
	qglDisableVertexAttribArray( 8 );
	for ( int unit = 2; unit >= 0; unit-- ) {
		GL_SelectTextureNoClient( unit );
		globalImages->BindNull();
	}
	backEnd.glState.currenttmu = -1;
	GL_SelectTexture( 0 );
	R_UnbindGLSLProgram();
}

