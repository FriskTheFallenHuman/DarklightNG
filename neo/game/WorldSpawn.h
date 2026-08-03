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

#ifndef __GAME_WORLDSPAWN_H__
#define __GAME_WORLDSPAWN_H__

/*
===============================================================================

  World entity.

===============================================================================
*/

D3_CLASS()
class idWorldspawn : public idEntity {
public:
	CLASS_PROTOTYPE( idWorldspawn );

					~idWorldspawn();

	void			Spawn( void );

	void			Save( idRestoreGame *savefile );
	void			Restore( idRestoreGame *savefile );

private:
	D3_EVENT( EV_Remove )
	D3_EVENT( EV_SafeRemove )
	void			Event_Remove( void );
};

#endif /* !__GAME_WORLDSPAWN_H__ */
