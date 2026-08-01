/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

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
	GLSLPROG_COLOR_PROCESS,
	GLSLPROG_HEAT_HAZE,
	GLSLPROG_HEAT_HAZE_WITH_MASK,
	GLSLPROG_HEAT_HAZE_WITH_MASK_AND_VERTEX,
	GLSLPROG_BAKED_SHADOW,
	GLSLPROG_INTERACTION_SHADOW,
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
};

void	R_GLSL_Init( void );
void	R_ShutdownGLSLPrograms( void );
void	R_ReloadGLSLPrograms_f( const idCmdArgs &args );
int		R_FindGLSLShader( GLenum target, const char *shaderName );
bool	R_BindGLSLProgram( int vertexShader, int fragmentShader );
bool	R_BindGLSLProgram( int program );
void	R_UnbindGLSLProgram( void );
void	R_SetGLSLProgramEnvParameter( GLenum target, int index, const float *value );
void	R_SetGLSLProgramLocalParameter( GLenum target, int index, const float *value );

#endif
