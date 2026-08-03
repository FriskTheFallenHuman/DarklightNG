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

#ifndef __GAME_WEAPON_H__
#define __GAME_WEAPON_H__

/*
===============================================================================

	Player Weapon
	
===============================================================================
*/

extern const idEventDef EV_Weapon_State;

typedef enum {
	WP_READY,
	WP_OUTOFAMMO,
	WP_RELOAD,
	WP_HOLSTERED,
	WP_RISING,
	WP_LOWERING
} weaponStatus_t;

typedef int ammo_t;
static const int AMMO_NUMTYPES = 16;

class idPlayer;

static const int LIGHTID_WORLD_MUZZLE_FLASH = 1;
static const int LIGHTID_VIEW_MUZZLE_FLASH = 100;

class idMoveableItem;

typedef struct {
	char			name[64];
	char			particlename[128];
	bool			active;
	int				startTime;
	jointHandle_t	joint;			//The joint on which to attach the particle
	bool			smoke;			//Is this a smoke particle
	const idDeclParticle* particle;		//Used for smoke particles
	idFuncEmitter*  emitter;		//Used for non-smoke particles
} WeaponParticle_t;

struct WeaponLight_t {
	WeaponLight_t() : active( false ), startTime( 0 ), joint( INVALID_JOINT ), light( NULL ) { name[0] = '\0'; }
	char			name[64];
	bool			active;
	int				startTime;
	jointHandle_t	joint;
	idRenderLight *	light;
};

D3_CLASS()
class idWeapon : public idAnimatedEntity {
public:
	CLASS_PROTOTYPE( idWeapon );

							idWeapon();
	virtual					~idWeapon();

	// Init
	void					Spawn( void );
	void					SetOwner( idPlayer *owner );
	idPlayer*				GetOwner( void );
	virtual bool			ShouldConstructScriptObjectAtSpawn( void ) const;

	static void				CacheWeapon( const char *weaponName );

	// save games
	void					Save( idSaveGame *savefile ) const;					// archives object for save game file
	void					Restore( idRestoreGame *savefile );					// unarchives object from save game file

	// Weapon definition management
	void					Clear( void );
	void					GetWeaponDef( const char *objectname, int ammoinclip );
	bool					IsLinked( void );
	bool					IsWorldModelReady( void );

	// GUIs
	const char *			Icon( void ) const;
	void					UpdateGUI( void );

	virtual void			SetModel( const char *modelname );
	bool					GetGlobalJointTransform( bool viewModel, const jointHandle_t jointHandle, idVec3 &offset, idMat3 &axis );
	void					SetPushVelocity( const idVec3 &pushVelocity );
	bool					UpdateSkin( void );

	// State control/player interface
	void					Think( void );
	void					Raise( void );
	void					PutAway( void );
	void					Reload( void );
	void					LowerWeapon( void );
	void					RaiseWeapon( void );
	void					HideWeapon( void );
	void					ShowWeapon( void );
	void					HideWorldModel( void );
	void					ShowWorldModel( void );
	void					OwnerDied( void );
	void					BeginAttack( void );
	void					EndAttack( void );
	bool					IsReady( void ) const;
	bool					IsReloading( void ) const;
	bool					IsHolstered( void ) const;
	bool					ShowCrosshair( void ) const;
	idEntity *				DropItem( const idVec3 &velocity, int activateDelay, int removeDelay, bool died );
	bool					CanDrop( void ) const;
	void					WeaponStolen( void );

	weaponStatus_t			GetStatus() { return status; };


	// Script state management
	virtual idThread *		ConstructScriptObject( void );
	virtual void			DeconstructScriptObject( void );
	void					SetState( const char *statename, int blendFrames );
	void					UpdateScript( void );
	void					EnterCinematic( void );
	void					ExitCinematic( void );
	void					NetCatchup( void );

	// Visual presentation
	void					PresentWeapon( bool showViewModel );
	int						GetZoomFov( void );
	void					GetWeaponAngleOffsets( int *average, float *scale, float *max );
	void					GetWeaponTimeOffsets( float *time, float *scale );
	bool					BloodSplat( float size );

