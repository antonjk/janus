#!/usr/bin/env bash
# Shared test harness for the janus test suite.
#
# Each test file sources this, uses the assert_* helpers, and ends with `summary`.
# Convention: tests print "PASS: ..." / "FAIL: ..." per check and exit non-zero if
# any failed.

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${HERE}/.." && pwd)"
export JANUS_ROOT="${ROOT}"

# Load the runtime (dev tree) for tests that drive the registry/engine directly.
_janus_load_runtime() {
   source "${ROOT}/include/trap.inc"
   source "${ROOT}/include/temp.inc"
   source "${ROOT}/include/cprintf.inc"
   source "${ROOT}/include/spinner.inc"
   source "${ROOT}/include/janus.inc"
}

# Path to the dev transpiler (janus-run), for transpiler/end-to-end tests.
JANUS_RUN="${ROOT}/bin/janus-run"

_T_FAIL=0
_T_COUNT=0

pass() { _T_COUNT=$(( _T_COUNT + 1 )); printf 'PASS: %s\n' "$1"; }
fail() { _T_COUNT=$(( _T_COUNT + 1 )); _T_FAIL=$(( _T_FAIL + 1 )); printf 'FAIL: %s\n' "$1"; }

# assert_eq <desc> <expected> <actual>
assert_eq() {
   if [[ "$2" == "$3" ]]; then pass "$1"; else fail "$1 (expected [$2], got [$3])"; fi
}

# assert_contains <desc> <haystack> <needle>
assert_contains() {
   if [[ "$2" == *"$3"* ]]; then pass "$1"; else fail "$1 (missing [$3] in output)"; fi
}

# assert_not_contains <desc> <haystack> <needle>
assert_not_contains() {
   if [[ "$2" != *"$3"* ]]; then pass "$1"; else fail "$1 (unexpectedly found [$3])"; fi
}

# assert_lt <desc> <a> <b>   (integer a < b)
assert_lt() {
   if [[ "$2" -lt "$3" ]]; then pass "$1"; else fail "$1 ($2 !< $3)"; fi
}

# assert_ge <desc> <a> <b>
assert_ge() {
   if [[ "$2" -ge "$3" ]]; then pass "$1"; else fail "$1 ($2 !>= $3)"; fi
}

# Strip ANSI colour + the ESC[K clear so assertions match on plain text.
strip_ansi() { sed -E 's/\x1b\[[0-9;]*m//g; s/\x1b\[K//g'; }

# Run a janus-run invocation on a temp script with a hard timeout guard, echo output.
# Usage: run_janus <timeout-secs> <script-path> [args...]
run_janus() {
   local to="$1"; shift
   local out; out="$(mktemp)"
   "${JANUS_RUN}" "$@" >"${out}" 2>&1 &
   local pid=$!
   ( sleep "${to}" && kill -9 "${pid}" 2>/dev/null ) & local guard=$!
   wait "${pid}" 2>/dev/null
   kill "${guard}" 2>/dev/null
   cat "${out}"; rm -f "${out}"
}

summary() {
   echo
   if (( _T_FAIL > 0 )); then
      printf 'RESULT: %d/%d FAILED\n' "${_T_FAIL}" "${_T_COUNT}"
      exit 1
   fi
   printf 'RESULT: ALL %d PASS\n' "${_T_COUNT}"
}
