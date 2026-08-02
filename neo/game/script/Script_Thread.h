/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).  

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#ifndef __SCRIPT_THREAD_H__
#define __SCRIPT_THREAD_H__

extern const idEventDef EV_Thread_Execute;
extern const idEventDef EV_Thread_SetCallback;
extern const idEventDef EV_Thread_TerminateThread;
extern const idEventDef EV_Thread_Pause;
extern const idEventDef EV_Thread_Wait;
extern const idEventDef EV_Thread_WaitFrame;
extern const idEventDef EV_Thread_WaitFor;
extern const idEventDef EV_Thread_WaitForThread;
extern const idEventDef EV_Thread_Print;
extern const idEventDef EV_Thread_PrintLn;
extern const idEventDef EV_Thread_Say;
extern const idEventDef EV_Thread_Assert;
extern const idEventDef EV_Thread_Trigger;
extern const idEventDef EV_Thread_SetCvar;
extern const idEventDef EV_Thread_GetCvar;
extern const idEventDef EV_Thread_Random;
extern const idEventDef EV_Thread_GetTime;
extern const idEventDef EV_Thread_KillThread;
extern const idEventDef EV_Thread_SetThreadName;
extern const idEventDef EV_Thread_GetEntity;
extern const idEventDef EV_Thread_Spawn;
extern const idEventDef EV_Thread_SetSpawnArg;
extern const idEventDef EV_Thread_SpawnString;
extern const idEventDef EV_Thread_SpawnFloat;
extern const idEventDef EV_Thread_SpawnVector;
extern const idEventDef EV_Thread_AngToForward;
extern const idEventDef EV_Thread_AngToRight;
extern const idEventDef EV_Thread_AngToUp;
extern const idEventDef EV_Thread_Sine;
extern const idEventDef EV_Thread_Cosine;
extern const idEventDef EV_Thread_Normalize;
extern const idEventDef EV_Thread_VecLength;
extern const idEventDef EV_Thread_VecDotProduct;
extern const idEventDef EV_Thread_VecCrossProduct;
extern const idEventDef EV_Thread_OnSignal;
extern const idEventDef EV_Thread_ClearSignal;
extern const idEventDef EV_Thread_SetCamera;
extern const idEventDef EV_Thread_FirstPerson;
extern const idEventDef EV_Thread_TraceFraction;
extern const idEventDef EV_Thread_TracePos;
extern const idEventDef EV_Thread_FadeIn;
extern const idEventDef EV_Thread_FadeOut;
extern const idEventDef EV_Thread_FadeTo;
extern const idEventDef EV_Thread_Restart;

D3_CLASS()
class idThread : public idClass {
private:
	static idThread				*currentThread;

	idThread					*waitingForThread;
	int							waitingFor;
	int							waitingUntil;
	idInterpreter				interpreter;

	idDict						spawnArgs;
								
	int 						threadNum;
	idStr 						threadName;

	int							lastExecuteTime;
	int							creationTime;

	bool						manualControl;

	static int					threadIndex;
	static idList<idThread *>	threadList;

	static trace_t				trace;

	void						Init( void );
	void						Pause( void );

	D3_EVENT( EV_Thread_Execute, "<execute>", void )
	void						Event_Execute( void );
	D3_EVENT( EV_Thread_SetThreadName, "threadname", void )
	void						Event_SetThreadName( const char *name );

