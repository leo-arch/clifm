/* Icon definitions for CliFM */

struct icons_t
{
	char *name;
	char *icon;
	char *color;
};

/* Icons (from the 'icons-in-terminal' project) */
char
	/* File types */
	ICON_DIR[4] = "\ue155",
	ICON_LOCK[4] = "\ue200",
	ICON_REG[4] = "\ue0f6",
	ICON_EXEC[4] = "\ue0f3",
	ICON_LINK[4] = "\ue18a",

	/* Extensions */
	ICON_BINARY[4] = "\ue073",
	ICON_MANPAGE[4] = "\ue799",
	ICON_MAKEFILE[4] = "\ue7a2",
	ICON_MARKDOWN[4] = "\uea45",
	ICON_DATABASE[4] = "\uedc0",
	ICON_CONF[4] = "\ue15e",
	ICON_DIFF[4] = "\ue7c3",
	ICON_DEB[4] = "\uedc1",
	ICON_CODE[4] = "\ue282",
	ICON_VIM[4] = "\ueacc",
	ICON_NASM[4] = "\ue8d5",
	ICON_JAVA[4] = "\uede3",
	ICON_JAVASCRIPT[4] = "\uede6",
	ICON_ELF[4] = "\ue23a",
	ICON_HTML[4] = "\ue282",
	ICON_GO[4] = "\uedd0",
	ICON_PHP[4] = "\uee09",
	ICON_PERL[4] = "\uee05",
	ICON_SCALA[4] = "\uee1f",
	ICON_MYSQL[4] = "\uedf8",
	ICON_FSHARP[4] = "\ueaae",
	ICON_LUA[4] = "\ue77e",
	ICON_PATCH[4] = "\ue7c4",
	ICON_IMG[4] = "\ue27e",
 	ICON_VID[4] = "\ue281",
	ICON_AUDIO[4] = "\ue280",
	ICON_PDF[4] = "\ue27a",
	ICON_ARCHIVE[4] = "\ue27f",
	ICON_CD[4] = "\uecd3",
	ICON_SCRIPT[4] = "\uee1d",
	ICON_TXT[4] = "\ue1bc",
	ICON_C[4] = "\uedb1",
	ICON_CPP[4] = "\uedb8",
	ICON_CSHARP[4] = "\uedb9",
	ICON_PYTHON[4] = "\uee10",
	ICON_WINDOWS[4] = "\uea16",
	ICON_WORD[4] = "\ue850",
	ICON_EXCEL[4] = "\ue851",
	ICON_ACCESS[4] = "\ue84d",
	ICON_POWERPOINT[4] = "\ue84f",
	ICON_PHOTOSHOP[4] = "\ueabf",
	ICON_COPYRIGHT[4] = "\ue2af",
	ICON_CONFIGURE[4] = "\ue8df",
	ICON_HISTORY[4] = "\ue292",
	ICON_ARCH[4] = "\uedaa",

	/* Dir names */
	ICON_DESKTOP[4] = "\ue1cd",
	ICON_DOCUMETS[4] = "\ue187",
	ICON_DOWNLOADS[4] = "\ue0f9",
	ICON_HOME[4] = "\ue0f5",
	ICON_TRASH[4] = "\ue0f4",
	ICON_FAV[4] = "\ue0e7",
	ICON_MUSIC[4] = "\ue0e2",
	ICON_PICTURES[4] = "\ue4fb",
	ICON_VIDEOS[4] = "\ue0e9",
	ICON_PUBLIC[4] = "\ue0fc",
	ICON_TEMPLATES[4] = "\ue18f",
	ICON_DOTGIT[4] = "\ue28b",
	ICON_GAMES[4] = "\ue1df",
	ICON_DROPBOX[4] = "\ue22a",
	ICON_STEAM[4] = "\ue270";

/*	ICON_MAIL[4] = "\ue1a7",
	ICON_NET[4] = "\ue1af",
	ICON_MYPC[4] = "\ue1cd",
	ICON_USB[4] = "\ue334",
	ICON_BLUETOOTH[4] = "\ue33f",
	ICON_CLOUD[4] = "\ue574",
	ICON_PRINTER[4] = "\ue5a9",
	ICON_ANDROID[4] = "\ue64d",
	ICON_APPLE[4] = "\uea18",
	ICON_CHROME[4] = "\uea4a",
	ICON_IE[4] = "\uea4b",
	ICON_FIREFOX[4] = "\uea4c",
	ICON_OPERA[4] = "\uea4d"; */

#define BLUE "\x1b[0;34m"
#define B_BLUE "\x1b[1;34m"
#define WHITE "\x1b[0;37m"
#define B_WHITE "\x1b[1;37m"
#define YELLOW "\x1b[0;33m"
#define B_YELLOW "\x1b[1;33m"
#define GREEN "\x1b[0;32m"
#define B_GREEN "\x1b[1;32m"
#define CYAN "\x1b[0;36m"
#define B_CYAN "\x1b[1;36m"
#define MAGENTA "\x1b[0;35m"
#define B_MAGENTA "\x1b[1;35m"
#define RED "\x1b[0;31m"
#define B_RED "\x1b[1;31m"
#define BLACK "\x1b[0;30m"
#define B_BLACK "\x1b[1;30m"

