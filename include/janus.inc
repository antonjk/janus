[[ -n "${_JANUS_INC_SOURCED:-}" ]] && return
_JANUS_INC_SOURCED=1

########################################################################################################################
# Janus runtime
#
# Registry API (used by transpiled scripts):
#   janus-group [--async[=N]] "<header>"   open a group (branch)
#   janus-group-end                        close the current group
#   janus-step  [--id X] [--depends a,b] "<header>" <<DELIM ... DELIM
#                                            register a leaf; body read from stdin (NOT executed yet)
#   janus-exec                             run the registered tree
#
# Model (see DESIGN.md s4): a group runs children in registration order; a maximal run
# of adjacent steps forms one concurrent batch (bounded by async=N; sync => size 1); a
# child group is a barrier.
#
# State lives in a spool dir on disk so it survives subshells. depends is parsed but
# NOT enforced in v1.
#
# NOTE: do not run under `set -u`; the vendored cprintf is not nounset-clean.
########################################################################################################################

# Concurrency width used when a group is opened with a bare `--async` (no `=N`).
# Defaults to "unbounded" but is env-overridable: a script or user can
# `export JANUS_ASYNC=4` (or =1 to force sync) before the run to control every
# bare-`--async` group at once.
: "${JANUS_ASYNC:=unbounded}"

# Dependencies (sourced by the transpiled script's prologue, but guard here too):
#   trap.inc  -> trap_add, kill_descendant_processes
#   temp.inc  -> temp_dir (per-PID folder)
#   cprintf.inc -> cprintf
#   spinner.inc -> (optional) spinner visuals

# ---------------------------------------------------------------------------------------------------------------------
# Spool dir + registry state
# ---------------------------------------------------------------------------------------------------------------------

_janus_spool() {
   # Per-PID spool dir for the janus tree. Created on first use.
   local base
   if declare -f temp_dir >/dev/null 2>&1; then
      base="$(temp_dir)"
   else
      base="/tmp/PID-$$"
   fi
   printf '%s/janus' "${base}"
}

# Detect the online CPU count portably (used to clamp "unbounded" concurrency).
#   getconf _NPROCESSORS_ONLN  -> macOS and Linux
#   nproc                      -> Linux / coreutils
#   sysctl -n hw.ncpu          -> macOS / BSD
# Falls back to 4 if none report a positive integer.
_janus_ncpu() {
   local n
   n="$(getconf _NPROCESSORS_ONLN 2>/dev/null)"
   [[ "${n}" =~ ^[0-9]+$ ]] || n="$(nproc 2>/dev/null)"
   [[ "${n}" =~ ^[0-9]+$ ]] || n="$(sysctl -n hw.ncpu 2>/dev/null)"
   [[ "${n}" =~ ^[0-9]+$ && "${n}" -gt 0 ]] || n=4
   printf '%s' "${n}"
}

_janus_init() {
   local spool; spool="$(_janus_spool)"
   [[ -d "${spool}" ]] && return 0
   mkdir -p "${spool}" || return 1
   : >"${spool}/tree"
   _JANUS_DEPTH=0
   _JANUS_UID=0
   # Clean up spooled children on interrupt if trap.inc is available.
   if declare -f trap_add >/dev/null 2>&1; then
      trap_add '_janus_cleanup' EXIT INT TERM
   fi
}

_janus_cleanup() {
   # Best-effort: kill any still-running step children.
   if declare -f kill_descendant_processes >/dev/null 2>&1; then
      kill_descendant_processes "$$" false 2>/dev/null
   fi
}

# tree record format (one line): TYPE|depth|id|meta|header
#   TYPE in GROUP | GROUP_END | STEP
_janus_record() {
   local spool; spool="$(_janus_spool)"
   printf '%s\n' "$1" >>"${spool}/tree"
}

