/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
*/

/* settings.h - User settings default values */

#ifndef SETTINGS_H
#define SETTINGS_H

/* User settings */

/* We provide 2 default color schemes: 16 and 256 colors. The 256 colors
 * version is used if supported by the running terminal. */

		/* ############################################
		 * # 1. Default color definitions (16 colors) #
		 * ############################################ */

/* DEF_FILE_COLORS and DEF_IFACE_COLORS are only used to construct the
 * default color scheme file in case it can be found neither in the local dir
 * (usually $HOME/.config/clifm/colors/) nor in the data dir, (usually
 * /usr/local/share/clifm/colors/). */
#define DEF_FILE_COLORS "bd=1;33:ca=30;41:cd=1:di=1;34:ed=2;34:\
ee=32:ef=2:ex=1;32:fi=0:ln=1;36:mh=30;46:nd=4;1;31:nf=:\
no=31;47:or=2;4;36:ow=34;40:pi=4;45;37:so=45;37:su=37;41:sg=30;43:st=37;44:\
tw=37;42:uf=2;4;37:"

#define DEF_IFACE_COLORS "ac=:db=:dd=:de=:df=0:dg=2;35:dk=:dl=2;37:\
dn=2;37:do=:dp=:dr=:dt=:du=35:dw=:dxd=:dxr=:dz=:\
el=36:em=1;31:fc=2;37:hb=36:hc=2;31:lc=:\
hd=36:he=0;36:hn=0:hp=36:hq=33:hr=31:hs=32:hv=32:\
li=1;32:mi=1;36:nm=1;32:ro=:sb=2;33:sc=2;36:sd=2;37:\
sf=4;2;36:sh=2;37:si=1;34:sp=2;31:sx=2;32:sz=4;2;36:\
ti=1;36:ts=4;35:tt=2;1;36:tx=0:\
wc=1;36:wm=1;33:ws1=34:ws2=31:ws3=33:ws4=32:ws5=36:ws6=36:ws7=36:ws8=36:\
xf=1;31:xs=32:"

/* If ExtColors is not defined in the color scheme file (or if this file isn't
 * found), DEF_EXT_COLORS is used instead. */
/* Definitions order: archive, image, video, sound, document, code, uninmportant */
#define DEF_EXT_COLORS "*.tar=1;31:*.tgz=1;31:*.arc=1;31:*.arj=1;31:\
*.taz=1;31:*.lha=1;31:*.lz4=1;31:*.lzh=1;31:*.lzma=1;31:*.tlz=1;31:\
*.txz=1;31:*.tzo=1;31:*.t7z=1;31:*.zip=1;31:*.z=1;31:*.dz=1;31:\
*.gz=1;31:*.lrz=1;31:*.lz=1;31:*.lzo=1;31:*.xz=1;31:*.zst=1;31:\
*.tzst=1;31:*.bz2=1;31:*.bz=1;31:*.tbz=1;31:*.tbz2=1;31:*.tz=1;31:\
*.deb=1;31:*.rpm=1;31:*.jar=1;31:*.war=1;31:*.ear=1;31:*.sar=1;31:\
*.rar=1;31:*.alz=1;31:*.ace=1;31:*.zoo=1;31:*.cpio=1;31:*.7z=1;31:\
*.rz=1;31:*.cab=1;31:*.wim=1;31:*.swm=1;31:*.dwm=1;31:*.esd=1;31:\
*.apk=1;31:\
*.avif=35:*.jpg=35:*.jpeg=35:*.jxl=35:*.mjpg=35:*.mjpeg=35:\
*.gif=35:*.bmp=35:*.xbm=35:*.xpm=35:*.png=35:*.svg=35:*.svgz=35:*.pcx=35:\
*.pbm=35:*.pgm=35:*.ppm=35:*.tga=35:*.tif=35:*.tiff=35:*.mng=35:\
*.mov=1;35:*.mpg=1;35:*.mpeg=1;35:*.m2v=1;35:*.mkv=1;35:*.webm=1;35:\
*.webp=1;35:*.ogm=1;35:*.mp4=1;35:*.m4v=1;35:*.mp4v=1;35:*.vob=1;35:\
*.qt=1;35:*.nuv=1;35:*.wmv=1;35:*.asf=1;35:*.rm=1;35:*.rmvb=1;35:\
*.flc=1;35:*.avi=1;35:*.fli=1;35:*.flv=1;35:*.gl=1;35:*.dl=1;35:\
*.xcf=1;35:*.xwd=1;35:*.yuv=1;35:*.cgm=1;35:*.emf=1;35:*.ogv=1;35:\
*.ogx=1;35:\
*.aac=36:*.au=36:*.m4a=36:*.mid=36:*.midi=36:*.mp3=36:*.mka=36:*.ogg=36:\
*.opus=36:*.spx=36:*.wma=36:*.wv=36:*.wav=36:*.flac=36:*.aif=36:\
*.pdf=1:*.djvu=1:*.epub=1:*.fb2=1:*.mobi=1:*.cbr=1:*.cbz=1:*.ps=1:*.sxw=1:\
*.doc=1:*.docx=1:*.xls=1:*.xlsx=1:*.xlr=1:*.sxi=1:*.ppt=1:*.pptx=1:\
*.odt=1:*.ods=1:*.odp=1:*.rtf=1:\
*.c=1;33:*.c++=1;33:*.h=1;33:*.cc=1;33:*.cpp=1;33:*.h++=1;33:\
*.hh=1;33:*.go=1;33:*.java=1;33:*.js=1;33:*.lua=1;33:*.rb=1;33:*.rs=1;33:\
*.kt=1;33:*.kts=1;33:*.hs=1;33:*.pl=1;33:*.vb=1;33:*.html=01;33:*.htm=1;33:\
*.shtml=1;33:xhtml=1;33:*.xml=1;33:*.php=1;33:*.tex=1;33:*.ltx=1;33:\
*.md=1;33:\
*.cache=2;37:*.tmp=2;37:*.temp=2;37:*.log=2;37:*.bak=2;37:*.bk=2;37:\
*.in=2;37:*.out=2;37:*.part=2;37:*.aux=2;37:*.old=2;37:*.orig=2;37:\
*.rej=2;37:*.swp=2;37:*.pid=2;37:"

