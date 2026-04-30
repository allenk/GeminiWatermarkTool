#!/usr/bin/env bash
# =============================================================================
# CI smoke test: AI denoise initialisation + end-to-end run
# =============================================================================
# Exercises the NcnnDenoiser::initialize() path, including the Vulkan loader
# probe and CPU fallback. Generates a synthetic 512x512 image, runs the binary
# with --denoise ai through the full pipeline, and asserts that the process
# exits successfully and produces an output file.
#
# This catches the class of issues seen in #30 / #31 -- where Vulkan loading
# fails on macOS without MoltenVK and the AI denoise init path crashes with
# SIGBUS instead of falling back to CPU. A plain `--version` smoke test does
# not exercise this code path.
#
# Usage: ci_smoke_ai_denoise.sh <path-to-binary>
# =============================================================================
set -euo pipefail

BIN="${1:?usage: $0 <path-to-binary>}"

if [[ ! -x "$BIN" && ! -f "$BIN" ]]; then
    echo "ERROR: binary not found: $BIN" >&2
    exit 2
fi

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

INPUT="$WORKDIR/smoke_in.png"
OUTPUT="$WORKDIR/smoke_out.png"

PY=python3
command -v "$PY" >/dev/null 2>&1 || PY=python

echo "--- generating 512x512 synthetic test image (using $PY) ---"
"$PY" - "$INPUT" <<'PY_EOF'
import struct, sys, zlib

def png(w, h, color=(128, 128, 128)):
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d))
    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    row = b"\0" + bytes(color) * w
    raw = row * h
    idat = chunk(b"IDAT", zlib.compress(raw))
    iend = chunk(b"IEND", b"")
    return sig + ihdr + idat + iend

with open(sys.argv[1], "wb") as f:
    f.write(png(512, 512))
PY_EOF
ls -lh "$INPUT"

echo "--- running AI denoise pipeline ---"
# --force         skip detection (no real watermark on a uniform image)
# --region br:auto  process the bottom-right region at the Gemini default position
# --denoise ai    exercise the NCNN/Vulkan/CPU dispatch path
"$BIN" --force --region br:auto --denoise ai -i "$INPUT" -o "$OUTPUT"
RC=$?

if [[ $RC -ne 0 ]]; then
    echo "ERROR: binary exited with code $RC" >&2
    exit $RC
fi

if [[ ! -f "$OUTPUT" ]]; then
    echo "ERROR: expected output file not produced: $OUTPUT" >&2
    exit 3
fi

OUT_SIZE=$(wc -c <"$OUTPUT")
if [[ $OUT_SIZE -lt 1000 ]]; then
    echo "ERROR: output file suspiciously small ($OUT_SIZE bytes)" >&2
    exit 4
fi

echo "--- AI denoise smoke test PASSED (output=$OUT_SIZE bytes) ---"
