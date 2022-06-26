# Theme file for CliFM
# Theme name: dracula (based on https://github.com/dracula/dracula-theme)
# Author: L. Abramovich
# License: GPL3

define D=0;38;2;248;248;242
define BD=0;1;38;2;248;248;242

define R=38;2;255;85;85
define BR=1;38;2;255;85;85
define DR=2;38;2;255;85;85

define G=38;2;80;250;123
define BG=1;38;2;80;250;123
define DG=2;38;2;80;250;123

define Y=38;2;241;250;140
define BY=1;38;2;241;250;140
define DY=2;38;2;241;250;140

define B=38;2;98;114;164
define BB=1;38;2;98;114;164
define DB=2;38;2;98;114;164

define M=38;2;255;121;198 #pink
define BM=1;38;2;255;121;198
define DM=2;38;2;255;121;198
define UM=4;38;2;255;121;198

define M2=38;2;189;147;249 #purple
define BM2=1;38;2;189;147;249
define DM2=2;38;2;189;147;249
define UM2=4;38;2;189;147;249

define C=38;2;139;233;253
define BC=1;38;2;139;233;253
define DC=2;38;2;139;233;253
define RC=7;38;2;139;233;253
define UDC=4;2;38;2;139;233;253
define BDC=1;2;38;2;139;233;253

define DW=2;38;2;248;248;242

# Foreground-background combinations
define RW=38;2;255;85;85;48;2;248;248;242
define BW=38;2;98;114;164;48;2;248;248;242
define WR=38;2;248;248;242;48;2;255;85;85 
define KY=38;2;40;42;54;48;2;241;250;140
define KR=38;2;40;42;54;48;2;255;85;85
define KG=38;2;40;42;54;48;2;80;250;123
define BlGr=38;2;98;114;164;48;2;80;250;123
define WB=38;2;248;248;242;48;2;98;114;164

FiletypeColors="di=BB:nd=BR:ed=DB:ne=DR:fi=D:ef=DY:nf=DR:ln=BC:mh=RC:or=DC:pi=M:so=BM2:bd=BY:cd=BD:su=WR:sg=KY:ca=KR:tw=KG:ow=BlGr:st=WB:ex=BG:ee=G:no=RW:uf=BW:"

InterfaceColors="el=BY:mi=BC:dl=B:tx=D:df=D:fc=DB:wc=BC:li=BG:si=BB:ti=BC:em=BR:wm=BY:nm=BG:bm=BC:dn=DW:dr=Y:dw=R:dxd=G:dxr=C:dg=Y:dd=B:dz=G:do=C:dp=M2:sb=DY:sc=DR:sf=UDC:sh=DM2:sp=DR:sx=DG:hb=C:hc=DR:hd=C:he=C:hn=M:hp=C:hq=Y:hr=R:hs=G:hv=G:tt=BDC:ts=UM2:wp=DR:ws1=B:ws2=R:ws3=Y:ws4=G:ws5=C:ws6=C:ws7=C:ws8=C:xs=G:xf=BR:"

ExtColors="*.tar=BR:*.tgz=BR:*.taz=BR:*.lha=BR:*.lz4=BR:*.lzh=BR:*.lzma=BR:*.tlz=BR:*.txz=BR:*.tzo=BR:*.t7z=BR:*.zip=BR:*.z=BR:*.dz=BR:*.gz=BR:*.lrz=BR:*.lz=BR:*.lzo=BR:*.xz=BR:*.zst=BR:*.tzst=BR:*.bz2=BR:*.bz=BR:*.tbz=BR:*.tbz2=BR:*.tz=BR:*.deb=BR:*.rpm=BR:*.rar=BR:*.cpio=BR:*.7z=BR:*.rz=BR:*.cab=BR:*.jpg=BM:*.JPG=BM:*.jpeg=BM:*.mjpg=BM:*.mjpeg=BM:*.gif=BM:*.GIF=BM:*.bmp=BM:*.xbm=BM:*.xpm=BM:*.png=BM:*.PNG=BM:*.svg=BM:*.pcx=BM:*.mov=BM:*.mpg=BM:*.mpeg=BM:*.m2v=BM:*.mkv=BM:*.webm=BM:*.webp=BM:*.ogm=BM:*.mp4=BM:*.MP4=BM:*.m4v=BM:*.mp4v=BM:*.vob=BM:*.wmv=BM:*.flc=BM:*.avi=BM:*.flv=BM:*.m4a=C:*.mid=C:*.midi=C:*.mp3=C:*.MP3=C:*.ogg=C:*.wav=C:*.pdf=BR:*.PDF=BR:*.doc=M:*.docx=M:*.xls=M:*.xlsx=M:*.ppt=M:*.pptx=M:*.odt=M:*.ods=M:*.odp=M:*.cache=DW:*.tmp=DW:*.temp=DW:*.log=DW:*.bak=DW:*.bk=DW:*.in=DW:*.out=DW:*.part=DW:*.aux=DW:*.c=BD:*.c++=BD:*.h=BD:*.cc=BD:*.cpp=BD:*.h=BD:*.h++=BD:*.hh=BD:*.go=BD:*.java=BD:*.js=BD:*.lua=BD:*.rb=BD:*.rs=BD:"

DirIconColor="Y"

Prompt="\[\e[0;38;2;248;248;242m\][\S\[\e[0;38;2;248;248;242m\]]\l \A \u:\H \[\e[0;38;2;139;233;253m\]\w\n\[\e[0;38;2;248;248;242m\]<\z\[\e[0;38;2;248;248;242m\]> \[\e[0;38;2;98;114;164m\]\$ \[\e[0m\]"
Notifications=true
EnableWarningPrompt=true
WarningPrompt="\[\e[0m\]\[\e[0;2;38;2;255;85;85m\](!) > "

DividingLine="-"

FzfTabOptions="--color='dark,prompt:#8be9fd,fg+:-1,pointer:#6272a4,hl:#bd93f9,hl+:#bd93f9,gutter:-1,marker:#50fa7b:bold,query:#f8f8f2,info:#f8f8f2:dim' --marker='*' --bind tab:accept,right:accept,left:abort --inline-info --layout=reverse-list"