/* If some color is not defined, one of the below colors will be used instead.
 * You most likely want to modify this instead of DEF_FILE_COLORS and
 * DEF_IFACE_COLORS above. */

/* File types */
#define DEF_BD_C  "\x1b[1;33m"    /* Block device */
#define DEF_CA_C  "\x1b[30;41m"   /* File with capabilities */
#define DEF_CD_C  "\x1b[0;1m"     /* Character device */
#define DEF_DI_C  "\x1b[1;34m"    /* Dir */
#define DEF_ED_C  "\x1b[2;34m"    /* Empty dir */
#define DEF_EE_C  "\x1b[0;32m"    /* Empty executable file */
#define DEF_EF_C  "\x1b[0;2m"     /* Empty regular file */
#define DEF_EX_C  "\x1b[1;32m"    /* Executable file */
#define DEF_FI_C  "\x1b[0m"       /* Regular file */
#define DEF_LN_C  "\x1b[1;36m"    /* Symbolic link */
#define DEF_MH_C  "\x1b[30;46m"   /* Multi-link */
#define DEF_ND_C  "\x1b[1;31m"    /* Unaccessible dir */
#define DEF_NF_C  ""              /* Unaccessible file */
#define DEF_NO_C  "\x1b[0;31;47m" /* Unknown file type */
#ifdef __sun
#define DEF_OO_C  "\x1b[0;35m"    /* Solaris door/port (ls default is 1;35) */
#endif /* __sun */
#define DEF_OR_C  "\x1b[2;4;36m"  /* Orphaned/broken symlink */
#define DEF_OW_C  "\x1b[34;40m"   /* Other-writable */
#define DEF_PI_C  "\x1b[4;45;37m" /* FIFO/pipe */
#define DEF_SG_C  "\x1b[30;43m"   /* SGID file */
#define DEF_SO_C  "\x1b[45;37m"   /* Socket */
#define DEF_ST_C  "\x1b[37;44m"   /* Sticky bit set */
#define DEF_SU_C  "\x1b[37;41m"   /* SUID file */
#define DEF_TW_C  "\x1b[37;42m"   /* Sticky and other-writable */
#define DEF_UF_C  "\x1b[2;4;37m"  /* Un'stat'able file */

/* Interface */
#define DEF_AC_C  "\001\x1b[2;37m\002"   /* Autocmd indicator */
#define DEF_DF_C  "\x1b[0m"      /* Reset attributes: default terminal color */
#define DEF_DL_C  "\x1b[2;37m"   /* Dividing line */
#define DEF_EL_C  "\x1b[0;36m"   /* ELN's */
#define DEF_EM_C  "\001\x1b[1;31m\002" /* Error msg indicator */
#define DEF_FC_C  "\x1b[0;2;37m" /* File counter */
#define DEF_LI_C  "\001\x1b[1;32m\002" /* Sel files indicator (prompt) */
#define DEF_LI_CB "\x1b[1;32m"   /* Sel files indicator (files list) */
#define DEF_MI_C  "\x1b[1;36m"   /* Misc */
#define DEF_NM_C  "\001\x1b[1;32m\002" /* Notice msg indicator */
#define DEF_RO_C  "\001\x1b[1;34m\002" /* Read-only mode indicator */
#define DEF_SI_C  "\001\x1b[1;34m\002" /* Stealth mode indicator */
#define DEF_TI_C  "\001\x1b[1;36m\002" /* Trash indicator */
#define DEF_TX_C  "\x1b[0m"      /* Input text */
#define DEF_WC_C  "\x1b[1;36m"   /* Welcome message */
#define DEF_WM_C  "\001\x1b[1;33m\002" /* Warning msg indicator */

/* Symlink char indicator (for ColorLinksAsTarget only) */
#define DEF_LC_C  "\x1b[1;36m"

#define DEF_TS_C  "\x1b[4;35m" /* Matching prefix for tab completed possible entries */
#define DEF_TT_C  "\x1b[1;2;36m" /* Tilde for truncated filenames */
#define DEF_WP_C  "\x1b[0;2;31m" /* Warning prompt input text */
#define DEF_XS_C  "\001\x1b[32m\002" /* Exit code: success */
#define DEF_XS_CB "\x1b[32m"     /* Exit code: Unicode success indicator */
#define DEF_XF_C  "\001\x1b[1;31m\002" /* Exit code: failure */
#define DEF_XF_CB "\x1b[1;31m"   /* Exit code: failure (dir read) */

/* Workspaces */
#define DEF_WS1_C "\001\x1b[34m\002"
#define DEF_WS2_C "\001\x1b[31m\002"
#define DEF_WS3_C "\001\x1b[33m\002"
#define DEF_WS4_C "\001\x1b[32m\002"
#define DEF_WS5_C "\001\x1b[36m\002"
#define DEF_WS6_C "\001\x1b[36m\002"
#define DEF_WS7_C "\001\x1b[36m\002"
#define DEF_WS8_C "\001\x1b[36m\002"

/* Suggestions */
#define DEF_SB_C  "\x1b[2;33m"   /* Shell builtins */
#define DEF_SC_C  "\x1b[2;36m"   /* Aliases and binaries in PATH */
#define DEF_SD_C  "\x1b[2;37m"   /* Internal commands description */
#define DEF_SH_C  "\x1b[2;37m"   /* Commands history */
#define DEF_SF_C  "\x1b[2;4;36m" /* ELN's, bookmark, file, and directory names */
#define DEF_SP_C  "\x1b[2;31m"   /* Suggestions pointer (12 > filename) */
#define DEF_SX_C  "\x1b[2;32m"   /* Internal commands and parameters */
#define DEF_SZ_C  "\x1b[2;4;36m" /* Filenames (fuzzy) */

