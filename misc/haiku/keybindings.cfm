# Keybindings file for CliFM

# Use the 'kbgen' plugin (compile it first: gcc -o kbgen kbgen.c) to 
# find out the escape code for the key or key sequence you want. Use 
# either octal, hexadecimal codes or symbols.
# Ex: For Alt-/ (in rxvt terminals) 'kbgen' will print the following 
# lines:
# Hex  | Oct | Symbol
# ---- | ---- | ------
# \x1b | \033 | ESC (\e)
# \x2f | \057 | /
# In this case, the keybinding, if using symbols, is: "\e/:function"
# In case you prefer the hex codes it would be: \x1b\x2f:function.
# GNU emacs escape sequences are also allowed (ex: "\M-a", Alt-a
# in most keyboards, or "\C-r" for Ctrl-r).
# Some codes, especially those involving keys like Ctrl or the arrow
# keys, vary depending on the terminal emulator and the system settings.
# These keybindings should be set up thus on a per terminal basis.
# You can also consult the terminfo database via the infocmp command.
# See terminfo(5) and infocmp(1).

# Ctrl-j
previous-dir:\C-j
# Ctrl-left
previous-dir2:\eO5D

# Ctrl-k
next-dir:\C-k
next-dir2:\eO5C

Page-down
first-dir:\e[5~
Page-up
last-dir:\e[6~

# Ctrl-u
parent-dir:\C-u
# Ctrl-up
parent-dir2:\eO5A

# Home key
home-dir2:\eOH

# End key
root-dir:\eOF

pinned-dir:\C-p
workspace1:\eOP
workspace2:\eOQ
workspace3:\eOR
workspace4:\eOS

# Help
# F5-7
show-manpage:\e[15~
show-cmds:\e[17~
show-kbinds:\e[18~

#create-file:\C-n
new-instance:\C-x
#previous-profile:\C-\M-o
#next-profile:\C-\M-p
#archive-sel:\C-\M-a
#rename-sel:\C-\M-r
#remove-sel:\C-\M-d
#trash-sel:\C-\M-t
#untrash-all:\C-\M-u
#move-sel:\C-\M-n
#export-sel:\C-\M-e
#open-sel:\C-\M-g
#bookmark-sel:\C-\M-b
#refresh-screen:\C-r
#show-dirhist:\C-h

Insert key
paste-sel:\e[2~

clear-line:\C-e
clear-msgs:\C-t
toggle-hidden:\C-h
toggle-light:\C-y
toggle-long:\C-l
sort-previous:\C-q
sort-next:\C-w
bookmarks:\C-b
select-all:\C-a
selbox:\C-s
deselect-all:\C-r

#mountpoints:\M-m

folders-first:\C-g
lock:\C-o
# F8-12
edit-color-scheme:\e[19~
open-keybinds:\e[20~
open-config:\e[21~
open-bookmarks:\e[23~
quit:\e[24~

#open-mime:\e[17~
#open-jump-db:\e[18~

# Plugins
# 1) Make sure your plugin is in the plugins directory (or use any of the
# plugins in there)
# 2) Link pluginx to your plugin using the 'actions edit' command. Ex:
# "plugin1=myplugin.sh"
# 3) Set a keybinding here for pluginx. Ex: "plugin1:\M-7"

#plugin1:
#plugin2:
#plugin3:
#plugin4:
