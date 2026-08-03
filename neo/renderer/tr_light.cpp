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

static const float CHECK_BOUNDS_EPSILON = 1.0f;


/*
===========================================================================================

VERTEX CACHE GENERATORS

===========================================================================================
*/

/*
==================
R_CreateAmbientCache

Create it if needed
==================
*/
bool R_CreateAmbientCache( srfTriangles_t *tri, bool needsLighting ) {
	if ( tri->isParticle ) {
		return true;
	}
	const bool skinningReady = !tri->gpuSkinned || ( tri->skinningBuffer && tri->jointBuffer );
	if ( tri->vertexBuffer && tri->indexBuffer && ( !tri->lightmapTexCoords || tri->lightmapBuffer ) && skinningReady ) {
		return true;
	}
	// we are going to use it for drawing, so make sure we have the tangents and normals
	if ( needsLighting && !tri->tangentsCalculated ) {
		R_DeriveTangents( tri );
	}

	if ( !tri->vertexBuffer && tri->verts && tri->numVerts > 0 ) {
		tri->vertexBuffer = new idVertexBuffer;
		if ( !tri->vertexBuffer->AllocBufferObject( tri->verts, tri->numVerts * sizeof( tri->verts[0] ) ) ) {
			delete tri->vertexBuffer;
			tri->vertexBuffer = NULL;
			return false;
		}
	}
	if ( !tri->vertexBuffer ) {
		return false;
	}
	if ( !tri->indexBuffer && tri->indexes && tri->numIndexes > 0 ) {
		tri->indexBuffer = new idIndexBuffer;
		if ( !tri->indexBuffer->AllocBufferObject( tri->indexes, tri->numIndexes * sizeof( tri->indexes[0] ) ) ) {
			delete tri->indexBuffer;
			tri->indexBuffer = NULL;
			return false;
		}
	}
	if ( tri->lightmapTexCoords && !tri->lightmapBuffer ) {
		tri->lightmapBuffer = new idVertexBuffer;
		if ( !tri->lightmapBuffer->AllocBufferObject( tri->lightmapTexCoords, tri->numVerts * sizeof( tri->lightmapTexCoords[0] ) ) ) {
			delete tri->lightmapBuffer;
			tri->lightmapBuffer = NULL;
			return false;
		}
	}
	if ( tri->gpuSkinned ) {
		if ( !glConfig.gpuSkinningAvailable || !tri->skinningVerts || !tri->jointMatrices || tri->numJoints <= 0 ) {
			common->Error( "R_CreateAmbientCache: GPU skeletal surface has no valid skinning data" );
		}
		if ( !tri->skinningBuffer ) {
			tri->skinningBuffer = new idVertexBuffer;
			if ( !tri->skinningBuffer->AllocBufferObject( tri->skinningVerts,
				tri->numVerts * sizeof( tri->skinningVerts[0] ) ) ) {
				common->Error( "R_CreateAmbientCache: failed to recreate skinning vertex buffer" );
			}
		}
		if ( !tri->jointBuffer ) {
			tri->jointBuffer = new idJointBuffer;
			if ( !tri->jointBuffer->AllocBufferObject( tri->jointMatrices[0].ToFloatPtr(), tri->numJoints ) ) {
				common->Error( "R_CreateAmbientCache: failed to recreate joint uniform buffer" );
			}
		}
	}
	return true;
}

/*
==================
R_SkyboxTexGen
==================
*/
void R_SkyboxTexGen( drawSurf_t *surf, const idVec3 &viewOrg ) {
	int		i;
	idVec3	localViewOrigin;

	R_GlobalPointToLocal( surf->space->modelMatrix, viewOrg, localViewOrigin );

	int numVerts = surf->geo->numVerts;
	int size = numVerts * sizeof( idVec3 );
	idVec3 *texCoords = (idVec3 *)R_FrameAlloc( size );

	const idDrawVert *verts = surf->geo->verts;
	for ( i = 0; i < numVerts; i++ ) {
		texCoords[i][0] = verts[i].xyz[0] - localViewOrigin[0];
		texCoords[i][1] = verts[i].xyz[1] - localViewOrigin[1];
		texCoords[i][2] = verts[i].xyz[2] - localViewOrigin[2];
	}

	surf->dynamicTexCoords = texCoords;
}

/*
==================
R_WobbleskyTexGen
==================
*/
void R_WobbleskyTexGen( drawSurf_t *surf, const idVec3 &viewOrg ) {
	int		i;
	idVec3	localViewOrigin;

	const int *parms = surf->material->GetTexGenRegisters();

	float	wobbleDegrees = surf->shaderRegisters[ parms[0] ];
	float	wobbleSpeed = surf->shaderRegisters[ parms[1] ];
	float	rotateSpeed = surf->shaderRegisters[ parms[2] ];

	wobbleDegrees = wobbleDegrees * idMath::PI / 180;
	wobbleSpeed = wobbleSpeed * 2 * idMath::PI / 60;
	rotateSpeed = rotateSpeed * 2 * idMath::PI / 60;

	// very ad-hoc "wobble" transform
	float	transform[16];
	float	a = tr.viewDef->floatTime * wobbleSpeed;
	float	s = sin( a ) * sin( wobbleDegrees );
	float	c = cos( a ) * sin( wobbleDegrees );
	float	z = cos( wobbleDegrees );

	idVec3	axis[3];

	axis[2][0] = c;
	axis[2][1] = s;
	axis[2][2] = z;

	axis[1][0] = -sin( a * 2 ) * sin( wobbleDegrees );
	axis[1][2] = -s * sin( wobbleDegrees );
	axis[1][1] = sqrt( 1.0f - ( axis[1][0] * axis[1][0] + axis[1][2] * axis[1][2] ) );

	// make the second vector exactly perpendicular to the first
	axis[1] -= ( axis[2] * axis[1] ) * axis[2];
	axis[1].Normalize();

	// construct the third with a cross
	axis[0].Cross( axis[1], axis[2] );

	// add the rotate
	s = sin( rotateSpeed * tr.viewDef->floatTime );
	c = cos( rotateSpeed * tr.viewDef->floatTime );

	transform[0] = axis[0][0] * c + axis[1][0] * s;
	transform[4] = axis[0][1] * c + axis[1][1] * s;
	transform[8] = axis[0][2] * c + axis[1][2] * s;

	transform[1] = axis[1][0] * c - axis[0][0] * s;
	transform[5] = axis[1][1] * c - axis[0][1] * s;
	transform[9] = axis[1][2] * c - axis[0][2] * s;

	transform[2] = axis[2][0];
	transform[6] = axis[2][1];
	transform[10] = axis[2][2];

	transform[3] = transform[7] = transform[11] = 0.0f;
	transform[12] = transform[13] = transform[14] = 0.0f;

	R_GlobalPointToLocal( surf->space->modelMatrix, viewOrg, localViewOrigin );

	int numVerts = surf->geo->numVerts;
	int size = numVerts * sizeof( idVec3 );
	idVec3 *texCoords = (idVec3 *)R_FrameAlloc( size );

	const idDrawVert *verts = surf->geo->verts;
	for ( i = 0; i < numVerts; i++ ) {
		idVec3 v;

		v[0] = verts[i].xyz[0] - localViewOrigin[0];
		v[1] = verts[i].xyz[1] - localViewOrigin[1];
		v[2] = verts[i].xyz[2] - localViewOrigin[2];

		R_LocalPointToGlobal( transform, v, texCoords[i] );
	}

	surf->dynamicTexCoords = texCoords;
}

