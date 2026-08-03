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

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "../libbink/bink.h"
#include "tr_local.h"

#include <climits>

class idCinematicLocal : public idCinematic {
public:
							idCinematicLocal();
	virtual					~idCinematicLocal();

	virtual bool			InitFromFile( const char *qpath, bool looping );
	virtual cinData_t	ImageForTime( int milliseconds );
	virtual int				AnimationLength();
	virtual void			Close();
	virtual void			ResetTime( int time );

private:
	bool					DecodeFrame( unsigned long frameNumber );
	void					CopyFrame();

	idStr					fileName;
	cw_bink_decoder *		decoder;
	cinStatus_t				status;
	int						animationLength;
	int						startTime;
	long					decodedFrame;
	unsigned long			width;
	unsigned long			height;
	unsigned long			frameCount;
	unsigned long			fpsNumerator;
	unsigned long			fpsDenominator;
	byte *					image;
	bool					looping;
};

/*
==============
idCinematic::InitCinematic
==============
*/
void idCinematic::InitCinematic( void ) {
}

/*
==============
idCinematic::ShutdownCinematic
==============
*/
void idCinematic::ShutdownCinematic( void ) {
}

/*
==============
idCinematic::Alloc
==============
*/
idCinematic *idCinematic::Alloc() {
	return new idCinematicLocal;
}

/*
==============
idCinematic::~idCinematic
==============
*/
idCinematic::~idCinematic() {
	Close();
}

bool idCinematic::InitFromFile( const char *qpath, bool looping ) {
	return false;
}

int idCinematic::AnimationLength() {
	return 0;
}

void idCinematic::ResetTime( int milliseconds ) {
}

cinData_t idCinematic::ImageForTime( int milliseconds ) {
	cinData_t data;
	memset( &data, 0, sizeof( data ) );
	return data;
}

void idCinematic::Close() {
}

/*
==============
idCinematicLocal::idCinematicLocal
==============
*/
idCinematicLocal::idCinematicLocal() {
	decoder = NULL;
	status = FMV_EOF;
	animationLength = 0;
	startTime = -1;
	decodedFrame = -1;
	width = 0;
	height = 0;
	frameCount = 0;
	fpsNumerator = 0;
	fpsDenominator = 0;
	image = NULL;
	looping = false;
}

/*
==============
idCinematicLocal::~idCinematicLocal
==============
*/
idCinematicLocal::~idCinematicLocal() {
	Close();
}

/*
==============
idCinematicLocal::InitFromFile

The original material declarations name RoQ files.  Keep those declarations
compatible while loading a Bink movie with the same base name.
==============
*/
bool idCinematicLocal::InitFromFile( const char *qpath, bool amILooping ) {
	Close();

	if ( !qpath || !qpath[0] ) {
		return false;
	}

	if ( strstr( qpath, "/" ) == NULL && strstr( qpath, "\\" ) == NULL ) {
		fileName = va( "video/%s", qpath );
	} else {
		fileName = qpath;
	}
	fileName.SetFileExtension( "bik" );

	// Resolve the virtual game path to the loose OS file expected by libbink.
	// Bink assets are intentionally kept outside pk4 files so the decoder can
	// stream them directly.
	idFile *binkFile = fileSystem->OpenFileRead( fileName );
	if ( !binkFile ) {
		common->DPrintf( "Bink cinematic not found: %s\n", fileName.c_str() );
		return false;
	}
	idStr osPath = binkFile->GetFullPath();
	fileSystem->CloseFile( binkFile );

	decoder = cw_bink_open_file( osPath.c_str() );
	if ( !decoder ) {
		common->Warning( "Could not open Bink cinematic '%s'", fileName.c_str() );
		Close();
		return false;
	}

	if ( cw_bink_info( decoder, &width, &height, &frameCount,
		&fpsNumerator, &fpsDenominator ) != 0 ||
		!width || !height || !frameCount || !fpsNumerator || !fpsDenominator ) {
		common->Warning( "Invalid Bink cinematic '%s'", fileName.c_str() );
		Close();
		return false;
	}

	double length = (double)frameCount * (double)fpsDenominator * 1000.0 /
		(double)fpsNumerator;
	animationLength = length >= (double)INT_MAX ? INT_MAX : (int)( length + 0.5 );
	looping = amILooping;
	startTime = 0;
	image = (byte *)Mem_Alloc( width * height * 4 );

	if ( !DecodeFrame( 0 ) ) {
		Close();
		return false;
	}

	status = looping ? FMV_PLAY : FMV_IDLE;
	return true;
}

