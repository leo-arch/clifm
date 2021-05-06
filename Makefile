######################
# Makefile for CliFM #
######################

OS := $(shell uname -s)

SHELL = /bin/sh
PREFIX = /usr/bin
PROG = clifm

build:
	cd src && $(MAKE) build

install: build
	@install -Dm755 -- "${PROG}" "${PREFIX}"/
	@rm -- ${PROG}
	@mkdir -p /usr/share/man/man1
	@install -g 0 -o 0 -Dm644 manpage /usr/share/man/man1/"${PROG}".1
	@gzip /usr/share/man/man1/"${PROG}".1
	@mkdir -p /usr/share/{bash-completion/completions,zsh/site-functions}
	@install -g 0 -o 0 -Dm644 completions.bash /usr/share/bash-completion/completions/"${PROG}"
	@install -g 0 -o 0 -Dm644 completions.zsh /usr/share/zsh/site-functions/_"${PROG}"
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
	@rm /usr/share/bash-completion/completions/"${PROG}"
	@rm /usr/share/zsh/site-functions/_"${PROG}"
	@rm -r /usr/share/${PROG}
	@printf "Successfully uninstalled ${PROG}\n"
