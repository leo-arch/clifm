                  ###################################
                  #   Configuration file for Lira   #
                  #     CliFM's resource opener     #
                  ###################################

# Commented and blank lines are omitted

# The below settings cover the most common filetypes
# It is recommended to edit this file placing your prefered applications
# at the beginning of the apps list to speed up the opening process

# The file is read top to bottom and left to right; the first existent
# application found will be used

# Applications defined here are NOT desktop files, but commands (arguments
# could be used as well). Write you own handmade scripts to open specific
# files if necessary. Ex: X:^text/.*:~/scripts/my_cool_script.sh

# Use 'X' to specify a GUI environment and '!X' for non-GUI environments,
# like the kernel built-in console or a remote SSH session.

# Use 'N' to match file names instead of MIME types.

# Regular expressions are allowed for both file types and file names.

# Use the %f placeholder to specify the position of the file name to be
# opened in the command. Example:
# 'mpv %f --terminal=no' -> 'mpv FILE --terminal=no' 
# If %f is not specified, the file name will be added to the end of the
# command. Ex: 'mpv --terminal=no' -> 'mpv --terminal=no FILE'

# Running the opening application in the background:
# For GUI applications:
#    APP %f &
# For terminal applications:
#    TERM -e APP %f &
# Replace 'TERM' and 'APP' by the corresponding values. The -e option
# might vary depending on the terminal emulator used (TERM)

# To silence STDERR and/or STDOUT use !E and !O respectivelly (they could
# be used together). Examples:
# Silence STDERR only and run in the foreground:
#    mpv %f !E
# Silence both (STDERR and STDOUT) and run in the background:
#    mpv %f !EO &
# or
#    mpv %f !E !O &

# Environment variables could be used as well. Example:
# X:text/plain=$TERM -e $EDITOR %f &;$VISUAL;nano;vi

###########################
#  File names/extensions  #
###########################

# Match a full file name
#X:N:some_filename=cmd

# Match all file names starting with 'str'
#X:N:^str.*=cmd

# Match files with extension 'ext'
#X:N:.*\.ext$=cmd

X:N:.*\.djvu$=djview;zathura;evince;atril
X:N:.*\.epub$=mupdf;zathura;ebook-viewer
X:N:.*\.mobi$=ebook-viewer
X:N:.*\.(cbr|cbz)$=zathura
X:N:(.*\.cfm$|clifmrc)=$EDITOR;$VISUAL;kak;micro;nvim;vim;vis;vi;mg;emacs;ed;nano;mili;leafpad;mousepad;featherpad;gedit;kate;pluma
!X:N:(.*\.cfm$|clifmrc)=$EDITOR;$VISUAL;kak;micro;nvim;vim;vis;vi;mg;emacs;ed;nano

##################
#   MIME types   #
##################

# Directories - only for the open-with command (ow) and the --open command
# line option
# In graphical environments directories will be opened in a new window
X:inode/directory=xterm -e clifm %f &;xterm -e vifm %f &;pcmanfm %f &;thunar %f &;xterm -e ncdu %f &
!X:inode/directory=vifm;ranger;nnn;ncdu

# Web content
X:^text/html$=$BROWSER;surf;vimprobable;vimprobable2;qutebrowser;dwb;jumanji;luakit;uzbl;uzbl-tabbed;uzbl-browser;uzbl-core;iceweasel;midori;opera;firefox;seamonkey;brave;chromium-browser;chromium;google-chrome;epiphany;konqueror;elinks;links2;links;lynx;w3m
!X:^text/html$=$BROWSER;elinks;links2;links;lynx;w3m

# Text
#X:^text/x-(c|shellscript|perl|script.python|makefile|fortran|java-source|javascript|pascal)$=geany
X:(^text/.*|application/json|inode/x-empty)=$EDITOR;$VISUAL;kak;micro;dte;nvim;vim;vis;vi;mg;emacs;ed;nano;mili;leafpad;mousepad;featherpad;nedit;kate;gedit;pluma;io.elementary.code;liri-text;xed;atom;nota;gobby;kwrite;xedit
!X:(^text/.*|application/json|inode/x-empty)=$EDITOR;$VISUAL;kak;micro;dte;nvim;vim;vis;vi;mg;emacs;ed;nano

# Office documents
X:^application/.*(open|office)document.*=libreoffice;soffice;ooffice

# Archives
# Note: 'ad' is CliFM's built-in archives utility (based on atool). Remove it if you
# prefer another application
X:^application/(zip|gzip|zstd|x-7z-compressed|x-xz|x-bzip*|x-tar|x-iso9660-image)=ad;xarchiver %f &;lxqt-archiver %f &;ark %f &
!X:^application/(zip|gzip|zstd|x-7z-compressed|x-xz|x-bzip*|x-tar|x-iso9660-image)=ad

# PDF
X:.*/pdf$=mupdf;sioyek;llpp;lpdf;zathura;mupdf-x11;apvlv;xpdf;evince;atril;okular;epdfview;qpdfview

# Images
X:^image/gif$=animate;pqiv;sxiv -a;nsxiv -a
X:^image/.*=fim;display;sxiv;nsxiv;pqiv;gpicview;qview;qimgv;inkscape;mirage;ristretto;eog;eom;xviewer;viewnior;nomacs;geeqie;gwenview;gthumb;gimp
!X:^image/*=fim;img2txt;cacaview;fbi;fbv

# Video and audio
X:^video/.*=ffplay;mplayer;mplayer2;mpv;vlc;gmplayer;smplayer;celluloid;qmplayer2;haruna;totem
X:^audio/.*=ffplay -nodisp -autoexit;mplayer;mplayer2;mpv;vlc;gmplayer;smplayer;totem

# Fonts
X:^font/.*=fontforge;fontpreview

# Torrent:
X:application/x-bittorrent=rtorrent;transimission-gtk;transmission-qt;deluge-gtk;ktorrent

# Fallback to another resource opener as last resource
.*=xdg-open;mimeo;mimeopen -n;whippet -m;open;linopen;
