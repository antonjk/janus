#!/usr/bin/env bash
# Transpiler (janus-run --emit) output shapes, and the "two-faced" property: the same
# annotated file runs under plain bash with #@ directives inert.
set -eo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/_harness.sh"

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

# --- directive -> runtime-call shapes --------------------------------------------
cat > "${TMP}/g.sh" <<'EOF'
#!/usr/bin/env janus-run
#@ group: Sync group
    #@ step: $(echo dynamic-header)
    echo one
    #@ end
#@ end
#@ group async=4: Async group
for f in x y; do
    #@ step id=S depends=a,b: ${f}
    echo "do ${f}"
    #@ end
done
#@ end
EOF
emit="$("${JANUS_RUN}" --emit "${TMP}/g.sh")"
assert_contains "sync group -> janus-group with header"   "${emit}" 'janus-group "Sync group"'
assert_contains "async=N -> --async=4"                    "${emit}" 'janus-group --async=4 "Async group"'
assert_contains "dynamic header preserved for runtime"    "${emit}" 'janus-step "$(echo dynamic-header)"'
assert_contains "step attrs id/depends emitted"           "${emit}" '--id S --depends a,b'
assert_contains "loop step uses unquoted heredoc"         "${emit}" '"${f}" <<JANUS_STEP_'
assert_contains "runtime sourced in prologue"             "${emit}" 'source'
assert_contains "epilogue runs the tree"                  "${emit}" 'janus-exec'
assert_contains "for-loop structure preserved"            "${emit}" 'for f in x y; do'

# unique heredoc delimiters per step (no collision)
d1="$(printf '%s\n' "${emit}" | grep -oE '<<JANUS_STEP_[0-9]+' | sed -n 1p)"
d2="$(printf '%s\n' "${emit}" | grep -oE '<<JANUS_STEP_[0-9]+' | sed -n 2p)"
if [[ -n "$d1" && -n "$d2" && "$d1" != "$d2" ]]; then pass "unique heredoc delimiters ($d1 vs $d2)"; else fail "heredoc delimiters not unique ($d1 / $d2)"; fi

# --- two-faced: same file under plain bash, #@ inert, sequential ------------------
# (Swap the shebang to bash; directives are comments; bodies run top-to-bottom.)
sed '1s|.*|#!/usr/bin/env bash|' "${TMP}/g.sh" > "${TMP}/g-bash.sh"
chmod +x "${TMP}/g-bash.sh"
bout="$(/opt/homebrew/opt/bash/bin/bash "${TMP}/g-bash.sh" 2>&1)"
assert_contains "plain-bash run executes step-1 body"       "${bout}" "one"
assert_contains "plain-bash run executes loop body (x)"     "${bout}" "do x"
assert_contains "plain-bash run executes loop body (y)"     "${bout}" "do y"
assert_not_contains "plain-bash run emits no janus calls"   "${bout}" "janus-group"

summary
