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

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "Game_local.h"

static void CopyCommandString( char *dest, const char *source ) {
	idStr::Copynz( dest, source, 256 );
}

static const char *ReadCommandString( idAnimCommandReader &src, char *dest ) {
	if ( !src.ReadTokenOnLine( dest, 256 ) ) {
		return "Unexpected end of line";
	}
	return NULL;
}

class idGameAnimNotify : public idAnimNotify {
public:
	virtual const char *ParseFrameCommand( int commandType, const idDeclModelDef *modelDef, idAnimCommandReader &src, animFrameCommand_t &command );
	virtual void ExecuteFrameCommand( void *owner, const idDeclModelDef *modelDef, const char *animName, int frame, const animFrameCommand_t &command );
	virtual void SetAnimatorActive( void *owner );
	virtual void SetAnimatorStopped( void *owner );
	virtual float RandomFloat( void );
	virtual int RandomInt( int max );
	virtual bool SkipAnimationFrame( void ) const;
	virtual bool DebugAnimator( const void *owner ) const;
	virtual const char *AnimatorName( const void *owner ) const;
	virtual int GameTime( void ) const;

private:
	void ExecuteSound( idEntity *entity, const char *animName, int frame, const animFrameCommand_t &command, s_channelType channel, int soundFlags );
};

static idGameAnimNotify gameAnimNotify;

const char *idGameAnimNotify::ParseFrameCommand( int commandType, const idDeclModelDef *modelDef, idAnimCommandReader &src, animFrameCommand_t &command ) {
	char token[ 256 ];
	const jointInfo_t *jointInfo;
	const char *error;

	command.type = commandType;
	command.index = 0;
	command.hasString = false;
	command.string[ 0 ] = '\0';
	command.string2[ 0 ] = '\0';

	switch ( commandType ) {
		case FC_SCRIPTFUNCTION:
			error = ReadCommandString( src, command.string );
			if ( error ) {
				return error;
			}
			if ( !gameLocal.program.FindFunction( command.string ) ) {
				return va( "Function '%s' not found", command.string );
			}
			command.hasString = true;
			break;

		case FC_EVENTFUNCTION: {
			error = ReadCommandString( src, command.string );
			if ( error ) {
				return error;
			}
			const idEventDef *event = idEventDef::FindEvent( command.string );
			if ( !event ) {
				return va( "Event '%s' not found", command.string );
			}
			if ( event->GetNumArgs() != 0 ) {
				return va( "Event '%s' has arguments", command.string );
			}
			command.hasString = true;
			break;
		}

		case FC_SOUND:
		case FC_SOUND_VOICE:
		case FC_SOUND_VOICE2:
		case FC_SOUND_BODY:
		case FC_SOUND_BODY2:
		case FC_SOUND_BODY3:
		case FC_SOUND_WEAPON:
		case FC_SOUND_ITEM:
		case FC_SOUND_GLOBAL:
		case FC_SOUND_CHATTER:
			error = ReadCommandString( src, command.string );
			if ( error ) {
				return error;
			}
			command.hasString = true;
			if ( idStr::Cmpn( command.string, "snd_", 4 ) != 0 ) {
				const idSoundShader *shader = declManager->FindSound( command.string );
				if ( shader->GetState() == DS_DEFAULTED ) {
					gameLocal.Warning( "Sound '%s' not found", command.string );
				}
			}
			break;

		case FC_SKIN:
			error = ReadCommandString( src, command.string );
			if ( error ) {
				return error;
			}
			command.hasString = true;
			if ( idStr::Icmp( command.string, "none" ) && !declManager->FindSkin( command.string, false ) ) {
				return va( "Skin '%s' not found", command.string );
			}
			break;

		case FC_FX:
			error = ReadCommandString( src, command.string );
			if ( error ) {
				return error;
			}
			if ( !declManager->FindType( DECL_FX, command.string ) ) {
				return va( "fx '%s' not found", command.string );
			}
			command.hasString = true;
			break;

		case FC_MELEE:
		case FC_DIRECTDAMAGE:
		case FC_BEGINATTACK:
			error = ReadCommandString( src, command.string );
			if ( error ) {
				return error;
			}
			if ( !gameLocal.FindEntityDef( command.string, false ) ) {
				return va( "Unknown entityDef '%s'", command.string );
			}
			command.hasString = true;
			break;

		case FC_LAUNCH_PROJECTILE:
			error = ReadCommandString( src, command.string );
			if ( error ) {
				return error;
			}
			if ( !declManager->FindDeclWithoutParsing( DECL_ENTITYDEF, command.string, false ) ) {
				return "Unknown projectile def";
			}
			command.hasString = true;
			break;

		case FC_SCRIPTFUNCTIONOBJECT:
		case FC_TRIGGER:
		case FC_TRIGGER_SMOKE_PARTICLE:
		case FC_STOP_EMITTER:
			error = ReadCommandString( src, command.string );
			if ( error ) {
				return error;
			}
			command.hasString = true;
			break;

		case FC_MUZZLEFLASH:
			if ( src.ReadTokenOnLine( token, sizeof( token ) ) ) {
				if ( !modelDef->FindJoint( token ) ) {
					return va( "Joint '%s' not found", token );
				}
				CopyCommandString( command.string, token );
				command.hasString = true;
			}
			break;

		case FC_CREATEMISSILE:
		case FC_LAUNCHMISSILE:
			error = ReadCommandString( src, command.string );
			if ( error ) {
				return error;
			}
			if ( !modelDef->FindJoint( command.string ) ) {
				return va( "Joint '%s' not found", command.string );
			}
			command.hasString = true;
			break;

		case FC_FIREMISSILEATTARGET:
		case FC_TRIGGER_FX:
			error = ReadCommandString( src, command.string2 );
			if ( error ) {
				return error;
			}
			jointInfo = modelDef->FindJoint( command.string2 );
			if ( !jointInfo ) {
				return va( "Joint '%s' not found", command.string2 );
			}
			error = ReadCommandString( src, command.string );
			if ( error ) {
				return error;
			}
			if ( commandType == FC_TRIGGER_FX && !declManager->FindType( DECL_FX, command.string, false ) ) {
				return "Unknown FX def";
			}
			command.index = jointInfo->num;
			command.hasString = true;
			break;

		case FC_START_EMITTER:
			error = ReadCommandString( src, command.string );
			if ( error ) {
				return error;
			}
			if ( !src.ReadTokenOnLine( token, sizeof( token ) ) ) {
				return "Unexpected end of line";
			}
			jointInfo = modelDef->FindJoint( token );
			if ( !jointInfo ) {
				return va( "Joint '%s' not found", token );
			}
			error = ReadCommandString( src, command.string2 );
			if ( error ) {
				return error;
			}
			command.index = jointInfo->num;
			command.hasString = true;
			break;

		case FC_ENABLE_LEG_IK:
		case FC_DISABLE_LEG_IK:
			if ( !src.ReadTokenOnLine( token, sizeof( token ) ) ) {
				return "Unexpected end of line";
			}
			command.index = atoi( token );
			break;

		case FC_RECORDDEMO:
		case FC_AVIGAME:
			if ( src.ReadTokenOnLine( token, sizeof( token ) ) ) {
				CopyCommandString( command.string, token );
				command.hasString = true;
			}
			break;

		default:
			break;
	}

	return NULL;
}