/*
=================
R_SpecularTexGen

Calculates the specular coordinates for cards without vertex programs.
=================
*/
static void R_SpecularTexGen( drawSurf_t *surf, const idVec3 &globalLightOrigin, const idVec3 &viewOrg ) {
	const srfTriangles_t *tri;
	idVec3	localLightOrigin;
	idVec3	localViewOrigin;

	R_GlobalPointToLocal( surf->space->modelMatrix, globalLightOrigin, localLightOrigin );
	R_GlobalPointToLocal( surf->space->modelMatrix, viewOrg, localViewOrigin );

	tri = surf->geo;

	// FIXME: change to 3 component?
	int	size = tri->numVerts * sizeof( idVec4 );
	idVec4 *texCoords = (idVec4 *)R_FrameAlloc( size );

#if 1

	SIMDProcessor->CreateSpecularTextureCoords( texCoords, localLightOrigin, localViewOrigin,
											tri->verts, tri->numVerts, tri->indexes, tri->numIndexes );

#else

	bool *used = (bool *)_alloca16( tri->numVerts * sizeof( used[0] ) );
	memset( used, 0, tri->numVerts * sizeof( used[0] ) );

	// because a realtime light may use a small subset of the full surface,
	// it makes sense to only deal with the verts used
	for ( int j = 0; j < tri->numIndexes; j++ ) {
		int i = tri->indexes[j];
		if ( used[i] ) {
			continue;
		}
		used[i] = true;

		float ilength;

		const idDrawVert *v = &tri->verts[i];

		idVec3 lightDir = localLightOrigin - v->xyz;
		idVec3 viewDir = localViewOrigin - v->xyz;

		ilength = idMath::RSqrt( lightDir * lightDir );
		lightDir[0] *= ilength;
		lightDir[1] *= ilength;
		lightDir[2] *= ilength;

		ilength = idMath::RSqrt( viewDir * viewDir );
		viewDir[0] *= ilength;
		viewDir[1] *= ilength;
		viewDir[2] *= ilength;

		lightDir += viewDir;

		texCoords[i][0] = lightDir * v->tangents[0];
		texCoords[i][1] = lightDir * v->tangents[1];
		texCoords[i][2] = lightDir * v->normal;
		texCoords[i][3] = 1;
	}

#endif

	surf->dynamicTexCoords = texCoords;
}


//=======================================================================================================

/*
=============
R_SetEntityDefViewEntity

If the entityDef isn't already on the viewEntity list, create
a viewEntity and add it to the list with an empty scissor rect.

This does not instantiate dynamic models for the entity yet.
=============
*/
viewEntity_t *R_SetEntityDefViewEntity( idRenderEntityLocal *def ) {
	viewEntity_t		*vModel;

	if ( def->viewCount == tr.viewCount ) {
		return def->viewEntity;
	}
	def->viewCount = tr.viewCount;

	// set the model and modelview matricies
	vModel = (viewEntity_t *)R_ClearedFrameAlloc( sizeof( *vModel ) );
	vModel->entityDef = def;

	// the scissorRect will be expanded as the model bounds is accepted into visible portal chains
	vModel->scissorRect.Clear();

	// copy the model and weapon depth hack for back-end use
	vModel->modelDepthHack = def->GetModelDepthHack();
	vModel->weaponDepthHack = def->GetWeaponDepthHack();

	R_AxisToModelMatrix( def->GetAxis(), def->GetOrigin(), vModel->modelMatrix );

	myGlMultMatrix( vModel->modelMatrix, tr.viewDef->worldSpace.modelViewMatrix, vModel->modelViewMatrix );
	vModel->next = tr.viewDef->viewEntitys;
	tr.viewDef->viewEntitys = vModel;

	def->viewEntity = vModel;

	return vModel;
}

/*
=============
R_SetLightDefViewLight

If the lightDef isn't already on the viewLight list, create
a viewLight and add it to the list with an empty scissor rect.
=============
*/
viewLight_t *R_SetLightDefViewLight( idRenderLightLocal *light ) {
	viewLight_t *vLight;

	if ( light->viewCount == tr.viewCount ) {
		return light->viewLight;
	}
	light->viewCount = tr.viewCount;

	// add to the view light chain
	vLight = (viewLight_t *)R_ClearedFrameAlloc( sizeof( *vLight ) );
	vLight->lightDef = light;

	// the scissorRect will be expanded as the light bounds is accepted into visible portal chains
	vLight->scissorRect.Clear();

	// copy data used by backend
	vLight->globalLightOrigin = light->globalLightOrigin;
	vLight->lightProject[0] = light->lightProject[0];
	vLight->lightProject[1] = light->lightProject[1];
	vLight->lightProject[2] = light->lightProject[2];
	vLight->lightProject[3] = light->lightProject[3];
	vLight->fogPlane = light->frustum[5];
	vLight->frustumTris = light->frustumTris;
	vLight->falloffImage = light->falloffImage;
	vLight->lightShader = light->lightShader;
	vLight->shaderRegisters = NULL;		// allocated and evaluated in R_AddLightSurfaces

	// link the view light
	vLight->next = tr.viewDef->viewLights;
	tr.viewDef->viewLights = vLight;

	light->viewLight = vLight;

	return vLight;
}

