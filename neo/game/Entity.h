/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).  

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#ifndef __GAME_ENTITY_H__
#define __GAME_ENTITY_H__

/*
===============================================================================

	Game entity base class.

===============================================================================
*/

static const int DELAY_DORMANT_TIME = 3000;

extern const idEventDef EV_PostSpawn;
extern const idEventDef EV_FindTargets;
extern const idEventDef EV_Touch;
extern const idEventDef EV_Use;
extern const idEventDef EV_Activate;
extern const idEventDef EV_ActivateTargets;
extern const idEventDef EV_Hide;
extern const idEventDef EV_Show;
extern const idEventDef EV_GetShaderParm;
extern const idEventDef EV_SetShaderParm;
extern const idEventDef EV_SetOwner;
extern const idEventDef EV_GetAngles;
extern const idEventDef EV_SetAngles;
extern const idEventDef EV_SetLinearVelocity;
extern const idEventDef EV_SetAngularVelocity;
extern const idEventDef EV_SetSkin;
extern const idEventDef EV_StartSoundShader;
extern const idEventDef EV_StopSound;
extern const idEventDef EV_CacheSoundShader;

// Think flags
enum {
	TH_ALL					= -1,
	TH_THINK				= 1,		// run think function each frame
	TH_PHYSICS				= 2,		// run physics each frame
	TH_ANIMATE				= 4,		// update animation each frame
	TH_UPDATEVISUALS		= 8,		// update renderEntity
	TH_UPDATEPARTICLES		= 16
};

//
// Signals
// make sure to change script/doom_defs.script if you add any, or change their order
//
typedef enum {
	SIG_TOUCH,				// object was touched
	SIG_USE,				// object was used
	SIG_TRIGGER,			// object was activated
	SIG_REMOVED,			// object was removed from the game
	SIG_DAMAGE,				// object was damaged
	SIG_BLOCKED,			// object was blocked

	SIG_MOVER_POS1,			// mover at position 1 (door closed)
	SIG_MOVER_POS2,			// mover at position 2 (door open)
	SIG_MOVER_1TO2,			// mover changing from position 1 to 2
	SIG_MOVER_2TO1,			// mover changing from position 2 to 1

	NUM_SIGNALS
} signalNum_t;

// FIXME: At some point we may want to just limit it to one thread per signal, but
// for now, I'm allowing multiple threads.  We should reevaluate this later in the project
#define MAX_SIGNAL_THREADS 16		// probably overkill, but idList uses a granularity of 16

struct signal_t {
	int					threadnum;
	const function_t	*function;
};

class signalList_t {
public:
	idList<signal_t> signal[ NUM_SIGNALS ];
};


D3_EVENT( EV_ClearSignal, "clearSignal", void )
void D3_EventSignature_ClearSignal( int signal );
D3_EVENT( EV_MotionBlurOn, "motionBlurOn", void )
void D3_EventSignature_MotionBlurOn( void );
D3_EVENT( EV_MotionBlurOff, "motionBlurOff", void )
void D3_EventSignature_MotionBlurOff( void );
D3_CLASS( Abstract )
class idEntity : public idClass {
public:
	static const int		MAX_PVS_AREAS = 4;

	int						entityNumber;			// index into the entity list
	int						entityDefNumber;		// index into the entity def list

	idLinkList<idEntity>	spawnNode;				// for being linked into spawnedEntities list
	idLinkList<idEntity>	activeNode;				// for being linked into activeEntities list

	idLinkList<idEntity>	snapshotNode;			// for being linked into snapshotEntities list
	int						snapshotSequence;		// last snapshot this entity was in
	int						snapshotBits;			// number of bits this entity occupied in the last snapshot

	idStr					name;					// name of entity
	idDict					spawnArgs;				// key/value pairs used to spawn and initialize entity
	idScriptObject			scriptObject;			// contains all script defined data for this entity

	int						thinkFlags;				// TH_? flags
	int						dormantStart;			// time that the entity was first closed off from player
	bool					cinematic;				// during cinematics, entity will only think if cinematic is set

