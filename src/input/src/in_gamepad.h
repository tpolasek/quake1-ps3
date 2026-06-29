/*
 * Copyright (C) 1996-1997 Id Software, Inc.
 * Copyright (C) Henrique Barateli, <henriquejb194@gmail.com>, et al.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
// in_gamepad.h -- gamepad code


#ifndef __IN_GAMEPAD__
#define __IN_GAMEPAD__

#include "quakedef.h"
#include "client.h"

// Desktop builds receive gamepad input asynchronously via SDL events
// (IN_GamepadEvent, pumped from Sys_SendKeyEvents). PS3 builds bypass
// SDL entirely and poll the DualShock 3 directly via PSL1GHT's io/pad.h
// each frame (IN_PollGamepad) -- the SDL2 PSL1GHT backend had no usable
// game-controller mapping and the Bluetooth pad-rumble layer added
// latency. PSL1GHT gives us a padData bitfield with named BTN_CROSS /
// BTN_TRIANGLE / etc. fields, so we read those directly.
#ifdef CHOCOLATE_QUAKE_PS3
void IN_PollGamepad(void);
#else
#include <SDL_events.h>
void IN_GamepadEvent(const SDL_Event* event);
#endif

void IN_JoyMove(usercmd_t* cmd);

void IN_InitGamepad(void);

void IN_ShutdownGamepad(void);

#endif
