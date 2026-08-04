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

#ifndef __GLSLPROGRAM_H__
#define __GLSLPROGRAM_H__

const int GLSL_MAX_PROGRAM_PARMS = 32;
const int MAX_GLSL_SHADERS = 200;

typedef enum {
	GLSLPROG_INVALID,
	GLSLPROG_INTERACTION,
	GLSLPROG_ENVIRONMENT,
	GLSLPROG_BUMPY_ENVIRONMENT,
	GLSLPROG_TEST,
	GLSLPROG_AMBIENT,
	GLSLPROG_GLASSWARP,
	GLSLPROG_BAKED_LIGHT,
	GLSLPROG_AMBIENT_CUBE,
	GLSLPROG_COLOR_PROCESS,
	GLSLPROG_HEAT_HAZE,
	GLSLPROG_HEAT_HAZE_WITH_MASK,
	GLSLPROG_HEAT_HAZE_WITH_MASK_AND_VERTEX,
	GLSLPROG_BAKED_SHADOW,
	GLSLPROG_INTERACTION_SHADOW,
	GLSLPROG_GPU_SKINNING,
	GLSLPROG_FOG_TEXGEN,
	GLSLPROG_BLEND_LIGHT_TEXGEN,
	GLSLPROG_USER
} glslProgram_t;

class idGLSLProgram {
public:
					idGLSLProgram();
					~idGLSLProgram();

	void			Init( int vertexShaderIndex, const char *vertexShaderName,
						int fragmentShaderIndex, const char *fragmentShaderName );
	bool			Reload();
	void			Purge();
	void			Bind();
	bool			IsLoaded() const { return program != 0; }

	int				GetVertexShaderIndex() const { return vertexShaderIndex; }
	int				GetFragmentShaderIndex() const { return fragmentShaderIndex; }
	const char *	GetName() const { return name.c_str(); }

	void			SetVertexEnvParameter( int index, const float *value ) const;
	void			SetFragmentEnvParameter( int index, const float *value ) const;
	void			SetVertexLocalParameter( int index, const float *value ) const;
	void			SetFragmentLocalParameter( int index, const float *value ) const;
	void			SetGPUSkinning( bool enabled ) const;
	void			UploadParameters( const float vertexEnv[][4], const float fragmentEnv[][4],
						const float vertexLocal[][4], const float fragmentLocal[][4] ) const;

private:
	GLuint			CompileShader( GLenum type, const char *shaderName, const char *extension );
	void			FindUniformLocations();
	void			SetSamplerUniforms();

	idStr			name;
	idStr			vertexShaderName;
	idStr			fragmentShaderName;
	int				vertexShaderIndex;
	int				fragmentShaderIndex;
	GLuint			program;
	GLint			vertexEnvLocations[GLSL_MAX_PROGRAM_PARMS];
	GLint			fragmentEnvLocations[GLSL_MAX_PROGRAM_PARMS];
	GLint			vertexLocalLocations[GLSL_MAX_PROGRAM_PARMS];
	GLint			fragmentLocalLocations[GLSL_MAX_PROGRAM_PARMS];
	GLint			gpuSkinningLocation;
};

void	R_GLSL_Init( void );
void	R_ShutdownGLSLPrograms( void );
void	R_ReloadGLSLPrograms_f( const idCmdArgs &args );
int		R_FindGLSLShader( GLenum target, const char *shaderName );
bool	R_BindGLSLProgram( int vertexShader, int fragmentShader );
bool	R_BindGLSLProgram( int program );
bool	R_BindGLSLVertexProgram( int vertexShader );
void	R_UnbindGLSLProgram( void );
bool	R_IsGLSLProgramBound( void );
void	R_SetGLSLGPUSkinning( bool enabled );
void	R_SetGLSLProgramEnvParameter( GLenum target, int index, const float *value );
void	R_SetGLSLProgramLocalParameter( GLenum target, int index, const float *value );

#endif