/*
=================
R_LinkLightSurf
=================
*/
void R_LinkLightSurf( const drawSurf_t **link, const srfTriangles_t *tri, const viewEntity_t *space, 
				   const idMaterial *shader, const idScreenRect &scissor ) {
	drawSurf_t		*drawSurf;

	if ( !space ) {
		space = &tr.viewDef->worldSpace;
	}

	drawSurf = (drawSurf_t *)R_FrameAlloc( sizeof( *drawSurf ) );

	drawSurf->geo = tri;
	drawSurf->space = space;
	drawSurf->material = shader;
	drawSurf->scissorRect = scissor;

	if ( !shader ) {
		drawSurf->shaderRegisters = NULL;
	} else {
		// process the shader expressions for conditionals / color / texcoords
		const float *constRegs = shader->ConstantRegisters();
		if ( constRegs ) {
			// this shader has only constants for parameters
			drawSurf->shaderRegisters = constRegs;
		} else {
			// FIXME: share with the ambient surface?
			float *regs = (float *)R_FrameAlloc( shader->GetNumRegisters() * sizeof( float ) );
			drawSurf->shaderRegisters = regs;
			shader->EvaluateRegisters( regs, space->entityDef->GetShaderParms(), tr.viewDef, space->entityDef->GetReferenceSound() );
		}
	}

	// actually link it in
	drawSurf->nextOnLight = *link;
	*link = drawSurf;
}

/*
======================
R_ClippedLightScissorRectangle
======================
*/
idScreenRect R_ClippedLightScissorRectangle( viewLight_t *vLight ) {
	int i, j;
	const idRenderLightLocal *light = vLight->lightDef;
	idScreenRect r;
	idFixedWinding w;

	r.Clear();

	for ( i = 0 ; i < 6 ; i++ ) {
		const idWinding *ow = light->frustumWindings[i];

		// projected lights may have one of the frustums degenerated
		if ( !ow ) {
			continue;
		}

		// the light frustum planes face out from the light,
		// so the planes that have the view origin on the negative
		// side will be the "back" faces of the light, which must have
		// some fragment inside the portalStack to be visible
		if ( light->frustum[i].Distance( tr.viewDef->renderView.vieworg ) >= 0 ) {
			continue;
		}

		w = *ow;

		// now check the winding against each of the frustum planes
		for ( j = 0; j < 5; j++ ) {
			if ( !w.ClipInPlace( -tr.viewDef->frustum[j] ) ) {
				break;
			}
		}

		// project these points to the screen and add to bounds
		for ( j = 0; j < w.GetNumPoints(); j++ ) {
			idPlane		eye, clip;
			idVec3		ndc;

			R_TransformModelToClip( w[j].ToVec3(), tr.viewDef->worldSpace.modelViewMatrix, tr.viewDef->projectionMatrix, eye, clip );

			if ( clip[3] <= 0.01f ) {
				clip[3] = 0.01f;
			}

			R_TransformClipToDevice( clip, tr.viewDef, ndc );

			float windowX = 0.5f * ( 1.0f + ndc[0] ) * ( tr.viewDef->viewport.x2 - tr.viewDef->viewport.x1 );
			float windowY = 0.5f * ( 1.0f + ndc[1] ) * ( tr.viewDef->viewport.y2 - tr.viewDef->viewport.y1 );

			if ( windowX > tr.viewDef->scissor.x2 ) {
				windowX = tr.viewDef->scissor.x2;
			} else if ( windowX < tr.viewDef->scissor.x1 ) {
				windowX = tr.viewDef->scissor.x1;
			}
			if ( windowY > tr.viewDef->scissor.y2 ) {
				windowY = tr.viewDef->scissor.y2;
			} else if ( windowY < tr.viewDef->scissor.y1 ) {
				windowY = tr.viewDef->scissor.y1;
			}

			r.AddPoint( windowX, windowY );
		}
	}

	// add the fudge boundary
	r.Expand();

	return r;
}

/*
==================
R_CalcLightScissorRectangle

The light screen bounds crop realtime lighting work.
==================
*/
int	c_clippedLight, c_unclippedLight;

idScreenRect	R_CalcLightScissorRectangle( viewLight_t *vLight ) {
	idScreenRect	r;
	srfTriangles_t *tri;
	idPlane			eye, clip;
	idVec3			ndc;

	if ( vLight->lightDef->GetPointLight() ) {
		idBounds bounds;
		idRenderLightLocal *lightDef = vLight->lightDef;
		tr.viewDef->viewFrustum.ProjectionBounds( idBox( lightDef->GetOrigin(), lightDef->GetLightRadius(), lightDef->GetAxis() ), bounds );
		return R_ScreenRectFromViewFrustumBounds( bounds );
	}

	if ( r_useClippedLightScissors.GetInteger() == 2 ) {
		return R_ClippedLightScissorRectangle( vLight );
	}

	r.Clear();

	tri = vLight->lightDef->frustumTris;
	for ( int i = 0 ; i < tri->numVerts ; i++ ) {
		R_TransformModelToClip( tri->verts[i].xyz, tr.viewDef->worldSpace.modelViewMatrix,
			tr.viewDef->projectionMatrix, eye, clip );

		// if it is near clipped, clip the winding polygons to the view frustum
		if ( clip[3] <= 1 ) {
			c_clippedLight++;
			if ( r_useClippedLightScissors.GetInteger() ) {
				return R_ClippedLightScissorRectangle( vLight );
			} else {
				r.x1 = r.y1 = 0;
				r.x2 = ( tr.viewDef->viewport.x2 - tr.viewDef->viewport.x1 ) - 1;
				r.y2 = ( tr.viewDef->viewport.y2 - tr.viewDef->viewport.y1 ) - 1;
				return r;
			}
		}

		R_TransformClipToDevice( clip, tr.viewDef, ndc );

		float windowX = 0.5f * ( 1.0f + ndc[0] ) * ( tr.viewDef->viewport.x2 - tr.viewDef->viewport.x1 );
		float windowY = 0.5f * ( 1.0f + ndc[1] ) * ( tr.viewDef->viewport.y2 - tr.viewDef->viewport.y1 );

		if ( windowX > tr.viewDef->scissor.x2 ) {
			windowX = tr.viewDef->scissor.x2;
		} else if ( windowX < tr.viewDef->scissor.x1 ) {
			windowX = tr.viewDef->scissor.x1;
		}
		if ( windowY > tr.viewDef->scissor.y2 ) {
			windowY = tr.viewDef->scissor.y2;
		} else if ( windowY < tr.viewDef->scissor.y1 ) {
			windowY = tr.viewDef->scissor.y1;
		}

		r.AddPoint( windowX, windowY );
	}

	// add the fudge boundary
	r.Expand();

	c_unclippedLight++;

	return r;
}

