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

#ifndef __GAME_CAMERA_H__
#define __GAME_CAMERA_H__


/*
===============================================================================

Camera providing an alternative view of the level.

===============================================================================
*/

D3_CLASS( Abstract )
class idCamera : public idEntity {
public:
	ABSTRACT_PROTOTYPE( idCamera );

	void					Spawn( void );
	virtual void			GetViewParms( renderView_t *view ) = 0;
	virtual renderView_t *	GetRenderView();
	virtual void			Stop( void ){} ;
};

/*
===============================================================================

idCameraView

===============================================================================
*/

D3_CLASS()
class idCameraView : public idCamera {
public:
	CLASS_PROTOTYPE( idCameraView );
							idCameraView();

	// save games
	void					Save( idSaveGame *savefile ) const;				// archives object for save game file
	void					Restore( idRestoreGame *savefile );				// unarchives object from save game file

	void					Spawn( );
	virtual void			GetViewParms( renderView_t *view );
	virtual void			Stop( void );

protected:
	D3_EVENT( EV_Activate )
	void					Event_Activate( idEntity *activator );
	D3_EVENT( EV_Camera_SetAttachments, "<getattachments>", void )
	void					Event_SetAttachments();
	void					SetAttachment( idEntity **e, const char *p );
	float					fov;
	idEntity				*attachedTo;
	idEntity				*attachedView;
};



/*
===============================================================================

A camera which follows a path defined by an animation.

===============================================================================
*/

typedef struct {
	idCQuat				q;
	idVec3				t;
	float				fov;
} cameraFrame_t;

D3_CLASS()
class idCameraAnim : public idCamera {
public:
	CLASS_PROTOTYPE( idCameraAnim );

							idCameraAnim();
							~idCameraAnim();

	// save games
	void					Save( idSaveGame *savefile ) const;				// archives object for save game file
	void					Restore( idRestoreGame *savefile );				// unarchives object from save game file

	void					Spawn( void );
	virtual void			GetViewParms( renderView_t *view );

private:
	int						threadNum;
	idVec3					offset;
	int						frameRate;
	int						starttime;
	int						cycle;
	idList<int>				cameraCuts;
	idList<cameraFrame_t>	camera;
	idEntityPtr<idEntity>	activator;

	void					Start( void );
	void					Stop( void );
	void					Think( void );

	void					LoadAnim( void );
	D3_EVENT( EV_Camera_Start, "start", void )
	void					Event_Start( void );
	D3_EVENT( EV_Camera_Stop, "stop", void )
	void					Event_Stop( void );
	D3_EVENT( EV_Thread_SetCallback, "<script_setcallback>", void )
	void					Event_SetCallback( void );
	D3_EVENT( EV_Activate )
	void					Event_Activate( idEntity *activator );
};

#endif /* !__GAME_CAMERA_H__ */
