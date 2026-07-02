# Dragonfly Quake

Dragonfly Quake is a faithful source port of Quake 1 (DOS version) that preserves the
original software renderer at its original 320x200 resolution. The port targets PS3
homebrew exclusively (PSL1GHT/PS3DEV toolchain, powerpc64-ps3-elf target) and talks to
the hardware through native PSL1GHT APIs -- RSX for video, libaudio for sound, the pad
API for input, and `sysGetSystemTime` for the frame clock. The earlier desktop
(Linux/macOS/Windows) builds have been removed.

## Source layout

Each Quake subsystem is its own static library under `src/<sub>/{src,include}/`. The
top-level `src/CMakeLists.txt` aggregates them into the `dragonfly-quake` executable.
Key subsystems: `host` (frame loop, `Host_Init`/`Host_Frame`), `common` (filesystem,
pak loading, byte swap), `sys` (PS3 logger, sysutil callback, `Sys_Error`/`Sys_Printf`),
`renderer` (software rasterizer), `camera` (view setup), `screen` (SCR_UpdateScreen),
`video` (native RSX framebuffer + 8-to-32-bit palette expansion), `input` (gamepad-only
via the PSL1GHT pad API), `sound` (native PSL1GHT libaudio, int16-to-float32 DMA),
`client`, `server`, `progs` (QuakeC VM).

## PS3 build & deploy

Requires a local ps3dev (PSL1GHT) install at `$PS3DEV` providing the `ppu-gcc`
compilers, the PSL1GHT SDK + codec portlibs, and the host signing tools
(`ppu-strip`, `sprxlinker`, `make_self_npdrm`, `sfo.py`, `pkg.py`). Put the
binaries on PATH and run **from the host**:

```bash
# Fast iteration: rebuild PPU binary, sign as NPDRM, upload EBOOT.BIN to PS3 via FTP.
# Reuses the existing build-ps3/ tree. Run a full build first (below) before the
# first dev_deploy.sh invocation -- it sanity-checks the artifact exists.
bash cmake/ps3/dev_deploy.sh

# One-time full build (configures build-ps3/ from scratch):
export PATH="$PS3DEV/ppu/bin:$PS3DEV/bin:$PATH"
cmake -S . -B build-ps3 -G "Unix Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE=cmake/ps3.toolchain.cmake
cmake --build build-ps3 -j"$(nproc)"

# Package a full installable .pkg (bundles id1/ + EBOOT.BIN):
bash cmake/ps3/make_pkg.sh build-ps3/src/dragonfly-quake <path-to-id1> dragonfly-quake.pkg
```

Env vars for `dev_deploy.sh` (all optional, defaults shown):

```
PS3_FTP_HOST=192.168.1.245
PS3_FTP_USER=anonymous
PS3_FTP_PASS=
PS3_INSTALL_DIR=/dev_hdd0/game/CHQK00001/USRDIR
```

## PS3 runtime log

The PS3 build redirects stdout to a log file next to EBOOT.BIN. Fetch it via FTP:

```bash
curl -s --max-time 20 \
    'ftp://192.168.1.245/dev_hdd0/game/CHQK00001/USRDIR/dragonfly-quake.log' \
    --user 'anonymous:'
```

To clear the log between runs, just relaunch the game -- `Sys_OpenLog` reopens with
`"w"` so the log is truncated on each launch.

## PS3-specific gotchas

- **Stack size.** The OS-spawned EBOOT main thread has only ~128 KB of stack. The
  game runs on a 2 MB worker PPU thread spawned by `main()` via `sysThreadCreate`
  (see `src/main.c`). Several Quake functions allocate large stack arrays that would
  otherwise overflow (`R_AliasDrawModel` ~88 KB, `R_RenderWorld` up to ~80 KB,
  `COM_LoadPackFile` 128 KB -- last one is `static` to avoid the issue too).
- **Base directory.** PSL1GHT exposes no base-path API, so
  `Sys_GetDefaultBaseDir` hardcodes `/dev_hdd0/game/CHQK00001/USRDIR` -- where
  `make_pkg.sh` installs `id1/` next to `EBOOT.BIN`. Change the `TITLE_ID` in
  `cmake/ps3/make_pkg.sh` and this constant together.
- **Tracing.** `SYS_TRACE(...)` is a macro that does `fprintf(stdout, ...)` followed by
  `fflush` so the last successful step is captured even on hard crash. It compiles to
  nothing unless `SYS_TRACE_ACTIVE` is defined before `sys.h` is included (opt-in per
  translation unit). Defined in `src/sys/include/sys.h`. Files using it must include
  `sys.h`; if a new translation unit hits "undefined reference to SYS_TRACE" or
  "sys.h not found", add `target_include_directories(<target> PRIVATE ../sys/include)`
  to its `CMakeLists.txt`.
- **Networking.** Unavailable. `src/net/` still compiles but links against a local
  stub (`cmake/ps3/ps3_net_stub/`) that provides a UDP socket/packet API; every call
  fails gracefully. Demo playback and single-player work.
- **XMB / PS button.** `sysUtilRegisterCallback` is registered in `Sys_Init`. The
  worker thread's frame loop calls `Sys_XmbMenuOpen()` each frame, which is the sole
  pumper of `sysUtilCheckCallback` -- so the sysutil callback fires on the worker.
  `Sys_XmbMenuOpen()` returns true while the XMB overlay is up, and the worker parks
  its loop (`usleep` until `SYSUTIL_DRAW_END`). The callback runs `Host_Shutdown()` +
  `exit(0)` on `SYSUTIL_EXIT_GAME`. `main()` just joins the worker, then
  `sysProcessExit`.
- **`dev_deploy.sh` shows full build errors on failure** (the in-container script
  captures cmake output to a temp file and cats it on non-zero exit).

## Desktop build

Removed. The port is PS3-only: the subsystem libraries talk directly to PSL1GHT,
the end-screen module and keyboard/mouse input were deleted, and the link line binds
the runtime libs directly (`-lgcm_sys -lrsx -lsysutil -lio -laudio -lrt -llv2 -lm`).
The top-level `CMakeLists.txt` no longer has a desktop branch, the vcpkg submodule
and manifest are gone, and the desktop-only CI workflow has been deleted. Use the
PS3 build above.