	//
	// script callable Events
	//
	D3_EVENT( EV_Thread_TerminateThread, "terminate", void )
	void						Event_TerminateThread( int num );
	D3_EVENT( EV_Thread_Pause, "pause", void )
	void						Event_Pause( void );
	D3_EVENT( EV_Thread_Wait )
	void						Event_Wait( float time );
	D3_EVENT( EV_Thread_WaitFrame )
	void						Event_WaitFrame( void );
	D3_EVENT( EV_Thread_WaitFor, "waitFor", void )
	void						Event_WaitFor( idEntity *ent );
	D3_EVENT( EV_Thread_WaitForThread, "waitForThread", void )
	void						Event_WaitForThread( int num );
	D3_NODE( Title = "Print", Category = "System|Debug", Description = "Writes text to the developer console.", Keywords = "log console debug" )
	D3_EVENT( EV_Thread_Print, "print", void )
	void						Event_Print( const char *text );
	D3_EVENT( EV_Thread_PrintLn, "println", void )
	void						Event_PrintLn( const char *text );
	D3_EVENT( EV_Thread_Say, "say", void )
	void						Event_Say( const char *text );
	D3_EVENT( EV_Thread_Assert, "assert", void )
	void						Event_Assert( float value );
	D3_EVENT( EV_Thread_Trigger, "trigger", void )
	void						Event_Trigger( idEntity *ent );
	D3_EVENT( EV_Thread_SetCvar, "setcvar", void )
	void						Event_SetCvar( const char *name, const char *value ) const;
	D3_EVENT( EV_Thread_GetCvar, "getcvar", string )
	void						Event_GetCvar( const char *name ) const;
	D3_EVENT( EV_Thread_Random, "random", float )
	void						Event_Random( float range ) const;
	D3_EVENT( EV_Thread_RandomInt, "randomInt", integer )
	void						Event_RandomInt( int range ) const;
	D3_EVENT( EV_Thread_GetTime, "getTime", float )
	void						Event_GetTime( void );
	D3_EVENT( EV_Thread_KillThread, "killthread", void )
	void						Event_KillThread( const char *name );
	D3_EVENT( EV_Thread_GetEntity, "getEntity", entity )
	void						Event_GetEntity( const char *name );
	D3_EVENT( EV_Thread_Spawn, "spawn", entity )
	void						Event_Spawn( const char *classname );
	D3_EVENT( EV_Thread_CopySpawnArgs, "copySpawnArgs", void )
	void						Event_CopySpawnArgs( idEntity *ent );
	D3_EVENT( EV_Thread_SetSpawnArg, "setSpawnArg", void )
	void						Event_SetSpawnArg( const char *key, const char *value );
	D3_EVENT( EV_Thread_SpawnString, "SpawnString", string )
	void						Event_SpawnString( const char *key, const char *defaultvalue );
	D3_EVENT( EV_Thread_SpawnFloat, "SpawnFloat", float )
	void						Event_SpawnFloat( const char *key, float defaultvalue );
	D3_EVENT( EV_Thread_SpawnVector, "SpawnVector", vector )
	void						Event_SpawnVector( const char *key, idVec3 &defaultvalue );
	D3_EVENT( EV_Thread_ClearPersistantArgs, "clearPersistantArgs", void )
	void						Event_ClearPersistantArgs( void );
	D3_EVENT( EV_Thread_SetPersistantArg, "setPersistantArg", void )
	void 						Event_SetPersistantArg( const char *key, const char *value );
	D3_EVENT( EV_Thread_GetPersistantString, "getPersistantString", string )
	void 						Event_GetPersistantString( const char *key );
	D3_EVENT( EV_Thread_GetPersistantFloat, "getPersistantFloat", float )
	void 						Event_GetPersistantFloat( const char *key );
	D3_EVENT( EV_Thread_GetPersistantVector, "getPersistantVector", vector )
	void 						Event_GetPersistantVector( const char *key );
	D3_EVENT( EV_Thread_AngToForward, "angToForward", vector )
	void						Event_AngToForward( idAngles &ang );
	D3_EVENT( EV_Thread_AngToRight, "angToRight", vector )
	void						Event_AngToRight( idAngles &ang );
	D3_EVENT( EV_Thread_AngToUp, "angToUp", vector )
	void						Event_AngToUp( idAngles &ang );
	D3_EVENT( EV_Thread_Sine, "sin", float )
	void						Event_GetSine( float angle );
	D3_EVENT( EV_Thread_Cosine, "cos", float )
	void						Event_GetCosine( float angle );
	D3_EVENT( EV_Thread_ArcSine, "asin", float )
	void						Event_GetArcSine( float a );
	D3_EVENT( EV_Thread_ArcCosine, "acos", float )
	void						Event_GetArcCosine( float a );
	D3_EVENT( EV_Thread_SquareRoot, "sqrt", float )
	void						Event_GetSquareRoot( float theSquare );
	D3_EVENT( EV_Thread_Normalize, "vecNormalize", vector )
	void						Event_VecNormalize( idVec3 &vec );
	D3_EVENT( EV_Thread_VecLength, "vecLength", float )
	void						Event_VecLength( idVec3 &vec );
	D3_EVENT( EV_Thread_VecDotProduct, "DotProduct", float )
	void						Event_VecDotProduct( idVec3 &vec1, idVec3 &vec2 );
	D3_EVENT( EV_Thread_VecCrossProduct, "CrossProduct", vector )
	void						Event_VecCrossProduct( idVec3 &vec1, idVec3 &vec2 );
	D3_EVENT( EV_Thread_VecToAngles, "VecToAngles", vector )
	void						Event_VecToAngles( idVec3 &vec );
	D3_EVENT( EV_Thread_VecToOrthoBasisAngles, "VecToOrthoBasisAngles", vector )
	void						Event_VecToOrthoBasisAngles( idVec3 &vec );
	D3_EVENT( EV_Thread_RotateVector, "rotateVector", vector )
	void						Event_RotateVector( idVec3 &vec, idVec3 &ang );
	D3_EVENT( EV_Thread_OnSignal, "onSignal", void )
	void						Event_OnSignal( int signal, idEntity *ent, const char *func );
	D3_EVENT( EV_Thread_ClearSignal, "clearSignalThread", void )
	void						Event_ClearSignalThread( int signal, idEntity *ent );
	D3_EVENT( EV_Thread_SetCamera, "setCamera", void )
	void						Event_SetCamera( idEntity *ent );
	D3_EVENT( EV_Thread_FirstPerson, "firstPerson", void )
	void						Event_FirstPerson( void );
	D3_EVENT( EV_Thread_Trace, "trace", float )
	void						Event_Trace( const idVec3 &start, const idVec3 &end, const idVec3 &mins, const idVec3 &maxs, int contents_mask, idEntity *passEntity );
	D3_EVENT( EV_Thread_TracePoint, "tracePoint", float )
	void						Event_TracePoint( const idVec3 &start, const idVec3 &end, int contents_mask, idEntity *passEntity );
	D3_EVENT( EV_Thread_GetTraceFraction, "getTraceFraction", float )
	void						Event_GetTraceFraction( void );
	D3_EVENT( EV_Thread_GetTraceEndPos, "getTraceEndPos", vector )
	void						Event_GetTraceEndPos( void );
	D3_EVENT( EV_Thread_GetTraceNormal, "getTraceNormal", vector )
	void						Event_GetTraceNormal( void );
	D3_EVENT( EV_Thread_GetTraceEntity, "getTraceEntity", entity )
	void						Event_GetTraceEntity( void );
	D3_EVENT( EV_Thread_GetTraceJoint, "getTraceJoint", string )
	void						Event_GetTraceJoint( void );
	D3_EVENT( EV_Thread_GetTraceBody, "getTraceBody", string )
	void						Event_GetTraceBody( void );
	D3_EVENT( EV_Thread_FadeIn, "fadeIn", void )
	void						Event_FadeIn( idVec3 &color, float time );
	D3_EVENT( EV_Thread_FadeOut, "fadeOut", void )
	void						Event_FadeOut( idVec3 &color, float time );
	D3_EVENT( EV_Thread_FadeTo, "fadeTo", void )
	void						Event_FadeTo( idVec3 &color, float alpha, float time );
	D3_EVENT( EV_SetShaderParm )
	void						Event_SetShaderParm( int parmnum, float value );
	D3_EVENT( EV_Thread_StartMusic, "music", void )
	void						Event_StartMusic( const char *name );
	D3_EVENT( EV_Thread_Warning, "warning", void )
	void						Event_Warning( const char *text );
	D3_EVENT( EV_Thread_Error, "error", void )
	void						Event_Error( const char *text );
	D3_EVENT( EV_Thread_StrLen, "strLength", integer )
	void 						Event_StrLen( const char *string );
	D3_EVENT( EV_Thread_StrLeft, "strLeft", string )
	void 						Event_StrLeft( const char *string, int num );
	D3_EVENT( EV_Thread_StrRight, "strRight", string )
	void 						Event_StrRight( const char *string, int num );
	D3_EVENT( EV_Thread_StrSkip, "strSkip", string )
	void 						Event_StrSkip( const char *string, int num );
	D3_EVENT( EV_Thread_StrMid, "strMid", string )
	void 						Event_StrMid( const char *string, int start, int num );
	D3_EVENT( EV_Thread_StrToFloat, "strToFloat", float )
	void						Event_StrToFloat( const char *string );
	D3_EVENT( EV_Thread_RadiusDamage, "radiusDamage", void )
	void						Event_RadiusDamage( const idVec3 &origin, D3_NULLABLE idEntity *inflictor, D3_NULLABLE idEntity *attacker, D3_NULLABLE idEntity *ignore, const char *damageDefName, float dmgPower );
	D3_EVENT( EV_Thread_IsClient, "isClient", float )
	void						Event_IsClient( void );
	D3_EVENT( EV_Thread_IsMultiplayer, "isMultiplayer", float )
	void 						Event_IsMultiplayer( void );
	D3_EVENT( EV_Thread_GetFrameTime, "getFrameTime", float )
	void 						Event_GetFrameTime( void );
	D3_EVENT( EV_Thread_GetTicsPerSecond, "getTicsPerSecond", float )
	void 						Event_GetTicsPerSecond( void );
	D3_EVENT( EV_CacheSoundShader )
	void						Event_CacheSoundShader( const char *soundName );
	D3_EVENT( EV_Thread_DebugLine, "debugLine", void )
	void						Event_DebugLine( const idVec3 &color, const idVec3 &start, const idVec3 &end, const float lifetime );
	D3_EVENT( EV_Thread_DebugArrow, "debugArrow", void )
	void						Event_DebugArrow( const idVec3 &color, const idVec3 &start, const idVec3 &end, const int size, const float lifetime );
	D3_EVENT( EV_Thread_DebugCircle, "debugCircle", void )
	void						Event_DebugCircle( const idVec3 &color, const idVec3 &origin, const idVec3 &dir, const float radius, const int numSteps, const float lifetime );
	D3_EVENT( EV_Thread_DebugBounds, "debugBounds", void )
	void						Event_DebugBounds( const idVec3 &color, const idVec3 &mins, const idVec3 &maxs, const float lifetime );
	D3_EVENT( EV_Thread_DrawText, "drawText", void )
	void						Event_DrawText( const char *text, const idVec3 &origin, float scale, const idVec3 &color, const int align, const float lifetime );
	D3_EVENT( EV_Thread_InfluenceActive, "influenceActive", integer )
	void						Event_InfluenceActive( void );

public:							
								CLASS_PROTOTYPE( idThread );
								
