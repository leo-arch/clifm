########################################
# Makefile for CliFM (Solaris/Illumos) #
########################################

BIN ?= clifm

PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share
MANDIR ?= $(DATADIR)/man
LOCALEDIR ?= $(DATADIR)/locale
DESKTOPPREFIX ?= $(DATADIR)/applications
DESKTOPICONPREFIX ?= $(DATADIR)/icons/hicolor
PROG_DATADIR ?= $(DATADIR)/$(BIN)

SHELL ?= /bin/sh
INSTALL ?= ginstall
RM ?= rm

SRCDIR = src
SRC = $(SRCDIR)/*.c
HEADERS = $(SRCDIR)/*.h

osver != uname -r | sed -e 's/\.\([0-9]\{1,1\}\)$$/0\1/' -e 's/\.//'

CFLAGS ?= -O3 -fstack-protector-strong
CFLAGS += -Wall -Wextra
CPPFLAGS += -DCLIFM_DATADIR=$(DATADIR) -DSUN_VERSION=$(osver)

LIBS ?= -lreadline -ltermcap -lmagic -lnvpair

$(BIN): $(SRC) $(HEADERS)
	$(CC) -o $(BIN) $(SRC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(LIBS)

build: $(BIN)

clean:
	$(RM) -- $(BIN)
	$(RM) -f -- $(SRCDIR)/*.o

install: $(BIN)
	$(INSTALL) -m 0755 -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(BIN) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PROG_DATADIR)
	$(INSTALL) -m 0755 -d $(DESTDIR)$(MANDIR)/man1
	$(INSTALL) -m 0755 -d $(DESTDIR)$(DATADIR)/bash-completion/completions
	$(INSTALL) -m 0755 -d $(DESTDIR)$(DATADIR)/zsh/site-functions
	$(INSTALL) -m 0755 -d $(DESTDIR)$(DATADIR)/fish/vendor_completions.d
	$(INSTALL) -m 0755 -d $(DESTDIR)$(DESKTOPPREFIX)
	$(INSTALL) -m 0755 -d $(DESTDIR)$(DESKTOPICONPREFIX)/scalable/apps
	$(INSTALL) -m 0644 misc/manpage $(DESTDIR)$(MANDIR)/man1/$(BIN).1
	gzip -f -- $(DESTDIR)$(MANDIR)/man1/$(BIN).1
	$(INSTALL) -m 0644 misc/completions.bash $(DESTDIR)$(DATADIR)/bash-completion/completions/$(BIN)
	$(INSTALL) -m 0644 misc/completions.zsh $(DESTDIR)$(DATADIR)/zsh/site-functions/_$(BIN)
	$(INSTALL) -m 0644 misc/completions.fish $(DESTDIR)$(DATADIR)/fish/vendor_completions.d/$(BIN).fish
	$(INSTALL) -m 0644 misc/$(BIN).desktop $(DESTDIR)$(DESKTOPPREFIX)
	$(INSTALL) -m 0644 misc/*.clifm $(DESTDIR)$(PROG_DATADIR)
	$(INSTALL) -m 0644 misc/solaris/*.clifm $(DESTDIR)$(PROG_DATADIR)
	$(INSTALL) -m 0644 misc/clifmrc $(DESTDIR)$(PROG_DATADIR)
	$(INSTALL) -m 0644 misc/logo/$(BIN).svg $(DESTDIR)$(DESKTOPICONPREFIX)/scalable/apps
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PROG_DATADIR)/plugins
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PROG_DATADIR)/functions
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PROG_DATADIR)/colors
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PROG_DATADIR)/tools
	$(INSTALL) -m 0755 misc/tools/imgprev/clifmrun $(DESTDIR)$(PROG_DATADIR)/plugins
	$(INSTALL) -m 0755 misc/tools/imgprev/clifmimg $(DESTDIR)$(PROG_DATADIR)/plugins
	$(INSTALL) -m 0755 plugins/* $(DESTDIR)$(PROG_DATADIR)/plugins
	$(INSTALL) -m 0755 misc/tools/*.py $(DESTDIR)$(PROG_DATADIR)/tools
	chmod 644 $(DESTDIR)$(PROG_DATADIR)/plugins/BFG.cfg
	chmod 644 $(DESTDIR)$(PROG_DATADIR)/plugins/kbgen.c
	chmod 644 $(DESTDIR)$(PROG_DATADIR)/plugins/plugins-helper
	$(INSTALL) -m 0644 misc/colors/*.clifm $(DESTDIR)$(PROG_DATADIR)/colors
	$(INSTALL) -m 0644 functions/* $(DESTDIR)$(PROG_DATADIR)/functions
	@printf "Successfully installed $(BIN)\n"

uninstall:
	$(RM) -- $(DESTDIR)$(BINDIR)/$(BIN)
	$(RM) -- $(DESTDIR)$(MANDIR)/man1/$(BIN).1*
	$(RM) -- $(DESTDIR)$(DATADIR)/bash-completion/completions/$(BIN)
	$(RM) -- $(DESTDIR)$(DATADIR)/zsh/site-functions/_$(BIN)
	$(RM) -- $(DESTDIR)$(DATADIR)/fish/vendor_completions.d/$(BIN).fish
	$(RM) -- $(DESTDIR)$(DESKTOPPREFIX)/$(BIN).desktop
	$(RM) -r -- $(DESTDIR)$(PROG_DATADIR)
	$(RM) -- $(DESTDIR)$(DESKTOPICONPREFIX)/scalable/apps/$(BIN).svg
	@printf "Successfully uninstalled $(BIN)\n"
