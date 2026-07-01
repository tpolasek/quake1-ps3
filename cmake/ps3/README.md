# Building `chocolate-quake.pkg` for PS3

This produces an installable PS3 package (.pkg) from the chocolate-quake
source, cross-compiling for the PS3's PPU (PowerPC 64-bit) using a local
PSL1GHT/ps3dev toolchain. Tested with CFW PS3 consoles and RPCS3.

## Prerequisites

- A local ps3dev (PSL1GHT) install at `$PS3DEV` providing `ppu-gcc`, the
  PSL1GHT SDK + codec portlibs (vorbis/ogg/mad/FLAC), and the host signing
  tools (`ppu-strip`, `sprxlinker`, `make_self_npdrm`, `sfo.py`, `pkg.py`).
  Put `$PS3DEV/ppu/bin` and `$PS3DEV/bin` on PATH.
- The Quake 1 data files: a directory containing `pak0.pak` and `pak1.pak`
  (typically the `id1/` folder from a registered Quake 1 install).

## 1. Build the PS3 (PPU) executable

From the repository root:

```bash
export PATH="$PS3DEV/ppu/bin:$PS3DEV/bin:$PATH"
cmake -S . -B build-ps3 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/ps3.toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-ps3 -j"$(nproc)"
```

This produces `build-ps3/src/chocolate-quake` — an ELF 64-bit MSB
PowerPC executable, statically linked.

For incremental rebuilds after the first configure:

```bash
cmake --build build-ps3 -j"$(nproc)"
```

## 2. Build the `.pkg`

Run the packaging script, pointing it at the ELF and the Quake `id1/`
data directory:

```bash
bash cmake/ps3/make_pkg.sh build-ps3/src/chocolate-quake /path/to/id1 chocolate-quake.pkg
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
| `cmake/ps3/ps3_net_stub/`           | PS3 networking stub (no usable net stack |
|                                     | on ps3dev); `src/net/` links here        |
| `cmake/ps3/make_pkg.sh`             | Packaging script                         |

### Native PSL1GHT backends

The port is PS3-only and uses PSL1GHT APIs directly. Backends:

| Subsystem | Backend                                                                |
|-----------|------------------------------------------------------------------------|
| video     | Native RSX via `src/video/src/vid_ps3.c`; the 8-bit framebuffer is     |
|           | expanded to ARGB on the PPU, then upscaled and flipped through gcm     |
| sound     | Native libaudio via `src/sound/src/snd_ps3.c`; block-based float32 DMA |
|           | with event-queue sync, int16 mix -> float32 in `SNDDMA_Submit`         |
| input     | PSL1GHT pad API polled each frame in `src/input/src/in_gamepad.c`      |
|           | (pad-only -- keyboard and mouse are gone)                              |
| timer     | `sysGetSystemTime()` in `Sys_FloatTime` (native, no init step)          |

The top-level `CMakeLists.txt` links the PSL1GHT runtime libs directly:
`-lgcm_sys -lrsx -lsysutil -lio -laudio -lrt -llv2 -lm`.

## Notes / caveats

- **Networking is stubbed.** `src/net/` links against a PS3 networking stub
  (`cmake/ps3/ps3_net_stub/`); every `PS3Net_*` call fails gracefully.
  LAN/online multiplayer will not work; single-player and demo playback
  should.
- **Runtime data path.** `Sys_GetDefaultBaseDir` hardcodes
  `/dev_hdd0/game/CHQK00001/USRDIR` as the basedir (where `make_pkg.sh`
  installs `id1/` next to `EBOOT.BIN`). If you change the title ID, update
  both `cmake/ps3/make_pkg.sh` and that constant in `src/sys/src/sys.c`.
