/* readline.c -- functions to control the behaviour of readline,
 * specially completions. It also introduces the suggestions system
 * via my_rl_getc function */

/*
 * This file is part of CliFM
 * 
 * Copyright (C) 2016-2021, L. Abramovich <johndoe.arch@outlook.com>
 * All rights reserved.

 * CliFM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * CliFM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
*/

#include "helpers.h"

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/stat.h>
#endif
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#ifdef __OpenBSD__
#include <strings.h>
#endif
#include <unistd.h>
#include <errno.h>

#ifdef __OpenBSD__
typedef char *rl_cpvfunc_t;
#include <ereadline/readline/readline.h>
#include <ereadline/readline/history.h>
#else
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "aux.h"
#include "checks.h"
#include "keybinds.h"
#include "navigation.h"
#include "readline.h"
#include "suggestions.h"

int
initialize_readline(void)
{
	/* #### INITIALIZE READLINE (what a hard beast to tackle!!) #### */

	/* Set the name of the program using readline. Mostly used for
	 * conditional constructs in $HOME/.inputrc */
	rl_readline_name = argv_bk[0];

	/* Enable tab auto-completion for commands (in PATH) in case of
	  * first entered string (if autocd and/or auto-open are enabled, check
	  * for paths as well). The second and later entered strings will
	  * be autocompleted with paths instead, just like in Bash, or with
	  * listed file names, in case of ELN's. I use a custom completion
	  * function to add command and ELN completion, since readline's
	  * internal completer only performs path completion */

	/* Define a function for path completion.
	 * NULL means to use filename_entry_function (), the default
	 * filename completer. */
	rl_completion_entry_function = my_rl_path_completion;

	/* Pointer to alternative function to create matches.
	 * Function is called with TEXT, START, and END.
	 * START and END are indices in RL_LINE_BUFFER saying what the
	 * boundaries of TEXT are.
	 * If this function exists and returns NULL then call the value of
	 * rl_completion_entry_function to try to match, otherwise use the
	 * array of strings returned. */
	rl_attempted_completion_function = my_rl_completion;
	rl_ignore_completion_duplicates = 1;

	/* I'm using here a custom quoting function. If not specified,
	 * readline uses the default internal function. */
	rl_filename_quoting_function = my_rl_quote;

	/* Tell readline what char to use for quoting. This is only the
	 * readline internal quoting function, and for custom ones, like the
	 * one I use above. However, custom quoting functions, though they
	 * need to define their own quoting chars, won't be called at all
	 * if this variable isn't set. */
	rl_completer_quote_characters = "\"'";
	rl_completer_word_break_characters = " ";

	/* Whenever readline finds any of the following chars, it will call
	 * the quoting function */
	rl_filename_quote_characters = " \t\n\"\\'`@$><=,;|&{[()]}?!*^";
	/* According to readline documentation, the following string is
	 * the default and the one used by Bash: " \t\n\"\\'`@$><=;|&{(" */

	/* Executed immediately before calling the completer function, it
	 * tells readline if a space char, which is a word break character
	 * (see the above rl_completer_word_break_characters variable) is
	 * quoted or not. If it is, readline then passes the whole string
	 * to the completer function (ex: "user\ file"), and if not, only
	 * wathever it found after the space char (ex: "file")
	 * Thanks to George Brocklehurst for pointing out this function:
	 * https://thoughtbot.com/blog/tab-completion-in-gnu-readline*/
	rl_char_is_quoted_p = quote_detector;

	if (suggestions)
		rl_getc_function = my_rl_getc;

	/* This function is executed inmediately before path completion. So,
	 * if the string to be completed is, for instance, "user\ file" (see
	 * the above comment), this function should return the dequoted
	 * string so it won't conflict with system file names: you want
	 * "user file", because "user\ file" does not exist, and, in this
	 * latter case, readline won't find any matches */
	rl_filename_dequoting_function = dequote_str;

	/* Initialize the keyboard bindings function */
	readline_kbinds();

	/* Copy the list of quote chars to a global variable to be used
	 * later by some of the program functions like split_str(),
	 * my_rl_quote(), is_quote_char(), and my_rl_dequote() */
	qc = savestring(rl_filename_quote_characters,
	    strlen(rl_filename_quote_characters));

	return EXIT_SUCCESS;
}

