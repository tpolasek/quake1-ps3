#!/usr/bin/env bash
#
# Fast PS3 dev iteration: rebuild the PPU binary, sign it as NPDRM, and
# upload EBOOT.BIN over FTP to the running PS3. No .pkg rebuild, no
# reinstall -- just hotswap the executable.
#
# Run this from the host. Requires a local ps3dev install (PSL1GHT) at
# $PS3DEV providing ppu-gcc, ppu-strip, sprxlinker, make_self_npdrm on
# PATH. Usage:
#
#   ./cmake/ps3/dev_deploy.sh
#
# Override the destination by setting env vars:
#
#   PS3_FTP_HOST=192.168.1.245 \
#   PS3_FTP_USER=anonymous \
#   PS3_FTP_PASS= \
#   PS3_INSTALL_DIR=/dev_hdd0/game/CHQK00001/USRDIR \
#   ./cmake/ps3/dev_deploy.sh

set -euo pipefail

# --- config (override via env) ------------------------------------------------
PS3_FTP_HOST="${PS3_FTP_HOST:-192.168.1.245}"
PS3_FTP_USER="${PS3_FTP_USER:-anonymous}"
PS3_FTP_PASS="${PS3_FTP_PASS:-}"
PS3_INSTALL_DIR="${PS3_INSTALL_DIR:-/dev_hdd0/game/CHQK00001/USRDIR}"
CONTENT_ID="${CONTENT_ID:-UP0000-CHQK00001_00-0000000000000001}"
LOCAL_ELF="${LOCAL_ELF:-build-ps3/src/chocolate-quake}"
PS3DEV="${PS3DEV:-/usr/local/ps3dev}"

# Put the ps3dev compilers + host signing tools on PATH.
export PATH="$PS3DEV/ppu/bin:$PS3DEV/bin:$PATH"

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO_ROOT"

# --- sanity checks ------------------------------------------------------------
[ -f "$LOCAL_ELF" ] || {
    echo "Missing $LOCAL_ELF -- run a full PS3 build first." >&2
    exit 1
}
for tool in ppu-gcc ppu-strip sprxlinker make_self_npdrm curl; do
    command -v "$tool" >/dev/null || {
        echo "$tool not found on PATH (expected under \$PS3DEV)." >&2
        echo "PS3DEV=$PS3DEV -- check your local ps3dev install." >&2
        exit 1
    }
done

# --- [1/4] Rebuild PPU binary -------------------------------------------------
echo "[1/4] Rebuilding PPU binary"
BUILD_LOG="$(mktemp)"
trap 'rm -f "$BUILD_LOG"' EXIT
if ! cmake --build build-ps3 -j"$(nproc)" >"$BUILD_LOG" 2>&1; then
    echo "=== BUILD FAILED -- full output: ==="
    cat "$BUILD_LOG"
    exit 1
fi

# --- [2/4] Sign EBOOT.BIN -----------------------------------------------------
echo "[2/4] Signing EBOOT.BIN (ppu-strip + sprxlinker + make_self_npdrm)"
rm -f /tmp/cq.elf /tmp/EBOOT.BIN
ppu-strip -o /tmp/cq.elf "$LOCAL_ELF"
sprxlinker /tmp/cq.elf
make_self_npdrm /tmp/cq.elf /tmp/EBOOT.BIN "$CONTENT_ID"
ls -la /tmp/EBOOT.BIN

# --- [3/4] Upload over FTP ----------------------------------------------------
echo "[3/4] Uploading to ftp://${PS3_FTP_HOST}${PS3_INSTALL_DIR}/EBOOT.BIN"
curl --connect-timeout 10 --max-time 120 --ftp-pasv \
    -T /tmp/EBOOT.BIN \
    --user "${PS3_FTP_USER}:${PS3_FTP_PASS}" \
    "ftp://${PS3_FTP_HOST}${PS3_INSTALL_DIR}/EBOOT.BIN"
echo "Upload OK."

# --- [4/4] Verify remote file size --------------------------------------------
echo "[4/4] Verifying remote file size"
LOCAL_SIZE=$(stat -c %s /tmp/EBOOT.BIN)
REMOTE_SIZE=$(curl -sI --max-time 10 --ftp-pasv \
    --user "${PS3_FTP_USER}:${PS3_FTP_PASS}" \
    "ftp://${PS3_FTP_HOST}${PS3_INSTALL_DIR}/EBOOT.BIN" \
    | awk 'tolower($1) == "content-length:" {print $2}' \
    | tr -d '\r\n')
if [ -z "$REMOTE_SIZE" ]; then
    echo "ERROR: Could not read remote EBOOT.BIN size (FTP HEAD failed)." >&2
    echo "       Local size was $LOCAL_SIZE bytes. The PS3 may be off," >&2
    echo "       FTP may be down, or the upload silently failed." >&2
    exit 1
fi
if [ "$LOCAL_SIZE" != "$REMOTE_SIZE" ]; then
    echo "ERROR: EBOOT.BIN size mismatch after upload." >&2
    echo "       Local : $LOCAL_SIZE bytes" >&2
    echo "       Remote: $REMOTE_SIZE bytes" >&2
    echo "       The PS3 is still running the previous build." >&2
    exit 1
fi
echo "Verified: $LOCAL_SIZE bytes on both sides."