/*
=================
R_AddLightSurfaces

Calc the light shader values, removing any light from the viewLight list
if it is determined to not have any visible effect due to being flashed off or turned off.

Removes lights from the viewLights list if they are completely
turned off, or completely off screen.

Realtime light surfaces are generated later while walking visible entities.
=================
*/
void R_AddLightSurfaces( void ) {
	viewLight_t		*vLight;
	idRenderLightLocal *light;
	viewLight_t		**ptr;

	// go through each visible light, possibly removing some from the list
	ptr = &tr.viewDef->viewLights;
	while ( *ptr ) {
		vLight = *ptr;
		light = vLight->lightDef;

		const idMaterial	*lightShader = light->lightShader;
		if ( !lightShader ) {
			common->Error( "R_AddLightSurfaces: NULL lightShader" );
		}

		// see if we are suppressing the light in this view
		if ( !r_skipSuppress.GetBool() ) {
			if ( light->GetSuppressLightInViewID()
			&& light->GetSuppressLightInViewID() == tr.viewDef->renderView.viewID ) {
				*ptr = vLight->next;
				light->viewCount = -1;
				continue;
			}
			if ( light->GetAllowLightInViewID()
			&& light->GetAllowLightInViewID() != tr.viewDef->renderView.viewID ) {
				*ptr = vLight->next;
				light->viewCount = -1;
				continue;
			}
		}

		// evaluate the light shader registers
		float *lightRegs =(float *)R_FrameAlloc( lightShader->GetNumRegisters() * sizeof( float ) );
		vLight->shaderRegisters = lightRegs;
		lightShader->EvaluateRegisters( lightRegs, light->GetShaderParms(), tr.viewDef, light->GetReferenceSound() );

		// if this is a purely additive light and no stage in the light shader evaluates
		// to a positive light value, we can completely skip the light
		if ( !lightShader->IsFogLight() && !lightShader->IsBlendLight() ) {
			int lightStageNum;
			for ( lightStageNum = 0 ; lightStageNum < lightShader->GetNumStages() ; lightStageNum++ ) {
				const shaderStage_t	*lightStage = lightShader->GetStage( lightStageNum );

				// ignore stages that fail the condition
				if ( !lightRegs[ lightStage->conditionRegister ] ) {
					continue;
				}

				const int *registers = lightStage->color.registers;

				// snap tiny values to zero to avoid lights showing up with the wrong color
				if ( lightRegs[ registers[0] ] < 0.001f ) {
					lightRegs[ registers[0] ] = 0.0f;
				}
				if ( lightRegs[ registers[1] ] < 0.001f ) {
					lightRegs[ registers[1] ] = 0.0f;
				}
				if ( lightRegs[ registers[2] ] < 0.001f ) {
					lightRegs[ registers[2] ] = 0.0f;
				}

				// FIXME:	when using the following values the light shows up bright red when using nvidia drivers/hardware
				//			this seems to have been fixed ?
				//lightRegs[ registers[0] ] = 1.5143074e-005f;
				//lightRegs[ registers[1] ] = 1.5483369e-005f;
				//lightRegs[ registers[2] ] = 1.7014690e-005f;

				if ( lightRegs[ registers[0] ] > 0.0f ||
						lightRegs[ registers[1] ] > 0.0f ||
							lightRegs[ registers[2] ] > 0.0f ) {
					break;
				}
			}
			if ( lightStageNum == lightShader->GetNumStages() ) {
				// we went through all the stages and didn't find one that adds anything
				// remove the light from the viewLights list, and change its frame marker
				// so transient light generation does not treat it as visible
				*ptr = vLight->next;
				light->viewCount = -1;
				continue;
			}
		}

		if ( r_useLightScissors.GetBool() ) {
			// calculate the screen area covered by the light frustum
			// which will be used to crop the stencil cull
			idScreenRect scissorRect = R_CalcLightScissorRectangle( vLight );
			// intersect with the portal crossing scissor rectangle
			vLight->scissorRect.Intersect( scissorRect );

			if ( r_showLightScissors.GetBool() ) {
				R_ShowColoredScreenRect( vLight->scissorRect, light->index );
			}
		}

#if 0
		// this never happens, because CullLightByPortals() does a more precise job
		if ( vLight->scissorRect.IsEmpty() ) {
			// this light doesn't touch anything on screen, so remove it from the list
			*ptr = vLight->next;
			continue;
		}
#endif

		// this one stays on the list
		ptr = &vLight->next;

		tr.pc.c_viewLights++;

		// fog lights will need to draw the light frustum triangles, so make sure they
		// are in the vertex cache
		if ( lightShader->IsFogLight() ) {
			if ( !light->frustumTris->vertexBuffer ) {
				if ( !R_CreateAmbientCache( light->frustumTris, false ) ) {
					// skip if we are out of vertex memory
					continue;
				}
			}
			// touch the surface so it won't get purged
		}

	}
}

//===============================================================================================================

/*
==================
R_IssueEntityDefCallback
==================
*/
bool R_IssueEntityDefCallback( idRenderEntityLocal *def ) {
	bool update;
	idBounds	oldBounds;

	if ( r_checkBounds.GetBool() ) {
		oldBounds = def->referenceBounds;
	}

	def->archived = false;		// will need to be written to the demo file
	tr.pc.c_entityDefCallbacks++;
	if ( tr.viewDef ) {
		update = def->GetCallback()( def, &tr.viewDef->renderView );
	} else {
		update = def->GetCallback()( def, NULL );
	}

	if ( !def->GetModel() ) {
		common->Error( "R_IssueEntityDefCallback: dynamic entity callback didn't set model" );
	}

	if ( r_checkBounds.GetBool() ) {
		if (	oldBounds[0][0] > def->referenceBounds[0][0] + CHECK_BOUNDS_EPSILON ||
				oldBounds[0][1] > def->referenceBounds[0][1] + CHECK_BOUNDS_EPSILON ||
				oldBounds[0][2] > def->referenceBounds[0][2] + CHECK_BOUNDS_EPSILON ||
				oldBounds[1][0] < def->referenceBounds[1][0] - CHECK_BOUNDS_EPSILON ||
				oldBounds[1][1] < def->referenceBounds[1][1] - CHECK_BOUNDS_EPSILON ||
				oldBounds[1][2] < def->referenceBounds[1][2] - CHECK_BOUNDS_EPSILON ) {
			common->Printf( "entity %i callback extended reference bounds\n", def->index );
		}
	}

	return update;
}