void idGameAnimNotify::ExecuteSound( idEntity *entity, const char *animName, int frame, const animFrameCommand_t &command, s_channelType channel, int soundFlags ) {
	if ( idStr::Cmpn( command.string, "snd_", 4 ) == 0 ) {
		if ( !entity->StartSound( command.string, channel, soundFlags, false, NULL ) ) {
			gameLocal.Warning( "Framecommand sound on entity '%s', anim '%s', frame %d: Could not find sound '%s'", entity->name.c_str(), animName, frame, command.string );
		}
	} else {
		entity->StartSoundShader( declManager->FindSound( command.string ), channel, soundFlags, false, NULL );
	}
}

void idGameAnimNotify::ExecuteFrameCommand( void *owner, const idDeclModelDef *modelDef, const char *animName, int frame, const animFrameCommand_t &command ) {
	idEntity *entity = static_cast<idEntity *>( owner );
	if ( !entity ) {
		return;
	}

	switch ( command.type ) {
		case FC_SCRIPTFUNCTION:
			gameLocal.CallFrameCommand( entity, gameLocal.program.FindFunction( command.string ) );
			break;
		case FC_SCRIPTFUNCTIONOBJECT:
			gameLocal.CallObjectFrameCommand( entity, command.string );
			break;
		case FC_EVENTFUNCTION: {
			const idEventDef *event = idEventDef::FindEvent( command.string );
			if ( event ) {
				entity->ProcessEvent( event );
			}
			break;
		}
		case FC_SOUND: ExecuteSound( entity, animName, frame, command, SND_CHANNEL_ANY, 0 ); break;
		case FC_SOUND_VOICE: ExecuteSound( entity, animName, frame, command, SND_CHANNEL_VOICE, 0 ); break;
		case FC_SOUND_VOICE2: ExecuteSound( entity, animName, frame, command, SND_CHANNEL_VOICE2, 0 ); break;
		case FC_SOUND_BODY: ExecuteSound( entity, animName, frame, command, SND_CHANNEL_BODY, 0 ); break;
		case FC_SOUND_BODY2: ExecuteSound( entity, animName, frame, command, SND_CHANNEL_BODY2, 0 ); break;
		case FC_SOUND_BODY3: ExecuteSound( entity, animName, frame, command, SND_CHANNEL_BODY3, 0 ); break;
		case FC_SOUND_WEAPON: ExecuteSound( entity, animName, frame, command, SND_CHANNEL_WEAPON, 0 ); break;
		case FC_SOUND_ITEM: ExecuteSound( entity, animName, frame, command, SND_CHANNEL_ITEM, 0 ); break;
		case FC_SOUND_GLOBAL: ExecuteSound( entity, animName, frame, command, SND_CHANNEL_ANY, SSF_GLOBAL ); break;
		case FC_SOUND_CHATTER:
			if ( entity->CanPlayChatterSounds() ) {
				ExecuteSound( entity, animName, frame, command, SND_CHANNEL_VOICE, 0 );
			}
			break;
		case FC_FX:
			idEntityFx::StartFx( command.string, NULL, NULL, entity, true );
			break;
		case FC_SKIN:
			entity->SetSkin( idStr::Icmp( command.string, "none" ) ? declManager->FindSkin( command.string ) : NULL );
			break;
		case FC_TRIGGER: {
			idEntity *target = gameLocal.FindEntity( command.string );
			if ( target ) {
				SetTimeState timeState( target->timeGroup );
				target->Signal( SIG_TRIGGER );
				target->ProcessEvent( &EV_Activate, entity );
				target->TriggerGuis();
			} else {
				gameLocal.Warning( "Framecommand trigger on entity '%s', anim '%s', frame %d: Could not find entity '%s'", entity->name.c_str(), animName, frame, command.string );
			}
			break;
		}
		case FC_TRIGGER_SMOKE_PARTICLE: entity->ProcessEvent( &AI_TriggerParticles, command.string ); break;
		case FC_MELEE: entity->ProcessEvent( &AI_AttackMelee, command.string ); break;
		case FC_DIRECTDAMAGE: entity->ProcessEvent( &AI_DirectDamage, command.string ); break;
		case FC_BEGINATTACK: entity->ProcessEvent( &AI_BeginAttack, command.string ); break;
		case FC_ENDATTACK: entity->ProcessEvent( &AI_EndAttack ); break;
		case FC_MUZZLEFLASH: entity->ProcessEvent( &AI_MuzzleFlash, command.string ); break;
		case FC_CREATEMISSILE: entity->ProcessEvent( &AI_CreateMissile, command.string ); break;
		case FC_LAUNCHMISSILE: entity->ProcessEvent( &AI_AttackMissile, command.string ); break;
		case FC_FIREMISSILEATTARGET: entity->ProcessEvent( &AI_FireMissileAtTarget, modelDef->GetJointName( command.index ), command.string ); break;
		case FC_LAUNCH_PROJECTILE: entity->ProcessEvent( &AI_LaunchProjectile, command.string ); break;
		case FC_TRIGGER_FX: entity->ProcessEvent( &AI_TriggerFX, modelDef->GetJointName( command.index ), command.string ); break;
		case FC_START_EMITTER:
			entity->ProcessEvent( &AI_StartEmitter, command.string, modelDef->GetJointName( command.index ), command.string2 );
			break;
		case FC_STOP_EMITTER:
			entity->ProcessEvent( &AI_StopEmitter, command.string );
			break;
		case FC_FOOTSTEP: entity->ProcessEvent( &EV_Footstep ); break;
		case FC_LEFTFOOT: entity->ProcessEvent( &EV_FootstepLeft ); break;
		case FC_RIGHTFOOT: entity->ProcessEvent( &EV_FootstepRight ); break;
		case FC_ENABLE_EYE_FOCUS: entity->ProcessEvent( &AI_EnableEyeFocus ); break;
		case FC_DISABLE_EYE_FOCUS: entity->ProcessEvent( &AI_DisableEyeFocus ); break;
		case FC_DISABLE_GRAVITY: entity->ProcessEvent( &AI_DisableGravity ); break;
		case FC_ENABLE_GRAVITY: entity->ProcessEvent( &AI_EnableGravity ); break;
		case FC_JUMP: entity->ProcessEvent( &AI_JumpFrame ); break;
		case FC_ENABLE_CLIP: entity->ProcessEvent( &AI_EnableClip ); break;
		case FC_DISABLE_CLIP: entity->ProcessEvent( &AI_DisableClip ); break;
		case FC_ENABLE_WALK_IK: entity->ProcessEvent( &EV_EnableWalkIK ); break;
		case FC_DISABLE_WALK_IK: entity->ProcessEvent( &EV_DisableWalkIK ); break;
		case FC_ENABLE_LEG_IK: entity->ProcessEvent( &EV_EnableLegIK, command.index ); break;
		case FC_DISABLE_LEG_IK: entity->ProcessEvent( &EV_DisableLegIK, command.index ); break;
		case FC_RECORDDEMO:
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, command.hasString ? va( "recordDemo %s", command.string ) : "stoprecording" );
			break;
		case FC_AVIGAME:
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, command.hasString ? va( "aviGame %s", command.string ) : "aviGame" );
			break;
	}
}

