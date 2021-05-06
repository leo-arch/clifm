######################
# Makefile for CliFM #
######################

OS := $(shell uname -s)

SHELL = /bin/sh
PREFIX = /usr/bin
PROG = clifm

CFLAGS = -O3 -s -fstack-protector-strong -march=native -Wall
LIBS_Linux = -lreadline -lacl -lcap
LIBS_FreeBSD = -lreadline -lintl

%.o: %.c
	cc -c $(CFLAGS) $<

build: clifm.o
	@printf "Detected operating system: ";
	@echo $(OS)
	cc -o $(PROG) $^ ${LIBS_${OS}}

install:
	@install -Dm755 -- "${PROG}" "${PREFIX}"/
	@rm -- ${PROG}
	@mkdir -p /usr/share/man/man1
	@install -g 0 -o 0 -Dm644 manpage /usr/share/man/man1/"${PROG}".1
	@gzip /usr/share/man/man1/"${PROG}".1
	@mkdir -p /usr/share/bash-completion/completions
	@install -g 0 -o 0 -Dm644 completions.bash /usr/share/bash-completion/completions/"${PROG}"
	@mkdir -p /usr/share/zsh/site-functions
	@install -g 0 -o 0 -Dm644 completions.bash /usr/share/zsh/site-functions/"_${PROG}"
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
