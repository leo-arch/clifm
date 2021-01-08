######################
# Makefile for CliFM #
######################

SHELL = /bin/sh
PREFIX = /usr/bin
PROG = clifm
OBJS = clifm.c

CC = gcc
CFLAGS_LINUX = -O3 -s -fstack-protector-strong -march=native -lreadline -lcap -lacl
CFLAGS_FREEBSD = -O3 -s -fstack-protector-strong -march=native -lreadline -lintl

build:
	@echo -n "Checking operating system... "
	@case $$(uname -s) in \
		Linux) \
			printf "GNU/Linux\nBuilding $(PROG)... "; \
			$(CC) $(CFLAGS_LINUX) -o $(PROG) $(OBJS) ; \
			printf "Done\n" ;; \
		FreeBSD) \
			printf "FreeBSD\nBuilding $(PROG)... "; \
			$(CC) $(CFLAGS_FREEBSD) -o $(PROG) $(OBJS) ; \
			printf "Done\n" ;; \
		*) \
			printf "\n'$(UNAME)': Operating system not supported" >&2 ;; \
	esac

install:
	@install -Dm755 ${PROG} ${PREFIX}/
	@rm "${PROG}"
	@mkdir -p /usr/share/man/man1 2>/dev/null
	@install -g 0 -o 0 -Dm644 manpage "/usr/share/man/man1/${PROG}.1"
	@gzip "/usr/share/man/man1/${PROG}.1"
	@mkdir -p /usr/share/locale/{es}/LC_MESSAGES 2>/dev/null
	@install -g 0 -o 0 -Dm644 translations/spanish/${PROG}.mo \
	"/usr/share/locale/es/LC_MESSAGES/${PROG}.mo"
	@echo "Successfully installed ${PROG}"

uninstall:
	@rm "${PREFIX}/${PROG}"
	@rm "/usr/share/man/man1/${PROG}.1.gz"
	@rm "/usr/share/locale/es/LC_MESSAGES/${PROG}.mo"
	@echo "Successfully uninstalled ${PROG}"
