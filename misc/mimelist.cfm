# Mimelist file example for CliFM
# This mimelist covers the most common filetypes

# Extensions
E:^djvu$=djview;zathura;evince;atril
E:^epub$=mupdf;zathura;ebook-viewer
E:^mobi$=ebook-viewer
E:^(cbr|cbz)$=zathura

# MIME types
# Web content
^text/html$=iceweasel;surf;vimprobable;vimprobable2;qutebrowser;dwb;jumanji;luakit;uzbl;uzbl-tabbed;uzbl-browser;uzbl-core;midori;opera;firefox;seamonkey;chromium-browser;chromium;google-chrome;epiphany;konqueror;elinks;links2;links;w3m

# Text
^text/xml$=geany
^text/x-(c|shellscript|perl|script.python|makefile|fortran|java-source|javascript|pascal)$=geany
(^text/.*|application/json|inode/x-empty)=leafpad;mousepad;kate;gedit;pluma;nano;vim;vi;emacs;ed

# Office documents
^application/.*(open|office)document.*=libreoffice;soffice;ooffice

# PDF
.*/pdf$=mupdf;llpp;zathura;mupdf-x11;apvlv;xpdf;evince;atril;okular;epdfview;qpdfview

# Images
^image/gif$=animate
^image/.*=gpicview;inkscape;display;imv;pqiv;sxiv;feh;mirage;ristretto;eog;eom;nomacs;geeqie;gwenview;gimp

# Video and audio
^video/.*=mpv;ffplay;mplayer;mplayer2;vlc;gmplayer;smplayer;totem
^audio/.*=mpv;ffplay -nodisp -autoexit;mplayer;mplayer2;vlc;gmplayer;smplayer;totem

# Fonts
^fonts/.*=fontforge;fontpreview

# Torrent:
application/x-bittorrent=rtorrent;transimission-gtk;transmission-qt;deluge-gtk;ktorrent

# Fallback to another resource opener as last resource
.*=xdg-open;mimeo;mimeopen -n;whippet -m;open;linopen;
