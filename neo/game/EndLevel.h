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

class idTarget_EndLevel : public idEntity {
public:
	CLASS_PROTOTYPE( idTarget_EndLevel );

	void	Spawn( void );
			~idTarget_EndLevel();

	void	Draw();
	// the endLevel will be responsible for drawing the entire screen
	// when it is active

	void	PlayerCommand( int buttons );
	// when an endlevel is active, plauer buttons get sent here instead
	// of doing anything to the player, which will allow moving to
	// the next level

	const char *ExitCommand();
	// the game will check this each frame, and return it to the
	// session when there is something to give

private:
	idStr	exitCommand;

	idVec3	initialViewOrg;
	idVec3	initialViewAngles;
	// set when the player triggers the exit

	idUserInterface	*gui;

	bool	buttonsReleased;
	// don't skip out until buttons are released, then pressed

	bool	readyToExit;
	bool	noGui;

	void	Event_Trigger( idEntity *activator );
};