/* Highlight */
#define DEF_HB_C  "\x1b[0;36m" /* Parenthesis, Brackets ( {[()]} ) */
#define DEF_HC_C  "\x1b[2;37m" /* Comments (#comment) */
#define DEF_HD_C  "\x1b[0;36m" /* Slashes (for paths) */
#define DEF_HE_C  "\x1b[0;36m" /* Expansion operators (* ~) */
#define DEF_HN_C  "\x1b[0m"    /* Numbers (including ELN's) */
#define DEF_HP_C  "\x1b[0;36m" /* Parameters (e.g. -h --help) */
#define DEF_HQ_C  "\x1b[0;33m" /* Quotes (single and double) */
#define DEF_HR_C  "\x1b[0;31m" /* Redirection operator (>) */
#define DEF_HS_C  "\x1b[0;32m" /* Process separators (; & |)*/
#define DEF_HV_C  "\x1b[0;32m" /* Variables ($VAR) */
#define DEF_HW_C  "\x1b[1;31m" /* Backslash (aka whack) */

/* Colors for the properties and long/detail view functions */
#define DEF_DB_C  "\x1b[0;33m"   /* File allocated blocks */
#define DEF_DD_C  "\x1b[0;36m"   /* Modification date */
#define DEF_DE_C  "\x1b[0;36m"   /* Inode number (long view only) */
#define DEF_DG_C  "\x1b[0;2;35m" /* Group ID */
#define DEF_DK_C  "\x1b[0;31m"   /* Number of links (long view only) */
#define DEF_DN_C  "\x1b[0;2;37m" /* Dash (no attribute) */
#define DEF_DO_C  "\x1b[0;36m"   /* Perms in octal */
#define DEF_DP_C  "\x1b[0;35m"   /* SUID, SGID, Sticky */
#define DEF_DR_C  "\x1b[0;33m"   /* Read perm */
#define DEF_DU_C  "\x1b[0;35m"   /* User ID */
#define DEF_DW_C  "\x1b[0;31m"   /* Write perm */
#define DEF_DXD_C "\x1b[0;32m"   /* Execute perm (dirs) */
#define DEF_DXR_C "\x1b[0;36m"   /* Execute perm (reg files) */

/* Default color shades for date field in file properties */
#define DEF_DATE_SHADES_8   "1,31-2,37-1,37,37-2,36,36-2"

/* Default color shades for size field in file properties */
#define DEF_SIZE_SHADES_8   "1,31-2,36,32,33,31,35"

#define DEF_DIR_ICO_C "\x1b[0;33m"

		/* #############################################
		 * # 2. Default color definitions (256 colors) #
		 * ############################################# */

#define DEF_FILE_COLORS_256 "bd=1;38;5;229:ca=30;41:cd=1;38;5;214:di=1;34:\
ed=2;34:ee=32:ef=2:ex=1;32:fi=0:ln=1;36:mh=7;36:nd=1;31:nf=:no=4;31;47:\
or=4;2;36:ow=48;5;235;38;5;33:pi=38;5;5;48;5;238:sg=30;43:so=1;38;5;5;48;5;238:\
st=37;44:su=37;41:tw=37;42:uf=4;2;37:"

#define DEF_IFACE_COLORS_256 "ac=:db=:dd=:de=:df=0:dg=2;35:dk=:dl=38;5;243:dn=:\
do=:dp=:dr=:dt=:du=35:dw=:dxd=:dxr=:dz=:el=36:em=1;31:fc=38;5;246:hb=36:hc=2;37:\
hd=36:he=36:hn=:hp=36:hq=LY:hr=31:hs=32:hv=32:lc=38;5;43:li=1;32:mi=1;36:\
nm=1;32:ro=:si=1;34:sb=2;33:sc=2;36:sd=38;5;240:sf=4;2;36:sh=38;5;240:sp=38;5;239:\
sx=2;32:sz=4;2;36:ti=1;36:ts=4;35:tt=1;2;36:tx=0:wc=1;36:wm=38;5;228:ws1=34:\
ws2=31:ws3=38;5;228:ws4=32:ws5=36:ws6=38;5;214:ws7=35:ws8=2;37:xf=1;31:xs=32:"

