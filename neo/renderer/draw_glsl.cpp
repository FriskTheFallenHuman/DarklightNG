/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).  

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"

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
		idDrawVert	*ac = (idDrawVert *)vertexCache.Position( surf->geo->ambientCache );
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

	if ( !tri || !tri->ambientCache || !tri->lightmapCache || !tri->bakedLightmap || !tri->bakedDeluxemap ) {
		return;
	}
	if ( shader->Coverage() != MC_OPAQUE || !shader->ReceivesLighting() || !tri->numIndexes ) {
		return;
	}

	GL_Cull( shader->GetCullType() );
	// vertexCache.Position() binds the VBO that owns the returned offset.  Set
	// every idDrawVert pointer while the ambient VBO is bound, then switch to
	// the separate lightmap VBO and set only attribute 12.  Binding both caches
	// before defining the pointers made OpenGL interpret positions, normals and
	// base UVs as offsets into the two-float lightmap stream.
	idDrawVert *ambient = (idDrawVert *)vertexCache.Position( tri->ambientCache );
	qglColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( idDrawVert ), ambient->color );
	qglVertexAttribPointer( 11, 3, GL_FLOAT, false, sizeof( idDrawVert ), ambient->normal.ToFloatPtr() );
	qglVertexAttribPointer( 10, 3, GL_FLOAT, false, sizeof( idDrawVert ), ambient->tangents[1].ToFloatPtr() );
	qglVertexAttribPointer( 9, 3, GL_FLOAT, false, sizeof( idDrawVert ), ambient->tangents[0].ToFloatPtr() );
	qglVertexAttribPointer( 8, 2, GL_FLOAT, false, sizeof( idDrawVert ), ambient->st.ToFloatPtr() );
	qglVertexPointer( 3, GL_FLOAT, sizeof( idDrawVert ), ambient->xyz.ToFloatPtr() );
	idVec2 *lightmap = (idVec2 *)vertexCache.Position( tri->lightmapCache );
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
			if ( tri && tri->bakedLightmap && tri->bakedDeluxemap && tri->lightmapCache ) {
				visibleBakedSurfaces++;
			}
		}
		common->Printf( "Baked map draw active: %i/%i visible surfaces, %i atlas surfaces, %i/%i static lights using baked surfaces (%i moved)\n",
			visibleBakedSurfaces, numDrawSurfs, renderWorld->bakedSurfaceCount,
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