/* This function is automatically called by readline() to handle input.
 * Taken from Bash 1.14.7 and modified to fit our needs. Used
 * to introduce the suggestions system */
int
my_rl_getc(FILE *stream)
{
	int result;
	unsigned char c;

#if defined(__GO32__)
	if (isatty(0))
		return (getkey() & 0x7F);
#endif /* __GO32__ */
	while(1) {
		result = read(fileno(stream), &c, sizeof(unsigned char));
		if (result == sizeof(unsigned char)) {
			if (suggestions) {
				/* rl_suggestions returns -1 is C was insetrted before
				 * the end of the current line, in which case we don't
				 * want to return it here (otherwise, it would be added
				 * to rl_line_buffer) */
				if (rl_suggestions(c) == -1) {
					rl_redisplay();
					continue;
				}
			}
			return (c);
		}

		/* If zero characters are returned, then the file that we are
		reading from is empty!  Return EOF in that case. */
		if (result == 0)
			return (EOF);

#if defined(EWOULDBLOCK)
		if (errno == EWOULDBLOCK) {
			int xflags;

			if ((xflags = fcntl(fileno(stream), F_GETFL, 0)) < 0)
				return (EOF);
			if (xflags & O_NDELAY) {
				xflags &= ~O_NDELAY;
				fcntl(fileno(stream), F_SETFL, flags);
				continue;
			}
			continue;
		}
#endif /* EWOULDBLOCK */

#if defined(_POSIX_VERSION) && defined(EAGAIN) && defined(O_NONBLOCK)
		if (errno == EAGAIN) {
			int xflags;

			if ((xflags = fcntl(fileno(stream), F_GETFL, 0)) < 0)
				return (EOF);
			if (xflags & O_NONBLOCK) {
				xflags &= ~O_NONBLOCK;
				fcntl(fileno(stream), F_SETFL, flags);
				continue;
			}
		}
#endif /* _POSIX_VERSION && EAGAIN && O_NONBLOCK */

#if !defined(__GO32__)
      /* If the error that we received was SIGINT, then try again,
	 this is simply an interrupted system call to read ().
	 Otherwise, some error ocurred, also signifying EOF. */
		if (errno != EINTR)
			return (EOF);
#endif /* !__GO32__ */
	}
}

/* Simply check a single chartacter (c) against the quoting characters
 * list defined in the qc global array (which takes its values from
 * rl_filename_quote_characters */
int
is_quote_char(const char c)
{
	if (c == '\0' || !qc)
		return -1;

	char *p = qc;

	while (*p) {
		if (c == *(p++))
			return 1;
	}

	return 0;
}

char *
rl_no_hist(const char *prompt)
{
	int bk = suggestions;
	suggestions = 0;
	stifle_history(0); /* Prevent readline from using the history
	setting */
	char *input = readline(prompt);
	unstifle_history();	 /* Reenable history */
	read_history(HIST_FILE); /* Reload history lines from file */
	suggestions = bk;

	if (input) {

		/* Make sure input isn't empty string */
		if (!*input) {
			free(input);
			return (char *)NULL;
		}

		/* Check we have some non-blank char */
		int no_blank = 0;
		char *p = input;

		while (*p) {
			if (*p != ' ' && *p != '\n' && *p != '\t') {
				no_blank = 1;
				break;
			}
			p++;
		}

		if (!no_blank) {
			free(input);
			return (char *)NULL;
		}

		return input;
	}

	return (char *)NULL;
}

/* Used by readline to check if a char in the string being completed is
 * quoted or not */
int
quote_detector(char *line, int index)
{
	if (index > 0 && line[index - 1] == '\\' && !quote_detector(line, index - 1))
		return 1;

	return 0;
}

/* Performs bash-style filename quoting for readline (put a backslash
 * before any char listed in rl_filename_quote_characters.
 * Modified version of:
 * https://utcc.utoronto.ca/~cks/space/blog/programming/ReadlineQuotingExample*/
