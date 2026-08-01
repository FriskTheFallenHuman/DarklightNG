/*
===========================================================================

Asynchronous named GPU markers and on-screen frame timeline.

===========================================================================
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"

namespace {

const int GPU_QUERY_FRAME_COUNT = 4;
const int MAX_GPU_MARKERS = 36;
const int GPU_QUERIES_PER_FRAME = 2 + MAX_GPU_MARKERS * 2;

const GLenum GL_TIMESTAMP_VALUE = 0x8E28;
const GLenum GL_DEBUG_SOURCE_APPLICATION_VALUE = 0x824A;

typedef unsigned long long gpuTimestamp_t;
typedef void ( APIENTRY *gpuGenQueries_t )( GLsizei count, GLuint *queries );
typedef void ( APIENTRY *gpuDeleteQueries_t )( GLsizei count, const GLuint *queries );
typedef void ( APIENTRY *gpuQueryCounter_t )( GLuint query, GLenum target );
typedef void ( APIENTRY *gpuGetQueryObjectiv_t )( GLuint query, GLenum pname, GLint *value );
typedef void ( APIENTRY *gpuGetQueryObjectui64v_t )( GLuint query, GLenum pname, gpuTimestamp_t *value );
typedef void ( APIENTRY *gpuPushDebugGroup_t )( GLenum source, GLuint id, GLsizei length, const char *message );
typedef void ( APIENTRY *gpuPopDebugGroup_t )( void );
typedef void ( APIENTRY *gpuPushGroupMarkerEXT_t )( GLsizei length, const char *marker );
typedef void ( APIENTRY *gpuPopGroupMarkerEXT_t )( void );

gpuGenQueries_t qglGpuGenQueries = NULL;
gpuDeleteQueries_t qglGpuDeleteQueries = NULL;
gpuQueryCounter_t qglGpuQueryCounter = NULL;
gpuGetQueryObjectiv_t qglGpuGetQueryObjectiv = NULL;
gpuGetQueryObjectui64v_t qglGpuGetQueryObjectui64v = NULL;
gpuPushDebugGroup_t qglGpuPushDebugGroup = NULL;
gpuPopDebugGroup_t qglGpuPopDebugGroup = NULL;
gpuPushGroupMarkerEXT_t qglGpuPushGroupMarkerEXT = NULL;
gpuPopGroupMarkerEXT_t qglGpuPopGroupMarkerEXT = NULL;

struct gpuMarkerQuery_t {
	char name[48];
	int depth;
	int startQuery;
	int endQuery;
};

struct gpuQueryFrame_t {
	GLuint queries[GPU_QUERIES_PER_FRAME];
	gpuMarkerQuery_t markers[MAX_GPU_MARKERS];
	int numMarkers;
	int droppedMarkers;
	unsigned int serial;
	bool pending;
};

struct gpuResolvedMarker_t {
	char name[48];
	int depth;
	gpuTimestamp_t start;
	gpuTimestamp_t end;
};

struct gpuResolvedFrame_t {
	gpuResolvedMarker_t markers[MAX_GPU_MARKERS];
	int numMarkers;
	int droppedMarkers;
	unsigned int serial;
	gpuTimestamp_t start;
	gpuTimestamp_t end;
	bool valid;
};

struct gpuProfilerState_t {
	gpuQueryFrame_t frames[GPU_QUERY_FRAME_COUNT];
	gpuResolvedFrame_t displayFrame;
	int activeFrame;
	int nextFrame;
	int markerDepth;
	unsigned int nextSerial;
	bool initialized;
	bool timerQueriesAvailable;
	bool frameMarkersEnabled;
	bool frameDebugGroupPushed;
};

gpuProfilerState_t gpuProfiler;

void RB_GPUProfilerPushDebugGroup( const char *name ) {
	if ( qglGpuPushDebugGroup && qglGpuPopDebugGroup ) {
		qglGpuPushDebugGroup( GL_DEBUG_SOURCE_APPLICATION_VALUE, 0, -1, name );
	} else if ( qglGpuPushGroupMarkerEXT && qglGpuPopGroupMarkerEXT ) {
		qglGpuPushGroupMarkerEXT( -1, name );
	}
}

void RB_GPUProfilerPopDebugGroup( void ) {
	if ( qglGpuPushDebugGroup && qglGpuPopDebugGroup ) {
		qglGpuPopDebugGroup();
	} else if ( qglGpuPushGroupMarkerEXT && qglGpuPopGroupMarkerEXT ) {
		qglGpuPopGroupMarkerEXT();
	}
}

void RB_GPUProfilerResolveFrames( void ) {
	if ( !gpuProfiler.timerQueriesAvailable ) {
		return;
	}

	for ( int frameIndex = 0; frameIndex < GPU_QUERY_FRAME_COUNT; frameIndex++ ) {
		gpuQueryFrame_t &queryFrame = gpuProfiler.frames[frameIndex];
		if ( !queryFrame.pending ) {
			continue;
		}

		GLint available = 0;
		qglGpuGetQueryObjectiv( queryFrame.queries[1], GL_QUERY_RESULT_AVAILABLE, &available );
		if ( !available ) {
			continue;
		}

		gpuResolvedFrame_t resolved;
		memset( &resolved, 0, sizeof( resolved ) );
		resolved.serial = queryFrame.serial;
		resolved.numMarkers = queryFrame.numMarkers;
		resolved.droppedMarkers = queryFrame.droppedMarkers;
		qglGpuGetQueryObjectui64v( queryFrame.queries[0], GL_QUERY_RESULT, &resolved.start );
		qglGpuGetQueryObjectui64v( queryFrame.queries[1], GL_QUERY_RESULT, &resolved.end );

		for ( int markerIndex = 0; markerIndex < queryFrame.numMarkers; markerIndex++ ) {
			const gpuMarkerQuery_t &queryMarker = queryFrame.markers[markerIndex];
			gpuResolvedMarker_t &resolvedMarker = resolved.markers[markerIndex];
			idStr::Copynz( resolvedMarker.name, queryMarker.name, sizeof( resolvedMarker.name ) );
			resolvedMarker.depth = queryMarker.depth;
			qglGpuGetQueryObjectui64v( queryFrame.queries[queryMarker.startQuery], GL_QUERY_RESULT, &resolvedMarker.start );
			qglGpuGetQueryObjectui64v( queryFrame.queries[queryMarker.endQuery], GL_QUERY_RESULT, &resolvedMarker.end );
		}

		resolved.valid = resolved.end > resolved.start;
		if ( resolved.valid && ( !gpuProfiler.displayFrame.valid || resolved.serial > gpuProfiler.displayFrame.serial ) ) {
			gpuProfiler.displayFrame = resolved;
		}
		queryFrame.pending = false;
	}
}

int RB_GPUProfilerBeginMarker( const char *name ) {
	if ( !gpuProfiler.frameMarkersEnabled ) {
		return -1;
	}

	RB_GPUProfilerPushDebugGroup( name );

	const int depth = gpuProfiler.markerDepth++;
	if ( gpuProfiler.activeFrame < 0 ) {
		return -2;
	}

	gpuQueryFrame_t &queryFrame = gpuProfiler.frames[gpuProfiler.activeFrame];
	if ( queryFrame.numMarkers >= MAX_GPU_MARKERS ) {
		queryFrame.droppedMarkers++;
		return -2;
	}

	const int markerIndex = queryFrame.numMarkers++;
	gpuMarkerQuery_t &marker = queryFrame.markers[markerIndex];
	idStr::Copynz( marker.name, name, sizeof( marker.name ) );
	marker.depth = depth;
	marker.startQuery = 2 + markerIndex * 2;
	marker.endQuery = marker.startQuery + 1;
	qglGpuQueryCounter( queryFrame.queries[marker.startQuery], GL_TIMESTAMP_VALUE );
	return markerIndex;
}

void RB_GPUProfilerEndMarker( int markerHandle ) {
	if ( markerHandle == -1 ) {
		return;
	}

	if ( markerHandle >= 0 && gpuProfiler.activeFrame >= 0 ) {
		gpuQueryFrame_t &queryFrame = gpuProfiler.frames[gpuProfiler.activeFrame];
		qglGpuQueryCounter( queryFrame.queries[queryFrame.markers[markerHandle].endQuery], GL_TIMESTAMP_VALUE );
	}

	if ( gpuProfiler.markerDepth > 0 ) {
		gpuProfiler.markerDepth--;
	}
	RB_GPUProfilerPopDebugGroup();
}

void RB_GPUProfilerDrawQuad( float x1, float y1, float x2, float y2, const idVec4 &color ) {
	qglColor4fv( color.ToFloatPtr() );
	qglBegin( GL_QUADS );
	qglTexCoord2f( 0.0f, 0.0f ); qglVertex2f( x1, y1 );
	qglTexCoord2f( 1.0f, 0.0f ); qglVertex2f( x2, y1 );
	qglTexCoord2f( 1.0f, 1.0f ); qglVertex2f( x2, y2 );
	qglTexCoord2f( 0.0f, 1.0f ); qglVertex2f( x1, y2 );
	qglEnd();
}

void RB_GPUProfilerDrawText( idImage *fontImage, float x, float y, const idVec4 &color, const char *text, int maxChars = 0 ) {
	const float charWidth = 6.0f;
	const float charHeight = 9.0f;
	qglColor4fv( color.ToFloatPtr() );
	fontImage->Bind();
	qglBegin( GL_QUADS );
	for ( int charIndex = 0; text[charIndex] && ( maxChars <= 0 || charIndex < maxChars ); charIndex++ ) {
		const unsigned char ch = text[charIndex];
		if ( ch != ' ' ) {
			const float s1 = ( ch & 15 ) * ( 1.0f / 16.0f );
			const float t1 = ( ch >> 4 ) * ( 1.0f / 16.0f );
			const float s2 = s1 + ( 1.0f / 16.0f );
			const float t2 = t1 + ( 1.0f / 16.0f );
			qglTexCoord2f( s1, t1 ); qglVertex2f( x, y );
			qglTexCoord2f( s2, t1 ); qglVertex2f( x + charWidth, y );
			qglTexCoord2f( s2, t2 ); qglVertex2f( x + charWidth, y + charHeight );
			qglTexCoord2f( s1, t2 ); qglVertex2f( x, y + charHeight );
		}
		x += charWidth;
	}
	qglEnd();
}

gpuTimestamp_t RB_GPUProfilerClampTimestamp( gpuTimestamp_t timestamp, const gpuResolvedFrame_t &frame ) {
	if ( timestamp < frame.start ) {
		return frame.start;
	}
	if ( timestamp > frame.end ) {
		return frame.end;
	}
	return timestamp;
}

} // namespace

idScopedGpuMarker::idScopedGpuMarker( const char *name, bool condition ) {
	markerHandle = condition ? RB_GPUProfilerBeginMarker( name ) : -1;
}

idScopedGpuMarker::~idScopedGpuMarker() {
	RB_GPUProfilerEndMarker( markerHandle );
}

void RB_GPUProfilerInit( void ) {
	memset( &gpuProfiler, 0, sizeof( gpuProfiler ) );
	gpuProfiler.activeFrame = -1;

	const bool coreDebugGroupsAdvertised = glConfig.glVersion >= 4.3f ||
		strstr( glConfig.extensions_string, "GL_KHR_debug" ) != NULL;
	if ( coreDebugGroupsAdvertised ) {
		qglGpuPushDebugGroup = (gpuPushDebugGroup_t)GLimp_ExtensionPointer( "glPushDebugGroup" );
		qglGpuPopDebugGroup = (gpuPopDebugGroup_t)GLimp_ExtensionPointer( "glPopDebugGroup" );
	}
	const bool extDebugGroupsAdvertised = strstr( glConfig.extensions_string, "GL_EXT_debug_marker" ) != NULL;
	if ( extDebugGroupsAdvertised ) {
		qglGpuPushGroupMarkerEXT = (gpuPushGroupMarkerEXT_t)GLimp_ExtensionPointer( "glPushGroupMarkerEXT" );
		qglGpuPopGroupMarkerEXT = (gpuPopGroupMarkerEXT_t)GLimp_ExtensionPointer( "glPopGroupMarkerEXT" );
	}
	if ( ( qglGpuPushDebugGroup && qglGpuPopDebugGroup ) ||
		( qglGpuPushGroupMarkerEXT && qglGpuPopGroupMarkerEXT ) ) {
		common->Printf( "...using named GPU debug groups\n" );
	} else {
		common->Printf( "X..named GPU debug groups unavailable\n" );
	}

	const bool timerQueriesAdvertised = glConfig.glVersion >= 3.3f ||
		strstr( glConfig.extensions_string, "GL_ARB_timer_query" ) != NULL;
	if ( timerQueriesAdvertised ) {
		qglGpuGenQueries = (gpuGenQueries_t)GLimp_ExtensionPointer( "glGenQueries" );
		if ( !qglGpuGenQueries ) {
			qglGpuGenQueries = (gpuGenQueries_t)GLimp_ExtensionPointer( "glGenQueriesARB" );
		}
		qglGpuDeleteQueries = (gpuDeleteQueries_t)GLimp_ExtensionPointer( "glDeleteQueries" );
		if ( !qglGpuDeleteQueries ) {
			qglGpuDeleteQueries = (gpuDeleteQueries_t)GLimp_ExtensionPointer( "glDeleteQueriesARB" );
		}
		qglGpuGetQueryObjectiv = (gpuGetQueryObjectiv_t)GLimp_ExtensionPointer( "glGetQueryObjectiv" );
		if ( !qglGpuGetQueryObjectiv ) {
			qglGpuGetQueryObjectiv = (gpuGetQueryObjectiv_t)GLimp_ExtensionPointer( "glGetQueryObjectivARB" );
		}
		qglGpuQueryCounter = (gpuQueryCounter_t)GLimp_ExtensionPointer( "glQueryCounter" );
		qglGpuGetQueryObjectui64v = (gpuGetQueryObjectui64v_t)GLimp_ExtensionPointer( "glGetQueryObjectui64v" );
	}

	gpuProfiler.timerQueriesAvailable = qglGpuGenQueries && qglGpuDeleteQueries && qglGpuQueryCounter &&
		qglGpuGetQueryObjectiv && qglGpuGetQueryObjectui64v;
	if ( gpuProfiler.timerQueriesAvailable ) {
		for ( int frameIndex = 0; frameIndex < GPU_QUERY_FRAME_COUNT; frameIndex++ ) {
			qglGpuGenQueries( GPU_QUERIES_PER_FRAME, gpuProfiler.frames[frameIndex].queries );
		}
		common->Printf( "...using asynchronous GPU timer queries\n" );
	} else {
		common->Printf( "X..GPU timer queries unavailable; r_showstats will only emit supported debug groups\n" );
	}

	gpuProfiler.initialized = true;
}

void RB_GPUProfilerShutdown( void ) {
	if ( gpuProfiler.initialized && gpuProfiler.timerQueriesAvailable && qglGpuDeleteQueries ) {
		for ( int frameIndex = 0; frameIndex < GPU_QUERY_FRAME_COUNT; frameIndex++ ) {
			qglGpuDeleteQueries( GPU_QUERIES_PER_FRAME, gpuProfiler.frames[frameIndex].queries );
		}
	}
	memset( &gpuProfiler, 0, sizeof( gpuProfiler ) );
	gpuProfiler.activeFrame = -1;
}

void RB_GPUProfilerBeginFrame( void ) {
	gpuProfiler.frameMarkersEnabled = gpuProfiler.initialized && r_showStats.GetBool();
	gpuProfiler.markerDepth = 0;
	gpuProfiler.activeFrame = -1;
	if ( !gpuProfiler.frameMarkersEnabled ) {
		return;
	}
	RB_GPUProfilerPushDebugGroup( "Frame" );
	gpuProfiler.frameDebugGroupPushed = true;
	if ( !gpuProfiler.timerQueriesAvailable ) {
		return;
	}

	RB_GPUProfilerResolveFrames();
	for ( int offset = 0; offset < GPU_QUERY_FRAME_COUNT; offset++ ) {
		const int frameIndex = ( gpuProfiler.nextFrame + offset ) % GPU_QUERY_FRAME_COUNT;
		gpuQueryFrame_t &queryFrame = gpuProfiler.frames[frameIndex];
		if ( queryFrame.pending ) {
			continue;
		}

		queryFrame.numMarkers = 0;
		queryFrame.droppedMarkers = 0;
		queryFrame.serial = ++gpuProfiler.nextSerial;
		gpuProfiler.activeFrame = frameIndex;
		gpuProfiler.nextFrame = ( frameIndex + 1 ) % GPU_QUERY_FRAME_COUNT;
		qglGpuQueryCounter( queryFrame.queries[0], GL_TIMESTAMP_VALUE );
		break;
	}
}

void RB_GPUProfilerEndFrame( void ) {
	if ( gpuProfiler.activeFrame >= 0 ) {
		gpuQueryFrame_t &queryFrame = gpuProfiler.frames[gpuProfiler.activeFrame];
		qglGpuQueryCounter( queryFrame.queries[1], GL_TIMESTAMP_VALUE );
		queryFrame.pending = true;
		gpuProfiler.activeFrame = -1;
	}
	if ( gpuProfiler.frameDebugGroupPushed ) {
		RB_GPUProfilerPopDebugGroup();
		gpuProfiler.frameDebugGroupPushed = false;
	}
	gpuProfiler.markerDepth = 0;
	gpuProfiler.frameMarkersEnabled = false;
}

void RB_GPUProfilerDraw( void ) {
	if ( !r_showStats.GetBool() || !gpuProfiler.initialized ) {
		return;
	}

	const idMaterial *fontMaterial = declManager->FindMaterial( "textures/bigchars" );
	if ( !fontMaterial || fontMaterial->GetNumStages() < 1 || !fontMaterial->GetStage( 0 )->texture.image ) {
		return;
	}
	idImage *fontImage = fontMaterial->GetStage( 0 )->texture.image;

	R_UnbindGLSLProgram();
	RB_SetGL2D();
	int textureUnits = glConfig.maxTextureUnits;
	if ( glConfig.maxTextureImageUnits > textureUnits ) {
		textureUnits = glConfig.maxTextureImageUnits;
	}
	if ( textureUnits > MAX_MULTITEXTURE_UNITS ) {
		textureUnits = MAX_MULTITEXTURE_UNITS;
	}
	for ( int textureUnit = textureUnits - 1; textureUnit > 0; textureUnit-- ) {
		GL_SelectTexture( textureUnit );
		globalImages->BindNull();
	}
	GL_SelectTexture( 0 );
	GL_TexEnv( GL_MODULATE );

	const gpuResolvedFrame_t &frame = gpuProfiler.displayFrame;
	const int rowCount = frame.valid ? frame.numMarkers : 0;
	const float panelHeight = 52.0f + rowCount * 11.0f + ( frame.droppedMarkers ? 11.0f : 0.0f );
	const float graphX = 255.0f;
	const float graphWidth = 369.0f;
	const float graphTop = 41.0f;
	const idVec4 panelColor( 0.015f, 0.02f, 0.03f, 0.88f );
	const idVec4 gridColor( 0.25f, 0.3f, 0.38f, 0.45f );
	const idVec4 textColor( 0.9f, 0.94f, 1.0f, 1.0f );
	const idVec4 mutedTextColor( 0.62f, 0.7f, 0.8f, 1.0f );
	static const idVec4 markerColors[] = {
		idVec4( 0.2f, 0.65f, 1.0f, 0.88f ),
		idVec4( 0.35f, 0.85f, 0.45f, 0.88f ),
		idVec4( 1.0f, 0.65f, 0.2f, 0.88f ),
		idVec4( 0.82f, 0.4f, 1.0f, 0.88f ),
		idVec4( 1.0f, 0.35f, 0.5f, 0.88f )
	};

	globalImages->whiteImage->Bind();
	RB_GPUProfilerDrawQuad( 8.0f, 8.0f, 632.0f, 8.0f + panelHeight, panelColor );
	for ( int tick = 0; tick <= 4; tick++ ) {
		const float x = graphX + graphWidth * ( tick * 0.25f );
		RB_GPUProfilerDrawQuad( x, 35.0f, x + 0.6f, 8.0f + panelHeight - 5.0f, gridColor );
	}

	if ( frame.valid ) {
		const double frameDuration = double( frame.end - frame.start );
		for ( int markerIndex = 0; markerIndex < rowCount; markerIndex++ ) {
			const gpuResolvedMarker_t &marker = frame.markers[markerIndex];
			const gpuTimestamp_t markerStart = RB_GPUProfilerClampTimestamp( marker.start, frame );
			gpuTimestamp_t markerEnd = RB_GPUProfilerClampTimestamp( marker.end, frame );
			if ( markerEnd < markerStart ) {
				markerEnd = markerStart;
			}
			const double startFraction = double( markerStart - frame.start ) / frameDuration;
			const double endFraction = double( markerEnd - frame.start ) / frameDuration;
			float x1 = graphX + graphWidth * (float)startFraction;
			float x2 = graphX + graphWidth * (float)endFraction;
			if ( x1 < graphX ) x1 = graphX;
			if ( x1 > graphX + graphWidth - 1.0f ) x1 = graphX + graphWidth - 1.0f;
			if ( x2 > graphX + graphWidth ) x2 = graphX + graphWidth;
			if ( x2 < x1 + 1.0f ) x2 = x1 + 1.0f;
			const float y = graphTop + markerIndex * 11.0f;
			const idVec4 &markerColor = markerColors[marker.depth % ( sizeof( markerColors ) / sizeof( markerColors[0] ) )];
			RB_GPUProfilerDrawQuad( x1, y, x2, y + 8.0f, markerColor );
		}
	}

	char text[160];
	if ( !gpuProfiler.timerQueriesAvailable ) {
		RB_GPUProfilerDrawText( fontImage, 15.0f, 14.0f, textColor, "GPU TIMELINE: TIMER QUERIES NOT SUPPORTED" );
		RB_GPUProfilerDrawText( fontImage, 15.0f, 27.0f, mutedTextColor, "NAMED DRIVER MARKERS ARE STILL ACTIVE WHEN AVAILABLE" );
		return;
	}
	if ( !frame.valid ) {
		RB_GPUProfilerDrawText( fontImage, 15.0f, 14.0f, textColor, "GPU TIMELINE: WAITING FOR ASYNCHRONOUS RESULTS..." );
		return;
	}

	const double frameMilliseconds = double( frame.end - frame.start ) * 0.000001;
	const unsigned int frameDelay = gpuProfiler.nextSerial > frame.serial ? gpuProfiler.nextSerial - frame.serial : 0;
	idStr::snPrintf( text, sizeof( text ), "GPU FRAME %u   START 0.000 ms   END %.3f ms   TOTAL %.3f ms   DELAY %u",
		frame.serial, frameMilliseconds, frameMilliseconds, frameDelay );
	RB_GPUProfilerDrawText( fontImage, 15.0f, 14.0f, textColor, text );
	RB_GPUProfilerDrawText( fontImage, graphX - 2.0f, 27.0f, mutedTextColor, "0" );
	idStr::snPrintf( text, sizeof( text ), "%.3f ms", frameMilliseconds );
	RB_GPUProfilerDrawText( fontImage, graphX + graphWidth - idStr::Length( text ) * 6.0f, 27.0f, mutedTextColor, text );

	for ( int markerIndex = 0; markerIndex < rowCount; markerIndex++ ) {
		const gpuResolvedMarker_t &marker = frame.markers[markerIndex];
		const gpuTimestamp_t markerStart = RB_GPUProfilerClampTimestamp( marker.start, frame );
		gpuTimestamp_t markerEnd = RB_GPUProfilerClampTimestamp( marker.end, frame );
		if ( markerEnd < markerStart ) {
			markerEnd = markerStart;
		}
		const double startMilliseconds = double( markerStart - frame.start ) * 0.000001;
		const double endMilliseconds = double( markerEnd - frame.start ) * 0.000001;
		const double durationMilliseconds = endMilliseconds - startMilliseconds;
		idStr::snPrintf( text, sizeof( text ), "%*s%-16.16s %5.2f-%5.2f %5.2fms", marker.depth, "", marker.name,
			startMilliseconds, endMilliseconds, durationMilliseconds );
		RB_GPUProfilerDrawText( fontImage, 15.0f, graphTop + markerIndex * 11.0f, textColor, text, 39 );
	}
	if ( frame.droppedMarkers ) {
		idStr::snPrintf( text, sizeof( text ), "+ %i GPU MARKERS OMITTED", frame.droppedMarkers );
		RB_GPUProfilerDrawText( fontImage, 15.0f, graphTop + rowCount * 11.0f, mutedTextColor, text );
	}
}
