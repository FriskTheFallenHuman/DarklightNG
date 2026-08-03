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
#include "Model_local.h"

static const char *MD5_SnapshotName = "_MD5_Snapshot_";


/***********************************************************************

	idMD5Mesh

***********************************************************************/

static int c_numVerts = 0;
static int c_numWeights = 0;
static int c_numWeightJoints = 0;

static const int MAX_GPU_JOINTS = 136;
static const int MAX_GPU_VERTEX_WEIGHTS = 4;

static void R_InvertJointMatrix( idJointMat &matrix ) {
	float *m = matrix.ToFloatPtr();
	float temp[3];
	temp[0] = m[0] * m[3] + m[4] * m[7] + m[8] * m[11];
	temp[1] = m[1] * m[3] + m[5] * m[7] + m[9] * m[11];
	temp[2] = m[2] * m[3] + m[6] * m[7] + m[10] * m[11];
	m[3] = -temp[0];
	m[7] = -temp[1];
	m[11] = -temp[2];
	temp[0] = m[1]; m[1] = m[4]; m[4] = temp[0];
	temp[1] = m[2]; m[2] = m[8]; m[8] = temp[1];
	temp[2] = m[6]; m[6] = m[9]; m[9] = temp[2];
}

static void R_MultiplyJointMatrices( idJointMat &result, const idJointMat &left, const idJointMat &right ) {
	const float *a = left.ToFloatPtr();
	const float *b = right.ToFloatPtr();
	float *r = result.ToFloatPtr();
	for ( int row = 0; row < 3; row++ ) {
		const int o = row * 4;
		r[o + 0] = a[o + 0] * b[0] + a[o + 1] * b[4] + a[o + 2] * b[8];
		r[o + 1] = a[o + 0] * b[1] + a[o + 1] * b[5] + a[o + 2] * b[9];
		r[o + 2] = a[o + 0] * b[2] + a[o + 1] * b[6] + a[o + 2] * b[10];
		r[o + 3] = a[o + 0] * b[3] + a[o + 1] * b[7] + a[o + 2] * b[11] + a[o + 3];
	}
}

typedef struct vertexWeight_s {
	int							vert;
	int							joint;
	idVec3						offset;
	float						jointWeight;
} vertexWeight_t;

/*
====================
idMD5Mesh::idMD5Mesh
====================
*/
idMD5Mesh::idMD5Mesh() {
	scaledWeights	= NULL;
	weightIndex		= NULL;
	basePose		= NULL;
	skinVertices	= NULL;
	shader			= NULL;
	numTris			= 0;
	deformInfo		= NULL;
	surfaceNum		= 0;
}

/*
====================
idMD5Mesh::~idMD5Mesh
====================
*/
idMD5Mesh::~idMD5Mesh() {
	Mem_Free16( scaledWeights );
	Mem_Free16( weightIndex );
	Mem_Free16( basePose );
	Mem_Free16( skinVertices );
	if ( deformInfo ) {
		R_FreeDeformInfo( deformInfo );
		deformInfo = NULL;
	}
}

