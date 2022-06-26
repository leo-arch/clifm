# Theme file for CliFM
# Theme name: clifm
# Author: L. Abramovich
# License: GPL3

# FiletypeColors, InterfaceColors, and ExtColors use the same format used
# by the LS_COLORS environment variable. Thus, "di=01;34" means that (non-empty)
# directories will be printed in bold blue.
# Color codes are just traditional ANSI escape sequences less the escape char
# and the final 'm'.
# 4-bit, 8-bit (256 colors), and 24-bit (RGB/HEX) colors are supported.
# Example:
# 31            4-bit
# 38;5;160      8-bit
# 38;2;255;0;0  24-bit (RGB)
# #ff0000       24-bit (HEX)
#
# One attribute can be used for hex colors using a dash and an attribute
# number (RRGGBB-[1-9]), where 1-9 is:
#
# 1: Bold or increased intensity
# 2: Faint, decreased intensity or dim
# 3: Italic (Not widely supported)
# 4: Underline
# 5: Slow blink
# 6: Rapid blink
# 7: Reverse video or invert
# 8: Conceal or hide (Not widely supported)
# 9: Crossed-out or strike
#
# For example, to print bold red color, the hex code is #ff0000-1

# Definitions
# Define here up to 64 custom color variables. They can be used for:
# FiletypeColors
# InterfaceColors
# ExtColors
# DirIconColor

define D=0 # Default terminal foreground color
#define BD=1 # Bold (keep current color)
define BD=0;1 # Bold (reset foreground color)

define R=31 # Red
define BR=1;31 # Bold red
define DR=2;31 # Dimmed red
define UDR=4;2;31 # Underlined dimmed red
define UBR=4;1;31 # Underlined bold red

define G=32 # Green
define BG=1;32 # Bold green
define DG=2;32 # Dimmed green

define Y=33 # Yellow
define BY=1;33 # Bold yellow
define DY=2;33 # Dimmed yellow

define B=34 # Blue
define BB=1;34 # Bold blue
define DB=2;34 # Dimmed blue

define M=35 # Magenta
define BM=1;35 # Bold Magenta
define DM=2;35 # Dimmed magenta
define UM=4;35 # Underlined magenta

define C=36 # Cyan
define BC=1;36 # Bold cyan
define DC=2;36 # Dimmed cyan
define RC=7;36 # Reverse cyan
define UDC=4;2;36 # Underlined dimmed cyan
define BDC=1;2;36 # Bold dimmed cyan

define DW=2;37 # Dimmed white

# Foreground-background combinations
define URW=4;31;47 # Red foreground, white background
define UBW=4;34;47 # Blue foreground, white background
define WR=37;41 # White foreground, red background
# K stands for black (B is used for Blue)
define KY=30;43 # Black foreground, yellow background
define KR=30;41 # Black foreground, red background
define KG=30;42 # Black foreground, green background
# BG is already used for bold green
define BlGr=34;42 # Blue foreground, green background
define WB=37;44 # white foreground, blue background

# FiletypeColors defines the color used for file names when listing files, 
# just as InterfaceColors defines colors for CliFM's interface.
# Consult the manpage for information about these codes.
FiletypeColors="bd=BY:ca=KR:cd=BD:di=BB:ed=DB:ee=G:ef=DY:ex=BG:fi=D:ln=BC:mh=RC:nd=UBR:ne=UDR:nf=UDR:no=URW:or=UDC:ow=BlGr:pi=M:sg=KY:so=BM:st=WB:su=WR:tw=KG:uf=UBW:"

InterfaceColors="bm=BG:dd=B:df=D:dg=Y:dl=DW:dn=DW:dr=Y:do=C:dp=M:dw=R:dxd=G:dxr=C:dz=G:el=C:em=BR:fc=DB:hb=C:hc=DR:hd=C:he=C:hn=M:hp=C:hq=Y:hr=R:hs=G:hv=G:li=BG:mi=BC:nm=BG:si=BB:sb=DY:sc=DR:sf=UDC:sh=DM:sp=DR:sx=DG:ti=BC:ts=UM:tt=BDC:tx=D:wc=BC:wm=BY:wp=DR:ws1=B:ws2=R:ws3=Y:ws4=G:ws5=C:ws6=C:ws7=C:ws8=C:xf=BR:xs=G:"

