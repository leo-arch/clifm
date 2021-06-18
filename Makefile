######################
# Makefile for CliFM #
######################

OS := $(shell uname -s)

PREFIX ?= /usr/local
MANPREFIX ?= $(PREFIX)/share/man
DESKTOPPREFIX ?= $(PREFIX)/share/applications
DESKTOPICONPREFIX ?= $(PREFIX)/share/icons/hicolor
DATADIR ?= $(PREFIX)/share

SHELL ?= /bin/sh
INSTALL ?= install
BIN ?= clifm
CP ?= cp
RM ?= rm

SRCDIR = src/
OBJS != ls $(SRCDIR)*.c | sed "s/.c\$$/.o/g"

CFLAGS ?= -I/usr/local/include -O3 -fstack-protector-strong -march=native -Wall
LIBS_Linux ?= -lreadline -lacl -lcap
LIBS_FreeBSD ?= -L/usr/local/lib -lreadline -lintl

build: ${OBJS}
	@printf "Detected operating system: ";
	@echo "$(OS)"
	$(CC) -o $(BIN) ${OBJS} ${LIBS_${OS}} $(CFLAGS)

clean:
	rm -f $(SRC)*.o

install: build
	@$(INSTALL) -m 0755 -d $(DESTDIR)$(PREFIX)/bin
	@$(INSTALL) -m 0755 $(BIN) $(DESTDIR)$(PREFIX)/bin
	@$(RM) -- $(BIN)
	@$(INSTALL) -m 0755 -d $(DESTDIR)$(DATADIR)/$(BIN)
	@$(INSTALL) -m 0755 -d $(DESTDIR)$(MANPREFIX)/man1
	@$(INSTALL) -m 0755 -d $(DESTDIR)$(DATADIR)/bash-completion/completions
	@$(INSTALL) -m 0755 -d $(DESTDIR)$(DESKTOPPREFIX)
	@$(INSTALL) -m 0755 -d $(DESTDIR)$(DATADIR)/zsh/site-functions
	@$(INSTALL) -m 0755 -d $(DESTDIR)$(DESKTOPICONPREFIX)/scalable/apps
	@$(INSTALL) -m 0755 -d $(DESTDIR)$(DATADIR)/locale/es/LC_MESSAGES
	@$(INSTALL) -m 0644 misc/manpage $(DESTDIR)$(MANPREFIX)/man1/$(BIN).1
	@gzip $(DESTDIR)$(MANPREFIX)/man1/$(BIN).1
	@$(INSTALL) -m 0644 misc/completions.bash $(DESTDIR)$(DATADIR)/bash-completion/completions/$(BIN)
	@$(INSTALL) -m 0644 misc/completions.zsh $(DESTDIR)$(DATADIR)/zsh/site-functions/_$(BIN)
	@$(INSTALL) -m 0644 misc/$(BIN).desktop $(DESTDIR)$(DATADIR)/applications
	@$(INSTALL) -m 0644 misc/mimelist.cfm $(DESTDIR)$(DATADIR)/$(BIN)
	@$(INSTALL) -m 0644 images/logo/$(BIN).svg $(DESTDIR)$(DESKTOPICONPREFIX)/scalable/apps
	@$(INSTALL) -m 0644 translations/spanish/$(BIN).mo $(DESTDIR)$(DATADIR)/locale/es/LC_MESSAGES/$(BIN).mo
	@$(INSTALL) -m 0755 -d $(DESTDIR)$(DATADIR)/$(BIN)/plugins
	@$(INSTALL) -m 0755 -d $(DESTDIR)$(DATADIR)/$(BIN)/functions
	@$(INSTALL) -m 0644 plugins/* $(DESTDIR)$(DATADIR)/$(BIN)/plugins
	@$(INSTALL) -m 0644 functions/* $(DESTDIR)$(DATADIR)/$(BIN)/functions
	@printf "Successfully installed $(BIN)\n"

uninstall:
	@$(RM) -- $(DESTDIR)$(PREFIX)/bin/$(BIN)
	@$(RM) -- $(DESTDIR)$(MANPREFIX)/man1/$(BIN).1.gz
	@$(RM) -- $(DESTDIR)$(DATADIR)/locale/*/LC_MESSAGES/$(BIN).mo
	@$(RM) -- $(DESTDIR)$(DATADIR)/bash-completion/completions/$(BIN)
	@$(RM) -- $(DESTDIR)$(DATADIR)/zsh/site-functions/_$(BIN)
	@$(RM) -- $(DESTDIR)$(DATADIR)/applications/$(BIN).desktop
	@$(RM) -r -- $(DESTDIR)$(DATADIR)/$(BIN)
	@$(RM) -- $(DESTDIR)$(DESKTOPICONPREFIX)/scalable/apps/$(BIN).svg
	@printf "Successfully uninstalled $(BIN)\n"