# ---------------------------------------------------------------------------------------------------------------------
# Registry API
# ---------------------------------------------------------------------------------------------------------------------
janus-group() {
   _janus_init
   # Concurrency resolution:
   #   no --async      -> "1"            (sync; one step at a time)
   #   --async         -> "$JANUS_ASYNC" (env-driven; defaults to "unbounded", but a
   #                                       script/user can export JANUS_ASYNC to pick a
   #                                       specific width, e.g. 4, or 1 to force sync)
   #   --async=N        -> "N"            (explicit literal always wins)
   local async="1" header=""
   while [[ $# -gt 0 ]]; do
      case "$1" in
         --async)    async="${JANUS_ASYNC}"; shift ;;
         --async=*)  async="${1#--async=}"; shift ;;
         --)         shift; break ;;
         -*)         shift ;;              # ignore unknown flags for forward-compat
         *)          break ;;
      esac
   done
   header="$1"
   local id="g$(( ++_JANUS_UID ))"
   local meta="async=${async}"
   _janus_record "GROUP|${_JANUS_DEPTH}|${id}|${meta}|${header}"
   _JANUS_DEPTH=$(( _JANUS_DEPTH + 1 ))
}

janus-group-end() {
   _janus_init
   # A group opened at level D raised _JANUS_DEPTH to D+1. Record the GROUP_END at
   # the group's OWN level D (so the walker can match it against the opening GROUP),
   # then drop back to D.
   (( _JANUS_DEPTH > 0 )) && _JANUS_DEPTH=$(( _JANUS_DEPTH - 1 ))
   _janus_record "GROUP_END|${_JANUS_DEPTH}|||"
}

janus-step() {
   _janus_init
   local id="" depends="" header=""
   local succ="" fail="" ok="" err=""
   while [[ $# -gt 0 ]]; do
      case "$1" in
         --id)        id="$2"; shift 2 ;;
         --id=*)      id="${1#--id=}"; shift ;;
         --depends)   depends="$2"; shift 2 ;;
         --depends=*) depends="${1#--depends=}"; shift ;;
         --success)   succ="$2"; shift 2 ;;
         --success=*) succ="${1#--success=}"; shift ;;
         --fail)      fail="$2"; shift 2 ;;
         --fail=*)    fail="${1#--fail=}"; shift ;;
         --ok)        ok="$2"; shift 2 ;;
         --ok=*)      ok="${1#--ok=}"; shift ;;
         --err)       err="$2"; shift 2 ;;
         --err=*)     err="${1#--err=}"; shift ;;
         --)          shift; break ;;
         -*)          shift ;;
         *)           break ;;
      esac
   done
   header="$1"
   local uid="s$(( ++_JANUS_UID ))"
   [[ -z "${id}" ]] && id="${uid}"
   # Read the body (heredoc on stdin). The transpiler uses an UNQUOTED heredoc, so
   # loop variables and $(...) are already expanded for THIS registration — baking
   # per-iteration values in. Define a uniquely-named function carrying that body.
   # The engine later calls this function in a forked subshell, so the body has full
   # access to the script's own functions, sourced includes, and shell variables.
   local body; body="$(cat)"
   local fn="_janus_fn_${uid}"
   # The leading/trailing newlines keep a lone leading comment or trailing `}` safe.
   eval "${fn}() {
${body}
}"
   # Also persist the exact same captured body to disk. If janus-step ran inside a
   # subshell (e.g. the right side of a pipe, as in `cmd | while read ... done`),
   # the function above dies with that subshell and is NOT visible when janus-exec
   # runs in the main shell. In that case the engine falls back to sourcing this
   # file (see _janus_run_batch), which still executes in a forked subshell of the
   # engine and therefore keeps access to the script's functions and includes.
   local spool; spool="$(_janus_spool)"
   printf '%s\n' "${body}" >"${spool}/step.${uid}.body"
   # Meta is a ';'-separated kv list; empty values mean "use the global default".
   # The function name travels in meta as fn=... so the engine knows what to call.
   local meta="depends=${depends};success=${succ};fail=${fail};ok=${ok};err=${err};fn=${fn}"
   _janus_record "STEP|${_JANUS_DEPTH}|${uid}|${meta}|${header}"
}

# Extract a value for KEY from a ';'-separated kv meta string. Echoes the value
# (possibly empty).
_janus_meta_get() {
   local meta="$1" key="$2" kv
   local IFS=';'
   for kv in ${meta}; do
      [[ "${kv%%=*}" == "${key}" ]] && { printf '%s' "${kv#*=}"; return 0; }
   done
   return 0
}

# ---------------------------------------------------------------------------------------------------------------------
# Engine (janus-exec)
# ---------------------------------------------------------------------------------------------------------------------

