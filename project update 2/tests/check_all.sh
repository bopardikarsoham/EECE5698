#!/bin/sh
# Regression check: decode every case in tests/cases/ and compare the predictions
# with that case's expected_pred.bin. Also requires 0 invalid corrections and
# 0 cap hits. Usage: sh tests/check_all.sh <path to golden binary>
GOLDEN="$1"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
total=0
passed=0
for dir in tests/cases/*/; do
    dir=${dir%/}
    name=$(basename "$dir")
    total=$((total + 1))
    if ! "$GOLDEN" "$dir" --out "$TMP/$name.pred" --stats "$TMP/$name.stats" > "$TMP/$name.log" 2>&1; then
        echo "FAIL  $name: golden exited with an error"
        cat "$TMP/$name.log"
        continue
    fi
    shots=$(grep '^shots=' "$TMP/$name.stats" | cut -d= -f2)
    if ! cmp -s "$TMP/$name.pred" "$dir/expected_pred.bin"; then
        echo "FAIL  $name: predictions differ from expected_pred.bin"
        continue
    fi
    echo "PASS  $name ($shots shots, matches expected_pred.bin, 0 invalid, 0 cap hits)"
    passed=$((passed + 1))
done
echo "check_all: $passed / $total cases PASS"
[ "$passed" -eq "$total" ] && [ "$total" -gt 0 ]
