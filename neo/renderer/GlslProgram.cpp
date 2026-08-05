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

typedef struct {
	char	vertexName[64];
	char	fragmentName[64];
} glslShaderDef_t;

static glslShaderDef_t glslShaders[MAX_GLSL_SHADERS];
static idList<idGLSLProgram *> glslPrograms;
static idGLSLProgram *activeGLSLProgram;
static bool glslShaderDefsInitialized;

static float vertexEnvParameters[GLSL_MAX_PROGRAM_PARMS][4];
static float fragmentEnvParameters[GLSL_MAX_PROGRAM_PARMS][4];
static float vertexLocalParameters[MAX_GLSL_SHADERS][GLSL_MAX_PROGRAM_PARMS][4];
static float fragmentLocalParameters[MAX_GLSL_SHADERS][GLSL_MAX_PROGRAM_PARMS][4];

static void R_SetBuiltinGLSLShader( glslProgram_t index, const char *name ) {
	idStr::Copynz( glslShaders[index].vertexName, name, sizeof( glslShaders[index].vertexName ) );
	idStr::Copynz( glslShaders[index].fragmentName, name, sizeof( glslShaders[index].fragmentName ) );
}

static void R_InitGLSLShaderDefs() {
	if ( glslShaderDefsInitialized ) {
		return;
	}
	glslShaderDefsInitialized = true;
	memset( glslShaders, 0, sizeof( glslShaders ) );
	memset( vertexEnvParameters, 0, sizeof( vertexEnvParameters ) );
	memset( fragmentEnvParameters, 0, sizeof( fragmentEnvParameters ) );
	memset( vertexLocalParameters, 0, sizeof( vertexLocalParameters ) );
	memset( fragmentLocalParameters, 0, sizeof( fragmentLocalParameters ) );

	R_SetBuiltinGLSLShader( GLSLPROG_INTERACTION, "interaction" );
	R_SetBuiltinGLSLShader( GLSLPROG_ENVIRONMENT, "environment" );
	R_SetBuiltinGLSLShader( GLSLPROG_BUMPY_ENVIRONMENT, "bumpyEnvironment" );
	R_SetBuiltinGLSLShader( GLSLPROG_TEST, "test" );
	R_SetBuiltinGLSLShader( GLSLPROG_AMBIENT, "ambientLight" );
	R_SetBuiltinGLSLShader( GLSLPROG_GLASSWARP, "glassWarp" );
	R_SetBuiltinGLSLShader( GLSLPROG_BAKED_LIGHT, "bakedLight" );
	R_SetBuiltinGLSLShader( GLSLPROG_AMBIENT_CUBE, "ambientCube" );
	R_SetBuiltinGLSLShader( GLSLPROG_ATMOSPHERE, "atmosphere" );
	R_SetBuiltinGLSLShader( GLSLPROG_COLOR_PROCESS, "colorProcess" );
	R_SetBuiltinGLSLShader( GLSLPROG_HEAT_HAZE, "heatHaze" );
	R_SetBuiltinGLSLShader( GLSLPROG_HEAT_HAZE_WITH_MASK, "heatHazeWithMask" );
	R_SetBuiltinGLSLShader( GLSLPROG_HEAT_HAZE_WITH_MASK_AND_VERTEX, "heatHazeWithMaskAndVertex" );
	R_SetBuiltinGLSLShader( GLSLPROG_BAKED_SHADOW, "bakedShadow" );
	R_SetBuiltinGLSLShader( GLSLPROG_INTERACTION_SHADOW, "interactionShadow" );
	idStr::Copynz( glslShaders[GLSLPROG_GPU_SKINNING].vertexName, "gpuSkinning", sizeof( glslShaders[GLSLPROG_GPU_SKINNING].vertexName ) );
	idStr::Copynz( glslShaders[GLSLPROG_FOG_TEXGEN].vertexName, "fogTexgen", sizeof( glslShaders[GLSLPROG_FOG_TEXGEN].vertexName ) );
	idStr::Copynz( glslShaders[GLSLPROG_BLEND_LIGHT_TEXGEN].vertexName, "blendLightTexgen", sizeof( glslShaders[GLSLPROG_BLEND_LIGHT_TEXGEN].vertexName ) );
}

