/*
===========================================================================

Game-side animation notifications and serialization bridges.

===========================================================================
*/
#ifndef __GAME_ANIMNOTIFY_H__
#define __GAME_ANIMNOTIFY_H__

class idAnimator;
class idSaveGame;
class idRestoreGame;

typedef enum {
	FC_SCRIPTFUNCTION,
	FC_SCRIPTFUNCTIONOBJECT,
	FC_EVENTFUNCTION,
	FC_SOUND,
	FC_SOUND_VOICE,
	FC_SOUND_VOICE2,
	FC_SOUND_BODY,
	FC_SOUND_BODY2,
	FC_SOUND_BODY3,
	FC_SOUND_WEAPON,
	FC_SOUND_ITEM,
	FC_SOUND_GLOBAL,
	FC_SOUND_CHATTER,
	FC_SKIN,
	FC_TRIGGER,
	FC_TRIGGER_SMOKE_PARTICLE,
	FC_MELEE,
	FC_DIRECTDAMAGE,
	FC_BEGINATTACK,
	FC_ENDATTACK,
	FC_MUZZLEFLASH,
	FC_CREATEMISSILE,
	FC_LAUNCHMISSILE,
	FC_FIREMISSILEATTARGET,
	FC_FOOTSTEP,
	FC_LEFTFOOT,
	FC_RIGHTFOOT,
	FC_ENABLE_EYE_FOCUS,
	FC_DISABLE_EYE_FOCUS,
	FC_FX,
	FC_DISABLE_GRAVITY,
	FC_ENABLE_GRAVITY,
	FC_JUMP,
	FC_ENABLE_CLIP,
	FC_DISABLE_CLIP,
	FC_ENABLE_WALK_IK,
	FC_DISABLE_WALK_IK,
	FC_ENABLE_LEG_IK,
	FC_DISABLE_LEG_IK,
	FC_RECORDDEMO,
	FC_AVIGAME,
	FC_LAUNCH_PROJECTILE,
	FC_TRIGGER_FX,
	FC_START_EMITTER,
	FC_STOP_EMITTER
} frameCommandType_t;

void	AnimNotify_Init( void );
void	AnimNotify_Shutdown( void );
void	Anim_SaveAnimator( const idAnimator &animator, idSaveGame *savefile );
void	Anim_RestoreAnimator( idAnimator &animator, idRestoreGame *savefile );

#endif
