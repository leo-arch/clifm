######################
# Makefile for CliFM #
######################

SHELL = /bin/sh
PREFIX = /usr/bin
PROG = clifm
OBJS = clifm.c

CC = gcc
CFLAGS = -O3 -s -fstack-protector-strong -march=native -Wall
LIBS_LINUX = -lreadline -lacl -lcap
LIBS_FREEBSD = -lreadline -lintl

build:
	@printf "Checking operating system... "; \
	UNAME=$$(uname -s); \
	case $${UNAME} in \
		Linux) \
			printf "GNU/Linux\nCompiling $(PROG)...\n"; \
			printf "Running '$(CC) $(CFLAGS) -o $(PROG) $(OBJS) $(LIBS_LINUX)'... "; \
			$(CC) $(CFLAGS) -o $(PROG) $(OBJS) $(LIBS_LINUX); \
			printf "Done\n"; ;; \
		FreeBSD) \
			printf "FreeBSD\nCompiling $(PROG)...\n"; \
			printf "Running '$(CC) $(CFLAGS) -o $(PROG) $(OBJS) $(LIBS_FREEBSD)'... "; \
			$(CC) $(CFLAGS) -o $(PROG) $(OBJS) $(LIBS_FREEBSD); \
			printf "Done\n"; ;; \
		*) \
			printf "\n'$${UNAME}': Operating system not supported\n" >&2; ;; \
	esac

install:
	@install -Dm755 -- "${PROG}" "${PREFIX}"/
	@rm -- ${PROG}
	@mkdir -p /usr/share/man/man1
	@install -g 0 -o 0 -Dm644 manpage /usr/share/man/man1/"${PROG}".1
	@gzip /usr/share/man/man1/"${PROG}".1
	@mkdir -p /usr/share/bash-completion/completions
	@install -g 0 -o 0 -Dm644 completions.bash /usr/share/bash-completion/completions/"${PROG}"
	@mkdir -p /usr/share/locale/es/LC_MESSAGES
	@install -g 0 -o 0 -Dm644 translations/spanish/"${PROG}".mo \
	/usr/share/locale/es/LC_MESSAGES/"${PROG}".mo
	@mkdir -p /usr/share/${PROG}
	@cp -r plugins /usr/share/${PROG}
	@cp -r functions /usr/share/${PROG}
	@printf "Successfully installed ${PROG}\n"

uninstall:
	@rm -- "${PREFIX}/${PROG}"
	@rm /usr/share/man/man1/"${PROG}".1.gz
	@rm /usr/share/locale/*/LC_MESSAGES/"${PROG}".mo
	@rm -r /usr/share/${PROG}
	@printf "Successfully uninstalled ${PROG}\n"