#define DEF_EXT_COLORS_256 "*.tar=1;31:*.tgz=1;31:*.arc=1;31:*.arj=1;31:\
*.taz=1;31:*.lha=1;31:*.lz4=1;31:*.lzh=1;31:*.lzma=1;31:*.tlz=1;31:*.txz=1;31:\
*.tzo=1;31:*.t7z=1;31:*.zip=1;31:*.z=1;31:*.dz=1;31:*.gz=1;31:*.lrz=1;31:\
*.lz=1;31:*.lzo=1;31:*.xz=1;31:*.zst=1;31:*.tzst=1;31:*.bz2=1;31:*.bz=1;31:\
*.tbz=1;31:*.tbz2=1;31:*.tz=1;31:*.deb=1;31:*.rpm=1;31:*.jar=1;31:*.war=1;31:\
*.ear=1;31:*.sar=1;31:*.rar=1;31:*.alz=1;31:*.ace=1;31:*.zoo=1;31:*.cpio=1;31:\
*.7z=1;31:*.rz=1;31:*.cab=1;31:*.wim=1;31:*.swm=1;31:*.dwm=1;31:*.esd=1;31:\
*.apk=1;31:*.iso=1;31:*.img=1;31:\
*.avif=35:*.jpg=35:*.jpeg=35:*.jxl=35:*.mjpg=35:*.mjpeg=35:*.gif=35:*.bmp=35:\
*.xbm=35:*.xpm=35:*.png=35:*.svg=35:*.svgz=35:*.pcx=35:*.pbm=35:*.pgm=35:\
*.ppm=35:*.tga=35:*.tif=35:*.tiff=35:*.mng=35:\
*.mov=1;35:*.mpg=1;35:*.mpeg=1;35:*.m2v=1;35:*.mkv=1;35:*.webm=1;35:\
*.ogm=1;35:*.mp4=1;35:*.m4v=1;35:*.mp4v=1;35:*.vob=1;35:*.qt=1;35:\
*.nuv=1;35:*.wmv=1;35:*.asf=1;35:*.rm=1;35:*.rmvb=1;35:*.flc=1;35:*.avi=1;35:\
*.fli=1;35:*.flv=1;35:*.gl=1;35:*.dl=1;35:*.xcf=1;35:*.xwd=1;35:*.yuv=1;35:\
*.cgm=1;35:*.emf=1;35:*.ogv=1;35:*.ogx=1;35:*.webp=1;35:\
*.aac=38;5;214:*.au=38;5;214:*.m4a=38;5;214:*.mid=38;5;214:*.midi=38;5;214:\
*.mp3=38;5;214:*.mka=38;5;214:*.ogg=38;5;214:*.opus=38;5;214:*.spx=38;5;214:\
*.wma=38;5;214:*.wv=38;5;214:*.wav=38;5;214:*.flac=38;5;214:*.aif=38;5;214:\
*.pdf=38;5;223:*.djvu=38;5;223:*.epub=38;5;223:*.mobi=38;5;223:*.cbr=38;5;223:\
*.cbz=38;5;223:*.fb2=38;5;223:\
*.ps=38;5;228:*.sxw=38;5;228:*.doc=38;5;228:*.docx=38;5;228:*.xls=38;5;228:\
*.xlsx=38;5;228:*.xlr=38;5;228:*.sxi=38;5;228:*.ppt=38;5;228:*.pptx=38;5;228:\
*.odt=38;5;228:*.ods=38;5;228:*.odp=38;5;228:*.odg=38;5;228:*.rtf=38;5;228:\
*.c=1;38;5;247:*.c++=1;38;5;247:*.cc=1;38;5;247:*.cpp=1;38;5;247:\
*.h=1;38;5;247:*.h++=1;38;5;247:*.hh=1;38;5;247:*.go=1;38;5;247:\
*.java=1;38;5;247:*.js=1;38;5;247:*.lua=1;38;5;247:*.php=1;38;5;247:\
*.py=1;38;5;247:*.rb=1;38;5;247:*.rs=1;38;5;247:*.kt=1;38;5;247:\
*.kts=1;38;5;247:*.hs=1;38;5;247:*.pl=1;38;5;247:*.vb=1;38;5;247:\
*.html=38;5;85:*.htm=38;5;85:*.shtml=38;5;85:*.xhtml=38;5;85:*.xml=38;5;85:\
*.rss=38;5;85:*.css=38;5;85:*.tex=38;5;85:*.ltx=38;5;85:*.md=38;5;85:\
*.opf=38;5;85:\
*.cache=38;5;247:*.tmp=38;5;247:*.temp=38;5;247:*.log=38;5;247:*.bak=38;5;247:\
*.bk=38;5;247:*.in=38;5;247:*.out=38;5;247:*.part=38;5;247:*.aux=38;5;247:\
*.old=38;5;247:*.orig=38;5;247:*.rej=38;5;247:*.swp=38;5;247:*.pid=38;5;247:"

/* File types */
#define DEF_BD_C256  "\x1b[1;38;5;229m" /* Block device */
#define DEF_CA_C256  "\x1b[30;41m"      /* File with capabilities */
#define DEF_CD_C256  "\x1b[1;38;5;214m" /* Character device */
#define DEF_DI_C256  "\x1b[1;34m"       /* Dir */
#define DEF_ED_C256  "\x1b[2;34m"       /* Empty dir */
#define DEF_EE_C256  "\x1b[0;32m"       /* Empty executable file */
#define DEF_EF_C256  "\x1b[0;2m"        /* Empty regular file */
#define DEF_EX_C256  "\x1b[1;32m"       /* Executable file */
#define DEF_FI_C256  "\x1b[0m"          /* Regular file */
#define DEF_LN_C256  "\x1b[1;36m"       /* Symbolic link */
#define DEF_MH_C256  "\x1b[7;36m"       /* Multi-link */
#define DEF_ND_C256  "\x1b[1;31m"       /* Unaccessible dir */
#define DEF_NF_C256  ""                 /* Unaccessible file */
#define DEF_NO_C256  "\x1b[4;31;47m"    /* Unknown file type */
#ifdef __sun
#define DEF_OO_C256  "\x1b[0;35m"    /* Solaris door/port (ls default is 1;35) */
#endif /* __sun */
#define DEF_OR_C256  "\x1b[2;4;36m" /* Orphaned/broken symlink */
#define DEF_OW_C256  "\x1b[48;5;235;38;5;33m" /* Other-writable */
#define DEF_PI_C256  "\x1b[38;5;5;48;5;238m" /* FIFO/pipe */
#define DEF_SG_C256  "\x1b[30;43m"  /* SGID file */
#define DEF_SO_C256  "\x1b[1;38;5;5;48;5;238m"  /* Socket */
#define DEF_ST_C256  "\x1b[37;44m"  /* Sticky bit set */
#define DEF_SU_C256  "\x1b[37;41m"  /* SUID file */
#define DEF_TW_C256  "\x1b[37;42m"  /* Sticky and other-writable */
#define DEF_UF_C256  "\x1b[2;4;37m" /* Un'stat'able file */

