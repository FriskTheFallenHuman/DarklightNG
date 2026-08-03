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

#ifndef __AI_H__
#define __AI_H__

/*
===============================================================================

	idAI

===============================================================================
*/

const float	SQUARE_ROOT_OF_2			= 1.414213562f;
const float	AI_TURN_PREDICTION			= 0.2f;
const float	AI_TURN_SCALE				= 60.0f;
const float	AI_SEEK_PREDICTION			= 0.3f;
const float	AI_FLY_DAMPENING			= 0.15f;
const float	AI_HEARING_RANGE			= 2048.0f;
const int	DEFAULT_FLY_OFFSET			= 68;

#define ATTACK_IGNORE			0
#define ATTACK_ON_DAMAGE		1
#define ATTACK_ON_ACTIVATE		2
#define ATTACK_ON_SIGHT			4

// defined in script/ai_base.script.  please keep them up to date.
typedef enum {
	MOVETYPE_DEAD,
	MOVETYPE_ANIM,
	MOVETYPE_SLIDE,
	MOVETYPE_FLY,
	MOVETYPE_STATIC,
	NUM_MOVETYPES
} moveType_t;

typedef enum {
	MOVE_NONE,
	MOVE_FACE_ENEMY,
	MOVE_FACE_ENTITY,

	// commands < NUM_NONMOVING_COMMANDS don't cause a change in position
	NUM_NONMOVING_COMMANDS,

	MOVE_TO_ENEMY = NUM_NONMOVING_COMMANDS,
	MOVE_TO_ENEMYHEIGHT,
	MOVE_TO_ENTITY, 
	MOVE_OUT_OF_RANGE,
	MOVE_TO_ATTACK_POSITION,
	MOVE_TO_COVER,
	MOVE_TO_POSITION,
	MOVE_TO_POSITION_DIRECT,
	MOVE_SLIDE_TO_POSITION,
	MOVE_WANDER,
	NUM_MOVE_COMMANDS
} moveCommand_t;

typedef enum {
	TALK_NEVER,
	TALK_DEAD,
	TALK_OK,
	TALK_BUSY,
	NUM_TALK_STATES
} talkState_t;

//
// status results from move commands
// make sure to change script/doom_defs.script if you add any, or change their order
//
typedef enum {
	MOVE_STATUS_DONE,
	MOVE_STATUS_MOVING,
	MOVE_STATUS_WAITING,
	MOVE_STATUS_DEST_NOT_FOUND,
	MOVE_STATUS_DEST_UNREACHABLE,
	MOVE_STATUS_BLOCKED_BY_WALL,
	MOVE_STATUS_BLOCKED_BY_OBJECT,
	MOVE_STATUS_BLOCKED_BY_ENEMY,
	MOVE_STATUS_BLOCKED_BY_MONSTER
} moveStatus_t;

#define	DI_NODIR	-1

// obstacle avoidance
typedef struct obstaclePath_s {
	idVec3				seekPos;					// seek position avoiding obstacles
	idEntity *			firstObstacle;				// if != NULL the first obstacle along the path
	idVec3				startPosOutsideObstacles;	// start position outside obstacles
	idEntity *			startPosObstacle;			// if != NULL the obstacle containing the start position 
	idVec3				seekPosOutsideObstacles;	// seek position outside obstacles
	idEntity *			seekPosObstacle;			// if != NULL the obstacle containing the seek position 
} obstaclePath_t;

// path prediction
typedef enum {
	SE_BLOCKED			= BIT(0),
	SE_ENTER_LEDGE_AREA	= BIT(1),
	SE_ENTER_OBSTACLE	= BIT(2),
	SE_FALL				= BIT(3),
	SE_LAND				= BIT(4)
} stopEvent_t;

typedef struct predictedPath_s {
	idVec3				endPos;						// final position
	idVec3				endVelocity;				// velocity at end position
	idVec3				endNormal;					// normal of blocking surface
	int					endTime;					// time predicted
	int					endEvent;					// event that stopped the prediction
	const idEntity *	blockingEntity;				// entity that blocks the movement
} predictedPath_t;

//
// events
//
extern const idEventDef AI_BeginAttack;
extern const idEventDef AI_EndAttack;
extern const idEventDef AI_MuzzleFlash;
extern const idEventDef AI_CreateMissile;
extern const idEventDef AI_AttackMissile;
extern const idEventDef AI_FireMissileAtTarget;
extern const idEventDef AI_LaunchProjectile;
extern const idEventDef AI_TriggerFX;
extern const idEventDef AI_StartEmitter;
extern const idEventDef AI_StopEmitter;
extern const idEventDef AI_AttackMelee;
extern const idEventDef AI_DirectDamage;
extern const idEventDef AI_JumpFrame;
extern const idEventDef AI_EnableClip;
extern const idEventDef AI_DisableClip;
extern const idEventDef AI_EnableGravity;
extern const idEventDef AI_DisableGravity;
extern const idEventDef AI_TriggerParticles;
extern const idEventDef AI_RandomPath;