/*
====================
idMD5Mesh::ParseMesh
====================
*/
void idMD5Mesh::ParseMesh( idLexer &parser, int numJoints, const idJointMat *joints ) {
	idToken		token;
	idToken		name;
	int			num;
	int			count;
	int			jointnum;
	idStr		shaderName;
	int			i, j;
	idList<int>	tris;
	idList<int>	firstWeightForVertex;
	idList<int>	numWeightsForVertex;
	int			maxweight;
	idList<vertexWeight_t> tempWeights;

	parser.ExpectTokenString( "{" );

	//
	// parse name
	//
	if ( parser.CheckTokenString( "name" ) ) {
		parser.ReadToken( &name );
	}

	//
	// parse shader
	//
	parser.ExpectTokenString( "shader" );

	parser.ReadToken( &token );
	shaderName = token;

    shader = declManager->FindMaterial( shaderName );

	//
	// parse texture coordinates
	//
	parser.ExpectTokenString( "numverts" );
	count = parser.ParseInt();
	if ( count < 0 ) {
		parser.Error( "Invalid size: %s", token.c_str() );
	}

	texCoords.SetNum( count );
	firstWeightForVertex.SetNum( count );
	numWeightsForVertex.SetNum( count );

	numWeights = 0;
	maxweight = 0;
	for( i = 0; i < texCoords.Num(); i++ ) {
		parser.ExpectTokenString( "vert" );
		parser.ParseInt();

		parser.Parse1DMatrix( 2, texCoords[ i ].ToFloatPtr() );

		firstWeightForVertex[ i ]	= parser.ParseInt();
		numWeightsForVertex[ i ]	= parser.ParseInt();

		if ( !numWeightsForVertex[ i ] ) {
			parser.Error( "Vertex without any joint weights." );
		}

		numWeights += numWeightsForVertex[ i ];
		if ( numWeightsForVertex[ i ] + firstWeightForVertex[ i ] > maxweight ) {
			maxweight = numWeightsForVertex[ i ] + firstWeightForVertex[ i ];
		}
	}

	//
	// parse tris
	//
	parser.ExpectTokenString( "numtris" );
	count = parser.ParseInt();
	if ( count < 0 ) {
		parser.Error( "Invalid size: %d", count );
	}

	tris.SetNum( count * 3 );
	numTris = count;
	for( i = 0; i < count; i++ ) {
		parser.ExpectTokenString( "tri" );
		parser.ParseInt();

		tris[ i * 3 + 0 ] = parser.ParseInt();
		tris[ i * 3 + 1 ] = parser.ParseInt();
		tris[ i * 3 + 2 ] = parser.ParseInt();
	}

	//
	// parse weights
	//
	parser.ExpectTokenString( "numweights" );
	count = parser.ParseInt();
	if ( count < 0 ) {
		parser.Error( "Invalid size: %d", count );
	}

	if ( maxweight > count ) {
		parser.Warning( "Vertices reference out of range weights in model (%d of %d weights).", maxweight, count );
	}

	tempWeights.SetNum( count );

	for( i = 0; i < count; i++ ) {
		parser.ExpectTokenString( "weight" );
		parser.ParseInt();

		jointnum = parser.ParseInt();
		if ( ( jointnum < 0 ) || ( jointnum >= numJoints ) ) {
			parser.Error( "Joint Index out of range(%d): %d", numJoints, jointnum );
		}

		tempWeights[ i ].joint			= jointnum;
		tempWeights[ i ].jointWeight	= parser.ParseFloat();

		parser.Parse1DMatrix( 3, tempWeights[ i ].offset.ToFloatPtr() );
	}

	// create pre-scaled weights and an index for the vertex/joint lookup
	scaledWeights = (idVec4 *) Mem_Alloc16( numWeights * sizeof( scaledWeights[0] ) );
	weightIndex = (int *) Mem_Alloc16( numWeights * 2 * sizeof( weightIndex[0] ) );
	memset( weightIndex, 0, numWeights * 2 * sizeof( weightIndex[0] ) );

	count = 0;
	for( i = 0; i < texCoords.Num(); i++ ) {
		num = firstWeightForVertex[i];
		for( j = 0; j < numWeightsForVertex[i]; j++, num++, count++ ) {
			scaledWeights[count].ToVec3() = tempWeights[num].offset * tempWeights[num].jointWeight;
			scaledWeights[count].w = tempWeights[num].jointWeight;
			weightIndex[count * 2 + 0] = tempWeights[num].joint * sizeof( idJointMat );
		}
		weightIndex[count * 2 - 1] = 1;
	}

	parser.ExpectTokenString( "}" );

	// update counters
	c_numVerts += texCoords.Num();
	c_numWeights += numWeights;
	c_numWeightJoints++;
	for ( i = 0; i < numWeights; i++ ) {
		c_numWeightJoints += weightIndex[i*2+1];
	}

	//
	// build the information that will be common to all animations of this mesh:
	// silhouette edge connectivity and normal / tangent generation information
	//
	idDrawVert *verts = (idDrawVert *) _alloca16( texCoords.Num() * sizeof( idDrawVert ) );
	for ( i = 0; i < texCoords.Num(); i++ ) {
		verts[i].Clear();
		verts[i].st = texCoords[i];
	}
	TransformVerts( verts, joints );
	deformInfo = R_BuildDeformInfo( texCoords.Num(), verts, tris.Num(), tris.Ptr(), shader->UseUnsmoothedTangents() );

	// Keep a complete bind-pose stream. The vertex shader applies matrices that
	// are relative to this pose, so positions and tangent vectors can share the
	// same compact four-weight skinning stream.
	basePose = (idDrawVert *)Mem_Alloc16( deformInfo->numOutputVerts * sizeof( basePose[0] ) );
	memcpy( basePose, verts, deformInfo->numSourceVerts * sizeof( basePose[0] ) );
	const int mirrorBase = deformInfo->numOutputVerts - deformInfo->numMirroredVerts;
	for ( i = 0; i < deformInfo->numMirroredVerts; i++ ) {
		basePose[mirrorBase + i] = basePose[deformInfo->mirroredVerts[i]];
	}

	srfTriangles_t tangentTri;
	memset( &tangentTri, 0, sizeof( tangentTri ) );
	tangentTri.numVerts = deformInfo->numOutputVerts;
	tangentTri.verts = basePose;
	tangentTri.numIndexes = deformInfo->numIndexes;
	tangentTri.indexes = deformInfo->indexes;
	tangentTri.dominantTris = deformInfo->dominantTris;
	R_DeriveTangents( &tangentTri, false );

	// Pack the strongest four influences exactly once. Models requiring more
	// than the UBO-backed joint budget are rejected instead of falling back to
	// CPU deformation.
	if ( numJoints > MAX_GPU_JOINTS ) {
		parser.Error( "Model has %d joints; GPU skinning supports at most %d.", numJoints, MAX_GPU_JOINTS );
	}
	skinVertices = (gpuSkinVertex_t *)Mem_Alloc16( deformInfo->numOutputVerts * sizeof( skinVertices[0] ) );
	memset( skinVertices, 0, deformInfo->numOutputVerts * sizeof( skinVertices[0] ) );

	for ( i = 0; i < texCoords.Num(); i++ ) {
		idList<int> sortedWeights;
		sortedWeights.SetNum( numWeightsForVertex[i] );
		for ( j = 0; j < sortedWeights.Num(); j++ ) {
			sortedWeights[j] = firstWeightForVertex[i] + j;
		}
		for ( j = 0; j < sortedWeights.Num(); j++ ) {
			for ( int k = j + 1; k < sortedWeights.Num(); k++ ) {
				if ( tempWeights[sortedWeights[k]].jointWeight > tempWeights[sortedWeights[j]].jointWeight ) {
					const int temp = sortedWeights[j];
					sortedWeights[j] = sortedWeights[k];
					sortedWeights[k] = temp;
				}
			}
		}

		const int usedWeights = Min( MAX_GPU_VERTEX_WEIGHTS, sortedWeights.Num() );
		float usedWeight = 0.0f;
		for ( j = 0; j < usedWeights; j++ ) {
			usedWeight += tempWeights[sortedWeights[j]].jointWeight;
		}
		if ( usedWeight <= 0.0f ) {
			parser.Error( "Vertex %d has no positive joint weights.", i );
		}

		int packedTotal = 0;
		for ( j = 0; j < usedWeights; j++ ) {
			const vertexWeight_t &weight = tempWeights[sortedWeights[j]];
			const int packedWeight = idMath::ClampInt( 0, 255,
				idMath::FtoiFast( weight.jointWeight * ( 255.0f / usedWeight ) ) );
			skinVertices[i].joints[j] = (byte)weight.joint;
			skinVertices[i].weights[j] = (byte)packedWeight;
			packedTotal += packedWeight;
		}
		const int residual = 255 - packedTotal;
		skinVertices[i].weights[0] = (byte)idMath::ClampInt( 0, 255,
			(int)skinVertices[i].weights[0] + residual );
	}

	for ( i = 0; i < deformInfo->numMirroredVerts; i++ ) {
		skinVertices[mirrorBase + i] = skinVertices[deformInfo->mirroredVerts[i]];
	}

	tempWeights.Clear();
	numWeightsForVertex.Clear();
	firstWeightForVertex.Clear();
}

