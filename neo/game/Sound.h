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

#ifndef __GAME_SOUND_H__
#define __GAME_SOUND_H__

/*
===============================================================================

  Generic sound emitter.

===============================================================================
*/

D3_CLASS()
class idSound : public idEntity {
public:
	CLASS_PROTOTYPE( idSound );

					idSound( void );

	void			Save( idSaveGame *savefile ) const;
	void			Restore( idRestoreGame *savefile );

	virtual void	UpdateChangeableSpawnArgs( const idDict *source );

	void			Spawn( void );

	void			ToggleOnOff( idEntity *other, idEntity *activator );
	void			Think( void );
	void			SetSound( const char *sound, int channel = SND_CHANNEL_ANY );

	virtual void	ShowEditingDialog( void );

private:
	float			lastSoundVol;
	float			soundVol;
	float			random;
	float			wait;
	bool			timerOn;
	idVec3			shakeTranslate;
	idAngles		shakeRotate;
	int				playingUntilTime;

	D3_EVENT( EV_Activate )
	void			Event_Trigger( idEntity *activator );
	D3_EVENT( EV_Speaker_Timer, "<timer>", void )
	void			Event_Timer( void );
	D3_EVENT( EV_Speaker_On, "On", void )
	void			Event_On( void );
	D3_EVENT( EV_Speaker_Off, "Off", void )
	void			Event_Off( void );
	void			DoSound( bool play );
};

#endif /* !__GAME_SOUND_H__ */
