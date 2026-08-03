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

#ifndef __RENDERENTITY_H__
#define __RENDERENTITY_H__

class idRenderModel;
class idMaterial;
class idDeclSkin;
class idSoundEmitter;
class idUserInterface;
class idRenderWorldLocal;
class idRenderModelDecal;
class idRenderModelOverlay;
class idRenderEntity;
class idRenderEntityLocal;
struct renderView_s;
struct areaReference_s;
struct viewEntity_s;

const int MAX_RENDERENTITY_GUI = 3;

typedef bool ( *deferredEntityCallback_t )( idRenderEntity *, const renderView_s * );

class idRenderEntity {
public:
						idRenderEntity( const idRenderEntity & ) = delete;
	idRenderEntity &		operator=( const idRenderEntity & ) = delete;

	virtual void			FreeRenderEntity() = 0;
	virtual void			RemoveFromRenderWorld() = 0;
	virtual void			UpdateRenderEntity( bool force = false ) = 0;
	virtual void			ForceUpdate() = 0;
	virtual bool			IsInRenderWorld() const = 0;
	virtual int				GetIndex() const = 0;
	virtual void			ProjectOverlay( const idPlane localTextureAxis[2], const idMaterial *material ) = 0;
	virtual void			RemoveDecals() = 0;

	void					Reset() {
		hModel = NULL;
		entityNum = 0;
		bodyId = 0;
		bounds.Zero();
		callback = NULL;
		callbackData = NULL;
		suppressSurfaceInViewID = 0;
		suppressShadowInViewID = 0;
		suppressShadowInLightID = 0;
		allowSurfaceInViewID = 0;
		origin.Zero();
		axis.Zero();
		customShader = NULL;
		referenceShader = NULL;
		customSkin = NULL;
		referenceSound = NULL;
		memset( shaderParms, 0, sizeof( shaderParms ) );
		memset( gui, 0, sizeof( gui ) );
		remoteRenderView = NULL;
		numJoints = 0;
		joints = NULL;
		modelDepthHack = 0.0f;
		noSelfShadow = false;
		noShadow = false;
		noDynamicInteractions = false;
		weaponDepthHack = false;
		forceUpdate = 0;
		timeGroup = 0;
		xrayIndex = 0;
		dirtyFlags = DIRTY_ALL;
	}

	void					CopyFrom( const idRenderEntity &other ) {
		SetModel( other.GetModel() );
		SetEntityNum( other.GetEntityNum() );
		SetBodyId( other.GetBodyId() );
		SetBounds( other.GetBounds() );
		SetCallback( other.GetCallback() );
		SetCallbackData( other.GetCallbackData() );
		SetSuppressSurfaceInViewID( other.GetSuppressSurfaceInViewID() );
		SetSuppressShadowInViewID( other.GetSuppressShadowInViewID() );
		SetSuppressShadowInLightID( other.GetSuppressShadowInLightID() );
		SetAllowSurfaceInViewID( other.GetAllowSurfaceInViewID() );
		SetOrigin( other.GetOrigin() );
		SetAxis( other.GetAxis() );
		SetCustomShader( other.GetCustomShader() );
		SetReferenceShader( other.GetReferenceShader() );
		SetCustomSkin( other.GetCustomSkin() );
		SetReferenceSound( other.GetReferenceSound() );
		for ( int i = 0; i < MAX_ENTITY_SHADER_PARMS; i++ ) {
			SetShaderParm( i, other.GetShaderParm( i ) );
		}
		for ( int i = 0; i < MAX_RENDERENTITY_GUI; i++ ) {
			SetGui( i, other.GetGui( i ) );
		}
		SetRemoteRenderView( other.GetRemoteRenderView() );
		SetJoints( other.GetNumJoints(), other.GetJoints() );
		SetModelDepthHack( other.GetModelDepthHack() );
		SetNoSelfShadow( other.GetNoSelfShadow() );
		SetNoShadow( other.GetNoShadow() );
		SetNoDynamicInteractions( other.GetNoDynamicInteractions() );
		SetWeaponDepthHack( other.GetWeaponDepthHack() );
		SetForceUpdate( other.GetForceUpdate() );
		SetTimeGroup( other.GetTimeGroup() );
		SetXrayIndex( other.GetXrayIndex() );
	}

