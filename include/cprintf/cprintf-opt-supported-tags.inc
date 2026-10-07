#? Display all supported tags with examples
function xprintf() {
  printf "$> cprintf "
  printf " \"%s\"" "$@"
  printf "\n"
  cprintf "$@"
  printf '\n'
}

function cprintf::opt::supported-tags() {
    printf "=== Supported Tags ===\n\n"

    # Text Formatting Tags
    printf "Text Formatting Tags:\n"
    printf "  <b></b>           - Bold text\n"
    printf "  <i></i>           - Italic text\n"
    printf "  <u></u>           - Underlined text\n"
    printf "  <dim></dim>       - Dim text\n"
    printf "  <inv></inv>       - Inverted colors\n"
    printf "  <hidden></hidden> - Hidden text\n"
    printf "  <strike></strike> - Strikethrough text\n"
    printf "  <blink></blink>   - Blinking text\n\n"

    # Color Tags
    printf "Color Tags:\n"
    printf "  <fg:color></fg>   - Foreground color\n"
    printf "  <bg:color></bg>   - Background color\n\n"

    printf "=== Text Formatting Examples ===\n\n"
    xprintf "<b>Bold text</b>\n"
    xprintf "<i>Italic text</i>\n"
    xprintf "<u>Underlined text</u>\n"
    xprintf "<dim>Dim text</dim>\n"
    xprintf "<inv>Inverted text</inv>\n"
    xprintf "<strike>Strikethrough text</strike>\n"
    xprintf "<blink>Blinking text</blink>\n"
    printf "\n"

    printf "=== Color Examples ===\n\n"

    # Basic Colors
    printf "Basic Colors:\n"
    xprintf "<fg:red>Red text</fg>\n"
    xprintf "<fg:green>Green text</fg>\n"
    xprintf "<fg:blue>Blue text</fg>\n"
    xprintf "<fg:yellow>Yellow text</fg>\n"
    xprintf "<fg:magenta>Magenta text</fg>\n"
    xprintf "<fg:cyan>Cyan text</fg>\n"
    xprintf "<fg:white>White text</fg>\n"
    xprintf "<fg:black>Black text</fg>\n"
    printf "\n"

    # Background Colors
    printf "Background Colors:\n"
    xprintf "<bg:red>Red background</bg>\n"
    xprintf "<bg:green>Green background</bg>\n"
    xprintf "<bg:blue>Blue background</bg>\n"
    xprintf "<bg:yellow>Yellow background</bg>\n"
    printf "\n"

    # 256 Color Support
    printf "256 Color Support (using color codes):\n"
    xprintf "<fg:196>Bright red (196)</fg>\n"
    xprintf "<fg:46>Bright green (46)</fg>\n"
    xprintf "<fg:21>Bright blue (21)</fg>\n"
    xprintf "<fg:208>Orange (208)</fg>\n"
    printf "\n"

    # True Color Support
    printf "True Color Support (RGB hex values):\n"
    xprintf "<fg:#FF6B6B>Custom red (#FF6B6B)</fg>\n"
    xprintf "<fg:#4ECDC4>Custom teal (#4ECDC4)</fg>\n"
    xprintf "<fg:#45B7D1>Custom blue (#45B7D1)</fg>\n"
    printf "\n"

    printf "=== Color Modifiers ===\n\n"
    printf "Modifiers can be added to colors:\n"
    printf "  !  - Bold effect\n"
    printf "  *  - Italic effect\n"
    printf "  _  - Underline effect\n"
    printf "  ~  - Invert effect\n"
    printf "  +  - High intensity\n"
    printf "  -  - Low intensity\n\n"

    printf "Modifier Examples:\n"
    xprintf "<fg:red!>Bold red</fg>\n"
    xprintf "<fg:green*>Italic green</fg>\n"
    xprintf "<fg:blue_>Underlined blue</fg>\n"
    xprintf "<fg:yellow~>Inverted yellow</fg>\n"
    xprintf "<fg:magenta+>High intensity magenta</fg>\n"
    xprintf "<fg:cyan->Low intensity cyan</fg>\n"
    printf "\n"

    printf "Multiple Modifiers:\n"
    xprintf "<fg:red!*>Bold italic red</fg>\n"
    xprintf "<fg:green!_>Bold underlined green</fg>\n"
    xprintf "<fg:blue*_>Italic underlined blue</fg>\n"
    printf "\n"

    printf "=== Nested Tags ===\n\n"
    xprintf "<fg:red>Red <b>bold</b> text</fg>\n"
    xprintf "<bg:yellow><fg:black>Black on yellow <i>italic</i></fg></bg>\n"
    xprintf "<fg:blue><b><u>Bold underlined blue</u></b></fg>\n"
    printf "\n"

    printf "=== Color Aliases ===\n\n"
    printf "Standard color names are supported:\n"
    printf "  black, red, green, yellow, blue, magenta, cyan, white\n\n"

    printf "Bright colors (capitalized first letter):\n"
    printf "  Black, Red, Green, Yellow, Blue, Magenta, Cyan, White\n\n"

    printf "Bright Color Examples:\n"
    xprintf "<fg:Red>Bright red text</fg>\n"
    xprintf "<fg:Green>Bright green text</fg>\n"
    xprintf "<fg:Blue>Bright blue text</fg>\n"
    xprintf "<fg:Yellow>Bright yellow text</fg>\n"
    printf "\n"

    printf "Color Codes:\n"
    printf "  0-255: 256-color palette\n"
    printf "  #RRGGBB: True color hex values\n\n"

    printf "=== Complex Examples ===\n\n"
    xprintf "<fg:red!><b>ERROR:</b></fg> <fg:white>File not found: <u>%s</u></fg>\n" "config.txt"
    xprintf "<fg:green!><b>SUCCESS:</b></fg> <fg:cyan>Operation completed in <b>%.2f</b> seconds</fg>\n" "1.23"
    xprintf "<bg:blue><fg:white> INFO </fg></bg> <fg:blue>Processing <i>%d</i> items...</fg>\n" "42"
    printf "\n"

    printf "=== Usage as Library ===\n\n"
    printf "When sourced, use the cprintf function directly:\n"
    printf "  source ./cprintf\n"
    printf "  cprintf '<fg:green>Success:</fg> %%s\\n' 'Done'\n\n"
}