/*
====================
idMD5Mesh::TransformVerts
====================
*/
void idMD5Mesh::TransformVerts( idDrawVert *verts, const idJointMat *entJoints ) {
	SIMDProcessor->TransformVerts( verts, texCoords.Num(), entJoints, scaledWeights, weightIndex, numWeights );
}

/*
====================
idMD5Mesh::UpdateSurface
====================
*/
void idMD5Mesh::UpdateSurface( const idRenderEntity *ent, const idJointMat *entJoints,
		const idJointMat *relativeJoints, int numJoints, modelSurface_t *surf ) {
	srfTriangles_t *tri;

	tr.pc.c_deformedSurfaces++;
	tr.pc.c_deformedVerts += deformInfo->numOutputVerts;
	tr.pc.c_deformedIndexes += deformInfo->numIndexes;

	// Keep the authored mesh material on the cached snapshot. The render world
	// applies entity skins and custom shaders when the surface is submitted;
	// caching that remap here would run the skin table a second time.
	surf->shader = shader;

	if ( surf->geometry ) {
		if ( surf->geometry->numVerts != deformInfo->numOutputVerts ||
			surf->geometry->numIndexes != deformInfo->numIndexes ) {
			R_FreeStaticTriSurf( surf->geometry );
			surf->geometry = R_AllocStaticTriSurf();
		}
	} else {
		surf->geometry = R_AllocStaticTriSurf();
	}

	tri = surf->geometry;

	// note that some of the data is references, and should not be freed
	tri->deformedSurface = true;
	tri->tangentsCalculated = true;
	tri->facePlanesCalculated = false;
	tri->gpuSkinned = true;

	tri->numIndexes = deformInfo->numIndexes;
	tri->indexes = deformInfo->indexes;
	tri->silIndexes = deformInfo->silIndexes;
	tri->numMirroredVerts = deformInfo->numMirroredVerts;
	tri->mirroredVerts = deformInfo->mirroredVerts;
	tri->numDupVerts = deformInfo->numDupVerts;
	tri->dupVerts = deformInfo->dupVerts;
	tri->numSilEdges = deformInfo->numSilEdges;
	tri->silEdges = deformInfo->silEdges;
	tri->dominantTris = deformInfo->dominantTris;
	tri->numVerts = deformInfo->numOutputVerts;

	if ( tri->verts == NULL ) {
		R_AllocStaticTriSurfVerts( tri, tri->numVerts );
		memcpy( tri->verts, basePose, tri->numVerts * sizeof( tri->verts[0] ) );
	}

	if ( tri->numJoints != numJoints ) {
		Mem_Free16( tri->jointMatrices );
		tri->jointMatrices = NULL;
		delete tri->jointBuffer;
		tri->jointBuffer = NULL;
		tri->numJoints = numJoints;
	}
	if ( tri->skinningVerts == NULL ) {
		tri->skinningVerts = (gpuSkinVertex_t *)Mem_Alloc16( tri->numVerts * sizeof( tri->skinningVerts[0] ) );
		memcpy( tri->skinningVerts, skinVertices, tri->numVerts * sizeof( tri->skinningVerts[0] ) );
	}
	if ( tri->jointMatrices == NULL ) {
		tri->jointMatrices = (idJointMat *)Mem_Alloc16( numJoints * sizeof( tri->jointMatrices[0] ) );
	}
	memcpy( tri->jointMatrices, relativeJoints, numJoints * sizeof( tri->jointMatrices[0] ) );

	if ( !tri->vertexBuffer ) {
		tri->vertexBuffer = new idVertexBuffer;
		if ( !tri->vertexBuffer->AllocBufferObject( tri->verts, tri->numVerts * sizeof( tri->verts[0] ) ) ) {
			common->Error( "idMD5Mesh::UpdateSurface: failed to allocate bind-pose vertex buffer" );
		}
	}
	if ( !tri->indexBuffer ) {
		tri->indexBuffer = new idIndexBuffer;
		if ( !tri->indexBuffer->AllocBufferObject( tri->indexes, tri->numIndexes * sizeof( tri->indexes[0] ) ) ) {
			common->Error( "idMD5Mesh::UpdateSurface: failed to allocate index buffer" );
		}
	}
	if ( !tri->skinningBuffer ) {
		tri->skinningBuffer = new idVertexBuffer;
		if ( !tri->skinningBuffer->AllocBufferObject( tri->skinningVerts,
			tri->numVerts * sizeof( tri->skinningVerts[0] ) ) ) {
			common->Error( "idMD5Mesh::UpdateSurface: failed to allocate skinning vertex buffer" );
		}
	}
	if ( !tri->jointBuffer ) {
		tri->jointBuffer = new idJointBuffer;
		if ( !tri->jointBuffer->AllocBufferObject( tri->jointMatrices[0].ToFloatPtr(), numJoints ) ) {
			common->Error( "idMD5Mesh::UpdateSurface: failed to allocate joint uniform buffer" );
		}
	} else {
		tri->jointBuffer->Update( tri->jointMatrices[0].ToFloatPtr(), numJoints );
	}

	// Vertex deformation is GPU-only. CPU skinning remains confined to bounds
	// and other front-end visibility work.
	tri->bounds = CalcBounds( entJoints );
}