/*
===================
R_EntityDefDynamicModel

Issues a deferred entity callback if necessary.
If the model isn't dynamic, it returns the original.
Returns the cached dynamic model if present, otherwise creates
it and any necessary overlays
===================
*/
idRenderModel *R_EntityDefDynamicModel( idRenderEntityLocal *def ) {
	bool callbackUpdate;

	// allow deferred entities to construct themselves
	if ( def->GetCallback() ) {
		callbackUpdate = R_IssueEntityDefCallback( def );
	} else {
		callbackUpdate = false;
	}

	idRenderModel *model = def->GetModel();

	if ( !model ) {
		common->Error( "R_EntityDefDynamicModel: NULL model" );
	}

	if ( model->IsDynamicModel() == DM_STATIC ) {
		def->dynamicModel = NULL;
		def->dynamicModelFrameCount = 0;
		return model;
	}

	// continously animating models (particle systems, etc) will have their snapshot updated every single view
	if ( callbackUpdate || ( model->IsDynamicModel() == DM_CONTINUOUS && def->dynamicModelFrameCount != tr.frameCount ) ) {
		R_ClearEntityDefDynamicModel( def );
	}

	// if we don't have a snapshot of the dynamic model, generate it now
	if ( !def->dynamicModel ) {

		// instantiate the snapshot of the dynamic model, possibly reusing memory from the cached snapshot
		def->cachedDynamicModel = model->InstantiateDynamicModel( def, tr.viewDef, def->cachedDynamicModel );

		if ( def->cachedDynamicModel ) {

			// add any overlays to the snapshot of the dynamic model
			if ( def->overlay && !r_skipOverlays.GetBool() ) {
				def->overlay->AddOverlaySurfacesToModel( def->cachedDynamicModel );
			} else {
				idRenderModelOverlay::RemoveOverlaySurfacesFromModel( def->cachedDynamicModel );
			}

			if ( r_checkBounds.GetBool() ) {
				idBounds b = def->cachedDynamicModel->Bounds();
				if (	b[0][0] < def->referenceBounds[0][0] - CHECK_BOUNDS_EPSILON ||
						b[0][1] < def->referenceBounds[0][1] - CHECK_BOUNDS_EPSILON ||
						b[0][2] < def->referenceBounds[0][2] - CHECK_BOUNDS_EPSILON ||
						b[1][0] > def->referenceBounds[1][0] + CHECK_BOUNDS_EPSILON ||
						b[1][1] > def->referenceBounds[1][1] + CHECK_BOUNDS_EPSILON ||
						b[1][2] > def->referenceBounds[1][2] + CHECK_BOUNDS_EPSILON ) {
					common->Printf( "entity %i dynamic model exceeded reference bounds\n", def->index );
				}
			}
		}

		def->dynamicModel = def->cachedDynamicModel;
		def->dynamicModelFrameCount = tr.frameCount;
	}

	// set model depth hack value
	if ( def->dynamicModel && model->DepthHack() != 0.0f && tr.viewDef ) {
		idPlane eye, clip;
		idVec3 ndc;
		R_TransformModelToClip( def->GetOrigin(), tr.viewDef->worldSpace.modelViewMatrix, tr.viewDef->projectionMatrix, eye, clip );
		R_TransformClipToDevice( clip, tr.viewDef, ndc );
		def->SetModelDepthHack( model->DepthHack() * ( 1.0f - ndc.z ) );
	}

	// FIXME: if any of the surfaces have deforms, create a frame-temporary model with references to the
	// undeformed surfaces.  This would allow deforms to be light interacting.

	return def->dynamicModel;
}

/*
=================
R_AddDrawSurf
=================
*/
void R_AddDrawSurf( const srfTriangles_t *tri, const viewEntity_t *space, const idRenderEntity *renderEntity,
					const idMaterial *shader, const idScreenRect &scissor ) {
	drawSurf_t		*drawSurf;
	const float		*shaderParms;
	static float	refRegs[MAX_EXPRESSION_REGISTERS];	// don't put on stack, or VC++ will do a page touch
	float			generatedShaderParms[MAX_ENTITY_SHADER_PARMS];

	drawSurf = (drawSurf_t *)R_FrameAlloc( sizeof( *drawSurf ) );
	drawSurf->geo = tri;
	drawSurf->space = space;
	drawSurf->material = shader;
	drawSurf->scissorRect = scissor;
	drawSurf->sort = shader->GetSort() + tr.sortOffset;

	// bumping this offset each time causes surfaces with equal sort orders to still
	// deterministically draw in the order they are added
	tr.sortOffset += 0.000001f;

	// if it doesn't fit, resize the list
	if ( tr.viewDef->numDrawSurfs == tr.viewDef->maxDrawSurfs ) {
		drawSurf_t	**old = tr.viewDef->drawSurfs;
		int			count;

		if ( tr.viewDef->maxDrawSurfs == 0 ) {
			tr.viewDef->maxDrawSurfs = INITIAL_DRAWSURFS;
			count = 0;
		} else {
			count = tr.viewDef->maxDrawSurfs * sizeof( tr.viewDef->drawSurfs[0] );
			tr.viewDef->maxDrawSurfs *= 2;
		}
		tr.viewDef->drawSurfs = (drawSurf_t **)R_FrameAlloc( tr.viewDef->maxDrawSurfs * sizeof( tr.viewDef->drawSurfs[0] ) );
		memcpy( tr.viewDef->drawSurfs, old, count );
	}
	tr.viewDef->drawSurfs[tr.viewDef->numDrawSurfs] = drawSurf;
	tr.viewDef->numDrawSurfs++;

	// process the shader expressions for conditionals / color / texcoords
	const float	*constRegs = shader->ConstantRegisters();
	if ( constRegs ) {
		// shader only uses constant values
		drawSurf->shaderRegisters = constRegs;
	} else {
		float *regs = (float *)R_FrameAlloc( shader->GetNumRegisters() * sizeof( float ) );
		drawSurf->shaderRegisters = regs;

		// a reference shader will take the calculated stage color value from another shader
		// and use that for the parm0-parm3 of the current shader, which allows a stage of
		// a light model and light flares to pick up different flashing tables from
		// different light shaders
		if ( renderEntity->GetReferenceShader() ) {
			// evaluate the reference shader to find our shader parms
			const shaderStage_t *pStage;

			renderEntity->GetReferenceShader()->EvaluateRegisters( refRegs, renderEntity->GetShaderParms(), tr.viewDef, renderEntity->GetReferenceSound() );
			pStage = renderEntity->GetReferenceShader()->GetStage(0);

			memcpy( generatedShaderParms, renderEntity->GetShaderParms(), sizeof( generatedShaderParms ) );
			generatedShaderParms[0] = refRegs[ pStage->color.registers[0] ];
			generatedShaderParms[1] = refRegs[ pStage->color.registers[1] ];
			generatedShaderParms[2] = refRegs[ pStage->color.registers[2] ];

			shaderParms = generatedShaderParms;
		} else {
			// evaluate with the entityDef's shader parms
			shaderParms = renderEntity->GetShaderParms();
		}

		float oldFloatTime;
		int oldTime;

		if ( space->entityDef && space->entityDef->GetTimeGroup() ) {
			oldFloatTime = tr.viewDef->floatTime;
			oldTime = tr.viewDef->renderView.time;

			tr.viewDef->floatTime = game->GetTimeGroupTime( space->entityDef->GetTimeGroup() ) * 0.001;
			tr.viewDef->renderView.time = game->GetTimeGroupTime( space->entityDef->GetTimeGroup() );
		}

		shader->EvaluateRegisters( regs, shaderParms, tr.viewDef, renderEntity->GetReferenceSound() );

		if ( space->entityDef && space->entityDef->GetTimeGroup() ) {
			tr.viewDef->floatTime = oldFloatTime;
			tr.viewDef->renderView.time = oldTime;
		}
	}

	// check for deformations
	R_DeformDrawSurf( drawSurf );

	// skybox surfaces need a dynamic texgen
	switch( shader->Texgen() ) {
		case TG_SKYBOX_CUBE:
			R_SkyboxTexGen( drawSurf, tr.viewDef->renderView.vieworg );
			break;
		case TG_WOBBLESKY_CUBE:
			R_WobbleskyTexGen( drawSurf, tr.viewDef->renderView.vieworg );
			break;
	}

	// check for gui surfaces
	idUserInterface	*gui = NULL;

	if ( !space->entityDef ) {
		gui = shader->GlobalGui();
	} else {
		int guiNum = shader->GetEntityGui() - 1;
		if ( guiNum >= 0 && guiNum < MAX_RENDERENTITY_GUI ) {
			gui = renderEntity->GetGui( guiNum );
		}
		if ( gui == NULL ) {
			gui = shader->GlobalGui();
		}
	}

	if ( gui ) {
		// force guis on the fast time
		float oldFloatTime;
		int oldTime;

		oldFloatTime = tr.viewDef->floatTime;
		oldTime = tr.viewDef->renderView.time;

		tr.viewDef->floatTime = game->GetTimeGroupTime( 1 ) * 0.001;
		tr.viewDef->renderView.time = game->GetTimeGroupTime( 1 );

		idBounds ndcBounds;

		if ( !R_PreciseCullSurface( drawSurf, ndcBounds ) ) {
			// did we ever use this to forward an entity color to a gui that didn't set color?
//			memcpy( tr.guiShaderParms, shaderParms, sizeof( tr.guiShaderParms ) );
			R_RenderGuiSurf( gui, drawSurf );
		}

		tr.viewDef->floatTime = oldFloatTime;
		tr.viewDef->renderView.time = oldTime;
	}

	// we can't add subviews at this point, because that would
	// increment tr.viewCount, messing up the rest of the surface
	// adds for this view
}

