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

#ifndef __GAME_MOVEABLE_H__
#define __GAME_MOVEABLE_H__

/*
===============================================================================

  Entity using rigid body physics.

===============================================================================
*/

extern const idEventDef EV_BecomeNonSolid;
extern const idEventDef EV_IsAtRest;

D3_CLASS()
class idMoveable : public idEntity {
public:
	CLASS_PROTOTYPE( idMoveable );

							idMoveable( void );
							~idMoveable( void );

	void					Spawn( void );

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	virtual void			Think( void );

	virtual void			Hide( void );
	virtual void			Show( void );

	bool					AllowStep( void ) const;
	void					EnableDamage( bool enable, float duration );
	virtual bool			Collide( const trace_t &collision, const idVec3 &velocity );
	virtual void			Killed( idEntity *inflictor, idEntity *attacker, int damage, const idVec3 &dir, int location );
	virtual void			WriteToSnapshot( idBitMsgDelta &msg ) const;
	virtual void			ReadFromSnapshot( const idBitMsgDelta &msg );

	void					SetAttacker( idEntity *ent );

protected:
	idPhysics_RigidBody		physicsObj;				// physics object
	idStr					brokenModel;			// model set when health drops down to or below zero
	idStr					damage;					// if > 0 apply damage to hit entities
	idStr					monsterDamage;
	idEntity				*attacker;
	idStr					fxCollide;				// fx system to start when collides with something
	int						nextCollideFxTime;		// next time it is ok to spawn collision fx
	float					minDamageVelocity;		// minimum velocity before moveable applies damage
	float					maxDamageVelocity;		// velocity at which the maximum damage is applied
	idCurve_Spline<idVec3> *initialSpline;			// initial spline path the moveable follows
	idVec3					initialSplineDir;		// initial relative direction along the spline path
	bool					explode;				// entity explodes when health drops down to or below zero
	bool					unbindOnDeath;			// unbind from master when health drops down to or below zero
	bool					allowStep;				// allow monsters to step on the object
	bool					canDamage;				// only apply damage when this is set
	int						nextDamageTime;			// next time the movable can hurt the player
	int						nextSoundTime;			// next time the moveable can make a sound

	const idMaterial *		GetRenderModelMaterial( void ) const;
	void					BecomeNonSolid( void );
	void					InitInitialSpline( int startTime );
	bool					FollowInitialSplinePath( void );

	D3_EVENT( EV_Activate )
	void					Event_Activate( idEntity *activator );
	D3_EVENT( EV_BecomeNonSolid )
	void					Event_BecomeNonSolid( void );
	D3_EVENT( EV_SetOwnerFromSpawnArgs, "<setOwnerFromSpawnArgs>", void )
	void					Event_SetOwnerFromSpawnArgs( void );
	D3_EVENT( EV_IsAtRest, "isAtRest", integer )
	void					Event_IsAtRest( void );
	D3_EVENT( EV_EnableDamage, "enableDamage", void )
	void					Event_EnableDamage( float enable );
};


/*
===============================================================================

  A barrel using rigid body physics. The barrel has special handling of
  the view model orientation to make it look like it rolls instead of slides.

===============================================================================
*/

D3_CLASS()
class idBarrel : public idMoveable {

public:
	CLASS_PROTOTYPE( idBarrel );
							idBarrel();

	void					Spawn( void );

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	void					BarrelThink( void );
	virtual void			Think( void );
	virtual bool			GetPhysicsToVisualTransform( idVec3 &origin, idMat3 &axis );
	virtual void			ClientPredictionThink( void );

private:
	float					radius;					// radius of barrel
	int						barrelAxis;				// one of the coordinate axes the barrel cylinder is parallel to
	idVec3					lastOrigin;				// origin of the barrel the last think frame
	idMat3					lastAxis;				// axis of the barrel the last think frame
	float					additionalRotation;		// additional rotation of the barrel about it's axis
	idMat3					additionalAxis;			// additional rotation axis
};


/*
===============================================================================

  A barrel using rigid body physics and special handling of the view model
  orientation to make it look like it rolls instead of slides. The barrel
  can burn and explode when damaged.

===============================================================================
*/

D3_CLASS()
class idExplodingBarrel : public idBarrel {
public:
	CLASS_PROTOTYPE( idExplodingBarrel );

							idExplodingBarrel();
							~idExplodingBarrel();

	void					Spawn( void );

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	bool					IsStable( void );
	void					SetStability( bool stability );
	void					StartBurning( void );
	void					StopBurning( void );

	virtual void			Think( void );
	virtual void			Damage( idEntity *inflictor, idEntity *attacker, const idVec3 &dir, 
								const char *damageDefName, const float damageScale, const int location );
	virtual void			Killed( idEntity *inflictor, idEntity *attacker, int damage, const idVec3 &dir, int location );

	virtual void			WriteToSnapshot( idBitMsgDelta &msg ) const;
	virtual void			ReadFromSnapshot( const idBitMsgDelta &msg );
	virtual bool			ClientReceiveEvent( int event, int time, const idBitMsg &msg );

	enum {
		EVENT_EXPLODE = idEntity::EVENT_MAXEVENTS,
		EVENT_MAXEVENTS
	};

private:
	typedef enum {
		NORMAL = 0,
		BURNING,
		BURNEXPIRED,
		EXPLODING
	} explode_state_t;
	explode_state_t			state;

	idVec3					spawnOrigin;
	idMat3					spawnAxis;
	idRenderEntity *		particleRenderEntity;
	idRenderLight *			light;
	int						particleTime;
	int						lightTime;
	float					time;
	bool					isStable;

	void					AddParticles( const char *name, bool burn );
	void					AddLight( const char *name , bool burn );
	void					ExplodingEffects( void );

	D3_EVENT( EV_Activate )
	void					Event_Activate( idEntity *activator );
	D3_EVENT( EV_Respawn, "<respawn>", void )
	void					Event_Respawn();
	D3_EVENT( EV_Explode, "<explode>", void )
	void					Event_Explode();
	D3_EVENT( EV_TriggerTargets, "<triggertargets>", void )
	void					Event_TriggerTargets();
};

#endif /* !__GAME_MOVEABLE_H__ */
