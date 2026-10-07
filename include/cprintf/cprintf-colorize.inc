declare -a ACTIVE_STATE=(0 0 0 0 0 0 0 0 0 0 '39' '49')
declare -a EFFECT_REFCOUNT=(0 0 0 0 0 0 0 0 0 0)
declare -a FG_STYLE=(0 0 0 0 0 0 0 0 0 0 '39')
declare -a BG_STYLE=(0 0 0 0 0 0 0 0 0 0 '49')
declare -a STYLES=()
declare -A TRANSFORMATIONS
STYLE_SET="DEFAULT"
ANSI_EFFECT_COUNT=10
FG_STATE=$((ANSI_EFFECT_COUNT))
BG_STATE=$((FG_STATE+1))
#-----------------------------------------------------------------------------------------------------------------------
# Generate and output ANSI escape sequence based on current style state.
#
# Compares current style state with effect counts and color stacks to determine which ANSI codes need to be output.
# Only outputs codes when changes are detected to minimize escape sequence overhead.
#
# State Management:
#   - Tracks 10 text effects (bold, dim, italic, underline, blink, invert, hidden, strikethrough)
#   - Tracks foreground color (index 10) and background color (index 11)
#   - Enables effects when count > 0 and currently disabled
#   - Disables effects when count = 0 and currently enabled
#   - Updates colors when top of color stack differs from current state
#
# Output Format:
#   ESC[code1;code2;...m where codes are semicolon-separated ANSI SGR parameters
#   Only outputs when changes detected (ansi_seq is non-empty)
#
# Global Variables:
#   _current_style - Current active style state (11 elements)
#   _effect_counts - Reference counts for each effect (10 elements)
#   _fg - Foreground color stack with effects
#   _bg - Background color stack with effects
#   ANSI_ESC - Escape character
#-----------------------------------------------------------------------------------------------------------------------
function cprintf::ansi_seq() {
    local ansi_seq effects=0
    for (( e = 1; e < ANSI_EFFECT_COUNT; e++ )); do
        ((EFFECT_REFCOUNT[e] > 0 && ACTIVE_STATE[e] == 0)) && {
            ACTIVE_STATE[e]=1
            ansi_seq+=";$e"
        }
        ((effects += ACTIVE_STATE[e]))
        ((EFFECT_REFCOUNT[e] == 0 && ACTIVE_STATE[e] > 0)) && {
            ACTIVE_STATE[e]=0
            [[ $e == 1 ]] && ansi_seq+=";${ANSI_DISABLE_EFFECT_PREFIX}2"
            [[ $e -gt 1 ]] && ansi_seq+=";${ANSI_DISABLE_EFFECT_PREFIX}$e"
        }

    done
    [[ "${ACTIVE_STATE[$FG_STATE]}" != "${FG_STYLE[-1]}" ]] && {
        ansi_seq+=";${FG_STYLE[-1]}"
        ACTIVE_STATE[$FG_STATE]="${FG_STYLE[-1]}"
    }
    [[ "${ACTIVE_STATE[$BG_STATE]}" != "${BG_STYLE[-1]}" ]] && {
        ansi_seq+=";${BG_STYLE[-1]}"
        ACTIVE_STATE[$BG_STATE]="${BG_STYLE[-1]}"
    }
    [[ -n "${ansi_seq}" ]] && printf "%b[%bm" "${ANSI_ESC}" "${ansi_seq:1}"
}

#-----------------------------------------------------------------------------------------------------------------------
# Add or remove a text effect by adjusting its reference count.
#
# Maintains reference counting for text effects to support nested tags. When count reaches zero,
# the effect is disabled in the next ANSI sequence output.
#
# Arguments:
#   $1 - Effect index (ANSI_BOLD, ANSI_DIM, ANSI_ITALIC, etc.)
#   $2 - Increment value (positive to add, negative to remove)
#
# Global Variables:
#   _effect_counts - Array of reference counts for each effect
#-----------------------------------------------------------------------------------------------------------------------
function cprintf::add_effect() {
    ((EFFECT_REFCOUNT[$1] += $2))
    ((EFFECT_REFCOUNT[$1] < 0)) && ((EFFECT_REFCOUNT[$1] = 0))
}

function cprintf::add_transform() {
    if [[ -z "${TRANSFORMATIONS[$1]}" ]]; then
        TRANSFORMATIONS[$1]=0
    fi
    ((TRANSFORMATIONS[$1]+=$2))
    ((TRANSFORMATIONS[$1] == 0)) && unset "TRANSFORMATIONS[$1]"
}

