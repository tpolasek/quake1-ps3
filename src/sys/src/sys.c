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


#include "sys.h"
#include "client.h"
#include "cmd.h"
#include "config.h"
#include "end_screen.h"
#include "input.h"
#include "keys.h"
#include "menu.h"
#include <SDL_events.h>
#include <SDL_filesystem.h>
#include <SDL_hints.h>
#include <SDL_stdinc.h>
#include <SDL_timer.h>
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#ifdef CHOCOLATE_QUAKE_PS3
// PS3 sysutil API: lets us intercept the PS button (XMB overlay open/close)
// and the "Quit Game" XMB command so the OS doesn't yank the rug out from
// under us mid-frame.
#include <sysutil/sysutil.h>

// Forward declarations -- the helpers themselves live further down but
// Sys_Error / Sys_Quit reference them.
static void Sys_OpenLog(void);
static void Sys_FlushLog(void);
#endif

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif


qboolean isDedicated;

/*
===============================================================================

FILE IO

===============================================================================
*/

#define MAX_HANDLES 10
FILE* sys_handles[MAX_HANDLES];

i32 findhandle(void) {
    i32 i;

    for (i = 1; i < MAX_HANDLES; i++)
        if (!sys_handles[i])
            return i;
    Sys_Error("out of handles");
    return -1;
}

/*
================
filelength
================
*/
i32 filelength(FILE* f) {
    i32 pos;
    i32 end;

    pos = ftell(f);
    fseek(f, 0, SEEK_END);
    end = ftell(f);
    fseek(f, pos, SEEK_SET);

    return end;
}

i32 Sys_FileOpenRead(char* path, i32* hndl) {
    FILE* f;
    i32 i;

    i = findhandle();

    f = fopen(path, "rb");
    if (!f) {
        *hndl = -1;
        return -1;
    }
    sys_handles[i] = f;
    *hndl = i;

    return filelength(f);
}

i32 Sys_FileOpenWrite(char* path) {
    FILE* f;
    i32 i;

    i = findhandle();

    f = fopen(path, "wb");
    if (!f)
        Sys_Error("Error opening %s: %s", path, strerror(errno));
    sys_handles[i] = f;

    return i;
}

void Sys_FileClose(i32 handle) {
    fclose(sys_handles[handle]);
    sys_handles[handle] = NULL;
}

void Sys_FileSeek(i32 handle, i32 position) {
    fseek(sys_handles[handle], position, SEEK_SET);
}

size_t Sys_FileRead(i32 handle, void* dest, i32 count) {
    return fread(dest, 1, count, sys_handles[handle]);
}

size_t Sys_FileWrite(i32 handle, void* data, i32 count) {
    return fwrite(data, 1, count, sys_handles[handle]);
}

i32 Sys_FileTime(char* path) {
    FILE* f;

    f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return 1;
    }

    return -1;
}

void Sys_mkdir(char* path) {
}


/*
===============================================================================

SYSTEM IO

===============================================================================
*/

static void Sys_ShowErrorModal(const char* msg) {
    u32 flags = SDL_MESSAGEBOX_ERROR;
    const char* title = PACKAGE_STRING;
    SDL_Window* window = NULL;
    SDL_ShowSimpleMessageBox(flags, title, msg, window);
}

void Sys_Error(char* error, ...) {
    va_list argptr;
    char string[1024];

    va_start(argptr, error);
    vsprintf(string, error, argptr);
    va_end(argptr);

    fflush(stdout);
#ifdef CHOCOLATE_QUAKE_PS3
    // stdout is redirected to the log file; route the error there so we
    // can read it back over FTP after the crash.
    fprintf(stdout, "Error: %s\n", string);
    Sys_FlushLog();
#else
    fprintf(stderr, "Error: %s\n", string);
#endif
    Sys_ShowErrorModal(string);

    Host_Shutdown();
    exit(1);
}

void Sys_Printf(char* fmt, ...) {
    va_list argptr;

    va_start(argptr, fmt);
    vprintf(fmt, argptr);
    va_end(argptr);
}

#ifdef CHOCOLATE_QUAKE_PS3
// Backing for the SYS_TRACE macro declared in sys.h. Unbuffered fprintf +
// fflush so we get every step in the log even if the process is killed
// mid-startup before the next newline would have flushed.
void Sys_Trace(const char* fmt, ...) {
    va_list argptr;
    va_start(argptr, fmt);
    vfprintf(stdout, fmt, argptr);
    va_end(argptr);
    fflush(stdout);
}
#endif

void Sys_Quit(void) {
    Host_Shutdown();
#ifdef CHOCOLATE_QUAKE_PS3
    Sys_FlushLog();
#endif
    ES_DisplayScreen();
    exit(0);
}