								idThread();
								idThread( idEntity *self, const function_t *func );
								idThread( const function_t *func );
								idThread( idInterpreter *source, const function_t *func, int args );
								idThread( idInterpreter *source, idEntity *self, const function_t *func, int args );

	virtual						~idThread();

								// tells the thread manager not to delete this thread when it ends
	void						ManualDelete( void );

	// save games
	void						Save( idSaveGame *savefile ) const;				// archives object for save game file
	void						Restore( idRestoreGame *savefile );				// unarchives object from save game file

	void						EnableDebugInfo( void ) { interpreter.debug = true; };
	void						DisableDebugInfo( void ) { interpreter.debug = false; };

	void						WaitMS( int time );
	void						WaitSec( float time );
	void						WaitFrame( void );
								
								// NOTE: If this is called from within a event called by this thread, the function arguments will be invalid after calling this function.
	void						CallFunction( const function_t	*func, bool clearStack );

								// NOTE: If this is called from within a event called by this thread, the function arguments will be invalid after calling this function.
	void						CallFunction( idEntity *obj, const function_t *func, bool clearStack );

	void						DisplayInfo();
	static idThread				*GetThread( int num );
	static void					ListThreads_f( const idCmdArgs &args );
	static void					Restart( void );
	static void					ObjectMoveDone( int threadnum, idEntity *obj );
								
