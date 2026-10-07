[[ -n "${_WHISPER_INC_SOURCED:-}" ]] && return
_WHISPER_INC_SOURCED=1

########################################################################################################################
# Whisper runtime
#
# Registry API (used by transpiled scripts):
#   whisper-group [--async[=N]] "<header>"   open a group (branch)
#   whisper-group-end                        close the current group
#   whisper-step  [--id X] [--depends a,b] "<header>" <<DELIM ... DELIM
#                                            register a leaf; body read from stdin (NOT executed yet)
#   whisper-exec                             run the registered tree
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

# Dependencies (sourced by the transpiled script's prologue, but guard here too):
#   trap.inc  -> trap_add, kill_descendant_processes
#   temp.inc  -> temp_dir (per-PID folder)
#   cprintf.inc -> cprintf
#   spinner.inc -> (optional) spinner visuals

# ---------------------------------------------------------------------------------------------------------------------
# Spool dir + registry state
# ---------------------------------------------------------------------------------------------------------------------

_whisper_spool() {
   # Per-PID spool dir for the whisper tree. Created on first use.
   local base
   if declare -f temp_dir >/dev/null 2>&1; then
      base="$(temp_dir)"
   else
      base="/tmp/PID-$$"
   fi
   printf '%s/whisper' "${base}"
}

_whisper_init() {
   local spool; spool="$(_whisper_spool)"
   [[ -d "${spool}" ]] && return 0
   mkdir -p "${spool}" || return 1
   : >"${spool}/tree"
   _WHISPER_DEPTH=0
   _WHISPER_UID=0
   # Clean up spooled children on interrupt if trap.inc is available.
   if declare -f trap_add >/dev/null 2>&1; then
      trap_add '_whisper_cleanup' EXIT INT TERM
   fi
}

_whisper_cleanup() {
   # Best-effort: kill any still-running step children.
   if declare -f kill_descendant_processes >/dev/null 2>&1; then
      kill_descendant_processes "$$" false 2>/dev/null
   fi
}

# tree record format (one line): TYPE|depth|id|meta|header
#   TYPE in GROUP | GROUP_END | STEP
_whisper_record() {
   local spool; spool="$(_whisper_spool)"
   printf '%s\n' "$1" >>"${spool}/tree"
}

# ---------------------------------------------------------------------------------------------------------------------
# Registry API
# ---------------------------------------------------------------------------------------------------------------------

whisper-group() {
   _whisper_init
   local async="" header=""
   while [[ $# -gt 0 ]]; do
      case "$1" in
         --async)    async="unbounded"; shift ;;
         --async=*)  async="${1#--async=}"; shift ;;
         --)         shift; break ;;
         -*)         shift ;;              # ignore unknown flags for forward-compat
         *)          break ;;
      esac
   done
   header="$1"
   local id="g$(( ++_WHISPER_UID ))"
   local meta="async=${async}"
   _whisper_record "GROUP|${_WHISPER_DEPTH}|${id}|${meta}|${header}"
   _WHISPER_DEPTH=$(( _WHISPER_DEPTH + 1 ))
}

whisper-group-end() {
   _whisper_init
   # A group opened at level D raised _WHISPER_DEPTH to D+1. Record the GROUP_END at
   # the group's OWN level D (so the walker can match it against the opening GROUP),
   # then drop back to D.
   (( _WHISPER_DEPTH > 0 )) && _WHISPER_DEPTH=$(( _WHISPER_DEPTH - 1 ))
   _whisper_record "GROUP_END|${_WHISPER_DEPTH}|||"
}

