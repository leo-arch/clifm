# Readline keybindings for CliFM

# For the complete list of Readline options see:
# https://www.gnu.org/software/bash/manual/html_node/Readline-Init-File-Syntax.html#Readline-Init-File-Syntax

#$include /etc/inputrc

# Color files by types
set colored-stats on
# Append char to indicate type
set visible-stats on
# Mark symlinked directories
set mark-symlinked-directories on
# Color the common prefix
set colored-completion-prefix on
# Color the common prefix in menu-complete
set menu-complete-display-prefix on
# Disable paste protection
set enable-bracketed-paste on

set show-all-if-ambiguous on
set completion-ignore-case on

set meta-flag on
set input-meta on
set output-meta on

$if mode=emacs

# For linux console and RH/Debian xterm
"\e[5C": forward-word
"\e[5D": backward-word
"\e\e[C": forward-word
"\e\e[D": backward-word
"\e[1;5C": forward-word
"\e[1;5D": backward-word

# For rxvt
"\x1b\x4f\x64": backward-word
"\x1b\x4f\x63": forward-word

# A few keybinds to avoid conflicts with CliFM specific keybinds
"\C-d":
"\e\e":
"\C-r\C-r": re-read-init-file
"\C-p\C-p": exchange-point-and-mark
"\C-zA": do-lowercase-version
"\C-zB": do-lowercase-version
"\C-zC": do-lowercase-version
"\C-zD": do-lowercase-version
"\C-zE": do-lowercase-version
"\C-zF": do-lowercase-version
"\C-zG": do-lowercase-version
"\C-zH": do-lowercase-version
"\C-zI": do-lowercase-version
"\C-zJ": do-lowercase-version
"\C-zK": do-lowercase-version
"\C-zL": do-lowercase-version
"\C-zM": do-lowercase-version
"\C-zN": do-lowercase-version
"\C-zO": do-lowercase-version
"\C-zP": do-lowercase-version
"\C-zQ": do-lowercase-version
"\C-zR": do-lowercase-version
"\C-zS": do-lowercase-version
"\C-zT": do-lowercase-version
"\C-zU": do-lowercase-version
"\C-zV": do-lowercase-version
"\C-zW": do-lowercase-version
"\C-zX": do-lowercase-version
"\C-zY": do-lowercase-version
"\C-zZ": do-lowercase-version

# History completion based on prefix
#"\e[A": history-search-backward
#"\e[B": history-search-forward

$endif
