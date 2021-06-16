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
	@$(INSTALL) -m 0755 -d $(INSTALLPREFIX)
	@$(INSTALL) -m 0755 $(PROG) $(INSTALLPREFIX)
	@$(RM) -- $(PROG)
	@$(INSTALL) -m 0755 -d $(DESKTOPPREFIX)/$(PROG)
	@$(INSTALL) -m 0755 -d $(DESKTOPPREFIX)/man/man1
	@$(INSTALL) -m 0755 -d $(DESKTOPPREFIX)/bash-completion/completions
	@$(INSTALL) -m 0755 -d $(DESKTOPPREFIX)/applications
	@$(INSTALL) -m 0755 -d $(DESKTOPPREFIX)/zsh/site-functions
	@$(INSTALL) -m 0755 -d $(DESKTOPPREFIX)/icons/hicolor/scalable/apps
	@$(INSTALL) -m 0755 -d $(DESKTOPPREFIX)/locale/es/LC_MESSAGES
	@$(INSTALL) -m 0644 misc/manpage $(DESKTOPPREFIX)/man/man1/$(PROG).1
	@gzip $(DESKTOPPREFIX)/man/man1/$(PROG).1
	@$(INSTALL) -m 0644 misc/completions.bash $(DESKTOPPREFIX)/bash-completion/completions/$(PROG)
	@$(INSTALL) -m 0644 misc/completions.zsh $(DESKTOPPREFIX)/zsh/site-functions/_$(PROG)
	@$(INSTALL) -m 0644 misc/$(PROG).desktop $(DESKTOPPREFIX)/applications
	@$(INSTALL) -m 0644 misc/mimelist.cfm $(DESKTOPPREFIX)/$(PROG)
	@$(INSTALL) -m 0644 images/logo/$(PROG).svg $(ICONPREFIX)/scalable/apps
	@$(INSTALL) -m 0644 translations/spanish/$(PROG).mo $(DESKTOPPREFIX)/locale/es/LC_MESSAGES/$(PROG).mo
	@$(INSTALL) -m 0755 -d $(DESKTOPPREFIX)/$(PROG)/plugins
	@$(INSTALL) -m 0755 -d $(DESKTOPPREFIX)/$(PROG)/functions
	@$(INSTALL) -m 0644 plugins/* $(DESKTOPPREFIX)/$(PROG)/plugins
	@$(INSTALL) -m 0644 functions/* $(DESKTOPPREFIX)/$(PROG)/functions
	@printf "Successfully installed $(PROG)\n"

uninstall:
	@$(RM) -- $(INSTALLPREFIX)/$(PROG)
	@$(RM) -- $(DESKTOPPREFIX)/man/man1/$(PROG).1.gz
	@$(RM) -- $(DESKTOPPREFIX)/locale/*/LC_MESSAGES/$(PROG).mo
	@$(RM) -- $(DESKTOPPREFIX)/bash-completion/completions/$(PROG)
	@$(RM) -- $(DESKTOPPREFIX)/zsh/site-functions/_$(PROG)
	@$(RM) -- $(DESKTOPPREFIX)/applications/$(PROG).desktop
	@$(RM) -r -- $(DESKTOPPREFIX)/$(PROG)
	@$(RM) -- $(ICONPREFIX)/scalable/apps/$(PROG).svg
	@printf "Successfully uninstalled $(PROG)\n"
