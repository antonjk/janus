[[ -n "${_CPRINTF_INC_SOURCED:-}" ]] && return
_CPRINTF_INC_SOURCED=1

########################################################################################################################
# Janus color wrapper.
#
# Provides `cprintf` / `ceprintf` (printf with <fg:...>, <b>, etc. markup). Resolution order:
#   1. A `cprintf` already on PATH (e.g. a system install of antonjk/bash-cprintf).
#   2. The vendored copy at include/cprintf/cprintf (sourced as a library).
#   3. A plain-printf stub that strips `<...>` markup, so the engine never hard-fails
#      on formatting even where cprintf is unavailable.
#
# Upstream: https://github.com/antonjk/bash-cprintf (MIT). Vendored under include/cprintf/.
########################################################################################################################

_CPRINTF_INC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if command -v cprintf >/dev/null 2>&1; then
   # (1) PATH install. Source it as a library so the cprintf function is defined in-process.
   # The upstream script defines the function when sourced.
   # shellcheck disable=SC1090
   . "$(command -v cprintf)"
   _CPRINTF_BACKEND="path"
elif [[ -r "${_CPRINTF_INC_DIR}/cprintf/cprintf" ]]; then
   # (2) Vendored copy.
   # shellcheck disable=SC1090
   . "${_CPRINTF_INC_DIR}/cprintf/cprintf"
   _CPRINTF_BACKEND="vendored"
else
   _CPRINTF_BACKEND="stub"
fi

# If neither backend defined cprintf, install plain-printf stubs that strip markup.
if ! declare -f cprintf >/dev/null 2>&1; then
   # Strip any <...> tags, then printf.
   function cprintf() {
      local template="$1"; shift
      template="$(printf '%s' "${template}" | sed -E 's/<[^>]*>//g')"
      # shellcheck disable=SC2059
      printf -- "${template}" "$@"
   }
fi

# Ensure ceprintf (stderr variant) exists regardless of backend.
if ! declare -f ceprintf >/dev/null 2>&1; then
   function ceprintf() {
      cprintf "$@" >&2
   }
fi