	renderView_t *			renderView;				// for camera views from this entity
	idEntity *				cameraTarget;			// any remoteRenderMap shaders will use this

	idList< idEntityPtr<idEntity> >	targets;		// when this entity is activated these entities entity are activated

	int						health;					// FIXME: do all objects really need health?

	struct entityFlags_s {
		bool				notarget			:1;	// if true never attack or target this entity
		bool				noknockback			:1;	// if true no knockback from hits
		bool				takedamage			:1;	// if true this entity can be damaged
		bool				hidden				:1;	// if true this entity is not visible
		bool				bindOrientated		:1;	// if true both the master orientation is used for binding
		bool				solidForTeam		:1;	// if true this entity is considered solid when a physics team mate pushes entities
		bool				forcePhysicsUpdate	:1;	// if true always update from the physics whether the object moved or not
		bool				selected			:1;	// if true the entity is selected for editing
		bool				neverDormant		:1;	// if true the entity never goes dormant
		bool				isDormant			:1;	// if true the entity is dormant
		bool				hasAwakened			:1;	// before a monster has been awakened the first time, use full PVS for dormant instead of area-connected
		bool				networkSync			:1; // if true the entity is synchronized over the network
		bool				grabbed				:1;	// if true object is currently being grabbed
	} fl;

	int						timeGroup;

	bool					noGrab;

	idRenderEntity *		xrayEntity;
	const idDeclSkin *		xraySkin;

	void					DetermineTimeGroup( bool slowmo );

	void					SetGrabbedState( bool grabbed );
	bool					IsGrabbed();

public:
	ABSTRACT_PROTOTYPE( idEntity );

							idEntity();
							~idEntity();

	void					Spawn( void );

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	const char *			GetEntityDefName( void ) const;
	void					SetName( const char *name );
	const char *			GetName( void ) const;
	virtual void			UpdateChangeableSpawnArgs( const idDict *source );

							// clients generate views based on all the player specific options,
							// cameras have custom code, and everything else just uses the axis orientation
	virtual renderView_t *	GetRenderView();

	// thinking
	virtual void			Think( void );
	bool					CheckDormant( void );	// dormant == on the active list, but out of PVS
	virtual	void			DormantBegin( void );	// called when entity becomes dormant
	virtual	void			DormantEnd( void );		// called when entity wakes from being dormant
	bool					IsActive( void ) const;
	void					BecomeActive( int flags );
	void					BecomeInactive( int flags );
	void					UpdatePVSAreas( const idVec3 &pos );

	// visuals
	virtual void			Present( void );
	virtual void			SnapshotRenderTransform( void );
	virtual void			PresentRenderInterpolation( float interpolation );
	virtual void			ResetRenderInterpolation( void );
	virtual idRenderEntity *GetRenderEntity( void );
	virtual idRenderEntity *GetRenderEntityDef( void );
	virtual void			SetModel( const char *modelname );
	void					SetSkin( const idDeclSkin *skin );
	const idDeclSkin *		GetSkin( void ) const;
	void					SetShaderParm( int parmnum, float value );
	virtual void			SetColor( float red, float green, float blue );
	virtual void			SetColor( const idVec3 &color );
	virtual void			GetColor( idVec3 &out ) const;
	virtual void			SetColor( const idVec4 &color );
	virtual void			GetColor( idVec4 &out ) const;
	virtual void			FreeModelDef( void );
	virtual void			FreeLightDef( void );
	virtual void			Hide( void );
	virtual void			Show( void );
	bool					IsHidden( void ) const;
	void					UpdateVisuals( void );
	void					UpdateModel( void );
	void					UpdateModelTransform( void );
	virtual void			ProjectOverlay( const idVec3 &origin, const idVec3 &dir, float size, const char *material );
	int						GetNumPVSAreas( void );
	const int *				GetPVSAreas( void );
	void					ClearPVSAreas( void );
	bool					PhysicsTeamInPVS( pvsHandle_t pvsHandle );