char *
my_rl_quote(char *text, int mt, char *qp)
{
	/* NOTE: mt and qp arguments are not used here, but are required by
	 * rl_filename_quoting_function */

	/*
	 * How it works: P and R are pointers to the same memory location
	 * initialized (calloced) twice as big as the line that needs to be
	 * quoted (in case all chars in the line need to be quoted); TP is a
	 * pointer to TEXT, which contains the string to be quoted. We move
	 * through TP to find all chars that need to be quoted ("a's" becomes
	 * "a\'s", for example). At this point we cannot return P, since this
	 * pointer is at the end of the string, so that we return R instead,
	 * which is at the beginning of the same string pointed to by P.
	 * */
	char *r = (char *)NULL, *p = (char *)NULL, *tp = (char *)NULL;

	size_t text_len = strlen(text);
	/* Worst case: every character of text needs to be escaped. In this
	 * case we need 2x text's bytes plus the NULL byte. */
	p = (char *)xnmalloc((text_len * 2) + 1, sizeof(char));
	r = p;

	if (r == NULL)
		return (char *)NULL;

	/* Escape whatever char that needs to be escaped */
	for (tp = text; *tp; tp++) {

		if (is_quote_char(*tp))
			*p++ = '\\';

		*p++ = *tp;
	}

	/* Add a final null byte to the string */
	*p = '\0';

	return r;
}

/* This is the filename_completion_function() function of an old Bash
 * release (1.14.7) modified to fit CliFM needs */
