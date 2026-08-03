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
#ifndef GEKEYVALUEMODIFIER_H_
#define GEKEYVALUEMODIFIER_H_

#ifndef GEMODIFIER_H_
#include "GEModifier.h"
#endif

class rvGEKeyValueModifier : public rvGEModifier
{
public:

	rvGEKeyValueModifier ( const char* name, idWindow* window, const char* key, const char* value );
	
	virtual bool		Apply		( void );
	virtual bool		Undo		( void );

	virtual bool		CanMerge	( rvGEModifier* merge );
	virtual bool		Merge		( rvGEModifier* merge );
			
protected:
	
	idStr		mKey;
	idStr		mValue;
	idStr		mUndoValue;
}; 

ID_INLINE bool rvGEKeyValueModifier::CanMerge ( rvGEModifier* merge )
{
	return !((rvGEKeyValueModifier*)merge)->mKey.Icmp ( mKey );
}

#endif // GEKEYVALUEMODIFIER_H_