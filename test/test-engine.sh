#!/usr/bin/env bash
# Direct runtime test (no transpiler yet): builds the DESIGN.md s4 nested-async tree
# by calling the registry API directly, then runs the engine and checks ordering.
#
# Expected execution order:
#   1 & 2 concurrently -> barrier -> 3 & 4 concurrently -> barrier -> 5

set -eo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${HERE}/.." && pwd)"

source "${ROOT}/include/trap.inc"
source "${ROOT}/include/temp.inc"
source "${ROOT}/include/cprintf.inc"
source "${ROOT}/include/spinner.inc"
source "${ROOT}/include/janus.inc"

# Each step appends "<name> <start|end> <epoch-ms>" to this shared log.
EVLOG="$(temp_file evlog.XXXX)"
export EVLOG

mk() {   # mk <name>  -> emits a body that logs start, sleeps, logs end
   local name="$1"
   printf 'printf "%%s start %%s\\n" "%s" "$(date +%%s%%N)" >>"%s"\n' "${name}" "${EVLOG}"
   printf 'sleep 1\n'
   printf 'printf "%%s end %%s\\n" "%s" "$(date +%%s%%N)" >>"%s"\n' "${name}" "${EVLOG}"
}

janus-group --async "outer"
  janus-step "1" <<BODY
  printf "Done\n"
$(mk 1)
BODY
  janus-step "2" <<BODY
$(mk 2)
BODY
  janus-group --async "nested"
    janus-step "3" <<BODY
$(mk 3)
BODY
    janus-step "4" <<BODY
$(mk 4)
BODY
  janus-group-end
  janus-step "5" <<BODY
$(mk 5)
BODY
janus-group-end

echo "=== tree ==="
cat "$(temp_dir)/janus/tree"

echo "=== exec ==="
janus-exec
rc=$?
echo "exec_rc=${rc}"

echo "=== event log ==="
cat "${EVLOG}"

# ---- Assertions on ordering ----
# Extract start times (ns) per step.
start() { awk -v s="$1" '$1==s && $2=="start"{print $3}' "${EVLOG}"; }
end()   { awk -v s="$1" '$1==s && $2=="end"  {print $3}' "${EVLOG}"; }

fail=0
check() { # check <desc> <condition-cmd...>
   local desc="$1"; shift
   if "$@"; then echo "PASS: ${desc}"; else echo "FAIL: ${desc}"; fail=1; fi
}

# 1 and 2 overlap: max(start1,start2) < min(end1,end2)
lt() { [[ "$1" -lt "$2" ]]; }
s1=$(start 1); e1=$(end 1); s2=$(start 2); e2=$(end 2)
s3=$(start 3); e3=$(end 3); s4=$(start 4); e4=$(end 4)
s5=$(start 5); e5=$(end 5)

maxs12=$(( s1 > s2 ? s1 : s2 )); mine12=$(( e1 < e2 ? e1 : e2 ))
check "steps 1 and 2 run concurrently" lt "${maxs12}" "${mine12}"

maxs34=$(( s3 > s4 ? s3 : s4 )); mine34=$(( e3 < e4 ? e3 : e4 ))
check "steps 3 and 4 run concurrently" lt "${maxs34}" "${mine34}"

# barrier: nested group (3,4) starts only after 1 AND 2 finished
maxe12=$(( e1 > e2 ? e1 : e2 ))
check "nested group waits for 1 and 2 (3 starts after both end)" lt "${maxe12}" "${s3}"
check "nested group waits for 1 and 2 (4 starts after both end)" lt "${maxe12}" "${s4}"

# barrier: step 5 starts only after 3 AND 4 finished
maxe34=$(( e3 > e4 ? e3 : e4 ))
check "step 5 waits for nested group (5 starts after 3 and 4 end)" lt "${maxe34}" "${s5}"

echo
if (( fail )); then echo "RESULT: FAILURES"; exit 1; else echo "RESULT: ALL PASS"; fi