/* Default icons and colors for directories and files */
#define DEF_DIR_ICON ICON_DIR
#define DEF_DIR_ICON_COLOR YELLOW
#define DEF_FILE_ICON ICON_REG
#define DEF_FILE_ICON_COLOR WHITE

/* Per file extension icons */
struct icons_t icon_ext[] = {
	{ "1", ICON_MANPAGE, WHITE },
	{ "7z", ICON_ARCHIVE, YELLOW },

	{ "a", ICON_MANPAGE, WHITE },
	{ "apk", ICON_ARCHIVE, YELLOW },
	{ "asm", ICON_NASM, WHITE },
	{ "aup", ICON_AUDIO, YELLOW },
	{ "avi", ICON_VID, BLUE },

	{ "bat", ICON_SCRIPT, WHITE },
	{ "bin", ICON_EXEC, WHITE },
	{ "bmp", ICON_IMG, GREEN },
	{ "bz2", ICON_ARCHIVE, YELLOW },

	{ "c", ICON_C, BLUE },
	{ "c++", ICON_CPP, WHITE },
	{ "cab", ICON_ARCHIVE, YELLOW },
	{ "cbr", ICON_ARCHIVE, YELLOW },
	{ "cbz", ICON_ARCHIVE, YELLOW },
	{ "cc", ICON_CPP, WHITE },
	{ "class", ICON_JAVA, WHITE },
	{ "cmake", ICON_MAKEFILE, WHITE },
	{ "conf", ICON_CONF, WHITE },
	{ "cpio", ICON_ARCHIVE, YELLOW },
	{ "cpp", ICON_CPP, WHITE },
	{ "cue", ICON_AUDIO, YELLOW },
	{ "cvs", ICON_CONF, WHITE },
	{ "cxx", ICON_CPP, WHITE },

	{ "db", ICON_DATABASE, WHITE },
	{ "deb", ICON_DEB, RED },
	{ "diff", ICON_DIFF, WHITE },
	{ "dll", ICON_MANPAGE, WHITE },
	{ "doc", ICON_WORD, BLUE },
	{ "docx", ICON_WORD, BLUE },

	{ "ejs", ICON_CODE, WHITE },
	{ "elf", ICON_ELF, WHITE },
	{ "epub", ICON_PDF, B_RED },
	{ "exe", ICON_WINDOWS, BLUE },

	{ "f#", ICON_FSHARP, WHITE },
	{ "flac", ICON_AUDIO, YELLOW },
	{ "flv", ICON_VID, BLUE },
	{ "fs", ICON_FSHARP, WHITE },
	{ "fsi", ICON_FSHARP, WHITE },
	{ "fsscript", ICON_FSHARP, WHITE },
	{ "fsx", ICON_FSHARP,WHITE },

	{ "gem", ICON_ARCHIVE, YELLOW },
	{ "gif", ICON_IMG, GREEN },
	{ "gpg", ICON_LOCK, YELLOW },
	{ "go", ICON_GO, WHITE },
	{ "gz", ICON_ARCHIVE, YELLOW },
	{ "gzip", ICON_ARCHIVE, YELLOW },

	{ "h", ICON_C, BLUE },
	{ "hh", ICON_CPP, WHITE },
	{ "htaccess", ICON_CONF, WHITE },
	{ "htpasswd", ICON_CONF, WHITE },
	{ "htm", ICON_HTML, WHITE },
	{ "html", ICON_HTML, WHITE },
	{ "hxx", ICON_CPP, WHITE },

	{ "ico", ICON_IMG, GREEN },
	{ "img", ICON_IMG, GREEN },
	{ "ini", ICON_CONF, WHITE },
	{ "iso", ICON_CD, B_WHITE },

	{ "jar", ICON_JAVA, WHITE },
	{ "java", ICON_JAVA, WHITE },
	{ "jpeg", ICON_IMG, GREEN },
	{ "jpg", ICON_IMG, GREEN },
	{ "js", ICON_JAVASCRIPT, WHITE },
	{ "json", ICON_JAVASCRIPT, WHITE },

	{ "lha", ICON_ARCHIVE, YELLOW },
	{ "log", ICON_TXT, WHITE },
	{ "lua", ICON_LUA, WHITE },
	{ "lzh", ICON_ARCHIVE, YELLOW },
	{ "lzma", ICON_ARCHIVE, YELLOW },

