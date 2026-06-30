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
// vid_buffers.c -- video buffers for screen drawing


#include "vid_buffers.h"
#include "d_local.h"
#include "sys.h"


#ifdef CHOCOLATE_QUAKE_PS3

/*
================================================================================

PS3 NATIVE BUFFERS (no SDL)

================================================================================
*/

#include <string.h>

static byte *screen_buffer = NULL;   // 8-bit paletted (vid.buffer points here)
static u32  *argb_buffer = NULL;     // 32-bit ARGB intermediate
static u32   pal32[256];             // 32-bit ARGB palette
static qboolean palette_changed;

static i32 VID_highhunkmark;
static i32 vid_surfcachesize;


/*
================================================================================

PALETTE

================================================================================
*/

// PS3 is big-endian: ARGB means bytes [A][R][G][B] in memory, which is
// 0xAARRGGBB as a u32 on big-endian.

static void VID_UpdatePalette(void) {
    palette_changed = true;
}

void VID_SetPalette(const byte* palette) {
    for (int i = 0; i < 256; i++) {
        u32 r = (u32)(palette[i * 3] & ~3);
        u32 g = (u32)(palette[i * 3 + 1] & ~3);
        u32 b = (u32)(palette[i * 3 + 2] & ~3);
        pal32[i] = (0xFF000000u) | (r << 16) | (g << 8) | b;
    }
    palette_changed = true;
}

void VID_ShiftPalette(const byte* palette) {
    VID_SetPalette(palette);
}

//==============================================================================


/*
================================================================================

INITIALIZATION AND SHUTDOWN

================================================================================
*/

static void VID_AllocSurfaceCache() {
    byte* buffer = (byte*) d_pzbuffer;
    size_t cache_offset = vid.width * vid.height * sizeof(*d_pzbuffer);
    byte* cache = &buffer[cache_offset];
    D_InitCaches(cache, vid_surfcachesize);
}

static void VID_AllocZBuffer() {
    i32 chunk = vid.width * vid.height * sizeof(*d_pzbuffer);
    chunk += vid_surfcachesize;
    VID_highhunkmark = Hunk_HighMark();
    d_pzbuffer = Hunk_HighAllocName(chunk, "video");
    if (!d_pzbuffer) {
        Sys_Error("Not enough memory for video mode\n");
    }
}

static void VID_AllocRgbaBuffer(void) {
    int size = vid.width * vid.height * sizeof(u32);
    argb_buffer = (u32*) Q_calloc(1, size);
}

static void VID_AllocScreenBuffer(void) {
    int size = vid.width * vid.height;
    screen_buffer = (byte*) Q_calloc(1, size);
    memset(screen_buffer, 0, size);
    vid.buffer = screen_buffer;
}

void VID_ReallocBuffers(void) {
    VID_FreeBuffers();

    vid_surfcachesize = D_SurfaceCacheForRes(vid.width, vid.height);
    VID_AllocScreenBuffer();
    VID_AllocRgbaBuffer();
    VID_AllocZBuffer();
    VID_AllocSurfaceCache();

    VID_UpdatePalette();
}

void VID_FreeBuffers(void) {
    if (screen_buffer) {
        Q_free(screen_buffer);
        screen_buffer = NULL;
    }
    if (argb_buffer) {
        Q_free(argb_buffer);
        argb_buffer = NULL;
    }
    if (d_pzbuffer) {
        D_FlushCaches();
        Hunk_FreeToHighMark(VID_highhunkmark);
        d_pzbuffer = NULL;
    }
}

//==============================================================================


/*
================================================================================

BUFFER ACCESS

================================================================================
*/

void VID_LockBuffer(void) {
    // No-op: single-threaded, no surface locking needed.
}

void VID_UnlockBuffer(void) {
    // No-op.
}

// CPU palette expansion + present:
// Expand the dirty rect from 8-bit paletted to 32-bit ARGB, then hand
// the full ARGB buffer to vid_ps3.c for upscale + flip.
void VID_UpdateAndPresent(vrect_t* rect) {
    if (!argb_buffer || !screen_buffer) return;

    if (palette_changed) {
        rect->x = 0;
        rect->y = 0;
        rect->width = vid.width;
        rect->height = vid.height;
        palette_changed = false;
    }

    // 8-bit → 32-bit expansion for the dirty rect
    for (int y = rect->y; y < rect->y + rect->height; y++) {
        const byte *src = screen_buffer + y * vid.width + rect->x;
        u32        *dst = argb_buffer  + y * vid.width + rect->x;
        for (int x = 0; x < rect->width; x++) {
            dst[x] = pal32[src[x]];
        }
    }

    // Hand the full ARGB buffer to vid_ps3.c for upscale + flip
    VID_PS3_Present(argb_buffer, (int) vid.width, (int) vid.height);
}

void* VID_GetARGBPixels(void) {
    return argb_buffer;
}

//==============================================================================


#else /* !CHOCOLATE_QUAKE_PS3 */


/*
================================================================================

DESKTOP SDL BUFFERS

================================================================================
*/

// The paletted buffer that we draw to (i.e. the one that holds vid_buffer).
static SDL_Surface* screen_buffer = NULL;