	// Ammo
	static ammo_t			GetAmmoNumForName( const char *ammoname );
	static const char		*GetAmmoNameForNum( ammo_t ammonum );
	static const char		*GetAmmoPickupNameForNum( ammo_t ammonum );
	ammo_t					GetAmmoType( void ) const;
	int						AmmoAvailable( void ) const;
	int						AmmoInClip( void ) const;
	void					ResetAmmoClip( void );
	int						ClipSize( void ) const;
	int						LowAmmo( void ) const;
	int						AmmoRequired( void ) const;
	int						AmmoCount() const;
	int						GetGrabberState() const;

	virtual void			WriteToSnapshot( idBitMsgDelta &msg ) const;
	virtual void			ReadFromSnapshot( const idBitMsgDelta &msg );

	enum {
		EVENT_RELOAD = idEntity::EVENT_MAXEVENTS,
		EVENT_ENDRELOAD,
		EVENT_CHANGESKIN,
		EVENT_MAXEVENTS
	};
	virtual bool			ClientReceiveEvent( int event, int time, const idBitMsg &msg );

	virtual void			ClientPredictionThink( void );

private:
	// script control
	idScriptBool			WEAPON_ATTACK;
	idScriptBool			WEAPON_RELOAD;
	idScriptBool			WEAPON_NETRELOAD;
	idScriptBool			WEAPON_NETENDRELOAD;
	idScriptBool			WEAPON_RAISEWEAPON;
	idScriptBool			WEAPON_LOWERWEAPON;
	weaponStatus_t			status;
	idThread *				thread;
	idStr					state;
	idStr					idealState;
	int						animBlendFrames;
	int						animDoneTime;
	bool					isLinked;

	// precreated projectile
	idEntity				*projectileEnt;

	idPlayer *				owner;
	idEntityPtr<idAnimatedEntity>	worldModel;

	// hiding (for GUIs and NPCs)
	int						hideTime;
	float					hideDistance;
	int						hideStartTime;
	float					hideStart;
	float					hideEnd;
	float					hideOffset;
	bool					hide;
	bool					disabled;

	// berserk
	int						berserk;

	// these are the player render view parms, which include bobbing
	idVec3					playerViewOrigin;
	idMat3					playerViewAxis;

	// the view weapon render entity parms
	idVec3					viewWeaponOrigin;
	idMat3					viewWeaponAxis;
	
	// the muzzle bone's position, used for launching projectiles and trailing smoke
	idVec3					muzzleOrigin;
	idMat3					muzzleAxis;

	idVec3					pushVelocity;

	// weapon definition
	// we maintain local copies of the projectile and brass dictionaries so they
	// do not have to be copied across the DLL boundary when entities are spawned
	const idDeclEntityDef *	weaponDef;
	const idDeclEntityDef *	meleeDef;
	idDict					projectileDict;
	float					meleeDistance;
	idStr					meleeDefName;
	idDict					brassDict;
	int						brassDelay;
	idStr					icon;

	// view weapon gui light
	idRenderLight *			guiLight;

	// muzzle flash
	idRenderLight *			muzzleFlash;		// positioned on view weapon bone

	idRenderLight *			worldMuzzleFlash;	// positioned on world weapon bone

	idVec3					flashColor;
	int						muzzleFlashEnd;
	int						flashTime;
	bool					lightOn;
	bool					silent_fire;
	bool					allowDrop;

	// effects
	bool					hasBloodSplat;

	// weapon kick
	int						kick_endtime;
	int						muzzle_kick_time;
	int						muzzle_kick_maxtime;
	idAngles				muzzle_kick_angles;
	idVec3					muzzle_kick_offset;

	// ammo management
	ammo_t					ammoType;
	int						ammoRequired;		// amount of ammo to use each shot.  0 means weapon doesn't need ammo.
	int						clipSize;			// 0 means no reload
	int						ammoClip;
	int						lowAmmo;			// if ammo in clip hits this threshold, snd_
	bool					powerAmmo;			// true if the clip reduction is a factor of the power setting when
												// a projectile is launched
	// mp client
	bool					isFiring;

	// zoom
    int						zoomFov;			// variable zoom fov per weapon

	// joints from models
	jointHandle_t			barrelJointView;
	jointHandle_t			flashJointView;
	jointHandle_t			ejectJointView;
	jointHandle_t			guiLightJointView;
	jointHandle_t			ventLightJointView;

	jointHandle_t			flashJointWorld;
	jointHandle_t			barrelJointWorld;
	jointHandle_t			ejectJointWorld;

	jointHandle_t			smokeJointView;

	idHashTable<WeaponParticle_t>	weaponParticles;
	idHashTable<WeaponLight_t>		weaponLights;

