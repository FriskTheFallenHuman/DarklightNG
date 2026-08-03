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

idCVar r_showBuffers( "r_showBuffers", "0", CVAR_RENDERER | CVAR_INTEGER, "print per-surface GPU buffer allocations" );

void UnbindBufferObjects() {
	qglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
	qglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0 );
	if ( qglBindBufferBase ) {
		qglBindBufferBase( GL_UNIFORM_BUFFER, 0, 0 );
	}
}

idVertexBuffer::idVertexBuffer() : size( 0 ), apiObject( 0 ), mapped( false ) {
}

idVertexBuffer::~idVertexBuffer() {
	FreeBufferObject();
}

bool idVertexBuffer::AllocBufferObject( const void *data, int allocSize, GLenum usage ) {
	if ( allocSize <= 0 || !glConfig.ARBVertexBufferObjectAvailable ) {
		return false;
	}
	FreeBufferObject();
	qglGenBuffersARB( 1, &apiObject );
	if ( apiObject == 0 ) {
		return false;
	}
	size = allocSize;
	Bind();
	qglBufferDataARB( GL_ARRAY_BUFFER_ARB, (GLsizeiptrARB)size, data, usage );
	if ( qglGetError() == GL_OUT_OF_MEMORY ) {
		FreeBufferObject();
		return false;
	}
	if ( r_showBuffers.GetBool() ) {
		common->Printf( "vertex buffer %u: %i bytes\n", apiObject, size );
	}
	return true;
}

void idVertexBuffer::FreeBufferObject() {
	if ( apiObject == 0 ) {
		return;
	}
	if ( mapped ) {
		UnmapBuffer();
	}
	if ( qglDeleteBuffersARB ) {
		qglDeleteBuffersARB( 1, &apiObject );
	}
	apiObject = 0;
	size = 0;
}

void idVertexBuffer::Update( const void *data, int updateSize ) const {
	if ( apiObject == 0 || data == NULL || updateSize < 0 || updateSize > size ) {
		common->Error( "idVertexBuffer::Update: invalid update of %i byte buffer with %i bytes", size, updateSize );
	}
	Bind();
	qglBufferSubDataARB( GL_ARRAY_BUFFER_ARB, 0, (GLsizeiptrARB)updateSize, data );
}

void *idVertexBuffer::MapBuffer( bufferMapType_t mapType ) const {
	if ( apiObject == 0 || mapped ) {
		return NULL;
	}
	Bind();
	void *data = qglMapBufferARB( GL_ARRAY_BUFFER_ARB, mapType == BM_READ ? GL_READ_ONLY_ARB : GL_WRITE_ONLY_ARB );
	mapped = data != NULL;
	return data;
}

void idVertexBuffer::UnmapBuffer() const {
	if ( apiObject == 0 || !mapped ) {
		return;
	}
	Bind();
	qglUnmapBufferARB( GL_ARRAY_BUFFER_ARB );
	mapped = false;
}

void idVertexBuffer::Bind() const {
	qglBindBufferARB( GL_ARRAY_BUFFER_ARB, apiObject );
}

idIndexBuffer::idIndexBuffer() : size( 0 ), apiObject( 0 ), mapped( false ) {
}

idIndexBuffer::~idIndexBuffer() {
	FreeBufferObject();
}

bool idIndexBuffer::AllocBufferObject( const void *data, int allocSize, GLenum usage ) {
	if ( allocSize <= 0 || !glConfig.ARBVertexBufferObjectAvailable ) {
		return false;
	}
	FreeBufferObject();
	qglGenBuffersARB( 1, &apiObject );
	if ( apiObject == 0 ) {
		return false;
	}
	size = allocSize;
	Bind();
	qglBufferDataARB( GL_ELEMENT_ARRAY_BUFFER_ARB, (GLsizeiptrARB)size, data, usage );
	if ( qglGetError() == GL_OUT_OF_MEMORY ) {
		FreeBufferObject();
		return false;
	}
	if ( r_showBuffers.GetBool() ) {
		common->Printf( "index buffer %u: %i bytes\n", apiObject, size );
	}
	return true;
}

