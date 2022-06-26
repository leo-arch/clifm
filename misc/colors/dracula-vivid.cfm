# Theme file for CliFM
# Theme name: dracula-vivid (based on vivid(1))
# Author: L. Abramovich
# License: GPL3

define D=00;38;5;253
define BD=00;01 # Bold (reset foreground color)

define R=38;5;203 # Red
define BR=1;38;5;203 # Bold red
define DR=2;38;5;203 # Dimmed red

define G=38;5;84 # Green
define BG=1;38;5;84 # Bold green
define DG=2;38;5;84 # Dimmed green

define Y=38;5;228 # Yellow
define BY=1;38;5;228 # Bold yellow
define DY=2;38;5;228 # Dimmed yellow

define B=38;5;141 # Blue
define BB=1;38;5;141 # Bold blue
define DB=2;38;5;141 # Dimmed blue

define M=38;5;212 # Magenta
define BM=1;38;5;212 # Bold Magenta
define DM=2;38;5;212 # Dimmed magenta
define UM=4;38;5;212 # Underlined magenta

define C=;38;5;117 # Cyan
define BC=1;38;5;117 # Bold cyan
define DC=2;38;5;117 # Dimmed cyan
define RC=7;38;5;117 # Reverse cyan
define UDC=4;02;38;5;117 # Underlined dimmed cyan
define BDC=1;02;38;5;117 # Bold dimmed cyan

define O=38;5;215
define BO=1;38;5;215
define DO=2;38;5;215

define W=38;5;253
define DW=2;38;5;253 # Dimmed white

# Foreground-background combinations
define RW=31;47 # Red foreground, white background
define BW=34;47 # Blue foreground, white background
define WR=37;41 # White foreground, red background
# K stands for black (B is used for Blue)
define KY=30;43 # Black foreground, yellow background
define KR=30;41 # Black foreground, red background
define KG=30;42 # Black foreground, green background
# BG is already used for bold green
define BlGr=34;42 # Blue foreground, green background
define WB=37;44 # white foreground, blue background

FiletypeColors="pi=1;38;5;228;48;5;235:ln=0;38;5;117:ca=0:st=0;38;5;228;48;5;117:sg=0;38;5;228;48;5;212:su=0;38;5;228;48;5;212:tw=0;38;5;228;48;5;84:ow=0;38;5;117;48;5;235:do=1;38;5;228;48;5;235:or=0;38;5;203;48;5;235:cd=1;38;5;215;48;5;235:bd=1;38;5;215;48;5;235:mh=0:di=B:ed=DB:fi=W:ef=DW:ex=0;38;5;84:ee=0;2;38;5;84:so=1;38;5;228;48;5;235:no=0;38;5;231:"

InterfaceColors="el=O:mi=BC:dl=B:tx=W:df=W:fc=DB:wc=00;38;5;117:li=BG:si=BB:ti=BC:em=BR:wm=BY:nm=BG:bm=BC:dn=DW:dr=Y:dw=R:dxd=G:dxr=0;38;5;141:dg=Y:dd=0;38;5;141:dz=G:do=C:dp=M:sb=DY:sc=DR:sf=UDC:sh=DM:sp=DR:sx=DG:hb=C:hc=DR:hd=C:he=C:hn=M:hp=C:hq=Y:hr=R:hs=G:hv=G:tt=BDC:ts=UM:wp=DR:ws1=B:ws2=R:ws3=Y:ws4=G:ws5=C:ws6=C:ws7=C:ws8=C:xs=0;38;5;84:xf=R:"