/*
===============
R_AddAmbientDrawsurfs

Adds surfaces for the given viewEntity
Walks through the viewEntitys list and creates drawSurf_t for each surface of
each viewEntity that has a non-empty scissorRect
===============
*/
static void R_AddAmbientDrawsurfs( viewEntity_t *vEntity ) {
	int					i, total;
	idRenderEntityLocal	*def;
	srfTriangles_t		*tri;
	idRenderModel		*model;
	const idMaterial	*shader;

	def = vEntity->entityDef;

	if ( def->dynamicModel ) {
		model = def->dynamicModel;
	} else {
		model = def->GetModel();
	}

	// add all the surfaces
	total = model->NumSurfaces();
	for ( i = 0 ; i < total ; i++ ) {
		const modelSurface_t	*surf = model->Surface( i );

		// for debugging, only show a single surface at a time
		if ( r_singleSurface.GetInteger() >= 0 && i != r_singleSurface.GetInteger() ) {
			continue;
		}

		tri = surf->geometry;
		if ( !tri ) {
			continue;
		}
		if ( !tri->numIndexes ) {
			continue;
		}
		shader = surf->shader;
		shader = R_RemapShaderBySkin( shader, def->GetCustomSkin(), def->GetCustomShader() );

		R_GlobalShaderOverride( &shader );

		if ( !shader ) {	
			continue;
		}
		if ( !shader->IsDrawn() ) {
			continue;
		}

		// debugging tool to make sure we are have the correct pre-calculated bounds
		if ( r_checkBounds.GetBool() ) {
			int j, k;
			for ( j = 0 ; j < tri->numVerts ; j++ ) {
				for ( k = 0 ; k < 3 ; k++ ) {
					if ( tri->verts[j].xyz[k] > tri->bounds[1][k] + CHECK_BOUNDS_EPSILON
						|| tri->verts[j].xyz[k] < tri->bounds[0][k] - CHECK_BOUNDS_EPSILON ) {
						common->Printf( "bad tri->bounds on %s:%s\n", def->GetModel()->Name(), shader->GetName() );
						break;
					}
					if ( tri->verts[j].xyz[k] > def->referenceBounds[1][k] + CHECK_BOUNDS_EPSILON
						|| tri->verts[j].xyz[k] < def->referenceBounds[0][k] - CHECK_BOUNDS_EPSILON ) {
						common->Printf( "bad referenceBounds on %s:%s\n", def->GetModel()->Name(), shader->GetName() );
						break;
					}
				}
				if ( k != 3 ) {
					break;
				}
			}
		}

		if ( !R_CullLocalBox( tri->bounds, vEntity->modelMatrix, 5, tr.viewDef->frustum ) ) {

			def->visibleCount = tr.viewCount;

			// make sure we have an ambient cache
			if ( !R_CreateAmbientCache( tri, shader->ReceivesLighting() ) ) {
				// don't add anything if the vertex cache was too full to give us an ambient cache
				return;
			}
			// add the surface for drawing
			R_AddDrawSurf( tri, vEntity, vEntity->entityDef, shader, vEntity->scissorRect );

			// ambientViewCount lets transient realtime lighting reject surfaces
			// if the ambient surface isn't visible at all
			tri->ambientViewCount = tr.viewCount;
		}
	}

	// add the lightweight decal surfaces
	for ( idRenderModelDecal *decal = def->decals; decal; decal = decal->Next() ) {
		decal->AddDecalDrawSurf( vEntity );
	}
}

