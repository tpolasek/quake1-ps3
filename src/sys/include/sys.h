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
// sys.h -- non-portable functions

#ifndef __SYS__
#define __SYS__

#include "quakedef.h"
#include "host.h"


// Suppresses resolution and cache size console output.
// An fullscreen DIB focus gain/loss.
extern qboolean msg_suppress_1;

extern qboolean isDedicated;


//
// file IO
//

// returns the file size
// return -1 if file is not present
// the file should be in BINARY mode for stupid OSs that care
i32 Sys_FileOpenRead(char* path, i32* hndl);

i32 Sys_FileOpenWrite(char* path);

void Sys_FileClose(i32 handle);

void Sys_FileSeek(i32 handle, i32 position);

size_t Sys_FileRead(i32 handle, void* dest, i32 count);

size_t Sys_FileWrite(i32 handle, void* data, i32 count);

i32 Sys_FileTime(char* path);

void Sys_mkdir(char* path);

//
// an error will cause the entire program to exit
//
void Sys_Error(char* error, ...);

//
// send text to the console
//
void Sys_Printf(char* fmt, ...);

void Sys_Quit(void);

double Sys_FloatTime();

char* Sys_ConsoleInput(void);

//
// Perform Key_Event () callbacks until the input que is empty
//
void Sys_SendKeyEvents();

void Sys_LowFPPrecision(void);

void Sys_HighFPPrecision(void);

// PS3 only: returns true while the XMB overlay is open (PS button pressed).
// Calls sysUtilCheckCallback internally so the sysutil callback fires on the
// calling thread. On non-PS3 builds this is always false and the call is a
// no-op, so the main loop stays unchanged.
qboolean Sys_XmbMenuOpen(void);

// PS3-only startup tracing. Writes immediately (fprintf + fflush) to the log
// file opened by Sys_OpenLog, so we capture the last successful step even if
// the process hard-crashes mid-startup. Compiles to nothing elsewhere so
// desktop builds stay quiet. Inlined as a macro (rather than a call to a
// Sys_Trace function) so each call site emits its own fprintf/fflush and we
// don't have to chase static-library link order across the various lib*.a
// archives that make up the executable.
//
// SYS_TRACE is opt-in: it compiles to nothing unless SYS_TRACE_ACTIVE is
// defined before sys.h is included. The PS3 log was getting drowned by
// ~130 trace call sites spread across the codebase, making it hard to
// debug specific subsystems. To enable tracing in a file, add
// `#define SYS_TRACE_ACTIVE 1` at the top before any #include.
#ifdef CHOCOLATE_QUAKE_PS3
#include <stdio.h>
#ifdef SYS_TRACE_ACTIVE
#define SYS_TRACE(...) do { \
    fprintf(stdout, "[trace] " __VA_ARGS__); \
    fflush(stdout); \
} while (0)
#else
#define SYS_TRACE(...) ((void)0)
#endif
#else
#define SYS_TRACE(...) ((void)0)
#endif

quakeparms_t* Sys_Init(i32 argc, char* argv[]);

#endif