	// animation
	virtual bool			UpdateAnimationControllers( void );
	bool					UpdateRenderEntity( idRenderEntity *renderEntity, const renderView_t *renderView );
	static bool				ModelCallback( idRenderEntity *renderEntity, const renderView_t *renderView );
	virtual idAnimator *	GetAnimator( void );	// returns animator object used by this entity

	// sound
	virtual bool			CanPlayChatterSounds( void ) const;
	bool					StartSound( const char *soundName, const s_channelType channel, int soundShaderFlags, bool broadcast, int *length );
	bool					StartSoundShader( const idSoundShader *shader, const s_channelType channel, int soundShaderFlags, bool broadcast, int *length );
	void					StopSound( const s_channelType channel, bool broadcast );	// pass SND_CHANNEL_ANY to stop all sounds
	void					SetSoundVolume( float volume );
	void					UpdateSound( void );
	int						GetListenerId( void ) const;
	idSoundEmitter *		GetSoundEmitter( void ) const;
	void					FreeSoundEmitter( bool immediate );

	// entity binding
	virtual void			PreBind( void );
	virtual void			PostBind( void );
	virtual void			PreUnbind( void );
	virtual void			PostUnbind( void );
	void					JoinTeam( idEntity *teammember );
	void					Bind( idEntity *master, bool orientated );
	void					BindToJoint( idEntity *master, const char *jointname, bool orientated );
	void					BindToJoint( idEntity *master, jointHandle_t jointnum, bool orientated );
	void					BindToBody( idEntity *master, int bodyId, bool orientated );
	void					Unbind( void );
	bool					IsBound( void ) const;
	bool					IsBoundTo( idEntity *master ) const;
	idEntity *				GetBindMaster( void ) const;
	jointHandle_t			GetBindJoint( void ) const;
	int						GetBindBody( void ) const;
	idEntity *				GetTeamMaster( void ) const;
	idEntity *				GetNextTeamEntity( void ) const;
	void					ConvertLocalToWorldTransform( idVec3 &offset, idMat3 &axis );
	idVec3					GetLocalVector( const idVec3 &vec ) const;
	idVec3					GetLocalCoordinates( const idVec3 &vec ) const;
	idVec3					GetWorldVector( const idVec3 &vec ) const;
	idVec3					GetWorldCoordinates( const idVec3 &vec ) const;
	bool					GetMasterPosition( idVec3 &masterOrigin, idMat3 &masterAxis ) const;
	void					GetWorldVelocities( idVec3 &linearVelocity, idVec3 &angularVelocity ) const;

	// physics
							// set a new physics object to be used by this entity
	void					SetPhysics( idPhysics *phys );
							// get the physics object used by this entity
	idPhysics *				GetPhysics( void ) const;
							// restore physics pointer for save games
	void					RestorePhysics( idPhysics *phys );
							// run the physics for this entity
	bool					RunPhysics( void );
							// set the origin of the physics object (relative to bindMaster if not NULL)
	void					SetOrigin( const idVec3 &org );
							// set the axis of the physics object (relative to bindMaster if not NULL)
	void					SetAxis( const idMat3 &axis );
							// use angles to set the axis of the physics object (relative to bindMaster if not NULL)
	void					SetAngles( const idAngles &ang );
							// get the floor position underneath the physics object
	bool					GetFloorPos( float max_dist, idVec3 &floorpos ) const;
							// retrieves the transformation going from the physics origin/axis to the visual origin/axis
	virtual bool			GetPhysicsToVisualTransform( idVec3 &origin, idMat3 &axis );
							// retrieves the transformation going from the physics origin/axis to the sound origin/axis
	virtual bool			GetPhysicsToSoundTransform( idVec3 &origin, idMat3 &axis );
							// called from the physics object when colliding, should return true if the physics simulation should stop
	virtual bool			Collide( const trace_t &collision, const idVec3 &velocity );
							// retrieves impact information, 'ent' is the entity retrieving the info
	virtual void			GetImpactInfo( idEntity *ent, int id, const idVec3 &point, impactInfo_t *info );
							// apply an impulse to the physics object, 'ent' is the entity applying the impulse
	virtual void			ApplyImpulse( idEntity *ent, int id, const idVec3 &point, const idVec3 &impulse );
							// add a force to the physics object, 'ent' is the entity adding the force
	virtual void			AddForce( idEntity *ent, int id, const idVec3 &point, const idVec3 &force );
							// activate the physics object, 'ent' is the entity activating this entity
	virtual void			ActivatePhysics( idEntity *ent );
							// returns true if the physics object is at rest
	virtual bool			IsAtRest( void ) const;
							// returns the time the physics object came to rest
	virtual int				GetRestStartTime( void ) const;
							// add a contact entity
	virtual void			AddContactEntity( idEntity *ent );
							// remove a touching entity
	virtual void			RemoveContactEntity( idEntity *ent );