/*
====================
R_CullLocalLightBounds

Tests local-space bounds against inward-facing local light frustum planes.
Transforming the six planes once per entity/light pair avoids transforming
every bounds corner for the model and each of its surfaces.
====================
*/
static bool R_CullLocalLightBounds( const idBounds &bounds, const idPlane localClipPlanes[6] ) {
	const int cullMode = r_useCulling.GetInteger();
	if ( cullMode == 0 ) {
		return false;
	}

	if ( cullMode == 1 ) {
		const idVec3 center = bounds.GetCenter();
		const float radius = ( bounds[0] - center ).Length();
		for ( int i = 0; i < 6; i++ ) {
			if ( localClipPlanes[i].Distance( center ) < -radius ) {
				return true;
			}
		}
		return false;
	}

	for ( int i = 0; i < 6; i++ ) {
		const idPlane &plane = localClipPlanes[i];
		const idVec3 nearestInsideCorner(
			plane[0] >= 0.0f ? bounds[1][0] : bounds[0][0],
			plane[1] >= 0.0f ? bounds[1][1] : bounds[0][1],
			plane[2] >= 0.0f ? bounds[1][2] : bounds[0][2] );
		if ( plane.Distance( nearestInsideCorner ) <= 0.0f ) {
			tr.pc.c_box_cull_out++;
			return true;
		}
	}

	tr.pc.c_box_cull_in++;
	return false;
}

/*
====================
R_CreateTransientLightTris

Builds a frame-local subset of a visible surface for one realtime light.
Nothing is retained on the entity or light after the view is submitted.
====================
*/
static srfTriangles_t *R_CreateTransientLightTris( const idRenderEntityLocal *entityDef,
		const srfTriangles_t *tri, const idRenderLightLocal *lightDef, const idMaterial *shader,
		const idPlane localClipPlanes[6], const idVec3 &localLightOrigin ) {
	static const float LIGHT_CLIP_EPSILON = 0.1f;
	int frontBits = 0;

	for ( int i = 0; i < 6; i++ ) {
		if ( tri->bounds.PlaneDistance( localClipPlanes[i] ) >= LIGHT_CLIP_EPSILON ) {
			frontBits |= 1 << i;
		}
	}

	// Bind-pose vertices deliberately are not CPU-deformed. Animated bounds are
	// still exact, but per-vertex and per-face light clipping must stay on the
	// conservative path for GPU-skinned surfaces.
	const bool allInside = tri->gpuSkinned || frontBits == ( ( 1 << 6 ) - 1 );
	const bool includeBackFaces = tri->gpuSkinned || r_lightAllBackFaces.GetBool() ||
		lightDef->lightShader->LightEffectsBackSides() || shader->ReceivesLightingOnBackSides() ||
		entityDef->GetNoSelfShadow() || entityDef->GetNoShadow();

	byte *facing = NULL;
	if ( !includeBackFaces ) {
		const int numFaces = tri->numIndexes / 3;
		if ( !tri->facePlanes || !tri->facePlanesCalculated ) {
			R_DeriveFacePlanes( const_cast<srfTriangles_t *>( tri ) );
		}

		facing = (byte *)_alloca16( numFaces * sizeof( facing[0] ) );
		float *planeSide = (float *)_alloca16( numFaces * sizeof( planeSide[0] ) );
		SIMDProcessor->Dot( planeSide, localLightOrigin, tri->facePlanes, numFaces );
		SIMDProcessor->CmpGE( facing, planeSide, 0.0f, numFaces );
	}

	if ( allInside && includeBackFaces ) {
		srfTriangles_t *lightTris = (srfTriangles_t *)R_ClearedFrameAlloc( sizeof( *lightTris ) );
		lightTris->ambientSurface = const_cast<srfTriangles_t *>( tri );
		lightTris->verts = tri->verts;
		lightTris->numVerts = tri->numVerts;
		lightTris->indexes = tri->indexes;
		lightTris->numIndexes = tri->numIndexes;
		lightTris->bounds = tri->bounds;
		lightTris->vertexBuffer = tri->vertexBuffer;
		lightTris->indexBuffer = tri->indexBuffer;
		lightTris->gpuSkinned = tri->gpuSkinned;
		lightTris->skinningBuffer = tri->skinningBuffer;
		lightTris->jointBuffer = tri->jointBuffer;
		lightTris->isParticle = tri->isParticle;
		tr.pc.c_createLightTris++;
		return lightTris;
	}

	byte *cullBits = NULL;
	if ( !allInside ) {
		cullBits = (byte *)_alloca16( tri->numVerts * sizeof( cullBits[0] ) );
		SIMDProcessor->Memset( cullBits, 0, tri->numVerts * sizeof( cullBits[0] ) );
		float *planeSide = (float *)_alloca16( tri->numVerts * sizeof( planeSide[0] ) );

		for ( int i = 0; i < 6; i++ ) {
			if ( frontBits & ( 1 << i ) ) {
				continue;
			}
			SIMDProcessor->Dot( planeSide, localClipPlanes[i], tri->verts, tri->numVerts );
			SIMDProcessor->CmpLT( cullBits, i, planeSide, LIGHT_CLIP_EPSILON, tri->numVerts );
		}
	}

	glIndex_t *indexes = (glIndex_t *)R_FrameAlloc( tri->numIndexes * sizeof( indexes[0] ) );
	int numIndexes = 0;
	for ( int i = 0, faceNum = 0; i < tri->numIndexes; i += 3, faceNum++ ) {
		if ( facing && !facing[faceNum] ) {
			continue;
		}

		const int i0 = tri->indexes[i + 0];
		const int i1 = tri->indexes[i + 1];
		const int i2 = tri->indexes[i + 2];
		if ( cullBits && ( cullBits[i0] & cullBits[i1] & cullBits[i2] ) ) {
			continue;
		}

		indexes[numIndexes + 0] = i0;
		indexes[numIndexes + 1] = i1;
		indexes[numIndexes + 2] = i2;
		numIndexes += 3;
	}

	if ( numIndexes == 0 ) {
		return NULL;
	}

	srfTriangles_t *lightTris = (srfTriangles_t *)R_ClearedFrameAlloc( sizeof( *lightTris ) );
	lightTris->ambientSurface = const_cast<srfTriangles_t *>( tri );
	lightTris->verts = tri->verts;
	lightTris->numVerts = tri->numVerts;
	lightTris->indexes = indexes;
	lightTris->numIndexes = numIndexes;
	lightTris->vertexBuffer = tri->vertexBuffer;
	lightTris->gpuSkinned = tri->gpuSkinned;
	lightTris->skinningBuffer = tri->skinningBuffer;
	lightTris->jointBuffer = tri->jointBuffer;
	lightTris->isParticle = tri->isParticle;
	SIMDProcessor->MinMax( lightTris->bounds[0], lightTris->bounds[1], tri->verts, indexes, numIndexes );
	tr.pc.c_createLightTris++;
	return lightTris;
}

