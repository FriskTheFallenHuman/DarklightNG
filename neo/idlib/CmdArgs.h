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

#ifndef __CMDARGS_H__
#define __CMDARGS_H__

/*
===============================================================================

	Command arguments.

===============================================================================
*/

class idCmdArgs {
public:
							idCmdArgs( void ) { argc = 0; }
							idCmdArgs( const char *text, bool keepAsStrings ) { TokenizeString( text, keepAsStrings ); }

	void					operator=( const idCmdArgs &args );

							// The functions that execute commands get their parameters with these functions.
	int						Argc( void ) const { return argc; }
							// Argv() will return an empty string, not NULL if arg >= argc.
	const char *			Argv( int arg ) const { return ( arg >= 0 && arg < argc ) ? argv[arg] : ""; }
							// Returns a single string containing argv(start) to argv(end)
							// escapeArgs is a fugly way to put the string back into a state ready to tokenize again
	const char *			Args( int start = 1, int end = -1, bool escapeArgs = false ) const;

							// Takes a null terminated string and breaks the string up into arg tokens.
							// Does not need to be /n terminated.
							// Set keepAsStrings to true to only seperate tokens from whitespace and comments, ignoring punctuation
	void					TokenizeString( const char *text, bool keepAsStrings );

	void					AppendArg( const char *text );
	void					Clear( void ) { argc = 0; }
	const char **			GetArgs( int *argc );

private:
	static const int		MAX_COMMAND_ARGS = 64;
	static const int		MAX_COMMAND_STRING = 2 * MAX_STRING_CHARS;

	int						argc;								// number of arguments
	char *					argv[MAX_COMMAND_ARGS];				// points into tokenized
	char					tokenized[MAX_COMMAND_STRING];		// will have 0 bytes inserted
};

#endif /* !__CMDARGS_H__ */
