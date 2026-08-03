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
#ifndef __ANIM_H__
#define __ANIM_H__

// Metadata annotations are inert outside the game type-info build.
#ifndef D3_ENUM
#define D3_ENUM( ... )
#endif

//
// animation channels
// these can be changed by modmakers and licensees to be whatever they need.
const int ANIM_NumAnimChannels		= 5;
const int ANIM_MaxAnimsPerChannel	= 3;
const int ANIM_MaxSyncedAnims		= 3;

//
// animation channels.  make sure to change script/doom_defs.script if you add any channels, or change their order
//
D3_ENUM( BlueprintType )
enum animChannel_t {
	ANIMCHANNEL_ALL			= 0,
	ANIMCHANNEL_TORSO			= 1,
	ANIMCHANNEL_LEGS			= 2,
	ANIMCHANNEL_HEAD			= 3,
	ANIMCHANNEL_EYELIDS		= 4
};

// for converting from 24 frames per second to milliseconds
ID_INLINE int FRAME2MS( int framenum ) {
	return ( framenum * 1000 ) / 24;
}

class idRenderModel;
class idAnimator;
class idAnimatorHandle;
class idAnimatorLocal;
class idAnimBlend;
class idAnimManager;
class idAnimNotify;
class idDeclModelDef;

typedef struct {
	int		cycleCount;	// how many times the anim has wrapped to the begining (0 for clamped anims)
	int		frame1;
	int		frame2;
	float	frontlerp;
	float	backlerp;
} frameBlend_t;

typedef struct {
	int						nameIndex;
	int						parentNum;
	int						animBits;
	int						firstComponent;
} jointAnimInfo_t;

typedef struct {
	jointHandle_t			num;
	jointHandle_t			parentNum;
	int						channel;
} jointInfo_t;

//
// joint modifier modes.  make sure to change script/doom_defs.script if you add any, or change their order.
//
D3_ENUM( BlueprintType )
typedef enum {
	JOINTMOD_NONE,				// no modification
	JOINTMOD_LOCAL,				// modifies the joint's position or orientation in joint local space
	JOINTMOD_LOCAL_OVERRIDE,	// sets the joint's position or orientation in joint local space
	JOINTMOD_WORLD,				// modifies joint's position or orientation in model space
	JOINTMOD_WORLD_OVERRIDE		// sets the joint's position or orientation in model space
} jointModTransform_t;

typedef struct {
	jointHandle_t			jointnum;
	idMat3					mat;
	idVec3					pos;
	jointModTransform_t		transform_pos;
	jointModTransform_t		transform_axis;
} jointMod_t;

#define	ANIM_TX				BIT( 0 )
#define	ANIM_TY				BIT( 1 )
#define	ANIM_TZ				BIT( 2 )
#define	ANIM_QX				BIT( 3 )
#define	ANIM_QY				BIT( 4 )
#define	ANIM_QZ				BIT( 5 )

typedef struct {
	int						num;
	int						firstCommand;
} frameLookup_t;

typedef struct {
	int						type;
	int						index;
	bool						hasString;
	char					string[ 256 ];
	char					string2[ 256 ];
} animFrameCommand_t;

typedef animFrameCommand_t frameCommand_t;

typedef struct {
	bool					prevent_idle_override		: 1;
	bool					random_cycle_start			: 1;
	bool					ai_no_turn					: 1;
	bool					anim_turn					: 1;
} animFlags_t;

class idAnimSaveGame {
public:
	virtual						~idAnimSaveGame() {}
	virtual void				WriteInt( int value ) = 0;
	virtual void				WriteShort( short value ) = 0;
	virtual void				WriteFloat( float value ) = 0;
	virtual void				WriteBool( bool value ) = 0;
	virtual void				WriteVec3( const idVec3 &value ) = 0;
	virtual void				WriteMat3( const idMat3 &value ) = 0;
	virtual void				WriteBounds( const idBounds &value ) = 0;
	virtual void				WriteString( const char *value ) = 0;
	virtual void				WriteObject( const void *object ) = 0;
};

