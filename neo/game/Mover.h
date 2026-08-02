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

#ifndef __GAME_MOVER_H__
#define __GAME_MOVER_H__

extern const idEventDef EV_TeamBlocked;
extern const idEventDef EV_PartBlocked;
extern const idEventDef EV_ReachedPos;
extern const idEventDef EV_ReachedAng;

/*
===============================================================================

  General movers.

===============================================================================
*/

D3_CLASS()
class idMover : public idEntity {
public:
	CLASS_PROTOTYPE( idMover );

							idMover( void );

	void					Spawn( void );

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	virtual void			Killed( idEntity *inflictor, idEntity *attacker, int damage, const idVec3 &dir, int location );

	virtual void			WriteToSnapshot( idBitMsgDelta &msg ) const;
	virtual void			ReadFromSnapshot( const idBitMsgDelta &msg );

	virtual void			Hide( void );
	virtual void			Show( void );

	void					SetPortalState( bool open );

protected:
	typedef enum {
		ACCELERATION_STAGE,
		LINEAR_STAGE,
		DECELERATION_STAGE,
		FINISHED_STAGE
	} moveStage_t;

	typedef enum {
		MOVER_NONE,
		MOVER_ROTATING,
		MOVER_MOVING,
		MOVER_SPLINE
	} moverCommand_t;

	//
	// mover directions.  make sure to change script/doom_defs.script if you add any, or change their order
	//
	typedef enum {
		DIR_UP				= -1,
		DIR_DOWN			= -2,
		DIR_LEFT			= -3,
		DIR_RIGHT			= -4,
		DIR_FORWARD			= -5,
		DIR_BACK			= -6,
		DIR_REL_UP			= -7,
		DIR_REL_DOWN		= -8,
		DIR_REL_LEFT		= -9,
		DIR_REL_RIGHT		= -10,
		DIR_REL_FORWARD		= -11,
		DIR_REL_BACK		= -12
	} moverDir_t;

	typedef struct {
		moveStage_t			stage;
		int					acceleration;
		int					movetime;
		int					deceleration;
		idVec3				dir;
	} moveState_t;

	typedef struct {
		moveStage_t			stage;
		int					acceleration;
		int					movetime;
		int					deceleration;
		idAngles			rot;
	} rotationState_t;

	idPhysics_Parametric	physicsObj;

	D3_EVENT( EV_Mover_OpenPortal, "openPortal", void )
	void					Event_OpenPortal( void );
	D3_EVENT( EV_Mover_ClosePortal, "closePortal", void )
	void					Event_ClosePortal( void );
	D3_EVENT( EV_PartBlocked, "<partblocked>", void )
	void					Event_PartBlocked( idEntity *blockingEntity );

	void					MoveToPos( const idVec3 &pos);
	void					UpdateMoveSound( moveStage_t stage );
	void					UpdateRotationSound( moveStage_t stage );
	void					SetGuiStates( const char *state );
	void					FindGuiTargets( void );
	void					SetGuiState( const char *key, const char *val ) const;

	virtual void			DoneMoving( void );
	virtual void			DoneRotating( void );
	virtual void			BeginMove( idThread *thread = NULL );
	virtual void			BeginRotation( idThread *thread, bool stopwhendone );
	moveState_t				move;

private:
	rotationState_t			rot;

	int						move_thread;
	int						rotate_thread;
	idAngles				dest_angles;
	idAngles				angle_delta;
	idVec3					dest_position;
	idVec3					move_delta;
	float					move_speed;
	int						move_time;
	int						deceltime;
	int						acceltime;
	bool					stopRotation;
	bool					useSplineAngles;
	idEntityPtr<idEntity>	splineEnt;
	moverCommand_t			lastCommand;
	float					damage;

	qhandle_t				areaPortal;		// 0 = no portal

	idList< idEntityPtr<idEntity> >	guiTargets;

	void					VectorForDir( float dir, idVec3 &vec );
	idCurve_Spline<idVec3> *GetSpline( idEntity *splineEntity ) const;