idGLSLProgram::idGLSLProgram() {
	vertexShaderIndex = GLSLPROG_INVALID;
	fragmentShaderIndex = GLSLPROG_INVALID;
	program = 0;
	gpuSkinningLocation = -1;
	memset( vertexEnvLocations, -1, sizeof( vertexEnvLocations ) );
	memset( fragmentEnvLocations, -1, sizeof( fragmentEnvLocations ) );
	memset( vertexLocalLocations, -1, sizeof( vertexLocalLocations ) );
	memset( fragmentLocalLocations, -1, sizeof( fragmentLocalLocations ) );
}

idGLSLProgram::~idGLSLProgram() {
	Purge();
}

void idGLSLProgram::Init( int vertexIndex, const char *vertexName, int fragmentIndex, const char *fragmentName ) {
	vertexShaderIndex = vertexIndex;
	fragmentShaderIndex = fragmentIndex;
	vertexShaderName = vertexName;
	fragmentShaderName = fragmentName;
	if ( !vertexShaderName.Icmp( fragmentShaderName ) ) {
		name = vertexShaderName;
	} else {
		name = vertexShaderName;
		name += "/";
		name += fragmentShaderName;
	}
}

void idGLSLProgram::Purge() {
	if ( program && glConfig.isInitialized && qglDeleteProgram ) {
		qglDeleteProgram( program );
	}
	program = 0;
	memset( vertexEnvLocations, -1, sizeof( vertexEnvLocations ) );
	memset( fragmentEnvLocations, -1, sizeof( fragmentEnvLocations ) );
	memset( vertexLocalLocations, -1, sizeof( vertexLocalLocations ) );
	memset( fragmentLocalLocations, -1, sizeof( fragmentLocalLocations ) );
	gpuSkinningLocation = -1;
}

