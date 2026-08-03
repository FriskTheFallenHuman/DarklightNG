/*
===========================================================================

DarklightNG Source Code
Copyright (C) 2026 - Justin Marshall (aka IceColdDuke).

SDL2-based Linux system services.

===========================================================================
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "sys_local.h"
#include "sys_platform.h"

#include <SDL_loadso.h>

#include <dirent.h>
#include <execinfo.h>
#include <fcntl.h>
#include <fenv.h>
#include <pwd.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>

Win32Vars_t win32 = {};

idCVar Win32Vars_t::in_mouse( "in_mouse", "1", CVAR_SYSTEM | CVAR_BOOL, "enable mouse input" );
idCVar Win32Vars_t::win_xpos( "win_xpos", "3", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_INTEGER, "horizontal position of window" );
idCVar Win32Vars_t::win_ypos( "win_ypos", "22", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_INTEGER, "vertical position of window" );

static idCVar sys_videoRam( "sys_videoRam", "0", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_INTEGER,
	"texture memory in megabytes (0 uses a conservative default)", 0, 65536 );

xthreadInfo *g_threads[MAX_THREADS];
int g_thread_count = 0;

static SDL_mutex *criticalSections[MAX_CRITICAL_SECTIONS];
static SDL_sem *triggerEvents[MAX_TRIGGER_EVENTS];
static sysMemoryStats_t exeLaunchMemoryStats;
static char fatalError[MAX_STRING_CHARS];

static const int MAX_QUED_EVENTS = 256;
static const int MASK_QUED_EVENTS = MAX_QUED_EVENTS - 1;
static sysEvent_t eventQue[MAX_QUED_EVENTS];
static int eventHead;
static int eventTail;

static char consoleLine[1024];

static char *Sys_ConsoleInputSDL() {
	fd_set readSet;
	FD_ZERO( &readSet );
	FD_SET( STDIN_FILENO, &readSet );
	timeval timeout = {};
	if ( select( STDIN_FILENO + 1, &readSet, NULL, NULL, &timeout ) <= 0 ) {
		return NULL;
	}
	if ( fgets( consoleLine, sizeof( consoleLine ), stdin ) == NULL ) {
		return NULL;
	}
	consoleLine[strcspn( consoleLine, "\r\n" )] = '\0';
	return consoleLine[0] != '\0' ? consoleLine : NULL;
}

void Sys_QueEvent( int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr ) {
	sysEvent_t *event = &eventQue[eventHead & MASK_QUED_EVENTS];
	if ( eventHead - eventTail >= MAX_QUED_EVENTS ) {
		if ( event->evPtr != NULL ) {
			Mem_Free( event->evPtr );
		}
		++eventTail;
	}
	++eventHead;
	event->evType = type;
	event->evValue = value;
	event->evValue2 = value2;
	event->evPtrLength = ptrLength;
	event->evPtr = ptr;
}

sysEvent_t Sys_GetEvent( void ) {
	if ( eventHead > eventTail ) {
		return eventQue[eventTail++ & MASK_QUED_EVENTS];
	}
	sysEvent_t empty = {};
	return empty;
}

void Sys_ClearEvents( void ) {
	eventHead = eventTail = 0;
	Sys_ClearSDLInputEvents();
	SDL_FlushEvents( SDL_FIRSTEVENT, SDL_LASTEVENT );
}

void Sys_GenerateEvents( void ) {
	static bool entered;
	if ( entered ) {
		return;
	}
	entered = true;
	Sys_ProcessSDLEvents();
	IN_Frame();
	char *line = Sys_ConsoleInputSDL();
	if ( line != NULL ) {
		const int length = strlen( line ) + 1;
		char *copy = (char *)Mem_Alloc( length );
		idStr::Copynz( copy, line, length );
		Sys_QueEvent( Sys_Milliseconds(), SE_CONSOLE, 0, 0, length, copy );
	}
	entered = false;
}

void Sys_Printf( const char *msg, ... ) {
	va_list args;
	va_start( args, msg );
	vfprintf( stdout, msg, args );
	va_end( args );
	fflush( stdout );
}

void Sys_DebugPrintf( const char *fmt, ... ) {
	va_list args;
	va_start( args, fmt );
	vfprintf( stderr, fmt, args );
	va_end( args );
	fflush( stderr );
}

void Sys_DebugVPrintf( const char *fmt, va_list args ) {
	vfprintf( stderr, fmt, args );
	fflush( stderr );
}

void Sys_Error( const char *error, ... ) {
	char message[MAX_STRING_CHARS];
	va_list args;
	va_start( args, error );
	idStr::vsnPrintf( message, sizeof( message ), error, args );
	va_end( args );
	Sys_Printf( "Sys_Error: %s\n", message );
	if ( SDL_WasInit( SDL_INIT_VIDEO ) != 0 ) {
		SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, GAME_NAME, message, win32.sdlWindow );
	}
	SDL_Quit();
	exit( EXIT_FAILURE );
}

void Sys_Quit( void ) {
	SDL_Quit();
	exit( EXIT_SUCCESS );
}

void Sys_Init( void ) {
	for ( int index = 0; index < MAX_CRITICAL_SECTIONS; ++index ) {
		criticalSections[index] = SDL_CreateMutex();
	}
	for ( int index = 0; index < MAX_TRIGGER_EVENTS; ++index ) {
		triggerEvents[index] = SDL_CreateSemaphore( 0 );
	}
	Sys_GetCurrentMemoryStatus( exeLaunchMemoryStats );
}

void Sys_Shutdown( void ) {
	for ( int index = 0; index < MAX_TRIGGER_EVENTS; ++index ) {
		if ( triggerEvents[index] != NULL ) {
			SDL_DestroySemaphore( triggerEvents[index] );
			triggerEvents[index] = NULL;
		}
	}
	for ( int index = 0; index < MAX_CRITICAL_SECTIONS; ++index ) {
		if ( criticalSections[index] != NULL ) {
			SDL_DestroyMutex( criticalSections[index] );
			criticalSections[index] = NULL;
		}
	}
}

bool Sys_AlreadyRunning( void ) {
	return false;
}

void Sys_Sleep( int msec ) {
	SDL_Delay( msec > 0 ? (Uint32)msec : 0 );
}

int Sys_Milliseconds( void ) {
	return (int)SDL_GetTicks();
}

double Sys_GetClockTicks( void ) {
	return (double)SDL_GetPerformanceCounter();
}

double Sys_ClockTicksPerSecond( void ) {
	return (double)SDL_GetPerformanceFrequency();
}

cpuid_t Sys_GetProcessorId( void ) {
	int flags = CPUID_GENERIC | CPUID_CMOV;
	if ( SDL_HasMMX() ) flags |= CPUID_MMX;
	if ( SDL_HasSSE() ) flags |= CPUID_SSE | CPUID_FTZ;
	if ( SDL_HasSSE2() ) flags |= CPUID_SSE2 | CPUID_DAZ;
	if ( SDL_HasSSE3() ) flags |= CPUID_SSE3;
	return (cpuid_t)flags;
}

const char *Sys_GetProcessorString( void ) {
	return "SDL2 x86";
}

bool Sys_FPU_StackIsEmpty( void ) { return true; }
void Sys_FPU_ClearStack( void ) {}
const char *Sys_FPU_GetState( void ) { return "SSE floating-point state"; }
void Sys_FPU_EnableExceptions( int exceptions ) {}
void Sys_FPU_SetPrecision( int precision ) {}

void Sys_FPU_SetRounding( int rounding ) {
	static const int modes[] = { FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO };
	if ( rounding >= 0 && rounding < 4 ) {
		fesetround( modes[rounding] );
	}
}

static void Sys_SetMXCSRBit( unsigned int bit, bool enabled ) {
	unsigned int control;
	__asm__ __volatile__( "stmxcsr %0" : "=m" ( control ) );
	control = enabled ? control | bit : control & ~bit;
	__asm__ __volatile__( "ldmxcsr %0" : : "m" ( control ) );
}

void Sys_FPU_SetFTZ( bool enable ) { Sys_SetMXCSRBit( 1u << 15, enable ); }
void Sys_FPU_SetDAZ( bool enable ) { Sys_SetMXCSRBit( 1u << 6, enable ); }

int Sys_GetSystemRam( void ) {
	return SDL_GetSystemRAM();
}

int Sys_GetVideoRam( void ) {
	return sys_videoRam.GetInteger() > 0 ? sys_videoRam.GetInteger() : 512;
}

int Sys_GetDriveFreeSpace( const char *path ) {
	struct statvfs info;
	if ( statvfs( path, &info ) != 0 ) {
		return 0;
	}
	return (int)( ( (unsigned long long)info.f_bavail * info.f_frsize ) >> 20 );
}

void Sys_GetCurrentMemoryStatus( sysMemoryStats_t &stats ) {
	memset( &stats, 0, sizeof( stats ) );
	long pages = sysconf( _SC_PHYS_PAGES );
	long available = sysconf( _SC_AVPHYS_PAGES );
	long pageSize = sysconf( _SC_PAGESIZE );
	if ( pages > 0 && pageSize > 0 ) {
		stats.totalPhysical = (int)( ( (unsigned long long)pages * pageSize ) >> 20 );
		stats.availPhysical = (int)( ( (unsigned long long)available * pageSize ) >> 20 );
		stats.memoryLoad = stats.totalPhysical > 0 ? 100 - stats.availPhysical * 100 / stats.totalPhysical : 0;
	}
	stats.totalVirtual = stats.totalPhysical;
	stats.availVirtual = stats.availPhysical;
}

void Sys_GetExeLaunchMemoryStatus( sysMemoryStats_t &stats ) {
	stats = exeLaunchMemoryStats;
}

bool Sys_LockMemory( void *ptr, int bytes ) { return mlock( ptr, bytes ) == 0; }
bool Sys_UnlockMemory( void *ptr, int bytes ) { return munlock( ptr, bytes ) == 0; }
void Sys_SetPhysicalWorkMemory( int minBytes, int maxBytes ) {}

void Sys_GetCallStack( address_t *callStack, const int callStackSize ) {
	void *frames[128];
	const int count = backtrace( frames, idMath::ClampInt( 0, 128, callStackSize ) );
	int index = 0;
	for ( ; index < count; ++index ) callStack[index] = (address_t)frames[index];
	for ( ; index < callStackSize; ++index ) callStack[index] = 0;
}

const char *Sys_GetCallStackStr( const address_t *callStack, const int callStackSize ) {
	static char text[MAX_STRING_CHARS * 2];
	text[0] = '\0';
	for ( int index = 0; index < callStackSize && callStack[index] != 0; ++index ) {
		char address[32];
		idStr::snPrintf( address, sizeof( address ), " -> 0x%08lx", callStack[index] );
		idStr::Append( text, sizeof( text ), address );
	}
	return text;
}

const char *Sys_GetCallStackCurStr( int depth ) {
	address_t stack[128];
	depth = idMath::ClampInt( 0, 128, depth );
	Sys_GetCallStack( stack, depth );
	return Sys_GetCallStackStr( stack, depth );
}

const char *Sys_GetCallStackCurAddressStr( int depth ) { return Sys_GetCallStackCurStr( depth ); }
void Sys_ShutdownSymbols( void ) {}

int Sys_DLL_Load( const char *dllName ) {
	return (int)SDL_LoadObject( dllName );
}

void *Sys_DLL_GetProcAddress( int dllHandle, const char *procName ) {
	return SDL_LoadFunction( (void *)dllHandle, procName );
}

void Sys_DLL_Unload( int dllHandle ) {
	if ( dllHandle != 0 ) SDL_UnloadObject( (void *)dllHandle );
}

char *Sys_GetClipboardData( void ) {
	char *text = SDL_GetClipboardText();
	if ( text == NULL || text[0] == '\0' ) {
		SDL_free( text );
		return NULL;
	}
	const int length = strlen( text ) + 1;
	char *copy = (char *)Mem_Alloc( length );
	idStr::Copynz( copy, text, length );
	SDL_free( text );
	return copy;
}

void Sys_SetClipboardData( const char *string ) {
	if ( string != NULL ) SDL_SetClipboardText( string );
}

void Sys_ShowWindow( bool show ) {
	if ( win32.sdlWindow == NULL ) return;
	if ( show ) SDL_ShowWindow( win32.sdlWindow ); else SDL_HideWindow( win32.sdlWindow );
}

bool Sys_IsWindowVisible( void ) {
	return win32.sdlWindow != NULL && ( SDL_GetWindowFlags( win32.sdlWindow ) & SDL_WINDOW_SHOWN ) != 0;
}

void Sys_ShowConsole( int visLevel, bool quitOnClose ) {}

void Sys_Mkdir( const char *path ) { mkdir( path, 0777 ); }

ID_TIME_T Sys_FileTimeStamp( FILE *fp ) {
	struct stat info;
	return fstat( fileno( fp ), &info ) == 0 ? info.st_mtime : 0;
}

const char *Sys_EXEPath( void ) {
	static char path[MAX_OSPATH];
	const ssize_t length = readlink( "/proc/self/exe", path, sizeof( path ) - 1 );
	if ( length < 0 ) path[0] = '\0'; else path[length] = '\0';
	return path;
}

static const char *Sys_CurrentPath() {
	static char path[MAX_OSPATH];
	if ( getcwd( path, sizeof( path ) ) == NULL ) path[0] = '\0';
	return path;
}

const char *Sys_DefaultCDPath( void ) { return ""; }

const char *Sys_DefaultBasePath( void ) {
	static idStr path;
	path = Sys_EXEPath();
	path.StripFilename();
	idStr base = path + "/" BASE_GAMEDIR;
	struct stat info;
	if ( stat( base.c_str(), &info ) == 0 && S_ISDIR( info.st_mode ) ) return path.c_str();
	return Sys_CurrentPath();
}

const char *Sys_DefaultSavePath( void ) { return Sys_DefaultBasePath(); }

int Sys_ListFiles( const char *directory, const char *extension, idStrList &list ) {
	list.Clear();
	DIR *dir = opendir( directory );
	if ( dir == NULL ) return -1;
	const bool directoriesOnly = extension != NULL && !strcmp( extension, "/" );
	const char *wantedExtension = extension != NULL ? extension : "";
	for ( dirent *entry = readdir( dir ); entry != NULL; entry = readdir( dir ) ) {
		if ( !strcmp( entry->d_name, "." ) || !strcmp( entry->d_name, ".." ) ) continue;
		idStr fullPath = directory;
		fullPath += "/";
		fullPath += entry->d_name;
		struct stat info;
		if ( stat( fullPath.c_str(), &info ) != 0 ) continue;
		if ( directoriesOnly ) {
			if ( S_ISDIR( info.st_mode ) ) list.Append( entry->d_name );
		} else if ( !S_ISDIR( info.st_mode ) ) {
			idStr fileExtension;
			idStr( entry->d_name ).ExtractFileExtension( fileExtension );
			if ( wantedExtension[0] == '\0' || fileExtension.Icmp( wantedExtension[0] == '.' ? wantedExtension + 1 : wantedExtension ) == 0 ) {
				list.Append( entry->d_name );
			}
		}
	}
	closedir( dir );
	return list.Num();
}

struct SDLThreadStart {
	xthread_t function;
	void *parameter;
};

static int Sys_SDLThreadEntry( void *data ) {
	SDLThreadStart start = *(SDLThreadStart *)data;
	delete (SDLThreadStart *)data;
	return (int)start.function( start.parameter );
}

void Sys_CreateThread( xthread_t function, void *parms, xthreadPriority priority, xthreadInfo &info,
	const char *name, xthreadInfo *threads[MAX_THREADS], int *threadCount ) {
	SDLThreadStart *start = new SDLThreadStart;
	start->function = function;
	start->parameter = parms;
	SDL_Thread *thread = SDL_CreateThread( Sys_SDLThreadEntry, name, start );
	if ( thread == NULL ) {
		delete start;
		Sys_Error( "SDL_CreateThread(%s) failed: %s", name, SDL_GetError() );
	}
	info.threadHandle = (int)thread;
	info.threadId = (unsigned long)SDL_GetThreadID( thread );
	info.name = name;
	if ( *threadCount < MAX_THREADS ) threads[(*threadCount)++] = &info;
}

void Sys_DestroyThread( xthreadInfo &info ) {
	if ( info.threadHandle != 0 ) SDL_WaitThread( (SDL_Thread *)info.threadHandle, NULL );
	info.threadHandle = 0;
}

const char *Sys_GetThreadName( int *index ) {
	const SDL_threadID id = SDL_ThreadID();
	for ( int threadIndex = 0; threadIndex < g_thread_count; ++threadIndex ) {
		if ( g_threads[threadIndex]->threadId == (unsigned long)id ) {
			if ( index != NULL ) *index = threadIndex;
			return g_threads[threadIndex]->name;
		}
	}
	if ( index != NULL ) *index = -1;
	return "main";
}

void Sys_EnterCriticalSection( int index ) {
	assert( index >= 0 && index < MAX_CRITICAL_SECTIONS );
	SDL_LockMutex( criticalSections[index] );
}

void Sys_LeaveCriticalSection( int index ) {
	assert( index >= 0 && index < MAX_CRITICAL_SECTIONS );
	SDL_UnlockMutex( criticalSections[index] );
}

void Sys_WaitForEvent( int index ) {
	assert( index >= 0 && index < MAX_TRIGGER_EVENTS );
	SDL_SemWait( triggerEvents[index] );
}

void Sys_TriggerEvent( int index ) {
	assert( index >= 0 && index < MAX_TRIGGER_EVENTS );
	SDL_SemPost( triggerEvents[index] );
}

void Sys_SetFatalError( const char *error ) {
	idStr::Copynz( fatalError, error != NULL ? error : "", sizeof( fatalError ) );
}

void Sys_DoPreferences( void ) {}

void idSysLocal::OpenURL( const char *url, bool doexit ) {
	if ( SDL_OpenURL( url ) != 0 ) common->Warning( "Could not open URL %s: %s", url, SDL_GetError() );
	if ( doexit ) cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "quit\n" );
}

void idSysLocal::StartProcess( const char *exeName, bool doexit ) {
	const pid_t child = fork();
	if ( child == 0 ) {
		execl( "/bin/sh", "sh", "-c", exeName, (char *)NULL );
		_exit( 127 );
	}
	if ( child < 0 ) common->Warning( "Could not start process %s", exeName );
	if ( doexit ) cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "quit\n" );
}

static unsigned int Sys_SoundThread( void * ) {
	for ( ;; ) {
		SDL_Delay( 16 );
		common->SoundAsync();
	}
	return 0;
}

int main( int argc, char **argv ) {
	SDL_SetMainReady();
	if ( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER ) != 0 ) {
		fprintf( stderr, "SDL_Init failed: %s\n", SDL_GetError() );
		return EXIT_FAILURE;
	}
	common->Init( argc > 1 ? argc - 1 : 0, argc > 1 ? (const char **)&argv[1] : NULL, NULL );
	static xthreadInfo soundThreadInfo;
	Sys_CreateThread( Sys_SoundThread, NULL, THREAD_ABOVE_NORMAL, soundThreadInfo, "Sound", g_threads, &g_thread_count );
	for ( ;; ) common->Frame();
}
