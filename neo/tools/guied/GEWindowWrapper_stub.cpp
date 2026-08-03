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
#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../../ui/EditWindow.h"
#include "../../ui/ListWindow.h"
#include "../../ui/BindWindow.h"
#include "../../ui/RenderWindow.h"
#include "../../ui/ChoiceWindow.h"

#include "GEWindowWrapper.h"

static rvGEWindowWrapper stub_wrap( NULL, rvGEWindowWrapper::WT_UNKNOWN );

rvGEWindowWrapper::rvGEWindowWrapper( idWindow* window, EWindowType type ) { }

rvGEWindowWrapper* rvGEWindowWrapper::GetWrapper ( idWindow* window ) { return &stub_wrap; }

void rvGEWindowWrapper::SetStateKey( const char*, const char*, bool ) { }

void rvGEWindowWrapper::Finish() { }