void idGameAnimNotify::SetAnimatorActive( void *owner ) {
	if ( owner ) {
		static_cast<idEntity *>( owner )->BecomeActive( TH_ANIMATE );
	}
}

void idGameAnimNotify::SetAnimatorStopped( void *owner ) {
	if ( owner ) {
		idEntity *entity = static_cast<idEntity *>( owner );
		entity->BecomeInactive( TH_ANIMATE );
		entity->BecomeActive( TH_UPDATEVISUALS );
	}
}

float idGameAnimNotify::RandomFloat( void ) {
	return gameLocal.random.RandomFloat();
}

int idGameAnimNotify::RandomInt( int max ) {
	return gameLocal.random.RandomInt( max );
}

bool idGameAnimNotify::SkipAnimationFrame( void ) const {
	return gameLocal.inCinematic && gameLocal.skipCinematic;
}

bool idGameAnimNotify::DebugAnimator( const void *owner ) const {
	const idEntity *entity = static_cast<const idEntity *>( owner );
	const int debugEntity = cvarSystem->GetCVarInteger( "g_debugAnim" );
	return entity && ( debugEntity == entity->entityNumber || debugEntity == -2 );
}

const char *idGameAnimNotify::AnimatorName( const void *owner ) const {
	const idEntity *entity = static_cast<const idEntity *>( owner );
	return entity ? entity->GetName() : "<no owner>";
}

