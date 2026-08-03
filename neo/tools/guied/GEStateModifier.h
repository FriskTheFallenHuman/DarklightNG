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
#ifndef GESTATEMODIFIER_H_
#define GESTATEMODIFIER_H_

#ifndef GEMODIFIER_H_
#include "GEModifier.h"
#endif

class rvGEStateModifier : public rvGEModifier
{
public:

	rvGEStateModifier ( const char* name, idWindow* window, idDict& dict );
	
	virtual bool		Apply	( void );
	virtual bool		Undo	( void );
			
protected:

	bool	SetState	( idDict& dict );

	rvGEWindowWrapper::EWindowType	mWindowType;
	rvGEWindowWrapper::EWindowType	mUndoWindowType;
	idDict							mDict;
	idDict							mUndoDict;
}; 

#endif // GESTATEMODIFIER_H_