# Colors for specific file extensions
ExtColors="*.tar=BR:*.tgz=BR:*.taz=BR:*.lha=BR:*.lz4=BR:*.lzh=BR:*.lzma=BR:*.tlz=BR:*.txz=BR:*.tzo=BR:*.t7z=BR:*.zip=BR:*.z=BR:*.dz=BR:*.gz=BR:*.lrz=BR:*.lz=BR:*.lzo=BR:*.xz=BR:*.zst=BR:*.tzst=BR:*.bz2=BR:*.bz=BR:*.tbz=BR:*.tbz2=BR:*.tz=BR:*.deb=BR:*.rpm=BR:*.rar=BR:*.cpio=BR:*.7z=BR:*.rz=BR:*.cab=BR:*.jpg=BM:*.JPG=BM:*.jpeg=BM:*.mjpg=BM:*.mjpeg=BM:*.gif=BM:*.GIF=BM:*.bmp=BM:*.xbm=BM:*.xpm=BM:*.png=BM:*.PNG=BM:*.svg=BM:*.pcx=BM:*.mov=BM:*.mpg=BM:*.mpeg=BM:*.m2v=BM:*.mkv=BM:*.webm=BM:*.webp=BM:*.ogm=BM:*.mp4=BM:*.MP4=BM:*.m4v=BM:*.mp4v=BM:*.vob=BM:*.wmv=BM:*.flc=BM:*.avi=BM:*.flv=BM:*.m4a=C:*.mid=C:*.midi=C:*.mp3=C:*.MP3=C:*.ogg=C:*.wav=C:*.pdf=BR:*.PDF=BR:*.doc=M:*.docx=M:*.xls=M:*.xlsx=M:*.ppt=M:*.pptx=M:*.odt=M:*.ods=M:*.odp=M:*.cache=DW:*.tmp=DW:*.temp=DW:*.log=DW:*.bak=DW:*.bk=DW:*.in=DW:*.out=DW:*.part=DW:*.aux=DW:*.c=BD:*.c++=BD:*.h=BD:*.cc=BD:*.cpp=BD:*.h=BD:*.h++=BD:*.hh=BD:*.go=BD:*.java=BD:*.js=BD:*.lua=BD:*.rb=BD:*.rs=BD:"

# If icons are enabled, use this color for the directories icon
DirIconColor="Y"

# The prompt used by CliFM. Use the 'prompt' command to check for available
# prompts. Enter 'prompt --help' for more information
Prompt="clifm"

# Override prompt values
#Prompt=""
#Notifications=
#EnableWarningPrompt=
#WarningPrompt=""

# The string used to construct the line dividing the list of files and
# the prompt (Unicode is supported). Possible values:
# "0": Print just an empty line
# "C": C is a single char. This char is printed up to the end of the screen
# "CCC": 3 or more chars. Only these chars (no more) will be printed
# "": Print a special line drawn with box-drawing characters (not
#     supported by all terminals/consoles)
# The color of this line is controlled by the 'dl' code in InterfaceColors
DividingLine="-"

# If FZF TAB completion mode is enabled, pass these options to fzf:
FzfTabOptions="--color='16,prompt:6,fg+:-1,pointer:4,hl:5,hl+:5,gutter:-1,marker:2' --marker='*' --bind tab:accept,right:accept,left:abort --inline-info --layout=reverse-list"

# Same options, but colorless
#FzfTabOptions="--color='bw' --marker='*' --bind tab:accept,right:accept,left:abort --inline-info --layout=reverse-list"

# For more information consult fzf(1)
