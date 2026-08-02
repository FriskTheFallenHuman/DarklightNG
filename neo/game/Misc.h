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

#ifndef __GAME_MISC_H__
#define __GAME_MISC_H__


/*
===============================================================================

idSpawnableEntity

A simple, spawnable entity with a model and no functionable ability of it's own.
For example, it can be used as a placeholder during development, for marking
locations on maps for script, or for simple placed models without any behavior
that can be bound to other entities.  Should not be subclassed.
===============================================================================
*/

D3_CLASS()
class idSpawnableEntity : public idEntity {
public:
	CLASS_PROTOTYPE( idSpawnableEntity );

	void				Spawn( void );

private:
};

/*
===============================================================================

  Potential spawning position for players.
  The first time a player enters the game, they will be at an 'initial' spot.
  Targets will be fired when someone spawns in on them.

  When triggered, will cause player to be teleported to spawn spot.

===============================================================================
*/

D3_CLASS()
class idPlayerStart : public idEntity {
public:
	CLASS_PROTOTYPE( idPlayerStart );

	enum {
		EVENT_TELEPORTPLAYER = idEntity::EVENT_MAXEVENTS,
		EVENT_MAXEVENTS
	};

						idPlayerStart( void );

	void				Spawn( void );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

	virtual bool		ClientReceiveEvent( int event, int time, const idBitMsg &msg );

private:
	int					teleportStage;

	D3_EVENT( EV_Activate )
	void				Event_TeleportPlayer( idEntity *activator );
	D3_EVENT( EV_TeleportStage, "<TeleportStage>", void )
	void				Event_TeleportStage( idEntity *player );
	void				TeleportPlayer( idPlayer *player );
};


/*
===============================================================================

  Non-displayed entity used to activate triggers when it touches them.
  Bind to a mover to have the mover activate a trigger as it moves.
  When target by triggers, activating the trigger will toggle the
  activator on and off. Check "start_off" to have it spawn disabled.
	
===============================================================================
*/

D3_CLASS()
class idActivator : public idEntity {
public:
	CLASS_PROTOTYPE( idActivator );

	void				Spawn( void );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

	virtual void		Think( void );

private:
	bool				stay_on;

	D3_EVENT( EV_Activate )
	void				Event_Activate( idEntity *activator );
};


/*
===============================================================================

  Path entities for monsters to follow.

===============================================================================
*/
D3_CLASS()
class idPathCorner : public idEntity {
public:
	CLASS_PROTOTYPE( idPathCorner );

	void				Spawn( void );

	static void			DrawDebugInfo( void );

	static idPathCorner *RandomPath( const idEntity *source, const idEntity *ignore );

private:
	D3_EVENT( AI_RandomPath )
	void				Event_RandomPath( void );
};


/*
===============================================================================

  Object that fires targets and changes shader parms when damaged.

===============================================================================
*/

D3_CLASS()
class idDamagable : public idEntity {
public:
	CLASS_PROTOTYPE( idDamagable );

						idDamagable( void );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

	void				Spawn( void );
	void				Killed( idEntity *inflictor, idEntity *attacker, int damage, const idVec3 &dir, int location );

	virtual void		Hide( void );
	virtual void		Show( void );

private:
	int					count;
	int					nextTriggerTime;

	void				BecomeBroken( idEntity *activator );
	D3_EVENT( EV_Activate )
	void				Event_BecomeBroken( idEntity *activator );
	D3_EVENT( EV_RestoreDamagable, "<RestoreDamagable>", void )
	void				Event_RestoreDamagable( void );
};


/*
===============================================================================

  Hidden object that explodes when activated

===============================================================================
*/

D3_CLASS()
class idExplodable : public idEntity {
public:
	CLASS_PROTOTYPE( idExplodable );

	void				Spawn( void );

private:
	D3_EVENT( EV_Activate )
	void				Event_Explode( idEntity *activator );
};


/*
===============================================================================

  idSpring

===============================================================================
*/

D3_CLASS()
class idSpring : public idEntity {
public:
	CLASS_PROTOTYPE( idSpring );

	void				Spawn( void );

	virtual void		Think( void );

private:
	idEntity *			ent1;
	idEntity *			ent2;
	int					id1;
	int					id2;
	idVec3				p1;
	idVec3				p2;
	idForce_Spring		spring;

	D3_EVENT( EV_PostSpawn )
	void				Event_LinkSpring( void );
};


/*
===============================================================================

  idForceField

===============================================================================
*/

D3_CLASS()
class idForceField : public idEntity {
public:
	CLASS_PROTOTYPE( idForceField );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

