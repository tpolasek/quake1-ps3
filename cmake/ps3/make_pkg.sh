#!/usr/bin/env bash
#
# Packages the chocolate-quake PS3 (PPU) executable into an installable
# .pkg, bundling the Quake 1 id1/ data files alongside EBOOT.BIN.
#
# Run this *inside* the ps3dev-sdl2 docker container (or any ps3dev
# environment that has ppu-strip, sprxlinker, make_self_npdrm, sfo.py,
# pkg.py on PATH). Paths are relative to the repo root.
#
# Usage:
#   cmake/ps3/make_pkg.sh <elf> <id1_dir> <out_pkg>
#
#   <elf>      chocolate-quake PS3 executable (ELF, PowerPC)
#   <id1_dir>  directory containing pak0.pak / pak1.pak
#   <out_pkg>  output .pkg path
#
# Defaults match the chocolate-quake layout.

set -euo pipefail

ELF="${1:-build-ps3/src/chocolate-quake}"
ID1_DIR="${2:-/host-id1}"
OUT_PKG="${3:-chocolate-quake.pkg}"

# Title metadata. CONTENT_ID format: 2-char flag + 4 hex + '-' + 9-char
# TITLE_ID + "_00-" + 16 hex.
TITLE="Chocolate Quake"
TITLE_ID="CHQK00001"
APP_VER="01.00"
CONTENT_ID="UP0000-${TITLE_ID}_00-0000000000000001"

WORK="$(mktemp -d)"
PKG_DIR="${WORK}/pkg"
USRDIR="${PKG_DIR}/USRDIR"
mkdir -p "${USRDIR}"

echo "[1/6] Verifying inputs"
[ -f "${ELF}" ] || { echo "missing ELF: ${ELF}" >&2; exit 1; }
[ -f "${ID1_DIR}/pak0.pak" ] || { echo "missing pak0.pak in ${ID1_DIR}" >&2; exit 1; }

echo "[2/6] Stripping + sprxlinker -> ${WORK}/chocolate-quake.elf"
cp "${ELF}" "${WORK}/cq.raw"
ppu-strip -o "${WORK}/chocolate-quake.elf" "${WORK}/cq.raw"
sprxlinker "${WORK}/chocolate-quake.elf"

echo "[3/6] make_self_npdrm -> pkg/USRDIR/EBOOT.BIN"
make_self_npdrm "${WORK}/chocolate-quake.elf" "${USRDIR}/EBOOT.BIN" "${CONTENT_ID}"

echo "[4/6] Copying id1/ data into pkg/USRDIR/id1"
mkdir -p "${USRDIR}/id1"
cp -a "${ID1_DIR}/." "${USRDIR}/id1/"

echo "[5/6] Generating PARAM.SFO"
SFO_XML="${WORK}/sfo.xml"
cp /usr/local/ps3dev/bin/sfo.xml "${SFO_XML}"
sed -i "s/01\.00/${APP_VER}/g" "${SFO_XML}"
sfo.py --title "${TITLE}" --appid "${TITLE_ID}" -f "${SFO_XML}" "${PKG_DIR}/PARAM.SFO"

echo "[6/6] pkg.py -> ${OUT_PKG}"
pkg.py --contentid "${CONTENT_ID}" "${PKG_DIR}/" "${OUT_PKG}"

echo
echo "Done."
echo "  Title:       ${TITLE}"
echo "  Title ID:    ${TITLE_ID}"
echo "  App version: ${APP_VER}"
echo "  Content ID:  ${CONTENT_ID}"
echo "  Package:     ${OUT_PKG} ($(stat -c%s "${OUT_PKG}") bytes)"

rm -rf "${WORK}"