class idPathCorner;

typedef struct particleEmitter_s {
	particleEmitter_s() {
		particle = NULL;
		time = 0;
		joint = INVALID_JOINT;
	};
	const idDeclParticle *particle;
	int					time;
	jointHandle_t		joint;
} particleEmitter_t;

typedef struct funcEmitter_s {
	char				name[64];
	idFuncEmitter*		particle;
	jointHandle_t		joint;
} funcEmitter_t;

class idMoveState {
public:
							idMoveState();

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	moveType_t				moveType;
	moveCommand_t			moveCommand;
	moveStatus_t			moveStatus;
	idVec3					moveDest;
	idVec3					moveDir;			// used for wandering and slide moves
	idEntityPtr<idEntity>	goalEntity;
	idVec3					goalEntityOrigin;	// move to entity uses this to avoid checking the floor position every frame
	int						toAreaNum;
	int						startTime;
	int						duration;
	float					speed;				// only used by flying creatures
	float					range;
	float					wanderYaw;
	int						nextWanderTime;
	int						blockTime;
	idEntityPtr<idEntity>	obstacle;
	idVec3					lastMoveOrigin;
	int						lastMoveTime;
	int						anim;
};

class idAASFindCover : public idAASCallback {
public:
						idAASFindCover( const idVec3 &hideFromPos );
						~idAASFindCover();

	virtual bool		TestArea( const idAAS *aas, int areaNum );

private:
	pvsHandle_t			hidePVS;
	int					PVSAreas[ idEntity::MAX_PVS_AREAS ];
};

class idAASFindAreaOutOfRange : public idAASCallback {
public:
						idAASFindAreaOutOfRange( const idVec3 &targetPos, float maxDist );

	virtual bool		TestArea( const idAAS *aas, int areaNum );

private:
	idVec3				targetPos;
	float				maxDistSqr;
};

class idAASFindAttackPosition : public idAASCallback {
public:
						idAASFindAttackPosition( const idAI *self, const idMat3 &gravityAxis, idEntity *target, const idVec3 &targetPos, const idVec3 &fireOffset );
						~idAASFindAttackPosition();

	virtual bool		TestArea( const idAAS *aas, int areaNum );

private:
	const idAI			*self;
	idEntity			*target;
	idBounds			excludeBounds;
	idVec3				targetPos;
	idVec3				fireOffset;
	idMat3				gravityAxis;
	pvsHandle_t			targetPVS;
	int					PVSAreas[ idEntity::MAX_PVS_AREAS ];
};

D3_CLASS()
class idAI : public idActor {
public:
	CLASS_PROTOTYPE( idAI );

							idAI();
							~idAI();

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	void					Spawn( void );
	void					HeardSound( idEntity *ent, const char *action );
	idActor					*GetEnemy( void ) const;
	void					TalkTo( idActor *actor );
	talkState_t				GetTalkState( void ) const;

	bool					GetAimDir( const idVec3 &firePos, idEntity *aimAtEnt, const idEntity *ignore, idVec3 &aimDir ) const;

	void					TouchedByFlashlight( idActor *flashlight_owner );

							// Outputs a list of all monsters to the console.
	static void				List_f( const idCmdArgs &args );

							// Finds a path around dynamic obstacles.
	static bool				FindPathAroundObstacles( const idPhysics *physics, const idAAS *aas, const idEntity *ignore, const idVec3 &startPos, const idVec3 &seekPos, obstaclePath_t &path );
							// Frees any nodes used for the dynamic obstacle avoidance.
	static void				FreeObstacleAvoidanceNodes( void );
							// Predicts movement, returns true if a stop event was triggered.
	static bool				PredictPath( const idEntity *ent, const idAAS *aas, const idVec3 &start, const idVec3 &velocity, int totalTime, int frameTime, int stopEvent, predictedPath_t &path );
							// Return true if the trajectory of the clip model is collision free.
	static bool				TestTrajectory( const idVec3 &start, const idVec3 &end, float zVel, float gravity, float time, float max_height, const idClipModel *clip, int clipmask, const idEntity *ignore, const idEntity *targetEntity, int drawtime );
							// Finds the best collision free trajectory for a clip model.
	static bool				PredictTrajectory( const idVec3 &firePos, const idVec3 &target, float projectileSpeed, const idVec3 &projGravity, const idClipModel *clip, int clipmask, float max_height, const idEntity *ignore, const idEntity *targetEntity, int drawtime, idVec3 &aimDir );

	virtual void			Gib( const idVec3 &dir, const char *damageDefName );

protected:
	// navigation
	idAAS *					aas;
	int						travelFlags;

	idMoveState				move;
	idMoveState				savedMove;

	float					kickForce;
	bool					ignore_obstacles;
	float					blockedRadius;
	int						blockedMoveTime;
	int						blockedAttackTime;

