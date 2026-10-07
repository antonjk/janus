# Include for ansi codes
# Escape code
export ANSI_ESC=$'\033'

# Text effects (SGR codes)
export ANSI_DISABLE_EFFECT_PREFIX="2" # Disable effect 21=No bold, 22=No Dim, etc

export ANSI_BOLD=1          # Bold/bright text
export ANSI_DIM=2           # Dim/faint text
export ANSI_ITALIC=3        # Italic text
export ANSI_UNDERLINE=4     # Underlined text
export ANSI_BLINK=5         # Blinking text
export ANSI_INVERT=7        # Inverted foreground/background
export ANSI_HIDDEN=8        # Hidden/invisible text
export ANSI_STRIKETHROUGH=9 #< Strikethrough text

# Color codes
export ANSI_COLOR_BLACK=0
export ANSI_COLOR_RED=1
export ANSI_COLOR_GREEN=2
export ANSI_COLOR_YELLOW=3
export ANSI_COLOR_BLUE=4
export ANSI_COLOR_MAGENTA=5
export ANSI_COLOR_CYAN=6
export ANSI_COLOR_WHITE=7
export ANSI_COLOR_DEFAULT=9


# Extended color prefixes (256-color and true color)
export ANSI_FG_EXTENDED_COLOR_PREFIX="38;5;" # 256-color foreground prefix
export ANSI_BG_EXTENDED_COLOR_PREFIX="48;5;" # 256-color background prefix
export ANSI_FG_TRUE_COLOR_PREFIX="38;2;"     # True color foreground prefix
export ANSI_BG_TRUE_COLOR_PREFIX="48;2;"     # True color background prefix

# Standard and high-intensity color bases
export ANSI_FG_STANDARD_COLOR_BASE=30 # Standard foreground colors (30-37)
export ANSI_BG_STANDARD_COLOR_BASE=40 # Standard background colors (40-47)
export ANSI_FG_HIGH_COLOR_BASE=90     # High-intensity foreground colors (90-97)
export ANSI_BG_HIGH_COLOR_BASE=100    # High-intensity background colors (100-107)