	{ "m4a", ICON_AUDIO, YELLOW },
	{ "m4v", ICON_VID, BLUE },
	{ "markdown", ICON_MARKDOWN, WHITE },
	{ "md", ICON_MARKDOWN, WHITE },
	{ "mk", ICON_MAKEFILE, WHITE },
	{ "mkv", ICON_VID, BLUE },
	{ "mov", ICON_VID, BLUE },
	{ "mp3", ICON_AUDIO, YELLOW },
	{ "mp4", ICON_VID, BLUE },
	{ "mpeg", ICON_VID, BLUE },
	{ "mpg", ICON_VID, BLUE },
	{ "msi", ICON_WINDOWS, BLUE },

	{ "o", ICON_MANPAGE, WHITE },
	{ "ogg", ICON_AUDIO, YELLOW },
	{ "opdownload", ICON_DOWNLOADS, WHITE },
	{ "out", ICON_ELF, WHITE },

	{ "part", ICON_DOWNLOADS, WHITE },
	{ "patch", ICON_PATCH, WHITE },
	{ "pdf", ICON_PDF, RED },
	{ "php", ICON_PHP, WHITE },
	{ "png", ICON_IMG, GREEN },
	{ "ppt", ICON_POWERPOINT, WHITE },
	{ "pptx", ICON_POWERPOINT, WHITE },
	{ "psv", ICON_PHOTOSHOP, WHITE },
	{ "psd", ICON_PHOTOSHOP, WHITE },
	{ "py", ICON_PYTHON, GREEN },
	{ "pyc", ICON_PYTHON, GREEN },
	{ "pyd", ICON_PYTHON, GREEN },
	{ "pyo", ICON_PYTHON, GREEN },

	{ "rar", ICON_ARCHIVE, YELLOW },
	{ "rc", ICON_CONF, WHITE },
	{ "rpm", ICON_ARCHIVE, YELLOW },
	{ "rtf", ICON_PDF, RED },

	{ "so" , ICON_MANPAGE, WHITE },
	{ "scala", ICON_SCALA, WHITE },
	{ "sh", ICON_SCRIPT, WHITE },
	{ "slim", ICON_CODE, WHITE },
	{ "sql", ICON_MYSQL, WHITE },
	{ "svg", ICON_IMG, GREEN },

	{ "tar", ICON_ARCHIVE, YELLOW },
	{ "tbz2", ICON_ARCHIVE, YELLOW },
	{ "tgz", ICON_ARCHIVE, YELLOW },
	{ "txt", ICON_TXT, WHITE },
	{ "txz", ICON_ARCHIVE, YELLOW },

	{ "vid", ICON_VID, BLUE },
	{ "vim", ICON_VIM, WHITE },
	{ "vimrc" , ICON_VIM, WHITE },

	{ "wav", ICON_AUDIO, YELLOW },
	{ "webm", ICON_VID, BLUE },
	{ "wma", ICON_AUDIO, YELLOW },
	{ "wmv", ICON_VID, BLUE },

	{ "xbps", ICON_ARCHIVE, YELLOW },
	{ "xthml", ICON_HTML, WHITE },
	{ "xls", ICON_EXCEL, GREEN },
	{ "xlsx", ICON_EXCEL, GREEN },
	{ "xml", ICON_CODE, WHITE },
	{ "xz", ICON_ARCHIVE, RED },

	{ "yaml", ICON_CONF, WHITE },
	{ "yml", ICON_CONF, WHITE },

	{ "zip", ICON_ARCHIVE, YELLOW },
	{ "zst", ICON_ARCHIVE, YELLOW },
};

/* Icons for some specific dir names */
struct icons_t icon_dirnames[] = {
	{ ".git", ICON_DOTGIT, DEF_DIR_ICON_COLOR },
	{ "Desktop", ICON_DESKTOP, DEF_DIR_ICON_COLOR },
	{ "Documents", ICON_DOCUMETS, DEF_DIR_ICON_COLOR },
	{ "Downloads", ICON_DOWNLOADS, DEF_DIR_ICON_COLOR },
	{ "Music", ICON_MUSIC, DEF_DIR_ICON_COLOR },
	{ "Public", ICON_PUBLIC, DEF_DIR_ICON_COLOR },
	{ "Pictures", ICON_PICTURES, DEF_DIR_ICON_COLOR },
	{ "Templates", ICON_TEMPLATES, DEF_DIR_ICON_COLOR },
	{ "Videos", ICON_VIDEOS, DEF_DIR_ICON_COLOR },
};

/* Icons for some specific file names */
struct icons_t icon_filenames[] = {
	{ "CHANGELOG", ICON_HISTORY, DEF_FILE_ICON_COLOR },
	{ "configure", ICON_CONFIGURE, DEF_FILE_ICON_COLOR },
	{ "License", ICON_COPYRIGHT, DEF_FILE_ICON_COLOR },
	{ "Makefile", ICON_MAKEFILE, DEF_FILE_ICON_COLOR },
	{ "PKGBUILD", ICON_ARCH, CYAN },
};
