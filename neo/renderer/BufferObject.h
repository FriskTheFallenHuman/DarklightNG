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
#ifndef __BUFFEROBJECT_H__
#define __BUFFEROBJECT_H__

enum bufferMapType_t {
	BM_READ,
	BM_WRITE
};

void UnbindBufferObjects();

class idVertexBuffer {
public:
					idVertexBuffer();
					~idVertexBuffer();

	bool			AllocBufferObject( const void *data, int allocSize, GLenum usage = GL_STATIC_DRAW_ARB );
	void			FreeBufferObject();
	void			Update( const void *data, int updateSize ) const;
	void *			MapBuffer( bufferMapType_t mapType ) const;
	void			UnmapBuffer() const;
	void			Bind() const;

	bool			IsAllocated() const { return apiObject != 0; }
	bool			IsMapped() const { return mapped; }
	int				GetSize() const { return size; }
	GLuint			GetAPIObject() const { return apiObject; }

private:
	int				size;
	GLuint			apiObject;
	mutable bool	mapped;

	idVertexBuffer( const idVertexBuffer & );
	idVertexBuffer &operator=( const idVertexBuffer & );
};

class idIndexBuffer {
public:
					idIndexBuffer();
					~idIndexBuffer();

	bool			AllocBufferObject( const void *data, int allocSize, GLenum usage = GL_STATIC_DRAW_ARB );
	void			FreeBufferObject();
	void			Update( const void *data, int updateSize ) const;
	void *			MapBuffer( bufferMapType_t mapType ) const;
	void			UnmapBuffer() const;
	void			Bind() const;

	bool			IsAllocated() const { return apiObject != 0; }
	bool			IsMapped() const { return mapped; }
	int				GetSize() const { return size; }
	GLuint			GetAPIObject() const { return apiObject; }

private:
	int				size;
	GLuint			apiObject;
	mutable bool	mapped;

	idIndexBuffer( const idIndexBuffer & );
	idIndexBuffer &operator=( const idIndexBuffer & );
};

/*
================================================
idJointBuffer

Each animated mesh owns one joint UBO. Matrices are three vec4 rows, matching
idJointMat and the BFG render-program layout.
================================================
*/
class idJointBuffer {
public:
					idJointBuffer();
					~idJointBuffer();

	bool			AllocBufferObject( const float *joints, int numAllocJoints );
	void			FreeBufferObject();
	void			Update( const float *joints, int numUpdateJoints ) const;
	void			Bind( GLuint bindingPoint = 0 ) const;

	bool			IsAllocated() const { return apiObject != 0; }
	int				GetNumJoints() const { return numJoints; }
	int				GetAllocedSize() const { return numJoints * 3 * 4 * sizeof( float ); }
	GLuint			GetAPIObject() const { return apiObject; }

private:
	int				numJoints;
	GLuint			apiObject;

	idJointBuffer( const idJointBuffer & );
	idJointBuffer &operator=( const idJointBuffer & );
};

#endif
