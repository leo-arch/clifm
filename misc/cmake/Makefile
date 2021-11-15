######################
# Makefile for CliFM #
######################

BIN ?= clifm

PREFIX ?= /usr/local
DATADIR ?= $(PREFIX)/share
MANDIR ?= $(PREFIX)/man
LOCALEDIR ?= $(DATADIR)/locale
DESKTOPPREFIX ?= $(DATADIR)/applications
DESKTOPICONPREFIX ?= $(DATADIR)/icons/hicolor
PROG_DATADIR ?= $(DATADIR)/$(BIN)

SHELL ?= /bin/sh
INSTALL ?= install
RM ?= rm

SRCDIR = src
SRC = $(SRCDIR)/*.c
HEADERS = $(SRCDIR)/*.h

clean:
	$(RM) -- $(BIN)
	$(RM) -f -- $(SRCDIR)/*.o

install:
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 0755 $(BIN) $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PROG_DATADIR)
	$(INSTALL) -m 0755 -d $(DESTDIR)$(MANDIR)/man1
	$(INSTALL) -m 0755 -d $(DESTDIR)$(DATADIR)/bash-completion/completions
	$(INSTALL) -m 0755 -d $(DESTDIR)$(DESKTOPPREFIX)
	$(INSTALL) -m 0755 -d $(DESTDIR)$(DATADIR)/zsh/site-functions
	$(INSTALL) -m 0755 -d $(DESTDIR)$(DESKTOPICONPREFIX)/scalable/apps
	$(INSTALL) -m 0644 misc/manpage $(DESTDIR)$(MANDIR)/man1/$(BIN).1
	gzip -- $(DESTDIR)$(MANDIR)/man1/$(BIN).1
	$(INSTALL) -m 0644 misc/completions.bash $(DESTDIR)$(DATADIR)/bash-completion/completions/$(BIN)
	$(INSTALL) -m 0644 misc/completions.zsh $(DESTDIR)$(DATADIR)/zsh/site-functions/_$(BIN)
	$(INSTALL) -m 0644 misc/$(BIN).desktop $(DESTDIR)$(DESKTOPPREFIX)
	$(INSTALL) -m 0644 misc/*.cfm $(DESTDIR)$(PROG_DATADIR)
	$(INSTALL) -m 0644 misc/clifmrc $(DESTDIR)$(PROG_DATADIR)
	$(INSTALL) -m 0644 images/logo/$(BIN).svg $(DESTDIR)$(DESKTOPICONPREFIX)/scalable/apps
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PROG_DATADIR)/plugins
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PROG_DATADIR)/functions
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PROG_DATADIR)/colors
	$(INSTALL) -m 0755 plugins/* $(DESTDIR)$(PROG_DATADIR)/plugins
	chmod 644 $(DESTDIR)$(PROG_DATADIR)/plugins/BFG.cfg
	chmod 644 $(DESTDIR)$(PROG_DATADIR)/plugins/kbgen.c
	$(INSTALL) -m 0644 misc/colors/*.cfm $(DESTDIR)$(PROG_DATADIR)/colors
	$(INSTALL) -m 0644 functions/* $(DESTDIR)$(PROG_DATADIR)/functions
	@printf "Successfully installed $(BIN)\n"

uninstall:
	$(RM) -- $(DESTDIR)$(PREFIX)/bin/$(BIN)
	$(RM) -- $(DESTDIR)$(MANDIR)/man1/$(BIN).1.gz
	$(RM) -- $(DESTDIR)$(DATADIR)/bash-completion/completions/$(BIN)
	$(RM) -- $(DESTDIR)$(DATADIR)/zsh/site-functions/_$(BIN)
	$(RM) -- $(DESTDIR)$(DESKTOPPREFIX)/$(BIN).desktop
	$(RM) -r -- $(DESTDIR)$(PROG_DATADIR)
	$(RM) -- $(DESTDIR)$(DESKTOPICONPREFIX)/scalable/apps/$(BIN).svg
	@printf "Successfully uninstalled $(BIN)\n"
