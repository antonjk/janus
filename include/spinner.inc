[[ -n "${_SPINNER_INC_SOURCED:-}" ]] && return
_SPINNER_INC_SOURCED=1

function shutdown_spinner() {
  tput cnorm # reset cursor
}

function bytelen() {
   myvar="$1"
   oLang=$LANG oLcAll=$LC_ALL
   LANG=C LC_ALL=C
   bytlen=${#myvar}
   LANG=$oLang LC_ALL=$oLcAll
   printf "%d\n" $bytlen
}

function spinopts() {
   local PATTERN_COUNT="%C"
   local PATTERN_NAME="%N"
   local PATTERN_CHARS="%c"
   local PATTERN_TAKE="%t"
   local PATTERN_INC="%i"
   local PATTERN_BYTES="%b"

   local selected
   case $1 in
      0 | arrows) # SPIN_ARROWS=('←↖↑↗→↘↓↙' 1 1)
         selected=('arrows' '←↖↑↗→↘↓↙' 1 1 3)
         ;;
      1 | quad-square) # SPIN_SQUARE_QUAD=('◰◳◲◱' 1 1)
         selected=('quad-square' '◰◳◲◱' 1 1 3)
         ;;
      2 | quad-dots) #SPIN_DOTS_QUAD
         selected=('quad-dots' '⣾⣽⣻⢿⡿⣟⣯⣷' 1 1 3)
         ;;
      3 | quad-blocks) #SPIN_BLOCKS_QUAD
         selected=('quad-blocks' '▖▘▝▗' 1 1 3)
         ;;
      4 | quad-circle) # SPIN_CIRCLE_QUAD
         selected=('pie' '◴◷◶◵' 1 1 3)
         ;;
      5 | half-circle) #   SPIN_CIRCLE_HALF
         selected=('half-circle' '◐◓◑◒' 1 1 3)
         ;;
      6 | triangle) # SPIN_TRIANGLE
         selected=('triangle' '◢◣◤◥' 1 1 3)
         ;;
      7 | wave-dots) # SPIN_WAVE_DOTS
         selected=('wave-dots' '⠁⠂⠄⡀⢀⠠⠐⠈' 4 1 3)
         ;;
      8 | wave-bars) # SPIN_WAVE_BARS
         selected=('wave-bars' '▁▂▃▄▅▆▇█▇▆▅▄▃▂▁' 5 1 3)
         ;;
      9 | bars) # SPIN_BARS
         selected=('bars' '▉▊▋▌▍▎▏▎▍▌▋▊▉' 1 1 3)
         ;;
      10 | lines) # SPIN_BOX_OUT
         selected=('lines' '┤┘┴└├┌┬┐' 1 1 3)
         ;;
      11 | spin | *) # SPIN_BOX_OUT
         selected=('spin' '┤┘┴└├┌┬┐' 1 1 3)
         ;;
      *)
         selected=('11' '-\\|\/' 1 1 1)
         ;;
   esac
   echo "${2:-"%c %t %i %b"}" |
      sed -E "s/${PATTERN_NAME}/'${selected[0]}'/g" |
      sed -E "s/${PATTERN_CHARS}/'${selected[1]}'/g" |
      sed -E "s/${PATTERN_TAKE}/${selected[2]}/g" |
      sed -E "s/${PATTERN_INC}/${selected[3]}/g" |
      sed -E "s/${PATTERN_BYTES}/'${selected[4]}'/g" |
      sed -E "s/${PATTERN_COUNT}/12/g"
}

#
#  Get the current screen position of the cursor in the format <row>;<col>
#
function curpos() {
   # based on a script from http://invisible-island.net/xterm/xterm.faq.html
   exec </dev/tty
   oldstty=$(stty -g)
   stty raw -echo min 0
   # on my system, the following line can be replaced by the line below it
   printf "\033[6n" >/dev/tty
   # tput u7 > /dev/tty    # when TERM=xterm (and relatives)
   IFS=';' read -r -d 'R' pos
   stty $oldstty
   echo "${pos:2}"
}

#
#  Move the cursor to the specified location.
#
#  Parameters:
#     $1 - The location in the format <row>;<col>
#
function setPos() {
   printf "\033[$1H"
}

function spin() {
   local curpos="$(curpos)"
   local work="${2}${2}"
   local spin=$2
   local i=$(($3 % ${#spin[0]}))
   cprintf "${1:-%s}" "${work:$i:$4}"
   setPos "${curpos}"
}
#
#  Display a spinner at the current cursor location while the specified process is active
#
#  Parameters:
#
#     --pid <pid>
#           The process id to wait for
#     --spin <chars>
#           The spin characters to use
#     --take <number>
#           The number of characters to display
#     --inc <number>
#           The number of characters to skip
#     --pattern <string>
#        The printf pattern to use when displaying the spinner
#
function spinner() {
   local pid=${1}
   local spin="$2"
   local pattern="$3"
   if [[ -z "${spin}" ]]; then
      spin="$(spinopts "default" "(%c %t %i)")"
   fi
   eval "spin=${spin}"
   spin="${spin:-${SPIN_DEFAULT[0]}}"
   take=${spin[1]:-1}
   inc=${spin[2]:-1}
   pattern="${pattern:-%s}"
   local i=$RANDOM
   tput civis # cursor invisible

   spin "${pattern}" "${spin[0]}" i take
   ((i=i+inc))
   while kill -0 $pid 2>/dev/null; do
      sleep .25
      spin "${pattern}" "${spin[0]}" i take
      ((i=i+inc))
   done

   tput cnorm
   wait $pid # capture exit code
   return $?
}

#trap_add shutdown_spinner EXIT
