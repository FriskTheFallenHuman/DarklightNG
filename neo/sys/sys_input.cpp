/*
===========================================================================

DarklightNG Source Code
Copyright (C) 2026 - Justin Marshall (aka IceColdDuke).

SDL2 keyboard and mouse input implementation.

===========================================================================
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "sys_platform.h"

struct sdlKeyboardPoll_t {
	int key;
	bool state;

	sdlKeyboardPoll_t() : key( 0 ), state( false ) {}
	sdlKeyboardPoll_t( int key_, bool state_ ) : key( key_ ), state( state_ ) {}
};

struct sdlMousePoll_t {
	int action;
	int value;

	sdlMousePoll_t() : action( 0 ), value( 0 ) {}
	sdlMousePoll_t( int action_, int value_ ) : action( action_ ), value( value_ ) {}
};

static idList<sdlKeyboardPoll_t> keyboardPolls;
static idList<sdlMousePoll_t> mousePolls;

// Kept for native editor child windows which still translate Win32 scan codes.
static const unsigned char scanToKey[256] = {
	0, 27, '1', '2', '3', '4', '5', '6',
	'7', '8', '9', '0', '-', '=', K_BACKSPACE, 9,
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
	'o', 'p', '[', ']', K_ENTER, K_CTRL, 'a', 's',
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
	'\'', '`', K_SHIFT, '\\', 'z', 'x', 'c', 'v',
	'b', 'n', 'm', ',', '.', '/', K_SHIFT, K_KP_STAR,
	K_ALT, ' ', K_CAPSLOCK, K_F1, K_F2, K_F3, K_F4, K_F5,
	K_F6, K_F7, K_F8, K_F9, K_F10, K_PAUSE, K_SCROLL, K_HOME,
	K_UPARROW, K_PGUP, K_KP_MINUS, K_LEFTARROW, K_KP_5, K_RIGHTARROW, K_KP_PLUS, K_END,
	K_DOWNARROW, K_PGDN, K_INS, K_DEL, 0, 0, 0, K_F11,
	K_F12, 0, 0, K_LWIN, K_RWIN, K_MENU, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 27, '!', '@', '#', '$', '%', '^',
	'&', '*', '(', ')', '_', '+', K_BACKSPACE, 9,
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
	'O', 'P', '{', '}', K_ENTER, K_CTRL, 'A', 'S',
	'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
	'"', '~', K_SHIFT, '|', 'Z', 'X', 'C', 'V',
	'B', 'N', 'M', '<', '>', '?', K_SHIFT, K_KP_STAR,
	K_ALT, ' ', K_CAPSLOCK, K_F1, K_F2, K_F3, K_F4, K_F5,
	K_F6, K_F7, K_F8, K_F9, K_F10, K_PAUSE, K_SCROLL, K_HOME,
	K_UPARROW, K_PGUP, K_KP_MINUS, K_LEFTARROW, K_KP_5, K_RIGHTARROW, K_KP_PLUS, K_END,
	K_DOWNARROW, K_PGDN, K_INS, K_DEL, 0, 0, 0, K_F11,
	K_F12, 0, 0, K_LWIN, K_RWIN, K_MENU, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};

static int Sys_MapSDLKey( const SDL_Keysym &keysym ) {
	const SDL_Scancode scan = keysym.scancode;
	if ( scan == SDL_SCANCODE_0 ) {
		return '0';
	}
	if ( scan >= SDL_SCANCODE_1 && scan <= SDL_SCANCODE_9 ) {
		return '1' + ( scan - SDL_SCANCODE_1 );
	}

	const SDL_Keycode key = keysym.sym;
	if ( key >= SDLK_SPACE && key <= SDLK_z ) {
		return key & 0xff;
	}

	switch ( key ) {
	case SDLK_BACKSPACE: return K_BACKSPACE;
	case SDLK_TAB: return K_TAB;
	case SDLK_RETURN: return K_ENTER;
	case SDLK_ESCAPE: return K_ESCAPE;
	case SDLK_PAUSE: return K_PAUSE;
	case SDLK_APPLICATION: return K_COMMAND;
	case SDLK_CAPSLOCK: return K_CAPSLOCK;
	case SDLK_SCROLLLOCK: return K_SCROLL;
	case SDLK_POWER: return K_POWER;
	case SDLK_UP: return K_UPARROW;
	case SDLK_DOWN: return K_DOWNARROW;
	case SDLK_LEFT: return K_LEFTARROW;
	case SDLK_RIGHT: return K_RIGHTARROW;
	case SDLK_LGUI: return K_LWIN;
	case SDLK_RGUI: return K_RWIN;
	case SDLK_MENU: return K_MENU;
	case SDLK_LALT: return K_ALT;
	case SDLK_RALT: return K_RIGHT_ALT;
	case SDLK_LCTRL:
	case SDLK_RCTRL: return K_CTRL;
	case SDLK_LSHIFT:
	case SDLK_RSHIFT: return K_SHIFT;
	case SDLK_INSERT: return K_INS;
	case SDLK_DELETE: return K_DEL;
	case SDLK_PAGEDOWN: return K_PGDN;
	case SDLK_PAGEUP: return K_PGUP;
	case SDLK_HOME: return K_HOME;
	case SDLK_END: return K_END;
	case SDLK_F1: return K_F1;
	case SDLK_F2: return K_F2;
	case SDLK_F3: return K_F3;
	case SDLK_F4: return K_F4;
	case SDLK_F5: return K_F5;
	case SDLK_F6: return K_F6;
	case SDLK_F7: return K_F7;
	case SDLK_F8: return K_F8;
	case SDLK_F9: return K_F9;
	case SDLK_F10: return K_F10;
	case SDLK_F11: return K_F11;
	case SDLK_F12: return K_F12;
	case SDLK_F13: return K_F13;
	case SDLK_F14: return K_F14;
	case SDLK_F15: return K_F15;
	case SDLK_KP_7: return K_KP_HOME;
	case SDLK_KP_8: return K_KP_UPARROW;
	case SDLK_KP_9: return K_KP_PGUP;
	case SDLK_KP_4: return K_KP_LEFTARROW;
	case SDLK_KP_5: return K_KP_5;
	case SDLK_KP_6: return K_KP_RIGHTARROW;
	case SDLK_KP_1: return K_KP_END;
	case SDLK_KP_2: return K_KP_DOWNARROW;
	case SDLK_KP_3: return K_KP_PGDN;
	case SDLK_KP_ENTER: return K_KP_ENTER;
	case SDLK_KP_0: return K_KP_INS;
	case SDLK_KP_PERIOD: return K_KP_DEL;
	case SDLK_KP_DIVIDE: return K_KP_SLASH;
	case SDLK_KP_MINUS: return K_KP_MINUS;
	case SDLK_KP_PLUS: return K_KP_PLUS;
	case SDLK_NUMLOCKCLEAR: return K_KP_NUMLOCK;
	case SDLK_KP_MULTIPLY: return K_KP_STAR;
	case SDLK_KP_EQUALS: return K_KP_EQUALS;
	case SDLK_PRINTSCREEN: return K_PRINT_SCR;
	case SDLK_MODE: return K_RIGHT_ALT;
	default:
		break;
	}

	if ( scan == SDL_SCANCODE_GRAVE ) {
		return Sys_GetConsoleKey( ( keysym.mod & KMOD_SHIFT ) != 0 );
	}
	return 0;
}

void Sys_QueueSDLKeyEvent( const SDL_Keysym &keysym, bool down, int time ) {
	const int key = Sys_MapSDLKey( keysym );
	if ( key == 0 ) {
		return;
	}
	keyboardPolls.Append( sdlKeyboardPoll_t( key, down ) );
	Sys_QueEvent( time, SE_KEY, key, down ? 1 : 0, 0, NULL );

	// SDL text input intentionally omits editing keys.
	if ( down && key == K_BACKSPACE ) {
		Sys_QueEvent( time, SE_CHAR, K_BACKSPACE, 0, 0, NULL );
	}
}

void Sys_QueueSDLTextEvent( const char *utf8, int time ) {
	if ( utf8 == NULL ) {
		return;
	}
	// Doom 3's SE_CHAR interface predates Unicode and consumes one byte at a time.
	for ( const unsigned char *c = (const unsigned char *)utf8; *c != '\0'; ++c ) {
		Sys_QueEvent( time, SE_CHAR, *c, 0, 0, NULL );
	}
}

void Sys_QueueSDLMouseMotionEvent( int x, int y, int time ) {
	if ( x == 0 && y == 0 ) {
		return;
	}
	if ( x != 0 ) {
		mousePolls.Append( sdlMousePoll_t( M_DELTAX, x ) );
	}
	if ( y != 0 ) {
		mousePolls.Append( sdlMousePoll_t( M_DELTAY, y ) );
	}
	Sys_QueEvent( time, SE_MOUSE, x, y, 0, NULL );
}

void Sys_QueueSDLMouseButtonEvent( Uint8 button, bool down, int time ) {
	int index;
	switch ( button ) {
	case SDL_BUTTON_LEFT: index = 0; break;
	case SDL_BUTTON_RIGHT: index = 1; break;
	case SDL_BUTTON_MIDDLE: index = 2; break;
	case SDL_BUTTON_X1: index = 3; break;
	case SDL_BUTTON_X2: index = 4; break;
	default:
		index = button - SDL_BUTTON_LEFT;
		if ( index < 0 || index >= 8 ) {
			return;
		}
		break;
	}
	mousePolls.Append( sdlMousePoll_t( M_ACTION1 + index, down ? 1 : 0 ) );
	Sys_QueEvent( time, SE_KEY, K_MOUSE1 + index, down ? 1 : 0, 0, NULL );
}

void Sys_QueueSDLMouseWheelEvent( int amount, int time ) {
	if ( amount == 0 ) {
		return;
	}
	mousePolls.Append( sdlMousePoll_t( M_DELTAZ, amount ) );
	const int key = amount < 0 ? K_MWHEELDOWN : K_MWHEELUP;
	for ( int count = idMath::Abs( amount ); count > 0; --count ) {
		Sys_QueEvent( time, SE_KEY, key, 1, 0, NULL );
		Sys_QueEvent( time, SE_KEY, key, 0, 0, NULL );
	}
}

void Sys_ClearSDLInputEvents( void ) {
	keyboardPolls.SetNum( 0, false );
	mousePolls.SetNum( 0, false );
}

void Sys_InitInput( void ) {
	common->Printf( "\n------- SDL2 Input Initialization -------\n" );
	keyboardPolls.SetGranularity( 64 );
	mousePolls.SetGranularity( 64 );
	SDL_StartTextInput();
	win32.in_mouse.ClearModified();
	common->Printf( "-----------------------------------------\n" );
}

void Sys_ShutdownInput( void ) {
	IN_DeactivateMouse();
	SDL_StopTextInput();
	keyboardPolls.Clear();
	mousePolls.Clear();
}

void Sys_InitScanTable( void ) {
}

const unsigned char *Sys_GetScanTable( void ) {
	return scanToKey;
}

int MapKey( int key ) {
	const int scanCode = ( key >> 16 ) & 255;
	if ( scanCode > 127 ) {
		return 0;
	}
	const bool extended = ( key & ( 1 << 24 ) ) != 0;
	if ( extended && scanCode == 0x35 ) {
		return K_KP_SLASH;
	}
	const int result = scanToKey[scanCode];
	if ( extended ) {
		switch ( result ) {
		case K_PAUSE: return K_KP_NUMLOCK;
		case K_ENTER: return K_KP_ENTER;
		case '/': return K_KP_SLASH;
		case K_KP_PLUS: return K_KP_PLUS;
		case K_KP_STAR: return K_PRINT_SCR;
		case K_ALT: return K_RIGHT_ALT;
		default: return result;
		}
	}
	switch ( result ) {
	case K_HOME: return K_KP_HOME;
	case K_UPARROW: return K_KP_UPARROW;
	case K_PGUP: return K_KP_PGUP;
	case K_LEFTARROW: return K_KP_LEFTARROW;
	case K_RIGHTARROW: return K_KP_RIGHTARROW;
	case K_END: return K_KP_END;
	case K_DOWNARROW: return K_KP_DOWNARROW;
	case K_PGDN: return K_KP_PGDN;
	case K_INS: return K_KP_INS;
	case K_DEL: return K_KP_DEL;
	default: return result;
	}
}

unsigned char Sys_GetConsoleKey( bool shifted ) {
	idStr language = cvarSystem->GetCVarString( "sys_lang" );
	if ( !language.Icmp( "french" ) ) return shifted ? '>' : '<';
	if ( !language.Icmp( "german" ) ) return shifted ? 176 : '^';
	if ( !language.Icmp( "italian" ) ) return shifted ? '|' : '\\';
	if ( !language.Icmp( "spanish" ) ) return shifted ? 170 : 186;
	return shifted ? '~' : '`';
}

void IN_ActivateMouse( void ) {
	GLimp_GrabInput( true );
}

void IN_DeactivateMouse( void ) {
	GLimp_GrabInput( false );
}

void IN_DeactivateMouseIfWindowed( void ) {
	if ( !win32.cdsFullscreen ) {
		IN_DeactivateMouse();
	}
}

void IN_Frame( void ) {
	bool shouldGrab = win32.in_mouse.GetBool();
	if ( !win32.cdsFullscreen && ( win32.mouseReleased || win32.movingWindow || !win32.activeApp ) ) {
		shouldGrab = false;
	}
	if ( shouldGrab != win32.mouseGrabbed ) {
		GLimp_GrabInput( shouldGrab );
	}
}

void Sys_GrabMouseCursor( bool grabIt ) {
#ifndef ID_DEDICATED
	win32.mouseReleased = !grabIt;
	if ( !grabIt ) {
		IN_Frame();
	}
#endif
}

int Sys_PollKeyboardInputEvents( void ) {
	return keyboardPolls.Num();
}

int Sys_ReturnKeyboardInputEvent( const int n, int &key, bool &state ) {
	if ( n < 0 || n >= keyboardPolls.Num() ) {
		return 0;
	}
	key = keyboardPolls[n].key;
	state = keyboardPolls[n].state;
	return 1;
}

void Sys_EndKeyboardInputEvents( void ) {
	keyboardPolls.SetNum( 0, false );
}

int Sys_PollMouseInputEvents( void ) {
	return mousePolls.Num();
}

int Sys_ReturnMouseInputEvent( const int n, int &action, int &value ) {
	if ( n < 0 || n >= mousePolls.Num() ) {
		return 0;
	}
	action = mousePolls[n].action;
	value = mousePolls[n].value;
	return 1;
}

void Sys_EndMouseInputEvents( void ) {
	mousePolls.SetNum( 0, false );
}

unsigned char Sys_MapCharForKey( int key ) {
	return (unsigned char)key;
}
