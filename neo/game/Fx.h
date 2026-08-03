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

#ifndef __GAME_FX_H__
#define __GAME_FX_H__

/*
===============================================================================

  Special effects.

===============================================================================
*/

struct idFXLocalAction {
	idRenderLight *			renderLight;			// renderer-owned light
	idRenderEntity *		renderEntity;			// renderer-owned entity
	float					delay;
	int						particleSystem;
	int						start;
	bool					soundStarted;
	bool					shakeStarted;
	bool					decalDropped;
	bool					launched;

							idFXLocalAction() : renderLight( NULL ), renderEntity( NULL ), delay( 0.0f ), particleSystem( -1 ), start( -1 ), soundStarted( false ), shakeStarted( false ), decalDropped( false ), launched( false ) {}
};

D3_CLASS()
class idEntityFx : public idEntity {
public:
	CLASS_PROTOTYPE( idEntityFx );

							idEntityFx();
	virtual					~idEntityFx();

	void					Spawn( void );

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	virtual void			Think();
	void					Setup( const char *fx );
	void					Run( int time );
	void					Start( int time );
	void					Stop( void );
	const int				Duration( void );
	const char *			EffectName( void );
	const char *			Joint( void );
	const bool				Done();

	virtual void			WriteToSnapshot( idBitMsgDelta &msg ) const;
	virtual void			ReadFromSnapshot( const idBitMsgDelta &msg );
	virtual void			ClientPredictionThink( void );

	static idEntityFx *		StartFx( const char *fx, const idVec3 *useOrigin, const idMat3 *useAxis, idEntity *ent, bool bind );

protected:
	D3_EVENT( EV_Activate )
	void					Event_Trigger( idEntity *activator );
	D3_EVENT( EV_Fx_KillFx, "_killfx", void )
	void					Event_ClearFx( void );

	void					CleanUp( void );
	void					CleanUpSingleAction( const idFXSingleAction& fxaction, idFXLocalAction& laction );
	void					ApplyFade( const idFXSingleAction& fxaction, idFXLocalAction& laction, const int time, const int actualStart );

	int						started;
	int						nextTriggerTime;
	const idDeclFX *		fxEffect;				// GetFX() should be called before using fxEffect as a pointer
	idList<idFXLocalAction>	actions;
	idStr					systemName;
};

D3_CLASS()
class idTeleporter : public idEntityFx {
public:
	CLASS_PROTOTYPE( idTeleporter );

private:
	// teleporters to this location
	D3_EVENT( EV_Fx_Action, "_fxAction", void )
	void					Event_DoAction( idEntity *activator );
};

#endif /* !__GAME_FX_H__ */
