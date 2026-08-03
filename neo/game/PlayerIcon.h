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

#ifndef	__PLAYERICON_H__
#define	__PLAYERICON_H__

typedef enum {
	ICON_LAG,
	ICON_CHAT,
#ifdef CTF
	ICON_TEAM_RED,
	ICON_TEAM_BLUE,
#endif
	ICON_NONE
} playerIconType_t;

class idPlayerIcon {
public:
	
public:
	idPlayerIcon();
	~idPlayerIcon();

	void	Draw( idPlayer *player, jointHandle_t joint );
	void	Draw( idPlayer *player, const idVec3 &origin );

public:
	playerIconType_t	iconType;
	idRenderEntity *	renderEnt;

public:
	void	FreeIcon( void );
	bool	CreateIcon( idPlayer* player, playerIconType_t type, const char *mtr, const idVec3 &origin, const idMat3 &axis );
	bool	CreateIcon( idPlayer* player, playerIconType_t type, const idVec3 &origin, const idMat3 &axis );
	void	UpdateIcon( idPlayer* player, const idVec3 &origin, const idMat3 &axis );

};


#endif	/* !_PLAYERICON_H_ */