	D3_EVENT( EV_Thread_SetCallback )
	void					Event_SetCallback( void );	
	D3_EVENT( EV_TeamBlocked, "<teamblocked>", void )
	void					Event_TeamBlocked( idEntity *blockedPart, idEntity *blockingEntity );
	D3_EVENT( EV_StopMoving, "stopMoving", void )
	void					Event_StopMoving( void );
	D3_EVENT( EV_StopRotating, "stopRotating", void )
	void					Event_StopRotating( void );
	D3_EVENT( EV_ReachedPos, "<reachedpos>", void )
	void					Event_UpdateMove( void );
	D3_EVENT( EV_ReachedAng, "<reachedang>", void )
	void					Event_UpdateRotation( void );
	D3_EVENT( EV_Speed, "speed", void )
	void					Event_SetMoveSpeed( float speed );
	D3_EVENT( EV_Time, "time", void )
	void					Event_SetMoveTime( float time );
	D3_EVENT( EV_DecelTime, "decelTime", void )
	void					Event_SetDecelerationTime( float time );
	D3_EVENT( EV_AccelTime, "accelTime", void )
	void					Event_SetAccellerationTime( float time );
	D3_EVENT( EV_MoveTo, "moveTo", void )
	void					Event_MoveTo( idEntity *ent );
	D3_EVENT( EV_MoveToPos, "moveToPos", void )
	void					Event_MoveToPos( idVec3 &pos );
	D3_EVENT( EV_Move, "move", void )
	void					Event_MoveDir( float angle, float distance );
	D3_EVENT( EV_MoveAccelerateTo, "accelTo", void )
	void					Event_MoveAccelerateTo( float speed, float time );
	D3_EVENT( EV_MoveDecelerateTo, "decelTo", void )
	void					Event_MoveDecelerateTo( float speed, float time );
	D3_EVENT( EV_RotateDownTo, "rotateDownTo", void )
	void					Event_RotateDownTo( int axis, float angle );
	D3_EVENT( EV_RotateUpTo, "rotateUpTo", void )
	void					Event_RotateUpTo( int axis, float angle );
	D3_EVENT( EV_RotateTo, "rotateTo", void )
	void					Event_RotateTo( idAngles &angles );
	D3_EVENT( EV_Rotate, "rotate", void )
	void					Event_Rotate( idAngles &angles );
	D3_EVENT( EV_RotateOnce, "rotateOnce", void )
	void					Event_RotateOnce( idAngles &angles );
	D3_EVENT( EV_Bob, "bob", void )
	void					Event_Bob( float speed, float phase, idVec3 &depth );
	D3_EVENT( EV_Sway, "sway", void )
	void					Event_Sway( float speed, float phase, idAngles &depth );
	D3_EVENT( EV_AccelSound, "accelSound", void )
	void					Event_SetAccelSound( const char *sound );
	D3_EVENT( EV_DecelSound, "decelSound", void )
	void					Event_SetDecelSound( const char *sound );
	D3_EVENT( EV_MoveSound, "moveSound", void )
	void					Event_SetMoveSound( const char *sound );
	D3_EVENT( EV_FindGuiTargets, "<FindGuiTargets>", void )
	void					Event_FindGuiTargets( void );
	D3_EVENT( EV_Mover_InitGuiTargets, "<initguitargets>", void )
	void					Event_InitGuiTargets( void );
	D3_EVENT( EV_EnableSplineAngles, "enableSplineAngles", void )
	void					Event_EnableSplineAngles( void );
	D3_EVENT( EV_DisableSplineAngles, "disableSplineAngles", void )
	void					Event_DisableSplineAngles( void );
	D3_EVENT( EV_RemoveInitialSplineAngles, "removeInitialSplineAngles", void )
	void					Event_RemoveInitialSplineAngles( void );
	D3_EVENT( EV_StartSpline, "startSpline", void )
	void					Event_StartSpline( idEntity *splineEntity );
	D3_EVENT( EV_StopSpline, "stopSpline", void )
	void					Event_StopSpline( void );
	D3_EVENT( EV_Activate )
	void					Event_Activate( idEntity *activator );
	D3_EVENT( EV_PostRestore, "<postrestore>", void )
	void					Event_PostRestore( int start, int total, int accel, int decel, int useSplineAng );
	D3_EVENT( EV_IsMoving, "isMoving", integer )
	void					Event_IsMoving( void );
	D3_EVENT( EV_IsRotating, "isRotating", integer )
	void					Event_IsRotating( void );
};