GLuint idGLSLProgram::CompileShader( GLenum type, const char *shaderName, const char *extension ) {
	idStr path = "glprogs/";
	path += shaderName;
	path += extension;

	char *source = NULL;
	fileSystem->ReadFile( path.c_str(), (void **)&source, NULL );
	common->Printf( "%s", path.c_str() );
	if ( !source ) {
		common->Printf( ": File not found\n" );
		return 0;
	}

	idStr sourceText = source;
	fileSystem->FreeFile( source );

	// Every GLSL vertex program receives the BFG four-weight skinning path.
	// Static surfaces leave u_gpuSkinning disabled, while MD5 surfaces bind
	// their per-mesh joint UBO and packed joint stream immediately before draw.
	if ( type == GL_VERTEX_SHADER && glConfig.gpuSkinningAvailable ) {
		if ( r_glIntelDriverHack.GetBool() ) {
			// layout(std140)/UBO grammar is native to GLSL 140+; Intel's compiler
			// rejects it under #version 120 even with GL_ARB_uniform_buffer_object
			// enabled. Bump the version and restore deprecated built-ins via
			// GL_ARB_compatibility so gl_Vertex/attribute/ftransform() still work.
			sourceText.Replace( "#version 120", "#version 140" );
		}

		sourceText.Replace( "attribute vec3 attr_Tangent;\r\n", "" );
		sourceText.Replace( "attribute vec3 attr_Tangent;\n", "" );
		sourceText.Replace( "attribute vec3 attr_Bitangent;\r\n", "" );
		sourceText.Replace( "attribute vec3 attr_Bitangent;\n", "" );
		sourceText.Replace( "attribute vec3 attr_Normal;\r\n", "" );
		sourceText.Replace( "attribute vec3 attr_Normal;\n", "" );
		// Keep the original ftransform assignment byte-for-byte in the static path.
		// The baked and realtime light passes use GL_EQUAL against a fixed-function
		// depth prepass, so even wrapping ftransform in a helper can lose the GLSL
		// invariance guarantee on some drivers.  Skinned draws override the result.
		sourceText.Replace( "gl_Position = ftransform();",
			"gl_Position = ftransform();\n\tif ( u_gpuSkinning ) {\n\t\tgl_Position = gl_ModelViewProjectionMatrix * gpuSkinnedPosition();\n\t}" );
		sourceText.Replace( "gl_Vertex", "gpuSkinnedPosition()" );
		sourceText.Replace( "gl_Normal", "gpuSkinnedNormal()" );
		sourceText.Replace( "attr_Tangent", "gpuSkinnedTangent()" );
		sourceText.Replace( "attr_Bitangent", "gpuSkinnedBitangent()" );
		sourceText.Replace( "attr_Normal", "gpuSkinnedNormal()" );

		char *skinningSource = NULL;
		fileSystem->ReadFile( "glprogs/skinning.inc", (void **)&skinningSource, NULL );
		if ( skinningSource == NULL ) {
			common->Printf( ": glprogs/skinning.inc not found\n" );
			return 0;
		}
		const int versionEnd = sourceText.Find( '\n' );
		if ( versionEnd < 0 ) {
			fileSystem->FreeFile( skinningSource );
			common->Printf( ": missing #version line\n" );
			return 0;
		}

		idStr injected;
		if ( r_glIntelDriverHack.GetBool() ) {
			injected = "\n#extension GL_ARB_compatibility : enable\n";
		} else {
			injected = "\n";
		}
		injected += skinningSource;
		injected += "\n";
		sourceText.Insert( injected.c_str(), versionEnd + 1 );
		fileSystem->FreeFile( skinningSource );
	}

	GLuint shader = qglCreateShader( type );
	const char *sources[1] = { sourceText.c_str() };
	qglShaderSource( shader, 1, sources, NULL );
	qglCompileShader( shader );

	GLint compiled = 0;
	qglGetShaderiv( shader, GL_COMPILE_STATUS, &compiled );
	if ( !compiled ) {
		char log[8192];
		GLsizei length = 0;
		qglGetShaderInfoLog( shader, sizeof( log ) - 1, &length, log );
		log[idMath::ClampInt( 0, sizeof( log ) - 1, length )] = '\0';
		common->Printf( ": compile failed\n%s\n", log );
		qglDeleteShader( shader );
		return 0;
	}

	common->Printf( "\n" );
	return shader;
}

bool idGLSLProgram::Reload() {
	Purge();
	if ( !glConfig.glslAvailable ) {
		return false;
	}

	GLuint vertexShader = CompileShader( GL_VERTEX_SHADER, vertexShaderName.c_str(), ".vert" );
	if ( !vertexShader ) {
		return false;
	}
	GLuint fragmentShader = 0;
	if ( !fragmentShaderName.IsEmpty() ) {
		fragmentShader = CompileShader( GL_FRAGMENT_SHADER, fragmentShaderName.c_str(), ".frag" );
		if ( !fragmentShader ) {
			qglDeleteShader( vertexShader );
			return false;
		}
	}

	program = qglCreateProgram();
	qglAttachShader( program, vertexShader );
	if ( fragmentShader != 0 ) {
		qglAttachShader( program, fragmentShader );
	}

	// Match the generic attribute slots used by idDrawVert throughout the renderer.
	qglBindAttribLocation( program, 8, "attr_TexCoord" );
	qglBindAttribLocation( program, 9, "attr_Tangent" );
	qglBindAttribLocation( program, 10, "attr_Bitangent" );
	qglBindAttribLocation( program, 11, "attr_Normal" );
	qglBindAttribLocation( program, 12, "attr_LightCoord" );
	qglBindAttribLocation( program, 13, "attr_JointIndices" );
	qglBindAttribLocation( program, 14, "attr_JointWeights" );

	qglLinkProgram( program );
	qglDeleteShader( vertexShader );
	if ( fragmentShader != 0 ) {
		qglDeleteShader( fragmentShader );
	}

	GLint linked = 0;
	qglGetProgramiv( program, GL_LINK_STATUS, &linked );
	if ( !linked ) {
		char log[8192];
		GLsizei length = 0;
		qglGetProgramInfoLog( program, sizeof( log ) - 1, &length, log );
		log[idMath::ClampInt( 0, sizeof( log ) - 1, length )] = '\0';
		common->Printf( "GLSL program %s: link failed\n%s\n", name.c_str(), log );
		qglDeleteProgram( program );
		program = 0;
		return false;
	}

	FindUniformLocations();
	if ( glConfig.gpuSkinningAvailable ) {
		const GLuint blockIndex = qglGetUniformBlockIndex( program, "matrices_ubo" );
		if ( blockIndex != GL_INVALID_INDEX ) {
			qglUniformBlockBinding( program, blockIndex, 0 );
		}
	}
	SetSamplerUniforms();
	return true;
}

