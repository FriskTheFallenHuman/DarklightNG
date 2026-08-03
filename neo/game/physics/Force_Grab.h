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

#ifndef __FORCE_GRAB_H__
#define __FORCE_GRAB_H__


/*
===============================================================================

	Drag force

===============================================================================
*/

D3_CLASS()
class idForce_Grab : public idForce {

public:
	CLASS_PROTOTYPE( idForce_Grab );

	void				Save( idSaveGame *savefile ) const;
	void				Restore( idRestoreGame *savefile );

						idForce_Grab( void );
	virtual				~idForce_Grab( void );
						// initialize the drag force
	void				Init( float damping );
						// set physics object being dragged
	void				SetPhysics( idPhysics *physics, int id, const idVec3 &goal );
						// update the goal position
	void				SetGoalPosition( const idVec3 &goal );


public: // common force interface
	virtual void		Evaluate( int time );
	virtual void		RemovePhysics( const idPhysics *phys );

	// Get the distance from object to goal position
	float				GetDistanceToGoal( void );

private:

	// properties
	float				damping;
	idVec3				goalPosition;

	float				distanceToGoal;

	// positioning
	idPhysics *			physics;		// physics object
	int					id;				// clip model id of physics object
};

#endif /* !__FORCE_GRAB_H__ */