D3_CLASS()
class idSplinePath : public idEntity {
public:
	CLASS_PROTOTYPE( idSplinePath );

							idSplinePath();

	void					Spawn( void );
};


struct floorInfo_s {
	idVec3					pos;
	idStr					door;
	int						floor;
};

D3_CLASS()
class idElevator : public idMover {
public:
	CLASS_PROTOTYPE( idElevator );
	D3_EVENT( EV_PartBlocked, idElevator::Event_PartBlocked )

							idElevator( void );

	void					Spawn();

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	virtual bool			HandleSingleGuiCommand( idEntity *entityGui, idLexer *src );
	D3_EVENT( EV_GotoFloor, "gotoFloor", void )
	void					Event_GotoFloor( int floor );
	floorInfo_s *			GetFloorInfo( int floor );

protected:
	virtual void			DoneMoving( void );
	virtual void			BeginMove( idThread *thread = NULL );
	void					SpawnTrigger( const idVec3 &pos );
	void					GetLocalTriggerPosition();
	D3_EVENT( EV_Touch )
	void					Event_Touch( idEntity *other, trace_t *trace );

private:
	typedef enum {
		INIT,
		IDLE,
		WAITING_ON_DOORS
	} elevatorState_t;

	elevatorState_t			state;
	idList<floorInfo_s>		floorInfo;
	int						currentFloor;
	int						pendingFloor;
	int						lastFloor;
	bool					controlsDisabled;
	float					returnTime;
	int						returnFloor;
	int						lastTouchTime;

	class idDoor *			GetDoor( const char *name );
	void					Think( void );
	void					OpenInnerDoor( void );
	void					OpenFloorDoor( int floor );
	void					CloseAllDoors( void );
	void					DisableAllDoors( void );
	void					EnableProperDoors( void );

	D3_EVENT( EV_TeamBlocked )
	void					Event_TeamBlocked( idEntity *blockedEntity, idEntity *blockingEntity );
	D3_EVENT( EV_Activate )
	void					Event_Activate( idEntity *activator );
	D3_EVENT( EV_PostArrival, "postArrival", void )
	void					Event_PostFloorArrival();

	D3_EVENT( EV_SetGuiStates, "setGuiStates", void )
	void					Event_SetGuiStates();

};


/*
===============================================================================

  Binary movers.

===============================================================================
*/

D3_ENUM( BlueprintType )
typedef enum {
	MOVER_POS1,
	MOVER_POS2,
	MOVER_1TO2,
	MOVER_2TO1
} moverState_t;

D3_CLASS()
class idMover_Binary : public idEntity {
public:
	CLASS_PROTOTYPE( idMover_Binary );

							idMover_Binary();
							~idMover_Binary();

	void					Spawn( void );

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	virtual void			PreBind( void );
	virtual void			PostBind( void );

	void					Enable( bool b );
	void					InitSpeed( idVec3 &mpos1, idVec3 &mpos2, float mspeed, float maccelTime, float mdecelTime );
	void					InitTime( idVec3 &mpos1, idVec3 &mpos2, float mtime, float maccelTime, float mdecelTime );
	void					GotoPosition1( void );
	void					GotoPosition2( void );
	void					Use_BinaryMover( idEntity *activator );
	void					SetGuiStates( const char *state );
	void					UpdateBuddies( int val );
	idMover_Binary *		GetActivateChain( void ) const { return activateChain; }
	idMover_Binary *		GetMoveMaster( void ) const { return moveMaster; }
	void					BindTeam( idEntity *bindTo );
	void					SetBlocked( bool b );
	bool					IsBlocked( void );
	idEntity *				GetActivator( void ) const;

	virtual void			WriteToSnapshot( idBitMsgDelta &msg ) const;
	virtual void			ReadFromSnapshot( const idBitMsgDelta &msg );