/* Interface */
#define DEF_AC_C256  "\001\x1b[38;5;247m\002" /* Autocmd indicator */
#define DEF_DF_C256  "\x1b[0m"      /* Reset attributes: default terminal color */
#define DEF_DL_C256  "\x1b[38;5;243m"   /* Dividing line */
#define DEF_EL_C256  "\x1b[0;36m"   /* ELN's */
#define DEF_EM_C256  "\001\x1b[1;31m\002" /* Error msg indicator */
#define DEF_FC_C256  "\x1b[0;38;5;247m" /* File counter */
#define DEF_LI_C256  "\001\x1b[1;32m\002" /* Sel files indicator (prompt) */
#define DEF_LI_CB256 "\x1b[1;32m"   /* Sel files indicator (file list) */
#define DEF_MI_C256  "\x1b[1;36m"   /* Misc */
#define DEF_NM_C256  "\001\x1b[1;32m\002" /* Notice msg indicator */
#define DEF_RO_C256  "\001\x1b[1;34m\002" /* Read-only mode indicator */
#define DEF_SI_C256  "\001\x1b[1;34m\002" /* Stealth mode indicator */
#define DEF_TI_C256  "\001\x1b[1;36m\002" /* Trash indicator */
#define DEF_TX_C256  "\x1b[0m"      /* Input text */
#define DEF_WC_C256  "\x1b[1;36m"   /* Welcome message */
#define DEF_WM_C256  "\001\x1b[38;5;228m\002" /* Warning msg indicator */

/* Symlink char indicator (for ColorLinksAsTarget only) */
#define DEF_LC_C256 "\x1b[0;38;5;43m"

#define DEF_TS_C256   "\x1b[4;35m" /* Matching prefix for tab completed possible entries */
#define DEF_TT_C256   "\x1b[1;2;36m" /* Tilde for truncated filenames */
#define DEF_WP_C256   "\x1b[0;2;31m" /* Warning prompt input text */
#define DEF_XS_C256   "\001\x1b[32m\002" /* Exit code: success */
#define DEF_XS_CB256  "\x1b[32m"     /* Exit code: Unicode success indicator */
#define DEF_XF_C256   "\001\x1b[1;31m\002" /* Exit code: failure */
#define DEF_XF_CB256  "\x1b[1;31m"   /* Exit code: failure (dir read) */

/* Workspaces */
#define DEF_WS1_C256 "\001\x1b[34m\002"
#define DEF_WS2_C256 "\001\x1b[31m\002"
#define DEF_WS3_C256 "\001\x1b[38;5;228m\002"
#define DEF_WS4_C256 "\001\x1b[32m\002"
#define DEF_WS5_C256 "\001\x1b[36m\002"
#define DEF_WS6_C256 "\001\x1b[38;5;214m\002"
#define DEF_WS7_C256 "\001\x1b[35m\002"
#define DEF_WS8_C256 "\001\x1b[2;37m\002"

/* Suggestions */
#define DEF_SB_C256  "\x1b[2;33m"   /* Shell builtins */
#define DEF_SC_C256  "\x1b[2;36m"   /* Aliases and binaries in PATH */
#define DEF_SD_C256  "\x1b[38;5;240m"   /* Internal commands description */
#define DEF_SH_C256  "\x1b[38;5;240m"   /* Commands history */
#define DEF_SF_C256  "\x1b[2;4;36m" /* ELN's, bookmark, file, and directory names */
#define DEF_SP_C256  "\x1b[38;5;239m"   /* Suggestions pointer (12 > filename) */
#define DEF_SX_C256  "\x1b[2;32m"   /* Internal commands and parameters */
#define DEF_SZ_C256  "\x1b[2;4;36m" /* Filenames (fuzzy) */

/* Highlight */
#define DEF_HB_C256  "\x1b[0;36m" /* Parenthesis, Brackets ( {[()]} ) */
#define DEF_HC_C256  "\x1b[2;37m" /* Comments (#comment) */
#define DEF_HD_C256  "\x1b[0;36m" /* Slashes (for paths) */
#define DEF_HE_C256  "\x1b[0;36m" /* Expansion operators (* ~) */
#define DEF_HN_C256  "\x1b[0m"    /* Numbers (including ELN's) */
#define DEF_HP_C256  "\x1b[0;36m" /* Parameters (e.g. -h --help) */
#define DEF_HQ_C256  "\x1b[0;38;5;185m" /* Quotes (single and double) */
#define DEF_HR_C256  "\x1b[0;31m" /* Redirection operator (>) */
#define DEF_HS_C256  "\x1b[0;32m" /* Process separators (; & |)*/
#define DEF_HV_C256  "\x1b[0;32m" /* Variables ($VAR) */
#define DEF_HW_C256  "\x1b[1;31m" /* Backslash (aka whack) */

/* Colors for the properties and long/detail view functions */
#define DEF_DB_C256  "\x1b[0;38;5;214m" /* File allocated blocks */
#define DEF_DD_C256  "\x1b[0;36m"       /* Modification date */
#define DEF_DE_C256  "\x1b[0;38;5;79m"  /* Inode number (long view only) */
#define DEF_DG_C256  "\x1b[0;2;35m"     /* Group ID */
#define DEF_DK_C256  "\x1b[0;38;5;197m" /* Number of links (long view only) */
#define DEF_DN_C256  "\x1b[0;2;37m"     /* Dash (no attribute) */
#define DEF_DO_C256  "\x1b[0;38;5;79m"  /* Perms in octal */
#define DEF_DP_C256  "\x1b[0;38;5;140m" /* SUID, SGID, Sticky */
#define DEF_DR_C256  "\x1b[0;38;5;228m" /* Read perm */
#define DEF_DU_C256  "\x1b[0;35m"       /* User ID */
#define DEF_DW_C256  "\x1b[0;38;5;197m" /* Write perm */
#define DEF_DXD_C256 "\x1b[0;38;5;77m"  /* Execute perm (dirs) */
#define DEF_DXR_C256 "\x1b[0;38;5;79m"  /* Execute perm (reg files) */

/* Default color shades for date field in file properties */
#define DEF_DATE_SHADES_256 "2,197-2,231,253,250,247,244"

/* Default color shades for size field in file properties */
#define DEF_SIZE_SHADES_256 "2,197-2,79,77,228,215,203"

#define DEF_DIR_ICO_C256 "\x1b[0;33m"

/* Character used to print unkonwn/wrong values */
#define UNKNOWN_CHR     '?'
/* Same as UNKNOWN_CHR, but as a string */
#define UNKNOWN_STR     "?"

