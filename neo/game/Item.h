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

#ifndef __GAME_ITEM_H__
#define __GAME_ITEM_H__


/*
===============================================================================

  Items the player can pick up or use.

===============================================================================
*/

D3_CLASS()
class idItem : public idEntity {
public:
	CLASS_PROTOTYPE( idItem );

							idItem();
	virtual					~idItem();

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	void					Spawn( void );
	void					GetAttributes( idDict &attributes );
	virtual bool			GiveToPlayer( idPlayer *player );
	virtual bool			Pickup( idPlayer *player );
	virtual void			Think( void );
	virtual void			Present();

	enum {
		EVENT_PICKUP = idEntity::EVENT_MAXEVENTS,
		EVENT_RESPAWN,
		EVENT_RESPAWNFX,
#ifdef CTF
        EVENT_TAKEFLAG,
        EVENT_DROPFLAG,
        EVENT_FLAGRETURN,
		EVENT_FLAGCAPTURE,
#endif
		EVENT_MAXEVENTS
	};

	virtual void			ClientPredictionThink( void );
	virtual bool			ClientReceiveEvent( int event, int time, const idBitMsg &msg );

	// networking
	virtual void			WriteToSnapshot( idBitMsgDelta &msg ) const;
	virtual void			ReadFromSnapshot( const idBitMsgDelta &msg );

private:
	idVec3					orgOrigin;
	bool					spin;
	bool					pulse;
	bool					canPickUp;

	// for item pulse effect
	idRenderEntity *		itemShellDef;
	const idMaterial *		shellMaterial;

	// used to update the item pulse effect
	mutable bool			inView;
	mutable int				inViewTime;
	mutable int				lastCycle;
	mutable int				lastRenderViewTime;

	bool					UpdateRenderEntity( idRenderEntity *renderEntity, const renderView_t *renderView ) const;
	static bool				ModelCallback( idRenderEntity *renderEntity, const renderView_t *renderView );

	D3_EVENT( EV_DropToFloor, "<dropToFloor>", void )
	void					Event_DropToFloor( void );
	D3_EVENT( EV_Touch )
	void					Event_Touch( idEntity *other, trace_t *trace );
	D3_EVENT( EV_Activate )
	void					Event_Trigger( idEntity *activator );
	D3_EVENT( EV_RespawnItem, "respawn", void )
	void					Event_Respawn( void );
	D3_EVENT( EV_RespawnFx, "<respawnFx>", void )
	void					Event_RespawnFx( void );
};

D3_CLASS()
class idItemPowerup : public idItem {
public:
	CLASS_PROTOTYPE( idItemPowerup );

							idItemPowerup();

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	void					Spawn();
	virtual bool			GiveToPlayer( idPlayer *player );

private:
	int						time;
	int						type;
};

D3_CLASS()
class idObjective : public idItem {
public:
	CLASS_PROTOTYPE( idObjective );

							idObjective();

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	void					Spawn();

private:
	idVec3					playerPos;

	D3_EVENT( EV_Activate )
	void					Event_Trigger( idEntity *activator );
	D3_EVENT( EV_HideObjective, "<hideobjective>", void )
	void					Event_HideObjective( idEntity *e );
	D3_EVENT( EV_GetPlayerPos, "<getplayerpos>", void )
	void					Event_GetPlayerPos();
	D3_EVENT( EV_CamShot, "<camshot>", void )
	void					Event_CamShot();
};

D3_CLASS()
class idVideoCDItem : public idItem {
public:
	CLASS_PROTOTYPE( idVideoCDItem );

	void					Spawn();
	virtual bool			GiveToPlayer( idPlayer *player );
};

D3_CLASS()
class idPDAItem : public idItem {
public:
	CLASS_PROTOTYPE( idPDAItem );

	virtual bool			GiveToPlayer( idPlayer *player );
};

D3_CLASS()
class idMoveableItem : public idItem {
public:
	CLASS_PROTOTYPE( idMoveableItem );

							idMoveableItem();
	virtual					~idMoveableItem();

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	void					Spawn( void );
	virtual void			Think( void );
	virtual bool			Collide( const trace_t &collision, const idVec3 &velocity );
	virtual bool			Pickup( idPlayer *player );