class idAnimRestoreGame {
public:
	virtual						~idAnimRestoreGame() {}
	virtual void				ReadInt( int &value ) = 0;
	virtual void				ReadShort( short &value ) = 0;
	virtual void				ReadFloat( float &value ) = 0;
	virtual void				ReadBool( bool &value ) = 0;
	virtual void				ReadVec3( idVec3 &value ) = 0;
	virtual void				ReadMat3( idMat3 &value ) = 0;
	virtual void				ReadBounds( idBounds &value ) = 0;
	virtual void				ReadString( char *value, int valueSize ) = 0;
	virtual void				ReadObject( void *&object ) = 0;
};

class idAnimCommandReader {
public:
	virtual						~idAnimCommandReader() {}
	// Tokens are copied into caller-owned storage so lexer objects never cross the DLL boundary.
	virtual bool				ReadTokenOnLine( char *value, int valueSize ) = 0;
};

class idAnimNotify {
public:
	virtual						~idAnimNotify() {}
	virtual const char *		ParseFrameCommand( int commandType, const idDeclModelDef *modelDef, idAnimCommandReader &src, animFrameCommand_t &command ) = 0;
	virtual void				ExecuteFrameCommand( void *owner, const idDeclModelDef *modelDef, const char *animName, int frame, const animFrameCommand_t &command ) = 0;
	virtual void				SetAnimatorActive( void *owner ) = 0;
	virtual void				SetAnimatorStopped( void *owner ) = 0;
	virtual float				RandomFloat( void ) = 0;
	virtual int					RandomInt( int max ) = 0;
	virtual bool				SkipAnimationFrame( void ) const = 0;
	virtual bool				DebugAnimator( const void *owner ) const = 0;
	virtual const char *		AnimatorName( const void *owner ) const = 0;
	virtual int					GameTime( void ) const = 0;
};


/*
==============================================================================================

	idMD5Anim

==============================================================================================
*/

class idMD5Anim {
private:
	int						numFrames;
	int						frameRate;
	int						animLength;
	int						numJoints;
	int						numAnimatedComponents;
	idList<idBounds>		bounds;
	idList<jointAnimInfo_t>	jointInfo;
	idList<idJointQuat>		baseFrame;
	idList<float>			componentFrames;
	idStr					name;
	idVec3					totaldelta;
	mutable int				ref_count;

public:
							idMD5Anim();
	virtual					~idMD5Anim();

	virtual void				Free( void );
	virtual bool				Reload( void );
	virtual size_t			Allocated( void ) const;
	virtual size_t			Size( void ) const { return sizeof( *this ) + Allocated(); };
	virtual bool				LoadAnim( const char *filename );

	virtual void				IncreaseRefs( void ) const;
	virtual void				DecreaseRefs( void ) const;
	virtual int				NumRefs( void ) const;
	
	virtual void				CheckModelHierarchy( const idRenderModel *model ) const;
	virtual void				GetInterpolatedFrame( frameBlend_t &frame, idJointQuat *joints, const int *index, int numIndexes ) const;
	virtual void				GetSingleFrame( int framenum, idJointQuat *joints, const int *index, int numIndexes ) const;
	virtual int				Length( void ) const;
	virtual int				NumFrames( void ) const;
	virtual int				NumJoints( void ) const;
	virtual const idVec3		&TotalMovementDelta( void ) const;
	virtual const char			*Name( void ) const;

	virtual void				GetFrameBlend( int framenum, frameBlend_t &frame ) const;	// frame 1 is first frame
	virtual void				ConvertTimeToFrame( int time, int cyclecount, frameBlend_t &frame ) const;

	virtual void				GetOrigin( idVec3 &offset, int currentTime, int cyclecount ) const;
	virtual void				GetOriginRotation( idQuat &rotation, int time, int cyclecount ) const;
	virtual void				GetBounds( idBounds &bounds, int currentTime, int cyclecount ) const;
};

/*
==============================================================================================

	idAnim

==============================================================================================
*/

