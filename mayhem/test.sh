#!/usr/bin/env bash
#
# mayhem/test.sh — RUN this repo's OWN functional test suite (already built by mayhem/build.sh).
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
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

# Runs the OpenAL known-answer self-test built by build.sh (mayhem/harnesses/openal_selftest.cpp):
# renders PCM16/U8/IMA4/MS-ADPCM buffers through axmol's vendored OpenAL-soft on a loopback device and
# asserts the rendered sample values; each check prints PASS/FAIL.
# Also runs the ax::network::Uri known-answer test (mayhem/harnesses/uri_selftest.cpp) and the TGA
# decoder / SAX parser known-answer test (mayhem/harnesses/core_selftest.cpp).
passed=0; failed=0
# run_suite <binary> <expected number of checks>
run_suite() {
  local runner="$1" expected="$2" out rc p f
  if [ ! -x "$runner" ]; then
    echo "missing $runner (build.sh should have built it)" >&2
    failed=$(( failed + expected )); return
  fi
  out="$("$runner" 2>&1)"; rc=$?
  echo "$out"
  p=$(grep -c '^PASS ' <<<"$out"); f=$(grep -c '^FAIL ' <<<"$out")
  # A crash/exit before all checks ran, or a non-zero exit, counts as failures.
  if [ $(( p + f )) -lt "$expected" ]; then f=$(( expected - p )); fi
  if [ "$rc" -ne 0 ] && [ "$f" -eq 0 ]; then f=1; fi
  passed=$(( passed + p )); failed=$(( failed + f ))
}
run_suite "$SRC/mayhem/build-test/openal_selftest" 8
run_suite "$SRC/mayhem/build-test/uri_selftest" 18
run_suite "$SRC/mayhem/build-test/core_selftest" 9
emit_ctrf "axmol-selftest" "$passed" "$failed" 0
