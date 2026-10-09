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

summary