whisper-step() {
   _whisper_init
   local id="" depends="" header=""
   while [[ $# -gt 0 ]]; do
      case "$1" in
         --id)        id="$2"; shift 2 ;;
         --id=*)      id="${1#--id=}"; shift ;;
         --depends)   depends="$2"; shift 2 ;;
         --depends=*) depends="${1#--depends=}"; shift ;;
         --)          shift; break ;;
         -*)          shift ;;
         *)           break ;;
      esac
   done
   header="$1"
   local uid="s$(( ++_WHISPER_UID ))"
   [[ -z "${id}" ]] && id="${uid}"
   local spool; spool="$(_whisper_spool)"
   # Capture the body (heredoc on stdin). Expansion already happened at registration
   # because the transpiler uses an UNQUOTED heredoc delimiter.
   cat >"${spool}/step.${uid}.body"
   local meta="depends=${depends}"
   _whisper_record "STEP|${_WHISPER_DEPTH}|${uid}|${meta}|${header}"
}

# ---------------------------------------------------------------------------------------------------------------------
# Engine (whisper-exec)
# ---------------------------------------------------------------------------------------------------------------------

# Parse the flat tree into parallel arrays, then walk it.
whisper-exec() {
   # The engine uses arithmetic (( )) and poll loops that are not `set -e` safe.
   # Neutralise the caller's errexit for the duration of execution.
   local __whisper_errexit=0; [[ $- == *e* ]] && __whisper_errexit=1
   set +e
   local spool; spool="$(_whisper_spool)"
   [[ -r "${spool}/tree" ]] || { ceprintf "<fg:red>whisper: no registered steps</fg>\n"; (( __whisper_errexit )) && set -e; return 1; }

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
   # matching the original whisper-exec.sh WIDTH).
   _WT_MAXHDR=0
   local _k
   for (( _k=0; _k<n; _k++ )); do
      [[ "${_WT_TYPE[$_k]}" == "STEP" ]] || continue
      local _indent_w=$(( ( ${_WT_DEPTH[$_k]} + 1 ) * WHISPER_INDENT ))
      local _w=$(( _indent_w + ${#_WT_HEADER[$_k]} ))
      (( _w > _WT_MAXHDR )) && _WT_MAXHDR=${_w}
   done

   local overall=0
   # Walk the top level (depth 0 nodes), recursing into groups.
   # _WT_CURSOR is a global index advanced by _whisper_walk_node (avoids nameref
   # scoping collisions with the recursion).
   _WT_CURSOR=0
   while (( _WT_CURSOR < n )); do
      _whisper_walk_node 0 || overall=1
   done
   (( __whisper_errexit )) && set -e
   return ${overall}
}

# Walk a single node at index (by nameref via eval on $1), at the given depth.
# Advances the caller's index. Returns nonzero if any contained step failed.
_whisper_walk_node() {
   local depth="$1"
   local n=${#_WT_TYPE[@]}
   local rc=0

   case "${_WT_TYPE[$_WT_CURSOR]}" in
      GROUP)
         local header="${_WT_HEADER[$_WT_CURSOR]}"
         local meta="${_WT_META[$_WT_CURSOR]}"  # async=<unbounded|N|>
         local async="${meta#async=}"
         _whisper_print_group "${header}" "${depth}"
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
                  if (( ${#batch[@]} )); then _whisper_run_batch "${depth}" "${async}" "${batch[@]}" || rc=1; fi
                  batch=()
                  _whisper_walk_node $(( depth + 1 )) || rc=1
                  ;;
               *)
                  _WT_CURSOR=$(( _WT_CURSOR + 1 ))
                  ;;
            esac
         done
         # Flush any trailing steps.
         if (( ${#batch[@]} )); then _whisper_run_batch "${depth}" "${async}" "${batch[@]}" || rc=1; fi
         ;;
      STEP)
         # A bare top-level step (not inside a group): run it alone, sequentially.
         _whisper_run_batch "${depth}" "" "$_WT_CURSOR" || rc=1
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
#   $2 = async spec ("" => sequential/size-1; "unbounded" => no cap; N => cap N)
#   $3.. = step indices into the _WT_* arrays
#
# Display model: steps are launched concurrently (bounded by cap), but reported
# strictly in REGISTRATION ORDER, one at a time. We focus on each step in turn:
# if it is still running we animate a single in-place spinner until it exits; if it
# already finished we emit its result immediately. The launch window is topped up as
# steps complete, so later steps keep making progress while we display earlier ones.
_whisper_run_batch() {
   local depth="$1"; shift
   local async="$1"; shift
   local -a steps=("$@")
   [[ ${#steps[@]} -eq 0 ]] && return 0

   local cap
   if [[ -z "${async}" ]]; then
      cap=1
   elif [[ "${async}" == "unbounded" ]]; then
      cap=${#steps[@]}
   else
      cap="${async}"
      (( cap < 1 )) && cap=1
   fi

   local spool; spool="$(_whisper_spool)"
   local rc=0
   local total=${#steps[@]}

   # PID per batch position (parallel to `steps`); empty until launched.
   local -a step_pid=()
   local p
   for (( p=0; p<total; p++ )); do step_pid[$p]=""; done
   local launched=0   # how many positions have been started

   # Launch up to `cap` steps starting at batch position `from`.
   _whisper_launch_window() {
      while (( launched < total )); do
         # Count currently-running launched-but-unreaped steps.
         local running=0 j
         for (( j=0; j<launched; j++ )); do
            [[ -n "${step_pid[$j]}" ]] && kill -0 "${step_pid[$j]}" 2>/dev/null && running=$(( running + 1 ))
         done
         (( running >= cap )) && break
         local idx="${steps[$launched]}"
         local uid="${_WT_ID[$idx]}"
         local body="${spool}/step.${uid}.body"
         local out="${spool}/step.${uid}.out"
         ( bash "${body}" ) >"${out}" 2>&1 &
         step_pid[$launched]="$!"
         launched=$(( launched + 1 ))
      done
   }

   _whisper_launch_window

   # Walk the batch in order, displaying one step at a time.
   local pos
   for (( pos=0; pos<total; pos++ )); do
      # Make sure this position has been launched (cap may have deferred it).
      while [[ -z "${step_pid[$pos]}" ]]; do
         _whisper_launch_window
         [[ -z "${step_pid[$pos]}" ]] && sleep 0.05
      done

      local idx="${steps[$pos]}"
      local uid="${_WT_ID[$idx]}"
      local out="${spool}/step.${uid}.out"
      local pid="${step_pid[$pos]}"
      local header="${_WT_HEADER[$idx]}"

      if [[ -t 1 ]]; then
         # TTY: the spinner owns the task line (header + animated spinner on ONE line).
         _whisper_spin_on_line "${pid}" "${depth}" "${header}"
      else
         # Non-TTY: emit a start line, then wait with no animation.
         _whisper_print_step_start "${header}" "${depth}"
         _whisper_wait_with_spinner "${pid}" "${depth}"
      fi
      wait "${pid}"; local st=$?
      (( st != 0 )) && rc=1
      _whisper_print_step_done "${header}" "${st}" "${out}" "${depth}"
   done

   unset -f _whisper_launch_window 2>/dev/null
   return ${rc}
}

# Non-TTY waiter: no animation, just block until the PID exits, topping up the
# launch window each tick so other steps keep running.
_whisper_wait_with_spinner() {
   local pid="$1" depth="$2"
   while kill -0 "${pid}" 2>/dev/null; do
      declare -f _whisper_launch_window >/dev/null 2>&1 && _whisper_launch_window
      sleep 0.1
   done
   return 0
}

# TTY spinner that shares the task line. Renders
#     <indent><header>  [ <spinner-glyph> ]
# redrawing in place (carriage return) each tick until the PID exits, then clears
# the line so the final result line (printed by the caller) replaces it.
#
# Padding matches the original whisper-exec.sh: the spinner occupies a fixed visible
# area of WHISPER_SPINNER_AREA columns. Because printf field widths count BYTES (not
# display columns), a multibyte glyph set needs a widened byte field:
#     SPINNER_WIDTH = (AREA - take) + (take * bytes_per_glyph)
: "${WHISPER_SPINNER_AREA:=5}"

_whisper_spin_on_line() {
   local pid="$1" depth="$2" header="$3"
   local pad; pad="$(_whisper_indent $(( depth + 1 )))"

   # Resolve glyphs/take/bytes from spinner.inc; else a simple ASCII set.
   local glyphs take bytes
   if declare -f spinopts >/dev/null 2>&1; then
      local opts
      eval "opts=$(spinopts "${WHISPER_SPINNER:-wave-bars}" "(%c %t %i %b)")"
      glyphs="${opts[0]}"; take="${opts[1]:-1}"; bytes="${opts[3]:-1}"
   else
      glyphs='|/-\'; take=1; bytes=1
   fi
   local nglyph=${#glyphs}
   (( nglyph < 1 )) && { glyphs='*'; nglyph=1; take=1; bytes=1; }

   # Byte-aware field width so the spinner box stays WHISPER_SPINNER_AREA columns wide.
   local area="${WHISPER_SPINNER_AREA}"
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
      declare -f _whisper_launch_window >/dev/null 2>&1 && _whisper_launch_window
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
: "${WHISPER_CONTEXT_SUCCESS:=2}"
: "${WHISPER_CONTEXT_FAIL:=25}"
: "${WHISPER_INDENT:=2}"   # spaces per nesting level

# Emit `depth * WHISPER_INDENT` spaces.
_whisper_indent() {
   local depth="${1:-0}" width=$(( ${1:-0} * WHISPER_INDENT ))
   (( width > 0 )) && printf '%*s' "${width}" ''
}

_whisper_print_group() {
   # $1 = header, $2 = depth
   local pad; pad="$(_whisper_indent "${2:-0}")"
   cprintf "%s<fg:yellow+><b>%s</b></fg>\n" "${pad}" "$1"
}

_whisper_print_step_start() {
   # $1 = header, $2 = depth (of the containing group); steps sit one level deeper
   local pad; pad="$(_whisper_indent $(( ${2:-0} + 1 )))"
   cprintf "%s<fg:blue>%s</fg>\n" "${pad}" "$1"
}

_whisper_print_step_done() {
   # $1 = header, $2 = status, $3 = out file, $4 = depth (of containing group)
   # Layout mirrors the spinner line: "<header>  [ STATUS ]" so the TTY transition
   # from spinner to result stays in place.
   local header="$1" status="$2" out="$3" depth="${4:-0}"
   local pad; pad="$(_whisper_indent $(( depth + 1 )))"
   local opad; opad="$(_whisper_indent $(( depth + 2 )))"
   # Pad the header so (indent + header) == global _WT_MAXHDR, aligning every box.
   local hdr_w=$(( ${_WT_MAXHDR:-0} - ${#pad} ))
   (( hdr_w < ${#header} )) && hdr_w=${#header}
   if (( status == 0 )); then
      cprintf "%s<fg:white>%-${hdr_w}s</fg>  [ <fg:green>%-${WHISPER_SPINNER_AREA}s</fg> ]\n" "${pad}" "${header}" "OK"
      if (( WHISPER_CONTEXT_SUCCESS > 0 )) && [[ -s "${out}" ]]; then
         tail -n "${WHISPER_CONTEXT_SUCCESS}" "${out}" | while IFS= read -r l; do
            cprintf "%s%s\n" "${opad}" "${l}"
         done
      fi
   else
      cprintf "%s<fg:white>%-${hdr_w}s</fg>  [ <fg:red>%-${WHISPER_SPINNER_AREA}s</fg> ]\n" "${pad}" "${header}" "ERR"
      if (( WHISPER_CONTEXT_FAIL > 0 )) && [[ -s "${out}" ]]; then
         tail -n "${WHISPER_CONTEXT_FAIL}" "${out}" | while IFS= read -r l; do
            cprintf "%s<fg:red>%s</fg>\n" "${opad}" "${l}"
         done
      fi
   fi
}