	static void				DropItems( idAnimatedEntity *ent, const char *type, idList<idEntity *> *list );
	static idEntity	*		DropItem( const char *classname, const idVec3 &origin, const idMat3 &axis, const idVec3 &velocity, int activateDelay, int removeDelay );

	virtual void			WriteToSnapshot( idBitMsgDelta &msg ) const;
	virtual void			ReadFromSnapshot( const idBitMsgDelta &msg );

#ifdef CTF    
protected:
#else
private:
#endif
	idPhysics_RigidBody		physicsObj;
	idClipModel *			trigger;
	const idDeclParticle *	smoke;
	int						smokeTime;

	int						nextSoundTime;
#ifdef CTF
	bool					repeatSmoke;	// never stop updating the particles
#endif

	void					Gib( const idVec3 &dir, const char *damageDefName );

	D3_EVENT( EV_DropToFloor )
	void					Event_DropToFloor( void );
	D3_EVENT( EV_Gib )
	void					Event_Gib( const char *damageDefName );
};

#ifdef CTF

D3_CLASS()
class idItemTeam : public idMoveableItem {
public:
    CLASS_PROTOTYPE( idItemTeam );

                            idItemTeam();
	virtual					~idItemTeam();

    void                    Spawn();
	virtual bool			Pickup( idPlayer *player );
	virtual bool			ClientReceiveEvent( int event, int time, const idBitMsg &msg );    
	virtual void			Think(void );

	void					Drop( bool death = false );	// was the drop caused by death of carrier?
	void					Return( idPlayer * player = NULL );
	void					Capture( void );

	virtual void			FreeLightDef( void );
	virtual void			Present( void );

	// networking
	virtual void			WriteToSnapshot( idBitMsgDelta &msg ) const;
	virtual void			ReadFromSnapshot( const idBitMsgDelta &msg );

public:
    int                     team;
	// TODO : turn this into a state : 
	bool					carried;			// is it beeing carried by a player?
	bool					dropped;			// was it dropped?

private:
	idVec3					returnOrigin;
	idMat3					returnAxis;
	int						lastDrop;

	const idDeclSkin *		skinDefault;
	const idDeclSkin *		skinCarried;

	const function_t *		scriptTaken;
	const function_t *		scriptDropped;
	const function_t *		scriptReturned;
	const function_t *		scriptCaptured;

    idRenderLight *         itemGlowDef;

	int						lastNuggetDrop;
	const char *			nuggetName;

private:

	D3_EVENT( EV_TakeFlag, "takeflag", void )
	void					Event_TakeFlag( idPlayer * player );
    D3_EVENT( EV_DropFlag, "dropflag", void )
    void					Event_DropFlag( bool death );
	D3_EVENT( EV_FlagReturn, "flagreturn", void )
	void					Event_FlagReturn( idPlayer * player = NULL );
	D3_EVENT( EV_FlagCapture, "flagcapture", void )
	void					Event_FlagCapture( void );

	void					PrivateReturn( void );
	function_t *			LoadScript( char * script );

	void					SpawnNugget( idVec3 pos );
    void                    UpdateGuis( void );
};

#endif


D3_CLASS()
class idMoveablePDAItem : public idMoveableItem {
public:
	CLASS_PROTOTYPE( idMoveablePDAItem );

	virtual bool			GiveToPlayer( idPlayer *player );
};

/*
===============================================================================

  Item removers.

===============================================================================
*/

D3_CLASS()
class idItemRemover : public idEntity {
public:
	CLASS_PROTOTYPE( idItemRemover );

	void					Spawn();
	void					RemoveItem( idPlayer *player );

private:
	D3_EVENT( EV_Activate )
	void					Event_Trigger( idEntity *activator );
};

D3_CLASS()
class idObjectiveComplete : public idItemRemover {
public:
	CLASS_PROTOTYPE( idObjectiveComplete );

							idObjectiveComplete();

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	void					Spawn();

private:
	idVec3					playerPos;

	D3_EVENT( EV_Activate )
	void					Event_Trigger( idEntity *activator );
	D3_EVENT( EV_HideObjective )
	void					Event_HideObjective( idEntity *e );
	D3_EVENT( EV_GetPlayerPos )
	void					Event_GetPlayerPos();
};

#endif /* !__GAME_ITEM_H__ */