	// sound
	const idSoundShader *	sndHum;

	// new style muzzle smokes
	const idDeclParticle *	weaponSmoke;			// null if it doesn't smoke
	int						weaponSmokeStartTime;	// set to gameLocal.time every weapon fire
	bool					continuousSmoke;		// if smoke is continuous ( chainsaw )
	const idDeclParticle *  strikeSmoke;			// striking something in melee
	int						strikeSmokeStartTime;	// timing	
	idVec3					strikePos;				// position of last melee strike	
	idMat3					strikeAxis;				// axis of last melee strike
	int						nextStrikeFx;			// used for sound and decal ( may use for strike smoke too )

	// nozzle effects
	bool					nozzleFx;			// does this use nozzle effects ( parm5 at rest, parm6 firing )
										// this also assumes a nozzle light atm
	int						nozzleFxFade;		// time it takes to fade between the effects
	int						lastAttack;			// last time an attack occured
	idRenderLight *			nozzleGlow;			// nozzle light

	idVec3					nozzleGlowColor;	// color of the nozzle glow
	const idMaterial *		nozzleGlowShader;	// shader for glow light
	float					nozzleGlowRadius;	// radius of glow light

	// weighting for viewmodel angles
	int						weaponAngleOffsetAverages;
	float					weaponAngleOffsetScale;
	float					weaponAngleOffsetMax;
	float					weaponOffsetTime;
	float					weaponOffsetScale;

	// flashlight
	void					AlertMonsters( void );

	// Visual presentation
	void					InitWorldModel( const idDeclEntityDef *def );
	void					MuzzleFlashLight( void );
	void					MuzzleRise( idVec3 &origin, idMat3 &axis );
	void					UpdateNozzleFx( void );
	void					UpdateFlashPosition( void );

	// script events
	D3_EVENT( EV_Weapon_Clear, "<clear>", void )
	void					Event_Clear( void );
	D3_EVENT( EV_Weapon_GetOwner, "getOwner", entity )
	void					Event_GetOwner( void );
	D3_EVENT( EV_Weapon_State, "weaponState", void )
	void					Event_WeaponState( const char *statename, int blendFrames );
	void					Event_SetWeaponStatus( float newStatus );
	D3_EVENT( EV_Weapon_WeaponReady, "weaponReady", void )
	void					Event_WeaponReady( void );
	D3_EVENT( EV_Weapon_WeaponOutOfAmmo, "weaponOutOfAmmo", void )
	void					Event_WeaponOutOfAmmo( void );
	D3_EVENT( EV_Weapon_WeaponReloading, "weaponReloading", void )
	void					Event_WeaponReloading( void );
	D3_EVENT( EV_Weapon_WeaponHolstered, "weaponHolstered", void )
	void					Event_WeaponHolstered( void );
	D3_EVENT( EV_Weapon_WeaponRising, "weaponRising", void )
	void					Event_WeaponRising( void );
	D3_EVENT( EV_Weapon_WeaponLowering, "weaponLowering", void )
	void					Event_WeaponLowering( void );
	D3_EVENT( EV_Weapon_UseAmmo, "useAmmo", void )
	void					Event_UseAmmo( int amount );
	D3_EVENT( EV_Weapon_AddToClip, "addToClip", void )
	void					Event_AddToClip( int amount );
	D3_EVENT( EV_Weapon_AmmoInClip, "ammoInClip", float )
	void					Event_AmmoInClip( void );
	D3_EVENT( EV_Weapon_AmmoAvailable, "ammoAvailable", float )
	void					Event_AmmoAvailable( void );
	D3_EVENT( EV_Weapon_TotalAmmoCount, "totalAmmoCount", float )
	void					Event_TotalAmmoCount( void );
	D3_EVENT( EV_Weapon_ClipSize, "clipSize", float )
	void					Event_ClipSize( void );
	D3_EVENT( AI_PlayAnim )
	void					Event_PlayAnim( animChannel_t channel, const char *animname );
	D3_EVENT( AI_PlayCycle )
	void					Event_PlayCycle( animChannel_t channel, const char *animname );
	D3_EVENT( AI_AnimDone )
	void					Event_AnimDone( animChannel_t channel, int blendFrames );
	D3_EVENT( AI_SetBlendFrames )
	void					Event_SetBlendFrames( animChannel_t channel, int blendFrames );
	D3_EVENT( AI_GetBlendFrames )
	void					Event_GetBlendFrames( animChannel_t channel );
	D3_EVENT( EV_Weapon_Next, "nextWeapon", void )
	void					Event_Next( void );
	D3_EVENT( EV_SetSkin )
	void					Event_SetSkin( const char *skinname );
	D3_EVENT( EV_Weapon_Flashlight, "flashlight", void )
	void					Event_Flashlight( int enable );
	D3_EVENT( EV_Light_GetLightParm )
	void					Event_GetLightParm( int parmnum );
	D3_EVENT( EV_Light_SetLightParm )
	void					Event_SetLightParm( int parmnum, float value );
	D3_EVENT( EV_Light_SetLightParms )
	void					Event_SetLightParms( float parm0, float parm1, float parm2, float parm3 );
	D3_EVENT( EV_Weapon_LaunchProjectiles, "launchProjectiles", void )
	void					Event_LaunchProjectiles( int num_projectiles, float spread, float fuseOffset, float launchPower, float dmgPower );
	D3_EVENT( EV_Weapon_CreateProjectile, "createProjectile", entity )
	void					Event_CreateProjectile( void );
	D3_EVENT( EV_Weapon_EjectBrass, "ejectBrass", void )
	void					Event_EjectBrass( void );
	D3_EVENT( EV_Weapon_Melee, "melee", integer )
	void					Event_Melee( void );
	D3_EVENT( EV_Weapon_GetWorldModel, "getWorldModel", entity )
	void					Event_GetWorldModel( void );
	D3_EVENT( EV_Weapon_AllowDrop, "allowDrop", void )
	void					Event_AllowDrop( int allow );
	D3_EVENT( EV_Weapon_AutoReload, "autoReload", float )
	void					Event_AutoReload( void );
	D3_EVENT( EV_Weapon_NetReload, "netReload", void )
	void					Event_NetReload( void );
	D3_EVENT( EV_Weapon_IsInvisible, "isInvisible", float )
	void					Event_IsInvisible( void );
	D3_EVENT( EV_Weapon_NetEndReload, "netEndReload", void )
	void					Event_NetEndReload( void );