int idGameAnimNotify::GameTime( void ) const {
	return gameLocal.time;
}

static const struct {
	const char *name;
	frameCommandType_t type;
} frameCommandNames[] = {
	{ "call", FC_SCRIPTFUNCTION }, { "object_call", FC_SCRIPTFUNCTIONOBJECT }, { "event", FC_EVENTFUNCTION },
	{ "sound", FC_SOUND }, { "sound_voice", FC_SOUND_VOICE }, { "sound_voice2", FC_SOUND_VOICE2 },
	{ "sound_body", FC_SOUND_BODY }, { "sound_body2", FC_SOUND_BODY2 }, { "sound_body3", FC_SOUND_BODY3 },
	{ "sound_weapon", FC_SOUND_WEAPON }, { "sound_item", FC_SOUND_ITEM }, { "sound_global", FC_SOUND_GLOBAL },
	{ "sound_chatter", FC_SOUND_CHATTER }, { "skin", FC_SKIN }, { "trigger", FC_TRIGGER },
	{ "triggerSmokeParticle", FC_TRIGGER_SMOKE_PARTICLE }, { "melee", FC_MELEE }, { "direct_damage", FC_DIRECTDAMAGE },
	{ "attack_begin", FC_BEGINATTACK }, { "attack_end", FC_ENDATTACK }, { "muzzle_flash", FC_MUZZLEFLASH },
	{ "create_missile", FC_CREATEMISSILE }, { "launch_missile", FC_LAUNCHMISSILE },
	{ "fire_missile_at_target", FC_FIREMISSILEATTARGET }, { "footstep", FC_FOOTSTEP },
	{ "leftfoot", FC_LEFTFOOT }, { "rightfoot", FC_RIGHTFOOT }, { "enableEyeFocus", FC_ENABLE_EYE_FOCUS },
	{ "disableEyeFocus", FC_DISABLE_EYE_FOCUS }, { "fx", FC_FX }, { "disableGravity", FC_DISABLE_GRAVITY },
	{ "enableGravity", FC_ENABLE_GRAVITY }, { "jump", FC_JUMP }, { "enableClip", FC_ENABLE_CLIP },
	{ "disableClip", FC_DISABLE_CLIP }, { "enableWalkIK", FC_ENABLE_WALK_IK }, { "disableWalkIK", FC_DISABLE_WALK_IK },
	{ "enableLegIK", FC_ENABLE_LEG_IK }, { "disableLegIK", FC_DISABLE_LEG_IK }, { "recordDemo", FC_RECORDDEMO },
	{ "aviGame", FC_AVIGAME }, { "launch_projectile", FC_LAUNCH_PROJECTILE }, { "trigger_fx", FC_TRIGGER_FX },
	{ "start_emitter", FC_START_EMITTER }, { "stop_emitter", FC_STOP_EMITTER }
};

