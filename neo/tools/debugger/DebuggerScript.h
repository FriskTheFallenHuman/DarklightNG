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
#ifndef DEBUGGERSCRIPT_H_
#define DEBUGGERSCRIPT_H_

class idProgram;
class idUserInterfaceLocal;

class rvDebuggerScript 
{
public:

	rvDebuggerScript ( void );
	~rvDebuggerScript ( void );

	bool	Load		( const char* filename );	
	bool	Reload		( void );

	const char*		GetFilename		( void );
	const char*		GetContents		( void );

	idProgram&		GetProgram		( void );

	bool			IsLineCode		( int linenumber );
	bool			IsFileModified	( bool updateTime = false );	

protected:

	void			Unload			( void );

	idProgram*				mProgram;
	idUserInterfaceLocal*	mInterface;
	char*					mContents;
	idStr					mFilename;
	ID_TIME_T					mModifiedTime;
};

ID_INLINE const char* rvDebuggerScript::GetFilename	( void )
{
	return mFilename;
}

ID_INLINE const char* rvDebuggerScript::GetContents	( void )
{
	return mContents?mContents:"";
}

ID_INLINE idProgram& rvDebuggerScript::GetProgram ( void )
{
	return *mProgram;
}

#endif // DEBUGGERSCRIPT_H_