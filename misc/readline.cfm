# CliFM readline initialization file

# You can re-read the inputrc file with C-r C-r.
# Lines beginning with '#' are comments.

# For the complete list of Readline options see:
# https://www.gnu.org/software/bash/manual/html_node/Readline-Init-File-Syntax.html#Readline-Init-File-Syntax

#$include ~/inputrc
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

set show-all-if-ambiguous on
set completion-ignore-case on

# Move between words with C-Left and C-Right
$if term=rxvt
"\x1b\x4f\x64": backward-word
"\x1b\x4f\x63": forward-word
$else
"\e[1;5D": backward-word
"\e[1;5C": forward-word
"\e[5D": backward-word
"\e[5C": forward-word
$endif

# A few keybindings redefinitions to avoid conflicts with CliFM
# specific keybindings
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

# Enable history completion
"\e[A": history-search-backward
"\e[B": history-search-forward