void idIndexBuffer::FreeBufferObject() {
	if ( apiObject == 0 ) {
		return;
	}
	if ( mapped ) {
		UnmapBuffer();
	}
	if ( qglDeleteBuffersARB ) {
		qglDeleteBuffersARB( 1, &apiObject );
	}
	apiObject = 0;
	size = 0;
}

void idIndexBuffer::Update( const void *data, int updateSize ) const {
	if ( apiObject == 0 || data == NULL || updateSize < 0 || updateSize > size ) {
		common->Error( "idIndexBuffer::Update: invalid update of %i byte buffer with %i bytes", size, updateSize );
	}
	Bind();
	qglBufferSubDataARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0, (GLsizeiptrARB)updateSize, data );
}

void *idIndexBuffer::MapBuffer( bufferMapType_t mapType ) const {
	if ( apiObject == 0 || mapped ) {
		return NULL;
	}
	Bind();
	void *data = qglMapBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, mapType == BM_READ ? GL_READ_ONLY_ARB : GL_WRITE_ONLY_ARB );
	mapped = data != NULL;
	return data;
}

void idIndexBuffer::UnmapBuffer() const {
	if ( apiObject == 0 || !mapped ) {
		return;
	}
	Bind();
	qglUnmapBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB );
	mapped = false;
}

void idIndexBuffer::Bind() const {
	qglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, apiObject );
}

idJointBuffer::idJointBuffer() : numJoints( 0 ), apiObject( 0 ) {
}

idJointBuffer::~idJointBuffer() {
	FreeBufferObject();
}

bool idJointBuffer::AllocBufferObject( const float *joints, int numAllocJoints ) {
	if ( numAllocJoints <= 0 || !glConfig.gpuSkinningAvailable || !qglBindBufferBase ) {
		return false;
	}
	FreeBufferObject();
	qglGenBuffersARB( 1, &apiObject );
	if ( apiObject == 0 ) {
		return false;
	}
	numJoints = numAllocJoints;
	qglBindBufferARB( GL_UNIFORM_BUFFER, apiObject );
	qglBufferDataARB( GL_UNIFORM_BUFFER, (GLsizeiptrARB)GetAllocedSize(), joints, GL_DYNAMIC_DRAW_ARB );
	if ( qglGetError() == GL_OUT_OF_MEMORY ) {
		FreeBufferObject();
		return false;
	}
	if ( r_showBuffers.GetBool() ) {
		common->Printf( "joint buffer %u: %i joints\n", apiObject, numJoints );
	}
	return true;
}

void idJointBuffer::FreeBufferObject() {
	if ( apiObject == 0 ) {
		return;
	}
	if ( qglDeleteBuffersARB ) {
		qglDeleteBuffersARB( 1, &apiObject );
	}
	apiObject = 0;
	numJoints = 0;
}

void idJointBuffer::Update( const float *joints, int numUpdateJoints ) const {
	if ( apiObject == 0 || joints == NULL || numUpdateJoints < 0 || numUpdateJoints > numJoints ) {
		common->Error( "idJointBuffer::Update: invalid update of %i joint buffer with %i joints", numJoints, numUpdateJoints );
	}
	qglBindBufferARB( GL_UNIFORM_BUFFER, apiObject );
	qglBufferSubDataARB( GL_UNIFORM_BUFFER, 0, (GLsizeiptrARB)( numUpdateJoints * sizeof( idJointMat ) ), joints );
}

void idJointBuffer::Bind( GLuint bindingPoint ) const {
	if ( apiObject != 0 ) {
		qglBindBufferBase( GL_UNIFORM_BUFFER, bindingPoint, apiObject );
	}
}
