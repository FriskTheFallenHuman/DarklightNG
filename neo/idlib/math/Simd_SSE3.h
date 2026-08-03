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

#ifndef __MATH_SIMD_SSE3_H__
#define __MATH_SIMD_SSE3_H__

/*
===============================================================================

	SSE3 implementation of idSIMDProcessor

===============================================================================
*/

class idSIMD_SSE3 : public idSIMD_SSE2 {
public:
#if defined(MACOS_X) && defined(__i386__)
	virtual const char * VPCALL GetName( void ) const;

#elif defined(_WIN32)
	virtual const char * VPCALL GetName( void ) const;

	virtual void VPCALL TransformVerts( idDrawVert *verts, const int numVerts, const idJointMat *joints, const idVec4 *weights, const int *index, const int numWeights );

#endif
};

#endif /* !__MATH_SIMD_SSE3_H__ */
