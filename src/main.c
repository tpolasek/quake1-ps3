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


#include "host.h"
#include "sys.h"
#include <SDL_main.h>

#ifdef CHOCOLATE_QUAKE_PS3
#include <unistd.h>
#include <sys/thread.h>
#include <sys/process.h>

// The PS3 OS spawns the EBOOT main thread with a fixed ~128 KB stack.
// That's smaller than several Quake functions' stack-allocated arrays
// (e.g. R_AliasDrawModel uses ~88 KB, R_RenderWorld up to ~80 KB,
// COM_LoadPackFile 128 KB), so they would silently blow the stack and
// crash before their first line of code ran. To get a bigger stack we
// spawn a worker PPU thread with a 2 MB stack and run the entire game
// there. main() just joins it.
#define PS3_GAME_STACK  (2 * 1024 * 1024)
#define PS3_GAME_PRIO   1000

static sys_ppu_thread_t g_game_thread_id;
static int              g_game_argc;
static char**           g_game_argv;

// The actual game entry point -- what main() used to do directly.
static void Quake_Run(int argc, char** argv) {
    quakeparms_t* parms = Sys_Init(argc, argv);
    SYS_TRACE("Quake_Run: Sys_Init returned (parms=%p), calling Host_Init\n",
              (void*) parms);
    Host_Init(parms);
    SYS_TRACE("Quake_Run: Host_Init returned, entering frame loop\n");

    double old_time = Sys_FloatTime();
    while (true) {
        // PS3: if the XMB overlay is open (PS button), park the loop so
        // we don't fight the OS for the framebuffer or waste CPU.
        if (Sys_XmbMenuOpen()) {
            usleep(16000);
            continue;
        }
        double new_time = Sys_FloatTime();
        double dt = new_time - old_time;
        Host_Frame((float) dt);
        old_time = new_time;
    }
}

static void Quake_GameThread(void* arg) {
    (void)arg;
    Quake_Run(g_game_argc, g_game_argv);
    sysThreadExit(0);
}
#endif


int main(int argc, char* argv[]) {
#ifdef CHOCOLATE_QUAKE_PS3
    g_game_argc = argc;
    g_game_argv = argv;
    s32 rc = sysThreadCreate(&g_game_thread_id, Quake_GameThread, NULL,
                             PS3_GAME_PRIO, PS3_GAME_STACK,
                             0, "quake_game");
    if (rc != 0) {
        // Cannot use SYS_TRACE here -- Sys_OpenLog hasn't run yet (it
        // runs inside Sys_Init on the worker thread). Last-resort
        // stderr; will not be visible on PS3 but is harmless.
        fprintf(stderr, "sysThreadCreate failed: %d\n", (int) rc);
        return 1;
    }
    // NOTE: We deliberately do NOT join the worker thread. When the game
    // freezes (the whole reason we're debugging), joining would block
    // forever and force a hard console reboot. Instead, sleep a fixed
    // watchdog timeout and then terminate the process -- the OS reaps
    // the still-running worker thread. The PS-button XMB exit path
    // (sysProcessExit from the sysutil callback) still fires immediately
    // and bypasses this sleep.
#define PS3_WATCHDOG_SECONDS  20
    usleep(PS3_WATCHDOG_SECONDS * 1000 * 1000);
    SYS_TRACE("main: watchdog (%d s) expired, force-exiting process\n",
              PS3_WATCHDOG_SECONDS);
    sysProcessExit(1);
    return 1; // unreachable
#else
    quakeparms_t* parms = Sys_Init(argc, argv);
    Host_Init(parms);

    double old_time = Sys_FloatTime();
    while (true) {
        double new_time = Sys_FloatTime();
        double dt = new_time - old_time;
        Host_Frame((float) dt);
        old_time = new_time;
    }
#endif
}
