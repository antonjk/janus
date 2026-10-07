[[ -n "${_TEMP_INC_SOURCED:-}" ]] && return
_TEMP_INC_SOURCED=1

######################################################################################################################
#  Temp file support
######################################################################################################################

## ####################################################################################################################
##  Creates (and lazily initialises) a per-PID temp folder and returns a fresh temp file path within it.
##
##  Arguments:
##    $1 - optional mktemp template name (within the per-PID folder). If omitted, a
##         generic "process.XXXXXXXXX" template is used.
## ####################################################################################################################
function temp_file() {
   local SCRIPT_TEMP_FOLDER="/tmp/PID-$$"
   if [ ! -d "$SCRIPT_TEMP_FOLDER" ]; then
      mktemp -d "$SCRIPT_TEMP_FOLDER" >/dev/null
   fi
   if [ -z "$1" ]; then
      echo "$(mktemp "${SCRIPT_TEMP_FOLDER}/process.XXXXXXXXX")"
   else
      echo "$(mktemp "${SCRIPT_TEMP_FOLDER}/$1")"
   fi
}

## ####################################################################################################################
##  Returns the per-PID temp folder path for the given PID (default: current $$).
##  Does not create it. Useful for building sub-paths (e.g. the whisper spool dir).
## ####################################################################################################################
function temp_dir() {
   echo "/tmp/PID-${1:-$$}"
}

## ####################################################################################################################
##  Called to clean up the temp environment
## ####################################################################################################################
function cleanup_temp_environment() {
   local SCRIPT_TEMP_FOLDER="/tmp/PID-$1"

   if [ -d "$SCRIPT_TEMP_FOLDER" ]; then
      rm -rf "$SCRIPT_TEMP_FOLDER"
   fi
}

# Add an exit trap to clean the environment
trap_add "cleanup_temp_environment $$" EXIT
