/*
===========================================================================

DarklightNG Source Code
Copyright (C) 2026 - Justin Marshall (aka IceColdDuke).

SDL2 event pump. Native Windows editor messages remain available through
SDL_SYSWMEVENT so the existing editor accelerator path keeps working.

===========================================================================
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "sys_platform.h"
#include "../renderer/tr_local.h"

#include <SDL_syswm.h>

static void Sys_HandleSDLWindowEvent( const SDL_WindowEvent &event ) {
	switch ( event.event ) {
	case SDL_WINDOWEVENT_FOCUS_GAINED:
		win32.activeApp = true;
		idKeyInput::ClearStates();
		com_editorActive = false;
		Sys_GrabMouseCursor( true );
		if ( session != NULL ) {
			session->SetPlayingSoundWorld();
		}
		break;

	case SDL_WINDOWEVENT_FOCUS_LOST:
		win32.activeApp = false;
		win32.movingWindow = false;
		GLimp_GrabInput( false );
		break;

	case SDL_WINDOWEVENT_MOVED:
		if ( !win32.cdsFullscreen ) {
			win32.win_xpos.SetInteger( event.data1 );
			win32.win_ypos.SetInteger( event.data2 );
			win32.win_xpos.ClearModified();
			win32.win_ypos.ClearModified();
		}
		break;

	case SDL_WINDOWEVENT_RESIZED:
	case SDL_WINDOWEVENT_SIZE_CHANGED:
		if ( event.data1 > 0 && event.data2 > 0 ) {
			glConfig.vidWidth = event.data1;
			glConfig.vidHeight = event.data2;
		}
		break;

	case SDL_WINDOWEVENT_MINIMIZED:
		win32.activeApp = false;
		GLimp_GrabInput( false );
		break;

	case SDL_WINDOWEVENT_RESTORED:
	case SDL_WINDOWEVENT_MAXIMIZED:
		win32.activeApp = true;
		break;

	case SDL_WINDOWEVENT_CLOSE:
		cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "quit\n" );
		break;

	default:
		break;
	}
}

void Sys_ProcessSDLEvents( void ) {
	SDL_Event event;
	while ( SDL_PollEvent( &event ) ) {
		const int eventTime = event.common.timestamp != 0 ? (int)event.common.timestamp : Sys_Milliseconds();
		win32.sysMsgTime = eventTime;

		switch ( event.type ) {
		case SDL_WINDOWEVENT:
			Sys_HandleSDLWindowEvent( event.window );
			break;

		case SDL_KEYDOWN:
			if ( event.key.keysym.sym == SDLK_RETURN && ( event.key.keysym.mod & KMOD_ALT ) != 0 ) {
				cvarSystem->SetCVarBool( "r_fullscreen", !renderSystem->IsFullScreen() );
				cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "vid_restart\n" );
				break;
			}
			Sys_QueueSDLKeyEvent( event.key.keysym, true, eventTime );
			break;

		case SDL_KEYUP:
			Sys_QueueSDLKeyEvent( event.key.keysym, false, eventTime );
			break;

		case SDL_TEXTINPUT:
			Sys_QueueSDLTextEvent( event.text.text, eventTime );
			break;

		case SDL_MOUSEMOTION:
			Sys_QueueSDLMouseMotionEvent( event.motion.xrel, event.motion.yrel, eventTime );
			break;

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			Sys_QueueSDLMouseButtonEvent( event.button.button, event.button.state == SDL_PRESSED, eventTime );
			break;

		case SDL_MOUSEWHEEL: {
			int amount = event.wheel.y;
			if ( event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ) {
				amount = -amount;
			}
			Sys_QueueSDLMouseWheelEvent( amount, eventTime );
			break;
		}

		case SDL_QUIT:
			cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "quit\n" );
			break;

#ifdef ID_ALLOW_TOOLS
		case SDL_SYSWMEVENT:
			if ( event.syswm.msg != NULL && event.syswm.msg->subsystem == SDL_SYSWM_WINDOWS ) {
				MSG message = {};
				message.hwnd = event.syswm.msg->msg.win.hwnd;
				message.message = event.syswm.msg->msg.win.msg;
				message.wParam = event.syswm.msg->msg.win.wParam;
				message.lParam = event.syswm.msg->msg.win.lParam;
				message.time = event.common.timestamp;
				GUIEditorHandleMessage( &message );
			}
			break;
#endif

		default:
			break;
		}
	}
}