# Parse the flat tree into parallel arrays, then walk it.
janus-exec() {
   # The engine uses arithmetic (( )) and poll loops that are not `set -e` safe.
   # Neutralise the caller's errexit for the duration of execution.
   local __janus_errexit=0; [[ $- == *e* ]] && __janus_errexit=1
   set +e
   local spool; spool="$(_janus_spool)"
   [[ -r "${spool}/tree" ]] || { ceprintf "<fg:red>janus: no registered steps</fg>\n"; (( __janus_errexit )) && set -e; return 1; }

   # Load records into arrays.
   _WT_TYPE=(); _WT_DEPTH=(); _WT_ID=(); _WT_META=(); _WT_HEADER=()
   local line
   while IFS= read -r line; do
      local type depth id meta header rest
      type="${line%%|*}";   rest="${line#*|}"
      depth="${rest%%|*}";  rest="${rest#*|}"
      id="${rest%%|*}";     rest="${rest#*|}"
      meta="${rest%%|*}";   header="${rest#*|}"
      _WT_TYPE+=("${type}"); _WT_DEPTH+=("${depth}"); _WT_ID+=("${id}")
      _WT_META+=("${meta}"); _WT_HEADER+=("${header}")
   done <"${spool}/tree"

   local n=${#_WT_TYPE[@]}

   # Global header-field width: the widest STEP "indent + header" across the whole
   # run, so every spinner/status box aligns on the same column (character count,
   # matching the original janus-exec.sh WIDTH).
   #
   # Global status-field width: the widest effective status word (per-step ok/err
   # override, else the global default) across all steps, so every status box is the
   # same width and the right edge stays uniform.
   _WT_MAXHDR=0
   _WT_STATUS_W=0
   local _k
   for (( _k=0; _k<n; _k++ )); do
      [[ "${_WT_TYPE[$_k]}" == "STEP" ]] || continue
      local _indent_w=$(( ( ${_WT_DEPTH[$_k]} + 1 ) * JANUS_INDENT ))
      local _w=$(( _indent_w + ${#_WT_HEADER[$_k]} ))
      (( _w > _WT_MAXHDR )) && _WT_MAXHDR=${_w}
      # Effective status words for this step.
      local _ok _err
      _ok="$(_janus_meta_get "${_WT_META[$_k]}" ok)";  [[ -z "${_ok}" ]]  && _ok="${JANUS_OK_TEXT}"
      _err="$(_janus_meta_get "${_WT_META[$_k]}" err)"; [[ -z "${_err}" ]] && _err="${JANUS_ERR_TEXT}"
      (( ${#_ok}  > _WT_STATUS_W )) && _WT_STATUS_W=${#_ok}
      (( ${#_err} > _WT_STATUS_W )) && _WT_STATUS_W=${#_err}
   done
   (( _WT_STATUS_W < 1 )) && _WT_STATUS_W=1

   local overall=0
   # Walk the top level (depth 0 nodes), recursing into groups.
   # _WT_CURSOR is a global index advanced by _janus_walk_node (avoids nameref
   # scoping collisions with the recursion).
   _WT_CURSOR=0
   while (( _WT_CURSOR < n )); do
      _janus_walk_node 0 || overall=1
   done
   (( __janus_errexit )) && set -e
   return ${overall}
}

# Walk a single node at index (by nameref via eval on $1), at the given depth.
# Advances the caller's index. Returns nonzero if any contained step failed.
_janus_walk_node() {
   local depth="$1"
   local n=${#_WT_TYPE[@]}
   local rc=0

   case "${_WT_TYPE[$_WT_CURSOR]}" in
      GROUP)
         local header="${_WT_HEADER[$_WT_CURSOR]}"
         local meta="${_WT_META[$_WT_CURSOR]}"  # async=<unbounded|N|>
         local async="${meta#async=}"
         if (( _WT_CURSOR > 0 )); then
           printf '\n'
         fi
         _janus_print_group "${header}" "${depth}"
         _WT_CURSOR=$(( _WT_CURSOR + 1 ))
         # Process children until the matching GROUP_END at this depth.
         local batch=()                         # indices of adjacent STEP children
         while (( _WT_CURSOR < n )); do
            if [[ "${_WT_TYPE[$_WT_CURSOR]}" == "GROUP_END" && "${_WT_DEPTH[$_WT_CURSOR]}" -eq "${depth}" ]]; then
               _WT_CURSOR=$(( _WT_CURSOR + 1 ))
               break
            fi
            case "${_WT_TYPE[$_WT_CURSOR]}" in
               STEP)
                  batch+=("$_WT_CURSOR")
                  _WT_CURSOR=$(( _WT_CURSOR + 1 ))
                  ;;
               GROUP)
                  # Barrier: flush the pending step batch first.
                  if (( ${#batch[@]} )); then _janus_run_batch "${depth}" "${async}" "${batch[@]}" || rc=1; fi
                  batch=()
                  _janus_walk_node $(( depth + 1 )) || rc=1
                  ;;
               *)
                  _WT_CURSOR=$(( _WT_CURSOR + 1 ))
                  ;;
            esac
         done
         # Flush any trailing steps.
         if (( ${#batch[@]} )); then _janus_run_batch "${depth}" "${async}" "${batch[@]}" || rc=1; fi
         ;;
      STEP)
         # A bare top-level step (not inside a group): run it alone, sequentially.
         _janus_run_batch "${depth}" "" "$_WT_CURSOR" || rc=1
         _WT_CURSOR=$(( _WT_CURSOR + 1 ))
         ;;
      *)
         _WT_CURSOR=$(( _WT_CURSOR + 1 ))
         ;;
   esac

   return ${rc}
}

# Run a batch of step indices with the given concurrency.
#   $1 = depth (of the containing group, for indentation)
#   $2 = async spec ("" => sync; "unbounded" => min(batch, ncpu); N => cap N)
#   $3.. = step indices into the _WT_* arrays
#
# Display model: steps are launched concurrently (bounded by cap), but reported
# strictly in REGISTRATION ORDER, one at a time. We focus on each step in turn:
# if it is still running we animate a single in-place spinner until it exits; if it
# already finished we emit its result immediately. The launch window is topped up as
# steps complete, so later steps keep making progress while we display earlier ones.
_janus_run_batch() {
   local depth="$1"; shift
   local async="$1"; shift
   local -a steps=("$@")
   [[ ${#steps[@]} -eq 0 ]] && return 0

   local cap
   if [[ -z "${async}" ]]; then
      cap=1
   elif [[ "${async}" == "unbounded" ]]; then
      # "unbounded" means "go as wide as sensible" — clamp to the CPU count so a
      # large batch can't fork hundreds of processes and thrash the machine. An
      # explicit async=N (below) is NOT clamped: if the user asks for a specific
      # width, that is their call.
      local ncpu; ncpu="$(_janus_ncpu)"
      cap=${#steps[@]}
      (( cap > ncpu )) && cap=${ncpu}
   else
      cap="${async}"
      (( cap < 1 )) && cap=1
   fi

   local spool; spool="$(_janus_spool)"
   local rc=0
   local total=${#steps[@]}

   # PID per batch position (parallel to `steps`); empty until launched.
   local -a step_pid=()
   local p
   for (( p=0; p<total; p++ )); do step_pid[$p]=""; done
   local launched=0   # how many positions have been started

   # Launch up to `cap` steps starting at batch position `from`.
   _janus_launch_window() {
      while (( launched < total )); do
         # Count currently-running launched-but-unreaped steps.
         local running=0 j
         for (( j=0; j<launched; j++ )); do
            [[ -n "${step_pid[$j]}" ]] && kill -0 "${step_pid[$j]}" 2>/dev/null && running=$(( running + 1 ))
         done
         (( running >= cap )) && break
         local idx="${steps[$launched]}"
         local uid="${_WT_ID[$idx]}"
         local fn; fn="$(_janus_meta_get "${_WT_META[$idx]}" fn)"
         local out="${spool}/step.${uid}.out"
         local body="${spool}/step.${uid}.body"
         # Run the step in a FORKED SUBSHELL so it inherits the script's functions,
         # sourced includes, and variables while its writes stay isolated.
         #   - If the step's function is visible in this shell, call it (normal case).
         #   - Otherwise the step was registered inside a subshell (e.g. the right
         #     side of a pipe), so its function died with that subshell; fall back to
         #     SOURCING the captured body in a forked subshell of the engine, which
         #     still runs in the main shell's context.
         if declare -F "${fn}" >/dev/null 2>&1; then
            ( "${fn}" ) >"${out}" 2>&1 &
         else
            ( source "${body}" ) >"${out}" 2>&1 &
         fi
         step_pid[$launched]="$!"
         launched=$(( launched + 1 ))
      done
   }

   _janus_launch_window

   # Walk the batch in order, displaying one step at a time.
   local pos
   for (( pos=0; pos<total; pos++ )); do
      # Make sure this position has been launched (cap may have deferred it).
      while [[ -z "${step_pid[$pos]}" ]]; do
         _janus_launch_window
         [[ -z "${step_pid[$pos]}" ]] && sleep 0.05
      done

      local idx="${steps[$pos]}"
      local uid="${_WT_ID[$idx]}"
      local out="${spool}/step.${uid}.out"
      local pid="${step_pid[$pos]}"
      local header="${_WT_HEADER[$idx]}"

      if [[ -t 1 ]]; then
         # TTY: the spinner owns the task line (header + animated spinner on ONE line).
         _janus_spin_on_line "${pid}" "${depth}" "${header}"
      else
         # Non-TTY: emit a start line, then wait with no animation.
         _janus_print_step_start "${header}" "${depth}"
         _janus_wait_with_spinner "${pid}" "${depth}"
      fi
      wait "${pid}"; local st=$?
      (( st != 0 )) && rc=1
      _janus_print_step_done "${header}" "${st}" "${out}" "${depth}" "${idx}"
   done

   unset -f _janus_launch_window 2>/dev/null
   return ${rc}
}

# Non-TTY waiter: no animation, just block until the PID exits, topping up the
# launch window each tick so other steps keep running.
_janus_wait_with_spinner() {
   local pid="$1" depth="$2"
   while kill -0 "${pid}" 2>/dev/null; do
      declare -f _janus_launch_window >/dev/null 2>&1 && _janus_launch_window
      sleep 0.1
   done
   return 0
}

# TTY spinner that shares the task line. Renders
#     <indent><header>  [ <spinner-glyph> ]
# redrawing in place (carriage return) each tick until the PID exits, then clears
# the line so the final result line (printed by the caller) replaces it.
#
# Padding matches the original janus-exec.sh: the spinner occupies a fixed visible
# area of JANUS_SPINNER_AREA columns. Because printf field widths count BYTES (not
# display columns), a multibyte glyph set needs a widened byte field:
#     SPINNER_WIDTH = (AREA - take) + (take * bytes_per_glyph)
: "${JANUS_SPINNER_AREA:=5}"

_janus_spin_on_line() {
   local pid="$1" depth="$2" header="$3"
   local pad; pad="$(_janus_indent $(( depth + 1 )))"

   # Resolve glyphs/take/bytes from spinner.inc; else a simple ASCII set.
   local glyphs take bytes
   if declare -f spinopts >/dev/null 2>&1; then
      local opts
      eval "opts=$(spinopts "${JANUS_SPINNER:-wave-bars}" "(%c %t %i %b)")"
      glyphs="${opts[0]}"; take="${opts[1]:-1}"; bytes="${opts[3]:-1}"
   else
      glyphs='|/-\'; take=1; bytes=1
   fi
   local nglyph=${#glyphs}
   (( nglyph < 1 )) && { glyphs='*'; nglyph=1; take=1; bytes=1; }

   # Byte-aware field width so the spinner box stays JANUS_SPINNER_AREA columns wide.
   local area="${JANUS_SPINNER_AREA}"
   local spin_w=$(( (area - take) + (take * bytes) ))
   (( spin_w < 1 )) && spin_w=1

   # Doubled glyph string so the sliding window wraps cleanly.
   local work="${glyphs}${glyphs}"

   # Pad the header so (indent + header) == global _WT_MAXHDR, aligning every box.
   local hdr_w=$(( ${_WT_MAXHDR:-0} - ${#pad} ))
   (( hdr_w < ${#header} )) && hdr_w=${#header}

   tput civis 2>/dev/null
   local i=0
   while kill -0 "${pid}" 2>/dev/null; do
      declare -f _janus_launch_window >/dev/null 2>&1 && _janus_launch_window
      local frame="${work:$(( i % nglyph )):${take}}"
      # Redraw the whole line: padded header then spinner (left-justified in spin_w bytes).
      cprintf "\r%s<fg:white>%-${hdr_w}s</fg>  [ <fg:blue>%-${spin_w}b</fg> ]" "${pad}" "${header}" "${frame}"
      i=$(( i + 1 ))
      sleep 0.1
   done
   tput cnorm 2>/dev/null
   # Clear the spinner line (carriage return + erase to end of line) so the caller's
   # result line starts clean regardless of header length.
   printf '\r\033[K' 2>/dev/null
}


# ---------------------------------------------------------------------------------------------------------------------
# Presentation (overridable). Context line counts can be tuned via env.
# ---------------------------------------------------------------------------------------------------------------------
: "${JANUS_CONTEXT_SUCCESS:=0}"
: "${JANUS_CONTEXT_FAIL:=25}"
: "${JANUS_INDENT:=2}"       # spaces per nesting level
: "${JANUS_OK_TEXT:=OK}"     # default success status word
: "${JANUS_ERR_TEXT:=ERR}"   # default failure status word

# Emit `depth * JANUS_INDENT` spaces.
_janus_indent() {
   local depth="${1:-0}" width=$(( ${1:-0} * JANUS_INDENT ))
   (( width > 0 )) && printf '%*s' "${width}" ''
}

_janus_print_group() {
   # $1 = header, $2 = depth
   local pad; pad="$(_janus_indent "${2:-0}")"
   cprintf "%s<fg:yellow+><b>%s</b></fg>\n" "${pad}" "$1"
}

_janus_print_step_start() {
   # $1 = header, $2 = depth (of the containing group); steps sit one level deeper
   local pad; pad="$(_janus_indent $(( ${2:-0} + 1 )))"
   cprintf "%s<fg:blue>%s</fg>\n" "${pad}" "$1"
}

_janus_print_step_done() {
   # $1=header $2=status $3=out $4=depth $5=step index (into _WT_* for meta lookup)
   # Layout mirrors the spinner line: "<header>  [ STATUS ]" so the TTY transition
   # from spinner to result stays in place.
   local header="$1" status="$2" out="$3" depth="${4:-0}" idx="${5:-}"
   local pad; pad="$(_janus_indent $(( depth + 1 )))"
   local opad; opad="$(_janus_indent $(( depth + 2 )))"

   # Per-step overrides (fall back to globals when unset).
   local meta="" o_succ="" o_fail="" o_ok="" o_err=""
   if [[ -n "${idx}" ]]; then
      meta="${_WT_META[$idx]}"
      o_succ="$(_janus_meta_get "${meta}" success)"
      o_fail="$(_janus_meta_get "${meta}" fail)"
      o_ok="$(_janus_meta_get "${meta}" ok)"
      o_err="$(_janus_meta_get "${meta}" err)"
   fi
   local succ_lines="${o_succ:-${JANUS_CONTEXT_SUCCESS}}"
   local fail_lines="${o_fail:-${JANUS_CONTEXT_FAIL}}"
   local ok_text="${o_ok:-${JANUS_OK_TEXT}}"
   local err_text="${o_err:-${JANUS_ERR_TEXT}}"

   # Pad the header to the global header column, and the status word to the global
   # status width (separate from the spinner width). Clear to end of line after the
   # status so a wider spinner remnant leaves no artifact.
   local hdr_w=$(( ${_WT_MAXHDR:-0} - ${#pad} ))
   (( hdr_w < ${#header} )) && hdr_w=${#header}
   local st_w="${_WT_STATUS_W:-3}"

   if (( status == 0 )); then
      cprintf "%s<fg:white>%-${hdr_w}s</fg>  [ <fg:green>%-${st_w}s</fg> ]\033[K\n" "${pad}" "${header}" "${ok_text}"
      if (( succ_lines > 0 )) && [[ -s "${out}" ]]; then
         tail -n "${succ_lines}" "${out}" | while IFS= read -r l; do
            cprintf "%s%s\n" "${opad}" "${l}"
         done
      fi
   else
      cprintf "%s<fg:white>%-${hdr_w}s</fg>  [ <fg:red>%-${st_w}s</fg> ]\033[K\n" "${pad}" "${header}" "${err_text}"
      if (( fail_lines > 0 )) && [[ -s "${out}" ]]; then
         tail -n "${fail_lines}" "${out}" | while IFS= read -r l; do
            cprintf "%s<fg:red>%s</fg>\n" "${opad}" "${l}"
         done
      fi
   fi
}