void AnimNotify_Init( void ) {
	animationLib->SetNotify( &gameAnimNotify );
	animationLib->ClearFrameCommands();
	for ( int i = 0; i < sizeof( frameCommandNames ) / sizeof( frameCommandNames[ 0 ] ); i++ ) {
		animationLib->RegisterFrameCommand( frameCommandNames[ i ].name, frameCommandNames[ i ].type );
	}
}

void AnimNotify_Shutdown( void ) {
	animationLib->ClearFrameCommands();
	animationLib->SetNotify( NULL );
}

class idGameAnimSave : public idAnimSaveGame {
public:
	explicit idGameAnimSave( idSaveGame *gameSave ) : save( gameSave ) {}
	virtual void WriteInt( int value ) { save->WriteInt( value ); }
	virtual void WriteShort( short value ) { save->WriteShort( value ); }
	virtual void WriteFloat( float value ) { save->WriteFloat( value ); }
	virtual void WriteBool( bool value ) { save->WriteBool( value ); }
	virtual void WriteVec3( const idVec3 &value ) { save->WriteVec3( value ); }
	virtual void WriteMat3( const idMat3 &value ) { save->WriteMat3( value ); }
	virtual void WriteBounds( const idBounds &value ) { save->WriteBounds( value ); }
	virtual void WriteString( const char *value ) { save->WriteString( value ); }
	virtual void WriteObject( const void *object ) { save->WriteObject( static_cast<const idClass *>( object ) ); }
private:
	idSaveGame *save;
};

class idGameAnimRestore : public idAnimRestoreGame {
public:
	explicit idGameAnimRestore( idRestoreGame *gameRestore ) : restore( gameRestore ) {}
	virtual void ReadInt( int &value ) { restore->ReadInt( value ); }
	virtual void ReadShort( short &value ) { restore->ReadShort( value ); }
	virtual void ReadFloat( float &value ) { restore->ReadFloat( value ); }
	virtual void ReadBool( bool &value ) { restore->ReadBool( value ); }
	virtual void ReadVec3( idVec3 &value ) { restore->ReadVec3( value ); }
	virtual void ReadMat3( idMat3 &value ) { restore->ReadMat3( value ); }
	virtual void ReadBounds( idBounds &value ) { restore->ReadBounds( value ); }
	virtual void ReadString( char *value, int valueSize ) {
		idStr text;
		restore->ReadString( text );
		idStr::Copynz( value, text.c_str(), valueSize );
	}
	virtual void ReadObject( void *&object ) {
		idClass *gameObject = NULL;
		restore->ReadObject( gameObject );
		object = gameObject;
	}
private:
	idRestoreGame *restore;
};

void Anim_SaveAnimator( const idAnimator &animator, idSaveGame *savefile ) {
	idGameAnimSave bridge( savefile );
	animator.Save( &bridge );
}

void Anim_RestoreAnimator( idAnimator &animator, idRestoreGame *savefile ) {
	idGameAnimRestore bridge( savefile );
	animator.Restore( &bridge );
}