double Sys_FloatTime() {
    static double frequency = 0.0;
    static u64 start_time = 0;

    if (start_time == 0) {
        frequency = (double) SDL_GetPerformanceFrequency();
        start_time = SDL_GetPerformanceCounter();
        SYS_TRACE("Sys_FloatTime: first call freq=%llu start=%llu -> %.6f\n",
                  (unsigned long long) frequency,
                  (unsigned long long) start_time,
                  (double) start_time / frequency);
        return (double) start_time / frequency;
    }
    u64 now = SDL_GetPerformanceCounter();
    double time_diff = (double) (now - start_time);
    return time_diff / frequency;
}

char* Sys_ConsoleInput(void) {
    return NULL;
}


/*
================================================================================

SYSTEM EVENT POLLING

================================================================================
*/

static void Sys_QuitEvent(void) {
    if (M_IsInQuitScreen()) {
        // Confirm quit.
        Key_Event('Y', true);
        return;
    }
    // Bring up the quit confirmation screen.
    Cmd_ExecuteString("quit", src_command);
}

void Sys_SendKeyEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                IN_KeyboardEvent(&event);
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEWHEEL:
                IN_MouseEvent(&event);
                break;
            case SDL_CONTROLLERDEVICEADDED:
            case SDL_CONTROLLERDEVICEREMOVED:
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP:
            case SDL_CONTROLLERAXISMOTION:
                IN_GamepadEvent(&event);
                break;
            case SDL_QUIT:
                Sys_QuitEvent();
                break;
            default:
                break;
        }
    }
}

//==============================================================================


void Sys_HighFPPrecision(void) {
}

void Sys_LowFPPrecision(void) {
}

//=============================================================================


/*
================================================================================

SIGNAL HANDLING

================================================================================
*/

#ifdef HAVE_SIGNAL_H
static void Sys_SigHandler(int sig) {
    CL_Disconnect();
    Host_ShutdownServer(false);
    Sys_Quit();
}

#ifdef HAVE_SIGACTION
static void Sys_SigHook(int sig) {
    struct sigaction action;
    sigaction(sig, NULL, &action);
    action.sa_handler = Sys_SigHandler;
    sigaction(sig, &action, NULL);
}
#else
static void Sys_SigHook(int sig) {
    signal(sig, Sys_SigHandler);
}
#endif

static void Sys_SigInit(void) {
    // Disable SDL default signal handlers
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    Sys_SigHook(SIGINT);
    Sys_SigHook(SIGTERM);
}
#endif /* HAVE_SIGNAL_H */

//=============================================================================


// Desktop builds happily grab 256 MB. PS3 has only 256 MB of main RAM
// (shared with the OS and SDL2/RSX framebuffer), so cap the heap lower.
#ifdef CHOCOLATE_QUAKE_PS3
#define DEFAULT_MEMORY (128 * 1024 * 1024)
#else
#define DEFAULT_MEMORY (256 * 1024 * 1024)
#endif

#ifdef CHOCOLATE_QUAKE_PS3
// On PS3 there's no stdout/stderr to capture, so write a log file next to
// EBOOT.BIN so we can FTP it back and see where the game died. Opened at
// the very start of Sys_Init; everything that goes through printf /
// Sys_Printf / Sys_Error lands here.
#define PS3_LOG_PATH "/dev_hdd0/game/CHQK00001/USRDIR/chocolate-quake.log"

static void Sys_OpenLog(void) {
    // PS3 newlib has no dup2, so we redirect stdout to the log file via
    // freopen (any printf / Sys_Printf output lands here automatically).
    // Sys_Error writes to stdout rather than stderr so its output is
    // captured too.
    if (!freopen(PS3_LOG_PATH, "w", stdout)) {
        return;
    }
    // Line-buffered so partial lines flush at every newline; if the game
    // hard-crashes mid-line we still get the previous lines.
    setvbuf(stdout, NULL, _IOLBF, 0);
    fprintf(stdout, "=== chocolate-quake PS3 log start ===\n");
}

static void Sys_FlushLog(void) {
    fflush(stdout);
    fflush(stderr);
}

// XMB overlay state. Set by the sysutil callback (fired from
// sysUtilCheckCallback, called on the main thread by Sys_XmbMenuOpen).
// Volatile because the callback runs in the same thread but we want the
// value to be re-read each loop iteration.
static volatile qboolean xmb_menu_open = false;