/*
==============
idCinematicLocal::Close
==============
*/
void idCinematicLocal::Close() {
	if ( decoder ) {
		cw_bink_close( decoder );
		decoder = NULL;
	}
	if ( image ) {
		Mem_Free( image );
		image = NULL;
	}

	fileName.Clear();
	status = FMV_IDLE;
	animationLength = 0;
	startTime = -1;
	decodedFrame = -1;
	width = 0;
	height = 0;
	frameCount = 0;
	fpsNumerator = 0;
	fpsDenominator = 0;
	looping = false;
}

/*
==============
idCinematicLocal::AnimationLength
==============
*/
int idCinematicLocal::AnimationLength() {
	return animationLength;
}

/*
==============
idCinematicLocal::ResetTime
==============
*/
void idCinematicLocal::ResetTime( int time ) {
	startTime = time;
	status = decoder ? FMV_PLAY : FMV_EOF;
}

/*
==============
idCinematicLocal::DecodeFrame
==============
*/
bool idCinematicLocal::DecodeFrame( unsigned long frameNumber ) {
	if ( !decoder || frameNumber >= frameCount ) {
		return false;
	}

	int decodeStatus = CW_BINK_MORE;
	if ( decodedFrame < 0 || frameNumber < (unsigned long)decodedFrame ) {
		decodeStatus = cw_bink_first( decoder );
		decodedFrame = decodeStatus == CW_BINK_ERROR ? -1 : 0;
	}

	while ( decodedFrame >= 0 && (unsigned long)decodedFrame < frameNumber ) {
		decodeStatus = cw_bink_next( decoder );
		if ( decodeStatus == CW_BINK_ERROR || decodeStatus == CW_BINK_DONE ) {
			decodedFrame = -1;
			break;
		}
		decodedFrame++;
	}

	if ( decodedFrame < 0 ) {
		common->Warning( "Bink decode failed for '%s': %s",
			fileName.c_str(), cw_bink_get_error( decoder ) );
		status = FMV_EOF;
		return false;
	}

	CopyFrame();
	return true;
}

/*
==============
idCinematicLocal::CopyFrame
==============
*/
void idCinematicLocal::CopyFrame() {
	const unsigned char *rgb = cw_bink_get_rgb24( decoder );
	if ( !rgb || !image ) {
		return;
	}

	const unsigned long pixelCount = width * height;
	for ( unsigned long pixel = 0; pixel < pixelCount; pixel++ ) {
		image[pixel * 4 + 0] = rgb[pixel * 3 + 0];
		image[pixel * 4 + 1] = rgb[pixel * 3 + 1];
		image[pixel * 4 + 2] = rgb[pixel * 3 + 2];
		image[pixel * 4 + 3] = 255;
	}
}

/*
==============
idCinematicLocal::ImageForTime
==============
*/
cinData_t idCinematicLocal::ImageForTime( int thisTime ) {
	cinData_t data;
	memset( &data, 0, sizeof( data ) );

	if ( r_skipBink.GetBool() || !decoder || status == FMV_EOF || status == FMV_IDLE ) {
		return data;
	}

	if ( thisTime < 0 ) {
		thisTime = 0;
	}
	if ( startTime < 0 ) {
		startTime = thisTime;
	}

	int elapsed = thisTime - startTime;
	if ( elapsed < 0 ) {
		elapsed = 0;
	}

	double frameTime = (double)elapsed * (double)fpsNumerator /
		( 1000.0 * (double)fpsDenominator );
	unsigned long frameNumber = (unsigned long)frameTime;

	if ( frameNumber >= frameCount ) {
		if ( looping ) {
			frameNumber %= frameCount;
		} else {
			status = FMV_IDLE;
			frameNumber = frameCount - 1;
		}
	}

	if ( (long)frameNumber != decodedFrame && !DecodeFrame( frameNumber ) ) {
		return data;
	}

	data.imageWidth = (int)width;
	data.imageHeight = (int)height;
	data.image = image;
	data.status = status;
	return data;
}

//===========================================

bool idSndWindow::InitFromFile( const char *qpath, bool looping ) {
	idStr fname = qpath;

	fname.ToLower();
	showWaveform = !fname.Icmp( "waveform" );
	return true;
}

cinData_t idSndWindow::ImageForTime( int milliseconds ) {
	return soundSystem->ImageForTime( milliseconds, showWaveform );
}

int idSndWindow::AnimationLength() {
	return -1;
}
