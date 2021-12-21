#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <curses.h>
#include <locale.h>
#include <ctype.h>

/* Compile with -lncurses */

void help(void)
{
	puts("Usage: kbgen [--help]");
	puts("Produce a representation, in hexadecimal, octal, and as "
	"character (or symbol, if non-printable) of keyboard presses, either "
	"for single keys or key combinations. Once in the program, press 'Q' "
	"to quit and 'W' to clear the screen.");
	puts("\nNOTE: Since 'Q' and 'W' are used to control the program flow, "
		 "there will be no representation for them. In case of need, "
		 "however, the values for these keys are:\n"
		 "  Hex  | Oct  | Symbol\n"
		 "  ---- | ---- | ------\n"
		 "  \\x51 | \\121 | Q\n"
		 "  \\x57 | \\127 | W");
	return;
}

int main(int argc, char **argv)
{
	if (argv[1] && strcmp(argv[1], "--help") == 0) {
		help();
		return EXIT_SUCCESS;
	}

	WINDOW *w;
	int c = 0, d = 0;

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
		   "Hex  | Oct  | Symbol\n"
		   "---- | ---- | ------\n");

	while (c != 'Q') {

		c = getch();

		if (c == 'W') {
			clear();
			refresh();
			printw("(Press Shift-q to quit or Shift-w to clear the "
				   "screen)\n"
				   "Hex  | Oct  | Symbol\n"
				   "---- | ---- | ------\n");
			continue;
		}

		if (isprint(c)) {
			printw("\\x%02x | \\%03o | %c\n", c, c, c);
			continue;
		}

		switch (c) {
			case 0:
				printw("\\x%02x | \\%03o | NUL\n", c, c); break;
			case 1:
				printw("\\x%02x | \\%03o | SOH\n", c, c); break;
			case 2:
				printw("\\x%02x | \\%03o | STX\n", c, c); break;
			case 3:
				printw("\\x%02x | \\%03o | ETX\n", c, c); break;
			case 4:
				printw("\\x%02x | \\%03o | EOT\n", c, c); break;
			case 5:
				printw("\\x%02x | \\%03o | ENQ\n", c, c); break;
			case 6:
				printw("\\x%02x | \\%03o | ACK\n", c, c); break;
			case 7:
				printw("\\x%02x | \\%03o | BEL\n", c, c); break;
			case 8:
				printw("\\x%02x | \\%03o | BS\n", c, c); break;
			case 9:
				printw("\\x%02x | \\%03o | HT\n", c, c); break;
			case 10:
				printw("\\x%02x | \\%03o | LF\n", c, c); break;
			case 11:
				printw("\\x%02x | \\%03o | CT\n", c, c); break;
			case 12:
				printw("\\x%02x | \\%03o | FF\n", c, c); break;
			case 13:
				printw("\\x%02x | \\%03o | CR\n", c, c); break;
			case 14:
				printw("\\x%02x | \\%03o | SO\n", c, c); break;
			case 15:
				printw("\\x%02x | \\%03o | SI\n", c, c); break;
			case 16:
				printw("\\x%02x | \\%03o | DLE\n", c, c); break;
			case 17:
				printw("\\x%02x | \\%03o | DC1\n", c, c); break;
			case 18:
				printw("\\x%02x | \\%03o | DC2\n", c, c); break;
			case 19:
				printw("\\x%02x | \\%03o | DC3\n", c, c); break;
			case 20:
				printw("\\x%02x | \\%03o | DC4\n", c, c); break;
			case 21:
				printw("\\x%02x | \\%03o | NAK\n", c, c); break;
			case 22:
				printw("\\x%02x | \\%03o | SYN\n", c, c); break;
			case 23:
				printw("\\x%02x | \\%03o | ETB\n", c, c); break;
			case 24:
				printw("\\x%02x | \\%03o | CAN\n", c, c); break;
			case 25:
				printw("\\x%02x | \\%03o | EM\n", c, c); break;
			case 26:
				printw("\\x%02x | \\%03o | SUB\n", c, c); break;
			case 27:
				printw("\\x%02x | \\%03o | ESC (\\e)\n", c, c); break;
			case 28:
				printw("\\x%02x | \\%03o | FS\n", c, c); break;
			case 29:
				printw("\\x%02x | \\%03o | GS\n", c, c); break;
			case 30:
				printw("\\x%02x | \\%03o | RS\n", c, c); break;
			case 31:
				printw("\\x%02x | \\%03o | US\n", c, c); break;
			default: break;
		}
	}

	/* Done */
	endwin();

	return EXIT_SUCCESS;
}
