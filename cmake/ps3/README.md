# Building `chocolate-quake.pkg` for PS3

This produces an installable PS3 package (.pkg) from the chocolate-quake
source, cross-compiling for the PS3's PPU (PowerPC 64-bit) using the
[hldtux/ps3dev-sdl2](https://hub.docker.com/r/hldtux/ps3dev-sdl2) Docker
image. Tested with CFW PS3 consoles and RPCS3.

## Prerequisites

- Docker, with the user in the `docker` group (or run commands via `sudo`).
- The Quake 1 data files: a directory containing `pak0.pak` and `pak1.pak`
  (typically the `id1/` folder from a registered Quake 1 install).

No local PS3 toolchain is required — everything runs inside the container.

## 1. Build the PS3 (PPU) executable

From the repository root:

```bash
sg docker -c "docker run --rm --platform linux/amd64 \
  -v $(pwd):/build -w /build hldtux/ps3dev-sdl2 bash -c '
    export PATH=/usr/local/ps3dev/ppu/bin:\$PATH
    cmake -S . -B build-ps3 \
      -DCMAKE_TOOLCHAIN_FILE=cmake/ps3.toolchain.cmake \
      -DCMAKE_BUILD_TYPE=Release
    cmake --build build-ps3 -j\$(nproc)
  '"
```

This produces `build-ps3/src/chocolate-quake` — an ELF 64-bit MSB
PowerPC executable, statically linked.

For incremental rebuilds after the first configure:

```bash
sg docker -c "docker run --rm --platform linux/amd64 \
  -v $(pwd):/build -w /build hldtux/ps3dev-sdl2 bash -c '
    export PATH=/usr/local/ps3dev/ppu/bin:\$PATH
    cmake --build build-ps3 -j\$(nproc)
  '"
```

## 2. Build the `.pkg`

Mount the Quake `id1/` data directory read-only at `/host-id1` inside
the container, then run the packaging script:

```bash
sg docker -c "docker run --rm --platform linux/amd64 \
  -v $(pwd):/build -w /build \
  -v /path/to/id1:/host-id1:ro \
  hldtux/ps3dev-sdl2 ./cmake/ps3/make_pkg.sh"
```

Defaults (override by passing positional args to `make_pkg.sh`):
- ELF input: `build-ps3/src/chocolate-quake`
- id1 source: `/host-id1`
- Output: `chocolate-quake.pkg`

The script:
1. `ppu-strip` + `sprxlinker` on the ELF
2. `make_self_npdrm` -> `pkg/USRDIR/EBOOT.BIN`
3. Copies the `id1/` data into `pkg/USRDIR/id1/`
4. Generates `PARAM.SFO` from the ps3dev `sfo.xml` template
5. `pkg.py` -> final `chocolate-quake.pkg`

### Package metadata

| Field        | Value                                   |
|--------------|-----------------------------------------|
| Title        | Chocolate Quake                         |
| Title ID     | CHQK00001                               |
| App version  | 01.00                                   |
| Content ID   | UP0000-CHQK00001_00-0000000000000001    |

To change these, edit `TITLE`, `TITLE_ID`, `APP_VER`, `CONTENT_ID` at the
top of `cmake/ps3/make_pkg.sh`.

## 3. Install / test

- **RPCS3**: `File → Install Packages/RAPs/Activators → Install Packages`
  and select `chocolate-quake.pkg`.
- **PS3 (CFW)**: copy the .pkg to `/dev_hdd0/pkg/` and install from the XMB
  package manager.

## Files added for the PS3 port

| File                                | Purpose                                  |
|-------------------------------------|------------------------------------------|
| `cmake/ps3.toolchain.cmake`         | CMake toolchain (ppu-gcc, ps3dev sysroot)|
| `cmake/ps3/sdl2_net_stub/`          | SDL-free `SDL_net` API stub (ps3dev has  |
|                                     | no SDL2_net); `src/net/` links here      |
| `cmake/ps3/make_pkg.sh`             | Packaging script                         |

### Native PSL1GHT backends (SDL2 removed)

The port is PS3-only and uses PSL1GHT APIs directly -- there is no SDL in the
source, the build, or the link line. Backends:

| Subsystem | Backend                                                                |
|-----------|------------------------------------------------------------------------|
| video     | Native RSX via `src/video/src/vid_ps3.c`; the 8-bit framebuffer is     |
|           | expanded to ARGB on the PPU, then upscaled and flipped through gcm     |
| sound     | Native libaudio via `src/sound/src/snd_ps3.c`; block-based float32 DMA |
|           | with event-queue sync, int16 mix -> float32 in `SNDDMA_Submit`         |
| input     | PSL1GHT pad API polled each frame in `src/input/src/in_gamepad.c`      |
|           | (pad-only -- keyboard and mouse are gone)                              |
| timer     | `sysGetSystemTime()` in `Sys_FloatTime` (no SDL timer init)            |

The top-level `CMakeLists.txt` links the PSL1GHT runtime libs directly:
`-lgcm_sys -lrsx -lsysutil -lio -laudio -lrt -llv2 -lm`.

## Notes / caveats

- The container runs as root, so `build-ps3/` and `chocolate-quake.pkg`
  will be root-owned. Reclaim with:
  ```bash
  sudo chown -R $USER:$USER build-ps3 chocolate-quake.pkg
  ```
- **Networking is stubbed.** `src/net/` links against an SDL-free `SDL_net`
  API stub (`cmake/ps3/sdl2_net_stub/`); every `SDLNet_*` call fails
  gracefully. LAN/online multiplayer will not work; single-player and demo
  playback should.
- **Runtime data path.** `Sys_GetDefaultBaseDir` hardcodes
  `/dev_hdd0/game/CHQK00001/USRDIR` as the basedir (where `make_pkg.sh`
  installs `id1/` next to `EBOOT.BIN`). If you change the title ID, update
  both `cmake/ps3/make_pkg.sh` and that constant in `src/sys/src/sys.c`.
