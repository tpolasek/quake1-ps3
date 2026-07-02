#!/usr/bin/env bash
#
# Packages the dragonfly-quake PS3 (PPU) executable into an installable
# .pkg. By default it also bundles the Quake 1 id1/ data files alongside
# EBOOT.BIN; omit <id1_dir> to ship the executable only (e.g. when the
# data is already installed on the console).
#
# Run this from the host with the ps3dev tools (ppu-strip, sprxlinker,
# make_self_npdrm, sfo.py, pkg.py) on PATH -- e.g. with $PS3DEV set and
# $PS3DEV/ppu/bin:$PS3DEV/bin prepended to PATH. Paths are relative to
# the repo root.
#
# Usage:
#   cmake/ps3/make_pkg.sh <elf> [id1_dir] [out_pkg]
#
#   <elf>      dragonfly-quake PS3 executable (ELF, PowerPC)
#   <id1_dir>  optional directory containing pak0.pak / pak1.pak; when
#               empty or omitted, no id1/ data is bundled
#   <out_pkg>  output .pkg path
#
# Defaults match the dragonfly-quake layout.

set -euo pipefail

ELF="${1:-build-ps3/src/dragonfly-quake}"
ID1_DIR="${2:-}"
OUT_PKG="${3:-dragonfly-quake.pkg}"

# Title metadata. CONTENT_ID format: 2-char flag + 4 hex + '-' + 9-char
# TITLE_ID + "_00-" + 16 hex.
TITLE="Dragonfly Quake"
TITLE_ID="CHQK00001"
APP_VER="01.00"
CONTENT_ID="UP0000-${TITLE_ID}_00-0000000000000001"

WORK="$(mktemp -d)"
PKG_DIR="${WORK}/pkg"
USRDIR="${PKG_DIR}/USRDIR"
mkdir -p "${USRDIR}"

# Total step count depends on whether id1/ is bundled.
TOTAL=5
[ -n "${ID1_DIR}" ] && TOTAL=6
N=0
step() { N=$((N+1)); echo "[${N}/${TOTAL}] $1"; }

step "Verifying inputs"
[ -f "${ELF}" ] || { echo "missing ELF: ${ELF}" >&2; exit 1; }
if [ -n "${ID1_DIR}" ]; then
    [ -f "${ID1_DIR}/pak0.pak" ] || { echo "missing pak0.pak in ${ID1_DIR}" >&2; exit 1; }
fi

step "Stripping + sprxlinker -> ${WORK}/dragonfly-quake.elf"
cp "${ELF}" "${WORK}/cq.raw"
ppu-strip -o "${WORK}/dragonfly-quake.elf" "${WORK}/cq.raw"
sprxlinker "${WORK}/dragonfly-quake.elf"

step "make_self_npdrm -> pkg/USRDIR/EBOOT.BIN"
make_self_npdrm "${WORK}/dragonfly-quake.elf" "${USRDIR}/EBOOT.BIN" "${CONTENT_ID}"

if [ -n "${ID1_DIR}" ]; then
    step "Copying id1/ data into pkg/USRDIR/id1"
    mkdir -p "${USRDIR}/id1"
    cp -a "${ID1_DIR}/." "${USRDIR}/id1/"
fi

step "Generating PARAM.SFO"
SFO_XML="${WORK}/sfo.xml"
cp "${PS3DEV:-/usr/local/ps3dev}/bin/sfo.xml" "${SFO_XML}"
sed -i "s/01\.00/${APP_VER}/g" "${SFO_XML}"
sfo.py --title "${TITLE}" --appid "${TITLE_ID}" -f "${SFO_XML}" "${PKG_DIR}/PARAM.SFO"

step "pkg.py -> ${OUT_PKG}"
pkg.py --contentid "${CONTENT_ID}" "${PKG_DIR}/" "${OUT_PKG}"

echo
echo "Done."
echo "  Title:       ${TITLE}"
echo "  Title ID:    ${TITLE_ID}"
echo "  App version: ${APP_VER}"
echo "  Content ID:  ${CONTENT_ID}"
echo "  Package:     ${OUT_PKG} ($(stat -c%s "${OUT_PKG}") bytes)"

rm -rf "${WORK}"