ExtColors="*.r=0;38;5;215:*.h=0;38;5;215:*.d=0;38;5;215:*.z=1;38;5;141:*.a=0;38;5;84:*.c=0;38;5;215:*.p=0;38;5;215:*.m=0;38;5;215:*.o=0;38;5;237:*.t=0;38;5;215:*.hh=0;38;5;215:*.wv=0;38;5;215:*.kt=0;38;5;215:*.bc=0;38;5;237:*.rm=1;38;5;215:*.ml=0;38;5;215:*.ps=0;38;5;215:*.pp=0;38;5;215:*.cr=0;38;5;215:*.xz=1;38;5;141:*.md=0;38;5;215:*.nb=0;38;5;215:*.sh=0;38;5;215:*.go=0;38;5;215:*.el=0;38;5;215:*.ko=0;38;5;84:*.vb=0;38;5;215:*.hi=0;38;5;237:*.as=0;38;5;215:*.jl=0;38;5;215:*.rs=0;38;5;215:*.py=0;38;5;215:*.pl=0;38;5;215:*.pm=0;38;5;215:*.gz=1;38;5;141:*.7z=1;38;5;141:*.ex=0;38;5;215:*.la=0;38;5;237:*.gv=0;38;5;215:*.cp=0;38;5;215:*css=0;38;5;215:*.td=0;38;5;215:*.rb=0;38;5;215:*.ll=0;38;5;215:*.js=0;38;5;215:*.so=0;38;5;84:*.hs=0;38;5;215:*.mn=0;38;5;215:*.bz=1;38;5;141:*.fs=0;38;5;215:*.di=0;38;5;215:*.cs=0;38;5;215:*.ts=0;38;5;215:*.ui=0;38;5;215:*.cc=0;38;5;215:*.lo=0;38;5;237:*.com=0;38;5;84:*TODO=1;38;5;215:*.dot=0;38;5;215:*.fls=0;38;5;237:*.idx=0;38;5;237:*.dmg=1;38;5;141:*.png=0;38;5;228:*.awk=0;38;5;215:*.ilg=0;38;5;237:*.rtf=0;38;5;215:*.ind=0;38;5;237:*.ics=0;38;5;215:*.pyd=0;38;5;237:*.iso=1;38;5;141:*.zst=1;38;5;141:*.vob=1;38;5;215:*.toc=0;38;5;237:*.sbt=0;38;5;215:*.inc=0;38;5;215:*.hpp=0;38;5;215:*.tex=0;38;5;215:*.sql=0;38;5;215:*.def=0;38;5;215:*.vcd=1;38;5;141:*.ico=0;38;5;228:*.aux=0;38;5;237:*.m4v=1;38;5;215:*.zip=1;38;5;141:*.tif=0;38;5;228:*.bbl=0;38;5;237:*.ipp=0;38;5;215:*.fsi=0;38;5;215:*.gvy=0;38;5;215:*.xml=0;38;5;215:*.avi=1;38;5;215:*.zsh=0;38;5;215:*.ps1=0;38;5;215:*.ltx=0;38;5;215:*.cpp=0;38;5;215:*.dpr=0;38;5;215:*.dll=0;38;5;84:*.cfg=0;38;5;215:*.fsx=0;38;5;215:*.mp3=0;38;5;215:*hgrc=0;38;5;215:*.sxw=0;38;5;215:*.tbz=1;38;5;141:*.c++=0;38;5;215:*.kex=0;38;5;215:*.git=0;38;5;237:*.mkv=1;38;5;215:*.log=0;38;5;237:*.flv=1;38;5;215:*.otf=0;38;5;215:*.ttf=0;38;5;215:*.ogg=0;38;5;215:*.bag=1;38;5;141:*.bz2=1;38;5;141:*.txt=0;38;5;215:*.gif=0;38;5;228:*.csx=0;38;5;215:*.exe=0;38;5;84:*.sxi=0;38;5;215:*.vim=0;38;5;215:*.pkg=1;38;5;141:*.apk=1;38;5;141:*.out=0;38;5;237:*.lua=0;38;5;215:*.tmp=0;38;5;237:*.asa=0;38;5;215:*.dox=0;38;5;215:*.hxx=0;38;5;215:*.bak=0;38;5;237:*.odt=0;38;5;215:*.mir=0;38;5;215:*.pyc=0;38;5;237:*.bsh=0;38;5;215:*.mpg=1;38;5;215:*.htc=0;38;5;215:*.php=0;38;5;215:*.ppt=0;38;5;215:*.wmv=1;38;5;215:*.arj=1;38;5;141:*.doc=0;38;5;215:*.xlr=0;38;5;215:*.pbm=0;38;5;228:*.bst=0;38;5;215:*.aif=0;38;5;215:*.mli=0;38;5;215:*.odp=0;38;5;215:*.img=1;38;5;141:*.ods=0;38;5;215:*.swf=1;38;5;215:*.xls=0;38;5;215:*.swp=0;38;5;237:*.rst=0;38;5;215:*.epp=0;38;5;215:*.tgz=1;38;5;141:*.eps=0;38;5;228:*.exs=0;38;5;215:*.fon=0;38;5;215:*.pdf=BR:*.rar=1;38;5;141:*.mp4=1;38;5;215:*.jar=1;38;5;141:*.ini=0;38;5;215:*.m4a=0;38;5;215:*.jpg=0;38;5;228:*.bat=0;38;5;84:*.erl=0;38;5;215:*.deb=1;38;5;141:*.csv=0;38;5;215:*.bcf=0;38;5;237:*.kts=0;38;5;215:*.xcf=0;38;5;228:*.wma=0;38;5;215:*.xmp=0;38;5;215:*.pid=0;38;5;237:*.ppm=0;38;5;228:*.cxx=0;38;5;215:*.tar=1;38;5;141:*.clj=0;38;5;215:*.sty=0;38;5;237:*.pps=0;38;5;215:*.psd=0;38;5;228:*.rpm=1;38;5;141:*.tsx=0;38;5;215:*.pgm=0;38;5;228:*.pyo=0;38;5;237:*.wav=0;38;5;215:*.mid=0;38;5;215:*.mov=1;38;5;215:*.blg=0;38;5;237:*.htm=0;38;5;215:*.inl=0;38;5;215:*.bin=1;38;5;141:*.bmp=0;38;5;228:*.cgi=0;38;5;215:*.pro=0;38;5;215:*.nix=0;38;5;215:*.bib=0;38;5;215:*.yml=0;38;5;215:*.tml=0;38;5;215:*.elm=0;38;5;215:*.fnt=0;38;5;215:*.svg=0;38;5;228:*.pas=0;38;5;215:*.h++=0;38;5;215:*.pod=0;38;5;215:*.tcl=0;38;5;215:*.html=0;38;5;215:*.hgrc=0;38;5;215:*.less=0;38;5;215:*.tbz2=1;38;5;141:*.psd1=0;38;5;215:*.bash=0;38;5;215:*.diff=0;38;5;215:*.pptx=0;38;5;215:*.xlsx=0;38;5;215:*.make=0;38;5;215:*.psm1=0;38;5;215:*.mpeg=1;38;5;215:*.docx=0;38;5;215:*.lock=0;38;5;237:*.jpeg=0;38;5;228:*.rlib=0;38;5;237:*.purs=0;38;5;215:*.flac=0;38;5;215:*.opus=0;38;5;215:*.lisp=0;38;5;215:*.yaml=0;38;5;215:*.java=0;38;5;215:*.toml=0;38;5;215:*.orig=0;38;5;237:*.h264=1;38;5;215:*.epub=0;38;5;215:*.conf=0;38;5;215:*.tiff=0;38;5;228:*.fish=0;38;5;215:*.webm=1;38;5;215:*.dart=0;38;5;215:*.json=0;38;5;215:*.mdown=0;38;5;215:*.scala=0;38;5;215:*passwd=0;38;5;215:*.xhtml=0;38;5;215:*.shtml=0;38;5;215:*shadow=0;38;5;215:*.cache=0;38;5;237:*.dyn_o=0;38;5;237:*.cabal=0;38;5;215:*README=0;38;5;215:*.swift=0;38;5;215:*.cmake=0;38;5;215:*.ipynb=0;38;5;215:*.toast=1;38;5;141:*.class=0;38;5;237:*.patch=0;38;5;215:*.groovy=0;38;5;215:*COPYING=0;38;5;215:*LICENSE=0;38;5;215:*.gradle=0;38;5;215:*.flake8=0;38;5;215:*INSTALL=0;38;5;215:*.ignore=0;38;5;215:*.dyn_hi=0;38;5;237:*TODO.md=1;38;5;215:*.config=0;38;5;215:*.matlab=0;38;5;215:*Doxyfile=0;38;5;215:*TODO.txt=1;38;5;215:*setup.py=0;38;5;215:*Makefile=0;38;5;215:*.desktop=0;38;5;215:*.gemspec=0;38;5;215:*.markdown=0;38;5;215:*.rgignore=0;38;5;215:*.kdevelop=0;38;5;215:*.cmake.in=0;38;5;215:*COPYRIGHT=0;38;5;215:*configure=0;38;5;215:*README.md=0;38;5;215:*.fdignore=0;38;5;215:*.DS_Store=0;38;5;237:*.gitconfig=0;38;5;215:*SConscript=0;38;5;215:*.gitignore=0;38;5;215:*Dockerfile=0;38;5;215:*SConstruct=0;38;5;215:*CODEOWNERS=0;38;5;215:*README.txt=0;38;5;215:*.scons_opt=0;38;5;237:*.localized=0;38;5;237:*INSTALL.md=0;38;5;215:*MANIFEST.in=0;38;5;215:*.gitmodules=0;38;5;215:*Makefile.am=0;38;5;215:*INSTALL.txt=0;38;5;215:*LICENSE-MIT=0;38;5;215:*.travis.yml=0;38;5;215:*Makefile.in=0;38;5;237:*.synctex.gz=0;38;5;237:*CONTRIBUTORS=0;38;5;215:*configure.ac=0;38;5;215:*.applescript=0;38;5;215:*.fdb_latexmk=0;38;5;237:*appveyor.yml=0;38;5;215:*.clang-format=0;38;5;215:*LICENSE-APACHE=0;38;5;215:*.gitattributes=0;38;5;215:*CMakeLists.txt=0;38;5;215:*CMakeCache.txt=0;38;5;237:*CONTRIBUTORS.md=0;38;5;215:*requirements.txt=0;38;5;215:*CONTRIBUTORS.txt=0;38;5;215:*.sconsign.dblite=0;38;5;237:*package-lock.json=0;38;5;237:*.CFUserTextEncoding=0;38;5;237"

DirIconColor="Y"

Prompt="\[\e[0;38;5;253m\][\S\[\e[0;38;5;253m\]]\l \A \u:\H \[\e[0;38;5;117m\]\w\n\[\e[0;38;5;253m\]<\z\[\e[0;38;5;253m\]> \[\e[0;38;5;141m\]\$ \[\e[0;38;5;253m\]"
Notifications=true
EnableWarningPrompt=true
WarningPrompt="\[\e[00;02;38;5;203m\](!) > "

DividingLine="-"

FzfTabOptions="--color='16,prompt:#87d7ff,fg+:-1,pointer:#af87ff,hl:#af87ff,hl+:#af87ff,gutter:-1,marker:#5fff87,query:#dadada,info:#dadada:dim' --marker='*' --bind tab:accept,right:accept,left:abort --inline-info --layout=reverse-list"
