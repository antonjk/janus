#? Check terminal color support and display capabilities (8, 16, 256, 24Bit)
cprintf::opt::check-color-support() {
    # Check number of colors supported
    colors=$(tput colors 2>/dev/null)
    # Not a terminal
    if [[ ! -t 1 ]]; then
        colors=0
    fi
    [[ $colors -ge 8 ]] && echo "8"
    [[ $colors -ge 16 ]] && echo "16"
    [[ $colors -ge 256 ]] && echo "256"
    # Check for true color (24-bit)
    [[ $COLORTERM == "truecolor" || $COLORTERM == "24bit" ]] && echo "24Bit"
}
