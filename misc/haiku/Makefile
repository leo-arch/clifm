##############################
# Makefile for CliFM (HAIKU) #
##############################

BIN ?= clifm

PREFIX ?= /boot/system/non-packaged
MANPREFIX ?= $(PREFIX)/documentation/man
DATADIR ?= $(PREFIX)/data
PROG_DATADIR ?= $(DATADIR)/$(BIN)

SHELL ?= /bin/sh
INSTALL ?= install
RM ?= rm

SRCDIR = src
OBJS != ls $(SRCDIR)/*.c | sed "s/.c\$$/.o/g"

#CFLAGS ?= -O3 -fstack-protector-strong -march=native -Wall
LIBS ?= -lreadline

ifdef _BE_POSIX
LIBS += -D_BE_POSIX
endif

ifdef _NO_ARCHIVING
LIBS += -D_NO_ARCHIVING
endif

ifdef _NERD
LIBS += -D_NERD
endif

ifdef _NO_GETTEXT
LIBS += -D_NO_GETTEXT
else
LIBS += -lintl
endif

ifdef _NO_ICONS
LIBS += -D_NO_ICONS
endif

ifdef _NO_LIRA
LIBS += -D_NO_LIRA -D_NO_MAGIC
endif

ifdef _NO_MAGIC
LIBS += -D_NO_MAGIC
else
LIBS += -lmagic
endif

ifdef _NO_SUGGESTIONS
LIBS += -D_NO_SUGGESTIONS
endif

ifdef _NO_TRASH
LIBS += -D_NO_TRASH
endif

CFLAGS += -DCLIFM_DATADIR=$(DATADIR)

build: ${OBJS}
	@printf "Detected operating system: ";
	@echo "$(OS)"
	$(CC) -o $(BIN) ${OBJS} $(LIBS) $(CFLAGS)

clean:
	$(RM) -- $(BIN)
	$(RM) -f -- $(SRCDIR)/*.o

install: build
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 0755 $(BIN) $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PROG_DATADIR)
	$(INSTALL) -m 0755 -d $(DESTDIR)$(MANPREFIX)/man1
	$(INSTALL) -m 0755 -d $(DESTDIR)$(DATADIR)/bash-completion/completions
	$(INSTALL) -m 0755 -d $(DESTDIR)$(DATADIR)/zsh/site-functions
	$(INSTALL) -m 0644 misc/manpage $(DESTDIR)$(MANPREFIX)/man1/$(BIN).1
	gzip -f -- $(DESTDIR)$(MANPREFIX)/man1/$(BIN).1
	$(INSTALL) -m 0644 misc/completions.bash $(DESTDIR)$(DATADIR)/bash-completion/completions/$(BIN)
	$(INSTALL) -m 0644 misc/completions.zsh $(DESTDIR)$(DATADIR)/zsh/site-functions/_$(BIN)
	$(INSTALL) -m 0644 misc/*.clifm $(DESTDIR)$(PROG_DATADIR)
	$(INSTALL) -m 0644 misc/haiku/*.clifm $(DESTDIR)$(PROG_DATADIR)
	$(INSTALL) -m 0644 misc/clifmrc $(DESTDIR)$(PROG_DATADIR)
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PROG_DATADIR)/plugins
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PROG_DATADIR)/functions
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PROG_DATADIR)/colors
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PROG_DATADIR)/tools
	$(INSTALL) -m 0755 plugins/* $(DESTDIR)$(PROG_DATADIR)/plugins
	$(INSTALL) -m 0755 misc/tools/*.py $(DESTDIR)$(PROG_DATADIR)/tools
	chmod 644 -- $(DESTDIR)$(PROG_DATADIR)/plugins/BFG.cfg
	chmod 644 -- $(DESTDIR)$(PROG_DATADIR)/plugins/kbgen.c
	$(INSTALL) -m 0644 misc/colors/*.clifm $(DESTDIR)$(PROG_DATADIR)/colors
	$(INSTALL) -m 0644 functions/* $(DESTDIR)$(PROG_DATADIR)/functions
	@printf "Successfully installed $(BIN)\n"

uninstall:
	$(RM) -- $(DESTDIR)$(PREFIX)/bin/$(BIN)
	$(RM) -- $(DESTDIR)$(MANPREFIX)/man1/$(BIN).1.gz
	$(RM) -- $(DESTDIR)$(DATADIR)/bash-completion/completions/$(BIN)
	$(RM) -- $(DESTDIR)$(DATADIR)/zsh/site-functions/_$(BIN)
	$(RM) -r -- $(DESTDIR)$(PROG_DATADIR)
	@printf "Successfully uninstalled $(BIN)\n"