	void				Spawn( void );

	virtual void		Think( void );

private:
	idForce_Field		forceField;

	void				Toggle( void );

	D3_EVENT( EV_Activate )
	void				Event_Activate( idEntity *activator );
	D3_EVENT( EV_Toggle, "Toggle", void )
	void				Event_Toggle( void );
	D3_EVENT( EV_FindTargets )
	void				Event_FindTargets( void );
};


/*
===============================================================================

  idAnimated

===============================================================================
*/

D3_CLASS()
class idAnimated : public idAFEntity_Gibbable {
public:
	CLASS_PROTOTYPE( idAnimated );

							idAnimated();
							~idAnimated();

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	void					Spawn( void );
	virtual bool			LoadAF( void );
	bool					StartRagdoll( void );
	virtual bool			GetPhysicsToSoundTransform( idVec3 &origin, idMat3 &axis );

private:
	int						num_anims;
	int						current_anim_index;
	int						anim;
	int						blendFrames;
	jointHandle_t			soundJoint;
	idEntityPtr<idEntity>	activator;
	bool					activated;

	void					PlayNextAnim( void );

	D3_EVENT( EV_Activate )
	void					Event_Activate( idEntity *activator );	
	D3_EVENT( EV_Animated_Start, "<start>", void )
	void					Event_Start( void );
	D3_EVENT( EV_StartRagdoll, "startRagdoll", void )
	void					Event_StartRagdoll( void );
	D3_EVENT( EV_AnimDone, "<AnimDone>", void )
	void					Event_AnimDone( int animIndex );
	D3_EVENT( EV_Footstep )
	D3_EVENT( EV_FootstepLeft )
	D3_EVENT( EV_FootstepRight )
	void					Event_Footstep( void );
	D3_EVENT( EV_LaunchMissiles, "launchMissiles", void )
	void					Event_LaunchMissiles( const char *projectilename, const char *sound, const char *launchjoint, const char *targetjoint, int numshots, int framedelay );
	D3_EVENT( EV_LaunchMissilesUpdate, "<launchMissiles>", void )
	void					Event_LaunchMissilesUpdate( int launchjoint, int targetjoint, int numshots, int framedelay );
	D3_EVENT( EV_SetAnimation, "setAnimation", void )
	void					Event_SetAnimation( const char *animName );
	D3_EVENT( EV_GetAnimationLength, "getAnimationLength", float )
	void					Event_GetAnimationLength();
};


/*
===============================================================================

  idStaticEntity

===============================================================================
*/

D3_CLASS()
class idStaticEntity : public idEntity {
public:
	CLASS_PROTOTYPE( idStaticEntity );

						idStaticEntity( void );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

	void				Spawn( void );
	void				ShowEditingDialog( void );
	virtual void		Hide( void );
	virtual void		Show( void );
	void				Fade( const idVec4 &to, float fadeTime );
	virtual void		Think( void );

	virtual void		WriteToSnapshot( idBitMsgDelta &msg ) const;
	virtual void		ReadFromSnapshot( const idBitMsgDelta &msg );

private:
	D3_EVENT( EV_Activate )
	void				Event_Activate( idEntity *activator );

	int					spawnTime;
	bool				active;
	idVec4				fadeFrom;
	idVec4				fadeTo;
	int					fadeStart;
	int					fadeEnd;
	bool				runGui;
};


/*
===============================================================================

idFuncEmitter

===============================================================================
*/

D3_CLASS()
class idFuncEmitter : public idStaticEntity {
public:
	CLASS_PROTOTYPE( idFuncEmitter );

						idFuncEmitter( void );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

	void				Spawn( void );
	D3_EVENT( EV_Activate )
	void				Event_Activate( idEntity *activator );

	virtual void		WriteToSnapshot( idBitMsgDelta &msg ) const;
	virtual void		ReadFromSnapshot( const idBitMsgDelta &msg );

private:
	bool				hidden;

};


/*
===============================================================================

idFuncSmoke

===============================================================================
*/

D3_CLASS()
class idFuncSmoke : public idEntity {
public:
	CLASS_PROTOTYPE( idFuncSmoke );

							idFuncSmoke();

	void					Spawn( void );

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	virtual void			Think( void );
	D3_EVENT( EV_Activate )
	void					Event_Activate( idEntity *activator );

private:
	int						smokeTime;
	const idDeclParticle *	smoke;
	bool					restart;
};


/*
===============================================================================

idFuncSplat

===============================================================================
*/

D3_CLASS()
class idFuncSplat : public idFuncEmitter {
public:
	CLASS_PROTOTYPE( idFuncSplat );

