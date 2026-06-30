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

#ifndef __VID_BUFFERS__
#define __VID_BUFFERS__

#include "quakedef.h"
#include "vid.h"
#ifndef CHOCOLATE_QUAKE_PS3
#include <SDL_render.h>
#endif

void VID_ReallocBuffers(void);

void VID_FreeBuffers(void);

#ifndef CHOCOLATE_QUAKE_PS3
void VID_UpdateTexture(SDL_Texture* texture, vrect_t* rect);
void VID_BlitToSurface(SDL_Surface* dst);
#endif

#ifdef CHOCOLATE_QUAKE_PS3
// PS3: CPU palette expansion + direct RSX present (no SDL texture).
void VID_UpdateAndPresent(vrect_t* rect);
#endif

// PS3 direct-RSX backend (vid_ps3.c).
void VID_PS3_Init(void);
void VID_PS3_Shutdown(void);
void VID_PS3_Present(const void* src_pixels, int src_w, int src_h);
qboolean VID_PS3_IsReady(void);

// Accessor: raw 32-bit ARGB pixel data after the 8-bit → 32-bit conversion.
void* VID_GetARGBPixels(void);

#endif
