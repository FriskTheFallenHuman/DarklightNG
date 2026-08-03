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

#ifndef __RENDERLIGHT_H__
#define __RENDERLIGHT_H__

class idRenderModel;
class idMaterial;
class idSoundEmitter;
class idImage;
class idRenderWorldLocal;
class idRenderLight;
class idRenderLightLocal;
struct areaReference_s;
struct viewLight_s;
struct doublePortal_s;

class idRenderLight {
public:
						idRenderLight( const idRenderLight & ) = delete;
	idRenderLight &		operator=( const idRenderLight & ) = delete;

	virtual void			FreeRenderLight() = 0;
	virtual void			RemoveFromRenderWorld() = 0;
	virtual void			UpdateRenderLight( bool force = false ) = 0;
	virtual void			ForceUpdate() = 0;
	virtual bool			IsInRenderWorld() const = 0;
	virtual int				GetIndex() const = 0;

	void					Reset() {
		axis.Zero();
		origin.Zero();
		suppressLightInViewID = 0;
		allowLightInViewID = 0;
		noShadows = false;
		noSpecular = false;
		pointLight = false;
		parallel = false;
		bakedLight = false;
		lightRadius.Zero();
		lightCenter.Zero();
		target.Zero();
		right.Zero();
		up.Zero();
		start.Zero();
		end.Zero();
		prelightModel = NULL;
		lightId = 0;
		shader = NULL;
		memset( shaderParms, 0, sizeof( shaderParms ) );
		referenceSound = NULL;
		dirtyFlags = DIRTY_ALL;
	}

	void					CopyFrom( const idRenderLight &other ) {
		SetAxis( other.GetAxis() );
		SetOrigin( other.GetOrigin() );
		SetSuppressLightInViewID( other.GetSuppressLightInViewID() );
		SetAllowLightInViewID( other.GetAllowLightInViewID() );
		SetNoShadows( other.GetNoShadows() );
		SetNoSpecular( other.GetNoSpecular() );
		SetPointLight( other.GetPointLight() );
		SetParallel( other.GetParallel() );
		SetBakedLight( other.GetBakedLight() );
		SetLightRadius( other.GetLightRadius() );
		SetLightCenter( other.GetLightCenter() );
		SetTarget( other.GetTarget() );
		SetRight( other.GetRight() );
		SetUp( other.GetUp() );
		SetStart( other.GetStart() );
		SetEnd( other.GetEnd() );
		SetPrelightModel( other.GetPrelightModel() );
		SetLightId( other.GetLightId() );
		SetShader( other.GetShader() );
		for ( int i = 0; i < MAX_ENTITY_SHADER_PARMS; i++ ) {
			SetShaderParm( i, other.GetShaderParm( i ) );
		}
		SetReferenceSound( other.GetReferenceSound() );
	}