function cprintf::add_style() {
    local style_name="STYLE_SET_${STYLE_SET}_${1^^}"
    local -n style="${style_name}"
    cprintf::colorize "${style[0]}"
    STYLES+=("${style[1]}")
}

function cprintf::remove_style() {
    if [[ ${#STYLES[@]} -gt 0 ]]; then
        cprintf::colorize "${STYLES[-1]}"
        unset "STYLES[-1]"
    fi
}

#-----------------------------------------------------------------------------------------------------------------------
# Remove the most recent color from the color stack and restore previous state.
#
# Pops the top 11 elements from the specified color plane stack (10 effects + 1 color code).
# Decrements effect counts by the amounts stored in the popped color entry to restore
# the previous nesting level's effect state.
#
# Arguments:
#   $1 - Color plane: "fg" (foreground) or "bg" (background)
#
# Global Variables:
#   _fg - Foreground color stack
#   _bg - Background color stack
#   _effect_counts - Effect reference counts
#-----------------------------------------------------------------------------------------------------------------------
function cprintf::remove_color_effect() {
    local color_plane="$1"

    if [[ "$color_plane" == "fg" && ${#FG_STYLE[@]} -gt 11 ]]; then
        local offset=$((${#FG_STYLE[@]} - 11))
        for (( i = 0; i < 10; i++ )); do
            ((EFFECT_REFCOUNT[i] -= ${FG_STYLE[offset + i]}))
        done
        for (( i = 0; i < 11; i++ )); do
            unset "FG_STYLE[-1]"
        done
    elif [[ "$color_plane" == "bg" && ${#BG_STYLE[@]} -gt 11 ]]; then
        local offset=$((${#BG_STYLE[@]} - 11))
        for (( i = 0; i < 10; i++ )); do
            ((EFFECT_REFCOUNT[i] -= BG_STYLE[offset + i]))
        done
        for (( i = 0; i < 11; i++ )); do
            unset "BG_STYLE[-1]"
        done
    fi
}

#-----------------------------------------------------------------------------------------------------------------------
# Parse color specification and push color state onto color stack.
#
# Processes color codes with optional modifiers and inline effects, resolves to ANSI escape codes,
# and pushes the complete color state (effects + color) onto the appropriate color stack.
#
# Color Notation Support:
#   Decimal: 0-7 (standard), 9 (default), 16-255 (8-bit), 256+ (24-bit)
#   @ Prefix: @0-@255 (8-bit), @256+ (24-bit)
#   # Prefix: #0-#7 (standard), #8-#F (high), #00-#FF (8-bit), #RRGGBB (24-bit)
#   Aliases: lowercase (standard), Capitalized (high intensity)
#
# Inline Modifiers:
#   + - Increase intensity (standard→high, high→bold, 8-bit/24-bit→bold)
#   - - Decrease intensity (high→standard, any→dim)
#   ! - Bold effect
#   * - Italic effect
#   _ - Underline effect
#   = - Strikethrough effect
#   ~ - Invert effect
#
# Processing Order:
#   1. Strip all spaces from color code
#   2. Process trailing modifiers right-to-left
#   3. Resolve color aliases
#   4. Parse color notation and determine color space
#   5. Apply intensity modifiers
#   6. Convert to ANSI escape code format
#   7. Push color state (10 effects + color code) onto stack
#
# Arguments:
#   $1 - Color plane: "fg" (foreground) or "bg" (background)
#   $2 - Color specification with optional modifiers (e.g., "red+!", "#FF0000", "@196*")
#
# Global Variables:
#   _fg - Foreground color stack
#   _bg - Background color stack
#   _effect_counts - Effect reference counts
#   _ANSI_COLOR_ALIASES__ - Color name to code mapping
#   ANSI_* - Effect index constants
#   ANSI_*_COLOR_BASE - Base codes for standard/high colors
#   ANSI_*_COLOR_PREFIX - Prefixes for extended/true colors
#-----------------------------------------------------------------------------------------------------------------------
function cprintf::add_color_effect() {
    local color_plane="$1"
    local color_code="${2// /}"
    local color_space
    local ansi_code
    local intensity=0
    local color=(0 0 0 0 0 0 0 0 0 0 "")
    while [[ ${#color_code} -gt 1 ]]; do
        case ${color_code: -1} in
            \+) ((intensity += 1)) ;;
            -) ((intensity -= 1)) ;;
            \*) ((color[ANSI_ITALIC] = 1)) ;;
            \!) ((color[ANSI_BOLD] = 1)) ;;
            _) ((color[ANSI_UNDERLINE] = 1)) ;;
            =) ((color[ANSI_STRIKETHROUGH] = 1)) ;;
            \~) ((color[ANSI_INVERT] = 1)) ;;
            *)
                lookup="${ANSI_COLOR_ALIAS[${color_code}]}"
                if [[ -n "${lookup}" ]]; then
                    color_code="${lookup}"
                    continue
                fi
                break
                ;;
        esac
        color_code="${color_code:0:-1}"
    done

    # Prefix #
    #  '#' Followed by 6 hex digits - true colors
    if [[ "$color_code" =~ ^#[0-9a-fA-F]{6}$ ]]; then
        color_space="TRUE"
        local hex="${color_code:1}"
        hex="${hex^^}"
        local r=$((16#${hex:0:2}))
        local g=$((16#${hex:2:2}))
        local b=$((16#${hex:4:2}))
        ansi_code="$r;$g;$b"
        #  '#' Followed by 2 hex digits - Extended colors
    elif [[ "$color_code" =~ ^#[0-9a-fA-F]{2}$ ]]; then
        color_space="EXTENDED"
        ansi_code=$((16#${color_code:1:3}))
        #  '#' Followed by 1 hex digit 0-8, standard colors
    elif [[ "$color_code" =~ ^#[0-7]$ ]]; then
        #  '#' Followed by 1 hex digit 9-F, high colors
        color_space="STANDARD"
        ansi_code=${color_code:1:2}
    elif [[ "$color_code" =~ ^#[8-9A-Fa-f]$ ]]; then
        color_space="HIGH"
        ansi_code=$((16#${color_code:1:2}))
        ((ansi_code -= 8))
    elif [[ "$color_code" =~ ^@[0-9]+$ ]]; then
        ansi_code="${color_code:1}"
        if [[ ${ansi_code} -lt 256 ]]; then
            color_space="EXTENDED"
        else
            color_space="TRUE"
        fi
    elif [[ "$color_code" =~ ^[0-9]+$ ]]; then
        ansi_code="$color_code"
        if [[ ${ansi_code} -lt 8 || ${ansi_code} -eq 9 ]]; then
            color_space="STANDARD"
        elif [[ ${ansi_code} -lt 256 ]]; then
            color_space="EXTENDED"
        else
            color_space="STANDARD"
            ansi_code=${ANSI_COLOR_DEFAULT}
        fi
    fi
    [[ "${intensity}" -gt 0 ]] && [[ "${color_space}" == "STANDARD" ]] && {
        color_space="HIGH"
        ((intensity--))
    }
    [[ "${intensity}" -gt 0 ]] && {
        color[ANSI_BOLD]=1
        ((intensity--))
    }
    [[ "${intensity}" -lt 0 ]] && [[ "${color_space}" == "HIGH" ]] && {
        color_space="STANDARD"
        ((intensity++))
    }
    [[ "${intensity}" -lt 0 ]] && {
        color[ANSI_DIM]=1
        ((intensity--))
    }
    if [[ "${color_space}" == "STANDARD" ]]; then
        local base_var="ANSI_${color_plane^^}_STANDARD_COLOR_BASE"
        ansi_code="$((ansi_code + ${!base_var}))"
        # if color code < 16, color space = high
    elif [[ "${color_space}" == "HIGH" ]]; then
        local base_var="ANSI_${color_plane^^}_HIGH_COLOR_BASE"
        ansi_code="$(((ansi_code) + ${!base_var}))"
        # if color code < 255, color space = extended
    elif [[ "${color_space}" == "EXTENDED" ]]; then
        local prefix_var="ANSI_${color_plane^^}_EXTENDED_COLOR_PREFIX"
        ansi_code="${!prefix_var}${ansi_code}"
    elif [[ -n "${ansi_code}" ]]; then
        local prefix_var="ANSI_${color_plane^^}_TRUE_COLOR_PREFIX"
        ansi_code="${!prefix_var}${ansi_code}"
    else
        local base_var="ANSI_${color_plane^^}_STANDARD_COLOR_BASE"
        ansi_code="$((9 + ${!base_var}))"
    fi
    color[10]="${ansi_code}"
    for (( i = 0; i < 10; i++ )); do
        ((EFFECT_REFCOUNT[i] += color[i]))
    done
    [[ "${color_plane}" == "fg" ]] && FG_STYLE+=(${color[@]})
    [[ "${color_plane}" == "bg" ]] && BG_STYLE+=(${color[@]})

}

#-----------------------------------------------------------------------------------------------------------------------
# Decode HTML entities and output text.
#
# Converts common HTML entities to their literal characters before output:
#   &lt; → <
#   &gt; → >
#   &amp; → &
#
# Arguments:
#   $1 - Text string potentially containing HTML entities
#
# Output:
#   Decoded text to stdout
#-----------------------------------------------------------------------------------------------------------------------
function cprintf::print() {

    local text="$1"
    text="${text//\&lt;/<}"
    text="${text//\&gt;/>}"
    text="${text//\&amp;/\&}"

    # Get keys and values
    for key in "${!TRANSFORMATIONS[@]}"; do
        case "${key}" in
        upper) text="${text^^}";;
        lower) text="${text,,}";;
        *)
        esac
    done
    printf -- "%s" "${text}"
}

#-----------------------------------------------------------------------------------------------------------------------
# Parse XML-style markup tags and convert to ANSI escape sequences.
#
# Processes text containing markup tags for colors and text effects, converting them to ANSI escape codes
# while maintaining proper nesting and state management. Supports nested tags and inline modifiers.
#
# Supported Tags:
#   Text Effects: <b>, <dim>, <i>, <u>, <blink>, <inv>, <hidden>, <strike>
#   Colors: <fg:color>, <bg:color>
#   Closing: </tag> to close any tag
#
# Tag Processing:
#   - Opening tags increment effect/color state
#   - Closing tags (/) decrement effect/color state
#   - Text between tags is output with current ANSI state
#   - HTML entities in text are decoded before output
#
# Arguments:
#   $1 - Text string containing markup tags
#
# Output:
#   Text with ANSI escape sequences to stdout
#
# Examples:
#   cprintf::colorize "<fg:red>Error:</fg> <b>File not found</b>"
#   cprintf::colorize "<fg:blue!*>Bold italic blue</fg>"
#   cprintf::colorize "<bg:yellow><fg:black>Warning</fg></bg>"
#-----------------------------------------------------------------------------------------------------------------------
function cprintf::colorize() {
    local _text="$1" text _code _inc

    while [[ ${#_text} -gt 0 && "$_text" == *"<"* ]]; do
        text="${_text%%<*}"
        _text="${_text:${#text}}"

        [[ ${#text} -gt 0 ]] && {
            cprintf::ansi_seq
            cprintf::print "${text}"
        }

        [[ "$_text" != "<"*">"* ]] && break

        _code="${_text%%>*}"
        _text="${_text:$((${#_code} + 1))}"
        _code="${_code:1}"
#        echo "${_code}"

        [[ "${_code:0:1}" == "/" ]] && {
            _code="${_code:1}"
            _inc=-1
        } || _inc=1
        case ${_code} in
            @*)
                [[ "$_inc" -gt 0 ]] && cprintf::add_style "${_code:1}"
                [[ "$_inc" -lt 0 ]] && cprintf::remove_style
                ;;
            b) cprintf::add_effect $ANSI_BOLD $_inc ;;
            dim) cprintf::add_effect $ANSI_DIM $_inc ;;
            i) cprintf::add_effect $ANSI_ITALIC $_inc ;;
            u) cprintf::add_effect $ANSI_UNDERLINE $_inc ;;
            blink) cprintf::add_effect $ANSI_BLINK $_inc ;;
            inv) cprintf::add_effect $ANSI_INVERT $_inc ;;
            hidden) cprintf::add_effect $ANSI_HIDDEN $_inc ;;
            strike) cprintf::add_effect $ANSI_STRIKETHROUGH $_inc ;;
            upper) cprintf::add_transform "${_code}" $_inc        ;;
            lower) cprintf::add_transform "${_code}" $_inc        ;;
            fg*)
                [[ "$_inc" -gt 0 ]] && cprintf::add_color_effect "fg" "${_code:3}"
                [[ "$_inc" -lt 0 ]] && cprintf::remove_color_effect "fg"
                ;;
            bg*)
                [[ "$_inc" -gt 0 ]] && cprintf::add_color_effect "bg" "${_code:3}"
                [[ "$_inc" -lt 0 ]] && cprintf::remove_color_effect "bg"
                ;;
        esac
    done

    cprintf::ansi_seq
    if [[ ${#_text} -gt 0 ]]; then
        cprintf::print "${_text}"
    fi
}
