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

#ifndef __TESTMODEL_H__
#define __TESTMODEL_H__

/*
==============================================================================================

	idTestModel

==============================================================================================
*/

D3_CLASS()
class idTestModel : public idAnimatedEntity {
public:
	CLASS_PROTOTYPE( idTestModel );

							idTestModel();
							~idTestModel();

	void					Save( idSaveGame *savefile );
	void					Restore( idRestoreGame *savefile );

	void					Spawn( void );

	virtual bool			ShouldConstructScriptObjectAtSpawn( void ) const;

	void					NextAnim( const idCmdArgs &args );
	void					PrevAnim( const idCmdArgs &args );
	void					NextFrame( const idCmdArgs &args );
	void					PrevFrame( const idCmdArgs &args );
	void					TestAnim( const idCmdArgs &args );
	void					BlendAnim( const idCmdArgs &args );

	static void 			KeepTestModel_f( const idCmdArgs &args );
	static void 			TestModel_f( const idCmdArgs &args );
	static void				ArgCompletion_TestModel( const idCmdArgs &args, void(*callback)( const char *s ) );
	static void 			TestSkin_f( const idCmdArgs &args );
	static void 			TestShaderParm_f( const idCmdArgs &args );
	static void 			TestParticleStopTime_f( const idCmdArgs &args );
	static void 			TestAnim_f( const idCmdArgs &args );
	static void				ArgCompletion_TestAnim( const idCmdArgs &args, void(*callback)( const char *s ) );
	static void 			TestBlend_f( const idCmdArgs &args );
	static void 			TestModelNextAnim_f( const idCmdArgs &args );
	static void 			TestModelPrevAnim_f( const idCmdArgs &args );
	static void 			TestModelNextFrame_f( const idCmdArgs &args );
	static void 			TestModelPrevFrame_f( const idCmdArgs &args );

private:
	idEntityPtr<idEntity>	head;
	idAnimator				*headAnimator;
	idPhysics_Parametric	physicsObj;
	idStr					animname;
	int						anim;
	int						headAnim;
	int						mode;
	int						frame;
	int						starttime;
	int						animtime;

	idList<copyJoints_t>	copyJoints;

	virtual void			Think( void );

	D3_EVENT( EV_FootstepLeft )
	D3_EVENT( EV_FootstepRight )
	void					Event_Footstep( void );
};

#endif /* !__TESTMODEL_H__ */