	static idList<idThread*>&	GetThreads ( void );
	
	bool						IsDoneProcessing ( void );
	bool						IsDying			 ( void );	
								
	void						End( void );
	static void					KillThread( const char *name );
	static void					KillThread( int num );
	bool						Execute( void );
	void						ManualControl( void ) { manualControl = true; CancelEvents( &EV_Thread_Execute ); };
	void						DoneProcessing( void ) { interpreter.doneProcessing = true; };
	void						ContinueProcessing( void ) { interpreter.doneProcessing = false; };
	bool						ThreadDying( void ) { return interpreter.threadDying; };
	void						EndThread( void ) { interpreter.threadDying = true; };
	bool						IsWaiting( void );
	void						ClearWaitFor( void );
	bool						IsWaitingFor( idEntity *obj );
	void						ObjectMoveDone( idEntity *obj );
	void						ThreadCallback( idThread *thread );
	void						DelayedStart( int delay );
	bool						Start( void );
	idThread					*WaitingOnThread( void );
	void						SetThreadNum( int num );
	int 						GetThreadNum( void );
	void						SetThreadName( const char *name );
	const char					*GetThreadName( void );

	void						Error( const char *fmt, ... ) const id_attribute((format(printf,2,3)));
	void						Warning( const char *fmt, ... ) const id_attribute((format(printf,2,3)));
								