	void					SetPortalState( bool open );

protected:
	idVec3					pos1;
	idVec3					pos2;
	moverState_t			moverState;
	idMover_Binary *		moveMaster;
	idMover_Binary *		activateChain;
	int						soundPos1;
	int						sound1to2;
	int						sound2to1;
	int						soundPos2;
	int						soundLoop;
	float					wait;
	float					damage;
	int						duration;
	int						accelTime;
	int						decelTime;
	idEntityPtr<idEntity>	activatedBy;
	int						stateStartTime;
	idStr					team;
	bool					enabled;
	int						move_thread;
	int						updateStatus;		// 1 = lock behaviour, 2 = open close status
	idStrList				buddies;
	idPhysics_Parametric	physicsObj;
	qhandle_t				areaPortal;			// 0 = no portal
	bool					blocked;
	bool					playerOnly;
	idList< idEntityPtr<idEntity> >	guiTargets;

	void					MatchActivateTeam( moverState_t newstate, int time );
	void					JoinActivateTeam( idMover_Binary *master );

	void					UpdateMoverSound( moverState_t state );
	void					SetMoverState( moverState_t newstate, int time );
	moverState_t			GetMoverState( void ) const { return moverState; }
	void					FindGuiTargets( void );
	void					SetGuiState( const char *key, const char *val ) const;

	D3_EVENT( EV_Thread_SetCallback )
	void					Event_SetCallback( void );
	D3_EVENT( EV_Mover_ReturnToPos1, "<returntopos1>", void )
	void					Event_ReturnToPos1( void );
	D3_EVENT( EV_Activate )
	void					Event_Use_BinaryMover( idEntity *activator );
	D3_EVENT( EV_ReachedPos )
	void					Event_Reached_BinaryMover( void );
	D3_EVENT( EV_Mover_MatchTeam, "<matchteam>", void )
	void					Event_MatchActivateTeam( moverState_t newstate, int time );
	D3_EVENT( EV_Mover_Enable, "enable", void )
	void					Event_Enable( void );
	D3_EVENT( EV_Mover_Disable, "disable", void )
	void					Event_Disable( void );
	D3_EVENT( EV_Mover_OpenPortal )
	void					Event_OpenPortal( void );
	D3_EVENT( EV_Mover_ClosePortal )
	void					Event_ClosePortal( void );
	D3_EVENT( EV_FindGuiTargets )
	void					Event_FindGuiTargets( void );
	D3_EVENT( EV_Mover_InitGuiTargets )
	void					Event_InitGuiTargets( void );

	static void				GetMovedir( float dir, idVec3 &movedir );
};

D3_CLASS()
class idDoor : public idMover_Binary {
public:
	CLASS_PROTOTYPE( idDoor );

							idDoor( void );
							~idDoor( void );

	void					Spawn( void );

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	virtual void			Think( void );
	virtual void			PreBind( void );
	virtual void			PostBind( void );
	virtual void			Hide( void );
	virtual void			Show( void );

	bool					IsOpen( void );
	bool					IsNoTouch( void );
	bool					AllowPlayerOnly( idEntity *ent );
	int						IsLocked( void );
	void					Lock( int f );
	void					Use( idEntity *other, idEntity *activator );
	void					Close( void );
	void					Open( void );
	void					SetCompanion( idDoor *door );

private:
	float					triggersize;
	bool					crusher;
	bool					noTouch;
	bool					aas_area_closed;
	idStr					buddyStr;
	idClipModel *			trigger;
	idClipModel *			sndTrigger;
	int						nextSndTriggerTime;
	idVec3					localTriggerOrigin;
	idMat3					localTriggerAxis;
	idStr					requires;
	int						removeItem;
	idStr					syncLock;
	int						normalAxisIndex;		// door faces X or Y for spectator teleports
	idDoor *				companionDoor;

	void					SetAASAreaState( bool closed );

	void					GetLocalTriggerPosition( const idClipModel *trigger );
	void					CalcTriggerBounds( float size, idBounds &bounds );