/* Characters used to classify files when running colorless (and classify
 * is enabled) */
#define CHR_CHR     '-'
#define BLK_CHR     '+'
#define BRK_LNK_CHR '!'
#define DIR_CHR     '/'
#define DOOR_CHR    '>' /* Solaris only */
#define EXEC_CHR    '*'
#define FIFO_CHR    '|'
#define LINK_CHR    '@'
#define SOCK_CHR    '='
#define WHT_CHR     '%' /* BSD whiteout */
#define UNK_CHR     UNKNOWN_CHR

/* Characters used to represent file types in the permissions string
 * (long view and p/pp command) */
#define DIR_PCHR     'd'
#define REG_PCHR     '.'
#define SOCK_PCHR    's'
#define BLKDEV_PCHR  'b'
#define CHARDEV_PCHR 'c'
#define FIFO_PCHR    'p'
#define LNK_PCHR     'l'
#define WHT_PCHR     'w' /* Whiteout (BSD) */
#define ARCH1_PCHR   'a' /* Archive state 1 (NetBSD) */
#define ARCH2_PCHR   'A' /* Archive state 2 (NetBSD) */
#define DOOR_PCHR    'D' /* Door (Solaris) */
#define PORT_PCHR    'P' /* Event port (Solaris) */
#define UNK_PCHR     UNKNOWN_CHR

/* When any of 'nf' or 'nd' color codes is unset, prepend this character
 * (a single one) to unaccessible files or directories in the file list
 * (only if not running in light mode and icons are disabled). */
#define NO_PERM_STR "!"

#define SELFILE_CHR   'S' /* Prompt indicator */
#define SELFILE_STR   "*" /* Selected file mark for file list (ASCII) */
#define SELFILE_STR_U "┃" /* Unicode alternative */

/* Symbolic link mark for listed files (if ColorLinksAsTarget is enabled). */
#define LINK_STR   "@" /* ASCII */
#define LINK_STR_U "@" /* Unicode alternative */

#define TRUNC_FILE_CHR '~'

/* Character used to mark files with extended attributes (long view) */
#define XATTR_CHAR '@'
#define XATTR_STR  "@"

/* Character used to mark dir sizes for which du(1) reported an error. */
#define DU_ERR_CHAR '!'

/* Character used to replace invalid characters (either a control char or an
 * invalid UTF-8 char) in filenames. */
#define INVALID_CHR '^'

/* A few identifiers used in the prompt */
#define ROOT_USR_CHAR     '#'
#define NON_ROOT_USR_CHAR '$'
#define LIGHT_MODE_CHAR   'L'

/* String used for the messages pointer (e.g.: "-> 2 file(s) removed")
 * Note: The color used for this string is 'mi' (in the color scheme file). */
#define MSG_PTR_STR   "->"
#define MSG_PTR_STR_U "→"  /* Unicode alternative */

/* Used only by the 'pp' command on a symbolic link */
#define MSG_PTR_STR_LEFT   "<-"
#define MSG_PTR_STR_LEFT_U "←"

/* Pointer used for successful operations (color used is 'xs')*/
#define SUCCESS_PTR_STR_U "✔"
#define SUCCESS_PTR_STR   "->"

//#define ERROR_PTR_STR_U   "✗"
//#define ERROR_PTR_STR     "->"

/* Used in multiple places, e.g., when listing prompts, color schemes, profiles,
 * and workspaces ('config dump' as well). */
#define MISC_PTR   ">"
#define MISC_PTR_U "┃"

/* If PrintDirCmds is enabled */
#define DIR_CMD_PTR   ">"
#define DIR_CMD_PTR_U DIR_CMD_PTR

/* Character used to print non-matching suggestions, e.g. "12 > filename". */
#define SUG_POINTER '>'

#define MNT_UDEVIL 	0
#define MNT_UDISKS2 1

/* Default options */
/* Default answers for confirmation prompts: 0 or 'u' (unset), 'y', or 'n' */
#define DEF_ANSWER_BULK_RENAME 'n' /* 'br' command */
#define DEF_ANSWER_OVERWRITE   'n' /* 'c' and 'm' commands mostly */
#define DEF_ANSWER_REMOVE      'n' /* 'r' command */
#define DEF_ANSWER_TRASH       'y' /* 't' command */
#define DEF_ANSWER_DEFAULT     'y' /* Remaining prompts */
#define DEF_ANSWER_DEFAULT_ALL 0   /* All prompts (overrides the above ones) */

#define DEF_APPARENT_SIZE 1
#define DEF_AUTOCD 1
/* InformAutocmd values:
 * AUTOCMD_MSG_NONE, AUTOCMD_MSG_MINI, AUTOCMD_MSG_SHORT, AUTOCMD_MSG_LONG */
#define DEF_AUTOCMD_MSG AUTOCMD_MSG_PROMPT
#define DEF_AUTO_OPEN 1
#define DEF_AUTOLS 1
#define DEF_CASE_SENS_LIST 0
#define DEF_CASE_SENS_DIRJUMP 0
#define DEF_CASE_SENS_PATH_COMP 0
#define DEF_CASE_SENS_SEARCH 0
#define DEF_CD_ON_QUIT 0
#define DEF_CHECK_CAP 1 /* Check files capabilities */
#define DEF_CHECK_EXT 1 /* Check filenames extension (for color) */
#define DEF_CLASSIFY 1
#define DEF_CLEAR_SCREEN 1
#define DEF_CMD_DESC_SUG 1
#define DEF_COLOR_SCHEME "default"
#define DEF_COLOR_SCHEME_256 "default-256"
#define DEF_COLORS 1
#define DEF_COLOR_LNK_AS_TARGET 0
#define DEF_COLUMNS 1
#define DEF_CP_CMD CP_CP
#define DEF_CUR_WS 0 /* First workspace */
#define DEF_CWD_IN_TITLE 0
#define DEF_DESKTOP_NOTIFICATIONS 0
#define DEF_DIRHIST_MAP 0
#define DEF_DISK_USAGE 0
#define DEF_DIV_LINE "-"
#define DEF_DIV_LINE_U "─" /* Unicode alternative */
#define DEF_ELN_USE_WORKSPACE_COLOR 0
/* Available styles: QUOTING_STYLE_BACKSLASH, QUOTING_STYLE_SINGLE_QUOTES,
 * and QUOTING_STYLE_DOUBLE_QUOTES */
