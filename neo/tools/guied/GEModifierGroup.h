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
#ifndef GEMODIFIERGROUP_H_
#define GEMODIFIERGROUP_H_

#ifndef GEMODIFIER_H_
#include "GEModifier.h"
#endif

class rvGEModifierGroup : public rvGEModifier
{
public:

	rvGEModifierGroup ( );
	~rvGEModifierGroup ( );

	virtual bool		Apply		( void );
	virtual bool		Undo		( void );

	virtual bool		CanMerge	( rvGEModifier* merge );
		
	virtual bool		Merge		( rvGEModifier* merge );
	
	virtual bool		IsValid		( void );

	bool				Append		( rvGEModifier* mod );	
	int					GetCount	( void );
	
	
protected:

	idList<rvGEModifier*>	mModifiers;
		
};

ID_INLINE int rvGEModifierGroup::GetCount( void )
{
	return mModifiers.Num ( );
}

#endif