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
#ifndef GEMODIFIER_H_
#define GEMODIFIER_H_

class idWindow;
class rvGEWindowWrapper;

class rvGEModifier
{
public:

	rvGEModifier ( const char* name, idWindow* window );
	virtual ~rvGEModifier ( ) { }

	virtual bool		Apply		( void ) = 0;
	virtual bool		Undo		( void ) = 0;	
	virtual const char*	GetName		( void );
	virtual bool		CanMerge	( rvGEModifier* merge );
	
	virtual bool		IsValid		( void );
	
	virtual bool		Merge		( rvGEModifier* merge );
	
	idWindow*			GetWindow	( void );
	
	
protected:
	
	idWindow*			mWindow;
	rvGEWindowWrapper*	mWrapper;
	idStr				mName;
};

ID_INLINE bool rvGEModifier::IsValid ( void )
{
	return true;
}

ID_INLINE idWindow* rvGEModifier::GetWindow ( void )
{
	return mWindow;
}

ID_INLINE const char* rvGEModifier::GetName ( void )
{
	return mName;
}

ID_INLINE bool rvGEModifier::CanMerge ( rvGEModifier* merge )
{
	return false;
}

ID_INLINE bool rvGEModifier::Merge ( rvGEModifier* merge )
{
	return false;
}

#endif // GEMODIFIER_H_
