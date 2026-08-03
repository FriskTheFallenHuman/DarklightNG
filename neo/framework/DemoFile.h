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

#ifndef __DEMOFILE_H__
#define __DEMOFILE_H__

/*
===============================================================================

	Demo file

===============================================================================
*/

typedef enum {
	DS_FINISHED,
	DS_RENDER,
	DS_SOUND,
	DS_VERSION
} demoSystem_t;

class idDemoFile : public idFile {
public:
					idDemoFile();
					~idDemoFile();

	const char *	GetName( void ) { return (f?f->GetName():""); }
	const char *	GetFullPath( void ) { return (f?f->GetFullPath():""); }

	void			SetLog( bool b, const char *p );
	void			Log( const char *p );
	bool			OpenForReading( const char *fileName );
	bool			OpenForWriting( const char *fileName );
	void			Close();

	const char *	ReadHashString();
	void			WriteHashString( const char *str );

	void			ReadDict( idDict &dict );
	void			WriteDict( const idDict &dict );

	int				Read( void *buffer, int len );
	int				Write( const void *buffer, int len );

private:
	static idCompressor *AllocCompressor( int type );

	bool			writing;
	byte *			fileImage;
	idFile *		f;
	idCompressor *	compressor;

	idList<idStr*>	demoStrings;
	idFile *		fLog;
	bool			log;
	idStr			logStr;

	static idCVar	com_logDemos;
	static idCVar	com_compressDemos;
	static idCVar	com_preloadDemos;
};

#endif /* !__DEMOFILE_H__ */
