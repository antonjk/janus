#!/usr/bin/env bash
# End-to-end behavior via janus-run (non-TTY): per-step overrides, step bodies calling
# script-level functions/includes, and steps registered inside a pipe subshell.
set -eo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/_harness.sh"

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

# --- per-step overrides: ok/err text and success/fail context line counts ---------
cat > "${TMP}/over.sh" <<'EOF'
#!/usr/bin/env janus-run
#@ group: checks
    #@ step ok=built: compile
    echo "compiled fine"; true
    #@ end
    #@ step fail=2 err=BROKEN: linker
    echo l1; echo l2; echo l3; echo l4; false
    #@ end
    #@ step success=1: quiet
    echo noise1; echo noise2; echo important-last; true
    #@ end
#@ end
EOF
out="$(JANUS_CONTEXT_SUCCESS=0 run_janus 20 "${TMP}/over.sh" | strip_ansi)"
assert_contains "custom ok=built status"            "${out}" "compile    [ built"
assert_contains "custom err=BROKEN status"          "${out}" "[ BROKEN"
assert_contains "fail=2 shows last-of-4 fail lines" "${out}" "l3"
assert_not_contains "fail=2 hides earlier fail line" "${out}" "l1"
assert_contains "success=1 shows the last line"     "${out}" "important-last"
assert_not_contains "success=1 hides earlier line"  "${out}" "noise1"

# --- function + sourced-include + variable visibility in a step body --------------
cat > "${TMP}/inc.sh" <<EOF
helper() { echo ">>\$1<<"; }
EOF
cat > "${TMP}/ctx.sh" <<EOF
#!/usr/bin/env janus-run
source "${TMP}/inc.sh"
GREETING="hi-from-script"
myfn() { echo "fn-got \$1"; }
#@ group: context
    #@ step: fn
    myfn alpha
    #@ end
    #@ step: inc
    helper beta
    #@ end
    #@ step: var
    echo "G=\$GREETING"
    #@ end
#@ end
EOF
out="$(JANUS_CONTEXT_SUCCESS=5 run_janus 20 "${TMP}/ctx.sh" | strip_ansi)"
assert_contains "step calls a script-level function" "${out}" "fn-got alpha"
assert_contains "step uses a sourced include"        "${out}" ">>beta<<"
assert_contains "step reads a script variable"       "${out}" "G=hi-from-script"

# --- steps registered inside a pipe subshell (cmd | while read) -------------------
cat > "${TMP}/pipe.sh" <<'EOF'
#!/usr/bin/env janus-run
tag() { echo "tag:$1"; }
#@ group: piped
printf 'aa\nbb\ncc\n' | while IFS= read -r item; do
   #@ step: ${item}
   echo "ran ${item}"; tag "${item}"
   #@ end
done
#@ end
EOF
out="$(JANUS_CONTEXT_SUCCESS=5 run_janus 20 "${TMP}/pipe.sh" | strip_ansi)"
assert_not_contains "no command-not-found for pipe-registered steps" "${out}" "command not found"
assert_contains "pipe step aa ran with per-iteration value" "${out}" "ran aa"
assert_contains "pipe step cc ran with per-iteration value" "${out}" "ran cc"
assert_contains "pipe step can call script function"        "${out}" "tag:bb"

# --- #@ exec: ordered, silent-on-success, runs once, surfaces failures ------------
cat > "${TMP}/exec.sh" <<'EOF'
#!/usr/bin/env janus-run
#@ group: Sync
    #@ step: meta
    echo SYNC_RAN
    #@ end
#@ end
#@ exec
echo EXEC_RAN
#@ end
EOF
out="$(run_janus 20 "${TMP}/exec.sh" | strip_ansi)"
assert_contains "exec body runs"                      "${out}" "EXEC_RAN"
# runs exactly once (not doubled by a stray janus-exec)
n_exec="$(printf '%s\n' "${out}" | grep -c 'EXEC_RAN' || true)"
assert_eq "exec body runs exactly once" "1" "${n_exec}"
# silent on success: the exec body line carries no status box of its own
exec_render="$(printf '%s\n' "${out}" | grep 'EXEC_RAN')"
assert_eq "exec line is the bare body, no status box" "EXEC_RAN" "${exec_render}"
# ordering: the Sync group renders before the exec body
sync_line="$(printf '%s\n' "${out}" | grep -n 'Sync' | head -1 | cut -d: -f1)"
exec_line="$(printf '%s\n' "${out}" | grep -n 'EXEC_RAN' | head -1 | cut -d: -f1)"
assert_lt "sync group runs before exec block" "${sync_line}" "${exec_line}"

# failure inside #@ exec still surfaces its context
cat > "${TMP}/execfail.sh" <<'EOF'
#!/usr/bin/env janus-run
#@ exec
echo BOOM_CONTEXT; false
#@ end
EOF
# This run exits non-zero by design; don't let errexit abort the test.
out="$(run_janus 20 "${TMP}/execfail.sh" | strip_ansi)" || true
assert_contains "exec failure shows context" "${out}" "BOOM_CONTEXT"

summary
