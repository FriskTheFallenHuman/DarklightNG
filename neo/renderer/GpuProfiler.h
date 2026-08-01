/*
===========================================================================

GPU timing markers and on-screen timeline.

===========================================================================
*/

#ifndef __GPU_PROFILER_H__
#define __GPU_PROFILER_H__

class idScopedGpuMarker {
public:
	explicit idScopedGpuMarker( const char *name, bool condition = true );
	~idScopedGpuMarker();

private:
	idScopedGpuMarker( const idScopedGpuMarker & );
	idScopedGpuMarker &operator=( const idScopedGpuMarker & );

	int markerHandle;
};

void RB_GPUProfilerInit( void );
void RB_GPUProfilerShutdown( void );
void RB_GPUProfilerBeginFrame( void );
void RB_GPUProfilerEndFrame( void );
void RB_GPUProfilerDraw( void );

#endif