	const idMat3 &			GetAxis() const { return axis; }
	void					SetAxis( const idMat3 &value ) { SetValue( axis, value, DIRTY_SHAPE ); }
	const idVec3 &			GetOrigin() const { return origin; }
	void					SetOrigin( const idVec3 &value ) { SetValue( origin, value, DIRTY_SHAPE ); }
	int						GetSuppressLightInViewID() const { return suppressLightInViewID; }
	void					SetSuppressLightInViewID( int value ) { SetValue( suppressLightInViewID, value, DIRTY_PARAMS ); }
	int						GetAllowLightInViewID() const { return allowLightInViewID; }
	void					SetAllowLightInViewID( int value ) { SetValue( allowLightInViewID, value, DIRTY_PARAMS ); }
	bool					GetNoShadows() const { return noShadows; }
	void					SetNoShadows( bool value ) { SetValue( noShadows, value, DIRTY_SHAPE ); }
	bool					GetNoSpecular() const { return noSpecular; }
	void					SetNoSpecular( bool value ) { SetValue( noSpecular, value, DIRTY_PARAMS ); }
	bool					GetPointLight() const { return pointLight; }
	void					SetPointLight( bool value ) { SetValue( pointLight, value, DIRTY_SHAPE ); }
	bool					GetParallel() const { return parallel; }
	void					SetParallel( bool value ) { SetValue( parallel, value, DIRTY_SHAPE ); }
	bool					GetBakedLight() const { return bakedLight; }
	void					SetBakedLight( bool value ) { SetValue( bakedLight, value, DIRTY_SHAPE ); }
	const idVec3 &			GetLightRadius() const { return lightRadius; }
	void					SetLightRadius( const idVec3 &value ) { SetValue( lightRadius, value, DIRTY_SHAPE ); }
	const idVec3 &			GetLightCenter() const { return lightCenter; }
	void					SetLightCenter( const idVec3 &value ) { SetValue( lightCenter, value, DIRTY_SHAPE ); }
	const idVec3 &			GetTarget() const { return target; }
	void					SetTarget( const idVec3 &value ) { SetValue( target, value, DIRTY_SHAPE ); }
	const idVec3 &			GetRight() const { return right; }
	void					SetRight( const idVec3 &value ) { SetValue( right, value, DIRTY_SHAPE ); }
	const idVec3 &			GetUp() const { return up; }
	void					SetUp( const idVec3 &value ) { SetValue( up, value, DIRTY_SHAPE ); }
	const idVec3 &			GetStart() const { return start; }
	void					SetStart( const idVec3 &value ) { SetValue( start, value, DIRTY_SHAPE ); }
	const idVec3 &			GetEnd() const { return end; }
	void					SetEnd( const idVec3 &value ) { SetValue( end, value, DIRTY_SHAPE ); }
	idRenderModel *			GetPrelightModel() const { return prelightModel; }
	void					SetPrelightModel( idRenderModel *value ) { SetValue( prelightModel, value, DIRTY_SHAPE ); }
	int						GetLightId() const { return lightId; }
	void					SetLightId( int value ) { SetValue( lightId, value, DIRTY_PARAMS ); }
	const idMaterial *		GetShader() const { return shader; }
	void					SetShader( const idMaterial *value ) { SetValue( shader, value, DIRTY_SHAPE ); }
	float					GetShaderParm( int index ) const { assert( index >= 0 && index < MAX_ENTITY_SHADER_PARMS ); return shaderParms[index]; }
	void					SetShaderParm( int index, float value ) { assert( index >= 0 && index < MAX_ENTITY_SHADER_PARMS ); SetValue( shaderParms[index], value, DIRTY_PARAMS ); }
	const float *			GetShaderParms() const { return shaderParms; }
	idSoundEmitter *		GetReferenceSound() const { return referenceSound; }
	void					SetReferenceSound( idSoundEmitter *value ) { SetValue( referenceSound, value, DIRTY_PARAMS ); }

protected:
	virtual				~idRenderLight() {}

	enum dirtyFlag_t {
		DIRTY_NONE = 0,
		DIRTY_SHAPE = BIT( 0 ),
		DIRTY_PARAMS = BIT( 1 ),
		DIRTY_ALL = DIRTY_SHAPE | DIRTY_PARAMS
	};

	template< class T > void SetValue( T &destination, const T &value, unsigned int dirtyBit ) {
		if ( destination != value ) {
			destination = value;
			dirtyFlags |= dirtyBit;
		}
	}

	idMat3					axis;
	idVec3					origin;
	int						suppressLightInViewID;
	int						allowLightInViewID;
	bool					noShadows;
	bool					noSpecular;
	bool					pointLight;
	bool					parallel;
	bool					bakedLight;
	idVec3					lightRadius;
	idVec3					lightCenter;
	idVec3					target;
	idVec3					right;
	idVec3					up;
	idVec3					start;
	idVec3					end;
	idRenderModel *			prelightModel;
	int						lightId;
	const idMaterial *		shader;
	float					shaderParms[MAX_ENTITY_SHADER_PARMS];
	idSoundEmitter *		referenceSound;
	unsigned int			dirtyFlags;

private:
	friend class idRenderLightLocal;
						idRenderLight() { Reset(); }
};

#endif