	idGrabber				grabber;
	int						grabberState;

	D3_EVENT( EV_Weapon_Grabber, "grabber", void )
	void					Event_Grabber( int enable );
	D3_EVENT( EV_Weapon_GrabberHasTarget, "grabberHasTarget", integer )
	void					Event_GrabberHasTarget( void );
	D3_EVENT( EV_Weapon_Grabber_SetGrabDistance, "grabberGrabDistance", void )
	void					Event_GrabberSetGrabDistance( float dist );
	D3_EVENT( EV_Weapon_LaunchProjectilesEllipse, "launchProjectilesEllipse", void )
	void					Event_LaunchProjectilesEllipse( int num_projectiles, float spreada, float spreadb, float fuseOffset, float power );
	D3_EVENT( EV_Weapon_LaunchPowerup, "launchPowerup", void )
	void					Event_LaunchPowerup( const char* powerup, float duration, int useAmmo );

	D3_EVENT( EV_Weapon_StartWeaponSmoke, "startWeaponSmoke", void )
	void					Event_StartWeaponSmoke();
	D3_EVENT( EV_Weapon_StopWeaponSmoke, "stopWeaponSmoke", void )
	void					Event_StopWeaponSmoke();

	D3_EVENT( EV_Weapon_StartWeaponParticle, "startWeaponParticle", void )
	void					Event_StartWeaponParticle( const char* name);
	D3_EVENT( EV_Weapon_StopWeaponParticle, "stopWeaponParticle", void )
	void					Event_StopWeaponParticle( const char* name);

	D3_EVENT( EV_Weapon_StartWeaponLight, "startWeaponLight", void )
	void					Event_StartWeaponLight( const char* name);
	D3_EVENT( EV_Weapon_StopWeaponLight, "stopWeaponLight", void )
	void					Event_StopWeaponLight( const char* name);
};

ID_INLINE bool idWeapon::IsLinked( void ) {
	return isLinked;
}

ID_INLINE bool idWeapon::IsWorldModelReady( void ) {
	return ( worldModel.GetEntity() != NULL );
}

ID_INLINE idPlayer* idWeapon::GetOwner( void ) {
	return owner;
}

#endif /* !__GAME_WEAPON_H__ */
