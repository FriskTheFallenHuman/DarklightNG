/*
===========================================================================

DarklightNG Source Code
Copyright (C) 2026 - Justin Marshall (aka IceColdDuke).

SDL2 OpenGL platform implementation.

===========================================================================
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "sys_platform.h"
#include "rc/doom_resource.h"
#include "../renderer/tr_local.h"

#include <SDL_syswm.h>

PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB;
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
PFNWGLGETPIXELFORMATATTRIBIVARBPROC wglGetPixelFormatAttribivARB;
PFNWGLGETPIXELFORMATATTRIBFVARBPROC wglGetPixelFormatAttribfvARB;
PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
PFNWGLCREATEPBUFFERARBPROC wglCreatePbufferARB;
PFNWGLGETPBUFFERDCARBPROC wglGetPbufferDCARB;
PFNWGLRELEASEPBUFFERDCARBPROC wglReleasePbufferDCARB;
PFNWGLDESTROYPBUFFERARBPROC wglDestroyPbufferARB;
PFNWGLQUERYPBUFFERARBPROC wglQueryPbufferARB;
PFNWGLBINDTEXIMAGEARBPROC wglBindTexImageARB;
PFNWGLRELEASETEXIMAGEARBPROC wglReleaseTexImageARB;
PFNWGLSETPBUFFERATTRIBARBPROC wglSetPbufferAttribARB;

static bool gammaSaved;

bool QGL_Init( const char *dllName );
void QGL_Shutdown( void );

static void GLimp_UpdateNativeHandles() {
	win32.hWnd = NULL;
	if ( win32.sdlWindow != NULL ) {
		SDL_SysWMinfo windowInfo;
		SDL_VERSION( &windowInfo.version );
		if ( SDL_GetWindowWMInfo( win32.sdlWindow, &windowInfo ) == SDL_TRUE && windowInfo.subsystem == SDL_SYSWM_WINDOWS ) {
			win32.hWnd = windowInfo.info.win.window;
		}
	}

	win32.hDC = wglGetCurrentDC();
	win32.hGLRC = wglGetCurrentContext();
	win32.pixelformat = win32.hDC != NULL ? GetPixelFormat( win32.hDC ) : 0;
	memset( &win32.pfd, 0, sizeof( win32.pfd ) );
	if ( win32.hDC != NULL && win32.pixelformat > 0 ) {
		DescribePixelFormat( win32.hDC, win32.pixelformat, sizeof( win32.pfd ), &win32.pfd );
	}
}

static void GLimp_BindWGLExtensions() {
	wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)SDL_GL_GetProcAddress( "wglGetExtensionsStringARB" );
	wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)SDL_GL_GetProcAddress( "wglSwapIntervalEXT" );
	wglGetPixelFormatAttribivARB = (PFNWGLGETPIXELFORMATATTRIBIVARBPROC)SDL_GL_GetProcAddress( "wglGetPixelFormatAttribivARB" );
	wglGetPixelFormatAttribfvARB = (PFNWGLGETPIXELFORMATATTRIBFVARBPROC)SDL_GL_GetProcAddress( "wglGetPixelFormatAttribfvARB" );
	wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)SDL_GL_GetProcAddress( "wglChoosePixelFormatARB" );
	wglCreatePbufferARB = (PFNWGLCREATEPBUFFERARBPROC)SDL_GL_GetProcAddress( "wglCreatePbufferARB" );
	wglGetPbufferDCARB = (PFNWGLGETPBUFFERDCARBPROC)SDL_GL_GetProcAddress( "wglGetPbufferDCARB" );
	wglReleasePbufferDCARB = (PFNWGLRELEASEPBUFFERDCARBPROC)SDL_GL_GetProcAddress( "wglReleasePbufferDCARB" );
	wglDestroyPbufferARB = (PFNWGLDESTROYPBUFFERARBPROC)SDL_GL_GetProcAddress( "wglDestroyPbufferARB" );
	wglQueryPbufferARB = (PFNWGLQUERYPBUFFERARBPROC)SDL_GL_GetProcAddress( "wglQueryPbufferARB" );
	wglBindTexImageARB = (PFNWGLBINDTEXIMAGEARBPROC)SDL_GL_GetProcAddress( "wglBindTexImageARB" );
	wglReleaseTexImageARB = (PFNWGLRELEASETEXIMAGEARBPROC)SDL_GL_GetProcAddress( "wglReleaseTexImageARB" );
	wglSetPbufferAttribARB = (PFNWGLSETPBUFFERATTRIBARBPROC)SDL_GL_GetProcAddress( "wglSetPbufferAttribARB" );

	if ( wglGetExtensionsStringARB != NULL && win32.hDC != NULL ) {
		glConfig.wgl_extensions_string = wglGetExtensionsStringARB( win32.hDC );
	} else {
		glConfig.wgl_extensions_string = "";
	}
}

static void GLimp_UpdateDisplayConfig() {
	if ( win32.sdlWindow == NULL ) {
		return;
	}
	SDL_GL_GetDrawableSize( win32.sdlWindow, &glConfig.vidWidth, &glConfig.vidHeight );
	glConfig.isFullscreen = ( SDL_GetWindowFlags( win32.sdlWindow ) & SDL_WINDOW_FULLSCREEN ) != 0;

	SDL_DisplayMode mode;
	if ( SDL_GetWindowDisplayMode( win32.sdlWindow, &mode ) == 0 ) {
		glConfig.displayFrequency = mode.refresh_rate;
	}
}

static void GLimp_SaveGamma() {
	if ( win32.sdlWindow != NULL && SDL_GetWindowGammaRamp( win32.sdlWindow,
		win32.oldHardwareGamma[0], win32.oldHardwareGamma[1], win32.oldHardwareGamma[2] ) == 0 ) {
		gammaSaved = true;
	}
}

static void GLimp_RestoreGamma() {
	if ( gammaSaved && win32.sdlWindow != NULL ) {
		SDL_SetWindowGammaRamp( win32.sdlWindow, win32.oldHardwareGamma[0],
			win32.oldHardwareGamma[1], win32.oldHardwareGamma[2] );
	}
	gammaSaved = false;
}

bool GLimp_Init( glimpParms_t parms ) {
	common->Printf( "Initializing SDL2 OpenGL subsystem\n" );

	if ( ( SDL_WasInit( SDL_INIT_VIDEO ) & SDL_INIT_VIDEO ) == 0 ) {
		common->Warning( "GLimp_Init called before SDL video initialization" );
		return false;
	}

	const char *driverName = r_glDriver.GetString()[0] ? r_glDriver.GetString() : NULL;
	if ( !QGL_Init( driverName ) ) {
		common->Printf( "^3GLimp_Init() could not load r_glDriver \"%s\"^0\n",
			driverName != NULL ? driverName : "system default" );
		return false;
	}

	SDL_DisplayMode desktopMode;
	if ( SDL_GetDesktopDisplayMode( 0, &desktopMode ) == 0 ) {
		win32.desktopWidth = desktopMode.w;
		win32.desktopHeight = desktopMode.h;
		win32.desktopBitsPixel = SDL_BITSPERPIXEL( desktopMode.format );
	}

	Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
	if ( parms.fullScreen ) {
		windowFlags |= SDL_WINDOW_FULLSCREEN;
	}

	int colorBits = 24;
	int depthBits = 24;
	int stencilBits = 8;
	for ( int attempt = 0; attempt < 16 && win32.sdlWindow == NULL; ++attempt ) {
		if ( attempt > 0 && attempt % 4 == 0 ) {
			switch ( attempt / 4 ) {
			case 1: depthBits = depthBits == 24 ? 16 : 8; break;
			case 2: colorBits = 16; break;
			case 3: stencilBits = 0; break;
			}
		}

		int testColorBits = colorBits;
		int testDepthBits = depthBits;
		int testStencilBits = stencilBits;
		if ( attempt % 4 == 1 ) testStencilBits = 0;
		if ( attempt % 4 == 2 ) testDepthBits = testDepthBits == 24 ? 16 : 8;
		if ( attempt % 4 == 3 ) testColorBits = 16;
		const int channelBits = testColorBits == 24 ? 8 : 4;

		SDL_GL_ResetAttributes();
		SDL_GL_SetAttribute( SDL_GL_RED_SIZE, channelBits );
		SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, channelBits );
		SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, channelBits );
		SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, channelBits );
		SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
		SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, testDepthBits );
		SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, testStencilBits );
		SDL_GL_SetAttribute( SDL_GL_STEREO, parms.stereo ? 1 : 0 );
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, parms.multiSamples > 1 ? 1 : 0 );
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, parms.multiSamples > 1 ? parms.multiSamples : 0 );

		const int x = parms.fullScreen ? SDL_WINDOWPOS_CENTERED : win32.win_xpos.GetInteger();
		const int y = parms.fullScreen ? SDL_WINDOWPOS_CENTERED : win32.win_ypos.GetInteger();
		win32.sdlWindow = SDL_CreateWindow( GAME_NAME, x, y, parms.width, parms.height, windowFlags );
		if ( win32.sdlWindow == NULL ) {
			common->DPrintf( "SDL_CreateWindow failed for %d/%d/%d: %s\n",
				testColorBits, testDepthBits, testStencilBits, SDL_GetError() );
			continue;
		}

		if ( parms.fullScreen && parms.displayHz > 0 ) {
			SDL_DisplayMode requestedMode = {};
			requestedMode.w = parms.width;
			requestedMode.h = parms.height;
			requestedMode.refresh_rate = parms.displayHz;
			SDL_SetWindowDisplayMode( win32.sdlWindow, &requestedMode );
		}

		win32.sdlGLContext = SDL_GL_CreateContext( win32.sdlWindow );
		if ( win32.sdlGLContext == NULL ) {
			common->DPrintf( "SDL_GL_CreateContext failed: %s\n", SDL_GetError() );
			SDL_DestroyWindow( win32.sdlWindow );
			win32.sdlWindow = NULL;
			continue;
		}

		glConfig.colorBits = testColorBits;
		glConfig.depthBits = testDepthBits;
		glConfig.stencilBits = testStencilBits;
		common->Printf( "Using %d color bits, %d depth, %d stencil display\n",
			testColorBits, testDepthBits, testStencilBits );
	}

	if ( win32.sdlWindow == NULL || win32.sdlGLContext == NULL ) {
		common->Warning( "No usable SDL2 OpenGL mode found: %s", SDL_GetError() );
		QGL_Shutdown();
		return false;
	}

	SDL_GL_MakeCurrent( win32.sdlWindow, win32.sdlGLContext );
	SDL_GL_SetSwapInterval( r_swapInterval.GetInteger() );
	r_swapInterval.ClearModified();

	GLimp_UpdateNativeHandles();
	GLimp_BindWGLExtensions();
	GLimp_UpdateDisplayConfig();
	GLimp_SaveGamma();

	win32.activeApp = true;
	win32.mouseGrabbed = false;
	win32.cdsFullscreen = glConfig.isFullscreen;
	SDL_EventState( SDL_SYSWMEVENT, SDL_ENABLE );
	return true;
}

bool GLimp_SetScreenParms( glimpParms_t parms ) {
	if ( win32.sdlWindow == NULL ) {
		return false;
	}

	GLimp_GrabInput( false );
	if ( SDL_SetWindowFullscreen( win32.sdlWindow, 0 ) != 0 ) {
		common->Warning( "SDL_SetWindowFullscreen(windowed) failed: %s", SDL_GetError() );
		return false;
	}

	SDL_SetWindowSize( win32.sdlWindow, parms.width, parms.height );
	if ( parms.fullScreen ) {
		SDL_DisplayMode mode = {};
		mode.w = parms.width;
		mode.h = parms.height;
		mode.refresh_rate = parms.displayHz;
		if ( SDL_SetWindowDisplayMode( win32.sdlWindow, &mode ) != 0 ||
			SDL_SetWindowFullscreen( win32.sdlWindow, SDL_WINDOW_FULLSCREEN ) != 0 ) {
			common->Warning( "Could not set SDL2 fullscreen mode: %s", SDL_GetError() );
			return false;
		}
	} else {
		SDL_SetWindowPosition( win32.sdlWindow, win32.win_xpos.GetInteger(), win32.win_ypos.GetInteger() );
	}

	win32.cdsFullscreen = parms.fullScreen;
	GLimp_UpdateDisplayConfig();
	return true;
}

void GLimp_Shutdown( void ) {
	common->Printf( "Shutting down SDL2 OpenGL subsystem\n" );
	GLimp_GrabInput( false );
	GLimp_RestoreGamma();

	if ( win32.sdlGLContext != NULL ) {
		SDL_GL_DeleteContext( win32.sdlGLContext );
		win32.sdlGLContext = NULL;
	}
	if ( win32.sdlWindow != NULL ) {
		SDL_DestroyWindow( win32.sdlWindow );
		win32.sdlWindow = NULL;
	}

	win32.hWnd = NULL;
	win32.hDC = NULL;
	win32.hGLRC = NULL;
	win32.cdsFullscreen = false;
	QGL_Shutdown();
}

void GLimp_SwapBuffers( void ) {
	if ( win32.sdlWindow == NULL ) {
		return;
	}
	if ( r_swapInterval.IsModified() ) {
		if ( SDL_GL_SetSwapInterval( r_swapInterval.GetInteger() ) != 0 ) {
			common->DPrintf( "SDL_GL_SetSwapInterval failed: %s\n", SDL_GetError() );
		}
		r_swapInterval.ClearModified();
	}
	SDL_GL_SwapWindow( win32.sdlWindow );
}

void GLimp_GrabInput( bool grab ) {
	if ( win32.sdlWindow == NULL ) {
		win32.mouseGrabbed = false;
		return;
	}
	SDL_SetWindowGrab( win32.sdlWindow, grab ? SDL_TRUE : SDL_FALSE );
	if ( SDL_SetRelativeMouseMode( grab ? SDL_TRUE : SDL_FALSE ) != 0 ) {
		common->DPrintf( "SDL_SetRelativeMouseMode failed: %s\n", SDL_GetError() );
	}
	SDL_ShowCursor( grab ? SDL_DISABLE : SDL_ENABLE );
	win32.mouseGrabbed = grab;
}

void GLimp_ActivateContext( void ) {
	if ( win32.sdlWindow != NULL && win32.sdlGLContext != NULL &&
		SDL_GL_MakeCurrent( win32.sdlWindow, win32.sdlGLContext ) != 0 ) {
		++win32.wglErrors;
	}
}

void GLimp_DeactivateContext( void ) {
	if ( win32.sdlWindow != NULL && SDL_GL_MakeCurrent( win32.sdlWindow, NULL ) != 0 ) {
		++win32.wglErrors;
	}
}

GLExtension_t GLimp_ExtensionPointer( const char *name ) {
	return (GLExtension_t)SDL_GL_GetProcAddress( name );
}

static int GLimp_RenderThreadWrapper( void * ) {
	win32.glimpRenderThread();
	return 0;
}

bool GLimp_SpawnRenderThread( void (*function)( void ) ) {
	if ( SDL_GetCPUCount() < 2 ) {
		return false;
	}
	win32.renderCommandsEvent = SDL_CreateSemaphore( 0 );
	win32.renderCompletedEvent = SDL_CreateSemaphore( 0 );
	win32.renderActiveEvent = SDL_CreateSemaphore( 0 );
	win32.glimpRenderThread = function;
	if ( win32.renderCommandsEvent == NULL || win32.renderCompletedEvent == NULL || win32.renderActiveEvent == NULL ) {
		common->Warning( "Could not create SDL2 render synchronization objects: %s", SDL_GetError() );
		return false;
	}
	win32.renderThreadHandle = SDL_CreateThread( GLimp_RenderThreadWrapper, "render", NULL );
	if ( win32.renderThreadHandle == NULL ) {
		common->Warning( "Could not create SDL2 render thread: %s", SDL_GetError() );
		return false;
	}
	return true;
}

void *GLimp_BackEndSleep( void ) {
	SDL_SemPost( win32.renderCompletedEvent );
	SDL_SemWait( win32.renderCommandsEvent );
	void *data = win32.smpData;
	SDL_SemPost( win32.renderActiveEvent );
	return data;
}

void GLimp_FrontEndSleep( void ) {
	SDL_SemWait( win32.renderCompletedEvent );
}

void GLimp_WakeBackEnd( void *data ) {
	win32.smpData = data;
	SDL_SemPost( win32.renderCommandsEvent );
	if ( SDL_SemWaitTimeout( win32.renderActiveEvent, 5000 ) != 0 ) {
		common->FatalError( "GLimp_WakeBackEnd: SDL2 render thread timeout" );
	}
}
