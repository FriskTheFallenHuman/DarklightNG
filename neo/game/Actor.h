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

#ifndef __GAME_ACTOR_H__
#define __GAME_ACTOR_H__

/*
===============================================================================

	idActor

===============================================================================
*/

extern const idEventDef AI_EnableEyeFocus;
extern const idEventDef AI_DisableEyeFocus;
extern const idEventDef EV_Footstep;
extern const idEventDef EV_FootstepLeft;
extern const idEventDef EV_FootstepRight;
extern const idEventDef EV_EnableWalkIK;
extern const idEventDef EV_DisableWalkIK;
extern const idEventDef EV_EnableLegIK;
extern const idEventDef EV_DisableLegIK;
extern const idEventDef AI_SetAnimPrefix;
extern const idEventDef AI_PlayAnim;
extern const idEventDef AI_PlayCycle;
extern const idEventDef AI_AnimDone;
extern const idEventDef AI_SetBlendFrames;
extern const idEventDef AI_GetBlendFrames;

extern const idEventDef AI_SetState;

class idDeclParticle;

class idAnimState {
public:
	bool					idleAnim;
	idStr					state;
	int						animBlendFrames;
	int						lastAnimBlendFrames;		// allows override anims to blend based on the last transition time

public:
							idAnimState();
							~idAnimState();

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	void					Init( idActor *owner, idAnimator *_animator, int animchannel );
	void					Shutdown( void );
	void					SetState( const char *name, int blendFrames );
	void					StopAnim( int frames );
	void					PlayAnim( int anim );
	void					CycleAnim( int anim );
	void					BecomeIdle( void );
	bool					UpdateState( void );
	bool					Disabled( void ) const;
	void					Enable( int blendFrames );
	void					Disable( void );
	bool					AnimDone( int blendFrames ) const;
	bool					IsIdle( void ) const;
	animFlags_t				GetAnimFlags( void ) const;

private:
	idActor *				self;
	idAnimator *			animator;
	idThread *				thread;
	int						channel;
	bool					disabled;
};

class idAttachInfo {
public:
	idEntityPtr<idEntity>	ent;
	int						channel;
};

typedef struct {
	jointModTransform_t		mod;
	jointHandle_t			from;
	jointHandle_t			to;
} copyJoints_t;

D3_CLASS()
class idActor : public idAFEntity_Gibbable {
public:
	CLASS_PROTOTYPE( idActor );

	int						team;
	int						rank;				// monsters don't fight back if the attacker's rank is higher
	idMat3					viewAxis;			// view axis of the actor

	idLinkList<idActor>		enemyNode;			// node linked into an entity's enemy list for quick lookups of who is attacking him
	idLinkList<idActor>		enemyList;			// list of characters that have targeted the player as their enemy

public:
							idActor( void );
	virtual					~idActor( void );

	void					Spawn( void );
	virtual void			Restart( void );

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	virtual void			Hide( void );
	virtual void			Show( void );
	virtual int				GetDefaultSurfaceType( void ) const;
	virtual void			ProjectOverlay( const idVec3 &origin, const idVec3 &dir, float size, const char *material );

	virtual bool			LoadAF( void );
	void					SetupBody( void );

	void					CheckBlink( void );

	virtual bool			GetPhysicsToVisualTransform( idVec3 &origin, idMat3 &axis );
	virtual bool			GetPhysicsToSoundTransform( idVec3 &origin, idMat3 &axis );

							// script state management
	void					ShutdownThreads( void );
	virtual bool			ShouldConstructScriptObjectAtSpawn( void ) const;
	virtual idThread *		ConstructScriptObject( void );
	void					UpdateScript( void );
	const function_t		*GetScriptFunction( const char *funcname );
	void					SetState( const function_t *newState );
	void					SetState( const char *statename );

							// vision testing
	void					SetEyeHeight( float height );
	float					EyeHeight( void ) const;
	idVec3					EyeOffset( void ) const;
	idVec3					GetEyePosition( void ) const;
	virtual void			GetViewPos( idVec3 &origin, idMat3 &axis ) const;
	void					SetFOV( float fov );
	bool					CheckFOV( const idVec3 &pos ) const;
	bool					CanSee( idEntity *ent, bool useFOV ) const;
	bool					PointVisible( const idVec3 &point ) const;
	virtual void			GetAIAimTargets( const idVec3 &lastSightPos, idVec3 &headPos, idVec3 &chestPos );

							// damage
	void					SetupDamageGroups( void );
	virtual	void			Damage( idEntity *inflictor, idEntity *attacker, const idVec3 &dir, const char *damageDefName, const float damageScale, const int location );
	int						GetDamageForLocation( int damage, int location );
	const char *			GetDamageGroup( int location );
	void					ClearPain( void );
	virtual bool			Pain( idEntity *inflictor, idEntity *attacker, int damage, const idVec3 &dir, int location );

