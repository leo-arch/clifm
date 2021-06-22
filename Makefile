######################
# Makefile for CliFM #
######################

OS := $(shell uname -s)

BIN ?= clifm

PREFIX ?= /usr/local
MANPREFIX ?= $(PREFIX)/share/man
DATADIR ?= $(PREFIX)/share
DESKTOPPREFIX ?= $(DATADIR)/applications
DESKTOPICONPREFIX ?= $(DATADIR)/icons/hicolor
PROG_DATADIR ?= $(DATADIR)/$(BIN)

SHELL ?= /bin/sh
INSTALL ?= install
RM ?= rm

SRCDIR = src
OBJS != ls $(SRCDIR)/*.c | sed "s/.c\$$/.o/g"

CFLAGS ?= -O3 -fstack-protector-strong -march=native -Wall
LIBS_Linux ?= -lreadline -lacl -lcap
LIBS_FreeBSD ?= -I/usr/local/include -L/usr/local/lib -lreadline -lintl

build: ${OBJS}
	@printf "Detected operating system: ";
	@echo "$(OS)"
	$(CC) -o $(BIN) ${OBJS} ${LIBS_${OS}} $(CFLAGS)

clean:
	$(RM) -- $(BIN)
	$(RM) -f -- $(SRCDIR)/*.o

install: build
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 0755 $(BIN) $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PROG_DATADIR)
	$(INSTALL) -m 0755 -d $(DESTDIR)$(MANPREFIX)/man1
	$(INSTALL) -m 0755 -d $(DESTDIR)$(DATADIR)/bash-completion/completions
	$(INSTALL) -m 0755 -d $(DESTDIR)$(DESKTOPPREFIX)
	$(INSTALL) -m 0755 -d $(DESTDIR)$(DATADIR)/zsh/site-functions
	$(INSTALL) -m 0755 -d $(DESTDIR)$(DESKTOPICONPREFIX)/scalable/apps
	$(INSTALL) -m 0755 -d $(DESTDIR)$(DATADIR)/locale/es/LC_MESSAGES
	$(INSTALL) -m 0644 misc/manpage $(DESTDIR)$(MANPREFIX)/man1/$(BIN).1
	gzip -- $(DESTDIR)$(MANPREFIX)/man1/$(BIN).1
	$(INSTALL) -m 0644 misc/completions.bash $(DESTDIR)$(DATADIR)/bash-completion/completions/$(BIN)
	$(INSTALL) -m 0644 misc/completions.zsh $(DESTDIR)$(DATADIR)/zsh/site-functions/_$(BIN)
	$(INSTALL) -m 0644 misc/$(BIN).desktop $(DESTDIR)$(DESKTOPPREFIX)
	$(INSTALL) -m 0644 misc/{mimelist.cfm,keybindings.cfm,clifmrc,actions.cfm} $(DESTDIR)$(PROG_DATADIR)
	$(INSTALL) -m 0644 images/logo/$(BIN).svg $(DESTDIR)$(DESKTOPICONPREFIX)/scalable/apps
	$(INSTALL) -m 0644 translations/spanish/$(BIN).mo $(DESTDIR)$(DATADIR)/locale/es/LC_MESSAGES/$(BIN).mo
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PROG_DATADIR)/plugins
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PROG_DATADIR)/functions
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PROG_DATADIR)/colors
	$(INSTALL) -m 0755 plugins/* $(DESTDIR)$(PROG_DATADIR)/plugins
	chmod 644 -- $(DESTDIR)$(PROG_DATADIR)/plugins/{BFG.cfg,kbgen.c}
	$(INSTALL) -m 0644 misc/colors/*.cfm $(DESTDIR)$(PROG_DATADIR)/colors
	$(INSTALL) -m 0644 functions/* $(DESTDIR)$(PROG_DATADIR)/functions
	@printf "Successfully installed $(BIN)\n"

uninstall:
	$(RM) -- $(DESTDIR)$(PREFIX)/bin/$(BIN)
	$(RM) -- $(DESTDIR)$(MANPREFIX)/man1/$(BIN).1.gz
	$(RM) -- $(DESTDIR)$(DATADIR)/locale/*/LC_MESSAGES/$(BIN).mo
	$(RM) -- $(DESTDIR)$(DATADIR)/bash-completion/completions/$(BIN)
	$(RM) -- $(DESTDIR)$(DATADIR)/zsh/site-functions/_$(BIN)
	$(RM) -- $(DESTDIR)$(DESKTOPPREFIX)/$(BIN).desktop
	$(RM) -r -- $(DESTDIR)$(PROG_DATADIR)
	$(RM) -- $(DESTDIR)$(DESKTOPICONPREFIX)/scalable/apps/$(BIN).svg
	@printf "Successfully uninstalled $(BIN)\n"