	// turning
	float					ideal_yaw;
	float					current_yaw;
	float					turnRate;
	float					turnVel;
	float					anim_turn_yaw;
	float					anim_turn_amount;
	float					anim_turn_angles;

	// physics
	idPhysics_Monster		physicsObj;

	// flying
	jointHandle_t			flyTiltJoint;
	float					fly_speed;
	float					fly_bob_strength;
	float					fly_bob_vert;
	float					fly_bob_horz;
	int						fly_offset;					// prefered offset from player's view
	float					fly_seek_scale;
	float					fly_roll_scale;
	float					fly_roll_max;
	float					fly_roll;
	float					fly_pitch_scale;
	float					fly_pitch_max;
	float					fly_pitch;

	bool					allowMove;					// disables any animation movement
	bool					allowHiddenMovement;		// allows character to still move around while hidden
	bool					disableGravity;				// disables gravity and allows vertical movement by the animation
	bool					af_push_moveables;			// allow the articulated figure to push moveable objects
	
	// weapon/attack vars
	bool					lastHitCheckResult;
	int						lastHitCheckTime;
	int						lastAttackTime;
	float					melee_range;
	float					projectile_height_to_distance_ratio;	// calculates the maximum height a projectile can be thrown
	idList<idVec3>			missileLaunchOffset;

	const idDict *			projectileDef;
	mutable idClipModel		*projectileClipModel;
	float					projectileRadius;
	float					projectileSpeed;
	idVec3					projectileVelocity;
	idVec3					projectileGravity;
	idEntityPtr<idProjectile> projectile;
	idStr					attack;

	// chatter/talking
	const idSoundShader		*chat_snd;
	int						chat_min;
	int						chat_max;
	int						chat_time;
	talkState_t				talk_state;
	idEntityPtr<idActor>	talkTarget;

	// cinematics
	int						num_cinematics;
	int						current_cinematic;

	bool					allowJointMod;
	idEntityPtr<idEntity>	focusEntity;
	idVec3					currentFocusPos;
	int						focusTime;
	int						alignHeadTime;
	int						forceAlignHeadTime;
	idAngles				eyeAng;
	idAngles				lookAng;
	idAngles				destLookAng;
	idAngles				lookMin;
	idAngles				lookMax;
	idList<jointHandle_t>	lookJoints;
	idList<idAngles>		lookJointAngles;
	float					eyeVerticalOffset;
	float					eyeHorizontalOffset;
	float					eyeFocusRate;
	float					headFocusRate;
	int						focusAlignTime;

	// special fx
	float					shrivel_rate;
	int						shrivel_start;
	
	bool					restartParticles;			// should smoke emissions restart
	bool					useBoneAxis;				// use the bone vs the model axis
	idList<particleEmitter_t> particles;				// particle data

	idRenderLight *			worldMuzzleFlash;			// positioned on world weapon bone
	jointHandle_t			flashJointWorld;
	int						muzzleFlashEnd;
	int						flashTime;

	// joint controllers
	idAngles				eyeMin;
	idAngles				eyeMax;
	jointHandle_t			focusJoint;
	jointHandle_t			orientationJoint;

	// enemy variables
	idEntityPtr<idActor>	enemy;
	idVec3					lastVisibleEnemyPos;
	idVec3					lastVisibleEnemyEyeOffset;
	idVec3					lastVisibleReachableEnemyPos;
	idVec3					lastReachableEnemyPos;
	bool					wakeOnFlashlight;

	bool					spawnClearMoveables;

	idHashTable<funcEmitter_t> funcEmitters;

	idEntityPtr<idHarvestable>	harvestEnt;

	// script variables
	idScriptBool			AI_TALK;
	idScriptBool			AI_DAMAGE;
	idScriptBool			AI_PAIN;
	idScriptFloat			AI_SPECIAL_DAMAGE;
	idScriptBool			AI_DEAD;
	idScriptBool			AI_ENEMY_VISIBLE;
	idScriptBool			AI_ENEMY_IN_FOV;
	idScriptBool			AI_ENEMY_DEAD;
	idScriptBool			AI_MOVE_DONE;
	idScriptBool			AI_ONGROUND;
	idScriptBool			AI_ACTIVATED;
	idScriptBool			AI_FORWARD;
	idScriptBool			AI_JUMP;
	idScriptBool			AI_ENEMY_REACHABLE;
	idScriptBool			AI_BLOCKED;
	idScriptBool			AI_OBSTACLE_IN_PATH;
	idScriptBool			AI_DEST_UNREACHABLE;
	idScriptBool			AI_HIT_ENEMY;
	idScriptBool			AI_PUSHED;

