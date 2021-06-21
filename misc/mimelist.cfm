# Mimelist file for CliFM

# This mimelist covers the most common filetypes
# Commented and blank lines are omitted
# It is recommended to edit this file leaving only applications you need to speed up the opening process
# The file is read top to bottom and left to right; the first existent application will be used
# Applications defined here are NOT desktop files, but commands (arguments could be used as well)

# Extensions
E:^djvu$=djview;zathura;evince;atril
E:^epub$=mupdf;zathura;ebook-viewer
E:^mobi$=ebook-viewer
E:^(cbr|cbz)$=zathura

# MIME types
# Web content
^text/html$=surf;vimprobable;vimprobable2;qutebrowser;dwb;jumanji;luakit;uzbl;uzbl-tabbed;uzbl-browser;uzbl-core;iceweasel;midori;opera;firefox;seamonkey;chromium-browser;chromium;google-chrome;epiphany;konqueror;elinks;links2;links;w3m

# Text
#^text/x-(c|shellscript|perl|script.python|makefile|fortran|java-source|javascript|pascal)$=geany
(^text/.*|application/json|inode/x-empty)=nano;vim;vi;emacs;ed;leafpad;mousepad;kate;gedit;pluma

# Office documents
^application/.*(open|office)document.*=libreoffice;soffice;ooffice

# PDF
.*/pdf$=mupdf;llpp;zathura;mupdf-x11;apvlv;xpdf;evince;atril;okular;epdfview;qpdfview

# Images
^image/gif$=animate
^image/.*=fim;feh;display;sxiv;imv;pqiv;gpicview;inkscape;mirage;ristretto;eog;eom;nomacs;geeqie;gwenview;gimp

# Video and audio
^video/.*=ffplay;mplayer;mplayer2;mpv;vlc;gmplayer;smplayer;totem
^audio/.*=ffplay -nodisp -autoexit;mplayer;mplayer2;mpv;vlc;gmplayer;smplayer;totem

# Fonts
^fonts/.*=fontforge;fontpreview

# Torrent:
application/x-bittorrent=rtorrent;transimission-gtk;transmission-qt;deluge-gtk;ktorrent

# Fallback to another resource opener as last resource
.*=xdg-open;mimeo;mimeopen -n;whippet -m;open;linopen;