	idRenderModel *			GetModel() const { return hModel; }
	void					SetModel( idRenderModel *value ) { SetValue( hModel, value, DIRTY_MODEL ); }
	int						GetEntityNum() const { return entityNum; }
	void					SetEntityNum( int value ) { SetValue( entityNum, value, DIRTY_PARAMS ); }
	int						GetBodyId() const { return bodyId; }
	void					SetBodyId( int value ) { SetValue( bodyId, value, DIRTY_PARAMS ); }
	const idBounds &		GetBounds() const { return bounds; }
	void					SetBounds( const idBounds &value ) { SetValue( bounds, value, DIRTY_BOUNDS ); }
	deferredEntityCallback_t GetCallback() const { return callback; }
	void					SetCallback( deferredEntityCallback_t value ) { SetValue( callback, value, DIRTY_CALLBACK ); }
	void *					GetCallbackData() const { return callbackData; }
	void					SetCallbackData( void *value ) { SetValue( callbackData, value, DIRTY_CALLBACK ); }
	int						GetSuppressSurfaceInViewID() const { return suppressSurfaceInViewID; }
	void					SetSuppressSurfaceInViewID( int value ) { SetValue( suppressSurfaceInViewID, value, DIRTY_PARAMS ); }
	int						GetSuppressShadowInViewID() const { return suppressShadowInViewID; }
	void					SetSuppressShadowInViewID( int value ) { SetValue( suppressShadowInViewID, value, DIRTY_PARAMS ); }
	int						GetSuppressShadowInLightID() const { return suppressShadowInLightID; }
	void					SetSuppressShadowInLightID( int value ) { SetValue( suppressShadowInLightID, value, DIRTY_PARAMS ); }
	int						GetAllowSurfaceInViewID() const { return allowSurfaceInViewID; }
	void					SetAllowSurfaceInViewID( int value ) { SetValue( allowSurfaceInViewID, value, DIRTY_PARAMS ); }
	const idVec3 &			GetOrigin() const { return origin; }
	void					SetOrigin( const idVec3 &value ) { SetValue( origin, value, DIRTY_TRANSFORM ); }
	const idMat3 &			GetAxis() const { return axis; }
	void					SetAxis( const idMat3 &value ) { SetValue( axis, value, DIRTY_TRANSFORM ); }
	const idMaterial *		GetCustomShader() const { return customShader; }
	void					SetCustomShader( const idMaterial *value ) { SetValue( customShader, value, DIRTY_PARAMS ); }
	const idMaterial *		GetReferenceShader() const { return referenceShader; }
	void					SetReferenceShader( const idMaterial *value ) { SetValue( referenceShader, value, DIRTY_PARAMS ); }
	const idDeclSkin *		GetCustomSkin() const { return customSkin; }
	void					SetCustomSkin( const idDeclSkin *value ) { SetValue( customSkin, value, DIRTY_PARAMS ); }
	idSoundEmitter *		GetReferenceSound() const { return referenceSound; }
	void					SetReferenceSound( idSoundEmitter *value ) { SetValue( referenceSound, value, DIRTY_PARAMS ); }
	float					GetShaderParm( int index ) const { assert( index >= 0 && index < MAX_ENTITY_SHADER_PARMS ); return shaderParms[index]; }
	void					SetShaderParm( int index, float value ) { assert( index >= 0 && index < MAX_ENTITY_SHADER_PARMS ); SetValue( shaderParms[index], value, DIRTY_PARAMS ); }
	const float *			GetShaderParms() const { return shaderParms; }
	idUserInterface *		GetGui( int index ) const { assert( index >= 0 && index < MAX_RENDERENTITY_GUI ); return gui[index]; }
	void					SetGui( int index, idUserInterface *value ) { assert( index >= 0 && index < MAX_RENDERENTITY_GUI ); SetValue( gui[index], value, DIRTY_PARAMS ); }
	renderView_s *			GetRemoteRenderView() const { return remoteRenderView; }
	void					SetRemoteRenderView( renderView_s *value ) { SetValue( remoteRenderView, value, DIRTY_PARAMS ); }
	int						GetNumJoints() const { return numJoints; }
	idJointMat *			GetJoints() const { return joints; }
	void					SetJoints( int count, idJointMat *value ) { if ( numJoints != count || joints != value ) { numJoints = count; joints = value; dirtyFlags |= DIRTY_PARAMS; } }
	float					GetModelDepthHack() const { return modelDepthHack; }
	void					SetModelDepthHack( float value ) { SetValue( modelDepthHack, value, DIRTY_PARAMS ); }
	bool					GetNoSelfShadow() const { return noSelfShadow; }
	void					SetNoSelfShadow( bool value ) { SetValue( noSelfShadow, value, DIRTY_PARAMS ); }
	bool					GetNoShadow() const { return noShadow; }
	void					SetNoShadow( bool value ) { SetValue( noShadow, value, DIRTY_PARAMS ); }
	bool					GetNoDynamicInteractions() const { return noDynamicInteractions; }
	void					SetNoDynamicInteractions( bool value ) { SetValue( noDynamicInteractions, value, DIRTY_PARAMS ); }
	bool					GetWeaponDepthHack() const { return weaponDepthHack; }
	void					SetWeaponDepthHack( bool value ) { SetValue( weaponDepthHack, value, DIRTY_PARAMS ); }
	int						GetForceUpdate() const { return forceUpdate; }
	void					SetForceUpdate( int value ) { SetValue( forceUpdate, value, DIRTY_PARAMS ); }
	int						GetTimeGroup() const { return timeGroup; }
	void					SetTimeGroup( int value ) { SetValue( timeGroup, value, DIRTY_PARAMS ); }
	int						GetXrayIndex() const { return xrayIndex; }
	void					SetXrayIndex( int value ) { SetValue( xrayIndex, value, DIRTY_PARAMS ); }

protected:
	virtual				~idRenderEntity() {}