/*
====================
idMD5Mesh::CalcBounds
====================
*/
idBounds idMD5Mesh::CalcBounds( const idJointMat *entJoints ) {
	idBounds	bounds;
	idDrawVert *verts = (idDrawVert *) _alloca16( texCoords.Num() * sizeof( idDrawVert ) );

	TransformVerts( verts, entJoints );

	SIMDProcessor->MinMax( bounds[0], bounds[1], verts, texCoords.Num() );

	return bounds;
}

/*
====================
idMD5Mesh::NearestJoint
====================
*/
int idMD5Mesh::NearestJoint( int a, int b, int c ) const {
	int i, bestJoint, vertNum, weightVertNum;
	float bestWeight;

	// duplicated vertices might not have weights
	if ( a >= 0 && a < texCoords.Num() ) {
		vertNum = a;
	} else if ( b >= 0 && b < texCoords.Num() ) {
		vertNum = b;
	} else if ( c >= 0 && c < texCoords.Num() ) {
		vertNum = c;
	} else {
		// all vertices are duplicates which shouldn't happen
		return 0;
	}

	// find the first weight for this vertex
 	weightVertNum = 0;
	for( i = 0; weightVertNum < vertNum; i++ ) {
		weightVertNum += weightIndex[i*2+1];
	}

	// get the joint for the largest weight
	bestWeight = scaledWeights[i].w;
	bestJoint = weightIndex[i*2+0] / sizeof( idJointMat );
	for( ; weightIndex[i*2+1] == 0; i++ ) {
		if ( scaledWeights[i].w > bestWeight ) {
			bestWeight = scaledWeights[i].w;
			bestJoint = weightIndex[i*2+0] / sizeof( idJointMat );
		}
	}
	return bestJoint;
}

