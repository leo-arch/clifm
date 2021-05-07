#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <wchar.h>

#include "helpers.h"
#include "xfunctions.h"
#include "globals.h"

char * savestring(const char *restrict str, size_t size) {
	if (!str)
		return (char *)NULL;

	char *ptr = (char *)NULL;
	ptr = (char *)xnmalloc(size + 1, sizeof(char));
	strcpy(ptr, str);

	return ptr;
}

static void log_msg(char *_msg, int print) {
/* Handle the error message 'msg'. Store 'msg' in an array of error
 * messages, write it into an error log file, and print it immediately
 * (if print is zero (NOPRINT_PROMPT) or tell the next prompt, if print
 * is one to do it (PRINT_PROMPT)). Messages wrote to the error log file
 * have the following format:
 * "[date] msg", where 'date' is YYYY-MM-DDTHH:MM:SS */
	if (!_msg)
		return;

	size_t msg_len = strlen(_msg);
	if (msg_len == 0)
		return;

	/* Store messages (for current session only) in an array, so that
	 * the user can check them via the 'msg' command */
	msgs_n++;
	messages = (char **)xrealloc(messages, (size_t)(msgs_n + 1)
								 * sizeof(char *));
	messages[msgs_n - 1] = savestring(_msg, msg_len);
	messages[msgs_n] = (char *)NULL;

	if (print) /* PRINT_PROMPT */
		/* The next prompt will take care of printing the message */
		print_msg = 1;
	else /* NOPRINT_PROMPT */
		/* Print the message directly here */
		fputs(_msg, stderr);

	/* If the config dir cannot be found or if msg log file isn't set
	 * yet... This will happen if an error occurs before running
	 * init_config(), for example, if the user's home cannot be found */
	if (!config_ok || !MSG_LOG_FILE || !*MSG_LOG_FILE)
		return;

	FILE *msg_fp = fopen(MSG_LOG_FILE, "a");

	if (!msg_fp) {
		/* Do not log this error: We might incur in an infinite loop
		 * trying to access a file that cannot be accessed */
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, MSG_LOG_FILE,
				strerror(errno));
		fputs("Press any key to continue... ", stdout);
		xgetchar();
		putchar('\n');
	}

	else {
		/* Write message to messages file: [date] msg */
		time_t rawtime = time(NULL);
		struct tm *tm = localtime(&rawtime);
		char date[64] = "";

		strftime(date, sizeof(date), "%b %d %H:%M:%S %Y", tm);
		fprintf(msg_fp, "[%d-%d-%dT%d:%d:%d] ", tm->tm_year + 1900,
				tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min,
				tm->tm_sec);
		fputs(_msg, msg_fp);
		fclose(msg_fp);
	}
}

int _err(int msg_type, int prompt, const char *format, ...) {
/* Custom POSIX implementation of GNU asprintf() modified to log program
 * messages. MSG_TYPE is one of: 'e', 'w', 'n', or zero (meaning this
 * latter that no message mark (E, W, or N) will be added to the prompt).
 * PROMPT tells whether to print the message immediately before the
 * prompt or rather in place. Based on littlstar's xasprintf
 * implementation:
 * https://github.com/littlstar/asprintf.c/blob/master/asprintf.c*/
	va_list arglist, tmp_list;
	int size = 0;

	va_start(arglist, format);
	va_copy(tmp_list, arglist);
	size = vsnprintf((char *)NULL, 0, format, tmp_list);
	va_end(tmp_list);

	if (size < 0) {
		va_end(arglist);
		return EXIT_FAILURE;
	}

	char *buf = (char *)xcalloc((size_t)size + 1, sizeof(char));

	vsprintf(buf, format, arglist);
	va_end(arglist);

	/* If the new message is the same as the last message, skip it */
	if (msgs_n && strcmp(messages[msgs_n - 1], buf) == 0) {
		free(buf);
		return EXIT_SUCCESS;
	}

	if (buf) {
		if (msg_type) {
			switch (msg_type) {
			case 'e': pmsg = error; break;
			case 'w': pmsg = warning; break;
			case 'n': pmsg = notice; break;
			default: pmsg = nomsg;
			}
		}

		log_msg(buf, (prompt) ? PRINT_PROMPT : NOPRINT_PROMPT);
		free(buf);

		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}
