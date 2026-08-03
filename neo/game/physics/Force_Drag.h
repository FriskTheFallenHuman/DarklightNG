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

#ifndef __FORCE_DRAG_H__
#define __FORCE_DRAG_H__

/*
===============================================================================

	Drag force

===============================================================================
*/

D3_CLASS()
class idForce_Drag : public idForce {

public:
	CLASS_PROTOTYPE( idForce_Drag );

						idForce_Drag( void );
	virtual				~idForce_Drag( void );
						// initialize the drag force
	void				Init( float damping );
						// set physics object being dragged
	void				SetPhysics( idPhysics *physics, int id, const idVec3 &p );
						// set position to drag towards
	void				SetDragPosition( const idVec3 &pos );
						// get the position dragged towards
	const idVec3 &		GetDragPosition( void ) const;
						// get the position on the dragged physics object
	const idVec3		GetDraggedPosition( void ) const;

public: // common force interface
	virtual void		Evaluate( int time );
	virtual void		RemovePhysics( const idPhysics *phys );

private:

	// properties
	float				damping;

	// positioning
	idPhysics *			physics;		// physics object
	int					id;				// clip model id of physics object
	idVec3				p;				// position on clip model
	idVec3				dragPosition;	// drag towards this position
};

#endif /* !__FORCE_DRAG_H__ */