/*
====================
idMD5Mesh::NumVerts
====================
*/
int idMD5Mesh::NumVerts( void ) const {
	return texCoords.Num();
}

/*
====================
idMD5Mesh::NumTris
====================
*/
int	idMD5Mesh::NumTris( void ) const {
	return numTris;
}

/*
====================
idMD5Mesh::NumWeights
====================
*/
int	idMD5Mesh::NumWeights( void ) const {
	return numWeights;
}

/***********************************************************************

	idRenderModelMD5

***********************************************************************/

/*
====================
idRenderModelMD5::ParseJoint
====================
*/
void idRenderModelMD5::ParseJoint( idLexer &parser, idMD5Joint *joint, idJointQuat *defaultPose ) {
	idToken	token;
	int		num;

	//
	// parse name
	//
	parser.ReadToken( &token );
	joint->name = token;

	//
	// parse parent
	//
	num = parser.ParseInt();
	if ( num < 0 ) {
		joint->parent = NULL;
	} else {
		if ( num >= joints.Num() - 1 ) {
			parser.Error( "Invalid parent for joint '%s'", joint->name.c_str() );
		}
		joint->parent = &joints[ num ];
	}

	//
	// parse default pose
	//
	parser.Parse1DMatrix( 3, defaultPose->t.ToFloatPtr() );
	parser.Parse1DMatrix( 3, defaultPose->q.ToFloatPtr() );
	defaultPose->q.w = defaultPose->q.CalcW();
}

/*
====================
idRenderModelMD5::InitFromFile
====================
*/
void idRenderModelMD5::InitFromFile( const char *fileName ) {
	name = fileName;
	LoadModel();
}

/*
====================
idRenderModelMD5::LoadModel

used for initial loads, reloadModel, and reloading the data of purged models
Upon exit, the model will absolutely be valid, but possibly as a default model
====================
*/
void idRenderModelMD5::LoadModel() {
	int			version;
	int			i;
	int			num;
	int			parentNum;
	idToken		token;
	idLexer		parser( LEXFL_ALLOWPATHNAMES | LEXFL_NOSTRINGESCAPECHARS );
	idJointQuat	*pose;
	idMD5Joint	*joint;
	idJointMat *poseMat3;

	if ( !purged ) {
		PurgeModel();
	}
	purged = false;

	if ( !parser.LoadFile( name ) ) {
		MakeDefaultModel();
		return;
	}

	parser.ExpectTokenString( MD5_VERSION_STRING );
	version = parser.ParseInt();

	if ( version != MD5_VERSION ) {
		parser.Error( "Invalid version %d.  Should be version %d\n", version, MD5_VERSION );
	}

	//
	// skip commandline
	//
	parser.ExpectTokenString( "commandline" );
	parser.ReadToken( &token );

	// parse num joints
	parser.ExpectTokenString( "numJoints" );
	num  = parser.ParseInt();
	joints.SetGranularity( 1 );
	joints.SetNum( num );
	defaultPose.SetGranularity( 1 );
	defaultPose.SetNum( num );
	poseMat3 = ( idJointMat * )_alloca16( num * sizeof( *poseMat3 ) );

	// parse num meshes
	parser.ExpectTokenString( "numMeshes" );
	num = parser.ParseInt();
	if ( num < 0 ) {
		parser.Error( "Invalid size: %d", num );
	}
	meshes.SetGranularity( 1 );
	meshes.SetNum( num );

	//
	// parse joints
	//
	parser.ExpectTokenString( "joints" );
	parser.ExpectTokenString( "{" );
	pose = defaultPose.Ptr();
	joint = joints.Ptr();
	for( i = 0; i < joints.Num(); i++, joint++, pose++ ) {
		ParseJoint( parser, joint, pose );
		poseMat3[ i ].SetRotation( pose->q.ToMat3() );
		poseMat3[ i ].SetTranslation( pose->t );
		if ( joint->parent ) {
			parentNum = joint->parent - joints.Ptr();
			pose->q = ( poseMat3[ i ].ToMat3() * poseMat3[ parentNum ].ToMat3().Transpose() ).ToQuat();
			pose->t = ( poseMat3[ i ].ToVec3() - poseMat3[ parentNum ].ToVec3() ) * poseMat3[ parentNum ].ToMat3().Transpose();
		}
	}
	parser.ExpectTokenString( "}" );
	referenceJoints.SetGranularity( 1 );
	referenceJoints.SetNum( joints.Num() );
	memcpy( referenceJoints.Ptr(), poseMat3, joints.Num() * sizeof( poseMat3[0] ) );
	for ( i = 0; i < referenceJoints.Num(); i++ ) {
		R_InvertJointMatrix( referenceJoints[i] );
	}

	for( i = 0; i < meshes.Num(); i++ ) {
		parser.ExpectTokenString( "mesh" );
		meshes[ i ].ParseMesh( parser, defaultPose.Num(), poseMat3 );
	}

	//
	// calculate the bounds of the model
	//
	CalculateBounds( poseMat3 );

	// set the timestamp for reloadmodels
	fileSystem->ReadFile( name, NULL, &timeStamp );
}