	//
	// ai/ai.cpp
	//
	void					SetAAS( void );
	virtual	void			DormantBegin( void );	// called when entity becomes dormant
	virtual	void			DormantEnd( void );		// called when entity wakes from being dormant
	void					Think( void );
	void					Activate( idEntity *activator );
	int						ReactionTo( const idEntity *ent );
	bool					CheckForEnemy( void );
	void					EnemyDead( void );
	virtual bool			CanPlayChatterSounds( void ) const;
	void					SetChatSound( void );
	void					PlayChatter( void );
	virtual void			Hide( void );
	virtual void			Show( void );
	idVec3					FirstVisiblePointOnPath( const idVec3 origin, const idVec3 &target, int travelFlags ) const;
	void					CalculateAttackOffsets( void );
	void					PlayCinematic( void );

	// movement
	virtual void			ApplyImpulse( idEntity *ent, int id, const idVec3 &point, const idVec3 &impulse );
	void					GetMoveDelta( const idMat3 &oldaxis, const idMat3 &axis, idVec3 &delta );
	void					CheckObstacleAvoidance( const idVec3 &goalPos, idVec3 &newPos );
	void					DeadMove( void );
	void					AnimMove( void );
	void					SlideMove( void );
	void					AdjustFlyingAngles( void );
	void					AddFlyBob( idVec3 &vel );
	void					AdjustFlyHeight( idVec3 &vel, const idVec3 &goalPos );
	void					FlySeekGoal( idVec3 &vel, idVec3 &goalPos );
	void					AdjustFlySpeed( idVec3 &vel );
	void					FlyTurn( void );
	void					FlyMove( void );
	void					StaticMove( void );

	// damage
	virtual bool			Pain( idEntity *inflictor, idEntity *attacker, int damage, const idVec3 &dir, int location );
	virtual void			Killed( idEntity *inflictor, idEntity *attacker, int damage, const idVec3 &dir, int location );

	// navigation
	void					KickObstacles( const idVec3 &dir, float force, idEntity *alwaysKick );
	bool					ReachedPos( const idVec3 &pos, const moveCommand_t moveCommand ) const;
	float					TravelDistance( const idVec3 &start, const idVec3 &end ) const;
	int						PointReachableAreaNum( const idVec3 &pos, const float boundsScale = 2.0f ) const;
	bool					PathToGoal( aasPath_t &path, int areaNum, const idVec3 &origin, int goalAreaNum, const idVec3 &goalOrigin ) const;
	void					DrawRoute( void ) const;
	bool					GetMovePos( idVec3 &seekPos );
	bool					MoveDone( void ) const;
	bool					EntityCanSeePos( idActor *actor, const idVec3 &actorOrigin, const idVec3 &pos );
	void					BlockedFailSafe( void );

	// movement control
	void					StopMove( moveStatus_t status );
	bool					FaceEnemy( void );
	bool					FaceEntity( idEntity *ent );
	bool					DirectMoveToPosition( const idVec3 &pos );
	bool					MoveToEnemyHeight( void );
	bool					MoveOutOfRange( idEntity *entity, float range );
	bool					MoveToAttackPosition( idEntity *ent, int attack_anim );
	bool					MoveToEnemy( void );
	bool					MoveToEntity( idEntity *ent );
	bool					MoveToPosition( const idVec3 &pos );
	bool					MoveToCover( idEntity *entity, const idVec3 &pos );
	bool					SlideToPosition( const idVec3 &pos, float time );
	bool					WanderAround( void );
	bool					StepDirection( float dir );
	bool					NewWanderDir( const idVec3 &dest );

	// effects
	const idDeclParticle	*SpawnParticlesOnJoint( particleEmitter_t &pe, const char *particleName, const char *jointName );
	void					SpawnParticles( const char *keyName );
	bool					ParticlesActive( void );

	// turning
	bool					FacingIdeal( void );
	void					Turn( void );
	bool					TurnToward( float yaw );
	bool					TurnToward( const idVec3 &pos );

	// enemy management
	void					ClearEnemy( void );
	bool					EnemyPositionValid( void ) const;
	void					SetEnemyPosition( void );
	void					UpdateEnemyPosition( void );
	void					SetEnemy( idActor *newEnemy );

	// attacks
	void					CreateProjectileClipModel( void ) const;
	idProjectile			*CreateProjectile( const idVec3 &pos, const idVec3 &dir );
	void					RemoveProjectile( void );
	idProjectile			*LaunchProjectile( const char *jointname, idEntity *target, bool clampToAttackCone );
	virtual void			DamageFeedback( idEntity *victim, idEntity *inflictor, int &damage );
	void					DirectDamage( const char *meleeDefName, idEntity *ent );
	bool					TestMelee( void ) const;
	bool					AttackMelee( const char *meleeDefName );
	void					BeginAttack( const char *name );
	void					EndAttack( void );
	void					PushWithAF( void );

	// special effects
	void					GetMuzzle( const char *jointname, idVec3 &muzzle, idMat3 &axis );
	void					InitMuzzleFlash( void );
	void					TriggerWeaponEffects( const idVec3 &muzzle );
	void					UpdateMuzzleFlash( void );
	virtual bool			UpdateAnimationControllers( void );
	void					UpdateParticles( void );
	void					TriggerParticles( const char *jointName );

