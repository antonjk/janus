#? Display color code reference with all notation formats
# Source ANSI codes
STYLE_H1='<fg:cyan+>%s</fg>\n'
STYLE_H2='<fg:yellow+>%s</fg>\n'
STYLE_H3='<fg:magenta>%s</fg>\n'
STYLE_H4='<fg:white+>%s</fg>\n'
STYLE_ITALIC='<i>%s</i>' # *text*
STYLE_UNDERLINE='<u>%s</u>' # _text_
STYLE_EMPHASIS_1='<b>%s</b>' # **text**
STYLE_EMPHASIS_2='<i><b>%s</b></i>' # ***text***
STYLE_CODE='<bg:black+><fg:green>%s</fg></bg>' # `code`
function cprintf::colorize-headings() {
    local line="$1"
    # Convert `code` to <fg:green>code</fg>
    while [[ "$line" =~ (.*)\`([^\`]+)\`(.*) ]]; do
        line="$(printf "%s$STYLE_CODE%s" "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}" "${BASH_REMATCH[3]}")"
    done

    # Convert ***text*** to <b><i>text</i></b>
    while [[ "$line" =~ (.*)\*\*\*([^*]+)\*\*\*(.*) ]]; do
        line="$(printf "%s$STYLE_EMPHASIS_2%s" "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}" "${BASH_REMATCH[3]}")"
    done

    # Convert **text** to <b>text</b>
    while [[ "$line" =~ (.*)\*\*([^*]+)\*\*(.*) ]]; do
        line="$(printf "%s$STYLE_EMPHASIS_1%s" "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}" "${BASH_REMATCH[3]}")"
    done

    # Convert *text* to <i>text</i>
    while [[ "$line" =~ (.*)\*([^*]+)\*(.*) ]]; do
        line="$(printf "%s$STYLE_ITALIC%s" "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}" "${BASH_REMATCH[3]}")"
    done

    # Convert _text_ to <u>text</u>
    while [[ "$line" =~ (.*)_([^_]+)_(.*) ]]; do
        line="$(printf "%s$STYLE_UNDERLINE%s" "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}" "${BASH_REMATCH[3]}")"
    done

    # Process headings
    if [[ "$line" =~ ^####[[:space:]](.+)$ ]]; then
        cprintf "$STYLE_H4" "$(cprintf "${BASH_REMATCH[1]}")"
    elif [[ "$line" =~ ^###[[:space:]](.+)$ ]]; then
        cprintf "$STYLE_H3" "$(cprintf "${BASH_REMATCH[1]}")"
    elif [[ "$line" =~ ^##[[:space:]](.+)$ ]]; then
        cprintf "$STYLE_H2" "$(cprintf "${BASH_REMATCH[1]}")"
    elif [[ "$line" =~ ^#[[:space:]](.+)$ ]]; then
        cprintf "$STYLE_H1" "$(cprintf "${BASH_REMATCH[1]}")"
    else
        cprintf "$line\n"
    fi
}

function cprintf::opt::color-codes-build() {
    COLORS=("black" "red" "green" "yellow" "blue" "magenta" "cyan" "white")
    while read line; do
        printf -- "${line}\n"
#        cprintf::colorize-headings "${line}"
    done <<EOF
# Color Codes Reference

This document describes all color notation formats, color spaces, aliases, and modifiers supported by cprintf.

## Color Notation Formats

Colors can be specified in multiple ways:

### Decimal Numbers (0-9, 16-255, 256+)
   - **0-7**: Standard colors
   - **9**: Special code - resets to terminal default
   - **16-255**: 8-bit extended colors
   - **256+**: 24-bit true colors (RGB as single decimal)

### @ Prefix (Extended/True Color)
   - **@0-@255**: 8-bit extended colors
   - **@256+**: 24-bit true colors

### # Prefix (Hex Notation)
   - **#0-#7**: Standard colors (single hex digit)
   - **#8-#F**: High intensity colors (single hex digit)
   - **#00-#FF**: 8-bit extended colors (two hex digits)
   - **#RRGGBB**: 24-bit true colors (six hex digits)

### Color Name Aliases
   - **lowercase**: Standard colors (black, red, green, yellow, blue, magenta, cyan, white)
   - **Capitalized**: High intensity colors (Black, Red, Green, Yellow, Blue, Magenta, Cyan, White)

## Intensity Modifiers (+/-)

Modifiers change color intensity or add effects:

   **Standard Colors (0-7)**
      + → High intensity color
      - → Dim effect

   **High Intensity Colors (8-15)**
      + → Bold effect
      - → Standard color

   **8-Bit Colors (16-255)**
      + → Bold effect
      - → Dim effect

   **24-Bit Colors (256+)**
      + → Bold effect
      - → Dim effect

## Color Spaces

### Standard Colors (0-7)

The standard color space provides 8 basic colors:

EOF
    for (( f = 0; f < ${#COLORS[@]}; f++ )); do
        printf "<bg:$f> <fg:$((8 - f))>   $f   </fg> </bg>"
    done
    cprintf "\n"
    while IFS= read -r line; do
        printf "${line}\n"
    done <<EOF
  black     red    green    yellow    blue    magenta   cyan     white

**Notation Options:**
   - Decimal: 0, 1, 2, 3, 4, 5, 6, 7
   - Hex: #0, #1, #2, #3, #4, #5, #6, #7
   - Aliases: black, red, green, yellow, blue, magenta, cyan, white

**Examples:**
   <fg:0>0</fg> = <fg:#0>#0</fg> = <fg:black>black</fg>
   <fg:1>1</fg> = <fg:#1>#1</fg> = <fg:red>red</fg>
   <fg:4>4</fg> = <fg:#4>#4</fg> = <fg:blue>blue</fg>

**With Modifiers:**
   <fg:0+>0+</fg> = <fg:#0+>#0+</fg> = <fg:black+>black+</fg> → High intensity
   <fg:1->1-</fg> = <fg:#1->#1-</fg> = <fg:red->red-</fg> → Dim effect

### High Intensity Colors (8-15)

High intensity (bright) versions of standard colors:

EOF
    for (( f = 0; f < ${#COLORS[@]}; f++ )); do
        printf "<bg:#$f> <fg:$((8 - f))>  #$f   </fg> </bg>"
    done
    printf "\n"
    while IFS= read -r line; do
        printf "${line}\n"
    done <<EOF
  black     red    green    yellow    blue    magenta   cyan     white

**Notation Options:**
   - Hex: #8, #9, #A, #B, #C, #D, #E, #F
   - Decimal with +: 0+, 1+, 2+, 3+, 4+, 5+, 6+, 7+
   - Aliases with +: black+, red+, green+, yellow+, blue+, magenta+, cyan+, white+
   - Capitalized aliases: Black, Red, Green, Yellow, Blue, Magenta, Cyan, White

**Examples:**
   <fg:#8>#8</fg> = <fg:0+>0+</fg> = <fg:black+>black+</fg> = <fg:Black>Black</fg>
   <fg:#9>#9</fg> = <fg:1+>1+</fg> = <fg:red+>red+</fg> = <fg:Red>Red</fg>
   <fg:#C>#C</fg> = <fg:4+>4+</fg> = <fg:blue+>blue+</fg> = <fg:Blue>Blue</fg>

**With Modifiers:**
   <fg:Black+>Black+</fg> = <fg:#8+>#8+</fg> → Bold effect
   <fg:Red->Red-</fg> = <fg:#9->#9-</fg> → Converts to standard red

### 8-Bit Extended Colors (16-255)

256-color palette providing extended color range:

EOF

    printf "Standard: "
    for (( f = 0; f < ${#COLORS[@]}; f++ )); do
        printf "<bg:@$f> <fg:$((8 - f))!>%4s</fg> </bg>" "@$f"
    done
    printf '\n'
    printf "High    : "
    for (( f = 0; f < ${#COLORS[@]}; f++ )); do
        printf "<bg:@$((f + 8))> <fg:$((15 - f))!>%4s</fg> </bg>" "@$((f + 8))"
    done
    printf '\n\n'

    PATTERN_LEFT="$(printf "<bg:%%s> <fg:%s>%%%%-3s</fg> </bg>" 255 255 255 255 255 255)"
    PATTERN_RIGHT="$(printf "<bg:%%s> <fg:%s>%%%%-3s</fg> </bg>" 0 0 0 0 0 0)"
    for (( i = 16; i <= 28; i += 6 )); do
        for (( j = 0; j < 6; j++ )); do
            printf '%10s' ' '
            ccl=$((i + (j * 36)))
            ccr=$(((i + 18) + (j * 36)))
            codes=(
                $((ccl))
                $((ccl + 1))
                $((ccl + 2))
                $((ccl + 3))
                $((ccl + 4))
                $((ccl + 5))
                $((ccr))
                $((ccr + 1))
                $((ccr + 2))
                $((ccr + 3))
                $((ccr + 4))
                $((ccr + 5))
            )

            printf "$(printf "$PATTERN_LEFT $PATTERN_RIGHT" "${codes[@]}")\n" "${codes[@]}"
        done
        printf '\n'
    done
    for (( j = 0; j < 4; j++ )); do
        if [[ $j == 0 ]]; then
            printf '%8s: ' 'Greys'
        else
            printf '%10s' ' '
        fi
        for (( k = 0; k < 6; k++ )); do
            color_code=$((232 + (j * 6) + k))
            fg=255
            if [ $j -gt 1 ]; then
                fg=0
            fi
            printf "<bg:${color_code}> <fg:$fg>%-3s</fg> </bg>" ${color_code}
        done
        printf '\n'
    done
    printf '\n'

    while IFS= read -r line; do
        printf "${line}\n"
    done <<EOF

**Notation Options:**
   - Decimal: 16-255
   - @ prefix: @0-@255
   - Hex (2 digits): #00-#FF

**Examples:**
   <bg:196><fg:white> 196 </fg></bg> <bg:@196><fg:white > @196 </fg></bg> <bg:#C4><fg:white> #C4 </fg></bg>  (Bright red)
   <bg:46 ><fg:black>  46 </fg></bg> <bg:@46 ><fg:black > @46  </fg></bg> <bg:#2E><fg:black> #2E </fg></bg>  (Bright green)
   <bg:21 ><fg:white>  21 </fg></bg> <bg:@21 ><fg:white > @21  </fg></bg> <bg:#15><fg:white> #15 </fg></bg>  (Bright blue)
   <bg:208><fg:black> 208 </fg></bg> <bg:@208><fg:black > @208 </fg></bg> <bg:#D0><fg:black> #D0 </fg></bg>  (Orange)

**Color Ranges:**
   - 16-231: 6×6×6 RGB cube
   - 232-255: Grayscale ramp

**With Modifiers:**
   <fg:196+>196+</fg> = <fg:@196+>@196+</fg> = <fg:#C4+>#C4+</fg> → Bold effect
   <fg:46->46-</fg> = <fg:@46->@46-</fg> = <fg:#2E->#2E-</fg> → Dim effect

### 24-Bit True Colors (16,777,216 colors)

Full RGB color support using 24-bit values:

**Notation Options:**
   - Hex (6 digits): #RRGGBB
   - Decimal: 256-16777215
   - @ prefix: @256-@16777215

**Examples:**
   <bg:#FF6B6B> <fg:black> #FF6B6B </fg></bg>  (Custom red)
   <bg:#4ECDC4> <fg:black> #4ECDC4 </fg></bg>  (Custom teal)
   <bg:#45B7D1> <fg:black> #45B7D1 </fg></bg>  (Custom blue)
   <bg:#F7DC6F> <fg:black> #F7DC6F </fg></bg>  (Custom yellow)

**RGB Breakdown:**
   #FF6B6B = R:255, G:107, B:107
   #4ECDC4 = R:78, G:205, B:196
   #45B7D1 = R:69, G:183, B:209

**With Modifiers:**
   <fg:#FF6B6B >#FF6B6B</fg> → <fg:#FF6B6B+>#FF6B6B+</fg> → Bold effect
   <fg:#4ECDC4 >#4ECDC4</fg> → <fg:#4ECDC4->#4ECDC4-</fg> → Dim effect

## Special Color Code

**Code 9**: Resets color to terminal default
   <fg:9>Terminal default foreground</fg>
   <bg:9>Terminal default background</bg>

## Summary Tables

### Standard Colors (0-7)
   Decimal:  0, 1, 2, 3, 4, 5, 6, 7
   Hex:      #0, #1, #2, #3, #4, #5, #6, #7
   Aliases:  black, red, green, yellow, blue, magenta, cyan, white

### High Intensity Colors (8-15)
   Hex:      #8, #9, #A, #B, #C, #D, #E, #F
   Decimal+: 0+, 1+, 2+, 3+, 4+, 5+, 6+, 7+
   Aliases+: black+, red+, green+, yellow+, blue+, magenta+, cyan+, white+
   Capital:  Black, Red, Green, Yellow, Blue, Magenta, Cyan, White

### 8-Bit Extended Colors (16-255)
   Decimal:  16-255
   @Prefix:  @16-@255
   Hex:      #10-#FF

### 24-Bit True Colors (256+)
   Hex:      #RRGGBB (6 hex digits)
   Decimal:  256-16777215
   @Prefix:  @256-@16777215

## Checking Terminal Support

Use the --check-color-support option to verify your terminal's capabilities:

   ./cprintf --check-color-support

Most modern terminals support Standard, High intensity, and 8-bit colors. True color (24-bit) support varies by terminal.

EOF
}

function cprintf::opt::color-codes() {
    cprintf "$(cprintf::opt::color-codes-build)"
}