	// damage
							// returns true if this entity can be damaged from the given origin
	virtual bool			CanDamage( const idVec3 &origin, idVec3 &damagePoint ) const;
							// applies damage to this entity
	virtual	void			Damage( idEntity *inflictor, idEntity *attacker, const idVec3 &dir, const char *damageDefName, const float damageScale, const int location );
							// adds a damage effect like overlays, blood, sparks, debris etc.
	virtual void			AddDamageEffect( const trace_t &collision, const idVec3 &velocity, const char *damageDefName );
							// callback function for when another entity received damage from this entity.  damage can be adjusted and returned to the caller.
	virtual void			DamageFeedback( idEntity *victim, idEntity *inflictor, int &damage );
							// notifies this entity that it is in pain
	virtual bool			Pain( idEntity *inflictor, idEntity *attacker, int damage, const idVec3 &dir, int location );
							// notifies this entity that is has been killed
	virtual void			Killed( idEntity *inflictor, idEntity *attacker, int damage, const idVec3 &dir, int location );

	// scripting
	virtual bool			ShouldConstructScriptObjectAtSpawn( void ) const;
	virtual idThread *		ConstructScriptObject( void );
	virtual void			DeconstructScriptObject( void );
	void					SetSignal( signalNum_t signalnum, idThread *thread, const function_t *function );
	void					ClearSignal( idThread *thread, signalNum_t signalnum );
	void					ClearSignalThread( signalNum_t signalnum, idThread *thread );
	bool					HasSignal( signalNum_t signalnum ) const;
	void					Signal( signalNum_t signalnum );
	void					SignalEvent( idThread *thread, signalNum_t signalnum );

	// gui
	void					TriggerGuis( void );
	bool					HandleGuiCommands( idEntity *entityGui, const char *cmds );
	virtual bool			HandleSingleGuiCommand( idEntity *entityGui, idLexer *src );

	// targets
	void					FindTargets( void );
	void					RemoveNullTargets( void );
	void					ActivateTargets( idEntity *activator ) const;

	// misc
	virtual void			Teleport( const idVec3 &origin, const idAngles &angles, idEntity *destination );
	bool					TouchTriggers( void ) const;
	idCurve_Spline<idVec3> *GetSpline( void ) const;
	virtual void			ShowEditingDialog( void );

	enum {
		EVENT_STARTSOUNDSHADER,
		EVENT_STOPSOUNDSHADER,
		EVENT_MAXEVENTS
	};

	virtual void			ClientPredictionThink( void );
	virtual void			WriteToSnapshot( idBitMsgDelta &msg ) const;
	virtual void			ReadFromSnapshot( const idBitMsgDelta &msg );
	virtual bool			ServerReceiveEvent( int event, int time, const idBitMsg &msg );
	virtual bool			ClientReceiveEvent( int event, int time, const idBitMsg &msg );