class idAnim {
private:
	const class idDeclModelDef	*modelDef;
	const idMD5Anim				*anims[ ANIM_MaxSyncedAnims ];
	int							numAnims;
	idStr						name;
	idStr						realname;
	idList<frameLookup_t>		frameLookup;
	idList<frameCommand_t>		frameCommands;
	animFlags_t					flags;

public:
							idAnim();
							idAnim( const idDeclModelDef *modelDef, const idAnim *anim );
	virtual					~idAnim();

	virtual void				SetAnim( const idDeclModelDef *modelDef, const char *sourcename, const char *animname, int num, const idMD5Anim *md5anims[ ANIM_MaxSyncedAnims ] );
	virtual const char			*Name( void ) const;
	virtual const char			*FullName( void ) const;
	virtual const idMD5Anim		*MD5Anim( int num ) const;
	virtual const idDeclModelDef	*ModelDef( void ) const;
	virtual int					Length( void ) const;
	virtual int					NumFrames( void ) const;
	virtual int					NumAnims( void ) const;
	virtual const idVec3			&TotalMovementDelta( void ) const;
	virtual bool					GetOrigin( idVec3 &offset, int animNum, int time, int cyclecount ) const;
	virtual bool					GetOriginRotation( idQuat &rotation, int animNum, int currentTime, int cyclecount ) const;
	virtual bool					GetBounds( idBounds &bounds, int animNum, int time, int cyclecount ) const;
	virtual const char			*AddFrameCommand( const class idDeclModelDef *modelDef, int framenum, idLexer &src, const idDict *def );
	virtual void					CallFrameCommands( void *owner, int from, int to ) const;
	virtual bool					HasFrameCommands( void ) const;

								// returns first frame (zero based) that command occurs.  returns -1 if not found.
	virtual int					FindFrameForFrameCommand( int framecommand, const frameCommand_t **command ) const;
	virtual void					SetAnimFlags( const animFlags_t &animflags );
	virtual const animFlags_t	&GetAnimFlags( void ) const;
};

/*
==============================================================================================

	idDeclModelDef

==============================================================================================
*/

class idDeclModelDef : public idDecl {
public:
							idDeclModelDef();
	virtual					~idDeclModelDef();

	virtual size_t				Size( void ) const;
	virtual const char *		DefaultDefinition( void ) const;
	virtual bool				Parse( const char *text, const int textLength );
	virtual void				FreeData( void );

	virtual void				Touch( void ) const;

	virtual const idDeclSkin *	GetDefaultSkin( void ) const;
	virtual const idJointQuat *	GetDefaultPose( void ) const;
	virtual void					SetupJoints( int *numJoints, idJointMat **jointList, idBounds &frameBounds, bool removeOriginOffset ) const;
	virtual idRenderModel *		ModelHandle( void ) const;
	virtual int					GetJointListBuffer( const char *jointnames, jointHandle_t *jointList, int maxJoints ) const;
	// This inline wrapper keeps the idList allocation in the calling module.
	void						GetJointList( const char *jointnames, idList<jointHandle_t> &jointList ) const {
		jointList.SetNum( NumJoints() );
		jointList.SetNum( GetJointListBuffer( jointnames, jointList.Ptr(), jointList.Num() ) );
	}
	virtual const jointInfo_t *	FindJoint( const char *name ) const;

	virtual int					NumAnims( void ) const;
	virtual const idAnim *		GetAnim( int index ) const;
	virtual int					GetSpecificAnim( const char *name ) const;
	virtual int					GetAnim( const char *name ) const;
	virtual bool					HasAnim( const char *name ) const;
	virtual const idDeclSkin *	GetSkin( void ) const;
	virtual const char *			GetModelName( void ) const;
	virtual const int *			JointParents( void ) const;
	virtual int					NumJoints( void ) const;
	virtual const jointInfo_t *	GetJoint( int jointHandle ) const;
	virtual const char *			GetJointName( int jointHandle ) const;
	virtual int					NumJointsOnChannel( int channel ) const;
	virtual const int *			GetChannelJoints( int channel ) const;