void idGLSLProgram::FindUniformLocations() {
	for ( int i = 0; i < GLSL_MAX_PROGRAM_PARMS; i++ ) {
		vertexEnvLocations[i] = qglGetUniformLocation( program, va( "u_vertexParm[%i]", i ) );
		fragmentEnvLocations[i] = qglGetUniformLocation( program, va( "u_fragmentParm[%i]", i ) );
		vertexLocalLocations[i] = qglGetUniformLocation( program, va( "u_vertexLocalParm[%i]", i ) );
		fragmentLocalLocations[i] = qglGetUniformLocation( program, va( "u_fragmentLocalParm[%i]", i ) );
	}
	gpuSkinningLocation = qglGetUniformLocation( program, "u_gpuSkinning" );
}

void idGLSLProgram::SetSamplerUniforms() {
	qglUseProgram( program );
	for ( int i = 0; i < MAX_MULTITEXTURE_UNITS; i++ ) {
		GLint location = qglGetUniformLocation( program, va( "u_texture%i", i ) );
		if ( location >= 0 ) {
			qglUniform1i( location, i );
		}
	}
	qglUseProgram( 0 );
}

void idGLSLProgram::Bind() {
	qglUseProgram( program );
	const int fragmentParameterIndex = fragmentShaderIndex == GLSLPROG_INVALID ? GLSLPROG_INVALID : fragmentShaderIndex;
	UploadParameters( vertexEnvParameters, fragmentEnvParameters,
		vertexLocalParameters[vertexShaderIndex], fragmentLocalParameters[fragmentParameterIndex] );
	SetGPUSkinning( false );
}

void idGLSLProgram::SetGPUSkinning( bool enabled ) const {
	if ( gpuSkinningLocation >= 0 ) {
		qglUniform1i( gpuSkinningLocation, enabled ? 1 : 0 );
	}
}

void idGLSLProgram::SetVertexEnvParameter( int index, const float *value ) const {
	if ( vertexEnvLocations[index] >= 0 ) {
		qglUniform4fv( vertexEnvLocations[index], 1, value );
	}
}

void idGLSLProgram::SetFragmentEnvParameter( int index, const float *value ) const {
	if ( fragmentEnvLocations[index] >= 0 ) {
		qglUniform4fv( fragmentEnvLocations[index], 1, value );
	}
}

void idGLSLProgram::SetVertexLocalParameter( int index, const float *value ) const {
	if ( vertexLocalLocations[index] >= 0 ) {
		qglUniform4fv( vertexLocalLocations[index], 1, value );
	}
}

void idGLSLProgram::SetFragmentLocalParameter( int index, const float *value ) const {
	if ( fragmentLocalLocations[index] >= 0 ) {
		qglUniform4fv( fragmentLocalLocations[index], 1, value );
	}
}

void idGLSLProgram::UploadParameters( const float vertexEnv[][4], const float fragmentEnv[][4],
		const float vertexLocal[][4], const float fragmentLocal[][4] ) const {
	for ( int i = 0; i < GLSL_MAX_PROGRAM_PARMS; i++ ) {
		SetVertexEnvParameter( i, vertexEnv[i] );
		SetFragmentEnvParameter( i, fragmentEnv[i] );
		SetVertexLocalParameter( i, vertexLocal[i] );
		SetFragmentLocalParameter( i, fragmentLocal[i] );
	}
}

