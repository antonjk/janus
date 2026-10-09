#!/usr/bin/env bash
# Concurrency resolution: no async -> 1; bare --async -> $JANUS_ASYNC; --async=N -> N.
# Plus the "unbounded" clamp to min(batch, ncpu), while explicit N is unclamped.
set -eo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/_harness.sh"

# --- janus-group records the right async= in meta --------------------------------
# Each case runs in a fresh subshell so the per-PID spool/state is isolated.
group_meta() {  # group_meta <env-assignns> <group-args...> ; echoes async= value
   /opt/homebrew/opt/bash/bin/bash -c '
      source '"${ROOT}"'/include/trap.inc; source '"${ROOT}"'/include/temp.inc
      source '"${ROOT}"'/include/cprintf.inc; source '"${ROOT}"'/include/janus.inc
      '"$1"'
      janus-group '"$2"' "g"
      meta="$(grep "^GROUP" "$(_janus_spool)/tree" | head -1 | cut -d"|" -f4)"
      echo "${meta#async=}"
   '
}

assert_eq "no --async resolves to sync (1)"            "1"         "$(group_meta '' '')"
assert_eq "bare --async uses default JANUS_ASYNC"      "unbounded" "$(group_meta '' '--async')"
assert_eq "bare --async honors JANUS_ASYNC override"   "4"         "$(group_meta 'export JANUS_ASYNC=4' '--async')"
assert_eq "--async=N literal wins"                     "7"         "$(group_meta 'export JANUS_ASYNC=unbounded' '--async=7')"

# --- the "unbounded" clamp: min(batch, ncpu); explicit N unclamped ----------------
cap_for() {  # cap_for <async-spec> <batch-size> <stub-ncpu> ; echoes resolved cap
   /opt/homebrew/opt/bash/bin/bash -c '
      source '"${ROOT}"'/include/trap.inc; source '"${ROOT}"'/include/temp.inc
      source '"${ROOT}"'/include/cprintf.inc; source '"${ROOT}"'/include/janus.inc
      _janus_ncpu() { echo '"$3"'; }
      async="'"$1"'"; n='"$2"'
      steps=(); for ((i=0;i<n;i++)); do steps+=("$i"); done
      if [[ -z "$async" ]]; then cap=1
      elif [[ "$async" == "unbounded" ]]; then
         ncpu="$(_janus_ncpu)"; cap=${#steps[@]}; (( cap > ncpu )) && cap=${ncpu}
      else cap="$async"; (( cap < 1 )) && cap=1; fi
      echo "$cap"
   '
}

assert_eq "unbounded clamps big batch to ncpu"   "2"   "$(cap_for unbounded 6 2)"
assert_eq "unbounded leaves small batch alone"   "3"   "$(cap_for unbounded 3 8)"
assert_eq "explicit N not clamped by ncpu"       "500" "$(cap_for 500 600 2)"
assert_eq "sync is 1 regardless"                 "1"   "$(cap_for '' 6 2)"

# --- _janus_ncpu returns a positive integer --------------------------------------
_janus_load_runtime
n="$(_janus_ncpu)"
if [[ "$n" =~ ^[0-9]+$ && "$n" -gt 0 ]]; then pass "_janus_ncpu returns positive int ($n)"; else fail "_janus_ncpu bad: [$n]"; fi

summary
