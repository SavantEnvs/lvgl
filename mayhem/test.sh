#!/usr/bin/env bash
#
# mayhem/test.sh — run the oracle self-test (/mayhem/lvgl_selftest, built by mayhem/build.sh with the
# project's normal flags). It asserts known answers: decoded pixel hashes for PNG/BMP/JPEG/LVGL-bin
# (raw, LZ4) images, SVG parse, glyph metrics and bitmaps for binary and TrueType fonts, Arabic
# shaping bytes and bidi reordering. exit 0 = pass.
#
#
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}


[ -x "$SRC/lvgl_selftest" ] || { echo "lvgl_selftest missing: mayhem/build.sh must build it" >&2; exit 2; }

out=$("$SRC/lvgl_selftest" 2>&1); rc=$?
printf '%s\n' "$out"

summary=$(printf '%s\n' "$out" | sed -n 's/^selftest: \([0-9][0-9]*\) passed, \([0-9][0-9]*\) failed$/\1 \2/p' | tail -n 1)
if [ -z "$summary" ]; then
  echo "no selftest summary line: the self-test did not run to completion" >&2
  emit_ctrf "lvgl-selftest" 0 1 0
  exit 1
fi
read -r passed failed <<<"$summary"
[ "$passed" -ge 49 ] || { echo "expected at least 49 passing checks, got $passed" >&2; failed=$((failed + 1)); }
[ "$rc" -eq 0 ] || failed=$((failed + 1))
emit_ctrf "lvgl-selftest" "$passed" "$failed" 0