char *
my_rl_path_completion(const char *text, int state)
{
	if (!text || !*text)
		return (char *)NULL;
	/* state is zero before completion, and 1 ... n after getting
	 * possible completions. Example:
	 * cd Do[TAB] -> state 0
	 * cuments/ -> state 1
	 * wnloads/ -> state 2
	 * */

	/* Dequote string to be completed (text), if necessary */
	static char *tmp_text = (char *)NULL;

	if (strchr(text, '\\')) {
		char *p = savestring(text, strlen(text));

		tmp_text = dequote_str(p, 0);

		free(p);
		p = (char *)NULL;

		if (!tmp_text)
			return (char *)NULL;
	}

	if (*text == '.' && text[1] == '.' && text[2] == '.') {

		char *p = savestring(text, strlen(text));
		tmp_text = fastback(p);

		free(p);
		p = (char *)NULL;

		if (!tmp_text)
			return (char *)NULL;
	}

	/* Perhaps I should add bookmarks here */

	int rl_complete_with_tilde_expansion = 0;
	/* ~/Doc -> /home/user/Doc */

	static DIR *directory;
	static char *filename = (char *)NULL;
	static char *dirname = (char *)NULL;
	static char *users_dirname = (char *)NULL;
	static size_t filename_len;
	static int match, ret;
	struct dirent *ent = (struct dirent *)NULL;
	static int exec = 0, exec_path = 0;
	static char *dir_tmp = (char *)NULL;
	static char tmp[PATH_MAX] = "";

	/* If we don't have any state, then do some initialization. */
	if (!state) {
		char *temp;

		if (dirname)
			free(dirname);
		if (filename)
			free(filename);
		if (users_dirname)
			free(users_dirname);

		/* tmp_text is true whenever text was dequoted */
		size_t text_len = strlen((tmp_text) ? tmp_text : text);
		if (text_len)
			filename = savestring((tmp_text) ? tmp_text : text, text_len);
		else
			filename = savestring("", 1);

		if (!*text)
			text = ".";

		if (text_len)
			dirname = savestring((tmp_text) ? tmp_text : text, text_len);
		else
			dirname = savestring("", 1);

		if (dirname[0] == '.' && dirname[1] == '/')
			exec = 1;
		else
			exec = 0;

		/* Get everything after last slash */
		temp = strrchr(dirname, '/');

		if (temp) {
			strcpy(filename, ++temp);
			*temp = '\0';
		} else {
			strcpy(dirname, ".");
		}

		/* We aren't done yet.  We also support the "~user" syntax. */

		/* Save the version of the directory that the user typed. */
		size_t dirname_len = strlen(dirname);

		users_dirname = savestring(dirname, dirname_len);
		/*      { */
		char *temp_dirname;
		int replace_dirname;

		temp_dirname = tilde_expand(dirname);
		free(dirname);
		dirname = temp_dirname;

		replace_dirname = 0;

		if (rl_directory_completion_hook)
			replace_dirname = (*rl_directory_completion_hook)(&dirname);

		if (replace_dirname) {
			free(users_dirname);
			users_dirname = savestring(dirname, dirname_len);
		}
		/*      } */
		directory = opendir(dirname);
		filename_len = strlen(filename);

		rl_filename_completion_desired = 1;
	}

	if (tmp_text) {
		free(tmp_text);
		tmp_text = (char *)NULL;
	}

	/* Now that we have some state, we can read the directory. If we found
	 * a match among files in dir, break the loop and print the match */

	match = 0;

	size_t dirname_len = 0;
	if (dirname)
		dirname_len = strlen(dirname);

	/* This block is used only in case of "/path/./" to remove the
	 * ending "./" from dirname and to be able to perform thus the
	 * executable check via access() */
	exec_path = 0;

	if (dirname_len > 2) {

		if (dirname[dirname_len - 3] == '/' && dirname[dirname_len - 2] == '.'
		&& dirname[dirname_len - 1] == '/') {
			dir_tmp = savestring(dirname, dirname_len);

			if (dir_tmp) {
				dir_tmp[dirname_len - 2] = '\0';
				exec_path = 1;
			}
		}
	}

	/* ############### COMPLETION FILTER ################## */
	/* #        This is the heart of the function         #
	 * #################################################### */
	mode_t type;

	while (directory && (ent = readdir(directory))) {

#if !defined(_DIRENT_HAVE_D_TYPE)
		struct stat attr;
		if (!dirname || (*dirname == '.' && !*(dirname + 1)))
			strncpy(tmp, ent->d_name, PATH_MAX - 1);
		else
			snprintf(tmp, PATH_MAX - 1, "%s%s", dirname, ent->d_name);
		if (lstat(tmp, &attr) == -1) {
			continue;
		}

		switch (attr.st_mode & S_IFMT) {
		case S_IFBLK: type = DT_BLK; break;
		case S_IFCHR: type = DT_CHR; break;
		case S_IFDIR: type = DT_DIR; break;
		case S_IFIFO: type = DT_FIFO; break;
		case S_IFLNK: type = DT_LNK; break;
		case S_IFREG: type = DT_REG; break;
		case S_IFSOCK: type = DT_SOCK; break;
		default: type = DT_UNKNOWN; break;
		}
#else
		type = ent->d_type;
#endif

		/* If the user entered nothing before TAB (ex: "cd [TAB]") */
		if (!filename_len) {

			/* Exclude "." and ".." as possible completions */
			if (SELFORPARENT(ent->d_name))
				continue;

			/* If 'cd', match only dirs or symlinks to dir */
			if (*rl_line_buffer == 'c'
			&& strncmp(rl_line_buffer, "cd ", 3) == 0) {
				ret = -1;

				switch (type) {
				case DT_LNK:
					if (dirname[0] == '.' && !dirname[1]) {
						ret = get_link_ref(ent->d_name);
					} else {
						snprintf(tmp, PATH_MAX, "%s%s", dirname,
						    ent->d_name);
						ret = get_link_ref(tmp);
					}

					if (ret == S_IFDIR)
						match = 1;

					break;

				case DT_DIR:
					match = 1;
					break;

				default:
					break;
				}
			}

			/* If 'open', allow only reg files, dirs, and symlinks */
			else if (*rl_line_buffer == 'o'
			&& (strncmp(rl_line_buffer, "o ", 2) == 0
			|| strncmp(rl_line_buffer, "open ", 5) == 0)) {
				ret = -1;

				switch (type) {
				case DT_LNK:

					if (dirname[0] == '.' && !dirname[1]) {
						ret = get_link_ref(ent->d_name);
					} else {
						snprintf(tmp, PATH_MAX, "%s%s", dirname,
						    ent->d_name);
						ret = get_link_ref(tmp);
					}

					if (ret == S_IFDIR || ret == S_IFREG)
						match = 1;

					break;

				case DT_REG: /* fallthrough */
				case DT_DIR:
					match = 1;
					break;

				default:
					break;
				}
			}

			/* If 'trash', allow only reg files, dirs, symlinks, pipes
			 * and sockets. You should not trash a block or a character
			 * device */
			else if (*rl_line_buffer == 't'
			&& (strncmp(rl_line_buffer, "t ", 2) == 0
			|| strncmp(rl_line_buffer, "tr ", 2) == 0
			|| strncmp(rl_line_buffer, "trash ", 6) == 0)) {

				if (type != DT_BLK && type != DT_CHR)
					match = 1;
			}

			/* If "./", list only executable regular files */
			else if (exec) {

				if (type == DT_REG && access(ent->d_name, X_OK) == 0)
					match = 1;
			}

			/* If "/path/./", list only executable regular files */
			else if (exec_path) {

				if (type == DT_REG) {
					/* dir_tmp is dirname less "./", already
					 * allocated before the while loop */
					snprintf(tmp, PATH_MAX, "%s%s", dir_tmp,
					    ent->d_name);

					if (access(tmp, X_OK) == 0)
						match = 1;
				}
			}

			/* No filter for everything else. Just print whatever is
			 * there */
			else
				match = 1;
		}

		/* If there is at least one char to complete (ex: "cd .[TAB]") */
		else {
			/* Check if possible completion match up to the length of
			 * filename. */
			if (case_sens_path_comp) {
				if (*ent->d_name != *filename
				|| (strncmp(filename, ent->d_name, filename_len) != 0))
					continue;
			} else {
				if (TOUPPER(*ent->d_name) != TOUPPER(*filename)
				|| (strncasecmp(filename, ent->d_name, filename_len) != 0))
					continue;
			}

			if (*rl_line_buffer == 'c'
			&& strncmp(rl_line_buffer, "cd ", 3) == 0) {
				ret = -1;

				switch (type) {
				case DT_LNK:
					if (dirname[0] == '.' && !dirname[1]) {
						ret = get_link_ref(ent->d_name);
					} else {
						snprintf(tmp, PATH_MAX, "%s%s", dirname,
						    ent->d_name);
						ret = get_link_ref(tmp);
					}

					if (ret == S_IFDIR)
						match = 1;
					break;

				case DT_DIR:
					match = 1;
					break;

				default: break;
				}
			}

			else if (*rl_line_buffer == 'o'
			&& (strncmp(rl_line_buffer, "o ", 2) == 0
			|| strncmp(rl_line_buffer, "open ", 5) == 0)) {
				ret = -1;

				switch (type) {
				case DT_REG: /* fallthrough */
				case DT_DIR:
					match = 1;
					break;

				case DT_LNK:
					if (dirname[0] == '.' && !dirname[1]) {
						ret = get_link_ref(ent->d_name);
					} else {
						snprintf(tmp, PATH_MAX, "%s%s", dirname,
						    ent->d_name);
						ret = get_link_ref(tmp);
					}

					if (ret == S_IFDIR || ret == S_IFREG)
						match = 1;
					break;

				default: break;
				}
			}

			else if (*rl_line_buffer == 't'
			&& (strncmp(rl_line_buffer, "t ", 2) == 0
			|| strncmp(rl_line_buffer, "tr ", 3) == 0
			|| strncmp(rl_line_buffer, "trash ", 6) == 0)) {
				if (type != DT_BLK && type != DT_CHR)
					match = 1;
			}

			else if (exec) {
				if (type == DT_REG && access(ent->d_name, X_OK) == 0)
					match = 1;
			}

			else if (exec_path) {
				if (type == DT_REG) {
					snprintf(tmp, PATH_MAX, "%s%s", dir_tmp, ent->d_name);
					if (access(tmp, X_OK) == 0)
						match = 1;
				}
			}

			else
				match = 1;
		}

		if (match)
			break;
	}

	if (dir_tmp) { /* == exec_path */
		free(dir_tmp);
		dir_tmp = (char *)NULL;
	}

	/* readdir() returns NULL on reaching the end of directory stream.
	 * So that if entry is NULL, we have no matches */

	if (!ent) { /* == !match */
		if (directory) {
			closedir(directory);
			directory = (DIR *)NULL;
		}

		if (dirname) {
			free(dirname);
			dirname = (char *)NULL;
		}

		if (filename) {
			free(filename);
			filename = (char *)NULL;
		}

		if (users_dirname) {
			free(users_dirname);
			users_dirname = (char *)NULL;
		}

		return (char *)NULL;
	}

	/* We have a match */
	else {
		char *temp = (char *)NULL;

		/* dirname && (strcmp(dirname, ".") != 0) */
		if (dirname && (dirname[0] != '.' || dirname[1])) {
			if (rl_complete_with_tilde_expansion && *users_dirname == '~') {
				size_t dirlen = strlen(dirname);
				temp = (char *)xcalloc(dirlen + strlen(ent->d_name) + 2,
															sizeof(char));
				strcpy(temp, dirname);
				/* Canonicalization cuts off any final slash present.
				 * We need to add it back. */

				if (dirname[dirlen - 1] != '/') {
					temp[dirlen] = '/';
					temp[dirlen + 1] = '\0';
				}
			} else {
				temp = (char *)xcalloc(strlen(users_dirname) +
						strlen(ent->d_name) + 1, sizeof(char));
				strcpy(temp, users_dirname);
			}
			strcat(temp, ent->d_name);
		}

		else
			temp = savestring(ent->d_name, strlen(ent->d_name));

		return (temp);
	}
}