#define DEF_QUOTING_STYLE QUOTING_STYLE_BACKSLASH
#define DEF_EXT_CMD_OK 1
#define DEF_FILES_COUNTER 1
#define DEF_FOLLOW_SYMLINKS 1
#define DEF_FOLLOW_SYMLINKS_LONG 0
#define DEF_FULL_DIR_SIZE 0
#define DEF_FUZZY_MATCH 0
#define DEF_FUZZY_MATCH_ALGO 2 /* 1 or 2. 2 is Unicode aware, but slower than 1 */
#define DEF_FZF_WIN_HEIGHT 40 /* Max screen percentage taken by FZF */
#define DEF_FZF_PREVIEW 1
#define DEF_HIGHLIGHT 1
#define DEF_HIST_STATUS 1
#define DEF_HISTIGNORE "^[q,Q]$|^quit$|^exit$|^ .*|^#.*|^[0-9]+$|^\\.+$"
#define DEF_ICONS 0
#define DEF_ICONS_GAP 1 /* Number of spaces between icons and filenames */
#define DEF_INT_VARS 0
#define DEF_KITTY_KEYS 0 /* Kitty keyboard protocol */
#define DEF_LIGHT_MODE 0
/* Possible values: LNK_CREAT_REG, LNK_CREAT_ABS, and LNK_CREAT_REL */
#define DEF_LINK_CREATION_MODE LNK_CREAT_REG
#define DEF_LIST_DIRS_FIRST 1
#define DEF_LISTING_MODE VERTLIST
#define DEF_LOG_MSGS 0
#define DEF_LOG_CMDS 0
#define DEF_LONG_VIEW 0
#define DEF_MAX_DIRHIST 100
#define DEF_MAX_FILES UNSET
#define DEF_MAX_NAME_LEN 32
#define DEF_MAX_HIST 1000
#define DEF_MAX_JUMP_TOTAL_RANK 100000
#define DEF_MAX_LOG 1000
#define DEF_MAX_PRINTSEL 0
#define DEF_MIN_JUMP_RANK 10
#define DEF_MIN_NAME_TRUNC 20
#define DEF_MOUNT_CMD MNT_UDEVIL
#define DEF_MV_CMD MV_MV
#define DEF_NOELN 0
#define DEF_ONLY_DIRS 0
#define DEF_PAGER 0
/* Possible values: PAGER_AUTO, PAGER_LONG, and PAGER_SHORT */
#define DEF_PAGER_VIEW PAGER_AUTO
#define DEF_PREVIEW_MAX_SIZE -1 /* Max size in KiB. -1 == unlimited */
#define DEF_PRINT_DIR_CMDS 0
#define DEF_PRINTSEL 0
#define DEF_PRINT_REMOVED_FILES 0
#define DEF_PRIORITY_SORT_CHAR ""
#define DEF_PRIVATE_WS_SETTINGS 0
#define DEF_PROMPT_B_MIN 0
#define DEF_PROMPT_B_PRECISION 2
#define DEF_PROMPT_F_DIR_LEN 1
#define DEF_PROMPT_F_FULL_LEN_DIRS 1
#define DEF_PROMPT_P_MAX_PATH 40
/* "xfpIsm" =
 * xattrs/caps/ACLs, file counter, perms, owner/grp names, size (human), mtime */
#define DEF_PROP_FIELDS "xfpIsm"
#define DEF_PROP_FIELDS_GAP 1 /* Spaces between columns in long view */
#define DEF_PURGE_JUMPDB 0
#define DEF_READ_AUTOCMD_FILES 0
#define DEF_READ_DOTHIDDEN 0
#define DEF_READONLY 0
#define DEF_REFRESH_ON_EMPTY_LINE 1
#define DEF_REFRESH_ON_RESIZE 1
#define DEF_RELATIVE_TIME 0
#define DEF_REPORT_CWD 0 /* Report CWD to terminal via OSC-7 escape sequence */
#define DEF_RESTORE_LAST_PATH 1
#define DEF_RL_EDIT_MODE 1
/* Check filenames safety with 'n' and 'm' commands. Allowed values:
 * SAFENAMES_NOCHECK: no check is performed
 * SAFENAMES_BASIC: basic checks such as leading/trailing whitespaces, ctrl chars, etc
 * SAFENAMES_STRICT: basic + disallow shell metacharacters
 * SAFENAMES_POSIX: basic + allow only chars in the POSIX Portable Filename Character Set */
#define DEF_SAFE_FILENAMES SAFENAMES_BASIC
#define DEF_SECURE_CMDS 0
#define DEF_SECURE_ENV 0
#define DEF_SECURE_ENV_FULL 0
#define DEF_SHARE_SELBOX 0
/* Supported values: HIDDEN_FALSE, HIDDEN_TRUE, HIDDEN_FIRST, and HIDDEN_LAST */
#define DEF_SHOW_HIDDEN HIDDEN_FALSE
#define DEF_SHOW_BACKUP_FILES 1
#define DEF_SKIP_NON_ALNUM_PREFIX 1
#define DEF_SI 0 /* If 1, compute sizes in powers of 1000 instead of 1024 */
/* Supported sort options:
 * SNONE, SNAME, STSIZE, SATIME, SBTIME, SCTIME, SMTIME
 * SVER, SEXT, SINO, SOWN, and SGRP */