/*
====================
R_AddTransientLightSurfaces

Adds frame-local realtime, fog, and blend-light surfaces for one visible entity.
====================
*/
static void R_AddTransientLightSurfaces( viewEntity_t *vEntity, const idRenderModel *model ) {
	idRenderEntityLocal *entityDef = vEntity->entityDef;
	const idBounds modelBounds = model->Bounds( entityDef );
	const int numSurfaces = model->NumSurfaces();

	for ( viewLight_t *vLight = tr.viewDef->viewLights; vLight; vLight = vLight->next ) {
		idRenderLightLocal *lightDef = vLight->lightDef;
		idScreenRect lightScissor = vLight->scissorRect;
		lightScissor.Intersect( vEntity->scissorRect );
		if ( lightScissor.IsEmpty() ) {
			continue;
		}

		idPlane localClipPlanes[6];
		for ( int i = 0; i < 6; i++ ) {
			R_GlobalPlaneToLocal( vEntity->modelMatrix, -lightDef->frustum[i], localClipPlanes[i] );
		}
		if ( R_CullLocalLightBounds( modelBounds, localClipPlanes ) ) {
			continue;
		}

		idVec3 localLightOrigin;
		R_GlobalPointToLocal( vEntity->modelMatrix, lightDef->globalLightOrigin, localLightOrigin );

		const bool useBakedSurfaceLighting = lightDef->world && lightDef->world->hasBakedLightmaps &&
			!r_skipBakedLightmaps.GetBool() && lightDef->GetBakedLight() && !lightDef->lightHasMoved;

		for ( int i = 0; i < numSurfaces; i++ ) {
			const modelSurface_t *surface = model->Surface( i );
			const srfTriangles_t *tri = surface->geometry;
			if ( !tri || !tri->numIndexes || tri->ambientViewCount != tr.viewCount ) {
				continue;
			}

			const idMaterial *shader = R_RemapShaderBySkin( surface->shader,
				entityDef->GetCustomSkin(), entityDef->GetCustomShader() );
			if ( !shader || !shader->ReceivesLighting() ||
				shader->Spectrum() != vLight->lightShader->Spectrum() ) {
				continue;
			}
			if (useBakedSurfaceLighting && tri->lightmapAtlas >= 0 && tri->lightmapTexCoords &&
				tri->bakedLightmap && tri->bakedDeluxemap) {
				continue;
			}
			if ( R_CullLocalLightBounds( tri->bounds, localClipPlanes ) ) {
				continue;
			}

			srfTriangles_t *lightTris = R_CreateTransientLightTris( entityDef, tri, lightDef, shader,
				localClipPlanes, localLightOrigin );
			if ( !lightTris ) {
				continue;
			}

			const idMaterial *drawShader = shader;
			R_GlobalShaderOverride( &drawShader );
			if ( !drawShader ) {
				continue;
			}

			if ( shader->Coverage() == MC_TRANSLUCENT ) {
				R_LinkLightSurf( &vLight->translucentInteractions, lightTris, vEntity, drawShader, lightScissor );
			} else {
				R_LinkLightSurf( &vLight->interactions, lightTris, vEntity, drawShader, lightScissor );
			}
		}
	}
}

/*
==================
R_CalcEntityScissorRectangle
==================
*/
idScreenRect R_CalcEntityScissorRectangle( viewEntity_t *vEntity ) {
	idBounds bounds;
	idRenderEntityLocal *def = vEntity->entityDef;

	tr.viewDef->viewFrustum.ProjectionBounds( idBox( def->referenceBounds, def->GetOrigin(), def->GetAxis() ), bounds );

	return R_ScreenRectFromViewFrustumBounds( bounds );
}

/*
===================
R_AddModelSurfaces

Here is where dynamic models are instantiated and their transient realtime
light surfaces are created. This is done on a sort-by-model basis
to keep source data in cache (most likely L2), since dynamic models will
typically be lit by two or more lights.
===================
*/
void R_AddModelSurfaces( void ) {
	viewEntity_t		*vEntity;
	idRenderModel		*model;

	// clear the ambient surface list
	tr.viewDef->numDrawSurfs = 0;
	tr.viewDef->maxDrawSurfs = 0;	// will be set to INITIAL_DRAWSURFS on R_AddDrawSurf

	// Go through each entity visible to the view.
	for ( vEntity = tr.viewDef->viewEntitys; vEntity; vEntity = vEntity->next ) {

		if ( r_useEntityScissors.GetBool() ) {
			// calculate the screen area covered by the entity
			idScreenRect scissorRect = R_CalcEntityScissorRectangle( vEntity );
			// intersect with the portal crossing scissor rectangle
			vEntity->scissorRect.Intersect( scissorRect );

			if ( r_showEntityScissors.GetBool() ) {
				R_ShowColoredScreenRect( vEntity->scissorRect, vEntity->entityDef->index );
			}
		}

		float oldFloatTime;
		int oldTime;

		game->SelectTimeGroup( vEntity->entityDef->GetTimeGroup() );

		if ( vEntity->entityDef->GetTimeGroup() ) {
			oldFloatTime = tr.viewDef->floatTime;
			oldTime = tr.viewDef->renderView.time;

			tr.viewDef->floatTime = game->GetTimeGroupTime( vEntity->entityDef->GetTimeGroup() ) * 0.001;
			tr.viewDef->renderView.time = game->GetTimeGroupTime( vEntity->entityDef->GetTimeGroup() );
		}

		if ( tr.viewDef->isXraySubview && vEntity->entityDef->GetXrayIndex() == 1 ) {
			if ( vEntity->entityDef->GetTimeGroup() ) {
				tr.viewDef->floatTime = oldFloatTime;
				tr.viewDef->renderView.time = oldTime;
			}
			continue;
		} else if ( !tr.viewDef->isXraySubview && vEntity->entityDef->GetXrayIndex() == 2 ) {
			if ( vEntity->entityDef->GetTimeGroup() ) {
				tr.viewDef->floatTime = oldFloatTime;
				tr.viewDef->renderView.time = oldTime;
			}
			continue;
		}

		// add the ambient surface if it has a visible rectangle
		if ( !vEntity->scissorRect.IsEmpty() ) {
			model = R_EntityDefDynamicModel( vEntity->entityDef );
			if ( model == NULL || model->NumSurfaces() <= 0 ) {
				if ( vEntity->entityDef->GetTimeGroup() ) {
					tr.viewDef->floatTime = oldFloatTime;
					tr.viewDef->renderView.time = oldTime;
				}
				continue;
			}

			R_AddAmbientDrawsurfs( vEntity );
			R_AddTransientLightSurfaces( vEntity, model );
			tr.pc.c_visibleViewEntities++;
		}

		if ( vEntity->entityDef->GetTimeGroup() ) {
			tr.viewDef->floatTime = oldFloatTime;
			tr.viewDef->renderView.time = oldTime;
		}

	}
}
