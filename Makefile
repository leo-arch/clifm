######################
# Makefile for CliFM #
######################

SHELL = /bin/sh
PREFIX = /usr/bin
PROG = clifm
OBJS = clifm.c

CC = gcc
CFLAGS_LINUX = -O3 -s -fstack-protector-strong -march=native -lreadline -lcap -lacl
CFLAGS_FREEBSD = -O3 -s -fstack-protector-strong -march=native -lreadline -lintl

UNAME := $(shell uname -s)

build:
	@echo -n "Checking operating system... "
	@case $(UNAME) in \
		Linux) \
			echo -ne "GNU/Linux\nBuilding $(PROG)... "; \
			$(CC) $(CFLAGS_LINUX) -o $(PROG) $(OBJS) ; \
			echo "Done" ;; \
		FreeBSD) \
			echo -ne "FreeBSD\nBuilding $(PROG)... "; \
			$(CC) $(CFLAGS_FREEBSD) -o $(PROG) $(OBJS) ; \
			echo "Done" ;; \
		*) \
			echo -e "\n'$(UNAME)': Operating system not supported" >&2 ;; \
	esac

install:
	@install -Dm755 ${PROG} ${PREFIX}/
	@echo "Successfully installed ${PROG} into ${PREFIX}"

uninstall:
	@rm ${PROG}
	@rm ${PREFIX}/${PROG}
	@echo "Successfully uninstalled ${PROG}"