							// model/combat model/ragdoll
	void					SetCombatModel( void );
	idClipModel *			GetCombatModel( void ) const;
	virtual void			LinkCombat( void );
	virtual void			UnlinkCombat( void );
	bool					StartRagdoll( void );
	void					StopRagdoll( void );
	virtual bool			UpdateAnimationControllers( void );

							// delta view angles to allow movers to rotate the view of the actor
	const idAngles &		GetDeltaViewAngles( void ) const;
	void					SetDeltaViewAngles( const idAngles &delta );

	bool					HasEnemies( void ) const;
	idActor *				ClosestEnemyToPoint( const idVec3 &pos );
	idActor *				EnemyWithMostHealth();

	virtual bool			OnLadder( void ) const;

	virtual void			GetAASLocation( idAAS *aas, idVec3 &pos, int &areaNum ) const;

	void					Attach( idEntity *ent );

	virtual void			Teleport( const idVec3 &origin, const idAngles &angles, idEntity *destination );

	virtual	renderView_t *	GetRenderView();	
	
							// animation state control
	int						GetAnim( int channel, const char *name );
	void					UpdateAnimState( void );
	void					SetAnimState( int channel, const char *name, int blendFrames );
	const char *			GetAnimState( int channel ) const;
	bool					InAnimState( int channel, const char *name ) const;
	const char *			WaitState( void ) const;
	void					SetWaitState( const char *_waitstate );
	bool					AnimDone( int channel, int blendFrames ) const;
	virtual void			SpawnGibs( const idVec3 &dir, const char *damageDefName );

	idEntity*				GetHeadEntity() { return head.GetEntity(); };

protected:
	friend class			idAnimState;

	float					fovDot;				// cos( fovDegrees )
	idVec3					eyeOffset;			// offset of eye relative to physics origin
	idVec3					modelOffset;		// offset of visual model relative to the physics origin

	idAngles				deltaViewAngles;	// delta angles relative to view input angles

	int						pain_debounce_time;	// next time the actor can show pain
	int						pain_delay;			// time between playing pain sound
	int						pain_threshold;		// how much damage monster can take at any one time before playing pain animation

	idStrList				damageGroups;		// body damage groups
	idList<float>			damageScale;		// damage scale per damage gruop

	bool						use_combat_bbox;	// whether to use the bounding box for combat collision
	idEntityPtr<idAFAttachment>	head;
	idList<copyJoints_t>		copyJoints;			// copied from the body animation to the head model

	// state variables
	const function_t		*state;
	const function_t		*idealState;

	// joint handles
	jointHandle_t			leftEyeJoint;
	jointHandle_t			rightEyeJoint;
	jointHandle_t			soundJoint;

	idIK_Walk				walkIK;

	idStr					animPrefix;
	idStr					painAnim;

	// blinking
	int						blink_anim;
	int						blink_time;
	int						blink_min;
	int						blink_max;

	// script variables
	idThread *				scriptThread;
	idStr					waitState;
	idAnimState				headAnim;
	idAnimState				torsoAnim;
	idAnimState				legsAnim;

	bool					allowPain;
	bool					allowEyeFocus;
	bool					finalBoss;

	int						painTime;

	idList<idAttachInfo>	attachments;

	int						damageCap;

	virtual void			Gib( const idVec3 &dir, const char *damageDefName );

							// removes attachments with "remove" set for when character dies
	void					RemoveAttachments( void );

							// copies animation from body to head joints
	void					CopyJointsFromBodyToHead( void );

private:
	void					SyncAnimChannels( int channel, int syncToChannel, int blendFrames );
	void					FinishSetup( void );
	void					SetupHead( void );
	void					PlayFootStepSound( void );