/*
==============
idRenderModelMD5::Print
==============
*/
void idRenderModelMD5::Print() const {
	const idMD5Mesh	*mesh;
	int			i;

	common->Printf( "%s\n", name.c_str() );
	common->Printf( "Dynamic model.\n" );
	common->Printf( "Generated smooth normals.\n" );
	common->Printf( "    verts  tris weights material\n" );
	int	totalVerts = 0;
	int	totalTris = 0;
	int	totalWeights = 0;
	for( mesh = meshes.Ptr(), i = 0; i < meshes.Num(); i++, mesh++ ) {
		totalVerts += mesh->NumVerts();
		totalTris += mesh->NumTris();
		totalWeights += mesh->NumWeights();
		common->Printf( "%2i: %5i %5i %7i %s\n", i, mesh->NumVerts(), mesh->NumTris(), mesh->NumWeights(), mesh->shader->GetName() );
	}	
	common->Printf( "-----\n" );
	common->Printf( "%4i verts.\n", totalVerts );
	common->Printf( "%4i tris.\n", totalTris );
	common->Printf( "%4i weights.\n", totalWeights );
	common->Printf( "%4i joints.\n", joints.Num() );
}

/*
==============
idRenderModelMD5::List
==============
*/
void idRenderModelMD5::List() const {
	int			i;
	const idMD5Mesh	*mesh;
	int			totalTris = 0;
	int			totalVerts = 0;

	for( mesh = meshes.Ptr(), i = 0; i < meshes.Num(); i++, mesh++ ) {
		totalTris += mesh->numTris;
		totalVerts += mesh->NumVerts();
	}
	common->Printf( " %4ik %3i %4i %4i %s(MD5)", Memory()/1024, meshes.Num(), totalVerts, totalTris, Name() );

	if ( defaulted ) {
		common->Printf( " (DEFAULTED)" );
	}

	common->Printf( "\n" );
}

/*
====================
idRenderModelMD5::CalculateBounds
====================
*/
void idRenderModelMD5::CalculateBounds( const idJointMat *entJoints ) {
	int			i;
	idMD5Mesh	*mesh;

	bounds.Clear();
	for( mesh = meshes.Ptr(), i = 0; i < meshes.Num(); i++, mesh++ ) {
		bounds.AddBounds( mesh->CalcBounds( entJoints ) );
	}
}

/*
====================
idRenderModelMD5::Bounds

This calculates a rough bounds by using the joint radii without
transforming all the points
====================
*/
idBounds idRenderModelMD5::Bounds( const idRenderEntity *ent ) const {
#if 0
	// we can't calculate a rational bounds without an entity,
	// because joints could be positioned to deform it into an
	// arbitrarily large shape
	if ( !ent ) {
		common->Error( "idRenderModelMD5::Bounds: called without entity" );
	}
#endif

	if ( !ent ) {
		// this is the bounds for the reference pose
		return bounds;
	}

	return ent->GetBounds();
}

/*
====================
idRenderModelMD5::DrawJoints
====================
*/
void idRenderModelMD5::DrawJoints( const idRenderEntity *ent, const struct viewDef_s *view ) const {
	int					i;
	int					num;
	idVec3				pos;
	const idJointMat	*joint;
	const idMD5Joint	*md5Joint;
	int					parentNum;

	num = ent->GetNumJoints();
	joint = ent->GetJoints();
	md5Joint = joints.Ptr();	
	for( i = 0; i < num; i++, joint++, md5Joint++ ) {
		pos = ent->GetOrigin() + joint->ToVec3() * ent->GetAxis();
		if ( md5Joint->parent ) {
			parentNum = md5Joint->parent - joints.Ptr();
			session->rw->DebugLine( colorWhite, ent->GetOrigin() + ent->GetJoints()[ parentNum ].ToVec3() * ent->GetAxis(), pos );
		}

		session->rw->DebugLine( colorRed,	pos, pos + joint->ToMat3()[ 0 ] * 2.0f * ent->GetAxis() );
		session->rw->DebugLine( colorGreen,	pos, pos + joint->ToMat3()[ 1 ] * 2.0f * ent->GetAxis() );
		session->rw->DebugLine( colorBlue,	pos, pos + joint->ToMat3()[ 2 ] * 2.0f * ent->GetAxis() );
	}

	idBounds bounds;

	bounds.FromTransformedBounds( ent->GetBounds(), vec3_zero, ent->GetAxis() );
	session->rw->DebugBounds( colorMagenta, bounds, ent->GetOrigin() );

	if ( ( r_jointNameScale.GetFloat() != 0.0f ) && ( bounds.Expand( 128.0f ).ContainsPoint( view->renderView.vieworg - ent->GetOrigin() ) ) ) {
		idVec3	offset( 0, 0, r_jointNameOffset.GetFloat() );
		float	scale;

		scale = r_jointNameScale.GetFloat();
		joint = ent->GetJoints();
		num = ent->GetNumJoints();
		for( i = 0; i < num; i++, joint++ ) {
			pos = ent->GetOrigin() + joint->ToVec3() * ent->GetAxis();
			session->rw->DrawText( joints[ i ].name, pos + offset, scale, colorWhite, view->renderView.viewaxis, 1 );
		}
	}
}