	virtual const idVec3 &		GetVisualOffset( void ) const;

private:
	void						CopyDecl( const idDeclModelDef *decl );
	bool						ParseAnim( idLexer &src, int numDefaultAnims );

private:
	idVec3						offset;
	idList<jointInfo_t>			joints;
	idList<int>					jointParents;
	idList<int>					channelJoints[ ANIM_NumAnimChannels ];
	idRenderModel *				modelHandle;
	idList<idAnim *>			anims;
	const idDeclSkin *			skin;
};

/*
==============================================================================================

	idAnimBlend

==============================================================================================
*/

class idAnimBlend {
private:
	const class idDeclModelDef	*modelDef;
	int							starttime;
	int							endtime;
	int							timeOffset;
	float						rate;

	int							blendStartTime;
	int							blendDuration;
	float						blendStartValue;
	float						blendEndValue;

	float						animWeights[ ANIM_MaxSyncedAnims ];
	short						cycle;
	short						frame;
	short						animNum;
	bool						allowMove;
	bool						allowFrameCommands;

	friend class				idAnimatorLocal;

	void						Reset( const idDeclModelDef *_modelDef );
	void						CallFrameCommands( void *owner, int fromtime, int totime ) const;
	void						SetFrame( const idDeclModelDef *modelDef, int animnum, int frame, int currenttime, int blendtime );
	void						CycleAnim( const idDeclModelDef *modelDef, int animnum, int currenttime, int blendtime );
	void						PlayAnim( const idDeclModelDef *modelDef, int animnum, int currenttime, int blendtime );
	bool						BlendAnim( int currentTime, int channel, int numJoints, idJointQuat *blendFrame, float &blendWeight, bool removeOrigin, bool overrideBlend, bool printInfo ) const;
	void						BlendOrigin( int currentTime, idVec3 &blendPos, float &blendWeight, bool removeOriginOffset ) const;
	void						BlendDelta( int fromtime, int totime, idVec3 &blendDelta, float &blendWeight ) const;
	void						BlendDeltaRotation( int fromtime, int totime, idQuat &blendDelta, float &blendWeight ) const;
	bool						AddBounds( int currentTime, idBounds &bounds, bool removeOriginOffset ) const;

public:
							idAnimBlend();
	virtual					~idAnimBlend() {}
	virtual void				Save( idAnimSaveGame *savefile ) const;
	virtual void				Restore( idAnimRestoreGame *savefile, const idDeclModelDef *modelDef );
	virtual const char			*AnimName( void ) const;
	virtual const char			*AnimFullName( void ) const;
	virtual float				GetWeight( int currenttime ) const;
	virtual float				GetFinalWeight( void ) const;
	virtual void				SetWeight( float newweight, int currenttime, int blendtime );
	virtual int					NumSyncedAnims( void ) const;
	virtual bool					SetSyncedAnimWeight( int num, float weight );
	virtual void				Clear( int currentTime, int clearTime );
	virtual bool					IsDone( int currentTime ) const;
	virtual bool					FrameHasChanged( int currentTime ) const;
	virtual int					GetCycleCount( void ) const;
	virtual void				SetCycleCount( int count );
	virtual void				SetPlaybackRate( int currentTime, float newRate );
	virtual float				GetPlaybackRate( void ) const;
	virtual void				SetStartTime( int startTime );
	virtual int					GetStartTime( void ) const;
	virtual int					GetEndTime( void ) const;
	virtual int					GetFrameNumber( int currenttime ) const;
	virtual int					AnimTime( int currenttime ) const;
	virtual int					NumFrames( void ) const;
	virtual int					Length( void ) const;
	virtual int					PlayLength( void ) const;
	virtual void				AllowMovement( bool allow );
	virtual void				AllowFrameCommands( bool allow );
	virtual const idAnim			*Anim( void ) const;
	virtual int					AnimNum( void ) const;
};

/*
==============================================================================================

	idAFPoseJointMod

==============================================================================================
*/

