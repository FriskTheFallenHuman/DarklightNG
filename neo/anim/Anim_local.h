/*
===========================================================================

Darklight animation system private implementation declarations.

===========================================================================
*/
#ifndef __ANIM_LOCAL_H__
#define __ANIM_LOCAL_H__

#include "Anim.h"

class idModelExport {
private:
	void					Reset( void );
	bool					ParseOptions( idLexer &lex );
	int						ParseExportSection( idParser &parser );
	static bool				CheckMayaInstall( void );
	static void				LoadMayaDll( void );
	bool					ConvertMayaToMD5( void );
	static bool				initialized;

public:
	idStr					commandLine;
	idStr					src;
	idStr					dest;
	bool					force;

							idModelExport();
	static void				Shutdown( void );
	int						ExportDefFile( const char *filename );
	bool					ExportModel( const char *model );
	bool					ExportAnim( const char *anim );
	int						ExportModels( const char *pathname, const char *extension );
};

class idAnimatorLocal : public idAnimator {
public:
								idAnimatorLocal( void *owner );
	virtual					~idAnimatorLocal();
	virtual void				Destroy( void ) { delete this; }
	virtual size_t				Allocated( void ) const;
	virtual size_t				Size( void ) const;
	virtual void				Save( idAnimSaveGame *savefile ) const;
	virtual void				Restore( idAnimRestoreGame *savefile );
	virtual void				SetOwner( void *newOwner );
	virtual void *				GetOwner( void ) const;
	virtual void				RemoveOriginOffset( bool remove );
	virtual bool				RemoveOrigin( void ) const;
	virtual int				GetJointListBuffer( const char *jointnames, jointHandle_t *jointList, int maxJoints ) const;
	virtual int					NumAnims( void ) const;
	virtual const idAnim			*GetAnim( int index ) const;
	virtual int					GetAnim( const char *name ) const;
	virtual bool					HasAnim( const char *name ) const;
	virtual void				ServiceAnims( int fromtime, int totime );
	virtual bool					IsAnimating( int currentTime ) const;
	virtual void				GetJoints( int *numJoints, idJointMat **jointsPtr );
	virtual int					NumJoints( void ) const;
	virtual jointHandle_t		GetFirstChild( jointHandle_t jointnum ) const;
	virtual jointHandle_t		GetFirstChild( const char *name ) const;
	virtual idRenderModel			*SetModel( const char *modelname );
	virtual idRenderModel			*ModelHandle( void ) const;
	virtual const idDeclModelDef	*ModelDef( void ) const;
	virtual void				ForceUpdate( void );
	virtual void				ClearForceUpdate( void );
	virtual bool					CreateFrame( int animtime, bool force );
	virtual bool					FrameHasChanged( int animtime ) const;
	virtual void				GetDelta( int fromtime, int totime, idVec3 &delta ) const;
	virtual bool					GetDeltaRotation( int fromtime, int totime, idMat3 &delta ) const;
	virtual void				GetOrigin( int currentTime, idVec3 &pos ) const;
	virtual bool					GetBounds( int currentTime, idBounds &bounds );
	virtual idAnimBlend			*CurrentAnim( int channelNum );
	virtual void				Clear( int channelNum, int currentTime, int cleartime );
	virtual void				SetFrame( int channelNum, int animnum, int frame, int currenttime, int blendtime );
	virtual void				CycleAnim( int channelNum, int animnum, int currenttime, int blendtime );
	virtual void				PlayAnim( int channelNum, int animnum, int currenttime, int blendTime );
	virtual void				SyncAnimChannels( int channelNum, int fromChannelNum, int currenttime, int blendTime );
	virtual void				SetJointPos( jointHandle_t jointnum, jointModTransform_t transform_type, const idVec3 &pos );
	virtual void				SetJointAxis( jointHandle_t jointnum, jointModTransform_t transform_type, const idMat3 &mat );
	virtual void				ClearJoint( jointHandle_t jointnum );
	virtual void				ClearAllJoints( void );
	virtual void				InitAFPose( void );
	virtual void				SetAFPoseJointMod( jointHandle_t jointNum, AFJointModType_t mod, const idMat3 &axis, const idVec3 &origin );
	virtual void				FinishAFPose( int animnum, const idBounds &bounds, int time );
	virtual void				SetAFPoseBlendWeight( float blendWeight );
	virtual bool					BlendAFPose( idJointQuat *blendFrame ) const;
	virtual void				ClearAFPose( void );
	virtual void				ClearAllAnims( int currentTime, int cleartime );
	virtual jointHandle_t		GetJointHandle( const char *name ) const;
	virtual const char			*GetJointName( jointHandle_t handle ) const;
	virtual int					GetChannelForJoint( jointHandle_t joint ) const;
	virtual bool					GetJointTransform( jointHandle_t jointHandle, int currenttime, idVec3 &offset, idMat3 &axis );
	virtual bool					GetJointLocalTransform( jointHandle_t jointHandle, int currentTime, idVec3 &offset, idMat3 &axis );
	virtual const animFlags_t	GetAnimFlags( int animnum ) const;
	virtual int					NumFrames( int animnum ) const;
	virtual int					NumSyncedAnims( int animnum ) const;
	virtual const char			*AnimName( int animnum ) const;
	virtual const char			*AnimFullName( int animnum ) const;
	virtual int					AnimLength( int animnum ) const;
	virtual const idVec3			&TotalMovementDelta( int animnum ) const;

private:
	void						FreeData( void );
	void						PushAnims( int channel, int currentTime, int blendTime );