/*
====================
idRenderModelMD5::InstantiateDynamicModel
====================
*/
idRenderModel *idRenderModelMD5::InstantiateDynamicModel( const idRenderEntity *ent, const struct viewDef_s *view, idRenderModel *cachedModel ) {
	int					i, surfaceNum;
	idMD5Mesh			*mesh;
	idRenderModelStatic	*staticModel;

	if ( cachedModel && !r_useCachedDynamicModels.GetBool() ) {
		delete cachedModel;
		cachedModel = NULL;
	}

	if ( purged ) {
		common->DWarning( "model %s instantiated while purged", Name() );
		LoadModel();
	}

	if ( !ent->GetJoints() ) {
		common->Printf( "idRenderModelMD5::InstantiateDynamicModel: NULL joints on renderEntity for '%s'\n", Name() );
		delete cachedModel;
		return NULL;
	} else if ( ent->GetNumJoints() != joints.Num() ) {
		common->Printf( "idRenderModelMD5::InstantiateDynamicModel: renderEntity has different number of joints than model for '%s'\n", Name() );
		delete cachedModel;
		return NULL;
	}
	if ( !glConfig.gpuSkinningAvailable ) {
		common->Error( "idRenderModelMD5::InstantiateDynamicModel: '%s' requires GPU uniform-buffer skinning", Name() );
	}
	if ( joints.Num() > MAX_GPU_JOINTS ) {
		common->Error( "idRenderModelMD5::InstantiateDynamicModel: '%s' has %d joints, maximum is %d",
			Name(), joints.Num(), MAX_GPU_JOINTS );
	}

	idJointMat *relativeJoints = (idJointMat *)_alloca16( joints.Num() * sizeof( relativeJoints[0] ) );
	for ( i = 0; i < joints.Num(); i++ ) {
		R_MultiplyJointMatrices( relativeJoints[i], ent->GetJoints()[i], referenceJoints[i] );
	}

	tr.pc.c_generateMd5++;

	if ( cachedModel ) {
		assert( dynamic_cast<idRenderModelStatic *>(cachedModel) != NULL );
		assert( idStr::Icmp( cachedModel->Name(), MD5_SnapshotName ) == 0 );
		staticModel = static_cast<idRenderModelStatic *>(cachedModel);
	} else {
		staticModel = new idRenderModelStatic;
		staticModel->InitEmpty( MD5_SnapshotName );
	}

	staticModel->bounds.Clear();

	if ( r_showSkel.GetInteger() ) {
		if ( ( view != NULL ) && ( !r_skipSuppress.GetBool() || !ent->GetSuppressSurfaceInViewID() || ( ent->GetSuppressSurfaceInViewID() != view->renderView.viewID ) ) ) {
			// only draw the skeleton
			DrawJoints( ent, view );
		}

		if ( r_showSkel.GetInteger() > 1 ) {
			// turn off the model when showing the skeleton
			staticModel->InitEmpty( MD5_SnapshotName );
			return staticModel;
		}
	}

	// create all the surfaces
	for( mesh = meshes.Ptr(), i = 0; i < meshes.Num(); i++, mesh++ ) {
		// avoid deforming the surface if it will be a nodraw due to a skin remapping
		// FIXME: may have to still deform clipping hulls
		const idMaterial *shader = mesh->shader;
		
		shader = R_RemapShaderBySkin( shader, ent->GetCustomSkin(), ent->GetCustomShader() );
		
		if ( !shader || ( !shader->IsDrawn() && !shader->SurfaceCastsShadow() ) ) {
			staticModel->DeleteSurfaceWithId( i );
			mesh->surfaceNum = -1;
			continue;
		}

		modelSurface_t *surf;

		if ( staticModel->FindSurfaceWithId( i, surfaceNum ) ) {
			mesh->surfaceNum = surfaceNum;
			surf = &staticModel->surfaces[surfaceNum];
		} else {

			// Remove Overlays before adding new surfaces
			idRenderModelOverlay::RemoveOverlaySurfacesFromModel( staticModel );

			mesh->surfaceNum = staticModel->NumSurfaces();
			surf = &staticModel->surfaces.Alloc();
			surf->geometry = NULL;
			surf->shader = NULL;
			surf->id = i;
		}

		mesh->UpdateSurface( ent, ent->GetJoints(), relativeJoints, joints.Num(), surf );

		staticModel->bounds.AddPoint( surf->geometry->bounds[0] );
		staticModel->bounds.AddPoint( surf->geometry->bounds[1] );
	}

	return staticModel;
}