typedef enum {
	AF_JOINTMOD_AXIS,
	AF_JOINTMOD_ORIGIN,
	AF_JOINTMOD_BOTH
} AFJointModType_t;

class idAFPoseJointMod {
public:
								idAFPoseJointMod( void );

	AFJointModType_t			mod;
	idMat3						axis;
	idVec3						origin;
};

ID_INLINE idAFPoseJointMod::idAFPoseJointMod( void ) {
	mod = AF_JOINTMOD_AXIS;
	axis.Identity();
	origin.Zero();
}

/*
==============================================================================================

	idAnimator

==============================================================================================
*/

class idAnimator {
public:
	virtual						~idAnimator() {}
	virtual void				Destroy( void ) = 0;
	virtual size_t				Allocated( void ) const = 0;
	virtual size_t				Size( void ) const = 0;
	virtual void				Save( idAnimSaveGame *savefile ) const = 0;
	virtual void				Restore( idAnimRestoreGame *savefile ) = 0;
	virtual void				SetOwner( void *owner ) = 0;
	virtual void *				GetOwner( void ) const = 0;
	virtual void				RemoveOriginOffset( bool remove ) = 0;
	virtual bool				RemoveOrigin( void ) const = 0;
	virtual int				GetJointListBuffer( const char *jointnames, jointHandle_t *jointList, int maxJoints ) const = 0;
	// This inline wrapper keeps the idList allocation in the calling module.
	void						GetJointList( const char *jointnames, idList<jointHandle_t> &jointList ) const {
		jointList.SetNum( NumJoints() );
		jointList.SetNum( GetJointListBuffer( jointnames, jointList.Ptr(), jointList.Num() ) );
	}
	virtual int					NumAnims( void ) const = 0;
	virtual const idAnim			*GetAnim( int index ) const = 0;
	virtual int					GetAnim( const char *name ) const = 0;
	virtual bool					HasAnim( const char *name ) const = 0;
	virtual void				ServiceAnims( int fromtime, int totime ) = 0;
	virtual bool					IsAnimating( int currentTime ) const = 0;
	virtual void				GetJoints( int *numJoints, idJointMat **jointsPtr ) = 0;
	virtual int					NumJoints( void ) const = 0;
	virtual jointHandle_t		GetFirstChild( jointHandle_t jointnum ) const = 0;
	virtual jointHandle_t		GetFirstChild( const char *name ) const = 0;
	virtual idRenderModel			*SetModel( const char *modelname ) = 0;
	virtual idRenderModel			*ModelHandle( void ) const = 0;
	virtual const idDeclModelDef	*ModelDef( void ) const = 0;
	virtual void				ForceUpdate( void ) = 0;
	virtual void				ClearForceUpdate( void ) = 0;
	virtual bool					CreateFrame( int animtime, bool force ) = 0;
	virtual bool					FrameHasChanged( int animtime ) const = 0;
	virtual void				GetDelta( int fromtime, int totime, idVec3 &delta ) const = 0;
	virtual bool					GetDeltaRotation( int fromtime, int totime, idMat3 &delta ) const = 0;
	virtual void				GetOrigin( int currentTime, idVec3 &pos ) const = 0;
	virtual bool					GetBounds( int currentTime, idBounds &bounds ) = 0;
	virtual idAnimBlend			*CurrentAnim( int channelNum ) = 0;
	virtual void				Clear( int channelNum, int currentTime, int cleartime ) = 0;
	virtual void				SetFrame( int channelNum, int animnum, int frame, int currenttime, int blendtime ) = 0;
	virtual void				CycleAnim( int channelNum, int animnum, int currenttime, int blendtime ) = 0;
	virtual void				PlayAnim( int channelNum, int animnum, int currenttime, int blendTime ) = 0;
	virtual void				SyncAnimChannels( int channelNum, int fromChannelNum, int currenttime, int blendTime ) = 0;
	virtual void				SetJointPos( jointHandle_t jointnum, jointModTransform_t transform_type, const idVec3 &pos ) = 0;
	virtual void				SetJointAxis( jointHandle_t jointnum, jointModTransform_t transform_type, const idMat3 &mat ) = 0;
	virtual void				ClearJoint( jointHandle_t jointnum ) = 0;
	virtual void				ClearAllJoints( void ) = 0;
	virtual void				InitAFPose( void ) = 0;
	virtual void				SetAFPoseJointMod( jointHandle_t jointNum, AFJointModType_t mod, const idMat3 &axis, const idVec3 &origin ) = 0;
	virtual void				FinishAFPose( int animnum, const idBounds &bounds, int time ) = 0;
	virtual void				SetAFPoseBlendWeight( float blendWeight ) = 0;
	virtual bool					BlendAFPose( idJointQuat *blendFrame ) const = 0;
	virtual void				ClearAFPose( void ) = 0;
	virtual void				ClearAllAnims( int currentTime, int cleartime ) = 0;
	virtual jointHandle_t		GetJointHandle( const char *name ) const = 0;
	virtual const char			*GetJointName( jointHandle_t handle ) const = 0;
	virtual int					GetChannelForJoint( jointHandle_t joint ) const = 0;
	virtual bool					GetJointTransform( jointHandle_t jointHandle, int currenttime, idVec3 &offset, idMat3 &axis ) = 0;
	virtual bool					GetJointLocalTransform( jointHandle_t jointHandle, int currentTime, idVec3 &offset, idMat3 &axis ) = 0;
	virtual const animFlags_t	GetAnimFlags( int animnum ) const = 0;
	virtual int					NumFrames( int animnum ) const = 0;
	virtual int					NumSyncedAnims( int animnum ) const = 0;
	virtual const char			*AnimName( int animnum ) const = 0;
	virtual const char			*AnimFullName( int animnum ) const = 0;
	virtual int					AnimLength( int animnum ) const = 0;
	virtual const idVec3			&TotalMovementDelta( int animnum ) const = 0;
};

