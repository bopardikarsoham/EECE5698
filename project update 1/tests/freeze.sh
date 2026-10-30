#!/bin/sh
# Copy a data case into tests/cases/ with the current software predictions as its
# expected output, so `make check_all` catches any later change in the decoder's
# output. Usage: sh tests/freeze.sh <case_dir>  (run `make sw CFG=<case_dir>` first)
set -e
SRC="${1%/}"
NAME=$(basename "$SRC")
DEST="tests/cases/$NAME"
if [ ! -f "$SRC/sw_pred.bin" ] || [ ! -f "$SRC/sw_stats.txt" ]; then
    echo "freeze: $SRC has no sw_pred.bin / sw_stats.txt, run 'make sw CFG=$SRC' first" >&2
    exit 1
fi
if ! grep -q '^invalid=0$' "$SRC/sw_stats.txt" || ! grep -q '^cap_hits=0$' "$SRC/sw_stats.txt"; then
    echo "freeze: refusing to freeze $SRC, it has invalid corrections or cap hits" >&2
    exit 1
fi
mkdir -p "$DEST"
for f in graph.txt dets.bin obs.bin meta.json pymatching_pred.bin; do
    [ -f "$SRC/$f" ] && cp "$SRC/$f" "$DEST/$f"
done
cp "$SRC/sw_pred.bin" "$DEST/expected_pred.bin"
echo "freeze: $SRC -> $DEST (expected_pred.bin = current sw_pred.bin)"
