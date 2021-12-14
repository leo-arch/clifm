###############################
#   Mimelist file for CliFM   #
###############################

# This mimelist covers the most common filetypes
# Commented and blank lines are omitted

# It is recommended to edit this file leaving only applications you need
# to speed up the opening process
# The file is read top to bottom and left to right; the first existent
# application found will be used
# Applications defined here are NOT desktop files, but commands (arguments
# could be used as well)

# Use 'E' to match file extensions instead of MIME types.

# Use 'X' to specify a GUI environment and '!X' for non-GUI environments,
# like the kernel built-in console or a remote SSH session.

# Regular expressions are allowed for both file types and extensions.

# Use the %f placeholder to specify the position of the file name to be
# executed in the command. Example:
# mpv %f --terminal=no
# If %f is not specified, the file name will be added to the end of the
# command.

# Running the opening application in the background:
# For GUI applications:
#    APP %f &>/dev/null &
# For terminal applications:
#    TERM -e APP %f &>/dev/null &
# Replace 'TERM' and 'APP' by the corresponding values. The -e option might
# vary depending on the terminal emulator used (TERM)

# Environment variables could be used as well. Example:
# X:text/plain=$TERM -e $EDITOR %f &>/dev/null &;$VISUAL;nano;vi

########################
#    File Extensions   #
########################

X:E:^djvu$=djview;zathura;evince;atril
X:E:^epub$=mupdf;zathura;ebook-viewer
X:E:^mobi$=ebook-viewer
X:E:^(cbr|cbz)$=zathura
X:E:^cfm$=$EDITOR;$VISUAL;nano;nvim;vim;vis;vi;mg;emacs;ed;micro;kak;leafpad;mousepad;featherpad;gedit;kate;pluma
!X:E:^cfm$=$EDITOR;$VISUAL;nano;nvim;vim;vis;vi;mg;emacs;ed;micro;kak

##################
#   MIME types   #
##################

# Directories - only for the open-with command (ow)
# In graphical environment directories will be opened in a new window
X:inode/directory=xterm -e clifm %f &>/dev/null &;xterm -e vifm %f &>/dev/null &;pcmanfm %f &>/dev/null &;thunar %f &>/dev/null &;xterm -e ncdu %f &
!X:inode/directory=vifm;ranger;nnn;ncdu

# Web content
X:^text/html$=$BROWSER;surf;vimprobable;vimprobable2;qutebrowser;dwb;jumanji;luakit;uzbl;uzbl-tabbed;uzbl-browser;uzbl-core;iceweasel;midori;opera;firefox;seamonkey;chromium-browser;chromium;google-chrome;epiphany;konqueror;elinks;links2;links;lynx;w3m
!X:^text/html$=$BROWSER;elinks;links2;links;lynx;w3m

# Text
#X:^text/x-(c|shellscript|perl|script.python|makefile|fortran|java-source|javascript|pascal)$=geany
X:(^text/.*|application/json|inode/x-empty)=$EDITOR;$VISUAL;nano;nvim;vim;vis;vi;mg;emacs;ed;micro;kak;leafpad;mousepad;featherpad;kate;gedit;pluma
!X:(^text/.*|application/json|inode/x-empty)=$EDITOR;$VISUAL;nano;nvim;vim;vis;vi;mg;emacs;ed;micro;kak

# Office documents
X:^application/.*(open|office)document.*=libreoffice;soffice;ooffice

# Archives
X:^application/(zip|gzip|zstd|x-7z-compressed|x-xz|x-bzip*|x-tar|x-iso9660-image)=ad;xarchiver %f &>/dev/null &;lxqt-archiver %f &>/dev/null &;ark %f &>/dev/null &
!X:^application/(zip|gzip|zstd|x-7z-compressed|x-xz|x-bzip*|x-tar|x-iso9660-image)=ad

# PDF
X:.*/pdf$=mupdf;llpp;zathura;mupdf-x11;apvlv;xpdf;evince;atril;okular;epdfview;qpdfview

# Images
X:^image/gif$=animate;pqiv;sxiv -a;nsxiv -a
X:^image/.*=fim;feh;display;sxiv;nsxiv;pqiv;gpicview;qview;qimgv;inkscape;mirage;ristretto;eog;eom;xviewer;viewnior;nomacs;geeqie;gwenview;gthumb;gimp
!X:^image/*=fim;img2txt;cacaview;fbi;fbv

# Video and audio
X:^video/.*=ffplay;mplayer;mplayer2;mpv;vlc;gmplayer;smplayer;totem
X:^audio/.*=ffplay -nodisp -autoexit;mplayer;mplayer2;mpv;vlc;gmplayer;smplayer;totem

# Fonts
X:^font/.*=fontforge;fontpreview

# Torrent:
X:application/x-bittorrent=rtorrent;transimission-gtk;transmission-qt;deluge-gtk;ktorrent

# Fallback to another resource opener as last resource
.*=xdg-open;mimeo;mimeopen -n;whippet -m;open;linopen;
