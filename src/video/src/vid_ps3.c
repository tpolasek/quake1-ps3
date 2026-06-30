// vid_ps3.c -- PS3 direct RSX video backend.
//
// Bypasses SDL2's renderer API entirely. The PSL1GHT SDL2 RSX backend's
// SDL_RenderPresent hangs after exactly 127 frames due to improper flip-queue
// management (it never calls gcmSetWaitFlip / gcmGetFlipStatus to recycle
// display buffers).
//
// Instead, we use PSL1GHT's gcm_* API directly to:
//   1. Allocate two display buffers in RSX memory (double-buffered).
//   2. Each frame: CPU-scale the 320x200 ARGB framebuffer to display
//      resolution, write to the back buffer, then flip with the proper
//      gcmSetFlip -> rsxFlushBuffer -> gcmSetWaitFlip -> gcmGetFlipStatus
//      pattern from the PSL1GHT rsx.h quick guide.

#include "sys.h"

#ifdef CHOCOLATE_QUAKE_PS3

#include <malloc.h>
#include <rsx/rsx.h>
#include <rsx/gcm_sys.h>
#include <sysutil/video.h>
#include <unistd.h>

#define PS3_NUM_BUFFERS 2

static void *s_io_buffer = NULL;

static gcmContextData *s_context = NULL;
static u32   s_offset[PS3_NUM_BUFFERS];   // RSX address offsets
static void *s_mem[PS3_NUM_BUFFERS];      // host addresses (RSX local memory)
static int   s_current_buf = 0;
static int   s_display_w   = 0;
static int   s_display_h   = 0;
static int   s_pitch       = 0;           // bytes per row
static qboolean s_initialized = false;

void VID_PS3_Init(void) {
    SYS_TRACE("[vid_ps3] init enter\n");

    // Try to reuse the global GCM context. If none was set up (e.g. SDL
    // is not initializing the video subsystem), fall back to calling rsxInit
    // ourselves with a private 1 MB IO buffer.
    s_context = gGcmContext;
    if (!s_context) {
        SYS_TRACE("[vid_ps3] gGcmContext is NULL, calling rsxInit\n");
        s_io_buffer = memalign(1024 * 1024, 1024 * 1024);
        if (!s_io_buffer) {
            Sys_Error("PS3 video: memalign failed for RSX IO buffer");
        }
        s32 rc = rsxInit(&s_context, 0x10000, 1024 * 1024, s_io_buffer);
        if (rc != 0) {
            SYS_TRACE("[vid_ps3] rsxInit failed: %d\n", (int) rc);
            Sys_Error("PS3 video: rsxInit failed: %d", (int) rc);
        }
    }
    SYS_TRACE("[vid_ps3] context = %p\n", (void *) s_context);

    // Read the current display mode from the system.
    videoState vstate;
    if (videoGetState(0, 0, &vstate) != 0) {
        Sys_Error("PS3 video: videoGetState failed");
    }

    videoResolution res;
    if (videoGetResolution(vstate.displayMode.resolution, &res) != 0) {
        Sys_Error("PS3 video: videoGetResolution failed");
    }
    s_display_w = res.width;
    s_display_h = res.height;
    s_pitch     = s_display_w * 4; // XRGB 32-bit

    SYS_TRACE("[vid_ps3] display: %dx%d pitch=%d\n",
              s_display_w, s_display_h, s_pitch);

    // Configure the display pipeline. Previously this was done by
    // SDL's PSL1GHT_InitModes (called inside SDL_Init(SDL_INIT_VIDEO)),
    // but now that SDL_Init is removed for PS3 we must do it ourselves.
    videoConfiguration vconfig = {0};
    vconfig.resolution = vstate.displayMode.resolution;
    vconfig.format     = VIDEO_BUFFER_FORMAT_XRGB;
    vconfig.pitch      = s_pitch;
    if (videoConfigure(0, &vconfig, NULL, 1) != 0) {
        Sys_Error("PS3 video: videoConfigure failed");
    }

    // Wait until the RSX display pipeline is ready (state 3 = not ready).
    videoState wait_state;
    do {
        usleep(10000);
        if (videoGetState(0, 0, &wait_state) != 0) break;
    } while (wait_state.state == 3);

    // Synchronise flips with vertical refresh to avoid tearing.
    gcmSetFlipMode(GCM_FLIP_VSYNC);

    // Allocate and register the two display buffers.
    for (int i = 0; i < PS3_NUM_BUFFERS; i++) {
        s_mem[i] = rsxMemalign(64, s_pitch * s_display_h);
        if (!s_mem[i]) {
            Sys_Error("PS3 video: rsxMemalign failed for buffer %d", i);
        }
        rsxAddressToOffset(s_mem[i], &s_offset[i]);
        gcmSetDisplayBuffer(i, s_offset[i], s_pitch,
                            s_display_w, s_display_h);
        SYS_TRACE("[vid_ps3] buf %d: mem=%p off=0x%x\n",
                  i, s_mem[i], s_offset[i]);
    }

    gcmResetFlipStatus();
    s_current_buf = 0;
    s_initialized = true;
    SYS_TRACE("[vid_ps3] init done\n");
}

void VID_PS3_Shutdown(void) {
    if (!s_initialized) return;
    for (int i = 0; i < PS3_NUM_BUFFERS; i++) {
        if (s_mem[i]) {
            rsxFree(s_mem[i]);
            s_mem[i] = NULL;
        }
    }
    if (s_io_buffer) {
        free(s_io_buffer);
        s_io_buffer = NULL;
    }
    s_initialized = false;
}

// Scale the 320x200 ARGB source to the display resolution using
// nearest-neighbour sampling (crisp pixels), then flip the display buffer.
void VID_PS3_Present(const void *src_pixels, int src_w, int src_h) {
    if (!s_initialized || !src_pixels) return;

    int next = s_current_buf ^ 1;
    const u32 *src = (const u32 *) src_pixels;
    u32       *dst = (u32 *) s_mem[next];

    // Nearest-neighbour upscale. Integer ratio avoidance keeps it correct
    // for any source / destination size combination.
    for (int y = 0; y < s_display_h; y++) {
        int sy = (y * src_h) / s_display_h;
        const u32 *sr = src + (sy * src_w);
        u32       *dr = dst + (y * s_display_w);
        for (int x = 0; x < s_display_w; x++)
            dr[x] = sr[(x * src_w) / s_display_w];
    }

    // The canonical PSL1GHT flip sequence (from rsx.h quick guide):
    //   1. gcmSetFlip     -- enqueue the flip command
    //   2. rsxFlushBuffer -- push commands to the RSX
    //   3. gcmSetWaitFlip -- insert a "wait for flip" into the command buffer
    // Step 3 is what SDL2's backend omits, causing the 127-frame hang.
    gcmSetFlip(s_context, next);
    rsxFlushBuffer(s_context);
    gcmSetWaitFlip(s_context);

    s_current_buf = next;

    // Block until the flip has actually happened so we never write to a
    // buffer that is still on screen.
    while (gcmGetFlipStatus())
        usleep(200);
    gcmResetFlipStatus();
}

qboolean VID_PS3_IsReady(void) {
    return s_initialized;
}

#endif // CHOCOLATE_QUAKE_PS3