	D3_EVENT( EV_ReachedPos )
	void					Event_Reached_BinaryMover( void );
	D3_EVENT( EV_TeamBlocked )
	void					Event_TeamBlocked( idEntity *blockedEntity, idEntity *blockingEntity );
	D3_EVENT( EV_PartBlocked )
	void					Event_PartBlocked( idEntity *blockingEntity );
	D3_EVENT( EV_Touch )
	void					Event_Touch( idEntity *other, trace_t *trace );
	D3_EVENT( EV_Activate )
	void					Event_Activate( idEntity *activator );
	D3_EVENT( EV_Door_StartOpen, "<startOpen>", void )
	void					Event_StartOpen( void );
	D3_EVENT( EV_Door_SpawnDoorTrigger, "<spawnDoorTrigger>", void )
	void					Event_SpawnDoorTrigger( void );
	D3_EVENT( EV_Door_SpawnSoundTrigger, "<spawnSoundTrigger>", void )
	void					Event_SpawnSoundTrigger( void );
	D3_EVENT( EV_Door_Close, "close", void )
	void					Event_Close( void );
	D3_EVENT( EV_Door_Open, "open", void )
	void					Event_Open( void );
	D3_EVENT( EV_Door_Lock, "lock", void )
	void					Event_Lock( int f );
	D3_EVENT( EV_Door_IsOpen, "isOpen", float )
	void					Event_IsOpen( void );
	D3_EVENT( EV_Door_IsLocked, "isLocked", float )
	void					Event_Locked( void );
	D3_EVENT( EV_SpectatorTouch, "spectatorTouch", void )
	void					Event_SpectatorTouch( idEntity *other, trace_t *trace );
	D3_EVENT( EV_Mover_OpenPortal )
	void					Event_OpenPortal( void );
	D3_EVENT( EV_Mover_ClosePortal )
	void					Event_ClosePortal( void );
};

D3_CLASS()
class idPlat : public idMover_Binary {
public:
	CLASS_PROTOTYPE( idPlat );

							idPlat( void );
							~idPlat( void );

	void					Spawn( void );

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	virtual void			Think( void );
	virtual void			PreBind( void );
	virtual void			PostBind( void );

private:
	idClipModel *			trigger;
	idVec3					localTriggerOrigin;
	idMat3					localTriggerAxis;

	void					GetLocalTriggerPosition( const idClipModel *trigger );
	void					SpawnPlatTrigger( idVec3 &pos );

	D3_EVENT( EV_TeamBlocked )
	void					Event_TeamBlocked( idEntity *blockedEntity, idEntity *blockingEntity );
	D3_EVENT( EV_PartBlocked )
	void					Event_PartBlocked( idEntity *blockingEntity );
	D3_EVENT( EV_Touch )
	void					Event_Touch( idEntity *other, trace_t *trace );
};


/*
===============================================================================

  Special periodic movers.

===============================================================================
*/

D3_CLASS()
class idMover_Periodic : public idEntity {
public:
	CLASS_PROTOTYPE( idMover_Periodic );

							idMover_Periodic( void );

	void					Spawn( void );
	
	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	virtual void			Think( void );

	virtual void			WriteToSnapshot( idBitMsgDelta &msg ) const;
	virtual void			ReadFromSnapshot( const idBitMsgDelta &msg );

protected:
	idPhysics_Parametric	physicsObj;
	float					damage;

	D3_EVENT( EV_TeamBlocked )
	void					Event_TeamBlocked( idEntity *blockedEntity, idEntity *blockingEntity );
	D3_EVENT( EV_PartBlocked )
	void					Event_PartBlocked( idEntity *blockingEntity );
};

D3_CLASS()
class idRotater : public idMover_Periodic {
public:
	CLASS_PROTOTYPE( idRotater );

							idRotater( void );

	void					Spawn( void );

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

private:
	idEntityPtr<idEntity>	activatedBy;

	D3_EVENT( EV_Activate )
	void					Event_Activate( idEntity *activator );
};

D3_CLASS()
class idBobber : public idMover_Periodic {
public:
	CLASS_PROTOTYPE( idBobber );

							idBobber( void );

	void					Spawn( void );

private:
};

D3_CLASS()
class idPendulum : public idMover_Periodic {
public:
	CLASS_PROTOTYPE( idPendulum );

							idPendulum( void );

	void					Spawn( void );

private:
};

D3_CLASS()
class idRiser : public idMover_Periodic {
public:
	CLASS_PROTOTYPE( idRiser );

	idRiser( void );

	void					Spawn( void );

private:
	D3_EVENT( EV_Activate )
	void					Event_Activate( idEntity *activator );
};

#endif /* !__GAME_MOVER_H__ */