/*
==============================================================================================

	idAnimManager

==============================================================================================
*/

class idAnimManager {
public:
	virtual						~idAnimManager() {}
	virtual void				RegisterDeclTypes( void ) = 0;
	virtual void				Shutdown( void ) = 0;
	virtual idMD5Anim *			GetAnim( const char *name ) = 0;
	virtual idAnimator *			AllocAnimator( void *owner ) = 0;
	virtual void				ReloadAnims( void ) = 0;
	virtual void				ListAnims( void ) const = 0;
	virtual int					JointIndex( const char *name ) = 0;
	virtual const char *			JointName( int index ) const = 0;
	virtual void				ClearAnimsInUse( void ) = 0;
	virtual void				FlushUnusedAnims( void ) = 0;
	virtual void				SetForceExport( bool force ) = 0;
	virtual bool					GetForceExport( void ) const = 0;
	virtual void				SetNotify( idAnimNotify *notify ) = 0;
	virtual idAnimNotify *		GetNotify( void ) const = 0;
	virtual void				ClearFrameCommands( void ) = 0;
	virtual void				RegisterFrameCommand( const char *name, int commandType ) = 0;
	virtual int					FindFrameCommand( const char *name ) const = 0;
	virtual void				ShutdownModelExporter( void ) = 0;
	virtual int					ExportDefFile( const char *filename ) = 0;
	virtual int					ExportModels( const char *pathname, const char *extension ) = 0;
};

extern idAnimManager *			animationLib;