	void					WriteBindToSnapshot( idBitMsgDelta &msg ) const;
	void					ReadBindFromSnapshot( const idBitMsgDelta &msg );
	void					WriteColorToSnapshot( idBitMsgDelta &msg ) const;
	void					ReadColorFromSnapshot( const idBitMsgDelta &msg );
	void					WriteGUIToSnapshot( idBitMsgDelta &msg ) const;
	void					ReadGUIFromSnapshot( const idBitMsgDelta &msg );

	void					ServerSendEvent( int eventId, const idBitMsg *msg, bool saveEvent, int excludeClient ) const;
	void					ClientSendEvent( int eventId, const idBitMsg *msg ) const;

protected:
	idRenderEntity *		renderEntity;						// renderer-owned presentation object
	idVec3					previousRenderOrigin;
	idMat3					previousRenderAxis;
	idVec3					currentRenderOrigin;
	idMat3					currentRenderAxis;
	bool					renderInterpolationValid;
	bool					renderInterpolationApplied;
	refSound_t				refSound;							// used to present sound to the audio engine

private:
	idPhysics_Static		defaultPhysicsObj;					// default physics object
	idPhysics *				physics;							// physics used for this entity
	idEntity *				bindMaster;							// entity bound to if unequal NULL
	jointHandle_t			bindJoint;							// joint bound to if unequal INVALID_JOINT
	int						bindBody;							// body bound to if unequal -1
	idEntity *				teamMaster;							// master of the physics team
	idEntity *				teamChain;							// next entity in physics team

	int						numPVSAreas;						// number of renderer areas the entity covers
	int						PVSAreas[MAX_PVS_AREAS];			// numbers of the renderer areas the entity covers

	signalList_t *			signals;

	int						mpGUIState;							// local cache to avoid systematic SetStateInt

private:
	void					FixupLocalizedStrings();

	bool					DoDormantTests( void );				// dormant == on the active list, but out of PVS

	// physics
							// initialize the default physics
	void					InitDefaultPhysics( const idVec3 &origin, const idMat3 &axis );
							// update visual position from the physics
	void					UpdateFromPhysics( bool moveBack );

	// entity binding
	bool					InitBind( idEntity *master );		// initialize an entity binding
	void					FinishBind( void );					// finish an entity binding
	void					RemoveBinds( void );				// deletes any entities bound to this object
	void					QuitTeam( void );					// leave the current team

	void					UpdatePVSAreas( void );

