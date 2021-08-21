######################
# Makefile for CliFM #
######################

OS != uname -s

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

CFLAGS ?= -O3 -fstack-protector-strong -march=native -Wall
LIBS_Linux ?= -lreadline -lacl -lcap -lmagic
LIBS_FreeBSD ?= -I/usr/local/include -L/usr/local/lib -lreadline -lintl -lmagic
LIBS_NetBSD ?= -I/usr/pkg/include -L/usr/pkg/lib -Wl,-R/usr/pkg/lib -lreadline -lintl -lmagic
LIBS_OpenBSD ?= -I/usr/local/include -L/usr/local/lib -lereadline -lintl -lmagic

ifdef _BE_POSIX
CFLAGS += -D_BE_POSIX
endif

ifdef _NO_ARCHIVING
CFLAGS += -D_NO_ARCHIVING
endif

ifdef _NERD
CFLAGS += -D_NERD
endif

ifdef _NO_GETTEXT
CFLAGS += -D_NO_GETTEXT
endif

ifdef _NO_ICONS
CFLAGS += -D_NO_ICONS
endif

ifdef _NO_LIRA
CFLAGS += -D_NO_LIRA
endif

ifdef _NO_MAGIC
CFLAGS += -D_NO_MAGIC
endif

ifdef _NO_SUGGESTIONS
CFLAGS += -D_NO_SUGGESTIONS
endif

ifdef _NO_TRASH
CFLAGS += -D_NO_TRASH
endif

build: $(SRC) $(HEADERS)
	@printf "Detected operating system: %s\n" "$(OS)"
	$(CC) -o $(BIN) $(SRC) $(CFLAGS) $(LDFLAGS) $(LIBS_$(OS))

clean:
	$(RM) -- $(BIN)
	$(RM) -f -- $(SRCDIR)/*.o

install: build
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
	$(RM) -- $(DESTDIR)$(LOCALEDIR)/*/LC_MESSAGES/$(BIN).mo
	$(RM) -- $(DESTDIR)$(DATADIR)/bash-completion/completions/$(BIN)
	$(RM) -- $(DESTDIR)$(DATADIR)/zsh/site-functions/_$(BIN)
	$(RM) -- $(DESTDIR)$(DESKTOPPREFIX)/$(BIN).desktop
	$(RM) -r -- $(DESTDIR)$(PROG_DATADIR)
	$(RM) -- $(DESTDIR)$(DESKTOPICONPREFIX)/scalable/apps/$(BIN).svg
	@printf "Successfully uninstalled $(BIN)\n"