#define DEF_SORT SVER
#define DEF_SORT_REVERSE 0
#define DEF_SPLASH_SCREEN 0
#ifdef __OpenBSD__
# define DEF_SUDO_CMD "doas"
#else
# define DEF_SUDO_CMD "sudo"
#endif /* __OpenBSD__ */
#define DEF_SUG_FILETYPE_COLOR 0
#define DEF_SUG_STRATEGY "ehfjac"
#define DEF_SUGGESTIONS 1
#define DEF_TIME_FOLLOWS_SORT 1
#define DEF_TIME_STYLE_RECENT "%b %e %H:%M" /* Timestamps in long view mode */
#define DEF_TIME_STYLE_OLDER  "%b %e  %Y"
#define DEF_TIME_STYLE_LONG   "%a %b %d %T %Y %z" /* Used by history and trash */
#define DEF_TIMESTAMP_MARK 0
#define DEF_TIPS 1
#define DEF_TRASRM 0
#define DEF_TRASH_FORCE 0
#define DEF_TRUNC_NAMES 1
#define DEF_WELCOME_MESSAGE 1

/* This expands to "CliFM > The command line file manager" */
#define DEF_WELCOME_MESSAGE_STR PROGRAM_NAME_UPPERCASE " > " PROGRAM_DESC

/* We have three search strategies: GLOB_ONLY, REGEX_ONLY, GLOB_REGEX */
#define DEF_SEARCH_STRATEGY GLOB_REGEX

/* Four bell styles: BELL_NONE, BELL_AUDIBLE, BELL_VISIBLE, BELL_FLASH */
#define DEF_BELL_STYLE     BELL_VISIBLE
#define VISIBLE_BELL_DELAY 30 /* Milliseconds */

/* cp and mv commands
 * All options used for cp(1) and mv(1) are POSIX. No need to check. */
#define DEFAULT_CP_CMD "cp -Rp"
#define DEFAULT_CP_CMD_FORCE "cp -Rp"
#define DEFAULT_ADVCP_CMD "advcp -gRp"
#define DEFAULT_ADVCP_CMD_FORCE "advcp -gRp"
#define DEFAULT_WCP_CMD "wcp"
#define DEFAULT_RSYNC_CMD "rsync -avP"

#define DEFAULT_MV_CMD "mv"
#define DEFAULT_MV_CMD_FORCE "mv"
#define DEFAULT_ADVMV_CMD "advmv -g"
#define DEFAULT_ADVMV_CMD_FORCE "advmv -g"

#define DEF_RM_FORCE 0

#define MAX_WS 8
#define MIN_SCREEN_WIDTH 20
#define MIN_SCREEN_HEIGHT 5

#define DEFAULT_WIN_COLS 80
#define DEFAULT_WIN_ROWS 24

/* If running colorless, let's try to use this prompt. If not available,
 * fallback to DEFAULT_PROMPT_NO_COLOR. */
#define DEF_PROMPT_NO_COLOR_NAME "clifm-no-color"
#define DEF_PROMPT_NOTIF 1
#define DEFAULT_PROMPT "\\[\\e[0m\\]\\I[\\S\\[\\e[0m\\]]\\l \
\\A \\u:\\H \\[\\e[0;36m\\]\\w\\n\\[\\e[0m\\]<\\z\\[\\e[0m\\]\
>\\[\\e[0;34m\\] \\$\\[\\e[0m\\] "
#define DEFAULT_PROMPT_NO_COLOR "\\I[\\S]\\l \\A \\u:\\H \\w\\n<\\z> \\$ "

#define DEF_WARNING_PROMPT 1
#define DEF_WPROMPT_STR_NO_COLOR "(!) > "
#if defined(RL_READLINE_VERSION) && RL_READLINE_VERSION < 0x0700
# define DEF_WPROMPT_STR DEF_WPROMPT_STR_NO_COLOR
#else
# define DEF_WPROMPT_STR "\\[\\e[0;1;31m\\](!)\\[\\e[0;2m\\] > "
#endif /* READLINE < 7.0 */

/* These options, except --preview-window=border-left (fzf 0.27), work with at
 * least fzf 0.18.0 (Mar 31, 2018) */
#define DEF_FZFTAB_OPTIONS "--color=16,prompt:6,fg+:-1,pointer:4,\
hl:2,hl+:2,gutter:-1,marker:2,border:7:dim --bind tab:accept,right:accept,\
left:abort,alt-p:toggle-preview,change:top,alt-up:preview-page-up,\
alt-down:preview-page-down --tiebreak=begin --inline-info --layout=reverse-list \
--preview-window=wrap,border-left"
// --preview-window=noborder (0.19, Nov 15, 2019)

#define DEF_FZFTAB_OPTIONS_NO_COLOR "--color=bw --bind tab:accept,\
right:accept,left:abort,alt-p:toggle-preview,change:top,alt-up:preview-page-up,\
alt-down:preview-page-down --tiebreak=begin --inline-info --layout=reverse-list \
--preview-window=wrap,border-left"

#define DEF_SMENU_OPTIONS "-a t:2,b b:4 c:r ct:2,r sf:6,r st:5,r mt:5,b"

/* Name of the file to store thumbnails original paths.
 * Each line in this file has this format: THUMB_FILE@FILE_URI,
 * where THUMB_FILE is the name of the thumbnail file (MD5 hash of FILE_URI),
 * and FILE_URI is the file URI for the absolute path to the original filename.
 * This file is located in $XDG_CACHE_HOME/clifm/thumbnails */
#define THUMBNAILS_INFO_FILE ".thumbs.info"

/* Should we add __APPLE__ here too? */
#if defined(__HAIKU__)
# define DEF_TERM_CMD "Terminal"
#else
# define DEF_TERM_CMD "xterm -e"
#endif /* __HAIKU__ */

#define FALLBACK_SHELL "/bin/sh"

#if defined(__APPLE__)
# define FALLBACK_OPENER "/usr/bin/open"
#elif defined(__CYGWIN__)
# define FALLBACK_OPENER "cygstart"
#elif defined(__HAIKU__)
# define FALLBACK_OPENER "open"
#else
# define FALLBACK_OPENER "xdg-open"
#endif /* __APPLE__ */

#endif /* SETTINGS_H */