class idAnimatorHandle : public idAnimator {
public:
								idAnimatorHandle() : animator( NULL ), owner( NULL ) {}
	virtual					~idAnimatorHandle() { Destroy(); }
	void						Init( void *newOwner ) { owner = newOwner; EnsureAnimator()->SetOwner( owner ); }
	virtual void				Destroy( void ) { if ( animator ) { animator->Destroy(); animator = NULL; } }
	virtual size_t				Allocated( void ) const { return EnsureAnimator()->Allocated(); }
	virtual size_t				Size( void ) const { return EnsureAnimator()->Size(); }
	virtual void				Save( idAnimSaveGame *file ) const { EnsureAnimator()->Save( file ); }
	virtual void				Restore( idAnimRestoreGame *file ) { EnsureAnimator()->Restore( file ); }
	virtual void				SetOwner( void *newOwner ) { owner = newOwner; EnsureAnimator()->SetOwner( owner ); }
	virtual void *				GetOwner( void ) const { return EnsureAnimator()->GetOwner(); }
	virtual void				RemoveOriginOffset( bool value ) { EnsureAnimator()->RemoveOriginOffset( value ); }
	virtual bool				RemoveOrigin( void ) const { return EnsureAnimator()->RemoveOrigin(); }
	virtual int				GetJointListBuffer( const char *names, jointHandle_t *list, int maxJoints ) const { return EnsureAnimator()->GetJointListBuffer( names, list, maxJoints ); }
	virtual int					NumAnims( void ) const { return EnsureAnimator()->NumAnims(); }
	virtual const idAnim *		GetAnim( int index ) const { return EnsureAnimator()->GetAnim( index ); }
	virtual int					GetAnim( const char *name ) const { return EnsureAnimator()->GetAnim( name ); }
	virtual bool					HasAnim( const char *name ) const { return EnsureAnimator()->HasAnim( name ); }
	virtual void				ServiceAnims( int from, int to ) { EnsureAnimator()->ServiceAnims( from, to ); }
	virtual bool					IsAnimating( int time ) const { return EnsureAnimator()->IsAnimating( time ); }
	virtual void				GetJoints( int *count, idJointMat **joints ) { EnsureAnimator()->GetJoints( count, joints ); }
	virtual int					NumJoints( void ) const { return EnsureAnimator()->NumJoints(); }
	virtual jointHandle_t		GetFirstChild( jointHandle_t joint ) const { return EnsureAnimator()->GetFirstChild( joint ); }
	virtual jointHandle_t		GetFirstChild( const char *name ) const { return EnsureAnimator()->GetFirstChild( name ); }
	virtual idRenderModel *		SetModel( const char *name ) { return EnsureAnimator()->SetModel( name ); }
	virtual idRenderModel *		ModelHandle( void ) const { return EnsureAnimator()->ModelHandle(); }
	virtual const idDeclModelDef *ModelDef( void ) const { return EnsureAnimator()->ModelDef(); }
	virtual void				ForceUpdate( void ) { EnsureAnimator()->ForceUpdate(); }
	virtual void				ClearForceUpdate( void ) { EnsureAnimator()->ClearForceUpdate(); }
	virtual bool					CreateFrame( int time, bool force ) { return EnsureAnimator()->CreateFrame( time, force ); }
	virtual bool					FrameHasChanged( int time ) const { return EnsureAnimator()->FrameHasChanged( time ); }
	virtual void				GetDelta( int from, int to, idVec3 &delta ) const { EnsureAnimator()->GetDelta( from, to, delta ); }
	virtual bool					GetDeltaRotation( int from, int to, idMat3 &delta ) const { return EnsureAnimator()->GetDeltaRotation( from, to, delta ); }
	virtual void				GetOrigin( int time, idVec3 &pos ) const { EnsureAnimator()->GetOrigin( time, pos ); }
	virtual bool					GetBounds( int time, idBounds &bounds ) { return EnsureAnimator()->GetBounds( time, bounds ); }
	virtual idAnimBlend *		CurrentAnim( int channel ) { return EnsureAnimator()->CurrentAnim( channel ); }
	virtual void				Clear( int channel, int time, int clearTime ) { EnsureAnimator()->Clear( channel, time, clearTime ); }
	virtual void				SetFrame( int channel, int anim, int frame, int time, int blend ) { EnsureAnimator()->SetFrame( channel, anim, frame, time, blend ); }
	virtual void				CycleAnim( int channel, int anim, int time, int blend ) { EnsureAnimator()->CycleAnim( channel, anim, time, blend ); }
	virtual void				PlayAnim( int channel, int anim, int time, int blend ) { EnsureAnimator()->PlayAnim( channel, anim, time, blend ); }
	virtual void				SyncAnimChannels( int channel, int from, int time, int blend ) { EnsureAnimator()->SyncAnimChannels( channel, from, time, blend ); }
	virtual void				SetJointPos( jointHandle_t joint, jointModTransform_t type, const idVec3 &pos ) { EnsureAnimator()->SetJointPos( joint, type, pos ); }
	virtual void				SetJointAxis( jointHandle_t joint, jointModTransform_t type, const idMat3 &axis ) { EnsureAnimator()->SetJointAxis( joint, type, axis ); }
	virtual void				ClearJoint( jointHandle_t joint ) { EnsureAnimator()->ClearJoint( joint ); }
	virtual void				ClearAllJoints( void ) { EnsureAnimator()->ClearAllJoints(); }
	virtual void				InitAFPose( void ) { EnsureAnimator()->InitAFPose(); }
	virtual void				SetAFPoseJointMod( jointHandle_t joint, AFJointModType_t mod, const idMat3 &axis, const idVec3 &origin ) { EnsureAnimator()->SetAFPoseJointMod( joint, mod, axis, origin ); }
	virtual void				FinishAFPose( int anim, const idBounds &bounds, int time ) { EnsureAnimator()->FinishAFPose( anim, bounds, time ); }
	virtual void				SetAFPoseBlendWeight( float weight ) { EnsureAnimator()->SetAFPoseBlendWeight( weight ); }
	virtual bool					BlendAFPose( idJointQuat *frame ) const { return EnsureAnimator()->BlendAFPose( frame ); }
	virtual void				ClearAFPose( void ) { EnsureAnimator()->ClearAFPose(); }
	virtual void				ClearAllAnims( int time, int clearTime ) { EnsureAnimator()->ClearAllAnims( time, clearTime ); }
	virtual jointHandle_t		GetJointHandle( const char *name ) const { return EnsureAnimator()->GetJointHandle( name ); }
	virtual const char *			GetJointName( jointHandle_t handle ) const { return EnsureAnimator()->GetJointName( handle ); }
	virtual int					GetChannelForJoint( jointHandle_t joint ) const { return EnsureAnimator()->GetChannelForJoint( joint ); }
	virtual bool					GetJointTransform( jointHandle_t joint, int time, idVec3 &offset, idMat3 &axis ) { return EnsureAnimator()->GetJointTransform( joint, time, offset, axis ); }
	virtual bool					GetJointLocalTransform( jointHandle_t joint, int time, idVec3 &offset, idMat3 &axis ) { return EnsureAnimator()->GetJointLocalTransform( joint, time, offset, axis ); }
	virtual const animFlags_t	GetAnimFlags( int anim ) const { return EnsureAnimator()->GetAnimFlags( anim ); }
	virtual int					NumFrames( int anim ) const { return EnsureAnimator()->NumFrames( anim ); }
	virtual int					NumSyncedAnims( int anim ) const { return EnsureAnimator()->NumSyncedAnims( anim ); }
	virtual const char *			AnimName( int anim ) const { return EnsureAnimator()->AnimName( anim ); }
	virtual const char *			AnimFullName( int anim ) const { return EnsureAnimator()->AnimFullName( anim ); }
	virtual int					AnimLength( int anim ) const { return EnsureAnimator()->AnimLength( anim ); }
	virtual const idVec3 &		TotalMovementDelta( int anim ) const { return EnsureAnimator()->TotalMovementDelta( anim ); }

private:
	idAnimator *					EnsureAnimator( void ) const {
		if ( !animator ) {
			assert( animationLib != NULL );
			animator = animationLib->AllocAnimator( owner );
		}
		return animator;
	}
	mutable idAnimator *		animator;
	void *						owner;
	idAnimatorHandle( const idAnimatorHandle & );
	idAnimatorHandle &operator=( const idAnimatorHandle & );
};

#endif /* !__ANIM_H__ */
