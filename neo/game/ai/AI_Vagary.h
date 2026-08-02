/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

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
