# Mimelist file for CliFM

# This mimelist covers the most common filetypes

# Commented and blank lines are omitted

# It is recommended to edit this file leaving only applications you need to speed up the opening process
# The file is read top to bottom and left to right; the first existent application found will be used
# Applications defined here are NOT desktop files, but commands (arguments could be used as well)

# Use 'E' to match file extensions instead of MIME types.

# Use 'X' to specify a GUI environment and '!X' for non-GUI environments, like the kernel built-in console
# or a remote SSH session.

# Regular expressions are allowed for both file types and extensions.

# Use the %f placeholder to specify the position of the file name to be executed in the command. Example:
# 'mpv %f --terminal=no'. If %f is not specified, the file name will be added to the end of the command. 

### File Extensions ###

X:E:^djvu$=djview;zathura;evince;atril
X:E:^epub$=mupdf;zathura;ebook-viewer
X:E:^mobi$=ebook-viewer
X:E:^(cbr|cbz)$=zathura

### MIME types ###

# Web content
X:^text/html$=surf;vimprobable;vimprobable2;qutebrowser;dwb;jumanji;luakit;uzbl;uzbl-tabbed;uzbl-browser;uzbl-core;iceweasel;midori;opera;firefox;seamonkey;chromium-browser;chromium;google-chrome;epiphany;konqueror;elinks;links2;links;lynx;w3m
!X:^text/html$=elinks;links2;links;lynx;w3m

# Text
#X:^text/x-(c|shellscript|perl|script.python|makefile|fortran|java-source|javascript|pascal)$=geany
X:(^text/.*|application/json|inode/x-empty)=micro;nano;vim;vi;emacs;ed;leafpad;mousepad;kate;gedit;pluma
!X:(^text/.*|application/json|inode/x-empty)=micro;nano;vim;vi;emacs;ed

# Office documents
X:^application/.*(open|office)document.*=libreoffice;soffice;ooffice

# PDF
X:.*/pdf$=mupdf;llpp;zathura;mupdf-x11;apvlv;xpdf;evince;atril;okular;epdfview;qpdfview

# Images
X:^image/gif$=animate;pqiv;sxiv -a
X:^image/.*=fim;feh;display;sxiv;imv;pqiv;gpicview;inkscape;mirage;ristretto;eog;eom;nomacs;geeqie;gwenview;gimp
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
