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
#ifndef DEBUGGERBREAKPOINT_H_
#define DEBUGGERBREAKPOINT_H_

class rvDebuggerBreakpoint 
{
public:

	rvDebuggerBreakpoint ( const char* filename, int linenumber, int id = -1 );
	rvDebuggerBreakpoint ( rvDebuggerBreakpoint& bp );
	~rvDebuggerBreakpoint ( void );

	const char*		GetFilename		( void );
	int				GetLineNumber	( void );
	int				GetID			( void );

protected:

	bool	mEnabled;
	int		mID;
	int		mLineNumber;
	idStr	mFilename;
	
private:

	static int	mNextID;
};

ID_INLINE const char* rvDebuggerBreakpoint::GetFilename ( void )
{
	return mFilename;
}

ID_INLINE int rvDebuggerBreakpoint::GetLineNumber ( void )
{
	return mLineNumber;
}

ID_INLINE int rvDebuggerBreakpoint::GetID ( void )
{
	return mID;
}

#endif // DEBUGGERBREAKPOINT_H_