	// events
	D3_EVENT( EV_GetName, "getName", string )
	void					Event_GetName( void );
	D3_EVENT( EV_SetName, "setName", void )
	void					Event_SetName( const char *name );
	D3_EVENT( EV_FindTargets, "<findTargets>", void )
	void					Event_FindTargets( void );
	D3_EVENT( EV_ActivateTargets, "activateTargets", void )
	void					Event_ActivateTargets( idEntity *activator );
	D3_EVENT( EV_NumTargets, "numTargets", float )
	void					Event_NumTargets( void );
	D3_EVENT( EV_GetTarget, "getTarget", entity )
	void					Event_GetTarget( float index );
	D3_EVENT( EV_RandomTarget, "randomTarget", entity )
	void					Event_RandomTarget( const char *ignore );
	D3_EVENT( EV_Bind, "bind", void )
	void					Event_Bind( idEntity *master );
	D3_EVENT( EV_BindPosition, "bindPosition", void )
	void					Event_BindPosition( idEntity *master );
	D3_EVENT( EV_BindToJoint, "bindToJoint", void )
	void					Event_BindToJoint( idEntity *master, const char *jointname, float orientated );
	D3_EVENT( EV_Unbind, "unbind", void )
	void					Event_Unbind( void );
	D3_EVENT( EV_RemoveBinds, "removeBinds", void )
	void					Event_RemoveBinds( void );
	D3_EVENT( EV_SpawnBind, "<spawnbind>", void )
	void					Event_SpawnBind( void );
	D3_EVENT( EV_SetOwner, "setOwner", void )
	void					Event_SetOwner( idEntity *owner );
	D3_EVENT( EV_SetModel, "setModel", void )
	void					Event_SetModel( const char *modelname );
	D3_EVENT( EV_SetSkin, "setSkin", void )
	void					Event_SetSkin( const char *skinname );
	D3_EVENT( EV_GetShaderParm, "getShaderParm", float )
	void					Event_GetShaderParm( int parmnum );
	D3_EVENT( EV_SetShaderParm, "setShaderParm", void )
	void					Event_SetShaderParm( int parmnum, float value );
	D3_EVENT( EV_SetShaderParms, "setShaderParms", void )
	void					Event_SetShaderParms( float parm0, float parm1, float parm2, float parm3 );
	D3_EVENT( EV_SetColor, "setColor", void )
	void					Event_SetColor( float red, float green, float blue );
	D3_EVENT( EV_GetColor, "getColor", vector )
	void					Event_GetColor( void );
	D3_EVENT( EV_IsHidden, "isHidden", integer )
	void					Event_IsHidden( void );
	D3_EVENT( EV_Hide, "hide", void )
	void					Event_Hide( void );
	D3_EVENT( EV_Show, "show", void )
	void					Event_Show( void );
	D3_EVENT( EV_CacheSoundShader, "cacheSoundShader", void )
	void					Event_CacheSoundShader( const char *soundName );
	D3_EVENT( EV_StartSoundShader, "startSoundShader", float )
	void					Event_StartSoundShader( const char *soundName, gameSoundChannel_t channel );
	D3_EVENT( EV_StopSound )
	void					Event_StopSound( gameSoundChannel_t channel, int netSync );
	D3_EVENT( EV_StartSound, "startSound", float )
	void					Event_StartSound( const char *soundName, gameSoundChannel_t channel, int netSync );
	D3_EVENT( EV_FadeSound, "fadeSound", void )
	void					Event_FadeSound( gameSoundChannel_t channel, float to, float over );
	D3_EVENT( EV_GetWorldOrigin, "getWorldOrigin", vector )
	void					Event_GetWorldOrigin( void );
	D3_EVENT( EV_SetWorldOrigin, "setWorldOrigin", void )
	void					Event_SetWorldOrigin( idVec3 const &org );
	D3_EVENT( EV_GetOrigin, "getOrigin", vector )
	void					Event_GetOrigin( void );
	D3_EVENT( EV_SetOrigin, "setOrigin", void )
	void					Event_SetOrigin( const idVec3 &org );
	D3_EVENT( EV_GetAngles )
	void					Event_GetAngles( void );
	D3_EVENT( EV_SetAngles )
	void					Event_SetAngles( const idAngles &ang );
	D3_EVENT( EV_SetLinearVelocity, "setLinearVelocity", void )
	void					Event_SetLinearVelocity( const idVec3 &velocity );
	D3_EVENT( EV_GetLinearVelocity, "getLinearVelocity", vector )
	void					Event_GetLinearVelocity( void );
	D3_EVENT( EV_SetAngularVelocity, "setAngularVelocity", void )
	void					Event_SetAngularVelocity( const idVec3 &velocity );
	D3_EVENT( EV_GetAngularVelocity, "getAngularVelocity", vector )
	void					Event_GetAngularVelocity( void );
	D3_EVENT( EV_SetSize, "setSize", void )
	void					Event_SetSize( const idVec3 &mins, const idVec3 &maxs );
	D3_EVENT( EV_GetSize, "getSize", vector )
	void					Event_GetSize( void );
	D3_EVENT( EV_GetMins, "getMins", vector )
	void					Event_GetMins( void );
	D3_EVENT( EV_GetMaxs, "getMaxs", vector )
	void					Event_GetMaxs( void );
	D3_EVENT( EV_Touches, "touches", integer )
	void					Event_Touches( D3_NULLABLE idEntity *ent );
	D3_EVENT( EV_SetGuiParm, "setGuiParm", void )
	void					Event_SetGuiParm( const char *key, const char *val );
	D3_EVENT( EV_SetGuiFloat, "setGuiFloat", void )
	void					Event_SetGuiFloat( const char *key, float f );
	D3_EVENT( EV_GetNextKey, "getNextKey", string )
	void					Event_GetNextKey( const char *prefix, const char *lastMatch );
	D3_EVENT( EV_SetKey, "setKey", void )
	void					Event_SetKey( const char *key, const char *value );
	D3_EVENT( EV_GetKey, "getKey", string )
	void					Event_GetKey( const char *key );
	D3_EVENT( EV_GetIntKey, "getIntKey", float )
	void					Event_GetIntKey( const char *key );
	D3_EVENT( EV_GetFloatKey, "getFloatKey", float )
	void					Event_GetFloatKey( const char *key );
	D3_EVENT( EV_GetVectorKey, "getVectorKey", vector )
	void					Event_GetVectorKey( const char *key );
	D3_EVENT( EV_GetEntityKey, "getEntityKey", entity )
	void					Event_GetEntityKey( const char *key );
	D3_EVENT( EV_RestorePosition, "restorePosition", void )
	void					Event_RestorePosition( void );
	D3_EVENT( EV_UpdateCameraTarget, "<updateCameraTarget>", void )
	void					Event_UpdateCameraTarget( void );
	D3_EVENT( EV_DistanceTo, "distanceTo", float )
	void					Event_DistanceTo( D3_NULLABLE idEntity *ent );
	D3_EVENT( EV_DistanceToPoint, "distanceToPoint", float )
	void					Event_DistanceToPoint( const idVec3 &point );
	D3_EVENT( EV_StartFx, "startFx", void )
	void					Event_StartFx( const char *fx );
	D3_EVENT( EV_Thread_WaitFrame, "waitFrame", void )
	void					Event_WaitFrame( void );
	D3_EVENT( EV_Thread_Wait, "wait", void )
	void					Event_Wait( float time );
	D3_EVENT( EV_HasFunction, "hasFunction", integer )
	void					Event_HasFunction( const char *name );
	D3_EVENT( EV_CallFunction, "callFunction", void )
	void					Event_CallFunction( const char *name );
	D3_EVENT( EV_SetNeverDormant, "setNeverDormant", void )
	void					Event_SetNeverDormant( int enable );
	D3_EVENT( EV_SetGui, "setGui", void )
	void					Event_SetGui( int guiNum, const char *guiName);
	D3_EVENT( EV_PrecacheGui, "precacheGui", void )
	void					Event_PrecacheGui( const char *guiName );
	D3_EVENT( EV_GetGuiParm, "getGuiParm", string )
	void					Event_GetGuiParm(int guiNum, const char *key);
	D3_EVENT( EV_GetGuiParmFloat, "getGuiParmFloat", float )
	void					Event_GetGuiParmFloat(int guiNum, const char *key);
	D3_EVENT( EV_GuiNamedEvent, "guiNamedEvent", void )
	void					Event_GuiNamedEvent(int guiNum, const char *event);
};