// The RGBA intermediate buffer that we blit the screen_buffer to.
static SDL_Surface* argb_buffer = NULL;

extern const u32 pixel_format;

static i32 VID_highhunkmark;
static i32 vid_surfcachesize;

static qboolean palette_changed;
static SDL_Color pal[256];


/*
================================================================================

PALETTE

================================================================================
*/

static void VID_UpdatePalette(void) {
    SDL_Palette* sdl_palette = screen_buffer->format->palette;
    SDL_SetPaletteColors(sdl_palette, pal, 0, 256);
    palette_changed = false;
}

void VID_SetPalette(const byte* palette) {
    for (i32 i = 0; i < 256; i++) {
        byte r = palette[i * 3];
        byte g = palette[(i * 3) + 1];
        byte b = palette[(i * 3) + 2];

        pal[i].r = r & ~3;
        pal[i].g = g & ~3;
        pal[i].b = b & ~3;
        pal[i].a = SDL_ALPHA_OPAQUE;
    }

    palette_changed = true;
}

void VID_ShiftPalette(const byte* palette) {
    VID_SetPalette(palette);
}

//==============================================================================


/*
================================================================================

INITIALIZATION AND SHUTDOWN

================================================================================
*/

static void VID_AllocSurfaceCache() {
    byte* buffer = (byte*) d_pzbuffer;
    size_t cache_offset = vid.width * vid.height * sizeof(*d_pzbuffer);
    byte* cache = &buffer[cache_offset];
    D_InitCaches(cache, vid_surfcachesize);
}

static void VID_AllocZBuffer() {
    i32 chunk = vid.width * vid.height * sizeof(*d_pzbuffer);
    chunk += vid_surfcachesize;
    VID_highhunkmark = Hunk_HighMark();
    d_pzbuffer = Hunk_HighAllocName(chunk, "video");
    if (!d_pzbuffer) {
        Sys_Error("Not enough memory for video mode\n");
    }
}

static void VID_AllocRgbaBuffer(void) {
    int w = (int) vid.width;
    int h = (int) vid.height;
    void* pixels = NULL;
    int depth = 0;
    int pitch = 0;
    argb_buffer = SDL_CreateRGBSurfaceWithFormatFrom(
        pixels, w, h, depth, pitch, pixel_format
    );
}

static void VID_AllocScreenBuffer(void) {
    u32 flags = 0;
    int w = (int) vid.width;
    int h = (int) vid.height;
    int depth = 8;
    u32 r_mask = 0;
    u32 g_mask = 0;
    u32 b_mask = 0;
    u32 a_mask = 0;

    screen_buffer = SDL_CreateRGBSurface(
        flags, w, h, depth, r_mask,
        g_mask, b_mask, a_mask
    );
    SDL_FillRect(screen_buffer, NULL, 0);

    vid.buffer = (byte*) screen_buffer->pixels;
}

void VID_ReallocBuffers(void) {
    VID_FreeBuffers();

    vid_surfcachesize = D_SurfaceCacheForRes(vid.width, vid.height);
    VID_AllocScreenBuffer();
    VID_AllocRgbaBuffer();
    VID_AllocZBuffer();
    VID_AllocSurfaceCache();

    VID_UpdatePalette();
}

void VID_FreeBuffers(void) {
    if (screen_buffer) {
        SDL_FreeSurface(screen_buffer);
        screen_buffer = NULL;
    }
    if (argb_buffer) {
        SDL_FreeSurface(argb_buffer);
        argb_buffer = NULL;
    }
    if (d_pzbuffer) {
        D_FlushCaches();
        Hunk_FreeToHighMark(VID_highhunkmark);
        d_pzbuffer = NULL;
    }
}

//==============================================================================


/*
================================================================================

BUFFER ACCESS

================================================================================
*/

void VID_LockBuffer(void) {
    if (SDL_MUSTLOCK(screen_buffer)) {
        SDL_LockSurface(screen_buffer);
    }
}

void VID_UnlockBuffer(void) {
    SDL_UnlockSurface(screen_buffer);
}

void VID_UpdateTexture(SDL_Texture* texture, vrect_t* rect) {
    if (palette_changed) {
        VID_UpdatePalette();
        rect->x = 0;
        rect->x = 0;
        rect->width = (i32) vid.width;
        rect->height = (i32) vid.height;
    }
    SDL_Rect src_rect = {
        .x = rect->x,
        .y = rect->y,
        .w = rect->width,
        .h = rect->height,
    };
    SDL_Rect dst_rect = {
        .x = 0,
        .y = 0,
        .w = rect->width,
        .h = rect->height,
    };
    SDL_LockTexture(texture, &src_rect, &argb_buffer->pixels,
                    &argb_buffer->pitch);
    SDL_LowerBlit(screen_buffer, &src_rect, argb_buffer, &dst_rect);
    SDL_UnlockTexture(texture);
}

void VID_BlitToSurface(SDL_Surface* dst) {
    SDL_BlitScaled(argb_buffer, NULL, dst, NULL);
}

void* VID_GetARGBPixels(void) {
    return argb_buffer ? argb_buffer->pixels : NULL;
}

//==============================================================================

#endif /* CHOCOLATE_QUAKE_PS3 */