/*
====================
idRenderModelMD5::IsDynamicModel
====================
*/
dynamicModel_t idRenderModelMD5::IsDynamicModel() const {
	return DM_CACHED;
}

/*
====================
idRenderModelMD5::NumJoints
====================
*/
int idRenderModelMD5::NumJoints( void ) const {
	return joints.Num();
}

/*
====================
idRenderModelMD5::GetJoints
====================
*/
const idMD5Joint *idRenderModelMD5::GetJoints( void ) const {
	return joints.Ptr();
}

/*
====================
idRenderModelMD5::GetDefaultPose
====================
*/
const idJointQuat *idRenderModelMD5::GetDefaultPose( void ) const {
	return defaultPose.Ptr();
}

/*
====================
idRenderModelMD5::GetJointHandle
====================
*/
jointHandle_t idRenderModelMD5::GetJointHandle( const char *name ) const {
	const idMD5Joint *joint;
	int	i;
	
	joint = joints.Ptr();
	for( i = 0; i < joints.Num(); i++, joint++ ) {
		if ( idStr::Icmp( joint->name.c_str(), name ) == 0 ) {
			return ( jointHandle_t )i;
		}
	}

	return INVALID_JOINT;
}

/*
=====================
idRenderModelMD5::GetJointName
=====================
*/
const char *idRenderModelMD5::GetJointName( jointHandle_t handle ) const {
	if ( ( handle < 0 ) || ( handle >= joints.Num() ) ) {
		return "<invalid joint>";
	}

	return joints[ handle ].name;
}

/*
====================
idRenderModelMD5::NearestJoint
====================
*/
int idRenderModelMD5::NearestJoint( int surfaceNum, int a, int b, int c ) const {
	int i;
	const idMD5Mesh *mesh;

	if ( surfaceNum > meshes.Num() ) {
		common->Error( "idRenderModelMD5::NearestJoint: surfaceNum > meshes.Num()" );
	}

	for ( mesh = meshes.Ptr(), i = 0; i < meshes.Num(); i++, mesh++ ) {
		if ( mesh->surfaceNum == surfaceNum ) {
			return mesh->NearestJoint( a, b, c );
		}
	}
	return 0;
}

/*
====================
idRenderModelMD5::TouchData

models that are already loaded at level start time
will still touch their materials to make sure they
are kept loaded
====================
*/
void idRenderModelMD5::TouchData() {
	idMD5Mesh	*mesh;
	int			i;

	for( mesh = meshes.Ptr(), i = 0; i < meshes.Num(); i++, mesh++ ) {
		declManager->FindMaterial( mesh->shader->GetName() );
	}
}

/*
===================
idRenderModelMD5::PurgeModel

frees all the data, but leaves the class around for dangling references,
which can regenerate the data with LoadModel()
===================
*/
void idRenderModelMD5::PurgeModel() {
	purged = true;
	joints.Clear();
	defaultPose.Clear();
	referenceJoints.Clear();
	meshes.Clear();
}

/*
===================
idRenderModelMD5::Memory
===================
*/
int	idRenderModelMD5::Memory() const {
	int		total, i;

	total = sizeof( *this );
	total += joints.MemoryUsed() + defaultPose.MemoryUsed() + referenceJoints.MemoryUsed() + meshes.MemoryUsed();

	// count up strings
	for ( i = 0; i < joints.Num(); i++ ) {
		total += joints[i].name.DynamicMemoryUsed();
	}

	// count up meshes
	for ( i = 0 ; i < meshes.Num() ; i++ ) {
		const idMD5Mesh *mesh = &meshes[i];

		total += mesh->texCoords.MemoryUsed() + mesh->numWeights * ( sizeof( mesh->scaledWeights[0] ) + sizeof( mesh->weightIndex[0] ) * 2 );
		total += mesh->deformInfo->numOutputVerts * ( sizeof( mesh->basePose[0] ) + sizeof( mesh->skinVertices[0] ) );

		// sum up deform info
		total += sizeof( mesh->deformInfo );
		total += R_DeformInfoMemoryUsed( mesh->deformInfo );
	}
	return total;
}