/*
===============================================================================

	Animated entity base class.
	
===============================================================================
*/

typedef struct damageEffect_s {
	jointHandle_t			jointNum;
	idVec3					localOrigin;
	idVec3					localNormal;
	int						time;
	const idDeclParticle*	type;
	struct damageEffect_s *	next;
} damageEffect_t;

D3_CLASS()
class idAnimatedEntity : public idEntity {
public:
	CLASS_PROTOTYPE( idAnimatedEntity );

							idAnimatedEntity();
							~idAnimatedEntity();

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	virtual void			ClientPredictionThink( void );
	virtual void			Think( void );

	void					UpdateAnimation( void );

	virtual idAnimator *	GetAnimator( void );
	virtual void			SetModel( const char *modelname );

	bool					GetJointWorldTransform( jointHandle_t jointHandle, int currentTime, idVec3 &offset, idMat3 &axis );
	bool					GetJointTransformForAnim( jointHandle_t jointHandle, int animNum, int currentTime, idVec3 &offset, idMat3 &axis ) const;

	virtual int				GetDefaultSurfaceType( void ) const;
	virtual void			AddDamageEffect( const trace_t &collision, const idVec3 &velocity, const char *damageDefName );
	void					AddLocalDamageEffect( jointHandle_t jointNum, const idVec3 &localPoint, const idVec3 &localNormal, const idVec3 &localDir, const idDeclEntityDef *def, const idMaterial *collisionMaterial );
	void					UpdateDamageEffects( void );

