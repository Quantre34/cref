#!/usr/bin/env bash
set -e

CREF=${CREF_BIN:-./cref}
failures=0

pass() { printf "  ok: %s\n" "$1"; }
fail() { printf "FAIL: %s\n" "$1" >&2; failures=$((failures + 1)); }

check() {
    local desc="$1"; shift
    if "$@" >/dev/null 2>&1; then pass "$desc"; else fail "$desc"; fi
}

echo "--help:"
check "--help exits 0"       "$CREF" --help
check "-h exits 0"           "$CREF" -h

echo "--score:"
out=$("$CREF" --score malloc ref 2>&1)
echo "$out" | grep -q "malloc" && pass "--score output contains query" \
                                || { fail "--score output missing query"; }

out=$("$CREF" --score malloc ref 2>&1)
echo "$out" | grep -qE "[0-9]+" && pass "--score output contains a number" \
                                 || { fail "--score output missing number"; }

echo "bad directory:"
"$CREF" -d /nonexistent_path_xyz 2>&1 | grep -qi "error\|cannot\|open" \
    && pass "bad dir prints error" \
    || fail "bad dir should print error"

if [ "$failures" -gt 0 ]; then
    echo ""
    echo "$failures test(s) failed." >&2
    exit 1
fi
echo ""
echo "All CLI tests passed."
