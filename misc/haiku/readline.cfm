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

set show-all-if-ambiguous on
set completion-ignore-case on

#set meta-flag on
#set input-meta on
#set output-meta on

$if mode=emacs

# For linux console and RH/Debian xterm
#"\e[5C": forward-word
#"\e[5D": backward-word
#"\e\e[C": forward-word
#"\e\e[D": backward-word
#"\e[1;5C": forward-word
#"\e[1;5D": backward-word

# For rxvt
#"\x1b\x4f\x64": backward-word
#"\x1b\x4f\x63": forward-word

#Disable some readline keybinds to prevent conflicts 
#"\C-i":
#"\C-m":
"\C-d":
"\C-x":
"\C-x\C-x":
"\C-n":
"\C-v":
"\C-u":
"\C-r":
"\ep":

# Rebind some keybinds to avoid conflicts with CliFM specific keybinds
"\ea": beginning-of-line
"\ee": end-of-line
"\C-x\C-x": re-read-init-file
"\C-p\C-p": exchange-point-and-mark

# History completion based on prefix
"\eOA": history-search-backward
"\eOB": history-search-forward

$endif