static void Sys_SysutilCallback(u64 status, u64 param, void* userdata) {
    (void)param;
    (void)userdata;
    switch (status) {
        case SYSUTIL_DRAW_BEGIN:
            // PS button was pressed; XMB overlay is opening. Suspend
            // rendering and game updates until SYSUTIL_DRAW_END.
            xmb_menu_open = true;
            break;
        case SYSUTIL_DRAW_END:
            // User closed the XMB; return to the game.
            xmb_menu_open = false;
            break;
        case SYSUTIL_EXIT_GAME:
            // User picked "Quit Game" from the XMB.
            fprintf(stderr, "SYSUTIL_EXIT_GAME received -- exiting\n");
            Sys_FlushLog();
            Host_Shutdown();
            exit(0);
        default:
            break;
    }
}

qboolean Sys_XmbMenuOpen(void) {
    // Pump sysutil events so the callback above fires.
    sysUtilCheckCallback();
    return xmb_menu_open;
}
#endif

static char* Sys_GetDefaultBaseDir(void) {
#ifdef _WIN32
    return ".";
#elif defined(CHOCOLATE_QUAKE_PS3)
    // On PS3, the .pkg ships id1/ next to EBOOT.BIN under the title's
    // USRDIR (e.g. /dev_hdd0/game/<TITLE_ID>/USRDIR/). The PSL1GHT SDL2
    // build returns NULL from SDL_GetBasePath(), so we hardcode the
    // title's USRDIR -- that's where make_pkg.sh installs id1/ alongside
    // EBOOT.BIN.
    #define PS3_BASE_DIR "/dev_hdd0/game/CHQK00001/USRDIR"
    SYS_TRACE("Sys_GetDefaultBaseDir: PS3 hardcoding '%s'\n", PS3_BASE_DIR);
    return PS3_BASE_DIR;
#else
    static char base_dir[MAX_OSPATH] = {0};
    if (base_dir[0]) {
        return base_dir;
    }
    char* path = SDL_GetPrefPath("", PACKAGE_TARNAME);
    Q_strncpy(base_dir, path, MAX_OSPATH);
    SDL_free(path);
    return base_dir;
#endif
}

static quakeparms_t* Sys_InitParms(i32 argc, char** argv) {
    static quakeparms_t parms;

    SYS_TRACE("Sys_InitParms: enter (DEFAULT_MEMORY=%u)\n",
              (unsigned) DEFAULT_MEMORY);
    parms.memsize = DEFAULT_MEMORY;
    SYS_TRACE("Sys_InitParms: calling Q_malloc(%u)\n",
              (unsigned) parms.memsize);
    parms.membase = Q_malloc(parms.memsize);
    SYS_TRACE("Sys_InitParms: Q_malloc returned %p\n", (void*) parms.membase);
    if (!parms.membase) {
        SYS_TRACE("Sys_InitParms: heap allocation FAILED\n");
    }
    SYS_TRACE("Sys_InitParms: calling Sys_GetDefaultBaseDir\n");
    parms.basedir = Sys_GetDefaultBaseDir();
    SYS_TRACE("Sys_InitParms: basedir='%s'\n",
              parms.basedir ? parms.basedir : "(null)");

    SYS_TRACE("Sys_InitParms: calling COM_InitArgv\n");
    COM_InitArgv(argc, argv);
    SYS_TRACE("Sys_InitParms: COM_InitArgv done, com_argc=%d\n", com_argc);
    parms.argc = com_argc;
    parms.argv = com_argv;

    SYS_TRACE("Sys_InitParms: done\n");
    return &parms;
}

quakeparms_t* Sys_Init(i32 argc, char* argv[]) {
#ifdef CHOCOLATE_QUAKE_PS3
    // Open the log file before anything else can fail, so we capture
    // startup messages and any Sys_Error output.
    SYS_TRACE("Sys_Init: opening log\n");
    Sys_OpenLog();
    SYS_TRACE("Sys_Init: log open, registering sysutil callback\n");
    // Register the sysutil callback so we see PS button (XMB open/close)
    // and "Quit Game" events. Slot 0 is fine -- we're the only consumer.
    if (sysUtilRegisterCallback(0, Sys_SysutilCallback, NULL) != 0) {
        fprintf(stderr, "sysUtilRegisterCallback failed\n");
    }
    SYS_TRACE("Sys_Init: sysutil callback registered\n");
#endif
#ifdef HAVE_SIGNAL_H
    SYS_TRACE("Sys_Init: calling Sys_SigInit\n");
    Sys_SigInit();
    SYS_TRACE("Sys_Init: signals set up\n");
#endif
    SYS_TRACE("Sys_Init: calling Sys_InitParms\n");
    quakeparms_t* parms = Sys_InitParms(argc, argv);
    SYS_TRACE("Sys_Init: returning parms=%p\n", (void*) parms);
    return parms;
}
