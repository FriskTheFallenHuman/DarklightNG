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

/*
===============================================================================

	Grabber Object - Class to extend idWeapon to include functionality for 
						manipulating physics objects.

===============================================================================
*/

class idBeam;

D3_CLASS()
class idGrabber : public idEntity {
public:
	CLASS_PROTOTYPE( idGrabber );

							idGrabber( void );
							~idGrabber( void );

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	void					Initialize( void );
	void					SetDragDistance( float dist );
	int						Update( idPlayer *player, bool hide );

private:
	idEntityPtr<idEntity>	dragEnt;			// entity being dragged
	idForce_Grab			drag;
	idVec3					saveGravity;

	int						id;					// id of body being dragged
	idVec3					localPlayerPoint;	// dragged point in player space
	idEntityPtr<idPlayer>	owner;
	int						oldUcmdFlags;
	bool					holdingAF;
	bool					shakeForceFlip;
	int						endTime;
	int						lastFiredTime;
	int						dragFailTime;
	int						startDragTime;
	float					dragTraceDist;
	int						savedContents;
	int						savedClipmask;

	idBeam*					beam;
	idBeam*					beamTarget;

	int						warpId;

	bool					grabbableAI( const char *aiName );
	void					StartDrag( idEntity *grabEnt, int id );
	void					StopDrag( bool dropOnly );
	void					UpdateBeams( void );
	void					ApplyShake( void );
};