	virtual bool			ClientReceiveEvent( int event, int time, const idBitMsg &msg );

	enum {
		EVENT_ADD_DAMAGE_EFFECT = idEntity::EVENT_MAXEVENTS,
		EVENT_MAXEVENTS
	};

protected:
	idAnimatorHandle		animator;
	damageEffect_t *		damageEffects;

private:
	D3_EVENT( EV_GetJointHandle, "getJointHandle", integer )
	void					Event_GetJointHandle( const char *jointname );
	D3_EVENT( EV_ClearAllJoints, "clearAllJoints", void )
	void 					Event_ClearAllJoints( void );
	D3_EVENT( EV_ClearJoint, "clearJoint", void )
	void 					Event_ClearJoint( jointHandle_t jointnum );
	D3_EVENT( EV_SetJointPos, "setJointPos", void )
	void 					Event_SetJointPos( jointHandle_t jointnum, jointModTransform_t transform_type, const idVec3 &pos );
	D3_EVENT( EV_SetJointAngle, "setJointAngle", void )
	void 					Event_SetJointAngle( jointHandle_t jointnum, jointModTransform_t transform_type, const idAngles &angles );
	D3_EVENT( EV_GetJointPos, "getJointPos", vector )
	void 					Event_GetJointPos( jointHandle_t jointnum );
	D3_EVENT( EV_GetJointAngle, "getJointAngle", vector )
	void 					Event_GetJointAngle( jointHandle_t jointnum );
};


class SetTimeState {
	bool					activated;
	bool					previousFast;
	bool					fast;

public:
							SetTimeState();
							SetTimeState( int timeGroup );
							~SetTimeState();

	void					PushState( int timeGroup );
};

ID_INLINE SetTimeState::SetTimeState() {
	activated = false;
}

ID_INLINE SetTimeState::SetTimeState( int timeGroup ) {
	activated = false;
	PushState( timeGroup );
}

ID_INLINE void SetTimeState::PushState( int timeGroup ) {

	// Don't mess with time in Multiplayer
	if ( !gameLocal.isMultiplayer ) {

		activated = true;

		// determine previous fast setting
		if ( gameLocal.time == gameLocal.slow.time ) {
			previousFast = false;
		}
		else {
			previousFast = true;
		}

		// determine new fast setting
		if ( timeGroup ) {
			fast = true;
		}
		else {
			fast = false;
		}

		// set correct time
		if ( fast ) {
			gameLocal.fast.Get( gameLocal.time, gameLocal.previousTime, gameLocal.msec, gameLocal.framenum, gameLocal.realClientTime );
		}
		else {
			gameLocal.slow.Get( gameLocal.time, gameLocal.previousTime, gameLocal.msec, gameLocal.framenum, gameLocal.realClientTime );
		}
	}
}

ID_INLINE SetTimeState::~SetTimeState() {
	if ( activated && !gameLocal.isMultiplayer ) {
		// set previous correct time
		if ( previousFast ) {
			gameLocal.fast.Get( gameLocal.time, gameLocal.previousTime, gameLocal.msec, gameLocal.framenum, gameLocal.realClientTime );
		}
		else {
			gameLocal.slow.Get( gameLocal.time, gameLocal.previousTime, gameLocal.msec, gameLocal.framenum, gameLocal.realClientTime );
		}
	}
}

#endif /* !__GAME_ENTITY_H__ */