	static idThread				*CurrentThread( void );
	static int					CurrentThreadNum( void );
	static bool					BeginMultiFrameEvent( idEntity *ent, const idEventDef *event );
	static void					EndMultiFrameEvent( idEntity *ent, const idEventDef *event );

	static void					ReturnString( const char *text );
	static void					ReturnFloat( float value );
	static void					ReturnInt( int value );
	static void					ReturnVector( idVec3 const &vec );
	static void					ReturnEntity( idEntity *ent );
};

/*
================
idThread::WaitingOnThread
================
*/
ID_INLINE idThread *idThread::WaitingOnThread( void ) {
	return waitingForThread;
}

/*
================
idThread::SetThreadNum
================
*/
ID_INLINE void idThread::SetThreadNum( int num ) {
	threadNum = num;
}

/*
================
idThread::GetThreadNum
================
*/
ID_INLINE int idThread::GetThreadNum( void ) {
	return threadNum;
}

/*
================
idThread::GetThreadName
================
*/
ID_INLINE const char *idThread::GetThreadName( void ) {
	return threadName.c_str();
}

/*
================
idThread::GetThreads
================
*/
ID_INLINE idList<idThread*>& idThread::GetThreads ( void ) {
	return threadList;
}	

/*
================
idThread::IsDoneProcessing
================
*/
ID_INLINE bool idThread::IsDoneProcessing ( void ) {
	return interpreter.doneProcessing;
}

/*
================
idThread::IsDying
================
*/
ID_INLINE bool idThread::IsDying ( void ) {
	return interpreter.threadDying;
}

#endif /* !__SCRIPT_THREAD_H__ */
