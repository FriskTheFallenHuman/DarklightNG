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

#ifndef __GAME_TRIGGER_H__
#define __GAME_TRIGGER_H__

extern const idEventDef EV_Enable;
extern const idEventDef EV_Disable;

/*
===============================================================================

  Trigger base.

===============================================================================
*/

D3_CLASS()
class idTrigger : public idEntity {
public:
	CLASS_PROTOTYPE( idTrigger );

	static void			DrawDebugInfo( void );

						idTrigger();
	void				Spawn( void );

	const function_t *	GetScriptFunction( void ) const;

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

	virtual void		Enable( void );
	virtual void		Disable( void );

protected:
	void				CallScript( void ) const;

	D3_EVENT( EV_Enable, "enable", void )
	void				Event_Enable( void );
	D3_EVENT( EV_Disable, "disable", void )
	void				Event_Disable( void );

	const function_t *	scriptFunction;
};


/*
===============================================================================

  Trigger which can be activated multiple times.

===============================================================================
*/

D3_CLASS()
class idTrigger_Multi : public idTrigger {
public:
	CLASS_PROTOTYPE( idTrigger_Multi );

						idTrigger_Multi( void );

	void				Spawn( void );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

#ifdef CTF    
protected:
#else
private:
#endif    

	float				wait;
	float				random;
	float				delay;
	float				random_delay;
	int					nextTriggerTime;
	idStr				requires;
	int					removeItem;
	bool				touchClient;
	bool				touchOther;
	bool				triggerFirst;
	bool				triggerWithSelf;

	bool				CheckFacing( idEntity *activator );
	void				TriggerAction( idEntity *activator );
	D3_EVENT( EV_TriggerAction, "<triggerAction>", void )
	void				Event_TriggerAction( idEntity *activator );
	D3_EVENT( EV_Activate )
	void				Event_Trigger( idEntity *activator );
	D3_EVENT( EV_Touch )
	void				Event_Touch( idEntity *other, trace_t *trace );
};


/*
===============================================================================

  Trigger which can only be activated by an entity with a specific name.

===============================================================================
*/

D3_CLASS()
class idTrigger_EntityName : public idTrigger {
public:
	CLASS_PROTOTYPE( idTrigger_EntityName );

						idTrigger_EntityName( void );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

	void				Spawn( void );

private:
	float				wait;
	float				random;
	float				delay;
	float				random_delay;
	int					nextTriggerTime;
	bool				triggerFirst;
	idStr				entityName;

	void				TriggerAction( idEntity *activator );
	D3_EVENT( EV_TriggerAction )
	void				Event_TriggerAction( idEntity *activator );
	D3_EVENT( EV_Activate )
	void				Event_Trigger( idEntity *activator );
	D3_EVENT( EV_Touch )
	void				Event_Touch( idEntity *other, trace_t *trace );
};

/*
===============================================================================

  Trigger which repeatedly fires targets.

===============================================================================
*/

D3_CLASS()
class idTrigger_Timer : public idTrigger {
public:
	CLASS_PROTOTYPE( idTrigger_Timer );

						idTrigger_Timer( void );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

	void				Spawn( void );

	virtual void		Enable( void );
	virtual void		Disable( void );

private:
	float				random;
	float				wait;
	bool				on;
	float				delay;
	idStr				onName;
	idStr				offName;

	D3_EVENT( EV_Timer, "<timer>", void )
	void				Event_Timer( void );
	D3_EVENT( EV_Activate )
	void				Event_Use( idEntity *activator );
};


/*
===============================================================================

  Trigger which fires targets after being activated a specific number of times.

===============================================================================
*/

D3_CLASS()
class idTrigger_Count : public idTrigger {
public:
	CLASS_PROTOTYPE( idTrigger_Count );

						idTrigger_Count( void );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

	void				Spawn( void );

private:
	int					goal;
	int					count;
	float				delay;

	D3_EVENT( EV_Activate )
	void				Event_Trigger( idEntity *activator );
	D3_EVENT( EV_TriggerAction )
	void				Event_TriggerAction( idEntity *activator );
};


/*
===============================================================================

  Trigger which hurts touching entities.

===============================================================================
*/

D3_CLASS()
class idTrigger_Hurt : public idTrigger {
public:
	CLASS_PROTOTYPE( idTrigger_Hurt );

						idTrigger_Hurt( void );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

	void				Spawn( void );

private:
	bool				on;
	float				delay;
	int					nextTime;

	D3_EVENT( EV_Touch )
	void				Event_Touch( idEntity *other, trace_t *trace );
	D3_EVENT( EV_Activate )
	void				Event_Toggle( idEntity *activator );
};


/*
===============================================================================

  Trigger which fades the player view.

===============================================================================
*/

D3_CLASS()
class idTrigger_Fade : public idTrigger {
public:

	CLASS_PROTOTYPE( idTrigger_Fade );

private:
	D3_EVENT( EV_Activate )
	void				Event_Trigger( idEntity *activator );
};


/*
===============================================================================

  Trigger which continuously tests whether other entities are touching it.

===============================================================================
*/

D3_CLASS()
class idTrigger_Touch : public idTrigger {
public:

	CLASS_PROTOTYPE( idTrigger_Touch );

						idTrigger_Touch( void );

	void				Spawn( void );
	virtual void		Think( void );

	void				Save( idSaveGame *savefile );
	void				Restore( idRestoreGame *savefile );

	virtual void		Enable( void );
	virtual void		Disable( void );

	void				TouchEntities( void );

private:
	idClipModel *		clipModel;

	D3_EVENT( EV_Activate )
	void				Event_Trigger( idEntity *activator );
};

#ifdef CTF
/*
===============================================================================

  Trigger that responces to CTF flags

===============================================================================
*/
D3_CLASS()
class idTrigger_Flag : public idTrigger_Multi {
public:
	CLASS_PROTOTYPE( idTrigger_Flag );

						idTrigger_Flag( void );
	void				Spawn( void );

private:
	int					team;
	bool				player;			// flag must be attached/carried by player

	const idEventDef *	eventFlag;

	D3_EVENT( EV_Touch )
	void				Event_Touch( idEntity *other, trace_t *trace );
};

#endif /* CTF */

#endif /* !__GAME_TRIGGER_H__ */