	void					TriggerFX( const char* joint, const char* fx );
	idEntity*				StartEmitter( const char* name, const char* joint, const char* particle );
	idEntity*				GetEmitter( const char* name );
	void					StopEmitter( const char* name );

	// AI script state management
	void					LinkScriptVariables( void );
	void					UpdateAIScript( void );

	//
	// ai/ai_events.cpp
	//
	D3_EVENT( EV_Activate )
	void					Event_Activate( idEntity *activator );
	D3_EVENT( EV_Touch )
	void					Event_Touch( idEntity *other, trace_t *trace );
	D3_EVENT( AI_FindEnemy, "findEnemy", entity )
	void					Event_FindEnemy( int useFOV );
	D3_EVENT( AI_FindEnemyAI, "findEnemyAI", entity )
	void					Event_FindEnemyAI( int useFOV );
	D3_EVENT( AI_FindEnemyInCombatNodes, "findEnemyInCombatNodes", entity )
	void					Event_FindEnemyInCombatNodes( void );
	D3_EVENT( AI_ClosestReachableEnemyOfEntity, "closestReachableEnemyOfEntity", entity )
	void					Event_ClosestReachableEnemyOfEntity( D3_NULLABLE idEntity *team_mate );
	D3_EVENT( AI_HeardSound, "heardSound", entity )
	void					Event_HeardSound( int ignore_team );
	D3_EVENT( AI_SetEnemy, "setEnemy", void )
	void					Event_SetEnemy( D3_NULLABLE idEntity *ent );
	D3_EVENT( AI_ClearEnemy, "clearEnemy", void )
	void					Event_ClearEnemy( void );
	D3_EVENT( AI_MuzzleFlash, "muzzleFlash", void )
	void					Event_MuzzleFlash( const char *jointname );
	D3_EVENT( AI_CreateMissile, "createMissile", entity )
	void					Event_CreateMissile( const char *jointname );
	D3_EVENT( AI_AttackMissile, "attackMissile", entity )
	void					Event_AttackMissile( const char *jointname );
	D3_EVENT( AI_FireMissileAtTarget, "fireMissileAtTarget", entity )
	void					Event_FireMissileAtTarget( const char *jointname, const char *targetname );
	D3_EVENT( AI_LaunchMissile, "launchMissile", entity )
	void					Event_LaunchMissile( const idVec3 &muzzle, const idAngles &ang );
	D3_EVENT( AI_LaunchProjectile, "launchProjectile", void )
	void					Event_LaunchProjectile( const char *entityDefName );
	D3_EVENT( AI_AttackMelee, "attackMelee", integer )
	void					Event_AttackMelee( const char *meleeDefName );
	D3_EVENT( AI_DirectDamage, "directDamage", void )
	void					Event_DirectDamage( idEntity *damageTarget, const char *damageDefName );
	D3_EVENT( AI_RadiusDamageFromJoint, "radiusDamageFromJoint", void )
	void					Event_RadiusDamageFromJoint( const char *jointname, const char *damageDefName );
	D3_EVENT( AI_BeginAttack, "attackBegin", void )
	void					Event_BeginAttack( const char *name );
	D3_EVENT( AI_EndAttack, "attackEnd", void )
	void					Event_EndAttack( void );
	D3_EVENT( AI_MeleeAttackToJoint, "meleeAttackToJoint", integer )
	void					Event_MeleeAttackToJoint( const char *jointname, const char *meleeDefName );
	D3_EVENT( AI_RandomPath, "randomPath", entity )
	void					Event_RandomPath( void );
	D3_EVENT( AI_CanBecomeSolid, "canBecomeSolid", float )
	void					Event_CanBecomeSolid( void );
	D3_EVENT( AI_BecomeSolid, "becomeSolid", void )
	void					Event_BecomeSolid( void );
	D3_EVENT( EV_BecomeNonSolid, "becomeNonSolid", void )
	void					Event_BecomeNonSolid( void );
	D3_EVENT( AI_BecomeRagdoll, "becomeRagdoll", integer )
	void					Event_BecomeRagdoll( void );
	D3_EVENT( AI_StopRagdoll, "stopRagdoll", void )
	void					Event_StopRagdoll( void );
	D3_EVENT( AI_SetHealth, "setHealth", void )
	void					Event_SetHealth( float newHealth );
	D3_EVENT( AI_GetHealth, "getHealth", float )
	void					Event_GetHealth( void );
	D3_EVENT( AI_AllowDamage, "allowDamage", void )
	void					Event_AllowDamage( void );
	D3_EVENT( AI_IgnoreDamage, "ignoreDamage", void )
	void					Event_IgnoreDamage( void );
	D3_EVENT( AI_GetCurrentYaw, "getCurrentYaw", float )
	void					Event_GetCurrentYaw( void );
	D3_EVENT( AI_TurnTo, "turnTo", void )
	void					Event_TurnTo( float angle );
	D3_EVENT( AI_TurnToPos, "turnToPos", void )
	void					Event_TurnToPos( const idVec3 &pos );
	D3_EVENT( AI_TurnToEntity, "turnToEntity", void )
	void					Event_TurnToEntity( D3_NULLABLE idEntity *ent );
	D3_EVENT( AI_MoveStatus, "moveStatus", integer )
	void					Event_MoveStatus( void );
	D3_EVENT( AI_StopMove, "stopMove", void )
	void					Event_StopMove( void );
	D3_EVENT( AI_MoveToCover, "moveToCover", void )
	void					Event_MoveToCover( void );
	D3_EVENT( AI_MoveToEnemy, "moveToEnemy", void )
	void					Event_MoveToEnemy( void );
	D3_EVENT( AI_MoveToEnemyHeight, "moveToEnemyHeight", void )
	void					Event_MoveToEnemyHeight( void );
	D3_EVENT( AI_MoveOutOfRange, "moveOutOfRange", void )
	void					Event_MoveOutOfRange( idEntity *entity, float range );
	D3_EVENT( AI_MoveToAttackPosition, "moveToAttackPosition", void )
	void					Event_MoveToAttackPosition( idEntity *entity, const char *attack_anim );
	D3_EVENT( AI_MoveToEntity, "moveToEntity", void )
	void					Event_MoveToEntity( idEntity *ent );
	D3_EVENT( AI_MoveToPosition, "moveToPosition", void )
	void					Event_MoveToPosition( const idVec3 &pos );
	D3_EVENT( AI_SlideTo, "slideTo", void )
	void					Event_SlideTo( const idVec3 &pos, float time );
	D3_EVENT( AI_Wander, "wander", void )
	void					Event_Wander( void );
	D3_EVENT( AI_FacingIdeal, "facingIdeal", integer )
	void					Event_FacingIdeal( void );
	D3_EVENT( AI_FaceEnemy, "faceEnemy", void )
	void					Event_FaceEnemy( void );
	D3_EVENT( AI_FaceEntity, "faceEntity", void )
	void					Event_FaceEntity( D3_NULLABLE idEntity *ent );
	D3_EVENT( AI_WaitAction, "waitAction", void )
	void					Event_WaitAction( const char *waitForState );
	D3_EVENT( AI_GetCombatNode, "getCombatNode", entity )
	void					Event_GetCombatNode( void );
	D3_EVENT( AI_EnemyInCombatCone, "enemyInCombatCone", integer )
	void					Event_EnemyInCombatCone( D3_NULLABLE idEntity *ent, int use_current_enemy_location );
	D3_EVENT( AI_WaitMove, "waitMove", void )
	void					Event_WaitMove( void );
	D3_EVENT( AI_GetJumpVelocity, "getJumpVelocity", vector )
	void					Event_GetJumpVelocity( const idVec3 &pos, float speed, float max_height );
	D3_EVENT( AI_EntityInAttackCone, "entityInAttackCone", integer )
	void					Event_EntityInAttackCone( D3_NULLABLE idEntity *ent );
	D3_EVENT( AI_CanSeeEntity, "canSee", integer )
	void					Event_CanSeeEntity( D3_NULLABLE idEntity *ent );
	D3_EVENT( AI_SetTalkTarget, "setTalkTarget", void )
	void					Event_SetTalkTarget( D3_NULLABLE idEntity *target );
	D3_EVENT( AI_GetTalkTarget, "getTalkTarget", entity )
	void					Event_GetTalkTarget( void );
	D3_EVENT( AI_SetTalkState, "setTalkState", void )
	void					Event_SetTalkState( int state );
	D3_EVENT( AI_EnemyRange, "enemyRange", float )
	void					Event_EnemyRange( void );
	D3_EVENT( AI_EnemyRange2D, "enemyRange2D", float )
	void					Event_EnemyRange2D( void );
	D3_EVENT( AI_GetEnemy, "getEnemy", entity )
	void					Event_GetEnemy( void );
	D3_EVENT( AI_GetEnemyPos, "getEnemyPos", vector )
	void					Event_GetEnemyPos( void );
	D3_EVENT( AI_GetEnemyEyePos, "getEnemyEyePos", vector )
	void					Event_GetEnemyEyePos( void );
	D3_EVENT( AI_PredictEnemyPos, "predictEnemyPos", vector )
	void					Event_PredictEnemyPos( float time );
	D3_EVENT( AI_CanHitEnemy, "canHitEnemy", integer )
	void					Event_CanHitEnemy( void );
	D3_EVENT( AI_CanHitEnemyFromAnim, "canHitEnemyFromAnim", integer )
	void					Event_CanHitEnemyFromAnim( const char *animname );
	D3_EVENT( AI_CanHitEnemyFromJoint, "canHitEnemyFromJoint", integer )
	void					Event_CanHitEnemyFromJoint( const char *jointname );
	D3_EVENT( AI_EnemyPositionValid, "enemyPositionValid", integer )
	void					Event_EnemyPositionValid( void );
	D3_EVENT( AI_ChargeAttack, "chargeAttack", void )
	void					Event_ChargeAttack( const char *damageDef );
	D3_EVENT( AI_TestChargeAttack, "testChargeAttack", float )
	void					Event_TestChargeAttack( void );
	D3_EVENT( AI_TestAnimMoveTowardEnemy, "testAnimMoveTowardEnemy", integer )
	void					Event_TestAnimMoveTowardEnemy( const char *animname );
	D3_EVENT( AI_TestAnimMove, "testAnimMove", integer )
	void					Event_TestAnimMove( const char *animname );
	D3_EVENT( AI_TestMoveToPosition, "testMoveToPosition", integer )
	void					Event_TestMoveToPosition( const idVec3 &position );
	D3_EVENT( AI_TestMeleeAttack, "testMeleeAttack", integer )
	void					Event_TestMeleeAttack( void );
	D3_EVENT( AI_TestAnimAttack, "testAnimAttack", integer )
	void					Event_TestAnimAttack( const char *animname );
	D3_EVENT( AI_Shrivel, "shrivel", void )
	void					Event_Shrivel( float shirvel_time );
	D3_EVENT( AI_Burn, "burn", void )
	void					Event_Burn( void );
	D3_EVENT( AI_PreBurn, "preBurn", void )
	void					Event_PreBurn( void );
	D3_EVENT( AI_ClearBurn, "clearBurn", void )
	void					Event_ClearBurn( void );
	D3_EVENT( AI_SetSmokeVisibility, "setSmokeVisibility", void )
	void					Event_SetSmokeVisibility( int num, int on );
	D3_EVENT( AI_NumSmokeEmitters, "numSmokeEmitters", integer )
	void					Event_NumSmokeEmitters( void );
	D3_EVENT( AI_StopThinking, "stopThinking", void )
	void					Event_StopThinking( void );
	D3_EVENT( AI_GetTurnDelta, "getTurnDelta", float )
	void					Event_GetTurnDelta( void );
	D3_EVENT( AI_GetMoveType, "getMoveType", integer )
	void					Event_GetMoveType( void );
	D3_EVENT( AI_SetMoveType, "setMoveType", void )
	void					Event_SetMoveType( int moveType );
	D3_EVENT( AI_SaveMove, "saveMove", void )
	void					Event_SaveMove( void );
	D3_EVENT( AI_RestoreMove, "restoreMove", void )
	void					Event_RestoreMove( void );
	D3_EVENT( AI_AllowMovement, "allowMovement", void )
	void					Event_AllowMovement( float flag );
	D3_EVENT( AI_JumpFrame, "<jumpframe>", void )
	void					Event_JumpFrame( void );
	D3_EVENT( AI_EnableClip, "enableClip", void )
	void					Event_EnableClip( void );
	D3_EVENT( AI_DisableClip, "disableClip", void )
	void					Event_DisableClip( void );
	D3_EVENT( AI_EnableGravity, "enableGravity", void )
	void					Event_EnableGravity( void );
	D3_EVENT( AI_DisableGravity, "disableGravity", void )
	void					Event_DisableGravity( void );
	D3_EVENT( AI_EnableAFPush, "enableAFPush", void )
	void					Event_EnableAFPush( void );
	D3_EVENT( AI_DisableAFPush, "disableAFPush", void )
	void					Event_DisableAFPush( void );
	D3_EVENT( AI_SetFlySpeed, "setFlySpeed", void )
	void					Event_SetFlySpeed( float speed );
	D3_EVENT( AI_SetFlyOffset, "setFlyOffset", void )
	void					Event_SetFlyOffset( int offset );
	D3_EVENT( AI_ClearFlyOffset, "clearFlyOffset", void )
	void					Event_ClearFlyOffset( void );
	D3_EVENT( AI_GetClosestHiddenTarget, "getClosestHiddenTarget", entity )
	void					Event_GetClosestHiddenTarget( const char *type );
	D3_EVENT( AI_GetRandomTarget, "getRandomTarget", entity )
	void					Event_GetRandomTarget( const char *type );
	D3_EVENT( AI_TravelDistanceToPoint, "travelDistanceToPoint", float )
	void					Event_TravelDistanceToPoint( const idVec3 &pos );
	D3_EVENT( AI_TravelDistanceToEntity, "travelDistanceToEntity", float )
	void					Event_TravelDistanceToEntity( idEntity *ent );
	D3_EVENT( AI_TravelDistanceBetweenPoints, "travelDistanceBetweenPoints", float )
	void					Event_TravelDistanceBetweenPoints( const idVec3 &source, const idVec3 &dest );
	D3_EVENT( AI_TravelDistanceBetweenEntities, "travelDistanceBetweenEntities", float )
	void					Event_TravelDistanceBetweenEntities( idEntity *source, idEntity *dest );
	D3_EVENT( AI_LookAtEntity, "lookAt", void )
	void					Event_LookAtEntity( D3_NULLABLE idEntity *ent, float duration );
	D3_EVENT( AI_LookAtEnemy, "lookAtEnemy", void )
	void					Event_LookAtEnemy( float duration );
	D3_EVENT( AI_SetJointMod, "setBoneMod", void )
	void					Event_SetJointMod( int allowJointMod );
	D3_EVENT( AI_ThrowMoveable, "throwMoveable", void )
	void					Event_ThrowMoveable( void );
	D3_EVENT( AI_ThrowAF, "throwAF", void )
	void					Event_ThrowAF( void );
	D3_EVENT( EV_SetAngles, "setAngles", void )
	void					Event_SetAngles( idAngles const &ang );
	D3_EVENT( EV_GetAngles, "getAngles", vector )
	void					Event_GetAngles( void );
	D3_EVENT( AI_RealKill, "<kill>", void )
	void					Event_RealKill( void );
	D3_EVENT( AI_Kill, "kill", void )
	void					Event_Kill( void );
	D3_EVENT( AI_WakeOnFlashlight, "wakeOnFlashlight", void )
	void					Event_WakeOnFlashlight( int enable );
	D3_EVENT( AI_LocateEnemy, "locateEnemy", void )
	void					Event_LocateEnemy( void );
	D3_EVENT( AI_KickObstacles, "kickObstacles", void )
	void					Event_KickObstacles( D3_NULLABLE idEntity *kickEnt, float force );
	D3_EVENT( AI_GetObstacle, "getObstacle", entity )
	void					Event_GetObstacle( void );
	D3_EVENT( AI_PushPointIntoAAS, "pushPointIntoAAS", vector )
	void					Event_PushPointIntoAAS( const idVec3 &pos );
	D3_EVENT( AI_GetTurnRate, "getTurnRate", float )
	void					Event_GetTurnRate( void );
	D3_EVENT( AI_SetTurnRate, "setTurnRate", void )
	void					Event_SetTurnRate( float rate );
	D3_EVENT( AI_AnimTurn, "animTurn", void )
	void					Event_AnimTurn( float angles );
	D3_EVENT( AI_AllowHiddenMovement, "allowHiddenMovement", void )
	void					Event_AllowHiddenMovement( int enable );
	D3_EVENT( AI_TriggerParticles, "triggerParticles", void )
	void					Event_TriggerParticles( const char *jointName );
	D3_EVENT( AI_FindActorsInBounds, "findActorsInBounds", entity )
	void					Event_FindActorsInBounds( const idVec3 &mins, const idVec3 &maxs );
	D3_EVENT( AI_CanReachPosition, "canReachPosition", integer )
	void 					Event_CanReachPosition( const idVec3 &pos );
	D3_EVENT( AI_CanReachEntity, "canReachEntity", integer )
	void 					Event_CanReachEntity( D3_NULLABLE idEntity *ent );
	D3_EVENT( AI_CanReachEnemy, "canReachEnemy", integer )
	void					Event_CanReachEnemy( void );
	D3_EVENT( AI_GetReachableEntityPosition, "getReachableEntityPosition", vector )
	void					Event_GetReachableEntityPosition( idEntity *ent );
	D3_EVENT( AI_MoveToPositionDirect, "moveToPositionDirect", void )
	void					Event_MoveToPositionDirect( const idVec3 &pos );
	D3_EVENT( AI_AvoidObstacles, "avoidObstacles", void )
	void					Event_AvoidObstacles( int ignore);
	D3_EVENT( AI_TriggerFX, "triggerFX", void )
	void					Event_TriggerFX( const char* joint, const char* fx );

	D3_EVENT( AI_StartEmitter, "startEmitter", entity )
	void					Event_StartEmitter( const char* name, const char* joint, const char* particle );
	D3_EVENT( AI_GetEmitter, "getEmitter", entity )
	void					Event_GetEmitter( const char* name );
	D3_EVENT( AI_StopEmitter, "stopEmitter", void )
	void					Event_StopEmitter( const char* name );
};

D3_CLASS()
class idCombatNode : public idEntity {
public:
	CLASS_PROTOTYPE( idCombatNode );

						idCombatNode();

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

	void				Spawn( void );
	bool				IsDisabled( void ) const;
	bool				EntityInView( idActor *actor, const idVec3 &pos );
	static void			DrawDebugInfo( void );

private:
	float				min_dist;
	float				max_dist;
	float				cone_dist;
	float				min_height;
	float				max_height;
	idVec3				cone_left;
	idVec3				cone_right;
	idVec3				offset;
	bool				disabled;

	D3_EVENT( EV_Activate )
	void				Event_Activate( idEntity *activator );
	D3_EVENT( EV_CombatNode_MarkUsed, "markUsed", void )
	void				Event_MarkUsed( void );
};

#endif /* !__AI_H__ */
