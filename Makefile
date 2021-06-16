######################
# Makefile for CliFM #
######################

OS := $(shell uname -s)

SHELL ?= /bin/sh
INSTALLPREFIX ?= /usr/bin
DESKTOPPREFIX ?= /usr/share
ICONPREFIX ?= $(DESKTOPPREFIX)/icons/hicolor
PROG ?= clifm
INSTALL ?= install
CP ?= cp
RM ?= rm

build:
	cd src && $(MAKE) build

install: build
	@$(INSTALL) -m 0755 -d $(DESTDIR)$(INSTALLPREFIX)
	@$(INSTALL) -m 0755 $(PROG) $(DESTDIR)$(INSTALLPREFIX)
	@$(RM) -- $(PROG)
	@$(INSTALL) -m 0755 -d $(DESTDIR)$(DESKTOPPREFIX)/$(PROG)
	@$(INSTALL) -m 0755 -d $(DESTDIR)$(DESKTOPPREFIX)/man/man1
	@$(INSTALL) -m 0755 -d $(DESTDIR)$(DESKTOPPREFIX)/bash-completion/completions
	@$(INSTALL) -m 0755 -d $(DESTDIR)$(DESKTOPPREFIX)/applications
	@$(INSTALL) -m 0755 -d $(DESTDIR)$(DESKTOPPREFIX)/zsh/site-functions
	@$(INSTALL) -m 0755 -d $(DESTDIR)$(DESKTOPPREFIX)/icons/hicolor/scalable/apps
	@$(INSTALL) -m 0755 -d $(DESTDIR)$(DESKTOPPREFIX)/locale/es/LC_MESSAGES
	@$(INSTALL) -m 0644 misc/manpage $(DESTDIR)$(DESKTOPPREFIX)/man/man1/$(PROG).1
	@gzip $(DESTDIR)$(DESKTOPPREFIX)/man/man1/$(PROG).1
	@$(INSTALL) -m 0644 misc/completions.bash $(DESTDIR)$(DESKTOPPREFIX)/bash-completion/completions/$(PROG)
	@$(INSTALL) -m 0644 misc/completions.zsh $(DESTDIR)$(DESKTOPPREFIX)/zsh/site-functions/_$(PROG)
	@$(INSTALL) -m 0644 misc/$(PROG).desktop $(DESTDIR)$(DESKTOPPREFIX)/applications
	@$(INSTALL) -m 0644 misc/mimelist.cfm $(DESTDIR)$(DESKTOPPREFIX)/$(PROG)
	@$(INSTALL) -m 0644 images/logo/$(PROG).svg $(DESTDIR)$(ICONPREFIX)/scalable/apps
	@$(INSTALL) -m 0644 translations/spanish/$(PROG).mo $(DESTDIR)$(DESKTOPPREFIX)/locale/es/LC_MESSAGES/$(PROG).mo
	@$(INSTALL) -m 0755 -d $(DESTDIR)$(DESKTOPPREFIX)/$(PROG)/plugins
	@$(INSTALL) -m 0755 -d $(DESTDIR)$(DESKTOPPREFIX)/$(PROG)/functions
	@$(INSTALL) -m 0644 plugins/* $(DESTDIR)$(DESKTOPPREFIX)/$(PROG)/plugins
	@$(INSTALL) -m 0644 functions/* $(DESTDIR)$(DESKTOPPREFIX)/$(PROG)/functions
	@printf "Successfully installed $(PROG)\n"

uninstall:
	@$(RM) -- $(DESTDIR)$(INSTALLPREFIX)/$(PROG)
	@$(RM) -- $(DESTDIR)$(DESKTOPPREFIX)/man/man1/$(PROG).1.gz
	@$(RM) -- $(DESTDIR)$(DESKTOPPREFIX)/locale/*/LC_MESSAGES/$(PROG).mo
	@$(RM) -- $(DESTDIR)$(DESKTOPPREFIX)/bash-completion/completions/$(PROG)
	@$(RM) -- $(DESTDIR)$(DESKTOPPREFIX)/zsh/site-functions/_$(PROG)
	@$(RM) -- $(DESTDIR)$(DESKTOPPREFIX)/applications/$(PROG).desktop
	@$(RM) -r -- $(DESTDIR)$(DESKTOPPREFIX)/$(PROG)
	@$(RM) -- $(DESTDIR)$(ICONPREFIX)/scalable/apps/$(PROG).svg
	@printf "Successfully uninstalled $(PROG)\n"