	idFuncSplat( void );

	void				Spawn( void );

private:
	D3_EVENT( EV_Activate )
	void				Event_Activate( idEntity *activator );
	D3_EVENT( EV_Splat, "<Splat>", void )
	void				Event_Splat();
};


/*
===============================================================================

idTextEntity

===============================================================================
*/

D3_CLASS()
class idTextEntity : public idEntity {
public:
	CLASS_PROTOTYPE( idTextEntity );

	void				Spawn( void );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

	virtual void		Think( void );

private:
	idStr				text;
	bool				playerOriented;
};


/*
===============================================================================

idLocationEntity

===============================================================================
*/

D3_CLASS()
class idLocationEntity : public idEntity {
public:
	CLASS_PROTOTYPE( idLocationEntity );

	void				Spawn( void );

	const char *		GetLocation( void ) const;

private:
};

D3_CLASS()
class idLocationSeparatorEntity : public idEntity {
public:
	CLASS_PROTOTYPE( idLocationSeparatorEntity );

	void				Spawn( void );

private:
};

D3_CLASS()
class idVacuumSeparatorEntity : public idEntity {
public:
	CLASS_PROTOTYPE( idVacuumSeparatorEntity );

						idVacuumSeparatorEntity( void );

	void				Spawn( void );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

	D3_EVENT( EV_Activate )
	void				Event_Activate( idEntity *activator );	

private:
	qhandle_t			portal;
};

D3_CLASS()
class idVacuumEntity : public idEntity {
public:
	CLASS_PROTOTYPE( idVacuumEntity );

	void				Spawn( void );

private:
};


/*
===============================================================================

  idBeam

===============================================================================
*/

D3_CLASS()
class idBeam : public idEntity {
public:
	CLASS_PROTOTYPE( idBeam );

						idBeam();

	void				Spawn( void );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

	virtual void		Think( void );

	void				SetMaster( idBeam *masterbeam );
	void				SetBeamTarget( const idVec3 &origin );

	virtual void		Show( void );

	virtual void		WriteToSnapshot( idBitMsgDelta &msg ) const;
	virtual void		ReadFromSnapshot( const idBitMsgDelta &msg );

private:
	D3_EVENT( EV_PostSpawn )
	void				Event_MatchTarget( void );
	D3_EVENT( EV_Activate )
	void				Event_Activate( idEntity *activator );

	idEntityPtr<idBeam>	target;
	idEntityPtr<idBeam>	master;
};


/*
===============================================================================

  idLiquid

===============================================================================
*/

class idRenderModelLiquid;

D3_CLASS()
class idLiquid : public idEntity {
public:
	CLASS_PROTOTYPE( idLiquid );

	void				Spawn( void );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

private:
	D3_EVENT( EV_Touch )
	void				Event_Touch( idEntity *other, trace_t *trace );


	idRenderModelLiquid *model;
};


/*
===============================================================================

  idShaking

===============================================================================
*/

D3_CLASS()
class idShaking : public idEntity {
public:
	CLASS_PROTOTYPE( idShaking );

							idShaking();

	void					Spawn( void );

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

private:
	idPhysics_Parametric	physicsObj;
	bool					active;

	void					BeginShaking( void );
	D3_EVENT( EV_Activate )
	void					Event_Activate( idEntity *activator );
};


/*
===============================================================================

  idEarthQuake

===============================================================================
*/

D3_CLASS()
class idEarthQuake : public idEntity {
public:
	CLASS_PROTOTYPE( idEarthQuake );
			
						idEarthQuake();

	void				Spawn( void );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

	virtual void		Think( void );

private:
	int					nextTriggerTime;
	int					shakeStopTime;
	float				wait;
	float				random;
	bool				triggered;
	bool				playerOriented;
	bool				disabled;
	float				shakeTime;

	D3_EVENT( EV_Activate )
	void				Event_Activate( idEntity *activator );
};


/*
===============================================================================

  idFuncPortal

===============================================================================
*/

D3_CLASS()
class idFuncPortal : public idEntity {
public:
	CLASS_PROTOTYPE( idFuncPortal );
			
						idFuncPortal();

	void				Spawn( void );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

private:
	qhandle_t			portal;
	bool				state;

	D3_EVENT( EV_Activate )
	void				Event_Activate( idEntity *activator );
};

/*
===============================================================================

  idFuncAASPortal

===============================================================================
*/

D3_CLASS()
class idFuncAASPortal : public idEntity {
public:
	CLASS_PROTOTYPE( idFuncAASPortal );
			