	D3_EVENT( AI_EnableEyeFocus, "enableEyeFocus", void )
	void					Event_EnableEyeFocus( void );
	D3_EVENT( AI_DisableEyeFocus, "disableEyeFocus", void )
	void					Event_DisableEyeFocus( void );
	D3_EVENT( EV_Footstep, "footstep", void )
	D3_EVENT( EV_FootstepLeft, "leftFoot", void )
	D3_EVENT( EV_FootstepRight, "rightFoot", void )
	void					Event_Footstep( void );
	D3_EVENT( EV_EnableWalkIK, "EnableWalkIK", void )
	void					Event_EnableWalkIK( void );
	D3_EVENT( EV_DisableWalkIK, "DisableWalkIK", void )
	void					Event_DisableWalkIK( void );
	D3_EVENT( EV_EnableLegIK, "EnableLegIK", void )
	void					Event_EnableLegIK( int num );
	D3_EVENT( EV_DisableLegIK, "DisableLegIK", void )
	void					Event_DisableLegIK( int num );
	D3_EVENT( AI_SetAnimPrefix, "setAnimPrefix", void )
	void					Event_SetAnimPrefix( const char *name );
	void					Event_LookAtEntity( idEntity *ent, float duration );
	D3_EVENT( AI_PreventPain, "preventPain", void )
	void					Event_PreventPain( float duration );
	D3_EVENT( AI_DisablePain, "disablePain", void )
	void					Event_DisablePain( void );
	D3_EVENT( AI_EnablePain, "enablePain", void )
	void					Event_EnablePain( void );
	D3_EVENT( AI_GetPainAnim, "getPainAnim", string )
	void					Event_GetPainAnim( void );
	D3_EVENT( AI_StopAnim, "stopAnim", void )
	void					Event_StopAnim( animChannel_t channel, int frames );
	D3_EVENT( AI_PlayAnim, "playAnim", integer )
	void					Event_PlayAnim( animChannel_t channel, const char *name );
	D3_EVENT( AI_PlayCycle, "playCycle", integer )
	void					Event_PlayCycle( animChannel_t channel, const char *name );
	D3_EVENT( AI_IdleAnim, "idleAnim", integer )
	void					Event_IdleAnim( animChannel_t channel, const char *name );
	D3_EVENT( AI_SetSyncedAnimWeight, "setSyncedAnimWeight", void )
	void					Event_SetSyncedAnimWeight( animChannel_t channel, int anim, float weight );
	D3_EVENT( AI_OverrideAnim, "overrideAnim", void )
	void					Event_OverrideAnim( animChannel_t channel );
	D3_EVENT( AI_EnableAnim, "enableAnim", void )
	void					Event_EnableAnim( animChannel_t channel, int blendFrames );
	D3_EVENT( AI_SetBlendFrames, "setBlendFrames", void )
	void					Event_SetBlendFrames( animChannel_t channel, int blendFrames );
	D3_EVENT( AI_GetBlendFrames, "getBlendFrames", integer )
	void					Event_GetBlendFrames( animChannel_t channel );
	D3_EVENT( AI_AnimState, "animState", void )
	void					Event_AnimState( animChannel_t channel, const char *name, int blendFrames );
	D3_EVENT( AI_GetAnimState, "getAnimState", string )
	void					Event_GetAnimState( animChannel_t channel );
	D3_EVENT( AI_InAnimState, "inAnimState", integer )
	void					Event_InAnimState( animChannel_t channel, const char *name );
	D3_EVENT( AI_FinishAction, "finishAction", void )
	void					Event_FinishAction( const char *name );
	D3_EVENT( AI_AnimDone, "animDone", integer )
	void					Event_AnimDone( animChannel_t channel, int blendFrames );
	D3_EVENT( AI_HasAnim, "hasAnim", float )
	void					Event_HasAnim( animChannel_t channel, const char *name );
	D3_EVENT( AI_CheckAnim, "checkAnim", void )
	void					Event_CheckAnim( animChannel_t channel, const char *animname );
	D3_EVENT( AI_ChooseAnim, "chooseAnim", string )
	void					Event_ChooseAnim( animChannel_t channel, const char *animname );
	D3_EVENT( AI_AnimLength, "animLength", float )
	void					Event_AnimLength( animChannel_t channel, const char *animname );
	D3_EVENT( AI_AnimDistance, "animDistance", float )
	void					Event_AnimDistance( animChannel_t channel, const char *animname );
	D3_EVENT( AI_HasEnemies, "hasEnemies", integer )
	void					Event_HasEnemies( void );
	D3_EVENT( AI_NextEnemy, "nextEnemy", entity )
	void					Event_NextEnemy( D3_NULLABLE idEntity *ent );
	D3_EVENT( AI_ClosestEnemyToPoint, "closestEnemyToPoint", entity )
	void					Event_ClosestEnemyToPoint( const idVec3 &pos );
	D3_EVENT( EV_StopSound, "stopSound", void )
	void					Event_StopSound( gameSoundChannel_t channel, int netsync );
	D3_EVENT( AI_SetNextState, "setNextState", void )
	void					Event_SetNextState( const char *name );
	D3_EVENT( AI_SetState, "setState", void )
	void					Event_SetState( const char *name );
	D3_EVENT( AI_GetState, "getState", string )
	void					Event_GetState( void );
	D3_EVENT( AI_GetHead, "getHead", entity )
	void					Event_GetHead( void );
	D3_EVENT( EV_SetDamageGroupScale, "setDamageGroupScale", void )
	void					Event_SetDamageGroupScale( const char* groupName, float scale);
	D3_EVENT( EV_SetDamageGroupScaleAll, "setDamageGroupScaleAll", void )
	void					Event_SetDamageGroupScaleAll( float scale );
	D3_EVENT( EV_GetDamageGroupScale, "getDamageGroupScale", float )
	void					Event_GetDamageGroupScale( const char* groupName );
	D3_EVENT( EV_SetDamageCap, "setDamageCap", void )
	void					Event_SetDamageCap( float _damageCap );
	D3_EVENT( EV_SetWaitState, "setWaitState", void )
	void					Event_SetWaitState( const char* waitState);
	D3_EVENT( EV_GetWaitState, "getWaitState", string )
	void					Event_GetWaitState();
	
};

#endif /* !__GAME_ACTOR_H__ */