static idGLSLProgram *R_FindLinkedGLSLProgram( int vertexShader, int fragmentShader, bool create ) {
	for ( int i = 0; i < glslPrograms.Num(); i++ ) {
		idGLSLProgram *program = glslPrograms[i];
		if ( program->GetVertexShaderIndex() == vertexShader && program->GetFragmentShaderIndex() == fragmentShader ) {
			return program;
		}
	}

	if ( !create || vertexShader <= GLSLPROG_INVALID || vertexShader >= MAX_GLSL_SHADERS ||
		 !glslShaders[vertexShader].vertexName[0] ) {
		return NULL;
	}
	if ( fragmentShader != GLSLPROG_INVALID &&
		( fragmentShader <= GLSLPROG_INVALID || fragmentShader >= MAX_GLSL_SHADERS || !glslShaders[fragmentShader].fragmentName[0] ) ) {
		return NULL;
	}

	idGLSLProgram *program = new idGLSLProgram;
	program->Init( vertexShader, glslShaders[vertexShader].vertexName, fragmentShader,
		fragmentShader == GLSLPROG_INVALID ? "" : glslShaders[fragmentShader].fragmentName );
	glslPrograms.Append( program );
	return program;
}

void R_GLSL_Init( void ) {
	R_InitGLSLShaderDefs();
	glConfig.allowGLSLPath = false;
	common->Printf( "---------- R_GLSL_Init ----------\n" );
	if ( !glConfig.glslAvailable ) {
		common->Printf( "Not available. OpenGL 2.0 or newer is required.\n" );
		return;
	}
	common->Printf( "Available.\n" );
	common->Printf( "---------------------------------\n" );
	glConfig.allowGLSLPath = true;
}

void R_ShutdownGLSLPrograms( void ) {
	R_UnbindGLSLProgram();
	for ( int i = 0; i < glslPrograms.Num(); i++ ) {
		glslPrograms[i]->Purge();
	}
}

int R_FindGLSLShader( GLenum target, const char *shaderName ) {
	R_InitGLSLShaderDefs();
	idStr stripped = shaderName;
	stripped.StripPath();
	stripped.StripFileExtension();

	for ( int i = 1; i < MAX_GLSL_SHADERS; i++ ) {
		const char *name = target == GL_VERTEX_SHADER ? glslShaders[i].vertexName : glslShaders[i].fragmentName;
		if ( name[0] && !idStr::Icmp( stripped.c_str(), name ) ) {
			return i;
		}
	}

	// A combined material program is registered one stage at a time.  Reuse the
	// opposite-stage slot when its base name matches so "program foo" gets one id.
	for ( int i = GLSLPROG_USER; i < MAX_GLSL_SHADERS; i++ ) {
		const char *otherName = target == GL_VERTEX_SHADER ? glslShaders[i].fragmentName : glslShaders[i].vertexName;
		if ( otherName[0] && !idStr::Icmp( stripped.c_str(), otherName ) ) {
			char *name = target == GL_VERTEX_SHADER ? glslShaders[i].vertexName : glslShaders[i].fragmentName;
			idStr::Copynz( name, stripped.c_str(), 64 );
			return i;
		}
	}

	for ( int i = GLSLPROG_USER; i < MAX_GLSL_SHADERS; i++ ) {
		if ( !glslShaders[i].vertexName[0] && !glslShaders[i].fragmentName[0] ) {
			char *name = target == GL_VERTEX_SHADER ? glslShaders[i].vertexName : glslShaders[i].fragmentName;
			idStr::Copynz( name, stripped.c_str(), 64 );
			return i;
		}
	}

	common->Error( "R_FindGLSLShader: MAX_GLSL_SHADERS" );
	return GLSLPROG_INVALID;
}