						idFuncAASPortal();

	void				Spawn( void );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

private:
	bool				state;

	D3_EVENT( EV_Activate )
	void				Event_Activate( idEntity *activator );
};

/*
===============================================================================

  idFuncAASObstacle

===============================================================================
*/

D3_CLASS()
class idFuncAASObstacle : public idEntity {
public:
	CLASS_PROTOTYPE( idFuncAASObstacle );
			
						idFuncAASObstacle();

	void				Spawn( void );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

private:
	bool				state;

	D3_EVENT( EV_Activate )
	void				Event_Activate( idEntity *activator );
};


/*
===============================================================================

idFuncRadioChatter

===============================================================================
*/

D3_CLASS()
class idFuncRadioChatter : public idEntity {
public:
	CLASS_PROTOTYPE( idFuncRadioChatter );

						idFuncRadioChatter();

	void				Spawn( void );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

private:
	float				time;
	D3_EVENT( EV_Activate )
	void				Event_Activate( idEntity *activator );
	D3_EVENT( EV_ResetRadioHud, "<resetradiohud>", void )
	void				Event_ResetRadioHud( idEntity *activator );
};


/*
===============================================================================

  idPhantomObjects

===============================================================================
*/

D3_CLASS()
class idPhantomObjects : public idEntity {
public:
	CLASS_PROTOTYPE( idPhantomObjects );
			
						idPhantomObjects();

	void				Spawn( void );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

	virtual void		Think( void );

private:
	D3_EVENT( EV_Activate )
	void				Event_Activate( idEntity *activator );
	void				Event_Throw( void );
	void				Event_ShakeObject( idEntity *object, int starttime );

	int					end_time;
	float				throw_time;
	float				shake_time;
	idVec3				shake_ang;
	float				speed;
	int					min_wait;
	int					max_wait;
	idEntityPtr<idActor>target;
	idList<int>			targetTime;
	idList<idVec3>		lastTargetPos;
};

/*
===============================================================================

idShockwave

===============================================================================
*/
D3_CLASS()
class idShockwave : public idEntity {
public:
	CLASS_PROTOTYPE( idShockwave );

	idShockwave();
	~idShockwave();

	void				Spawn( void );
	void				Think( void );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

private:
	D3_EVENT( EV_Activate )
	void				Event_Activate( idEntity *activator );

	bool				isActive;
	int					startTime;
	int					duration;

	float				startSize;
	float				endSize;
	float				currentSize;

	float				magnitude;

	float				height;
	bool				playerDamaged;
	float				playerDamageSize;

};

/*
===============================================================================

idFuncMountedObject

===============================================================================
*/
D3_CLASS()
class idFuncMountedObject : public idEntity {
public:
	CLASS_PROTOTYPE( idFuncMountedObject );

	idFuncMountedObject();
	~idFuncMountedObject();

	void				Spawn( void );
	void				Think( void );

	void				GetAngleRestrictions( int &yaw_min, int &yaw_max, int &pitch );

private:
	int					harc;
	int					varc;

	D3_EVENT( EV_Touch )
	void				Event_Touch( idEntity *other, trace_t *trace ); 
	D3_EVENT( EV_Activate )
	void				Event_Activate( idEntity *activator );

public:
	bool				isMounted;
	function_t	*		scriptFunction;
	idPlayer *			mountedPlayer;
};


D3_CLASS()
class idFuncMountedWeapon : public idFuncMountedObject {
public:
	CLASS_PROTOTYPE( idFuncMountedWeapon );

	idFuncMountedWeapon();
	~idFuncMountedWeapon();

	void				Spawn( void );
	void				Think( void );

private:

	// The actual turret that moves with the player's view
	idEntity	*		turret;

	// the muzzle bone's position, used for launching projectiles and trailing smoke
	idVec3				muzzleOrigin;
	idMat3				muzzleAxis;

	float				weaponLastFireTime;
	float				weaponFireDelay;

	const idDict *		projectile;

	const idSoundShader	*soundFireWeapon;

	D3_EVENT( EV_PostSpawn )
	void				Event_PostSpawn( void );
};

/*
===============================================================================

idPortalSky

===============================================================================
*/
D3_CLASS()
class idPortalSky : public idEntity {
public:
	CLASS_PROTOTYPE( idPortalSky );

	idPortalSky();
	~idPortalSky();

	void				Spawn( void );
	D3_EVENT( EV_PostSpawn )
	void				Event_PostSpawn();
	D3_EVENT( EV_Activate )
	void				Event_Activate( idEntity *activator );
};


#endif /* !__GAME_MISC_H__ */