	const idDeclModelDef *		modelDef;
	void *						owner;
	idAnimBlend					channels[ ANIM_NumAnimChannels ][ ANIM_MaxAnimsPerChannel ];
	idList<jointMod_t *>		jointMods;
	int							numJoints;
	idJointMat *				joints;
	mutable int					lastTransformTime;
	mutable bool				stoppedAnimatingUpdate;
	bool						removeOriginOffset;
	bool						forceUpdate;
	idBounds					frameBounds;
	float						AFPoseBlendWeight;
	idList<int>					AFPoseJoints;
	idList<idAFPoseJointMod>	AFPoseJointMods;
	idList<idJointQuat>			AFPoseJointFrame;
	idBounds					AFPoseBounds;
	int							AFPoseTime;
};

class idAnimManagerLocal : public idAnimManager {
public:
								idAnimManagerLocal();
	virtual					~idAnimManagerLocal();
	virtual void				RegisterDeclTypes( void );
	virtual void				Shutdown( void );
	virtual idMD5Anim *			GetAnim( const char *name );
	virtual idAnimator *			AllocAnimator( void *owner );
	virtual void				ReloadAnims( void );
	virtual void				ListAnims( void ) const;
	virtual int					JointIndex( const char *name );
	virtual const char *			JointName( int index ) const;
	virtual void				ClearAnimsInUse( void );
	virtual void				FlushUnusedAnims( void );
	virtual void				SetForceExport( bool force ) { forceExport = force; }
	virtual bool					GetForceExport( void ) const { return forceExport; }
	virtual void				SetNotify( idAnimNotify *newNotify ) { notify = newNotify; }
	virtual idAnimNotify *		GetNotify( void ) const { return notify; }
	virtual void				ClearFrameCommands( void );
	virtual void				RegisterFrameCommand( const char *name, int commandType );
	virtual int					FindFrameCommand( const char *name ) const;
	virtual void				ShutdownModelExporter( void );
	virtual int					ExportDefFile( const char *filename );
	virtual int					ExportModels( const char *pathname, const char *extension );

private:
	idHashTable<idMD5Anim *>	animations;
	idStrList					jointnames;
	idHashIndex					jointnamesHash;
	idHashTable<int>			frameCommands;
	idAnimNotify *				notify;
	bool						forceExport;
};

#endif
