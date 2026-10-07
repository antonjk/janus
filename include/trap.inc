[[ -n "${_TRAP_INC_SOURCED:-}" ]] && return
_TRAP_INC_SOURCED=1

#===================================================================
# FUNCTION trap_add ()
#
# Purpose:  appends a command to a trap
#
# - 1st arg:  code to add
# - remaining args:  names of traps to modify
#
# Example:  trap_add 'echo "in trap DEBUG"' DEBUG
#
# See: http://stackoverflow.com/questions/3338030/multiple-bash-traps-for-the-same-signal
#===================================================================
function trap_add() {
   trap_add_cmd=$1
   shift || fatal "${FUNCNAME} usage error"
   new_cmd=
   for trap_add_name in "$@"; do
      # Grab the currently defined trap commands for this trap
      existing_cmd=$(trap -p "${trap_add_name}" | awk -F"'" '{print $2}')

      # Define default command
      [ -z "${existing_cmd}" ] && existing_cmd="echo exiting @ $(date) >> /dev/null"

      if [ "$existing_cmd" == "${existing_cmd%$trap_add_cmd*}" ]; then
         # Generate the new command
         new_cmd="${existing_cmd};${trap_add_cmd}"
         # Assign the test
         trap "${new_cmd}" "${trap_add_name}" ||
            fatal "unable to add to trap ${trap_add_name}"
      fi
   done
}

function kill_descendant_processes() {
   local pid="$1"
   local and_self="${2:-false}"
   if children="$(pgrep -P "$pid")"; then
      for child in $children; do
         kill_descendant_processes "$child" true
      done
   fi
   if [[ "$and_self" == true ]]; then
      kill -9 "$pid"
   fi
}
