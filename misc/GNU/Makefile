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

LMAGIC = -lmagic
LINTL = -lintl

ifdef DEBUG
	CFLAGS += -g
else
	CFLAGS ?= -O3 -fstack-protector-strong
endif

ifdef CPU_NATIVE
	CFLAGS += -march=native
endif

ifdef CLIFM_SUCKLESS
	CPPFLAGS += -DCLIFM_SUCKLESS
endif

ifdef _NO_TAGS
	CPPFLAGS += -D_NO_TAGS
endif

ifdef _NO_BLEACH
	CPPFLAGS += -D_NO_BLEACH
endif

ifdef _TOURBIN_QSORT
	CPPFLAGS += -D_TOURBIN_QSORT
endif

ifdef _NO_HIGHLIGHT
	CPPFLAGS += -D_NO_HIGHLIGHT
endif

ifdef _NO_FZF
	CPPFLAGS += -D_NO_FZF
endif

ifdef _BE_POSIX
	CPPFLAGS += -D_BE_POSIX
endif

ifdef _NO_ARCHIVING
	CPPFLAGS += -D_NO_ARCHIVING
endif

ifdef _ICONS_IN_TERMINAL
	CPPFLAGS += -D_ICONS_IN_TERMINAL
endif

ifdef _NERD
	CPPFLAGS += -D_NERD
endif

ifdef _NO_GETTEXT
	CPPFLAGS += -D_NO_GETTEXT
	undefine LINTL
endif

ifdef _NO_ICONS
	CPPFLAGS += -D_NO_ICONS
endif

ifdef _NO_LIRA
	CPPFLAGS += -D_NO_LIRA
	undefine LMAGIC
endif

ifdef _NO_MAGIC
	CPPFLAGS += -D_NO_MAGIC
	undefine LMAGIC
endif

ifdef _NO_SUGGESTIONS
	CPPFLAGS += -D_NO_SUGGESTIONS
endif

ifdef _NO_TRASH
	CPPFLAGS += -D_NO_TRASH
endif

CFLAGS += -Wall -Wextra
CPPFLAGS += -DCLIFM_DATADIR=$(DATADIR)

LIBS_Linux ?= -lreadline -lacl -lcap $(LMAGIC)
LIBS_FreeBSD ?= -I/usr/local/include -L/usr/local/lib -lreadline $(LINTL) $(LMAGIC)
LIBS_NetBSD ?= -I/usr/pkg/include -L/usr/pkg/lib -Wl,-R/usr/pkg/lib -lreadline $(LINTL) $(LMAGIC)
LIBS_OpenBSD ?= -I/usr/local/include -L/usr/local/lib -lereadline $(LINTL) $(LMAGIC)
LIBS_Darwin ?= -I/opt/local/include -L/opt/local/lib -lreadline $(LINTL) $(LMAGIC)

$(BIN): $(SRC) $(HEADERS)
	@printf "Detected operating system: %s\n" "$(OS)"
	$(CC) -o $(BIN) $(SRC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(LIBS_$(OS))

build: $(BIN)

clean:
	$(RM) -- $(BIN)
	$(RM) -f -- $(SRCDIR)/*.o

install: $(BIN)
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 0755 $(BIN) $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PROG_DATADIR)
	$(INSTALL) -m 0755 -d $(DESTDIR)$(MANDIR)/man1
	$(INSTALL) -m 0755 -d $(DESTDIR)$(DATADIR)/bash-completion/completions
	$(INSTALL) -m 0755 -d $(DESTDIR)$(DESKTOPPREFIX)
	$(INSTALL) -m 0755 -d $(DESTDIR)$(DATADIR)/zsh/site-functions
	$(INSTALL) -m 0755 -d $(DESTDIR)$(DESKTOPICONPREFIX)/scalable/apps
	$(INSTALL) -m 0644 misc/manpage $(DESTDIR)$(MANDIR)/man1/$(BIN).1
	gzip -f -- $(DESTDIR)$(MANDIR)/man1/$(BIN).1
	$(INSTALL) -m 0644 misc/completions.bash $(DESTDIR)$(DATADIR)/bash-completion/completions/$(BIN)
	$(INSTALL) -m 0644 misc/completions.zsh $(DESTDIR)$(DATADIR)/zsh/site-functions/_$(BIN)
	$(INSTALL) -m 0644 misc/$(BIN).desktop $(DESTDIR)$(DESKTOPPREFIX)
	$(INSTALL) -m 0644 misc/*.clifm $(DESTDIR)$(PROG_DATADIR)
	$(INSTALL) -m 0644 misc/clifmrc $(DESTDIR)$(PROG_DATADIR)
	$(INSTALL) -m 0644 misc/logo/$(BIN).svg $(DESTDIR)$(DESKTOPICONPREFIX)/scalable/apps
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PROG_DATADIR)/plugins
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PROG_DATADIR)/functions
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PROG_DATADIR)/colors
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PROG_DATADIR)/tools
	$(INSTALL) -m 0755 plugins/* $(DESTDIR)$(PROG_DATADIR)/plugins
	$(INSTALL) -m 0755 misc/tools/*.py $(DESTDIR)$(PROG_DATADIR)/tools
	chmod 644 $(DESTDIR)$(PROG_DATADIR)/plugins/BFG.cfg
	chmod 644 $(DESTDIR)$(PROG_DATADIR)/plugins/kbgen.c
	chmod 644 $(DESTDIR)$(PROG_DATADIR)/plugins/plugins-helper
	$(INSTALL) -m 0644 misc/colors/*.clifm $(DESTDIR)$(PROG_DATADIR)/colors
	$(INSTALL) -m 0644 functions/* $(DESTDIR)$(PROG_DATADIR)/functions
	@printf "Successfully installed $(BIN)\n"

uninstall:
	$(RM) -- $(DESTDIR)$(PREFIX)/bin/$(BIN)
	$(RM) -- $(DESTDIR)$(MANDIR)/man1/$(BIN).1*
	$(RM) -- $(DESTDIR)$(DATADIR)/bash-completion/completions/$(BIN)
	$(RM) -- $(DESTDIR)$(DATADIR)/zsh/site-functions/_$(BIN)
	$(RM) -- $(DESTDIR)$(DESKTOPPREFIX)/$(BIN).desktop
	$(RM) -r -- $(DESTDIR)$(PROG_DATADIR)
	$(RM) -- $(DESTDIR)$(DESKTOPICONPREFIX)/scalable/apps/$(BIN).svg
	@printf "Successfully uninstalled $(BIN)\n"
