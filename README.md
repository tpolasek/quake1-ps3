# Dragonfly Quake (PS3 Quake I Port)
<img width="639" height="400" alt="dragonfly_quake" src="https://github.com/user-attachments/assets/ca66afe6-d34c-4ac0-a41c-c02656b04e6e" />


Dragonfly Quake is a faithful source port of Quake 1 (the original DOS release)
for **PlayStation 3 homebrew**. Inspired by DOS Quake, it preserves the
original software renderer at its native 320×200 resolution — no hardware
acceleration, no visual enhancements, just Quake as it was.

It targets the PS3 exclusively through native PSL1GHT APIs: RSX for video,
libaudio for sound, the pad API for input, and `sysGetSystemTime` for the frame
clock. Input is gamepad-only; networking is stubbed, so single-player and demo
playback work but LAN/online play does not.

> The legacy Windows / macOS / Linux desktop build is no longer supported.

## Philosophy

A port for purists: accurate behavior, original bugs and quirks included, the
original feel of Quake v1.09. If you want modern features or visual upgrades,
this isn't it.

## Requirements

- A local **ps3dev (PSL1GHT)** install at `$PS3DEV` providing:
  - the `ppu-gcc` / `ppu-g++` compilers,
  - the PSL1GHT SDK and codec portlibs (vorbis, ogg, mad, FLAC),
  - host signing/packaging tools (`ppu-strip`, `sprxlinker`, `make_self_npdrm`,
    `sfo.py`, `pkg.py`).
- CMake 3.21+ and `make`.
- The Quake 1 data files: an `id1/` directory containing `pak0.pak` and
  `pak1.pak` (from a registered Quake 1 install).
- To deploy over the network: `curl` and an FTP-enabled PS3 on your LAN. To run
  the packaged build: a CFW PS3 or [RPCS3](https://rpcs3.net).

## Build

Put the ps3dev tools on your PATH, then configure and build from the repo root:

```bash
export PATH="$PS3DEV/ppu/bin:$PS3DEV/bin:$PATH"

cmake -S . -B build-ps3 -G "Unix Makefiles" \
      -DCMAKE_TOOLCHAIN_FILE=cmake/ps3.toolchain.cmake
cmake --build build-ps3 -j"$(nproc)"
```

This produces `build-ps3/src/dragonfly-quake` — a statically-linked 64-bit
PowerPC ELF.

## Fast iteration (hotswap EBOOT.BIN over FTP)

After the first full build, you can rebuild, re-sign, and upload just the
executable to a running PS3 without repackaging:

```bash
bash cmake/ps3/dev_deploy.sh
```

Defaults (override via env vars): `PS3_FTP_HOST=192.168.1.245`,
`PS3_FTP_USER=anonymous`, `PS3_FTP_PASS=`,
`PS3_INSTALL_DIR=/dev_hdd0/game/CHQK00001/USRDIR`.

## Package an installable .pkg

Bundle the executable with your `id1/` data into a `.pkg`:

```bash
bash cmake/ps3/make_pkg.sh ./build-ps3/src/dragonfly-quake ~/.quake/id1/ dragonfly-quake.pkg
```

## Running

- **RPCS3**: `File → Install Packages/RAPs/Activators → Install Packages`, then
  select `dragonfly-quake.pkg`.
- **PS3 (CFW)**: copy the `.pkg` to `/dev_hdd0/pkg/` and install it from the XMB
  package manager.

On the console the game reads its data from
`/dev_hdd0/game/CHQK00001/USRDIR` — that's where `make_pkg.sh` installs `id1/`
next to `EBOOT.BIN`.

## Credits

Dragonfly Quake builds on the work of the Quake community and open-source
contributors. Special thanks to:

- [QuakeSpasm Spiked](https://github.com/Shpoike/Quakespasm) — portions of the
  sound and input subsystems are adapted from it.
- [@arrowgent](https://github.com/arrowgent) — thorough testing and bug reports.
- The Dragonfly Quake icon is based on graphics from the
  [EmojiTwo](https://github.com/EmojiTwo/emojitwo) project, licensed under
  [CC-BY-4.0](https://creativecommons.org/licenses/by/4.0/), with modifications
  by [@fpiesche](https://github.com/fpiesche).

## Acknowledgments

Dragonfly Quake is a fork of [Chocolate Quake](https://github.com/Henrique194/chocolate-quake).
All credit for the original source port goes to its authors — this project builds
directly on their work.
