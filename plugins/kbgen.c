/* KBGEN: An escape sequences generator for CliFM keybindings */

/* Original code from: https://cboard.cprogramming.com/c-programming/157438-capturing-keyboard-input-one-character-time.html*/

/* Compile with -lncurses */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <curses.h>
#include <locale.h>
#include <ctype.h>

void help(void)
{
	/* Use (left)Alt-/ as example: \x1b\x2f */
	printf("Coming soon!\n");

	return;
}

int main(int argc, char **argv)
{
	if (argv[1] && strcmp(argv[1], "--help") == 0) {
		help();
		return EXIT_SUCCESS;
	}

	WINDOW *w;
	int c = 0;

	/* This is optional. This tells the C libraries to use user's locale settings. */
	setlocale(LC_ALL, "");

	/* Initialize curses. */
	w = initscr();

	if (w == NULL) {
		fprintf(stderr, "Error initializing curses library.\n");
		return EXIT_FAILURE;
	}

	raw();              /* Terminal in raw mode, no buffering */
	noecho();           /* Disable echoing */
	nonl();             /* Disable newline/return mapping */
	keypad(w, FALSE);   /* FALSE: CSI codes, TRUE: curses codes for keys */
	scrollok(w, 1);		/* Enable screen scroll */

	printw("(Press Shift-q to quit or Shift-w to clear the screen)\n"
		   "Hex  | Oct\n"
		   "---- | ----\n");
 
	while (c != 'Q') {

		c = getch();

		if (c == 'W') {
		clear();
		refresh();
		printw("(Press Shift-q to quit or Shift-w to clear the screen)\n"
			   "Hex  | Oct\n"
			   "---- | ----\n");
		continue;
		}

		printw("\\x%02x | \\%03o\n", c, c);
	}

	/* Done */
	endwin();

	return EXIT_SUCCESS;
}