bool R_BindGLSLProgram( int vertexShader, int fragmentShader ) {
	idGLSLProgram *program = R_FindLinkedGLSLProgram( vertexShader, fragmentShader, true );
	if ( !program ) {
		R_UnbindGLSLProgram();
		return false;
	}
	if ( !program->IsLoaded() && !program->Reload() ) {
		R_UnbindGLSLProgram();
		return false;
	}
	activeGLSLProgram = program;
	program->Bind();
	return true;
}

bool R_BindGLSLProgram( int program ) {
	return R_BindGLSLProgram( program, program );
}

bool R_BindGLSLVertexProgram( int vertexShader ) {
	return R_BindGLSLProgram( vertexShader, GLSLPROG_INVALID );
}

void R_UnbindGLSLProgram( void ) {
	activeGLSLProgram = NULL;
	if ( glConfig.isInitialized && qglUseProgram ) {
		qglUseProgram( 0 );
	}
}

bool R_IsGLSLProgramBound( void ) {
	return activeGLSLProgram != NULL;
}

void R_SetGLSLGPUSkinning( bool enabled ) {
	if ( activeGLSLProgram != NULL ) {
		activeGLSLProgram->SetGPUSkinning( enabled );
	}
}

void R_SetGLSLProgramEnvParameter( GLenum target, int index, const float *value ) {
	if ( index < 0 || index >= GLSL_MAX_PROGRAM_PARMS ) {
		common->Error( "R_SetGLSLProgramEnvParameter: bad index %i", index );
	}
	float (*parameters)[4] = target == GL_VERTEX_SHADER ? vertexEnvParameters : fragmentEnvParameters;
	memcpy( parameters[index], value, sizeof( parameters[index] ) );
	if ( activeGLSLProgram ) {
		if ( target == GL_VERTEX_SHADER ) {
			activeGLSLProgram->SetVertexEnvParameter( index, value );
		} else {
			activeGLSLProgram->SetFragmentEnvParameter( index, value );
		}
	}
}

void R_SetGLSLProgramLocalParameter( GLenum target, int index, const float *value ) {
	if ( !activeGLSLProgram ) {
		return;
	}
	if ( index < 0 || index >= GLSL_MAX_PROGRAM_PARMS ) {
		common->Error( "R_SetGLSLProgramLocalParameter: bad index %i", index );
	}
	if ( target == GL_VERTEX_SHADER ) {
		memcpy( vertexLocalParameters[activeGLSLProgram->GetVertexShaderIndex()][index], value, sizeof( float ) * 4 );
		activeGLSLProgram->SetVertexLocalParameter( index, value );
	} else {
		memcpy( fragmentLocalParameters[activeGLSLProgram->GetFragmentShaderIndex()][index], value, sizeof( float ) * 4 );
		activeGLSLProgram->SetFragmentLocalParameter( index, value );
	}
}

void R_ReloadGLSLPrograms_f( const idCmdArgs &args ) {
	R_InitGLSLShaderDefs();
	common->Printf( "----- R_ReloadGLSLPrograms -----\n" );
	R_UnbindGLSLProgram();

	for ( int i = GLSLPROG_INTERACTION; i < GLSLPROG_USER; i++ ) {
		const bool vertexOnly = i == GLSLPROG_GPU_SKINNING || i == GLSLPROG_FOG_TEXGEN ||
			i == GLSLPROG_BLEND_LIGHT_TEXGEN;
		R_FindLinkedGLSLProgram( i, vertexOnly ? GLSLPROG_INVALID : i, true );
	}

	bool builtinsLoaded = glConfig.glslAvailable;
	for ( int i = 0; i < glslPrograms.Num(); i++ ) {
		bool loaded = glslPrograms[i]->Reload();
		if ( glslPrograms[i]->GetVertexShaderIndex() < GLSLPROG_USER && !loaded ) {
			builtinsLoaded = false;
		}
	}
	glConfig.allowGLSLPath = builtinsLoaded;
	common->Printf( "--------------------------------\n" );
}
