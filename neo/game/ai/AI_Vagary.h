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

#ifndef __GAME_AI_VAGARY_H__
#define __GAME_AI_VAGARY_H__

// This class used to be private to AI_Vagary.cpp. Generated class metadata is
// instantiated by Class.cpp, so the declaration must be visible there too.
D3_CLASS()
class idAI_Vagary : public idAI {
public:
	CLASS_PROTOTYPE( idAI_Vagary );

private:
	D3_EVENT( AI_Vagary_ChooseObjectToThrow, "vagary_ChooseObjectToThrow", entity )
	void	Event_ChooseObjectToThrow( const idVec3 &mins, const idVec3 &maxs, float speed, float minDist, float offset );
	D3_EVENT( AI_Vagary_ThrowObjectAtEnemy, "vagary_ThrowObjectAtEnemy", void )
	void	Event_ThrowObjectAtEnemy( idEntity *ent, float speed );
};

#endif /* !__GAME_AI_VAGARY_H__ */
