# Theme file for CliFM
# Theme name: zenburn (based on https://github.com/jnurmine/Zenburn)
# Author: L. Abramovich
# License: GPL3

define W=38;2;220;220;204
define BW=01;38;2;220;220;204
define DW=2;38;2;220;220;204

define R=38;2;220;163;163
define BR=1;38;2;220;163;163
define DR=2;38;2;220;163;163
define UBR=4;01;38;2;220;163;163
define UR=4;38;2;220;163;163

define C=38;2;140;208;211
define BC=1;38;2;140;208;211
define DC=2;38;2;140;208;211
define RC=7;38;2;140;208;211
define UC=4;38;2;140;208;211

define Y=38;2;248;248;147
define BY=1;38;2;248;248;147
define DY=2;38;2;248;248;147

define BG2=1;38;2;40;79;40
define G=38;2;112;159;127
define BG=1;38;2;112;159;127
define DG=2;38;2;112;159;127

define S=38;2;240;207;175 # Salmon
define BS=1;38;2;240;207;175
define DS=2;38;2;240;207;175

define M=38;2;159;175;175
define BM=1;38;2;159;175;175
define DM=2;38;2;159;175;175

define BV=1;38;5;125
define V=38;5;125

define KR=38;2;44;48;45;48;2;220;163;163
define KY=38;2;44;48;45;48;2;248;248;147
define WGr=38;2;255;255;255;48;2;49;60;54
define WR=38;2;255;255;255;48;2;220;163;163
define UWR=4;38;2;255;255;255;48;2;220;163;163
define WC=38;2;255;255;255;48;2;140;208;211
define KW=38;2;47;47;47;48;2;255;255;255
define RK=38;2;241;140;150;48;2;70;70;70

FiletypeColors="di=BS:nd=UBR:ed=S:ne=UR:fi=W:ef=DW:ex=BG:ee=G:ln=BC:or=UC:nf=UR:mh=RC:pi=BM:so=BV:bd=BW:cd=BW:su=KR:sg=KY:ca=WGr:tw=WR:ow=UWR:st=WC:no=KW:uf=RK:"

InterfaceColors="bm=BC:df=W:dl=BG2:dd=C:dg=Y:dn=DW:dr=Y:do=C:dp=M:dw=R:dxd=G:dxr=C:dz=G:el=BR:em=BR:fc=DS:hb=C:hc=DR:hd=G:he=C:hn=M:hp=C:hq=Y:hr=R:hs=G:hv=G:li=BG:mi=BC:nm=BG:si=BB:sb=DY:sc=DR:sf=UDC:sh=DM:sp=DR:sx=DG:ti=BC:ts=UM:tt=BDC:tx=W:wc=BC:wm=BY:wp=DR:ws1=C:ws2=R:ws3=Y:ws4=G:ws5=C:ws6=C:ws7=C:ws8=C:xs=G:xf=BR:"

ExtColors="*.tar=BR:*.tgz=BR:*.taz=BR:*.lha=BR:*.lz4=BR:*.lzh=BR:*.lzma=BR:*.tlz=BR:*.txz=BR:*.tzo=BR:*.t7z=BR:*.zip=BR:*.z=BR:*.dz=BR:*.gz=BR:*.lrz=BR:*.lz=BR:*.lzo=BR:*.xz=BR:*.zst=BR:*.tzst=BR:*.bz2=BR:*.bz=BR:*.tbz=BR:*.tbz2=BR:*.tz=BR:*.deb=BR:*.rpm=BR:*.rar=BR:*.cpio=BR:*.7z=BR:*.rz=BR:*.cab=BR:*.pdf=BV:*.PDF=BV:*.doc=M:*.docx=M:*.xls=M:*.xlsx=M:*.ppt=M:*.pptx=M:*.odt=M:*.ods=M:*.odp=M*.jpg=G:*.jpeg=G:*.mjpg=G:*.mjpeg=G:*.gif=G:*.bmp=G:*.xbm=G:*.xpm=G:*.png=G:*.svg=G:*.pcx=G:*.mov=G:*.mpg=G:*.mpeg=G:*.m2v=G:*.mkv=G:*.webm=G:*.webp=G:*.ogm=G:*.mp4=G:*.m4v=G:*.mp4v=G:*.vob=G:*.wmv=G:*.flc=G:*.avi=G:*.flv=G:*.m4a=G:*.mid=G:*.midi=G:*.mp3=G:*.ogg=G:*.wav=G:"

DirIconColor="Y"

Prompt="\[\e[38;2;220;220;204m\][\S\[\e[38;2;220;220;204m\]]\l \A \u:\H \[\e[01;38;2;112;159;127m\]\w\n\[\e[38;2;220;220;204m\]<\z\[\e[38;2;220;220;204m\]> \[\e[00;38;2;140;208;211m\]\$ \[\e[0m\]"
Notifications=true
EnableWarningPrompt=true
WarningPrompt="\[\e[0m\]\[\e[00;02;38;2;220;163;163m\](!) > "

DividingLine="-"

FzfTabOptions="--color='dark,prompt:#8cd0d3,fg+:-1,pointer:#af005f:bold,hl:5,hl+:5,gutter:-1,marker:#f0cfaf:bold' --marker='*' --bind tab:accept,right:accept,left:abort --inline-info --layout=reverse-list"