/* Used by bookmarks completion */
char *
bookmarks_generator(const char *text, int state)
{
	static int i;
	static size_t len;
	char *name;

	if (!state) {
		i = 0;
		len = strlen(text);
	}

	/* Look for bookmarks in bookmark names for a match */
	while ((name = bookmark_names[i++]) != NULL) {
		if (strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

/* Used by history completion */
char *
hist_generator(const char *text, int state)
{
	static int i;
	static size_t len;
	char *name;

	if (!state) {
		i = 0;
		len = strlen(text);
	}

	/* Look for cmd history entries for a match */
	while ((name = history[i++]) != NULL) {
		if (strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

/* Expand string into matching path in the jump database. Used by
 * j, jc, and jp commands */
char *
jump_generator(const char *text, int state)
{
	static int i;
	char *name;

	if (!state)
		i = 0;

	if (!jump_db)
		return (char *)NULL;

	/* Look for matches in the dirhist list */
	while ((name = jump_db[i++].path) != NULL) {

		/* Exclude CWD */
		if (name[1] == ws[cur_ws].path[1] && strcmp(name, ws[cur_ws].path) == 0)
			continue;

		/* Filter by parent */
		if (rl_line_buffer[1] == 'p') {
			if (!strstr(ws[cur_ws].path, name))
				continue;
		}

		/* Filter by child */
		else if (rl_line_buffer[1] == 'c') {
			if (!strstr(name, ws[cur_ws].path))
				continue;
		}

		if (strstr(name, text))
			return strdup(name);
	}

	return (char *)NULL;
}

/* Expand jump order number into the corresponding path. Used by the
 * jo command */
char *
jump_entries_generator(const char *text, int state)
{
	static size_t i;
	char *name;

	if (!state)
		i = 0;

	int num_text = atoi(text);

	/* Check list of jump entries for a match */
	while (i <= jump_n && (name = jump_db[i++].path) != NULL)
		if (*name == *jump_db[num_text - 1].path && strcmp(name,
						jump_db[num_text - 1].path) == 0)
			return strdup(name);

	return (char *)NULL;
}

char *
cschemes_generator(const char *text, int state)
{
	static int i;
	static size_t len;
	char *name;

	if (!state) {
		i = 0;
		len = strlen(text);
	} /* The state variable is zero only the first time the function is
	called, and a non-zero positive in later calls. This means that i
	and len will be necessarilly initialized the first time */

	if (!color_schemes)
		return (char *)NULL;

	/* Look for color schemes in color_schemes for a match */
	while ((name = color_schemes[i++]) != NULL) {
		if (strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

/* Used by profiles completion */
char *
profiles_generator(const char *text, int state)
{
	static int i;
	static size_t len;
	char *name;

	if (!state) {
		i = 0;
		len = strlen(text);
	} /* The state variable is zero only the first time the function is
	called, and a non-zero positive in later calls. This means that i
	and len will be necessarilly initialized the first time */

	/* Look for profiles in profile_names for a match */
	while ((name = profile_names[i++]) != NULL) {
		if (strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

/* Used by ELN expansion */
char *
filenames_gen_text(const char *text, int state)
{
	static size_t i, len = 0;
	char *name;
	rl_filename_completion_desired = 1;
	/* According to the GNU readline documention: "If it is set to a
	 * non-zero value, directory names have a slash appended and
	 * Readline attempts to quote completed file names if they contain
	 * any embedded word break characters." To make the quoting part
	 * work I had to specify a custom quoting function (my_rl_quote) */
	if (!state) { /* state is zero only the first time readline is
	executed */
		i = 0;
		len = strlen(text);
	}

	/* Check list of currently displayed files for a match */
	while (i < files && (name = file_info[i++].name) != NULL)
		if (case_sens_path_comp ? strncmp(name, text, len) == 0
		: strncasecmp(name, text, len) == 0)
			return strdup(name);

	return (char *)NULL;
}

/* Used by ELN expansion */
char *
filenames_gen_eln(const char *text, int state)
{
	static size_t i;
	char *name;
	rl_filename_completion_desired = 1;

	if (!state)
		i = 0;

	int num_text = atoi(text);

	/* Check list of currently displayed files for a match */
	while (i < files && (name = file_info[i++].name) != NULL) {
		if (*name == *file_info[num_text - 1].name
		&& strcmp(name, file_info[num_text - 1].name) == 0) {
			if (suggestion_buf)
				clear_suggestion();
			return strdup(name);
		}
	}

	return (char *)NULL;
}

/* Used by commands completion */
char *
bin_cmd_generator(const char *text, int state)
{
	static int i;
	static size_t len;
	char *name;

	if (!state) {
		i = 0;
		len = strlen(text);
	}

	while ((name = bin_commands[i++]) != NULL) {
		if (*text == *name && strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

char *
sort_num_generator(const char *text, int state)
{
	static size_t i;
	char *name;
	rl_filename_completion_desired = 1;

	if (!state)
		i = 0;

	int num_text = atoi(text);

	static char *sorts[] = {
	    "none",
	    "name",
	    "size",
	    "atime",
	    "btime",
	    "ctime",
	    "mtime",
	    "version",
	    "extension",
	    "inode",
	    "owner",
	    "group",
	    NULL
	};

	/* Check list of currently displayed files for a match */
	while (i <= SORT_TYPES && (name = sorts[i++]) != NULL) {
		if (*name == *sorts[num_text]
		&& strcmp(name, sorts[num_text]) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

char *
nets_generator(const char *text, int state)
{
	static int i;
	static size_t len;
	char *name;

	if (!state) {
		i = 0;
		len = strlen(text);
	}

	while ((name = remotes[i++].name) != NULL) {
		if (strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

char *
sort_name_generator(const char *text, int state)
{
	static int i;
	static size_t len;
	char *name;

	if (!state) {
		i = 0;
		len = strlen(text);
	}

	static char *sorts[] = {
	    "none",
	    "name",
	    "size",
	    "atime",
	    "btime",
	    "ctime",
	    "mtime",
	    "version",
	    "extension",
	    "inode",
	    "owner",
	    "group",
	    NULL};

	while ((name = sorts[i++]) != NULL) {
		if (*text == *name && strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

/* Generate entries from the jump database (not using the j function)*/
/*char *
jump_gen(const char *text, int state)
{
	static int i;
	static size_t len;
	char *name;

	if (!state) {
		i = 0;
		len = strlen(text);
	}

	while ((name = jump_db[i++].path) != NULL) {
		if (case_sens_path_comp ? strncmp(name, text, len) == 0
		: strncasecmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
} */

char **
my_rl_completion(const char *text, int start, int end)
{
	char **matches = (char **)NULL;

	if (start == 0) { /* Only for the first word entered in the prompt */
		/* Commands completion */
		if (end == 0) { /* If text is empty, do nothing */
			/* Prevent readline from attempting path completion if
			* rl_completion matches returns NULL */
			rl_attempted_completion_over = 1;
			return (char **)NULL;
		}

		/* History cmd completion */
		if (*text == '!')
			matches = rl_completion_matches(text + 1, &hist_generator);

		/* If autocd or auto-open, try to expand ELN's first */
		if (!matches && (autocd || auto_open)) {
			if (*text >= '1' && *text <= '9') {
				int num_text = atoi(text);

				if (is_number(text) && num_text > 0 && num_text <= (int)files)
					matches = rl_completion_matches(text, &filenames_gen_eln);
			}

			/* Compĺete with files in CWD */
			if (!matches && *text != '/')
				matches = rl_completion_matches(text, &filenames_gen_text);

			/* Complete with entries in the jump database */
/*			if (autocd && !matches)
				matches = rl_completion_matches(text, &jump_gen); */
		}

		/* Bookmarks completion */
		if (!matches && (autocd || auto_open) && expand_bookmarks)
			matches = rl_completion_matches(text, &bookmarks_generator);

		/* If neither autocd nor auto-open, try to complete with
		 * command names */
		if (!matches)
			matches = rl_completion_matches(text, &bin_cmd_generator);
	}

	/* Second word or more */
	else {
		/* #### ELN AND JUMP ORDER EXPANSION ### */

		/* Perform this check only if the first char of the string to be
		 * completed is a number in order to prevent an unnecessary call
		 * to atoi */
		if (*text >= '0' && *text <= '9') {
			int num_text = atoi(text);

			/* Dirjump: jo command */
			if (*rl_line_buffer == 'j' && rl_line_buffer[1] == 'o'
			&& rl_line_buffer[2] == ' ') {
				if (is_number(text) && num_text > 0 && num_text <= (int)jump_n) {
					matches = rl_completion_matches(text,
					    &jump_entries_generator);
				}
			}

			/* Sort number expansion */
			else if (*rl_line_buffer == 's'
			&& (strncmp(rl_line_buffer, "st ", 3) == 0
			|| strncmp(rl_line_buffer, "sort ", 5) == 0)
			&& is_number(text) && num_text >= 0 && num_text <= SORT_TYPES)
				matches = rl_completion_matches(text, &sort_num_generator);

			/* ELN expansion */
			else if (is_number(text) && num_text > 0 && num_text <= (int)files)
				matches = rl_completion_matches(text, &filenames_gen_eln);
		}

		/* ### DIRJUMP COMPLETION ### */
		/* j, jc, jp commands */
		else if (*rl_line_buffer == 'j' && (rl_line_buffer[1] == ' '
		|| ((rl_line_buffer[1] == 'c' || rl_line_buffer[1] == 'p')
		&& rl_line_buffer[2] == ' ')
		|| strncmp(rl_line_buffer, "jump ", 5) == 0))
			matches = rl_completion_matches(text, &jump_generator);

		/* ### BOOKMARKS COMPLETION ### */

		else if (*rl_line_buffer == 'b' && (rl_line_buffer[1] == 'm'
		|| rl_line_buffer[1] == 'o')
		&& (strncmp(rl_line_buffer, "bm ", 3) == 0
		|| strncmp(rl_line_buffer, "bookmarks ", 10) == 0)) {
			if (suggestion.type != FILE_SUG)
				rl_attempted_completion_over = 1;
			matches = rl_completion_matches(text, &bookmarks_generator);
		}

		/* ### COLOR SCHEMES COMPLETION ### */
		else if (*rl_line_buffer == 'c' && ((rl_line_buffer[1] == 's'
		&& rl_line_buffer[2] == ' ')
		|| strncmp(rl_line_buffer, "colorschemes ", 13) == 0)) {
			matches = rl_completion_matches(text,
			    &cschemes_generator);
		}

		/* ### PROFILES COMPLETION ### */

		else if (*rl_line_buffer == 'p' && (rl_line_buffer[1] == 'r'
		|| rl_line_buffer[1] == 'f')
		&& (strncmp(rl_line_buffer, "pf set ", 7) == 0
		|| strncmp(rl_line_buffer, "profile set ", 12) == 0
		|| strncmp(rl_line_buffer, "pf del ", 7) == 0
		|| strncmp(rl_line_buffer, "profile del ", 12) == 0)) {
			if (suggestion.type != FILE_SUG)
				rl_attempted_completion_over = 1;
			matches = rl_completion_matches(text, &profiles_generator);
		}

		else if (expand_bookmarks) {
			matches = rl_completion_matches(text, &bookmarks_generator);
		}

		else if (*rl_line_buffer == 's'
		&& (strncmp(rl_line_buffer, "st ", 3) == 0
		|| strncmp(rl_line_buffer, "sort ", 5) == 0))
			matches = rl_completion_matches(text, &sort_name_generator);

		else if (*rl_line_buffer == 'n'
		&& strncmp(rl_line_buffer, "net ", 4) == 0)
			matches = rl_completion_matches(text, &nets_generator);
	}

	/* ### PATH COMPLETION ### */

	/* If none of the above, readline will attempt
	 * path completion instead via my custom my_rl_path_completion() */
	return matches;
}