	enum dirtyFlag_t {
		DIRTY_NONE = 0,
		DIRTY_MODEL = BIT( 0 ),
		DIRTY_TRANSFORM = BIT( 1 ),
		DIRTY_BOUNDS = BIT( 2 ),
		DIRTY_CALLBACK = BIT( 3 ),
		DIRTY_PARAMS = BIT( 4 ),
		DIRTY_ALL = DIRTY_MODEL | DIRTY_TRANSFORM | DIRTY_BOUNDS | DIRTY_CALLBACK | DIRTY_PARAMS
	};

	template< class T > void SetValue( T &destination, const T &value, unsigned int dirtyBit ) {
		if ( destination != value ) {
			destination = value;
			dirtyFlags |= dirtyBit;
		}
	}

	idRenderModel *			hModel;
	int						entityNum;
	int						bodyId;
	idBounds				bounds;
	deferredEntityCallback_t callback;
	void *					callbackData;
	int						suppressSurfaceInViewID;
	int						suppressShadowInViewID;
	int						suppressShadowInLightID;
	int						allowSurfaceInViewID;
	idVec3					origin;
	idMat3					axis;
	const idMaterial *		customShader;
	const idMaterial *		referenceShader;
	const idDeclSkin *		customSkin;
	idSoundEmitter *		referenceSound;
	float					shaderParms[MAX_ENTITY_SHADER_PARMS];
	idUserInterface *		gui[MAX_RENDERENTITY_GUI];
	renderView_s *			remoteRenderView;
	int						numJoints;
	idJointMat *			joints;
	float					modelDepthHack;
	bool					noSelfShadow;
	bool					noShadow;
	bool					noDynamicInteractions;
	bool					weaponDepthHack;
	int						forceUpdate;
	int						timeGroup;
	int						xrayIndex;
	unsigned int			dirtyFlags;

private:
	friend class idRenderEntityLocal;
						idRenderEntity() { Reset(); }
};

#endif
