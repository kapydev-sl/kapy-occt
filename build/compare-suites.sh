#!/usr/bin/env bash
#
# Run the geometry suite against BOTH kernels and diff the failures.
#
# The only interesting output is the DIFFERENCE: a test that fails on both is a
# pre-existing repo problem, one that fails only on the custom build is ours.
# The shipped 7.7 suite is not clean on every machine (modelThumbnail hangs,
# shellDocScenario fails), so an absolute pass count proves nothing.
#
# Every run is wrapped in a watchdog. OCCT can spin inside a synchronous WASM
# call that vitest's --testTimeout cannot interrupt, and killing the parent
# leaves the forked workers pegged at 100% CPU forever — twice we lost hours to
# exactly that. The watchdog kills the workers too.
#
#   services/occt/build/compare-suites.sh <path-to-kapy-occt.js> [vitest filter]
#
# Exits 1 if any test fails only on the custom kernel.

set -uo pipefail

MODULE="${1:?usage: compare-suites.sh <kapy-occt.js> [filter]}"
FILTER="${2:-occt.test}"
TIMEOUT="${WATCHDOG_SECONDS:-900}"
# Files to skip entirely. modelThumbnail hangs on BOTH kernels on some machines
# (a synchronous spin, see the watchdog note above), so it tells us nothing.
EXCLUDE="${EXCLUDE_GLOB:-**/modelThumbnail*}"
OUT="$(mktemp -d)"

# Run vitest under a wall-clock limit, killing the forked workers on expiry.
run_suite() {
    local label="$1" log="$2"
    shift 2
    ( "$@" > "$log" 2>&1 ) &
    local pid=$!
    local waited=0
    while kill -0 "$pid" 2>/dev/null; do
        sleep 5
        waited=$((waited + 5))
        if [ "$waited" -ge "$TIMEOUT" ]; then
            echo "### $label: watchdog fired after ${TIMEOUT}s — killing"
            pkill -P "$pid" 2>/dev/null
            kill -9 "$pid" 2>/dev/null
            pkill -f 'vitest/dist/workers/forks.js' 2>/dev/null
            return 124
        fi
    done
    return 0
}

# Test ids that failed, one per line. `×` also occurs inside test names
# ("a 10×10 square"), so anchor on the reporter's leading marker.
failures() {
    grep -E '^[[:space:]]+× ' "$1" | sed 's/^[[:space:]]*× //; s/ [0-9]*ms$//' | sort
}

echo "== baseline: opencascade.js 7.7 =="
run_suite 7.7 "$OUT/base.log" npx vitest run "$FILTER" --reporter=verbose --exclude "$EXCLUDE"
failures "$OUT/base.log" > "$OUT/base.fail"
echo "   $(grep -cE '^[[:space:]]+✓ ' "$OUT/base.log") passed, $(wc -l < "$OUT/base.fail") failed"

echo "== custom: $MODULE =="
KAPY_OCCT_MODULE="$MODULE" run_suite custom "$OUT/new.log" npx vitest run "$FILTER" --reporter=verbose --exclude "$EXCLUDE"
failures "$OUT/new.log" > "$OUT/new.fail"
echo "   $(grep -cE '^[[:space:]]+✓ ' "$OUT/new.log") passed, $(wc -l < "$OUT/new.fail") failed"

echo
echo "== regressions (fail only on the custom kernel) =="
comm -13 "$OUT/base.fail" "$OUT/new.fail" | tee "$OUT/regressions"
echo
echo "== fixed (fail only on 7.7) =="
comm -23 "$OUT/base.fail" "$OUT/new.fail"
echo
echo "logs: $OUT"
[ -s "$OUT/regressions" ] && exit 1
exit 0
